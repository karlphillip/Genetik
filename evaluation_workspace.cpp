// Evidence harness: identical source compiled against archived and candidate headers.
#include "examples/EvaluationCorpus.hpp"
#include <bit>
#include <cstdlib>
#include <new>
#include <sstream>
#include <chrono>
namespace allocations {bool enabled=false;std::uint64_t calls=0,bytes=0;}
#ifdef GENETIK_COUNT_ALLOCATIONS
void* operator new(std::size_t size) {
    if(allocations::enabled){++allocations::calls;allocations::bytes+=size;}
    if(void* p=std::malloc(size?size:1))return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
// libstdc++ stable_sort uses nothrow temporary buffers. Route the paired
// nothrow forms through this interceptor too, including under ASan interposition.
void* operator new(std::size_t n,const std::nothrow_t&) noexcept {
    try{return ::operator new(n);}catch(...){return nullptr;}
}
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept {
    try{return ::operator new[](n);}catch(...){return nullptr;}
}
void operator delete(void* p,const std::nothrow_t&) noexcept {::operator delete(p);}
void operator delete[](void* p,const std::nothrow_t&) noexcept {::operator delete[](p);}

#endif
using namespace evaluation_evidence;
static std::uint64_t bits(double v){return std::bit_cast<std::uint64_t>(v);}
static void fitness(std::ostream& out,const Fitness& f){out<<int(f.state());if(f.state()==FitnessState::Valid)out<<':'<<bits(f.score());}
static void work(std::ostream& out,const TrainingWork& w) {
    out<<w.lossAttempts<<' '<<w.rowAttempts<<' '<<w.fittingInvocations<<' '<<w.fittingLossAttempts<<' '<<w.proposals<<' '<<w.accepted<<' '<<w.invalid<<' '<<w.infeasible<<' '<<w.duplicates<<' '<<w.invalidLosses<<' '<<w.ordinaryLossAttempts<<' '<<w.fittingBaselineAttempts<<' '<<w.fittingProbeAttempts<<' '<<w.fittingImproved<<'\n';
}
// A compiler barrier for every request prevents invariant-call hoisting. The loss
// and row count feed a checksum printed outside the timed region.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static RegressionMetrics request(const RegressionProblem& p,const Expression& e,std::uint64_t* rows) {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" ::: "memory");
#endif
    return p.measure(e,rows);
}
static void replay(const std::string& name,unsigned interval,bool shortRun) {
    const bool prime=name=="prime";const unsigned seed=prime?2:42;
    const auto problem=RegressionProblem::create(prime?prime_example::rows(prime_example::reference()):regression_example::arithmeticRows(0),{31,6,8});
    auto cfg=prime_example::config(shortRun?4096:100000);
    if(!prime){cfg.trainingLossThreshold=1e-14;cfg.fitting->lossThreshold=1e-14;}
    cfg.fitEveryGenerations=interval;
    if(shortRun){cfg.population=8;cfg.elites=1;cfg.maxGenerationAttempts=5;cfg.fitting->maxLossAttempts=8;cfg.trainingLossThreshold=0;cfg.fitting->lossThreshold=0;}
    RandomContext rng(seed);const auto start=std::chrono::steady_clock::now();
    const auto r=symbolicRegression(problem,cfg,rng);
    const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cerr<<name<<" interval="<<interval<<" seconds="<<std::setprecision(17)<<seconds<<'\n';
    std::cout<<"best="<<(r.best?r.best->text():"none")<<'\n';if(r.best){fitness(std::cout,r.best->evaluation());std::cout<<'\n';}
    std::cout<<"initial=";if(r.initialBestMSE)std::cout<<bits(*r.initialBestMSE);std::cout<<" raw=";if(r.rawInitialBestMSE)std::cout<<bits(*r.rawInitialBestMSE);std::cout<<'\n';
    for(const auto& s:r.rawInitialExpressions)std::cout<<"raw "<<s<<'\n';
    for(const auto& s:r.preparedInitialExpressions)std::cout<<"prepared "<<s<<'\n';
    for(const auto& f:r.preparedInitialFitness){fitness(std::cout,f);std::cout<<'\n';}
    std::cout<<"initial_work ";work(std::cout,r.initialWork);
    std::cout<<"final_work ";work(std::cout,r.trainingWork);
    std::cout<<"generations "<<r.attemptedGenerations<<' '<<r.publishedGenerations<<" best_generation "<<r.bestGeneration.value_or(UINT64_MAX)<<" candidate_requests "<<r.candidateEvaluations<<" stop "<<int(r.stoppingReason)<<'\n';
    std::cout<<"unpublished "<<r.unpublishedLossAttempts<<' '<<r.unpublishedRowAttempts<<' '<<r.unpublishedCandidateRequests<<'\n';
    const auto& p=r.preparationWork;
    std::cout<<"preparation "<<p.skippedBySchedule<<' '<<p.completedImproved<<' '<<p.completedUnchanged<<' '<<p.noConstants<<' '<<p.invalidStarts<<' '<<p.abortedFits<<'\n';
    for(const auto& a:p.origins)std::cout<<a.requests<<' '<<a.fits<<' '<<a.skipped<<' '<<a.ordinary<<' '<<a.baselines<<' '<<a.probes<<' '<<a.rows<<' '<<a.improved<<'\n';
    for(std::size_t i=0;i<p.terminations.size();++i){if(i)std::cout<<' ';std::cout<<p.terminations[i];}
    std::cout<<'\n';
    const auto& v=r.variation;
    std::cout<<"variation "<<v.initializations<<' '<<v.mutationProposals<<' '<<v.crossoverProposals<<' '<<v.structuralRejections<<' '<<v.mutationFallbacks<<' '<<v.crossoverFallbacks<<' '<<v.mutationChanges<<' '<<v.crossoverChanges<<'\n';
    for(auto t:r.improvements)std::cout<<"trace "<<t.lossAttempts<<' '<<t.generation<<' '<<bits(t.mse)<<'\n';
    std::cout<<"rng "<<rng.engine()<<'\n';
}
int main(int argc,char** argv) {
    try {
        if(argc<2)throw std::invalid_argument("--reference | --allocations | --timing CASE | --replay prime|arithmetic [4]");
        const std::string mode=argv[1];
        if(mode=="--replay") {
            if((argc!=3 && argc!=4) || (std::string(argv[2])!="prime" && std::string(argv[2])!="arithmetic") ||
               (argc==4 && std::string(argv[3])!="4"))throw std::invalid_argument("--replay prime|arithmetic [4]");
            replay(argv[2],argc>3?4:1,argc>3);return 0;
        }
        if(mode!="--reference" && mode!="--allocations" && mode!="--timing")throw std::invalid_argument("unknown evidence mode");
#ifndef GENETIK_COUNT_ALLOCATIONS
        if(mode=="--allocations")throw std::invalid_argument("compile with GENETIK_COUNT_ALLOCATIONS for allocation measurement");
#endif
        if(mode=="--timing" && argc!=3)throw std::invalid_argument("--timing requires a corpus case name");
        bool matched=false;
        const auto cases=corpus();
        for(const auto& c:cases) {
            const auto p=RegressionProblem::create(c.rows,{1023,64,std::numeric_limits<double>::max()});
            if(mode=="--reference") {
                std::cout<<c.name<<' '<<c.expression.toString()<<'\n';
                bool first=true;
                for(const auto& row:c.rows){if(!first)std::cout<<' ';first=false;auto r=c.expression.evaluate(row.inputs);std::cout<<r.valid()<<':'<<(r.valid()?bits(r.value()):*r.failedNode());}std::cout<<'\n';
                TrainingObjective objective(p);const auto loss=objective.measure(c.expression);
                fitness(std::cout,loss.loss);std::cout<<" samples="<<loss.samplesAttempted<<" max=";if(loss.maxAbsoluteError)std::cout<<bits(*loss.maxAbsoluteError);std::cout<<'\n';work(std::cout,objective.work());
                // All-row reporting deliberately has a different visitation contract.
                const auto report=regression_example::predictionReport(c.expression,c.rows);
                std::cout<<"report "<<report.rows<<' '<<report.invalid<<' '<<report.invalidErrors<<' '<<(report.maximum?bits(*report.maximum):0)<<'\n';
            } else if(mode=="--allocations") {
                std::uint64_t rows=0;allocations::calls=allocations::bytes=0;allocations::enabled=true;
                const auto r=p->measure(c.expression,&rows);allocations::enabled=false;
                std::cout<<c.name<<','<<c.expression.nodeCount()<<','<<rows<<','<<allocations::calls<<','<<allocations::bytes<<','<<int(r.loss.state())<<'\n';
            } else if(mode=="--timing" && c.name==argv[2]) {
                matched=true;
                for(int i=0;i<32;++i)(void)request(*p,c.expression,nullptr);
                std::uint64_t rows=0,checksum=0;
                const auto start=std::chrono::steady_clock::now();
                for(std::size_t i=0;i<c.batches;++i){const auto r=request(*p,c.expression,&rows);checksum+=(r.loss.state()==FitnessState::Valid)?bits(r.loss.score()):r.samplesAttempted;}
                const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
                std::cout<<c.name<<','<<c.batches<<','<<rows<<','<<checksum<<','<<std::setprecision(17)<<elapsed<<'\n';
            }
        }
        if(mode=="--timing" && !matched)throw std::invalid_argument("unknown corpus case");
        if(mode=="--reference") {
            const auto classify=[](const char* name,auto operation) {
                std::cout<<name<<' ';
                try{operation();std::cout<<"missing_error\n";}
                catch(const std::invalid_argument&){std::cout<<"invalid_argument\n";}
                catch(const std::logic_error&){std::cout<<"logic_error\n";}
                catch(const std::overflow_error&){std::cout<<"overflow_error\n";}
            };
            const auto x=Expression::variable(0);
            classify("wrong_arity",[&]{(void)x.evaluate(std::span<const double>{});});
            classify("nonfinite_binding",[&]{(void)x.evaluate(std::array{std::numeric_limits<double>::infinity()});});
            classify("malformed_structure",[]{const std::array n{ExpressionNode::binary(ExpressionKind::Add)};(void)Expression(n,1);});
            auto source=x;auto moved=std::move(source);(void)moved;
            classify("moved_expression",[&]{(void)source.evaluate(std::array{1.});});
            const auto p=RegressionProblem::create({{{1},0}});
            classify("loss_schema",[&]{(void)p->measure(Expression::variable(1,2));});
            std::uint64_t rows=UINT64_MAX;
            classify("row_counter",[&]{(void)p->measure(x,&rows);});
            std::cout<<"row_counter_preserved "<<rows<<'\n';
        }
    }catch(const std::exception& e){allocations::enabled=false;std::cerr<<e.what()<<'\n';return 1;}
}
