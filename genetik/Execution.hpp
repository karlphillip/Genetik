#pragma once
#include <algorithm>
#include <atomic>
#include <cfenv>
#include <charconv>
#include <condition_variable>
#include <exception>
#include <functional>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

struct ExecutionConfig {
    unsigned threads = 1;
    void validate() const {
        if (threads == 0 || threads > 256) throw std::invalid_argument("threads must be 1..256");
    }
    static ExecutionConfig parse(std::string_view text) {
        if(text.empty())throw std::invalid_argument("invalid --threads value");
        unsigned n=0;
        const auto r=std::from_chars(text.data(),text.data()+text.size(),n);
        if (r.ec!=std::errc{} || r.ptr!=text.data()+text.size())
            throw std::invalid_argument("invalid --threads value");
        ExecutionConfig result{n};result.validate();return result;
    }
};
// Expected control stops are subordinate to operational exceptions in a batch.
class EvaluationStopped : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
class EvaluationBudgetExceeded : public EvaluationStopped {
public: using EvaluationStopped::EvaluationStopped;
};
class EvaluationCancelled : public EvaluationStopped {
public: EvaluationCancelled():EvaluationStopped("candidate execution cancelled"){}
};
inline bool isEvaluationStop(std::exception_ptr error) noexcept {
    try { if(error)std::rethrow_exception(error); }
    catch(const EvaluationStopped&) {return true;}
    catch(const std::exception& e) {
        const auto* nested=dynamic_cast<const std::nested_exception*>(&e);
        return nested && isEvaluationStop(nested->nested_ptr());
    } catch(...) {}
    return false;
}

inline bool isEvaluationBudget(std::exception_ptr error) noexcept {
    try{if(error)std::rethrow_exception(error);}
    catch(const EvaluationBudgetExceeded&){return true;}
    catch(const std::exception& e){const auto* n=dynamic_cast<const std::nested_exception*>(&e);return n && isEvaluationBudget(n->nested_ptr());}
    catch(...){}
    return false;
}

inline bool isEvaluationCancelled(std::exception_ptr error) noexcept {
    try{if(error)std::rethrow_exception(error);}
    catch(const EvaluationCancelled&){return true;}
    catch(const std::exception& e){const auto* n=dynamic_cast<const std::nested_exception*>(&e);return n && isEvaluationCancelled(n->nested_ptr());}
    catch(...){}
    return false;
}

// One external coordinator. Only requestStop()/stopRequested() are concurrently
// accessible. run() drains its entire bounded batch before returning or throwing.
class ExecutionContext {
public:
    explicit ExecutionContext(ExecutionConfig config={}):config_(config){config_.validate();}
    virtual ~ExecutionContext(){shutdown();}
    ExecutionContext(const ExecutionContext&)=delete;
    ExecutionContext& operator=(const ExecutionContext&)=delete;
    unsigned requested() const noexcept {return config_.threads;}
    unsigned effective(std::size_t count) const noexcept {
        return static_cast<unsigned>(std::min<std::size_t>(config_.threads,count));
    }
    bool stopRequested() const noexcept {return stop_.load(std::memory_order_acquire);}
    void requestStop() noexcept {
        {std::lock_guard lock(mutex_);
         stop_.store(true,std::memory_order_release);
         if(running_ && active_==0){running_=false;done_.notify_one();}}
        ready_.notify_all();
    }
    const std::vector<unsigned char>& started() const noexcept {return started_;}
    const std::vector<std::exception_ptr>& failures() const noexcept {return failures_;}
    unsigned peakTasks() const noexcept {return peak_;}
    // Allocation-free guard for the legacy caller-thread population path.
    class SerialScope {
    public:
        explicit SerialScope(ExecutionContext& context):context_(context){
            if(context_.requested()!=1)throw std::logic_error("serial scope requires concurrency one");
            if(context_.busy_.test_and_set())throw std::logic_error("executor already running");
            context_.stop_.store(false,std::memory_order_release);
            context_.started_.clear();context_.failures_.clear();context_.peak_=1;
        }
        SerialScope(const SerialScope&)=delete;
        SerialScope& operator=(const SerialScope&)=delete;
        ~SerialScope(){context_.busy_.clear();}
    private:ExecutionContext& context_;
    };
    SerialScope serialScope(){return SerialScope(*this);}
    // One range descriptor, no per-task submission allocation or unbounded queue.
    void run(std::size_t count,std::function<void(std::size_t)> function) {
        if(busy_.test_and_set())throw std::logic_error("executor already running");
        struct Release {std::atomic_flag& flag;~Release(){flag.clear();}} release{busy_};
        started_.assign(count,0);failures_.assign(count,{});peak_=0;
        stop_.store(false,std::memory_order_release);
        if(count==0)return;
        if(config_.threads==1) {
            for(std::size_t i=0;i<count && !stopRequested();++i){
                started_[i]=1;peak_=1;
                try{function(i);}catch(...){failures_[i]=std::current_exception();requestStop();}
            }
        } else {
            if(std::fegetround()!=FE_TONEAREST || std::fegetenv(&environment_)!=0)
                throw std::invalid_argument("parallel evaluation requires default rounding and a readable FP environment");
            ensureWorkers(effective(count)); // no callbacks until startup fully succeeds
            {
                std::lock_guard lock(mutex_);
                function_=std::move(function);next_=0;count_=count;active_=0;running_=!stopRequested();
            }
            ready_.notify_all();
            {
                std::unique_lock lock(mutex_);
                done_.wait(lock,[&]{return !running_;});
                function_={};
            }
        }
        // All errors retained by slot. Lowest operational slot wins, even if a
        // budget/cancellation stop arrived earlier. Otherwise lowest control slot.
        for(const auto& e:failures_)if(e && !isEvaluationStop(e))std::rethrow_exception(e);
        for(const auto& e:failures_)if(e && isEvaluationBudget(e))std::rethrow_exception(e);
        for(const auto& e:failures_)if(e)std::rethrow_exception(e);
        if(stopRequested())throw EvaluationCancelled();
    }
protected:
    // Platform/thread-launch seam; called only by a fully constructed coordinator.
    virtual std::thread launchWorker(std::function<void()> body){return std::thread(std::move(body));}
private:
    void ensureWorkers(unsigned count) {
        if(shutdown_)throw std::logic_error("executor unavailable after startup failure");
        try {
            workers_.reserve(count);
            while(workers_.size()<count){
                auto thread=launchWorker([this]{worker();});
                if(!thread.joinable())throw std::runtime_error("worker factory returned no worker");
                workers_.push_back(std::move(thread));
            }
        }catch(...){shutdown();throw;}
    }
    void shutdown() noexcept {
        {std::lock_guard lock(mutex_);shutdown_=true;}
        ready_.notify_all();
        for(auto& worker:workers_)if(worker.joinable())worker.join();
        workers_.clear();
    }
    void worker() noexcept {
        for(;;){
            std::size_t slot;
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock,[&]{return shutdown_ || (running_ && !stopRequested() && next_<count_);});
                if(shutdown_)return;
                slot=next_++;started_[slot]=1;++active_;peak_=std::max(peak_,active_);
            }
            try {
                if(std::fesetenv(&environment_)!=0)throw std::runtime_error("cannot restore worker FP environment");
                function_(slot);
            }catch(...){failures_[slot]=std::current_exception();requestStop();}
            {
                std::lock_guard lock(mutex_);
                --active_;
                if((next_==count_ || stopRequested()) && active_==0){running_=false;done_.notify_one();ready_.notify_all();}
            }
        }
    }
    const ExecutionConfig config_;
    std::atomic_flag busy_=ATOMIC_FLAG_INIT;
    std::atomic<bool> stop_{false};
    std::mutex mutex_;
    std::condition_variable ready_,done_;
    std::vector<std::thread> workers_;
    std::function<void(std::size_t)> function_;
    std::vector<unsigned char> started_;
    std::vector<std::exception_ptr> failures_;
    std::fenv_t environment_{};
    std::size_t next_=0,count_=0;
    unsigned active_=0,peak_=0;
    bool running_=false,shutdown_=false;
};
