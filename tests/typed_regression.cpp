#include <TypedSymbolicRegression.hpp>
#include "../examples/TypedFittingFixture.hpp"
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <sstream>
#include <Population.hpp>
#include <array>
#include <bit>
#include <iostream>
using K=TypedExpressionKind;
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class E=std::exception,class F> void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("expected rejection");}
static void lossContracts(){
    const std::array scalarSchema{TypedShape::scalar()};
    std::vector<TypedRegressionRow> rows{{{TypedValue::scalar(-1)},TypedValue::scalar(1)},{{TypedValue::scalar(1)},TypedValue::scalar(3)}};
    auto problem=TypedRegressionProblem::create(scalarSchema,TypedShape::scalar(),rows);
    auto x=TypedExpression::variable(0,scalarSchema);
    check(problem->measure(x).loss.score()==4,"scalar MSE");
    rows[0].target=TypedValue::scalar(200);
    check(problem->measure(x).loss.score()==4,"dataset owns inputs");
    auto other=TypedRegressionProblem::create(scalarSchema,TypedShape::scalar(),rows);
    TypedExpressionCandidate a(x,problem),b(x,other);a.calculateFitness();b.calculateFitness();
    check(a.fitness()!=b.fitness(),"same schema does not certify same objective");
    auto snapshot=a;auto same=x;
    a.replaceExpression(same);check(a.fitnessState()==FitnessState::Unevaluated && snapshot.fitness()==4,"controlled replacement invalidates");
    a.calculateFitness();const auto validBeforeFailure=a.evaluation();
    rejects([&]{a.replaceExpression(TypedExpression::constant(TypedValue::vector(std::array{1.}),scalarSchema));});
    check(a.evaluation()==validBeforeFailure,"failed replacement preserves valid cached fitness");
    check(a.expression().toString()==x.toString(),"failed replacement preserves genome");
    const std::array vectorSchema{TypedShape::vector(2)};
    auto vp=TypedRegressionProblem::create(vectorSchema,TypedShape::vector(2),{{{TypedValue::vector(std::array{1.,3.})},TypedValue::vector(std::array{0.,1.})}});
    check(vp->measure(TypedExpression::variable(0,vectorSchema)).loss.score()==2.5,"normalize over components");
    const std::array matrixSchema{TypedShape::matrix(2,3)};
    auto mp=TypedRegressionProblem::create(matrixSchema,TypedShape::matrix(2,3),{{{TypedValue::matrix(2,3,std::array{1.,2.,3.,4.,5.,6.})},TypedValue::matrix(2,3,std::array{0.,0.,0.,0.,0.,0.})}});
    check(mp->measure(TypedExpression::variable(0,matrixSchema)).loss.score()==91./6.,"rectangular matrix MSE");
    rejects([&]{(void)TypedRegressionProblem::create(vectorSchema,TypedShape::matrix(1,2),{{{TypedValue::vector(std::array{1.,2.})},TypedValue::vector(std::array{1.,2.})}});});
    rejects([&]{(void)TypedRegressionProblem::create(scalarSchema,TypedShape::scalar(),{});});
    auto logarithm=TypedExpression::unary(K::Log,x);
    auto logProblem=TypedRegressionProblem::create(scalarSchema,TypedShape::scalar(),{{{TypedValue::scalar(1)},TypedValue::scalar(0)},{{TypedValue::scalar(-1)},TypedValue::scalar(0)},{{TypedValue::scalar(-2)},TypedValue::scalar(0)}});
    auto metrics=logProblem->measure(logarithm);
    check(metrics.loss.state()==FitnessState::Invalid && metrics.rowsAttempted==2 && metrics.invalidRows==1,"no favorable valid prefix");
    metrics=logProblem->measure(logarithm,nullptr,nullptr,true);
    check(metrics.rowsAttempted==3 && metrics.invalidRows==2 && !metrics.maxAbsoluteError,"report counts every invalid row");
    std::uint64_t attempted=0,operations=0;
    rejects<TypedResourceExhausted>([&]{(void)problem->measure(x,&attempted,&operations,false,3);});
    check(attempted==2 && operations==3,"resource stop after first row retains work");
    std::uint64_t primitiveCount=0;
    check(logarithm.evaluateWithWork(std::array{TypedValue::scalar(1)},primitiveCount).valid() && primitiveCount==1,"per-primitive accounted evaluator");
    check(!logarithm.evaluateWithWork(std::array{TypedValue::scalar(-1)},primitiveCount).valid() && primitiveCount==1,"domain rejection charges no primitive");
    primitiveCount=std::numeric_limits<std::uint64_t>::max();
    rejects<std::overflow_error>([&]{(void)logarithm.evaluateWithWork(std::array{TypedValue::scalar(1)},primitiveCount);});
    check(logarithm.evaluate(std::array{TypedValue::scalar(1)}).valid(),"independent evaluation after failed diagnostic");
    auto huge=TypedExpression::constant(TypedValue::scalar(std::numeric_limits<double>::max()),scalarSchema);
    check(problem->measure(huge).loss.state()==FitnessState::Invalid,"checked square overflow");
    auto objective=std::make_shared<TypedTrainingObjective>(problem,10);
    auto preparation=std::make_shared<TypedScoring>(objective);
    std::vector<TypedExpressionCandidate> initial{TypedExpressionCandidate(x,problem,preparation),TypedExpressionCandidate(x,problem,preparation)};
    RandomContext random(42,{2});Population<TypedExpressionCandidate,TypedRegressionOrdering> population(initial,random,ObjectiveDirection::Minimize);
    check(objective->work().lossAttempts==2 && objective->work().rowAttempts==4,"worker actual loss and row counts");
    const auto committed=population.bestSeen()->text();const auto work=objective->work();
    (void)other->measure(population.bestSeen()->expression());
    check(population.bestSeen()->fitness()==4 && objective->work()==work,"reporting cannot overwrite cached training fitness");
    // Wrong objective identity is rejected by generic staging, before publication.
    rejects([&]{population.publishGeneration({b,b});});
    check(population.bestSeen()->text()==committed && population.generation()==0,"failed preparation preserves committed state");
}

static void variationContracts(){
    for(unsigned split=0;split<3;++split){
        const auto rows=typed_example::data(split);
        for(std::size_t input=0;input<3;++input)for(std::size_t c=0;c<rows[0].inputs[input].shape().coefficients();++c){
            bool varies=false;for(const auto& row:rows)if(row.inputs[input].coefficients()[c]!=rows[0].inputs[input].coefficients()[c])varies=true;
            check(varies,"every campaign input coefficient varies");
        }
        for(std::size_t c=0;c<2;++c){bool varies=false;for(const auto& row:rows)if(row.target.at(c)!=rows[0].target.at(c))varies=true;check(varies,"campaign output components vary");
            bool productVaries=false;
            for(const auto& row:rows)if(row.target.at(c)-row.inputs[2].at(c)!=rows[0].target.at(c)-rows[0].inputs[2].at(c))productVaries=true;
            check(productVaries,"omitted Ax contribution is not constant");}
    }

    const std::array schema{TypedShape::matrix(2,3),TypedShape::vector(3),TypedShape::vector(2)};
    TypedExpressionLimits limits;limits.maxNodes=15;limits.maxDepth=5;limits.maxConstantNodes=4;limits.maxConstantCoefficients=16;
    auto problem=TypedRegressionProblem::create(schema,TypedShape::vector(2),{{{TypedValue::matrix(2,3,std::array{1.,2.,3.,-1.,0.,4.}),TypedValue::vector(std::array{2.,-1.,3.}),TypedValue::vector(std::array{5.,-2.})},TypedValue::vector(std::array{14.,8.})}},limits);
    TypedGrammar grammar;TypedVariation variation(problem,grammar);std::mt19937 random(42);
    const auto A=TypedExpression::variable(0,schema,limits),x=TypedExpression::variable(1,schema,limits),b=TypedExpression::variable(2,schema,limits);
    const auto product=TypedExpression::binary(K::MatVec,A,x,limits);
    const auto target=TypedExpression::binary(K::Add,product,b,limits);
    const auto replaced=replaceTypedSubtree(target,2,b,limits);
    check(replaced && replaced->nodeCount()==3 && replaced->evaluate(std::array{TypedValue::matrix(2,3,std::array{1.,2.,3.,-1.,0.,4.}),TypedValue::vector(std::array{2.,-1.,3.}),TypedValue::vector(std::array{5.,-2.})}).value().at(0)==10,"internal unequal-size replacement");
    check(!replaceTypedSubtree(target,2,x,limits),"shape mismatch is rejected");
    auto tight=limits;tight.maxNodes=4;check(!replaceTypedSubtree(b,0,target,tight),"root node limit");
    tight=limits;tight.maxDepth=3;
    check(!replaceTypedSubtree(target,0,TypedExpression::binary(K::Add,A,A,limits),tight),"destination depth checked in whole tree");
    check(replaceTypedSubtree(target,4,b,limits)->nodeCount()==1,"root replacement");
    TypedExpressionCandidate parent(target,problem);parent.calculateFitness();
    bool higher=false;
    for(int i=0;i<100;++i){auto initial=variation.initialize(random);(void)problem->checkedExpression(initial.expression());
        for(const auto& node:initial.expression().nodes())if(node.kind()==K::Variable && node.variableIndex()==2)higher=true;
        auto child=variation.mutation(parent,1,random);auto a=child,bb=parent;variation.crossover(parent,child,a,bb,1,random);
        (void)problem->checkedExpression(a.expression());(void)problem->checkedExpression(bb.expression());
        check(parent.fitness()==0 && parent.text()==target.toString(),"variation preserves parent and cache");
        if(child.text()!=parent.text())check(child.fitnessState()==FitnessState::Unevaluated,"changed child unevaluated");
    }
    check(higher && variation.statistics().mutationChanges>0 && variation.statistics().crossoverChanges>0,"operators actually change and use high variable");
    // No output terminal: the only production is rectangular matrix-vector multiplication.
    const std::array noTerminalSchema{TypedShape::matrix(2,3),TypedShape::vector(3)};
    auto noTerminal=TypedRegressionProblem::create(noTerminalSchema,TypedShape::vector(2),{{{TypedValue::matrix(2,3,std::array{1.,0.,0.,0.,1.,0.}),TypedValue::vector(std::array{1.,2.,3.})},TypedValue::vector(std::array{1.,2.})}},limits);
    grammar.operators={K::MatVec};grammar.constantShapes.clear();TypedVariation constructive(noTerminal,grammar);
    check(constructive.initialize(random).expression().nodeCount()==3,"construct operation-only output");
    grammar.operators={K::Add};rejects([&]{TypedVariation impossible(noTerminal,grammar);});
    grammar.operators={K::MatVec};grammar.initialMaxDepth=1;rejects([&]{TypedVariation impossible(noTerminal,grammar);});
    auto coefficientsLimited=limits;coefficientsLimited.maxConstantCoefficients=1;
    auto vectorConstant=TypedExpression::constant(TypedValue::vector(std::array{1.,2.}),schema,limits);
    check(!replaceTypedSubtree(b,0,vectorConstant,coefficientsLimited),"complete coefficient-count limit");
    // Unary, dot and rectangular matrix products with no usable output terminal.
    for(unsigned fixture=0;fixture<3;++fixture) {
        const std::vector<TypedShape> inputs=fixture==0?std::vector<TypedShape>{TypedShape::vector(3),TypedShape::vector(3)}:
            fixture==1?std::vector<TypedShape>{TypedShape::matrix(2,3),TypedShape::matrix(3,2)}:std::vector<TypedShape>{TypedShape::matrix(2,3)};
        const auto output=fixture==0?TypedShape::scalar():fixture==1?TypedShape::matrix(2,2):TypedShape::matrix(3,2);
        std::vector<TypedValue> bindings;
        for(auto shape:inputs){std::vector<double> coefficients(shape.coefficients(),1);
            bindings.push_back(shape.kind()==TypedValueKind::Vector?TypedValue::vector(coefficients):TypedValue::matrix(shape.rows(),shape.columns(),coefficients));}
        const auto targetValue=fixture==0?TypedValue::scalar(0):fixture==1?TypedValue::matrix(2,2,std::array{0.,0.,0.,0.}):TypedValue::matrix(3,2,std::array{0.,0.,0.,0.,0.,0.});
        auto fixtureProblem=TypedRegressionProblem::create(inputs,output,{{bindings,targetValue}},limits);
        TypedGrammar g;g.constantShapes.clear();g.operators={fixture==0?K::Dot:fixture==1?K::MatMul:K::Transpose};
        TypedVariation operatorVariation(fixtureProblem,g);
        const auto generated=operatorVariation.initialize(random).expression();
        check(generated.nodes().back().kind()==g.operators[0] && generated.nodeCount()>=(fixture==2?2u:3u) && generated.shape()==output && generated.evaluate(bindings).valid(),"constructive aggregate operation support");
    }
    for(auto operation:{K::Divide,K::Log}) {
        const std::array scalarSchema{TypedShape::scalar()};
        auto scalarProblem=TypedRegressionProblem::create(scalarSchema,TypedShape::scalar(),{{{TypedValue::scalar(-1)},TypedValue::scalar(0)}},limits);
        TypedGrammar g;g.operators={operation};g.constantShapes.clear();g.operationProbability=1;
        TypedVariation scalarVariation(scalarProblem,g);bool observed=false;
        for(unsigned sample=0;sample<32;++sample){auto generated=scalarVariation.initialize(random);if(generated.expression().nodes().back().kind()==operation)observed=true;(void)scalarProblem->checkedExpression(generated.expression());}
        check(observed,"opt-in scalar division/log production supported without validity resampling");
    }
    // Schema validation must not require every unused input to fit operand scratch.
    {
        const std::array s{TypedShape::vector(3)};auto small=limits;small.maxTemporaryCoefficients=1;
        auto p=TypedRegressionProblem::create(s,TypedShape::scalar(),{{{TypedValue::vector(std::array{1.,2.,3.})},TypedValue::scalar(1)}},small);
        TypedVariation feasible(p);auto generated=feasible.initialize(random);
        check(generated.expression().shape()==TypedShape::scalar() && p->measure(generated.expression()).loss.state()==FitnessState::Valid,"unused large binding excluded as terminal without rejecting feasible output");
    }
    // Search uses the shared engine; deterministic complete-generation outcomes are independent of worker count.
    TypedSymbolicRegressionConfig config;config.population=9;config.elites=1;config.maxGenerationAttempts=2;config.maxEvaluations=27;config.maxTrainingLossAttempts=1000;
    std::optional<TypedSymbolicRegressionResult> reference;std::string referenceRng;
    for(unsigned threads:{1u,2u,3u,4u}){
        RandomContext run(4,{threads});auto result=typedSymbolicRegression(problem,config,run);std::ostringstream rng;rng<<run.engine();
        if(reference){check(result.best->text()==reference->best->text() && result.best->evaluation()==reference->best->evaluation() && result.preparedInitialExpressions==reference->preparedInitialExpressions && result.preparedInitialFitness==reference->preparedInitialFitness && result.rawInitialExpressions==reference->rawInitialExpressions && result.trainingWork==reference->trainingWork && result.arithmeticOperations==reference->arithmeticOperations && rng.str()==referenceRng,"parallel equivalent search and RNG");}
        else {reference=result;referenceRng=rng.str();}
    }
}
struct OverlapState {
    std::mutex mutex;std::condition_variable cv;unsigned arrived=0,active=0,peak=0;
    void enter(){std::unique_lock lock(mutex);++arrived;++active;peak=std::max(peak,active);cv.notify_all();if(!cv.wait_for(lock,std::chrono::seconds(5),[&]{return arrived>=2;}))throw std::runtime_error("typed preparation did not overlap");}
    void leave(){std::lock_guard lock(mutex);--active;}
};
class OverlapObjective final:public TypedTrainingObjective {
public:
    OverlapObjective(std::shared_ptr<const TypedRegressionProblem> p,std::shared_ptr<OverlapState> s):TypedTrainingObjective(std::move(p),10000),state(std::move(s)){}
    bool parallelEvaluationSafe() const noexcept override{return true;}
protected:
    std::shared_ptr<TypedTrainingObjective> cloneForParallel() const override{return std::make_shared<OverlapObjective>(problem(),state);}
    TypedRegressionMetrics compute(const TypedExpression& e) override {
        if(!entered){entered=true;state->enter();try{auto result=TypedTrainingObjective::compute(e);state->leave();return result;}catch(...){state->leave();throw;}}
        return TypedTrainingObjective::compute(e);
    }
private:std::shared_ptr<OverlapState> state;bool entered=false;
};
static void overlapContracts(){
    const std::array schema{TypedShape::vector(2)};
    auto p=TypedRegressionProblem::create(schema,TypedShape::vector(2),{{{TypedValue::vector(std::array{1.,2.})},TypedValue::vector(std::array{2.,4.})}});
    auto state=std::make_shared<OverlapState>();auto objective=std::make_shared<OverlapObjective>(p,state);auto fitting=ConstantFitConfig{};fitting.maxLossAttempts=4;
    auto prep=std::make_shared<TypedConstantRefinement>(objective,fitting);
    auto e=TypedExpression::binary(K::Multiply,TypedExpression::constant(TypedValue::scalar(1),schema),TypedExpression::variable(0,schema));RandomContext random(1,{2});
    Population<TypedExpressionCandidate,TypedRegressionOrdering> population({TypedExpressionCandidate(e,p,prep),TypedExpressionCandidate(e,p,prep)},random,ObjectiveDirection::Minimize);
    check(state->peak==2 && objective->work().fittingInvocations==2 && objective->work().accepted>=2,"typed worker preparation forced overlap");
}

static void fittingContracts(){
    const auto initial=typed_example::unfitted();
    auto problem=TypedRegressionProblem::create(typed_example::fittingSchema,TypedShape::vector(2),typed_example::fittingData(0),typed_example::limits());
    const auto old=initial.toString();TypedTrainingObjective objective(problem,5000);
    auto result=fitTypedConstants(initial,objective,typed_example::fittingConfig());
    check(std::abs(result.startingLoss.score()-5.286458333333333)<1e-14,"independent initial coefficient-fixture MSE");
    check(result.improved && result.loss.score()==problem->measure(result.expression).loss.score() && initial.toString()==old,"accepted value and verified fitness pair; unchanged source");
    check(result.expression.nodeCount()==initial.nodeCount() && result.expression.depth()==initial.depth() && result.expression.constantCount()==2 && result.expression.constantCoefficients()==8,"fixed topology and eight independent coefficients");
    const auto nodes=result.expression.nodes();const std::array expected{1.5,-.75,.25,-.5,2.,1.25};
    for(std::size_t i=0;i<6;++i)check(std::abs(nodes[0].constantValue().coefficients()[i]-expected[i])<1e-7,"matrix coefficients recovered");
    check(std::abs(nodes[3].constantValue().at(0)-.625)<1e-7 && std::abs(nodes[3].constantValue().at(1)+1.375)<1e-7,"bias coefficients recovered independently");
    for(unsigned split=0;split<3;++split){auto p=TypedRegressionProblem::create(typed_example::fittingSchema,TypedShape::vector(2),typed_example::fittingData(split),typed_example::limits());check(*p->measure(result.expression).maxAbsoluteError<=1e-6,"fixed-structure sample recovery");}
    TypedTrainingObjective repeat(problem,5000);auto again=fitTypedConstants(initial,repeat,typed_example::fittingConfig());
    check(again.expression.toString()==result.expression.toString() && again.loss==result.loss && repeat.work()==objective.work(),"deterministic numerical fitting");
    const std::array schema{TypedShape::scalar()};
    auto p=TypedRegressionProblem::create(schema,TypedShape::scalar(),{{{TypedValue::scalar(1)},TypedValue::scalar(1.2)}});
    const auto constant=[&](double x){return TypedExpression::constant(TypedValue::scalar(x),schema);};
    auto fit=ConstantFitConfig{};fit.maxSweeps=1;fit.lossThreshold.reset();
    const auto logarithm=TypedExpression::binary(K::Add,constant(0),TypedExpression::unary(K::Log,constant(1)));
    TypedTrainingObjective logObjective(p);const auto logResult=fitTypedConstants(logarithm,logObjective,fit);
    check(logResult.improved && logResult.expression.nodes()[0].constantValue().scalarValue()==1 && logResult.expression.nodes()[1].constantValue().scalarValue()==1 && logObjective.work().invalid>0 && logResult.loss==p->measure(logResult.expression).loss,"invalid probe preserves earlier accepted coefficients");
    auto x=TypedExpression::variable(0,schema);
    TypedTrainingObjective freeObjective(p);check(fitTypedConstants(x,freeObjective,fit).stop==ConstantFitStop::NoConstants,"no constant outcome");
    TypedTrainingObjective invalidObjective(p);check(fitTypedConstants(TypedExpression::unary(K::Log,constant(-1)),invalidObjective,fit).stop==ConstantFitStop::InvalidStart,"invalid start outcome");
    TypedTrainingObjective limitedObjective(p);fit.maxLossAttempts=2;auto limited=fitTypedConstants(constant(0),limitedObjective,fit);
    check(limited.improved && limited.stop==ConstantFitStop::EvaluationLimit && limited.expression.nodes()[0].constantValue().scalarValue()==1,"local limit returns best accepted value");
    fit=ConstantFitConfig{};fit.lower=fit.upper=0;fit.maxSweeps=2;TypedTrainingObjective boundObjective(p);auto bounded=fitTypedConstants(constant(0),boundObjective,fit);
    check(!bounded.improved && boundObjective.work().infeasible>0,"infeasible proposals bounded");
    fit=ConstantFitConfig{};fit.maxSweeps=1;
    auto zeroProduct=TypedExpression::binary(K::Multiply,constant(0),constant(0));TypedTrainingObjective coupled(p);
    check(!fitTypedConstants(zeroProduct,coupled,fit).improved,"coupled zero coordinate stall reported honestly");
    auto two=TypedRegressionProblem::create(schema,TypedShape::vector(2),{{{TypedValue::scalar(0)},TypedValue::vector(std::array{2.,3.})}});
    auto independent=TypedExpression::constant(TypedValue::vector(std::array{1.,1.}),schema);TypedTrainingObjective independentObjective(two);fit.lossThreshold=0;
    auto independentResult=fitTypedConstants(independent,independentObjective,fit);
    check(independentResult.expression.nodes()[0].constantValue().at(0)==2 && independentResult.expression.nodes()[0].constantValue().at(1)==2,"equal-valued coefficients are independent in a limited sweep");
    // Subsequent scheduled fitting can improve an already fitted bounded result.
    auto refinementObjective=std::make_shared<TypedTrainingObjective>(two);auto refinement=std::make_shared<TypedConstantRefinement>(refinementObjective,fit,4);
    auto first=refinement->prepare(independent,*two,{0,PreparationOrigin::Initialization});
    auto skipped=refinement->prepare(first.expression,*two,{3,PreparationOrigin::Elite});
    auto next=refinement->prepare(skipped.expression,*two,{4,PreparationOrigin::Elite});
    check(next.loss.score()<first.loss.score() && refinement->statistics().skippedBySchedule==1,"scheduled inherited fit refines again");
    // Nonbinding parallel fitting preserves all candidate decisions, accounting, cadence and RNG.
    TypedSymbolicRegressionConfig config;config.population=7;config.elites=1;config.maxGenerationAttempts=3;config.maxEvaluations=28;config.maxTrainingLossAttempts=10000;
    config.fitting=ConstantFitConfig{};config.fitting->maxLossAttempts=4;config.fitEveryGenerations=2;
    config.grammar.constantShapes={TypedShape::scalar(),TypedShape::vector(2),TypedShape::matrix(2,3)};
    config.grammar.shapes={TypedShape::vector(3),TypedShape::vector(2),TypedShape::matrix(2,3),TypedShape::scalar()};
    std::optional<TypedSymbolicRegressionResult> reference;std::string rngReference;
    for(unsigned n:{1u,2u,3u,4u}){RandomContext run(21,{n});auto r=typedSymbolicRegression(problem,config,run);std::ostringstream rng;rng<<run.engine();
        if(reference)check(r.best->text()==reference->best->text() && r.best->evaluation()==reference->best->evaluation() && r.trainingWork==reference->trainingWork && r.preparationWork==reference->preparationWork && r.arithmeticOperations==reference->arithmeticOperations && r.preparedInitialExpressions==reference->preparedInitialExpressions && rng.str()==rngReference,"parallel fit/cadence equivalence");
        else {reference=r;rngReference=rng.str();}}
}
struct FaultState {std::mutex mutex;std::condition_variable cv;bool armed=false,progress=false,resource=false,cancel=false;ExecutionContext* execution=nullptr;};
class FaultObjective final:public TypedTrainingObjective {
public:
    FaultObjective(std::shared_ptr<const TypedRegressionProblem> p,std::uint64_t maximum,std::shared_ptr<FaultState> s):TypedTrainingObjective(std::move(p),maximum),state(std::move(s)){}
    bool parallelEvaluationSafe() const noexcept override{return true;}
protected:
    std::shared_ptr<TypedTrainingObjective> cloneForParallel() const override{return std::make_shared<FaultObjective>(problem(),maximum(),state);}
    TypedRegressionMetrics compute(const TypedExpression& e) override {
        if(state->resource)return computeWithLimits(e,3);
        if(state->cancel)state->execution->requestStop();
        if(state->armed){const double v=e.nodes()[0].constantValue().scalarValue();
            if(v==-1){std::lock_guard lock(state->mutex);state->progress=true;state->cv.notify_all();}
            if(v==.5){std::unique_lock lock(state->mutex);if(!state->cv.wait_for(lock,std::chrono::seconds(5),[&]{return state->progress;}))throw std::runtime_error("no private fit progress");throw std::runtime_error("typed injected operational failure");}
        }
        return TypedTrainingObjective::compute(e);
    }
private:std::shared_ptr<FaultState> state;
};
static std::string rootCause(std::exception_ptr e){try{std::rethrow_exception(e);}catch(const std::exception& x){const auto* n=dynamic_cast<const std::nested_exception*>(&x);return n && n->nested_ptr()?rootCause(n->nested_ptr()):x.what();}catch(...){return "nonstandard";}}
static void publicationContracts(){
    const std::array schema{TypedShape::scalar()};
    auto p=TypedRegressionProblem::create(schema,TypedShape::scalar(),{{{TypedValue::scalar(1)},TypedValue::scalar(1.2)},{{TypedValue::scalar(2)},TypedValue::scalar(1.2)}});
    const auto c=[&](double v){return TypedExpression::constant(TypedValue::scalar(v),schema);};
    for(unsigned threads:{1u,2u})for(unsigned interval:{1u,2u}){
        auto objective=std::make_shared<TypedTrainingObjective>(p,3);auto scoring=std::make_shared<TypedScoring>(objective);
        RandomContext run(3,{threads});using P=Population<TypedExpressionCandidate,TypedRegressionOrdering>;
        P population({TypedExpressionCandidate(c(0),p,scoring),TypedExpressionCandidate(c(.5),p,scoring)},run,ObjectiveDirection::Minimize);
        const auto before=population.bestSeen()->text();auto fit=ConstantFitConfig{};fit.maxLossAttempts=4;
        auto prep=std::make_shared<TypedConstantRefinement>(objective,fit,interval);
        try{population.publishGeneration({TypedExpressionCandidate(c(0),p,prep),TypedExpressionCandidate(c(.5),p,prep)});throw std::runtime_error("expected global stop");}
        catch(...){check(isEvaluationBudget(std::current_exception()),"global budget remains structured through nested preparation");}
        check(population.generation()==0 && population.bestSeen()->text()==before && objective->work().lossAttempts==3,"binding budget preserves committed state and exact admission");
        if(interval==2)check(objective->work().fittingInvocations==0,"unscheduled generation does ordinary scoring only");
    }
    auto state=std::make_shared<FaultState>();auto objective=std::make_shared<FaultObjective>(p,1000,state);
    auto scoring=std::make_shared<TypedScoring>(objective);RandomContext run(2,{2});
    Population<TypedExpressionCandidate,TypedRegressionOrdering> population({TypedExpressionCandidate(c(0),p,scoring),TypedExpressionCandidate(c(.5),p,scoring)},run,ObjectiveDirection::Minimize);
    const auto before=population.bestSeen()->text();const auto beforeFitness=population.bestSeen()->evaluation();
    auto fit=ConstantFitConfig{};fit.maxSweeps=1;auto prep=std::make_shared<TypedConstantRefinement>(objective,fit);
    state->armed=true;
    try{population.publishGeneration({TypedExpressionCandidate(c(0),p,prep),TypedExpressionCandidate(c(.5),p,prep)});throw std::logic_error("expected fit failure");}
    catch(...){check(rootCause(std::current_exception())=="typed injected operational failure","preserved original operational cause");}
    check(state->progress && objective->work().accepted>0 && population.generation()==0 && population.bestSeen()->text()==before && population.bestSeen()->evaluation()==beforeFitness,"private accepted improvement cannot escape failed publication");
    state->armed=false;state->resource=true;
    try{population.publishGeneration({TypedExpressionCandidate(c(0),p,scoring),TypedExpressionCandidate(c(.5),p,scoring)});throw std::logic_error("expected resource failure");}
    catch(...){check(rootCause(std::current_exception()).find("allowance exhausted")!=std::string::npos,"resource classification preserved");}
    check(population.generation()==0 && population.bestSeen()->text()==before,"resource stop preserves commit");
    state->resource=false;state->cancel=true;state->execution=&run.execution();
    try{population.publishGeneration({TypedExpressionCandidate(c(0),p,scoring),TypedExpressionCandidate(c(.5),p,scoring)});throw std::logic_error("expected cancellation");}
    catch(...){check(isEvaluationCancelled(std::current_exception()),"cooperative cancellation preserves classification");}
    check(population.generation()==0 && population.bestSeen()->text()==before,"cancellation preserves commit");
    // An all-invalid typed initial population has no committed generation or best.
    auto invalid=TypedExpression::unary(K::Log,c(-1));RandomContext invalidRun(0);
    Population<TypedExpressionCandidate,TypedRegressionOrdering> invalidPopulation({TypedExpressionCandidate(invalid,p)},invalidRun,ObjectiveDirection::Minimize);
    check(!invalidPopulation.generation() && !invalidPopulation.bestSeen(),"all-invalid initialization has no best");
    RandomContext validRun(0,{2});
    Population<TypedExpressionCandidate,TypedRegressionOrdering> validPopulation({TypedExpressionCandidate(c(0),p),TypedExpressionCandidate(c(.5),p)},validRun,ObjectiveDirection::Minimize);
    const auto committed=validPopulation.bestSeen()->text();
    check(validPopulation.publishGeneration({TypedExpressionCandidate(invalid,p),TypedExpressionCandidate(invalid,p)})==GenerationOutcome::NoViableCandidates && validPopulation.generation()==0 && validPopulation.bestSeen()->text()==committed,"all-invalid successor preserves committed typed population");
    const double maximum=std::numeric_limits<double>::max();
    auto impossibleLoss=TypedRegressionProblem::create(schema,TypedShape::scalar(),{{{TypedValue::scalar(maximum)},TypedValue::scalar(-maximum)}});
    TypedSymbolicRegressionConfig config;config.population=4;config.elites=1;config.grammar.operators.clear();config.grammar.constants={0};
    RandomContext invalidSearch(7,{2});auto outcome=typedSymbolicRegression(impossibleLoss,config,invalidSearch);
    check(!outcome.best && outcome.stoppingReason==SearchStop::NoViableCandidates && outcome.trainingWork.lossAttempts==4 && outcome.publishedGenerations==0,"typed search reports all-invalid initialization without retry");
}
int main(int argc,char**){try{if(argc==1){std::cerr<<"typed loss contracts\n";lossContracts();std::cerr<<"typed variation contracts\n";variationContracts();std::cerr<<"typed fitting contracts\n";fittingContracts();std::cerr<<"typed publication contracts\n";publicationContracts();}std::cerr<<"typed overlap contracts\n";overlapContracts();std::cout<<"typed regression contracts passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
