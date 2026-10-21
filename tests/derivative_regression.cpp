#include <DerivativeRegression.hpp>
#include <ExpressionParsing.hpp>
#include <ScalarTemplateRegression.hpp>
#include <barrier>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <new>
#include <cstdlib>

namespace fault {thread_local bool next=false;thread_local long after=-1;}
void* operator new(std::size_t n){if(fault::after==0){fault::after=-1;throw std::bad_alloc();}if(fault::after>0)--fault::after;if(fault::next){fault::next=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void operator delete(void* p) noexcept{std::free(p);}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
void* operator new(std::size_t n,const std::nothrow_t&) noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept{try{return ::operator new[](n);}catch(...){return nullptr;}}
void operator delete(void* p,const std::nothrow_t&) noexcept{::operator delete(p);}
void operator delete[](void* p,const std::nothrow_t&) noexcept{::operator delete[](p);}
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class E=std::exception,class F>static void rejects(F f){bool threw=false;try{f();}catch(const E&){threw=true;}check(threw,"expected rejection");}
static std::string cause(const std::exception& e){try{std::rethrow_if_nested(e);}catch(const std::exception& x){return cause(x);}return e.what();}
template<class F>static std::exception_ptr failure(F f){try{f();}catch(...){return std::current_exception();}throw std::runtime_error("missing failure");}
static std::string cause(std::exception_ptr e){try{std::rethrow_exception(e);}catch(const std::exception& x){return cause(x);}}
using D=DerivativeRegressionProblem;using O=ObservationStatus;using K=ExpressionKind;
using P=Population<ExpressionCandidate,RegressionOrdering>;
static auto snapshot(const P& p){std::vector<std::pair<std::string,Fitness>> s;const auto c=p.creatures();for(const auto& x:*c)s.push_back({x.text(),x.evaluation()});return s;}
static std::shared_ptr<const D> anchored(double target=1) {
    std::vector<RegressionRow> rows;for(int k=-4;k<=4;++k){double x=k*.5;rows.push_back({{x},2*x});}
    return D::create(1,{{std::nullopt,{{{0},target}},1,1},{0,rows,1,1}},{15,6,8});
}
static void lossAndIdentity() {
    const auto x=Expression::variable(0);
    std::vector<ScalarObservationChannel> channels{{std::nullopt,{{{0},1},{{1},3}},1,1},{0,{{{0},3},{{2},1}},2,3}};
    auto p=D::create(1,channels);const auto m=p->assess(x);
    check(m.loss==Fitness::valid(1) && m.channels[0].normalizedMean==2.5 && m.channels[1].normalizedMean==.5,"independent weighted normalized fixture");
    check(m.work.observations==4 && m.work.valueEvaluations==2 && m.work.partialEvaluations==2,"one observation per requested channel row");
    for(int c=0;c<2;++c){auto duplicate=channels;auto rows=duplicate[c].observations;duplicate[c].observations.insert(duplicate[c].observations.end(),rows.begin(),rows.end());check(D::create(1,duplicate)->measure(x).loss==m.loss,"channel duplication leaves score unchanged");}
    channels[0].observations[0].inputs[0]=100;channels[0].observations[0].target=100;
    check(p->measure(x).loss==m.loss,"owned nested observations");
    auto copy=*p;check(copy.measure(x).loss==m.loss,"problem copy independence");
    auto sorted=D::create(3,{{2,{{{2,3,4},6}},1,1},{std::nullopt,{{{2,3,4},26}},1,1},{0,{{{2,3,4},4}},1,1}});
    const auto multivariate=parseExpression("x0*x0+x1*x2+x2*x2",3);
    // Value=4+12+16=32, d0=4, d2=3+8=11; verify original indices.
    check(sorted->channels()[0].partial==std::nullopt && sorted->channels()[1].partial==0 && sorted->channels()[2].partial==2,"canonical channel order");
    auto exact=D::create(3,{{2,{{{2,3,4},11}},1,1},{0,{{{2,3,4},4}},1,1},{std::nullopt,{{{2,3,4},32}},1,1}});
    check(exact->measure(multivariate).loss==Fitness::valid(0),"two original-index partial channels");
    auto root=parseExpression("sqrt(x0)",1);auto okay=D::create(1,{{std::nullopt,{{{0},0}},1,1},{0,{{{1},.5}},1,1}});
    check(okay->measure(root).loss==Fitness::valid(0),"unrequested singular derivative does not affect value");
    const auto alternating=D::create(1,{{std::nullopt,{{{1},1},{{-1},0},{{4},2}},1,1}})->assess(root);
    check(alternating.complete && alternating.channels[0].primalInvalid==1 && alternating.work.valueEvaluations==3 && alternating.work.primalOperations==2,"lexical value workspace resets after invalid row");
    auto bad=D::create(1,{{0,{{{1},.5},{{0},0},{{-1},0},{{4},.25}},1,1}});
    const auto first=bad->measureObservations(root),all=bad->assess(root);
    check(first.status==O::DerivativeUnavailable && first.work.observations==2 && !first.complete,"first unscorable observation stops training");
    check(all.status==O::DerivativeUnavailable && all.complete && all.channels[0].derivativeUnavailable==1 && all.channels[0].primalInvalid==1 && !all.channels[0].normalizedMean,"all rows assessed with exact reason counts");
    check(all.loss.state()==FitnessState::Invalid,"no valid prefix score");
    auto only=D::create(1,{{0,{{{-1},-2},{{0},0},{{1},2}},1,1}});
    auto withAnchor=anchored();
    for(auto [offset,expected]:{std::pair{0.,.5},std::pair{1.,0.},std::pair{7.,18.}}) {
        auto f=Expression::binary(K::Add,Expression::binary(K::Multiply,x,x),Expression::constant(offset,1));
        check(only->measure(f).loss==Fitness::valid(0) && withAnchor->measure(f).loss==Fitness::valid(expected),"offset ambiguity and supplied anchor");
    }
    ExpressionCandidate a(x,p);a.calculateFitness();const auto old=a.evaluation();
    auto other=D::create(1,{{std::nullopt,{{{0},10}},2,3}});ExpressionCandidate b(x,other);b.calculateFitness();
    check(a.problemIdentity()!=b.problemIdentity() && old!=b.evaluation(),"different observations/scales/weights not interchangeable");
    (void)other->assess(a.expression());check(a.evaluation()==old,"reporting does not overwrite training fitness");
    a.replaceExpression(parseExpression("x0+1",1));check(a.evaluation().state()==FitnessState::Unevaluated,"edits invalidate fitness");
    RandomContext rng(4);P pop({b},rng,ObjectiveDirection::Minimize);const auto before=snapshot(pop);
    check(cause(failure([&]{pop.publishGeneration({a});})).find("problem changed")!=std::string::npos && snapshot(pop)==before,"problem identity retained at publication");
}
class LegacyMergeProbe final:public TrainingObjective {
public:
    using TrainingObjective::TrainingObjective;
    bool preserved() {return noexcept(TrainingObjective::mergeWorker(*this));}
protected:void mergeWorker(const TrainingObjective&) noexcept override {}
};
static void validationAndResources() {
    rejects<std::invalid_argument>([]{RegressionProblem empty({},{});});
    rejects<std::invalid_argument>([]{RegressionProblem empty({}, {}, {});});
    auto value=[](double scale=1,double weight=1){return ScalarObservationChannel{std::nullopt,{{{0},0}},scale,weight};};
    rejects([&]{(void)D::create(0,{value()});});rejects([&]{(void)D::create(1,{});});
    rejects([&]{(void)D::create(1,{value(),value()});});rejects([&]{(void)D::create(1,{{0,{},1,1}});});
    rejects([&]{(void)D::create(1,{{1,{{{0},0}},1,1}});});rejects([&]{(void)D::create(1,{{0,{{{0},0}},1,1},{0,{{{1},1}},1,1}});});
    for(double v:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        rejects([&]{(void)D::create(1,{value(v)});});rejects([&]{(void)D::create(1,{value(1,v)});});
    }
    rejects([&]{(void)D::create(2,{value()});});rejects([&]{(void)D::create(1,{{0,{{{0},INFINITY}},1,1}});});
    rejects([&]{(void)D::create(1,{{0,{{{INFINITY},0}},1,1}});});
    const double huge=std::numeric_limits<double>::max(),tiny=std::numeric_limits<double>::denorm_min();
    rejects([&]{(void)D::create(1,{value(1,huge),{0,{{{0},0}},1,huge}});});
    rejects([&]{(void)D::create(1,{value(1,tiny),{0,{{{0},0}},1,huge}});});
    auto limits=ObservationLimits{};limits.maxObservations=1;
    rejects<std::length_error>([&]{(void)D::create(1,{value(),{0,{{{0},0}},1,1}},{31,6,8},limits);});
    limits={};limits.maxInputCoefficients=1;rejects<std::length_error>([&]{(void)D::create(2,{{0,{{{0,0},0}},1,1}},{31,6,8},limits);});
    const auto constant=Expression::constant(1e200,1,{31,6,huge});auto normalized=D::create(1,{{std::nullopt,{{{0},0}},1e200,1}},{31,6,huge});
    const auto big=normalized->assess(constant);check(big.loss==Fitness::valid(1) && !big.channels[0].rawMSE && big.channels[0].maxAbsoluteError==1e200,"raw square overflow does not invalidate normalized objective");
    auto small=D::create(1,{{std::nullopt,{{{0},0}},1e-200,1}},{31,6,huge});const auto under=small->assess(Expression::constant(1e-200,1));
    check(under.loss==Fitness::valid(1) && !under.channels[0].rawMSE,"raw underflow is a diagnostic limitation");
    auto bad=D::create(1,{value(tiny)});check(bad->measureObservations(Expression::constant(1,1)).status==O::LossArithmeticFailure,"normalized loss failure separate");
    auto lost=D::create(1,{{std::nullopt,{{{0},0}},huge,1}});check(lost->measureObservations(Expression::constant(tiny,1)).status==O::LossArithmeticFailure,"normalization cannot silently zero a nonzero residual");
    auto p=anchored();check(LegacyMergeProbe(p).preserved(),"legacy noexcept merge hook preserved");
    auto e=parseExpression("x0*x0+1",1);auto baseline=p->assess(e);
    for(int kind=0;kind<5;++kind) {
        auto config=ObservationEvaluationLimits{};
        if(kind==0)config.maxObservations=9;
        if(kind==1)config.maxAggregationOperations=1;
        if(kind==2)config.partial.maxOperations=0;
        if(kind==3)config.maxValueOperations=0;
        if(kind==4)config.maxValueWorkspaceBytes=0;
        auto q=D::create(1,p->channels(),{15,6,8},{},config);DerivativeTrainingObjective objective(q,4);
        check(q->measureObservations(e).status==O::ResourceLimit,"training resource outcome");
        rejects([&]{(void)objective.measure(e);});
        check(objective.work().lossAttempts==1 && objective.work().invalidLosses==0 && objective.work().rowAttempts==objective.observationWork().observations,"resource stop counted and not invalid fitness");
        check(p->assess(e,config).status==O::ResourceLimit,"separate bounded reporting allowance");
    }
    auto bound=ObservationEvaluationLimits{};bound.maxObservations=baseline.work.observations;bound.maxAggregationOperations=baseline.work.aggregationOperations;
    check(p->assess(e,bound).loss==Fitness::valid(0),"inclusive aggregate work bounds");
    DerivativeTrainingObjective charged(p,4);fault::next=true;
    const auto operational=failure([&]{charged.measure(e);});
    bool allocationCause=false;
    auto nested=operational;
    while(nested) {
        try{std::rethrow_exception(nested);}
        catch(const std::bad_alloc&){allocationCause=true;break;}
        catch(const std::exception& error){const auto* wrapped=dynamic_cast<const std::nested_exception*>(&error);nested=wrapped?wrapped->nested_ptr():std::exception_ptr{};}
    }
    check(allocationCause,"operational allocation cause retained by type");
    check(charged.work().lossAttempts==1 && charged.work().rowAttempts==0 && charged.work().invalidLosses==0,"throwing objective attempt charged before allocation");
    bool failedAfterProgress=false;
    for(long allocation=0;allocation<18;++allocation) {
        DerivativeTrainingObjective probe(p,4);fault::after=allocation;
        try{(void)probe.measure(e);fault::after=-1;}
        catch(const std::exception&) {
            fault::after=-1;
            check(probe.work().lossAttempts==1 && probe.work().invalidLosses==0 && probe.work().rowAttempts==probe.observationWork().observations,"partial operational work merged before propagation");
            if(probe.observationWork().partialEvaluations && probe.observationWork().primalOperations)failedAfterProgress=true;
        }
    }
    check(failedAfterProgress,"allocation failure after observation progress exercised");


    const auto before=e.toString();fault::next=true;rejects<std::bad_alloc>([&]{(void)p->assess(e);});check(e.toString()==before,"operational allocation failure leaves expression owned and unchanged");
}
static void fitting() {
    ConstantFitConfig cfg;cfg.maxLossAttempts=512;
    auto coefficients=D::create(1,{{std::nullopt,{{{0},1.25}},1,1},{0,{{{-2},-6.5},{{-1.5},-5},{{-1},-3.5},{{-.5},-2},{{0},-.5},{{.5},1},{{1},2.5},{{1.5},4},{{2},5.5}},1,1}},{15,6,8});
    DerivativeTrainingObjective target(coefficients,512);auto initial=parseExpression("1*x0*x0+1*x0+1",1);auto f=fitConstants(initial,target,cfg);
    check(f.loss==Fitness::valid(0) && f.loss==coefficients->measure(f.expression).loss && target.work().lossAttempts<=512,"same composite objective for every fitting probe");
    const auto nodes=f.expression.nodes();check(nodes[0].constantValue()==1.5 && nodes[5].constantValue()==-.5 && nodes[9].constantValue()==1.25,"independent quadratic coefficients and anchor recovered");
    check(nodes.size()==initial.nodeCount() && target.work().rowAttempts==target.observationWork().observations,"structure and observation accounting");
    auto root=D::create(1,{{std::nullopt,{{{0},2.2}},1,1},{0,{{{0},.5}},1,1}});DerivativeTrainingObjective invalid(root,32);
    auto one=cfg;one.maxSweeps=1;one.maxLossAttempts=32;
    auto retained=fitConstants(parseExpression("0+sqrt(x0+1)",1),invalid,one);
    check(retained.expression.toString()=="(1.0 + sqrt((x0 + 1.0)))" && invalid.work().invalid>0 && retained.improved && root->measure(retained.expression).loss==retained.loss,"unavailable derivative probe retains earlier progress");
    check(root->assess(parseExpression("1+sqrt(x0+0)",1)).channels[0].primalInvalid==0,"ordinary value probe remains valid");
    const ScalarExpressionTemplate scaffold(parseExpression("(-1)*x0",1),1,{3,2,8},{5,3,8});
    auto observed=D::create(1,{{0,{{{1},-2}},1,1}},{5,3,8});
    auto domain=ScalarTemplateProblem::create(scaffold,observed,{0},{K::Multiply});DerivativeTrainingObjective frozen(observed,64);
    auto selected=fitScalarTemplateConstants(*domain,parseExpression("1*x0",1),frozen,cfg);
    check(selected.expression.nodes()[0].constantValue()==-1 && selected.loss==Fitness::valid(0) && domain->substitution(selected.expression).nodes()[0].constantValue()==2,"frozen scaffold excluded from composite fitting");
}
static void equivalence() {
    auto p=anchored(1.125);SymbolicRegressionConfig cfg;cfg.population=8;cfg.elites=1;cfg.maxEvaluations=32;cfg.maxTrainingLossAttempts=4096;cfg.maxGenerationAttempts=3;cfg.fitting=ConstantFitConfig{};cfg.fitting->maxLossAttempts=4;cfg.fitEveryGenerations=2;
    RandomContext serial(42,{1});auto reference=derivativeRegression(p,cfg,serial);
    check(reference.search.publishedGenerations==3,"nonbinding generations exercised");
    for(unsigned threads:{1u,2u,4u}) {
        RandomContext random(42,{threads});auto actual=derivativeRegression(p,cfg,random);auto& a=actual.search;auto& b=reference.search;
        check(a.best->text()==b.best->text() && a.best->evaluation()==b.best->evaluation() && a.preparedInitialExpressions==b.preparedInitialExpressions && a.preparedInitialFitness==b.preparedInitialFitness && a.rawInitialExpressions==b.rawInitialExpressions,"stable candidate order and fitness");
        check(a.trainingWork==b.trainingWork && actual.observationWork==reference.observationWork && a.variation==b.variation && a.publishedGenerations==b.publishedGenerations && a.stoppingReason==b.stoppingReason && a.bestGeneration==b.bestGeneration && random.engine()==serial.engine(),"nonbinding work/history/RNG equivalence");
        check(a.initialWork==b.initialWork && a.preparationWork==b.preparationWork && a.initialBestMSE==b.initialBestMSE && a.rawInitialBestMSE==b.rawInitialBestMSE && a.candidateEvaluations==b.candidateEvaluations && a.attemptedGenerations==b.attemptedGenerations && a.unpublishedLossAttempts==b.unpublishedLossAttempts && a.unpublishedRowAttempts==b.unpublishedRowAttempts,"initial, preparation and abandoned-work counters");
        check(a.improvements.size()==b.improvements.size(),"history size");for(std::size_t i=0;i<a.improvements.size();++i)check(a.improvements[i].mse==b.improvements[i].mse && a.improvements[i].lossAttempts==b.improvements[i].lossAttempts && a.improvements[i].generation==b.improvements[i].generation,"exact committed history");
    }
    auto diagnostics=std::make_shared<RefinementDiagnostics>(32);cfg.diagnostics=diagnostics;
    RandomContext unsupported(42,{2});auto state=unsupported.engine();
    rejects<std::invalid_argument>([&]{(void)derivativeRegression(p,cfg,unsupported);});
    check(unsupported.execution().requested()==2 && unsupported.engine()==state && unsupported.evaluationAttempts()==0,"parallel detailed diagnostics rejected without changing requested mode");
    RandomContext diagnosticRun(42);auto observed=derivativeRegression(p,cfg,diagnosticRun);
    check(observed.search.best->text()==reference.search.best->text() && observed.search.trainingWork==reference.search.trainingWork && observed.observationWork==reference.observationWork && diagnosticRun.engine()==serial.engine(),"serial diagnostics preserve computations and randomness");

}
struct State {
    enum Mode{None,Throw,Cancel,Resource};std::atomic<Mode> mode{None};std::atomic<std::uint64_t> starts{0};std::atomic<bool> progress{false};
    std::barrier<> gate{2};std::mutex mutex;std::condition_variable ready;ExecutionContext* execution=nullptr;
};
class Controlled final:public DerivativeTrainingObjective {
public:
    Controlled(std::shared_ptr<const D> p,std::uint64_t maximum,std::shared_ptr<State> s):DerivativeTrainingObjective(std::move(p),maximum),state_(std::move(s)){}
    bool parallelEvaluationSafe() const noexcept override{return true;}
protected:
    std::shared_ptr<TrainingObjective> cloneForParallel() const override{return std::make_shared<Controlled>(observationProblem(),maximum(),state_);}
    RegressionMetrics compute(const Expression& e) override {
        ++state_->starts;auto mode=state_->mode.load();
        if(mode!=State::None) {
            if(work().accepted>0){state_->progress=true;state_->ready.notify_all();}
            if(work().lossAttempts==1) {
                state_->gate.arrive_and_wait();
                if(e.nodes()[0].constantValue()==4) {
                    std::unique_lock lock(state_->mutex);if(!state_->ready.wait_for(lock,std::chrono::seconds(5),[&]{return state_->progress.load();}))throw std::runtime_error("no sibling fitting progress");
                    if(mode==State::Throw)throw std::runtime_error("observation worker fault after private improvement");
                    if(mode==State::Resource){ObservationMetrics m;m.resource=ObservationResource::Partial;throw ObservationResourceExhausted(m);}
                    state_->execution->requestStop();
                }
            }
        }
        return DerivativeTrainingObjective::compute(e);
    }
private:std::shared_ptr<State> state_;
};
static void overlap() {
    for(auto mode:{State::Throw,State::Cancel,State::Resource}) {
        auto observations=anchored()->channels();
        for(auto& row:observations[1].observations)row.target*=1.5;
        auto p=D::create(1,observations,{15,6,8});auto state=std::make_shared<State>();auto objective=std::make_shared<Controlled>(p,4096,state);ConstantFitConfig fit;fit.maxLossAttempts=4;
        auto preparation=std::make_shared<ConstantRefinement>(objective,fit);
        RandomContext random(5,{2});state->execution=&random.execution();
        auto candidate=[&](double a){return ExpressionCandidate(parseExpression(std::to_string(a)+"*x0*x0+1",1),p,preparation);};
        P pop({candidate(0),candidate(4)},random,ObjectiveDirection::Minimize);auto before=snapshot(pop);auto best=pop.bestSeen()->text();auto score=pop.bestSeen()->evaluation();auto rng=random.engine();auto generation=pop.generation();
        check(score.score()>0,"committed archive leaves room for a strictly better private candidate");
        state->mode=mode;auto error=failure([&]{pop.publishGeneration({candidate(.5),candidate(4)});});
        if(mode==State::Throw)check(cause(error)=="observation worker fault after private improvement","original exception cause");
        else if(mode==State::Cancel)check(isEvaluationCancelled(error),"cancel classification");
        else check(cause(error)=="scalar observation objective resource exhausted","resource not mathematical invalidity");
        check(snapshot(pop)==before && pop.generation()==generation && pop.bestSeen()->text()==best && pop.bestSeen()->evaluation()==score && random.engine()==rng,"failed attempt preserves committed state and RNG evaluation isolation");
        check(state->progress && random.execution().peakTasks()==2 && objective->work().lossAttempts==state->starts,"overlapping fitting with private accepted progress and honest starts");
        state->mode=State::None;check(pop.publishGeneration({candidate(.5),candidate(4)})==GenerationOutcome::Committed && pop.generation()==1 && pop.bestSeen()->fitness()==0,"drained tasks and schedule reusable");
    }
    auto p=anchored();auto objective=std::make_shared<DerivativeTrainingObjective>(p,9);ConstantFitConfig fit;fit.maxLossAttempts=4;
    auto preparation=std::make_shared<ConstantRefinement>(objective,fit);RandomContext random(8,{2});
    ExpressionCandidate c(parseExpression("0*x0*x0+1",1),p,preparation);P pop({c,c},random,ObjectiveDirection::Minimize);auto before=snapshot(pop);auto best=pop.bestSeen()->text();
    auto error=failure([&]{pop.publishGeneration({c,c});});check(isEvaluationBudget(error) && objective->work().lossAttempts==9 && snapshot(pop)==before && pop.generation()==0 && pop.bestSeen()->text()==best,"binding global budget preserves publication and counts failed work");
    auto unavailable=parseExpression("sqrt(x0*x0)",1);RandomContext invalidRun(9,{2});P all({ExpressionCandidate(unavailable,p),ExpressionCandidate(unavailable,p)},invalidRun,ObjectiveDirection::Minimize);
    check(!all.generation() && !all.bestSeen(),"required unavailable derivatives cannot publish all-invalid initial population");
    auto bound=ObservationEvaluationLimits{};bound.maxValueOperations=1;
    auto restricted=D::create(1,p->channels(),{15,6,8},{},bound);
    auto local=std::make_shared<DerivativeTrainingObjective>(restricted,64);
    auto noFit=std::make_shared<ConstantRefinement>(local);
    RandomContext resourceRun(10,{2});
    ExpressionCandidate initial(parseExpression("x0+1",1),restricted,noFit);
    P protectedPopulation({initial,initial},resourceRun,ObjectiveDirection::Minimize);
    const auto saved=snapshot(protectedPopulation);auto bestValue=protectedPopulation.bestSeen()->evaluation();
    ExpressionCandidate costly(parseExpression("x0*x0+1",1),restricted,noFit);
    auto resourceError=failure([&]{protectedPopulation.publishGeneration({costly,costly});});
    check(cause(resourceError)=="scalar observation objective resource exhausted" && snapshot(protectedPopulation)==saved && protectedPopulation.generation()==0 && protectedPopulation.bestSeen()->evaluation()==bestValue,"actual resource stop preserves committed population");
    check(local->work().invalidLosses==0 && local->observationWork().resourceStops>0,"actual resource failure remains separate from invalid losses");

}
int main(int argc,char**) {
    try{if(argc>1)overlap();else{lossAndIdentity();validationAndResources();fitting();equivalence();overlap();}std::cout<<"derivative regression contracts passed\n";}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
