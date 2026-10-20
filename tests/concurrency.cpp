#include <SymbolicRegression.hpp>
#include <array>
#include <atomic>
#include <barrier>
#include <bit>
#include <latch>
#include <sstream>
#include <iostream>
#include <numeric>

static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F> static std::exception_ptr failure(F function){try{function();}catch(...){return std::current_exception();}throw std::runtime_error("expected failure");}
static std::string cause(std::exception_ptr e){try{std::rethrow_exception(e);}catch(const std::exception& x){const auto* n=dynamic_cast<const std::nested_exception*>(&x);return n && n->nested_ptr()?cause(n->nested_ptr()):x.what();}catch(...){return "nonstandard";}}
struct Gate {
    std::atomic<unsigned> active{0},peak{0},starts{0};
    std::barrier<> barrier;
    explicit Gate(unsigned n):barrier(n){}
    void enter(){++starts;const auto a=++active;auto p=peak.load();while(p<a && !peak.compare_exchange_weak(p,a)){};}
};
class Candidate final:public EvaluatedCandidate {
public:
    int value;
    std::shared_ptr<Gate> gate;
    explicit Candidate(int v,std::shared_ptr<Gate> g={}):value(v),gate(std::move(g)){}
    bool parallelEvaluationSafe() const noexcept override{return true;}
    std::string stats() override{return std::to_string(value);}
    bool invalid=false,throwing=false,detachThrows=false,cancel=false;
    ExecutionContext* execution=nullptr;
    void beginParallelEvaluation(ExecutionContext&) override {if(detachThrows)throw std::bad_alloc();}
    std::shared_ptr<std::latch> first,last;
protected:
    Fitness prepareEvaluation() override { // exercise virtual preparation, not only const fitness
        if(gate){gate->enter();gate->barrier.arrive_and_wait();}
        if(first){if(value==0)first->wait();else first->count_down();}
        if(last){if(value==0)last->wait();else last->count_down();}
        if(cancel)execution->requestStop();
        if(gate)--gate->active;
        if(throwing)throw std::runtime_error("candidate fault");
        return evaluate();
    }
    Fitness evaluate() const override{return invalid?Fitness::invalid():Fitness::valid(value%2);}
};
class StartupFault:public ExecutionContext {
    unsigned launches=0;
public:StartupFault():ExecutionContext({3}){}
protected:
    std::thread launchWorker(std::function<void()> body) override {
        if(++launches==2)throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));
        return ExecutionContext::launchWorker(std::move(body));
    }
};
class DetachedFault final:public EvaluatedCandidate {
public:
    bool parallelEvaluationSafe() const noexcept override{return true;}
    void beginParallelEvaluation(ExecutionContext&) override {throw std::bad_alloc();}
    std::string stats() override{return "fault";}
protected:Fitness evaluate() const override{return Fitness::valid(0);}
};
static std::vector<int> values(const Population<Candidate>& population){std::vector<int> result;const auto snapshot=population.creatures();for(const auto& c:*snapshot)result.push_back(c.value);return result;}
static void executorContracts(){
    for(unsigned n:{1u,2u}){
        RandomContext run(0,{n});auto gate=std::make_shared<Gate>(n);
        std::vector<Candidate> input{Candidate(0,gate),Candidate(1,gate)};
        Population<Candidate> p(input,run);const auto before=values(p);const auto best=p.bestSeen()->value;
        input[0].cancel=true;input[0].execution=&run.execution();
        check(isEvaluationCancelled(failure([&]{p.publishGeneration(input);})) && values(p)==before && p.generation()==0 && p.bestSeen()->value==best,"serial/parallel cancellation preserves commit");
        check(run.evaluationAttempts()==2+n,"cancelled actual calls");
    }

    for(const auto text:{"0","-1","+2","2junk","257","999999999999999999999",""})
        check(cause(failure([&]{(void)ExecutionConfig::parse(text);}))!="","parse");
    for(unsigned n:{1u,2u,3u,4u}){
        RandomContext run(3,{n});auto gate=std::make_shared<Gate>(n);
        std::vector<Candidate> input;for(unsigned i=0;i<n*2;++i)input.emplace_back(i,gate);
        Population<Candidate> p(input,run,ObjectiveDirection::Minimize);
        check(gate->peak==n && gate->starts==n*2,"actual overlap and ceiling");
        check(run.evaluationAttempts()==n*2 && p.generation()==0,"starts counted");
        check(run.execution().peakTasks()<=n,"executor ceiling");
    }
    for(unsigned size:{1u,3u,5u}){
        std::vector<Candidate> input;for(unsigned i=0;i<size;++i)input.emplace_back(i);
        RandomContext run(7,{4});Population<Candidate> p(input,run);
        check(run.evaluationAttempts()==size && run.execution().peakTasks()<=std::min(4u,size),"small uneven population");
    }
    {
        ExecutionContext reverse({4});std::barrier ready(4);
        std::array<std::latch,4> release{std::latch(1),std::latch(1),std::latch(1),std::latch(1)};
        std::array<unsigned,4> arrival{};std::atomic<unsigned> cursor{0};
        reverse.run(4,[&](auto i){ready.arrive_and_wait();if(i<3)release[i+1].wait();arrival[cursor++]=i;release[i].count_down();});
        check(arrival==std::array<unsigned,4>{3,2,1,0},"reversed arrival forced");
    }
    // All callbacks admitted before reversed arrival. Slot zero must complete last.
    RandomContext run(1,{4});auto gate=std::make_shared<Gate>(4);
    auto latch=std::make_shared<std::latch>(3);std::vector<Candidate> input;
    for(int i=0;i<4;++i){input.emplace_back(i,gate);input.back().first=latch;}
    Population<Candidate> p(input,run,ObjectiveDirection::Minimize);
    check(values(p)==std::vector<int>({1,3,0,2}) && p.bestSeen()->value==2,"stable equal-fitness ordering");
    auto before=values(p);const auto best=p.bestSeen()->value;const auto fitness=p.bestSeen()->evaluation();
    for(auto& c:input){c.gate.reset();c.first.reset();c.invalid=true;}
    check(p.publishGeneration(input)==GenerationOutcome::NoViableCandidates && values(p)==before,"all invalid preserve");
    for(auto& c:input){c.invalid=false;c.throwing=true;}
    auto e=failure([&]{p.publishGeneration(input);});
    check(cause(e)=="candidate fault" && values(p)==before && p.generation()==0 && p.bestSeen()->value==best && p.bestSeen()->evaluation()==fitness,"fault preserves committed state");
    for(auto& c:input)c.throwing=false;
    input[1].detachThrows=true;const auto calls=run.evaluationAttempts();
    check(bool(failure([&]{p.publishGeneration(input);})) && values(p)==before && run.evaluationAttempts()==calls,"detach allocation preserves committed state");
    {
        StartupFault startupContext;RandomContext run(0,startupContext);
        Population<Candidate> committed(std::vector<Candidate>{Candidate(0)},run);
        const auto original=values(committed);
        check(bool(failure([&]{committed.publishGeneration(std::vector<Candidate>{Candidate(1),Candidate(2),Candidate(3)});})) && values(committed)==original && committed.generation()==0 && committed.bestSeen()->value==0 && run.evaluationAttempts()==1,"startup growth failure preserves commit and drains old workers");
    }
    {
        RandomContext run(0,{3});Candidate bad(0);bad.invalid=true;
        Population<Candidate> mixed(std::vector<Candidate>{bad,Candidate(1)},run);
        check(mixed.bestSeen()->value==1,"mixed validity");
        Population<Candidate> empty(std::vector<Candidate>{bad,bad,bad},run);
        check(!empty.generation() && !empty.bestSeen(),"all invalid parallel initialization");
    }
    StartupFault startup;
    check(bool(failure([&]{startup.run(3,[](auto){});})) && std::accumulate(startup.started().begin(),startup.started().end(),0)==0,"partial startup drained");
    RandomContext allocation(1,{2});
    check(bool(failure([&]{Population<DetachedFault> q(std::vector<DetachedFault>(2),allocation);})) && allocation.evaluationAttempts()==0,"detach allocation before attempts");
    // Cooperative cancellation drains active jobs and leaves queued jobs unstarted.
    ExecutionContext executor({2});std::latch both(2);std::atomic<unsigned> starts{0};
    e=failure([&]{executor.run(9,[&](auto){++starts;both.count_down();both.wait();executor.requestStop();});});
    check(isEvaluationStop(e) && starts==2,"cancellation queued/active");
    executor.run(3,[](auto){});check(!executor.stopRequested(),"clean reuse after cancellation");
    // Both simultaneous failures must be kept; operational beats lower-slot budget.
    std::barrier pair(2);
    e=failure([&]{executor.run(2,[&](auto i){pair.arrive_and_wait();if(i==0)throw EvaluationBudgetExceeded("budget");throw std::runtime_error("operation");});});
    check(cause(e)=="operation" && executor.failures()[0] && executor.failures()[1],"failure priority and retention");
    std::barrier errors(2);
    e=failure([&]{executor.run(2,[&](auto i){errors.arrive_and_wait();throw std::runtime_error(i?"second":"first");});});
    check(cause(e)=="first","operational errors ordered by slot");
    // Batch descriptor copy/submission failure: no per-job submissions exist.
    struct CopyFault {CopyFault()=default;CopyFault(const CopyFault&){throw std::bad_alloc();}void operator()(std::size_t)const{}};
    CopyFault descriptor;check(bool(failure([&]{executor.run(2,descriptor); })),"submission allocation");
    executor.run(2,[](auto){});
    std::barrier allocationGate(2);
    auto allocationError=failure([&]{executor.run(2,[&](auto i){allocationGate.arrive_and_wait();if(i==0)throw std::bad_alloc();});});
    bool allocationCause=false;try{std::rethrow_exception(allocationError);}catch(const std::bad_alloc&){allocationCause=true;}
    check(allocationCause,"active allocation failure retains cause and drains");
    executor.run(2,[](auto){});
}
static SymbolicRegressionResult search(unsigned n,unsigned interval,std::string& rng){
    auto problem=RegressionProblem::create({{{-2},7},{{-1},3},{{0},1},{{1},1},{{2},3}},{31,6,8});
    SymbolicRegressionConfig config;config.population=9;config.elites=1;config.maxGenerationAttempts=5;
    config.maxEvaluations=54;config.maxTrainingLossAttempts=100000;config.trainingLossThreshold=0;
    config.fitting=ConstantFitConfig{};config.fitting->maxLossAttempts=12;config.fitEveryGenerations=interval;
    config.grammar.continuousProbability=.5;
    RandomContext run(9,{n});auto r=symbolicRegression(problem,config,run);std::ostringstream out;out<<run.engine();rng=out.str();return r;
}
class SerialOnly final:public EvaluatedCandidate {
public:std::string stats()override{return "serial";}
protected:Fitness evaluate()const override{return Fitness::valid(0);}
};
static void diagnosticsCompatibility(){
    auto problem=RegressionProblem::create({{{0},1},{{1},2}}, {7,3,8});
    SymbolicRegressionConfig config;config.population=4;config.elites=1;
    config.maxGenerationAttempts=1;config.maxEvaluations=8;config.maxTrainingLossAttempts=64;
    config.fitting=ConstantFitConfig{};config.fitting->maxLossAttempts=4;
    config.diagnostics=std::make_shared<RefinementDiagnostics>(4);
    auto objective=std::make_shared<TrainingObjective>(problem,config.maxTrainingLossAttempts,config.diagnostics);
    RandomContext parallel(7,{3});const auto before=parallel.engine();
    bool rejected=false;
    try{(void)symbolicRegression(problem,config,parallel,objective);}
    catch(const std::invalid_argument& e){rejected=std::string(e.what()).find("no detailed refinement diagnostics")!=std::string::npos;}
    check(rejected,"parallel detailed diagnostics rejected clearly");
    check(parallel.execution().requested()==3 && parallel.execution().peakTasks()==0 && parallel.engine()==before && parallel.evaluationAttempts()==0,
          "incompatible diagnostics preserve execution request and consume no work or RNG");
    check(objective->work()==TrainingWork{} && config.diagnostics->expressions.empty(),"diagnostics not silently dropped or executed");
    RandomContext serial(7);
    const auto result=symbolicRegression(problem,config,serial);
    check(result.best.has_value() && result.trainingWork.lossAttempts>0 && !config.diagnostics->expressions.empty(),"serial detailed diagnostics remain supported");
}
static void equivalence(){
    RandomContext serial(0);Population<SerialOnly> serialPopulation(std::vector<SerialOnly>(2),serial);
    RandomContext parallel(0,{2});
    check(bool(failure([&]{Population<SerialOnly> p(std::vector<SerialOnly>(2),parallel);})) && parallel.evaluationAttempts()==0,"serial subclass explicit parallel rejection");
    for(unsigned interval:{1u,4u}){
        std::string referenceRng;auto a=search(1,interval,referenceRng);
        check(a.publishedGenerations==5 && a.stoppingReason==SearchStop::GenerationLimit,"nonbinding five-generation reference");
        if(interval==4)check(a.preparationWork.skippedBySchedule==36 && a.trainingWork.fittingInvocations==18,"scheduled and unscheduled preparation");
        for(unsigned n:{2u,3u,4u}){
            std::string rng;auto b=search(n,interval,rng);
            check(a.best->text()==b.best->text() && a.best->evaluation()==b.best->evaluation() && referenceRng==rng,"parallel search exact result/RNG");
            check(a.trainingWork==b.trainingWork && a.preparationWork==b.preparationWork && a.variation==b.variation,"parallel work/fit decisions");
            check(a.preparedInitialExpressions==b.preparedInitialExpressions && a.preparedInitialFitness==b.preparedInitialFitness && a.initialWork==b.initialWork,"prepared initialization");
            check(a.publishedGenerations==b.publishedGenerations && a.stoppingReason==b.stoppingReason && a.improvements.size()==b.improvements.size(),"generation/history");
            for(std::size_t i=0;i<a.improvements.size();++i)check(a.improvements[i].lossAttempts==b.improvements[i].lossAttempts && a.improvements[i].generation==b.improvements[i].generation && a.improvements[i].mse==b.improvements[i].mse,"trace equivalence");
        }
    }
}
// Concurrent objective with an independent record of actual compute starts.
class RecordedObjective:public TrainingObjective {
public:
    std::shared_ptr<std::atomic<unsigned>> starts;
    RecordedObjective(std::shared_ptr<const RegressionProblem> p,std::uint64_t cap,std::shared_ptr<std::atomic<unsigned>> s)
        :TrainingObjective(std::move(p),cap),starts(std::move(s)){}
    bool parallelEvaluationSafe()const noexcept override{return true;}
protected:
    std::shared_ptr<TrainingObjective> cloneForParallel()const override{return std::make_shared<RecordedObjective>(problem(),maximum(),starts);}
    RegressionMetrics compute(const Expression& e) override{++*starts;return TrainingObjective::compute(e);}
};
static void budgets(){
    const auto problem=RegressionProblem::create({{{1},2.5},{{2},2.5}},{31,6,8});
    for(auto cap:{0u,1u,2u,3u,4u,7u,8u,9u,31u}){
        auto starts=std::make_shared<std::atomic<unsigned>>(0);
        auto objective=std::make_shared<RecordedObjective>(problem,cap,starts);
        auto prep=std::make_shared<ConstantRefinement>(objective,ConstantFitConfig{});
        std::vector<ExpressionCandidate> initial;for(int i=0;i<4;++i)initial.emplace_back(Expression::constant(0,1,problem->limits()),problem,prep);
        RandomContext random(1,{4});
        auto e=failure([&]{Population<ExpressionCandidate,RegressionOrdering> p(initial,random,ObjectiveDirection::Minimize);});
        check(isEvaluationStop(e) && objective->work().lossAttempts==starts->load() && *starts<=cap,"global admission starts exactly charged");
    }
    // Exactly sufficient / one short, no fitting: last-permit contention.
    for(auto cap:{3u,4u}){
        auto starts=std::make_shared<std::atomic<unsigned>>(0);auto objective=std::make_shared<RecordedObjective>(problem,cap,starts);
        auto prep=std::make_shared<ConstantRefinement>(objective);
        std::vector<ExpressionCandidate> initial;for(int i=0;i<4;++i)initial.emplace_back(Expression::constant(1,1,problem->limits()),problem,prep);
        RandomContext run(1,{4});
        if(cap==3)check(isEvaluationStop(failure([&]{Population<ExpressionCandidate,RegressionOrdering> p(initial,run,ObjectiveDirection::Minimize);})),"one short");
        else {Population<ExpressionCandidate,RegressionOrdering> p(initial,run,ObjectiveDirection::Minimize);check(p.generation()==0,"exact sufficient");}
        check(objective->work().lossAttempts==cap && *starts==cap,"last permits");
    }
}

class GatedPreparation final:public RegressionPreparation {
public:
    std::shared_ptr<TrainingObjective> objective;
    std::shared_ptr<std::barrier<>> gate;
    GatedPreparation(std::shared_ptr<TrainingObjective> o,std::shared_ptr<std::barrier<>> g):objective(std::move(o)),gate(std::move(g)){}
    bool parallelEvaluationSafe()const noexcept override{return true;}
    std::shared_ptr<RegressionPreparation> forkParallel(ExecutionContext& e)override{return std::make_shared<GatedPreparation>(objective->forkParallel(e),gate);}
    void mergeParallel(const RegressionPreparation& p) override{objective->mergeParallel(*static_cast<const GatedPreparation&>(p).objective);}
    PreparedExpression prepare(const Expression& e,const RegressionProblem&)override{gate->arrive_and_wait();return {e,objective->measure(e).loss};}
};
static void contention(){
    auto problem=RegressionProblem::create({{{1},2}},{31,6,8});
    for(unsigned cap:{0u,1u,3u,4u}){
        auto starts=std::make_shared<std::atomic<unsigned>>(0);
        auto objective=std::make_shared<RecordedObjective>(problem,cap,starts);
        auto prep=std::make_shared<GatedPreparation>(objective,std::make_shared<std::barrier<>>(4));
        std::vector<ExpressionCandidate> input;for(int i=0;i<4;++i)input.emplace_back(Expression::constant(1,1),problem,prep);
        RandomContext run(0,{4});
        if(cap<4)check(isEvaluationBudget(failure([&]{Population<ExpressionCandidate,RegressionOrdering> p(input,run,ObjectiveDirection::Minimize); })),"simultaneous final permits");
        else{Population<ExpressionCandidate,RegressionOrdering> p(input,run,ObjectiveDirection::Minimize);check(p.generation()==0,"exact contention allowance");}
        check(*starts==cap && objective->work().lossAttempts==cap && objective->work().rowAttempts==cap && run.evaluationAttempts()==4,"independent start/admission accounting");
    }
}
struct FaultState {std::atomic<bool> enabled{false};std::atomic<unsigned> starts{0};std::latch progress{1};bool fitting=true;};
class FittingFaultObjective final:public TrainingObjective {
    std::shared_ptr<FaultState> state;
public:
    FittingFaultObjective(std::shared_ptr<const RegressionProblem> p,std::uint64_t cap,std::shared_ptr<FaultState> s):TrainingObjective(std::move(p),cap),state(std::move(s)){}
    bool parallelEvaluationSafe()const noexcept override{return true;}
protected:
    std::shared_ptr<TrainingObjective> cloneForParallel()const override{return std::make_shared<FittingFaultObjective>(problem(),maximum(),state);}
    RegressionMetrics compute(const Expression& e)override {
        ++state->starts;
        const double value=e.nodes()[0].constantValue();
        if(state->enabled){
            if(value==8){state->progress.wait();throw std::runtime_error("fault after sibling progress");}
            if((state->fitting && value==-1) || (!state->fitting && value==0)){
                if(state->fitting)check(work().accepted>0,"sibling accepted improvement before fault");
                state->progress.count_down();
            }
        }
        return TrainingObjective::compute(e);
    }
};
static std::string snapshot(const Population<ExpressionCandidate,RegressionOrdering>& p){
    std::ostringstream out;out<<*p.generation()<<':'<<*p.bestGeneration()<<':'<<p.bestSeen()->text()<<':'<<std::bit_cast<std::uint64_t>(p.bestSeen()->fitness());
    auto members=p.creatures();for(const auto& c:*members)out<<';'<<c.text()<<':'<<int(c.fitnessState())<<':'<<std::bit_cast<std::uint64_t>(c.fitness());return out.str();
}
static void failedPublication(){
    auto problem=RegressionProblem::create({{{1},2.5},{{2},2.5}},{31,6,8});
    for(unsigned interval:{1u,4u}){
        auto state=std::make_shared<FaultState>();state->fitting=interval==1;
        auto objective=std::make_shared<FittingFaultObjective>(problem,10000,state);
        ConstantFitConfig fit;fit.maxLossAttempts=4;
        auto prep=std::make_shared<ConstantRefinement>(objective,fit,interval);
        std::vector<ExpressionCandidate> input;for(int i=0;i<2;++i)input.emplace_back(Expression::constant(0,1),problem,prep);
        RandomContext run(2,{2});Population<ExpressionCandidate,RegressionOrdering> p(input,run,ObjectiveDirection::Minimize);
        prep->initializing=false;const auto before=snapshot(p);const auto work=objective->work();
        input[1].replaceExpression(Expression::constant(8,1));state->enabled=true;
        auto e=failure([&]{p.publishGeneration(input);});
        check(cause(e)=="fault after sibling progress" && snapshot(p)==before,"scheduled/unscheduled failure preserves entire commit");
        check(objective->work().lossAttempts==state->starts && objective->work().lossAttempts>work.lossAttempts,"throwing attempts retained");
        if(interval==1)check(objective->work().accepted>work.accepted,"private accepted fitting work retained in diagnostics");
        else check(prep->statistics().skippedBySchedule==2,"unscheduled fit skips");
        state->enabled=false;
        check(p.publishGeneration(input)==GenerationOutcome::Committed && p.generation()==1,"retry same prospective generation");
        if(interval==4)check(prep->statistics().skippedBySchedule==4,"failed attempt did not consume cadence");
    }
    for(unsigned interval:{1u,4u}){
        auto starts=std::make_shared<std::atomic<unsigned>>(0);auto objective=std::make_shared<RecordedObjective>(problem,9,starts);
        ConstantFitConfig fit;fit.maxLossAttempts=4;
        auto prep=std::make_shared<ConstantRefinement>(objective,fit,interval);
        std::vector<ExpressionCandidate> input;for(int i=0;i<2;++i)input.emplace_back(Expression::constant(0,1),problem,prep);
        RandomContext run(2,{2});Population<ExpressionCandidate,RegressionOrdering> p(input,run,ObjectiveDirection::Minimize);
        check(*starts==8,"known initial fit work");auto before=snapshot(p);
        check(isEvaluationBudget(failure([&]{p.publishGeneration(input);})) && snapshot(p)==before,"global exhaustion preserves scheduled/unscheduled commit");
        check(*starts==9 && objective->work().lossAttempts==9,"no excess loss admission");
        check(isEvaluationBudget(failure([&]{p.publishGeneration(input);})) && snapshot(p)==before && *starts==9,"zero remaining retry and unchanged generation");
    }
    // Another bounded fit can improve an inherited expression; no optimized flag.
    TrainingObjective objective(problem,1000);ConstantFitConfig fit;fit.maxLossAttempts=2;
    auto first=fitConstants(Expression::constant(0,1),objective,fit);
    auto second=fitConstants(first.expression,objective,fit);
    check(second.loss.score()<first.loss.score(),"inherited bounded refinement can improve again");
    // Independent engines share immutable problem only, not random/mutable services.
    std::array<SymbolicRegressionResult,2> results;std::array<std::exception_ptr,2> errors;
    std::array<std::thread,2> engines;
    for(std::size_t i=0;i<2;++i)engines[i]=std::thread([&,i]{try{SymbolicRegressionConfig c;c.population=5;c.elites=1;c.maxGenerationAttempts=2;c.maxEvaluations=15;RandomContext r(17,{2});results[i]=symbolicRegression(problem,c,r);}catch(...){errors[i]=std::current_exception();}});
    for(auto& thread:engines)thread.join();
    for(const auto& e:errors)if(e)std::rethrow_exception(e);
    check(results[0].best->text()==results[1].best->text() && results[0].trainingWork==results[1].trainingWork,"independent engines");
}
static void numerics(){
    const auto limits=ExpressionLimits{31,6,std::numeric_limits<double>::max()};
    const auto c=[&](double x){return Expression::constant(x,3,limits);};
    const auto div=[&](Expression a,Expression b){return Expression::binary(ExpressionKind::Divide,a,b);};
    const auto log=[&](Expression a){return Expression::unary(ExpressionKind::Log,a);};
    const double tiny=std::numeric_limits<double>::denorm_min();
    std::vector<Expression> expressions={c(-0.),Expression::variable(2,3,limits),div(c(1),c(0.)),div(c(1),c(-0.)),
        div(c(tiny),c(tiny)),div(c(-tiny),c(2)),div(c(-0.),c(2)),div(c(1),c(tiny)),div(c(2*tiny),c(2)),log(c(1)),log(c(tiny)),log(c(0)),log(c(-0.)),log(c(-1)),
        Expression::binary(ExpressionKind::Multiply,c(0),log(c(-1))),
        div(Expression::binary(ExpressionKind::Multiply,c(1e308),c(1e308)),c(1e308)),log(div(c(tiny),c(2)))};
    ExecutionContext executor({4});std::vector<ExpressionResult> expected;for(const auto& e:expressions)expected.push_back(e.evaluate(std::array{1.,2.,3.}));
    executor.run(expressions.size()*8,[&](auto i){auto n=i%expressions.size();auto actual=expressions[n].evaluate(std::array{1.,2.,3.});check(actual.valid()==expected[n].valid(),"worker arithmetic classification");if(actual.valid())check(std::bit_cast<std::uint64_t>(actual.value())==std::bit_cast<std::uint64_t>(expected[n].value()),"worker arithmetic exact bits");});
    std::barrier envGate(4);
    executor.run(4,[&](auto){envGate.arrive_and_wait();check(std::fesetround(FE_DOWNWARD)==0,"worker FP setup");});
    executor.run(4,[](auto){check(std::fegetround()==FE_TONEAREST,"worker FP environment reset per task");});
}
int main(){try{executorContracts();diagnosticsCompatibility();equivalence();budgets();contention();failedPublication();numerics();std::cout<<"concurrent contracts passed\n";}catch(const std::exception& e){std::cerr<<cause(std::current_exception())<<'\n';return 1;}}
