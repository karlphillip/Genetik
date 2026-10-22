#include <IntegralRegression.hpp>
#include <ExpressionParsing.hpp>
#include <barrier>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <new>
#include <cstdlib>
#include <bit>
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
using S=IntegralStatus;using R=IntegralReason;using K=ExpressionKind;
using P=Population<ExpressionCandidate,RegressionOrdering>;
static Expression expr(std::string_view text,std::size_t n=2){return parseExpression(text,n);}
static IntegralExpression model(std::string_view f,std::string_view a="0",std::string_view b="1",IntegralPolicy p={}){return IntegralExpression(1,expr(a,1),expr(b,1),expr(f),1,p);}
static void near(double a,double b,double tolerance,const char* text){check(std::isfinite(a) && std::abs(a-b)<=tolerance,text);}
static std::uint64_t bits(double x){return std::bit_cast<std::uint64_t>(x);}
static void numerical() {
    // Independent analytic references, rounded from 80/120-digit Decimal fixtures.
    for(auto [text,reference]:{std::pair{"x1*x1",.333333333333333333333333333333333333},
            {"exp(x1)",1.71828182845904523536028747135266250},
            {"1/(1+x1*x1)",.785398163397448309615660845819875721},
            {"sqrt(x1)",.666666666666666666666666666666666667}}) {
        auto r=model(text).evaluate(std::array{0.});check(r.accepted(),"known proper integral accepted");near(*r.estimate,reference,2e-10,"independent known integral");
        check(*r.estimatedAbsoluteError<=std::max(1e-10,1e-10*std::abs(*r.estimate)),"estimated acceptance criterion");
        check(r.work.integrandEvaluations==17+30*r.work.subdivisions,"17+30k complete sample accounting");
    }
    auto logarithm=model("log(x1)","1","2").evaluate(std::array{0.});near(*logarithm.estimate,.386294361119890618834464242916353136,2e-10,"independent log integral");
    auto affine=model("2*x1+1","0","x0");std::array input{2.};auto forward=affine.evaluate(input);
    check(input[0]==2 && forward.accepted(),"private bindings do not modify external inputs");near(*forward.estimate,6,1e-12,"affine integral");
    auto parameter=model("x0*x1","0","x0");for(double x:{-3.,0.,2.}){auto r=parameter.evaluate(std::array{x});check(r.accepted(),"parameter-dependent integrand");near(*r.estimate,x*x*x/2,1e-12,"external and local slots independent");}
    IntegralExpression twoInputs(2,expr("0",2),expr("x0+x1",2),expr("x0*x2+x1",3),2);
    const std::array supplied{2.,3.};auto two=twoInputs.evaluate(supplied);
    check(two.accepted() && supplied==std::array{2.,3.} && two.work.bindingsValidated==55,"external/local binding storage and accounting with N=2");
    near(*two.estimate,40,1e-12,"two external parameters and appended local are independent");
    auto reversed=model("2*x1+1","x0","0").evaluate(input);check(bits(*reversed.estimate)==bits(-*forward.estimate) && bits(*reversed.estimatedAbsoluteError)==bits(*forward.estimatedAbsoluteError),"orientation only negates final estimate");
    auto zero=model("log(x1)","(-0.0)","0").evaluate(input);check(zero.accepted() && bits(*zero.estimate)==bits(0.) && *zero.estimatedAbsoluteError==0 && zero.work.integrandEvaluations==0 && zero.work.boundEvaluations==2,"empty interval convention");
    auto cancellation=model("x1","(-1)","1").evaluate(input);check(cancellation.accepted() && *cancellation.estimate==0 && *cancellation.estimatedAbsoluteError>0,"absolute tolerance under cancellation");
    const auto endpoint=model("log(x1)").evaluate(input);check(endpoint.status==S::MathematicalInvalidity && endpoint.reason==R::Integrand && endpoint.work.integrandEvaluations==1,"invalid original endpoint counted");
    auto interior=model("1/(x1-0.5)").evaluate(input);check(interior.status==S::MathematicalInvalidity && interior.work.integrandEvaluations==3,"invalid center sample");
    auto bound=model("x1","log(-1)").evaluate(input);check(bound.status==S::MathematicalInvalidity && bound.reason==R::LowerBound && bound.work.boundEvaluations==1,"bounds lower then upper");
    auto upper=model("x1","0","1/0").evaluate(input);check(upper.reason==R::UpperBound && upper.work.boundEvaluations==2,"invalid upper cannot be hidden");
    auto overflow=model("1.7976931348623157e308").evaluate(input);check(overflow.status==S::NumericalFailure && overflow.work.integrandEvaluations==17,"finite samples overflowing checked quadrature");
    auto extreme=model("0","(-1.7976931348623157e308)","1.7976931348623157e308").evaluate(input);check(extreme.accepted() && *extreme.estimate==0,"opposite extreme mapping avoids overflowing subtraction");
    check(model("0","8.988465674311579e307","1.7976931348623157e308").evaluate(input).accepted(),"large same-sign mapping");
    auto limits=IntegralPolicy{};limits.absTolerance=1e-30;limits.relTolerance=0;
    auto tight=model("x1*x1","0","1",limits).evaluate(input);check(tight.status==S::Nonconvergence && tight.reason==R::Roundoff && tight.estimate,"roundoff floor does not claim tight convergence");
    auto alias=model("exp((-1)*powi((x1-0.123456789)*1000000,2))").evaluate(input);
    check(alias.accepted() && *alias.estimate==0 && *alias.estimatedAbsoluteError==0,"concrete missed narrow-feature estimator limitation"); // true integral >= 2e-6/e > 7e-7
}
static void limitsAndOwnership() {
    const std::array input{0.};auto baseline=model("x1*x1").evaluate(input);
    for(auto cap:{16u,17u,18u}){auto p=IntegralPolicy{};p.maxIntegrandEvaluations=cap;auto r=model("x1*x1","0","1",p).evaluate(input);check(r.accepted()==(cap>=17) && r.work.integrandEvaluations==std::min(17u,cap),"sample work boundary");if(cap==16)check(!r.estimate,"incomplete first panel not exposed");}
    auto p=IntegralPolicy{};p.maxIntegrandEvaluations=32;auto split=model("sqrt(x1)","0","1",p).evaluate(input);
    p.maxIntegrandEvaluations=17;auto parent=model("sqrt(x1)","0","1",p).evaluate(input);
    check(split.status==S::ResourceLimit && split.work.integrandEvaluations==32 && split.work.completedPanels==2 && split.work.subdivisions==0 && split.activeSubintervals==1 && bits(*split.estimate)==bits(*parent.estimate) && bits(*split.estimatedAbsoluteError)==bits(*parent.estimatedAbsoluteError),"failed right child retains complete parent partition");
    p=IntegralPolicy{};p.maxSubintervals=1;check(model("sqrt(x1)","0","1",p).evaluate(input).reason==R::Panels,"panel capacity");
    p=IntegralPolicy{};p.maxDepth=0;check(model("sqrt(x1)","0","1",p).evaluate(input).reason==R::Depth,"subdivision depth");
    for(auto which:{0,1,2,3,4}) {
        for(int delta:{-1,0,1}) {
            p=IntegralPolicy{};auto count=which==0?baseline.work.expressionOperations:which==1?baseline.work.nodeVisits:which==2?baseline.work.quadratureOperations:which==3?baseline.work.workspaceBytes:baseline.work.bindingsValidated;
            auto cap=static_cast<std::size_t>(count+delta);
            if(which==0)p.maxExpressionOperations=cap;else if(which==1)p.maxNodeVisits=cap;else if(which==2)p.maxQuadratureOperations=cap;else if(which==3)p.maxWorkspaceBytes=cap;else p.maxBindingsValidated=cap;
            auto r=model("x1*x1","0","1",p).evaluate(input);check(r.accepted()==(delta>=0),"exact resource accounting boundary");
        }
    }
    for(auto [a,b]:{std::pair{"0","4.9406564584124654e-324"},{"1","1.0000000000000002"},{"1","1.0000000000000018"}}){auto r=model("1",a,b).evaluate(input);check(r.status==S::Resolution && !r.estimate && r.work.integrandEvaluations==2,"nonzero collapsed interval not silently zero");}
    p=IntegralPolicy{};p.absTolerance=0;p.relTolerance=0;rejects<std::invalid_argument>([&]{(void)model("x1","0","1",p);});
    for(double bad:{-1.,std::numeric_limits<double>::infinity()}){p=IntegralPolicy{};p.absTolerance=bad;rejects<std::invalid_argument>([&]{(void)model("x1","0","1",p);});}
    rejects<std::invalid_argument>([]{(void)IntegralExpression(1,expr("0",1),expr("1",1),expr("x1"),0);});
    rejects<std::invalid_argument>([]{(void)IntegralExpression(1,expr("0",2),expr("1",1),expr("x1"),1);});
    rejects<std::invalid_argument>([&]{(void)model("x1").evaluate(std::array{std::numeric_limits<double>::infinity()});});
    rejects<std::invalid_argument>([&]{(void)model("x1").evaluate({});});
    p=IntegralPolicy{};p.maxSubintervals=std::numeric_limits<std::size_t>::max();p.maxWorkspaceBytes=std::numeric_limits<std::size_t>::max();
    check(model("x1","0","1",p).evaluate(input).reason==R::Workspace,"checked panel storage arithmetic before allocation");
    p=IntegralPolicy{};p.maxModelNodes=4;rejects<std::length_error>([&]{(void)model("x1*x1","0","1",p);});
    p.maxModelNodes=5;check(model("x1*x1","0","1",p).evaluate(input).accepted(),"exact model-node allowance");
    IntegralExpression constantModel(0,expr("0",0),expr("1",0),expr("2",1),0);near(*constantModel.evaluate({}).estimate,2,1e-12,"zero-external-input schema");
    auto g=expr("powi(x1,3)+(-0.0)");auto lower=expr("(-0.0)",1);IntegralExpression owned(1,lower,expr("x0",1),g,1);auto copy=owned;
    g=expr("x0");lower=expr("1",1);
    check(copy.integrand().toString()=="(powi(x1,3) + (-0.0))" && std::signbit(copy.lower().nodes()[0].constantValue()),"owned expression fields and exponent/signed zero export");
    check(copy.toString().find("local=x1")!=std::string::npos && copy.toString().find("policy[gk7/15")!=std::string::npos,"faithful binding and numerical policy inspection");
    auto parsed=parseExpression(copy.integrand().toString(),2);check(parsed.sameRepresentation(copy.integrand()),"ordinary field parser round trip only");
    ExecutionContext cancelled({2});cancelled.requestStop();auto stopped=copy.evaluate(input,&cancelled);check(stopped.status==S::Cancelled && stopped.work.integrandEvaluations==0,"explicit cancellation before work");
    for(int i=0;i<12;++i){fault::after=i;try{(void)copy.evaluate(std::array{1.});}catch(const std::bad_alloc&){}fault::after=-1;check(copy.evaluate(std::array{1.}).accepted(),"independent invocation after allocation failure");}
}
static auto problem(double a=1.5,double b=-.5,IntegralPolicy policy={},ExpressionLimits limits={9,4,8}) {
    std::vector<RegressionRow> rows;for(double x:{-2.,-1.5,-1.,-.5,.5,1.,1.5,2.})rows.push_back({{x},a*x*x/2+b*x});
    return IntegralRegressionProblem::create(1,expr("0",1),expr("x0",1),rows,limits,policy,IntegralRestrictions{{std::vector<std::size_t>{1}},{std::vector<K>{K::Add,K::Subtract,K::Multiply}}});
}
static void fittingAndIdentity() {
    auto p=IntegralRegressionProblem::create(1,expr("0",1),expr("x0",1),{{{1},1},{{2},6}});
    auto m=p->assess(expr("2"));near(m.loss.score(),2.5,1e-12,"independent full-model loss differs from pointwise 8.5");
    auto frozen=problem();IntegralTrainingObjective objective(frozen,512);ConstantFitConfig fit;fit.maxLossAttempts=512;
    auto initial=expr("1*x1+1");auto f=fitConstants(initial,objective,fit);
    check(frozen->measure(f.expression).loss==f.loss && f.expression.nodeCount()==initial.nodeCount(),"full-model probes match accepted score and structure");
    near(f.expression.nodes()[0].constantValue(),1.5,1e-6,"fitted slope");near(f.expression.nodes()[3].constantValue(),-.5,1e-6,"fitted intercept");
    check(frozen->lower().toString()=="0.0" && frozen->upper().toString()=="x0" && frozen->model(f.expression).localIndex()==1,"frozen bounds/binding not fitted");
    rejects<std::invalid_argument>([&]{(void)frozen->measure(expr("x0"));});
    auto other=problem(1,1);ExpressionCandidate c(initial,frozen);c.calculateFitness();const auto fitness=c.evaluation();(void)other->assess(initial);check(c.evaluation()==fitness,"assessment does not change cached training fitness");
    auto identityRun=RandomContext(3);P contextPopulation({c},identityRun,ObjectiveDirection::Minimize);
    ExpressionCandidate alien(initial,other);
    check(cause(failure([&]{contextPopulation.publishGeneration({alien});})).find("problem changed")!=std::string::npos && contextPopulation.generation()==0 && contextPopulation.bestSeen()->evaluation()==fitness,"full immutable objective identity at publication");
    c.replaceExpression(expr("2*x1+1"));check(c.evaluation().state()==FitnessState::Unevaluated,"genome edit invalidation");
    auto root=IntegralRegressionProblem::create(1,expr("0",1),expr("1",1),{{{0},3.2}},{9,4,8});IntegralTrainingObjective invalid(root,32);fit.maxLossAttempts=32;fit.maxSweeps=1;fit.initialStep=2;
    auto retained=fitConstants(expr("0+sqrt(1)"),invalid,fit);check(retained.improved && retained.expression.toString()=="(2.0 + sqrt(1.0))","accepted progress before domain-invalid sqrt probe");
    check(invalid.work().invalid>0 && root->measure(retained.expression).loss==retained.loss,"invalid integral probes charged and rejected");
    auto low=IntegralPolicy{};low.maxIntegrandEvaluations=16;auto exhausted=problem(1.5,-.5,low);IntegralTrainingObjective resource(exhausted,4);
    auto error=failure([&]{(void)fitConstants(initial,resource,fit);});check(cause(error)=="integral resource_limit" && resource.work().lossAttempts==1 && resource.work().invalidLosses==0 && resource.integralWork().integrandEvaluations==16,"resource interruption not invalid fitness or partial local fit");
    auto failing=IntegralRegressionProblem::create(1,expr("0",1),expr("1",1),{{{0},0}}, {9,4,8},IntegralPolicy{1e-30,0});IntegralTrainingObjective numerical(failing,4);
    check(cause(failure([&]{(void)numerical.measure(expr("1"));}))=="integral nonconvergence" && numerical.work().invalidLosses==0,"nonconvergence is distinct from mathematical invalidity");
    auto ownership=std::vector<RegressionRow>{{{1},1}};auto owned=IntegralRegressionProblem::create(1,expr("0",1),expr("x0",1),ownership);ownership[0].inputs[0]=100;ownership[0].target=100;near(owned->measure(expr("1")).loss.score(),0,1e-25,"owned observation rows");
    rejects<std::length_error>([]{IntegralDataLimits l;l.maxWorstCaseSamples=1;(void)IntegralRegressionProblem::create(1,expr("0",1),expr("1",1),{{{0},0}},{9,4,2},{},{},l);});
    rejects<std::invalid_argument>([]{(void)IntegralRegressionProblem::create(1,expr("0",1),expr("1",1),{{{INFINITY},0}});});
}
static void equivalence() {
    auto p=problem(1.5,-.375);SymbolicRegressionConfig cfg;cfg.population=8;cfg.elites=1;cfg.maxEvaluations=32;cfg.maxTrainingLossAttempts=4096;cfg.maxGenerationAttempts=3;cfg.fitting=ConstantFitConfig{};cfg.fitting->maxLossAttempts=4;cfg.fitEveryGenerations=2;
    RandomContext serial(42,{1});auto reference=integralRegression(p,cfg,serial);check(reference.search.publishedGenerations==3,"nonbinding complete generations");
    for(unsigned threads:{1u,2u,4u}) {
        RandomContext random(42,{threads});auto actual=integralRegression(p,cfg,random);auto& a=actual.search;auto& b=reference.search;
        check(a.best->text()==b.best->text() && a.best->evaluation()==b.best->evaluation() && a.preparedInitialExpressions==b.preparedInitialExpressions && a.preparedInitialFitness==b.preparedInitialFitness && a.rawInitialExpressions==b.rawInitialExpressions,"stable initialization and results");
        check(a.trainingWork==b.trainingWork && actual.integralWork==reference.integralWork && a.variation==b.variation && a.publishedGenerations==b.publishedGenerations && a.stoppingReason==b.stoppingReason && a.bestGeneration==b.bestGeneration && random.engine()==serial.engine(),"nonbinding counters/history/RNG equivalence");
        check(a.initialWork==b.initialWork && a.preparationWork==b.preparationWork && a.candidateEvaluations==b.candidateEvaluations && a.attemptedGenerations==b.attemptedGenerations && a.unpublishedLossAttempts==b.unpublishedLossAttempts,"initial and abandoned-work accounting");
        check(a.improvements.size()==b.improvements.size(),"history size");for(std::size_t i=0;i<a.improvements.size();++i)check(a.improvements[i].mse==b.improvements[i].mse && a.improvements[i].lossAttempts==b.improvements[i].lossAttempts && a.improvements[i].generation==b.improvements[i].generation,"exact committed improvement history");
    }
    cfg.grammar.operators.push_back(K::Divide);RandomContext rejected(42);auto rng=rejected.engine();rejects<std::invalid_argument>([&]{(void)integralRegression(p,cfg,rejected);});check(rng==rejected.engine() && rejected.evaluationAttempts()==0,"grammar rejected before work/RNG");
    auto diagnostics=std::make_shared<RefinementDiagnostics>(32);cfg.grammar.operators.pop_back();cfg.diagnostics=diagnostics;RandomContext parallel(42,{2});rejects([&]{(void)integralRegression(p,cfg,parallel);});check(parallel.evaluationAttempts()==0 && parallel.execution().requested()==2,"incompatible diagnostics do not change requested execution");
}
static auto snapshot(const P& p){std::vector<std::pair<std::string,Fitness>> s;const auto c=p.creatures();for(const auto& x:*c)s.push_back({x.text(),x.evaluation()});return s;}
struct State {
    enum Mode{None,Throw,Cancel,Resource};std::atomic<Mode> mode{None};std::atomic<std::uint64_t> starts{0};std::atomic<bool> progress{false};
    std::barrier<> gate{2};std::mutex mutex;std::condition_variable ready;ExecutionContext* execution=nullptr;
};
class Controlled final:public IntegralTrainingObjective {
public:
    Controlled(std::shared_ptr<const IntegralRegressionProblem> p,std::uint64_t maximum,std::shared_ptr<State> s):IntegralTrainingObjective(p,maximum),problem_(std::move(p)),state_(std::move(s)){}
    bool parallelEvaluationSafe() const noexcept override{return true;}
protected:
    std::shared_ptr<TrainingObjective> cloneForParallel() const override{return std::make_shared<Controlled>(problem_,maximum(),state_);}
    RegressionMetrics compute(const Expression& e) override {
        ++state_->starts;auto mode=state_->mode.load();
        if(mode!=State::None) {
            if(work().accepted>0){state_->progress=true;state_->ready.notify_all();}
            if(work().lossAttempts==1) {
                state_->gate.arrive_and_wait();
                if(e.nodes()[0].constantValue()==4) {
                    std::unique_lock lock(state_->mutex);if(!state_->ready.wait_for(lock,std::chrono::seconds(5),[&]{return state_->progress.load();}))throw std::runtime_error("no sibling integral fitting progress");
                    if(mode==State::Throw)throw std::runtime_error("integral worker fault after private improvement");
                    if(mode==State::Resource){IntegralResult m;m.status=S::ResourceLimit;m.reason=R::Samples;throw IntegralResourceExhausted(m);}
                    state_->execution->requestStop();
                }
            }
        }
        return IntegralTrainingObjective::compute(e);
    }
private:std::shared_ptr<const IntegralRegressionProblem> problem_;std::shared_ptr<State> state_;
};
static void overlap() {
    // Shared immutable model, distinct private binding/scratch and differing bounds.
    const auto shared=model("x0*x1","0","x0");std::barrier gate(2);std::array<IntegralResult,2> outcomes;
    std::jthread a([&]{gate.arrive_and_wait();outcomes[0]=shared.evaluate(std::array{2.});});
    std::jthread b([&]{gate.arrive_and_wait();outcomes[1]=shared.evaluate(std::array{-3.});});a.join();b.join();
    near(*outcomes[0].estimate,4,1e-12,"overlapping positive bound");near(*outcomes[1].estimate,-13.5,1e-12,"overlapping reversed bound");
    for(auto mode:{State::Throw,State::Cancel,State::Resource}) {
        auto p=problem(1.5,1);auto state=std::make_shared<State>();auto objective=std::make_shared<Controlled>(p,4096,state);ConstantFitConfig fit;fit.maxLossAttempts=4;
        auto preparation=std::make_shared<ConstantRefinement>(objective,fit);RandomContext random(5,{2});state->execution=&random.execution();
        auto candidate=[&](double c){return ExpressionCandidate(expr(std::to_string(c)+"*x1+1"),p,preparation);};
        P population({candidate(0),candidate(4)},random,ObjectiveDirection::Minimize);const auto before=snapshot(population);const auto best=population.bestSeen()->text();const auto fitness=population.bestSeen()->evaluation();auto rng=random.engine();
        check(fitness.score()>0,"committed archive permits better private fit");
        state->mode=mode;const auto error=failure([&]{population.publishGeneration({candidate(.5),candidate(4)});});
        if(mode==State::Throw)check(cause(error)=="integral worker fault after private improvement","original operational cause");
        else if(mode==State::Cancel)check(isEvaluationCancelled(error),"cooperative cancellation");
        else check(cause(error)=="integral resource_limit","resource not invalid fitness");
        check(snapshot(population)==before && population.generation()==0 && population.bestSeen()->text()==best && population.bestSeen()->evaluation()==fitness && random.engine()==rng,"failure preserves committed population/archive/generation and evaluation RNG");
        check(state->progress && random.execution().peakTasks()==2 && objective->work().lossAttempts==state->starts,"actual preparation overlap and charged attempts");
        state->mode=State::None;check(population.publishGeneration({candidate(.5),candidate(4)})==GenerationOutcome::Committed && population.generation()==1 && population.bestSeen()->fitness()<1e-25,"drained tasks support same-generation retry");
    }
    auto p=problem();auto objective=std::make_shared<IntegralTrainingObjective>(p,9);ConstantFitConfig fit;fit.maxLossAttempts=4;auto prep=std::make_shared<ConstantRefinement>(objective,fit);RandomContext run(8,{2});
    ExpressionCandidate c(expr("0*x1+1"),p,prep);P population({c,c},run,ObjectiveDirection::Minimize);auto before=snapshot(population);auto best=population.bestSeen()->text();
    auto error=failure([&]{population.publishGeneration({c,c});});check(isEvaluationBudget(error) && objective->work().lossAttempts==9 && snapshot(population)==before && population.bestSeen()->text()==best && population.generation()==0,"global loss cap and failed publication");
    auto low=IntegralPolicy{};low.maxExpressionOperations=18;
    auto restricted=problem(1.5,-.5,low);auto measured=std::make_shared<IntegralTrainingObjective>(restricted,64);auto noFit=std::make_shared<ConstantRefinement>(measured);RandomContext r(7,{2});
    ExpressionCandidate cheap(expr("x1"),restricted,noFit);P protectedPopulation({cheap,cheap},r,ObjectiveDirection::Minimize);auto saved=snapshot(protectedPopulation);auto archive=protectedPopulation.bestSeen()->evaluation();
    ExpressionCandidate costly(expr("x1*x1+x1"),restricted,noFit);auto resourceError=failure([&]{protectedPopulation.publishGeneration({costly,costly});});
    check(cause(resourceError)=="integral resource_limit" && snapshot(protectedPopulation)==saved && protectedPopulation.generation()==0 && protectedPopulation.bestSeen()->evaluation()==archive && measured->work().invalidLosses==0 && measured->integralWork().resources>0,"actual integration resource failure preserves committed state and counts work");
    auto invalidProblem=IntegralRegressionProblem::create(1,expr("0",1),expr("1",1),{{{0},0}},{9,4,8});RandomContext invalidRun(8,{2});ExpressionCandidate invalid(expr("log(x1)"),invalidProblem);P all({invalid,invalid},invalidRun,ObjectiveDirection::Minimize);check(!all.generation() && !all.bestSeen(),"all invalid endpoints do not publish");
    bool afterProgress=false;const auto supplied=expr("x1");
    for(int i=0;i<64;++i){IntegralTrainingObjective o(p,4);fault::after=i;try{(void)o.measure(supplied);}catch(const std::exception& e){fault::after=-1;if(cause(e)=="std::bad_alloc"){check(o.work().lossAttempts<=1,"charged allocation failure count");if(o.integralWork().integrals>0)afterProgress=true;}}fault::after=-1;}
    check(afterProgress,"operational fault after integral admission retains supplemental work");
}
int main(int argc,char**) {
    try{if(argc>1)overlap();else{numerical();limitsAndOwnership();fittingAndIdentity();equivalence();overlap();}std::cout<<"integral contracts passed\n";}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
