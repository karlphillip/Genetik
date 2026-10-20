// Fixed-work measurements only; no stochastic discovery/selection experiment.
#include "benchmarks/EvaluationCorpus.hpp"
#include <chrono>
#include <sys/resource.h>
#include <sstream>
using Clock=std::chrono::steady_clock;
static std::string fingerprint(const Population<ExpressionCandidate,RegressionOrdering>& population){
    std::ostringstream result;auto snapshot=population.creatures();
    for(const auto& c:*snapshot)result<<c.text()<<':'<<static_cast<int>(c.fitnessState())<<':'<<(c.fitnessState()==FitnessState::Valid?std::bit_cast<std::uint64_t>(c.fitness()):0)<<'\n';
    return result.str();
}
static std::array<std::uint64_t,8> work(const TrainingWork& w){return {w.lossAttempts,w.rowAttempts,w.fittingInvocations,w.accepted,w.proposals,w.invalid,w.infeasible,w.duplicates};}
static std::array<std::uint64_t,8> difference(const TrainingWork& a,const TrainingWork& b){auto x=work(a),y=work(b);for(std::size_t i=0;i<x.size();++i)x[i]-=y[i];return x;}
static long rss(){std::ifstream file("/proc/self/status");std::string name;while(file>>name){if(name=="VmRSS:"){long value;file>>value;return value;}std::string rest;std::getline(file,rest);}return -1;}
int main(){
    try{
        std::cout<<std::setprecision(17)<<"problem,round,phase,requested,effective,peak_tasks,seconds,candidates,loss_calls,rows,fits,accepted,invalid_probes,rss_kib,process_peak_kib\n";
        auto corpus=evaluation_evidence::corpus();
        for(const auto& item:corpus)if(item.name=="prime" || item.name=="mixed"){
            const auto problem=RegressionProblem::create(item.rows,{31,6,8});
            std::vector<Expression> raw;
            // Identical declared inputs with modest parameter variation and unequal fit work.
            for(unsigned i=0;i<64;++i){auto e=problem->checkedExpression(item.expression);const auto nodes=e.nodes();for(std::size_t j=0;j<nodes.size();++j)if(nodes[j].kind()==ExpressionKind::Constant){e=e.withConstant(j,nodes[j].constantValue()+.125*(int(i%5)-2));break;}raw.push_back(std::move(e));}
            std::string reference;std::array<std::uint64_t,8> referenceWork{};
            // One untimed warm-up per workload; final timings have no detailed diagnostics.
            {TrainingObjective objective(problem);ConstantFitConfig fit;(void)fitConstants(raw[0],objective,fit);}
            for(unsigned round=0;round<5;++round)for(unsigned n:round%2?std::array{4u,2u,1u}:std::array{1u,2u,4u}){
                const auto start=Clock::now();
                RandomContext run(2026,{n});auto objective=std::make_shared<TrainingObjective>(problem,1000000000);
                auto prep=std::make_shared<ConstantRefinement>(objective,ConstantFitConfig{});
                std::vector<ExpressionCandidate> input;for(const auto& e:raw)input.emplace_back(e,problem,prep);
                Population<ExpressionCandidate,RegressionOrdering> population(input,run,ObjectiveDirection::Minimize);
                const double cold=std::chrono::duration<double>(Clock::now()-start).count();
                const auto first=objective->work();const auto text=fingerprint(population);
                if(reference.empty()){reference=text;referenceWork=work(first);}
                if(text!=reference || work(first)!=referenceWork)throw std::runtime_error("fixed workload numerical/work mismatch");
                auto print=[&](const char* phase,double seconds,const auto& w){struct rusage usage{};getrusage(RUSAGE_SELF,&usage);std::cout<<item.name<<','<<round<<','<<phase<<','<<n<<','<<run.execution().effective(64)<<','<<(n==1?1:run.execution().peakTasks())<<','<<seconds<<",64,"<<w[0]<<','<<w[1]<<','<<w[2]<<','<<w[3]<<','<<w[5]<<','<<rss()<<','<<usage.ru_maxrss<<'\n';};
                print("startup",cold,work(first));
                const auto begin=Clock::now();population.publishGeneration(input);
                const double reused=std::chrono::duration<double>(Clock::now()-begin).count();
                auto delta=difference(objective->work(),first);
                if(fingerprint(population)!=reference || delta!=referenceWork)throw std::runtime_error("reused executor mismatch");
                print("reuse",reused,delta);
            }
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
