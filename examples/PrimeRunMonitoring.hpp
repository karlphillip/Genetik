#pragma once
// Shared application-only signal flag and steady committed-boundary scheduling.
#include <atomic>
#include <chrono>
#include <csignal>
#include <stdexcept>
namespace prime_example {
using ExactClock=std::chrono::steady_clock;
inline std::atomic<bool> exactInterrupt{false};
static_assert(std::atomic<bool>::is_always_lock_free);
inline void exactSignal(int) noexcept {exactInterrupt.store(true,std::memory_order_relaxed);}
class ExactSignalGuard {
    using Handler=void(*)(int);
    Handler previous_,previousTerm_;
public:
    ExactSignalGuard():previous_(SIG_DFL),previousTerm_(SIG_DFL) {
        exactInterrupt.store(false,std::memory_order_relaxed);
        previous_=std::signal(SIGINT,exactSignal);
        if(previous_==SIG_ERR)throw std::runtime_error("cannot install SIGINT handler");
        previousTerm_=std::signal(SIGTERM,exactSignal);
        if(previousTerm_==SIG_ERR){(void)std::signal(SIGINT,previous_);throw std::runtime_error("cannot install SIGTERM handler");}
    }
    ~ExactSignalGuard(){(void)std::signal(SIGINT,previous_);(void)std::signal(SIGTERM,previousTerm_);}
    ExactSignalGuard(const ExactSignalGuard&)=delete;
    ExactSignalGuard& operator=(const ExactSignalGuard&)=delete;
};
// Caller supplies monotonic timestamps; catch up in one step, never a print burst.
// Deadlines stay aligned with start, including time spent formatting/reporting.
class ExactReportSchedule {
    ExactClock::duration interval_;
    ExactClock::time_point next_;
    bool initial_=true;
public:
    ExactReportSchedule(ExactClock::time_point start,ExactClock::duration interval):interval_(interval),next_(start) {
        if(interval<=ExactClock::duration::zero() || start>ExactClock::time_point::max()-interval)
            throw std::invalid_argument("unrepresentable reporting interval");
        next_+=interval;
    }
    bool due(ExactClock::time_point now,bool force=false) {
        const bool report=initial_ || force || now>=next_;initial_=false;
        if(now>=next_) {
            const auto advance=interval_-(now-next_)%interval_;
            next_=now>ExactClock::time_point::max()-advance?ExactClock::time_point::max():now+advance;
        }
        return report;
    }
    ExactClock::time_point nextDeadline() const noexcept {return next_;}
};
}
