#include <ScalarTemplateRegression.hpp>
#include <ExpressionParsing.hpp>
#include "../examples/ScalarTemplateData.hpp"
#include <atomic>
#include <barrier>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <iostream>
#include <iomanip>
#include <new>

// Same test-only allocation-fault technique as the parser/workspace fixtures.
namespace template_fault {thread_local bool next=false;}
void* operator new(std::size_t n){if(template_fault::next){template_fault::next=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
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
using K=ExpressionKind;using namespace scalar_template_example;
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class F>static void rejects(F f){bool threw=false;try{f();}catch(const std::invalid_argument&){threw=true;}catch(const std::length_error&){threw=true;}catch(const std::out_of_range&){threw=true;}check(threw,"expected structural/configuration rejection");}
static std::string cause(const std::exception& e){try{std::rethrow_if_nested(e);}catch(const std::exception& x){return cause(x);}catch(...){}return e.what();}
template<class F>static std::exception_ptr failure(F f){try{f();}catch(...){return std::current_exception();}throw std::runtime_error("expected failure");}
static std::string cause(std::exception_ptr p){try{std::rethrow_exception(p);}catch(const std::exception& e){return cause(e);}}
static auto context(std::shared_ptr<const ScalarTemplateProblem> domain,std::shared_ptr<RegressionPreparation> p={}){return std::make_shared<const ScalarTemplateContext>(std::move(domain),std::move(p));}
static auto candidate(std::shared_ptr<const ScalarTemplateContext> c,const Expression& hole){return ExpressionCandidate::constrained(c->domain()->materialize(hole),c);}
static void structure() {
    auto d=problem(data(0));auto c=context(d);auto hole=parseExpression("x1*x1+1",2);const auto full=d->materialize(hole);
    check(full.nodeCount()==9 && full.depth()==5 && d->expressionTemplate().frozenNodes()==4 && d->expressionTemplate().ancestorDepth()==2,"full model accounting");
    check(full.evaluate(std::array{2.,3.}).value()==-20,"independent full model value");
    check(full.sameRepresentation(parseExpression(full.toString(),2)) && d->substitution(full).sameRepresentation(hole),"export/parser/selected subtree round trip");
    for(auto text:{"x0","x0-x0","0*x0","sin(x1)"})rejects([&]{(void)d->materialize(parseExpression(text,2));});
    rejects([&]{(void)d->materialize(Expression::constant(1,1));});
    rejects([&]{d->validateSubstitution(Expression::constant(3,2));});
    auto parent=candidate(c,hole);parent.calculateFitness();const auto score=parent.evaluation();auto clone=parent;
    clone.replaceExpression(d->materialize(Expression::constant(0,2)));
    check(clone.fitnessState()==FitnessState::Unevaluated && parent.evaluation()==score && parent.expression().sameRepresentation(full),"clone ownership and fitness invalidation");
    const auto changedFrozen=full.withConstant(0,1);rejects([&]{parent.replaceExpression(changedFrozen);});
    check(parent.evaluation()==score && parent.expression().sameRepresentation(full),"failed replacement retains cache");
    std::vector<std::size_t> variables{1};std::vector<K> operations{K::Add,K::Subtract,K::Multiply};
    auto independent=ScalarTemplateProblem::create(d->expressionTemplate(),d->problem(),variables,operations);variables[0]=0;operations[0]=K::Log;
    check(independent->allowedVariables()==std::vector<std::size_t>{1} && independent->allowedOperators()[0]==K::Add,"caller storage detached");
    auto t=d->expressionTemplate();check(t.materialize(hole).sameRepresentation(full),"independent scaffold copy");
    auto different=context(d);auto other=candidate(different,hole);other.calculateFitness();RandomContext r(1);
    rejects([&]{Population<ExpressionCandidate,RegressionOrdering> mixed({parent,other},r,ObjectiveDirection::Minimize);});
    auto changedRows=data(0);changedRows[0].target+=1;auto differentData=problem(changedRows);auto differentCandidate=candidate(context(differentData),hole);differentCandidate.calculateFitness();
    check(differentCandidate.evaluation()!=parent.evaluation() && differentCandidate.problemIdentity()!=parent.problemIdentity(),"same schema different targets/contexts distinct");
    const auto cached=parent.evaluation();(void)RegressionProblem::create(data(2),d->problem()->limits())->measure(full);check(parent.evaluation()==cached,"assessment never changes training cache");
    rejects([&]{(void)ScalarTemplateProblem::create(t,d->problem(),{2},{K::Add});});
    rejects([&]{(void)ScalarTemplateProblem::create(t,d->problem(),{1,1},{K::Add});});
    rejects([&]{(void)ScalarTemplateProblem::create(t,d->problem(),{1},{K::Constant});});
    // Repeated equal source subtrees are occurrences, not shared parameters/holes.
    auto equalSource=parseExpression("1+1",2);ScalarExpressionTemplate one(equalSource,0,{3,2,2},{5,3,2});
    check(one.materialize(Expression::constant(2,2)).toString()=="(2.0 + 1.0)","one occurrence selected");
    template_fault::next=true;bool allocation=false;try{(void)d->materialize(hole);}catch(const std::bad_alloc&){allocation=true;}
    check(allocation && parent.expression().sameRepresentation(full) && parent.evaluation()==score,"composition allocation is operational and leaves original");
}
static void limitsAndVariation() {
    const auto source=scaffoldSource();
    const auto local=parseExpression("((x1+x1)+(x1+x1))+x1",2);check(local.nodeCount()==9&&local.depth()==4,"local boundary fixture");
    ScalarExpressionTemplate exact(source,1,{9,4,2},{13,6,2});check(exact.materialize(local).nodeCount()==13 && exact.materialize(local).depth()==6,"complete boundary");
    ScalarExpressionTemplate shortNodes(source,1,{9,4,2},{12,6,2});rejects([&]{(void)shortNodes.materialize(local);});
    ScalarExpressionTemplate shortDepth(source,1,{9,4,2},{13,5,2});rejects([&]{(void)shortDepth.materialize(local);});
    ScalarExpressionTemplate work(source,1,{9,4,8,100},{13,6,8,2});
    check(work.effectiveSubstitutionLimits().maxArithmeticOperations==0&&work.materialize(Expression::constant(1,2)).arithmeticCost()==2,"frozen arithmetic work deducted");
    rejects([&]{(void)work.materialize(parseExpression("1+1",2));});
    const std::size_t slot=4*sizeof(ExpressionNode)+4*sizeof(std::size_t);
    ScalarExpressionTemplate workspace(source,1,{9,4,2},{13,6,2},5*slot);check(workspace.effectiveSubstitutionLimits().maxNodes==1,"workspace effective bound");
    rejects([&]{(void)workspace.materialize(local);});rejects([&]{(void)ScalarExpressionTemplate(source,1,{9,4,2},{13,6,2},5*slot-1);});
    rejects([&]{(void)ScalarExpressionTemplate(source,99);});
    auto d=problem(data(0));auto c=context(d);ExpressionGrammar grammar;ScalarTemplateVariation variation(c,grammar);std::mt19937 rng(17);
    auto father=variation.initialize(rng);father.calculateFitness();const auto frozen=father.text();const auto score=father.evaluation();
    for(int i=0;i<200;++i) {
        auto x=variation.initialize(rng);auto mutated=variation.mutation(x,1,rng);auto a=x,b=mutated;variation.crossover(x,mutated,a,b,1,rng);
        for(const auto* child:{&x,&mutated,&a,&b}) {
            const auto hole=d->substitution(child->expression());
            check(hole.nodeCount()<=9&&hole.depth()<=4&&child->expression().nodeCount()<=13&&child->expression().depth()<=6,"variation limits");
            for(const auto& node:hole.nodes())if(node.kind()==K::Variable)check(node.variableIndex()==1,"original higher variable identity");
        }
    }
    check(variation.statistics().mutationChanges>0&&variation.statistics().crossoverChanges>0,"variation actually changes subtrees");
    check(father.text()==frozen&&father.evaluation()==score,"parent snapshot safe");
    // Root-only replacements and finite fallbacks under single-node hard limits.
    auto smallDomain=ScalarTemplateProblem::create(workspace,RegressionProblem::create(data(0),{13,6,2}),{1},{K::Add,K::Subtract,K::Multiply});
    ScalarTemplateVariation small(context(smallDomain));auto single=small.initialize(rng);for(int i=0;i<40;++i)check(smallDomain->substitution(small.mutation(single,1,rng).expression()).nodeCount()==1,"terminal/root generation bounded");
    auto noTerminal=ScalarTemplateProblem::create(exact,d->problem(),{},{});auto invalid=grammar;invalid.constants.clear();invalid.operators.clear();rejects([&]{ScalarTemplateVariation v(context(noTerminal),invalid);});
    auto forbidden=grammar;forbidden.operators.push_back(K::Log);rejects([&]{ScalarTemplateVariation v(c,forbidden);});
    // A source subtree containing the target is discarded, not used in initialization.
    auto targetSource=d->materialize(parseExpression("x1*x1+1",2));ScalarExpressionTemplate target(targetSource,5,{9,4,2},{13,6,2});
    auto sameScaffold=ScalarTemplateProblem::create(target,d->problem(),{1},{K::Add,K::Subtract,K::Multiply});
    ScalarTemplateVariation other(context(sameScaffold));std::mt19937 a(42),b(42);
    ScalarTemplateVariation original(c);for(int i=0;i<30;++i)check(original.initialize(a).text()==other.initialize(b).text(),"removed source not injected");check(a==b,"same generator RNG independent of removed subtree");
}
static void fitting() {
    const ExpressionLimits local{9,4,8},full{13,6,8};
    auto p=RegressionProblem::create({{{1,0},-1},{{2,0},-6}},full);auto d=ScalarTemplateProblem::create(ScalarExpressionTemplate(scaffoldSource(),1,local,full),p,{1},{K::Add,K::Subtract,K::Multiply});
    TrainingObjective objective(p,512);ConstantFitConfig cfg;cfg.maxLossAttempts=512;cfg.minimumStep=1e-10;
    const auto fitted=fitScalarTemplateConstants(*d,Expression::constant(1,2),objective,cfg);
    const auto hole=d->substitution(fitted.expression);const auto coefficient=hole.nodes()[0].constantValue();
    check(std::abs(coefficient-2.6)<1e-7 && std::abs(fitted.loss.score()-1.6)<2e-14,"full-objective weighted optimum, not divided-target mean2");
    check(fitted.loss==p->measure(fitted.expression).loss&&objective.work().lossAttempts<=512,"accepted model/score exact, bounded work");
    std::cout<<std::setprecision(17)<<"weighted_constant="<<coefficient<<" full_model_mse="<<fitted.loss.score()<<" loss_calls="<<objective.work().lossAttempts<<'\n';
    auto affine=problem(data(0,true),true);TrainingObjective numerical(affine->problem(),512);
    const auto initial=parseExpression("1*x1+1",2);const auto fit=fitScalarTemplateConstants(*affine,initial,numerical,cfg);const auto coefficients=affine->substitution(fit.expression).nodes();
    check(coefficients[0].constantValue()==.5&&coefficients[3].constantValue()==1.5&&fit.loss.score()==0,"independent affine coefficient recovery");
    check(fit.expression.nodes()[0].constantValue()==-1 && initial.toString()=="((1.0 * x1) + 1.0)","frozen scaffold and input unchanged");
    for(int stage=0;stage<3;++stage)check(*RegressionProblem::create(data(stage,true),full)->measure(fit.expression).maxAbsoluteError<=1e-6,"affine assessment");
    // A frozen value outside fitting bounds is allowed and never selected.
    auto nodes=scaffoldSource().nodes();nodes[0]=ExpressionNode::constant(-9);Expression bigger(nodes,2,{13,6,10});
    auto rows=data(0,true);for(auto& row:rows)row.target*=9;
    auto frozenData=RegressionProblem::create(rows,{13,6,10});auto outside=ScalarTemplateProblem::create(ScalarExpressionTemplate(bigger,1,local,{13,6,10}),frozenData,{1},{K::Add,K::Subtract,K::Multiply});TrainingObjective fo(frozenData,512);
    auto frozenFit=fitScalarTemplateConstants(*outside,initial,fo,cfg);check(frozenFit.expression.nodes()[0].constantValue()==-9&&frozenFit.loss.score()==0,"frozen coefficient outside fit bounds preserved");
    // An accepted a=1 precedes c=2 (worse) and c=0 (invalid).
    auto lp=RegressionProblem::create({{{1,0},-1.2},{{2,1},-2.4}},full);auto ld=ScalarTemplateProblem::create(ScalarExpressionTemplate(scaffoldSource(),1,local,full),lp,{1},{K::Add,K::Log});TrainingObjective lo(lp,32);auto one=cfg;one.maxSweeps=1;one.maxLossAttempts=32;
    auto retained=fitScalarTemplateConstants(*ld,parseExpression("0+log(1)",2),lo,one);check(ld->substitution(retained.expression).toString()=="(1.0 + log(1.0))"&&lo.work().invalid>0&&lo.work().accepted>0&&retained.loss==lp->measure(retained.expression).loss,"selected fit retains progress after invalid probe");
    TrainingObjective wrong(RegressionProblem::create({{{1,0},0}},full),32);rejects([&]{(void)fitScalarTemplateConstants(*d,Expression::constant(1,2),wrong);});
    TrainingObjective bad(p,32);const auto model=d->materialize(Expression::constant(1,2));rejects([&]{(void)fitSelectedConstants(model,bad,std::array<std::size_t,2>{1,1},cfg);});
    check(bad.work().lossAttempts==0,"bad parameter selection fails before objective");
}
struct ControlledState {
    enum Mode{None,Throw,Cancel};std::atomic<Mode> mode{None};std::atomic<std::uint64_t> starts{0};std::atomic<bool> progress{false};
    std::barrier<> gate{2};std::mutex mutex;std::condition_variable ready;ExecutionContext* execution=nullptr;bool waitForImprovement=true;
};
class ControlledObjective final:public TrainingObjective {
public:
    ControlledObjective(std::shared_ptr<const RegressionProblem> p,std::uint64_t maximum,std::shared_ptr<ControlledState> state):TrainingObjective(std::move(p),maximum),state_(std::move(state)){}
    bool parallelEvaluationSafe() const noexcept override{return true;}
protected:
    std::shared_ptr<TrainingObjective> cloneForParallel() const override{return std::make_shared<ControlledObjective>(problem(),maximum(),state_);}
    RegressionMetrics compute(const Expression& e) override {
        ++state_->starts;const auto mode=state_->mode.load();
        if(mode!=ControlledState::None) {
            if(work().accepted>0){state_->progress=true;state_->ready.notify_all();}
            if(work().lossAttempts==1) {
                state_->gate.arrive_and_wait();
                if(e.nodes()[1].constantValue()==4) {
                    if(state_->waitForImprovement){std::unique_lock lock(state_->mutex);if(!state_->ready.wait_for(lock,std::chrono::seconds(5),[&]{return state_->progress.load();}))throw std::runtime_error("missing private fitting progress");}
                    if(mode==ControlledState::Throw)throw std::runtime_error("template worker fault after sibling progress");
                    state_->execution->requestStop();
                }
            }
        }
        return TrainingObjective::compute(e);
    }
private:std::shared_ptr<ControlledState> state_;
};
using P=Population<ExpressionCandidate,RegressionOrdering>;
static auto snapshot(const P& p){std::vector<std::pair<std::string,Fitness>> s;const auto owned=p.creatures();for(const auto& c:*owned)s.push_back({c.text(),c.evaluation()});return s;}
static void overlapFailures() {
    for(auto mode:{ControlledState::Throw,ControlledState::Cancel})for(bool scheduled:{false,true}) {
        auto p=RegressionProblem::create({{{1,0},-1},{{2,0},-6}},{13,6,8});auto d=ScalarTemplateProblem::create(ScalarExpressionTemplate(scaffoldSource(),1,{9,4,8},{13,6,8}),p,{1},{K::Add,K::Subtract,K::Multiply});
        auto state=std::make_shared<ControlledState>();auto objective=std::make_shared<ControlledObjective>(p,4096,state);ConstantFitConfig fit;fit.maxLossAttempts=4;
        auto preparation=std::make_shared<ConstantRefinement>(objective,fit,scheduled?1:2,std::make_shared<ScalarTemplateParameters>(d));auto ctx=context(d,preparation);
        std::vector<ExpressionCandidate> initial{candidate(ctx,Expression::constant(0,2)),candidate(ctx,Expression::constant(4,2))};RandomContext random(0,{2});state->execution=&random.execution();
        P pop(initial,random,ObjectiveDirection::Minimize);const auto before=snapshot(pop);const auto best=pop.bestSeen()->text();const auto fitness=pop.bestSeen()->evaluation();const auto generation=pop.generation();
        const auto rng=random.engine();auto next=*pop.creatures();for(std::size_t i=0;i<next.size();++i)next[i].replaceExpression(d->materialize(Expression::constant(i?4:0,2)));
        state->waitForImprovement=scheduled;state->mode=mode;
        const auto error=failure([&]{pop.publishGeneration(next);});
        if(mode==ControlledState::Throw)check(cause(error)=="template worker fault after sibling progress","original operational cause preserved");else check(isEvaluationCancelled(error),"cooperative cancellation classified");
        check(snapshot(pop)==before&&pop.generation()==generation&&pop.bestSeen()->text()==best&&pop.bestSeen()->evaluation()==fitness,"failed prepared generation preserves committed values/cache/archive");
        check(objective->work().lossAttempts==state->starts&&random.engine()==rng&&random.execution().peakTasks()==2,"attempt accounting, no worker RNG and real overlap");
        if(scheduled)check(state->progress&&objective->work().accepted>0,"private improvement consumed before sibling failure");
        state->mode=ControlledState::None;check(pop.publishGeneration(next)==GenerationOutcome::Committed&&pop.generation()==1,"same prospective generation retried");
        if(!scheduled)check(preparation->statistics().skippedBySchedule>=2,"retry stays unscheduled");
    }
}
class CorruptPreparation final:public RegressionPreparation {
public:
    bool corrupt=false;
    PreparedExpression prepare(const Expression& e,const RegressionProblem& p) override {
        return {corrupt ? e.withConstant(0,1):e,p.measure(e).loss};
    }
};
static void repeatedRefinementAndPublication() {
    auto d=problem(data(0),true);auto objective=std::make_shared<TrainingObjective>(d->problem(),128);
    ConstantFitConfig fit;fit.maxLossAttempts=2;
    auto prep=std::make_shared<ConstantRefinement>(objective,fit,2,std::make_shared<ScalarTemplateParameters>(d));auto ctx=context(d,prep);
    RandomContext random(0,{2});P population({candidate(ctx,Expression::constant(1,2)),candidate(ctx,Expression::constant(1,2))},random,ObjectiveDirection::Minimize);
    const auto initial=population.bestSeen()->fitness();const auto initialHole=d->substitution(population.bestSeen()->expression()).toString();
    population.publishGeneration(*population.creatures());check(population.bestSeen()->fitness()==initial&&d->substitution(population.bestSeen()->expression()).toString()==initialHole,"unscheduled clones score without fitting");
    population.publishGeneration(*population.creatures());check(population.bestSeen()->fitness()<initial&&prep->statistics().skippedBySchedule==2&&objective->work().fittingInvocations==4,"scheduled inherited expression benefits from another bounded fit");
    auto corrupt=std::make_shared<CorruptPreparation>();auto cc=context(d,corrupt);RandomContext serial(1);
    P protectedPopulation({candidate(cc,Expression::constant(1,2))},serial,ObjectiveDirection::Minimize);const auto before=snapshot(protectedPopulation);corrupt->corrupt=true;
    auto error=failure([&]{protectedPopulation.publishGeneration(*protectedPopulation.creatures());});
    check(cause(error)=="frozen template prefix changed"&&snapshot(protectedPopulation)==before&&protectedPopulation.generation()==0,"preparation cannot publish changed scaffold or stale matching score");
}
static void validityAndBudgets() {
    const ExpressionLimits local{9,4,8},full{13,6,8};auto p=RegressionProblem::create({{{1,0},1},{{2,1},2}},full);
    auto zero=parseExpression("(-0.0)*x1",2);auto d=ScalarTemplateProblem::create(ScalarExpressionTemplate(zero,1,local,full),p,{1},{K::Log,K::Add});auto c=context(d);
    auto invalid=candidate(c,parseExpression("log(-1)",2));check(!invalid.expression().evaluate(std::array{0.,0.}).valid(),"zero scaffold does not mask invalid hole");
    RandomContext run(1,{2});P empty({invalid,invalid},run,ObjectiveDirection::Minimize);check(!empty.generation()&&!empty.bestSeen(),"all invalid initialization leaves no model");
    auto valid=candidate(c,Expression::constant(1,2));P viable({valid,valid},run,ObjectiveDirection::Minimize);auto before=snapshot(viable);check(viable.publishGeneration({invalid,invalid})==GenerationOutcome::NoViableCandidates&&snapshot(viable)==before&&viable.generation()==0,"all invalid successor preserves commit");
    auto minusZero=valid.expression();rejects([&]{valid.replaceExpression(minusZero.withConstant(0,0));});
    // Global budget denial after private work, on a scheduled refinement attempt.
    auto base=problem(data(0),true);auto objective=std::make_shared<TrainingObjective>(base->problem(),9);ConstantFitConfig fit;fit.maxLossAttempts=4;
    auto preparation=std::make_shared<ConstantRefinement>(objective,fit,1,std::make_shared<ScalarTemplateParameters>(base));auto ctx=context(base,preparation);
    RandomContext random(2,{2});P pop({candidate(ctx,Expression::constant(1,2)),candidate(ctx,Expression::constant(1,2))},random,ObjectiveDirection::Minimize);const auto saved=snapshot(pop);const auto best=pop.bestSeen()->text();
    auto error=failure([&]{pop.publishGeneration(*pop.creatures());});check(isEvaluationBudget(error)&&objective->work().lossAttempts==9&&snapshot(pop)==saved&&pop.generation()==0&&pop.bestSeen()->text()==best,"global fitting admission discards attempt");
    // Budget exhaustion while unscheduled ordinary scoring also discards the attempt.
    auto small=std::make_shared<TrainingObjective>(base->problem(),9);auto prep=std::make_shared<ConstantRefinement>(small,fit,2,std::make_shared<ScalarTemplateParameters>(base));auto sc=context(base,prep);RandomContext sr(3,{2});
    P sp({candidate(sc,Expression::constant(1,2)),candidate(sc,Expression::constant(1,2))},sr,ObjectiveDirection::Minimize);auto ss=snapshot(sp);auto se=failure([&]{sp.publishGeneration(*sp.creatures());});check(isEvaluationBudget(se)&&small->work().lossAttempts==9&&snapshot(sp)==ss&&sp.generation()==0&&prep->statistics().skippedBySchedule>0,"unscheduled budget boundary");
    auto cfg=config();cfg.population=8;cfg.elites=1;cfg.maxTrainingLossAttempts=7;cfg.maxEvaluations=7;RandomContext none(4,{2});const auto rng=none.engine();auto stopped=scalarTemplateRegression(base,cfg,none);check(!stopped.best&&stopped.stoppingReason==SearchStop::EvaluationBudget&&none.engine()==rng&&stopped.trainingWork.lossAttempts==0,"insufficient initialization allowance consumes no RNG/work");
    auto divider=parseExpression("x1/0",2);auto all=ScalarTemplateProblem::create(ScalarExpressionTemplate(divider,0,local,full),p,{1},{K::Add,K::Subtract,K::Multiply});cfg.maxTrainingLossAttempts=64;cfg.maxEvaluations=64;RandomContext allRun(7,{2});auto failed=scalarTemplateRegression(all,cfg,allRun);check(!failed.best&&failed.stoppingReason==SearchStop::NoViableCandidates&&failed.trainingWork.lossAttempts==8,"all invalid search has no automatic retries");
}
static void equivalence() {
    auto rows=data(0);rows.push_back(rows.front());rows.back().target+=.125;auto d=problem(rows,true);auto cfg=config();cfg.population=8;cfg.elites=1;cfg.maxEvaluations=32;cfg.maxGenerationAttempts=3;cfg.maxTrainingLossAttempts=4096;cfg.fitting=ConstantFitConfig{};cfg.fitting->maxLossAttempts=4;cfg.fitEveryGenerations=2;
    RandomContext serial(42,{1});const auto reference=scalarTemplateRegression(d,cfg,serial);
    check(reference.publishedGenerations==3,"nonbinding fixture exercises generations");
    for(unsigned threads:{1u,2u,3u,4u}) {
        RandomContext run(42,{threads});const auto actual=scalarTemplateRegression(d,cfg,run);
        check(actual.best->text()==reference.best->text()&&actual.best->evaluation()==reference.best->evaluation()&&actual.bestGeneration==reference.bestGeneration,"serial/parallel best and tie order");
        check(actual.preparedInitialExpressions==reference.preparedInitialExpressions&&actual.preparedInitialFitness==reference.preparedInitialFitness&&actual.rawInitialExpressions==reference.rawInitialExpressions,"initialization equivalence");
        check(actual.trainingWork==reference.trainingWork&&actual.initialWork==reference.initialWork&&actual.variation==reference.variation&&actual.publishedGenerations==reference.publishedGenerations&&actual.stoppingReason==reference.stoppingReason&&run.engine()==serial.engine(),"work/history/stop/RNG equivalence");
        check(actual.improvements.size()==reference.improvements.size(),"history size");for(std::size_t i=0;i<actual.improvements.size();++i){const auto a=actual.improvements[i],b=reference.improvements[i];check(a.generation==b.generation&&a.mse==b.mse&&a.lossAttempts==b.lossAttempts,"committed history exact");}
    }
}
int main(int argc,char**) {
    try{if(argc>1)overlapFailures();else{structure();limitsAndVariation();fitting();repeatedRefinementAndPublication();validityAndBudgets();equivalence();overlapFailures();}std::cout<<"scalar template contracts passed\n";}
    catch(const std::exception& e){std::cerr<<cause(e)<<'\n';return 1;}
}
