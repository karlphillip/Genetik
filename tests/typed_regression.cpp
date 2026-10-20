#include <TypedSymbolicRegression.hpp>
#include "../examples/TypedRegressionData.hpp"
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
    rejects([&]{a.replaceExpression(TypedExpression::constant(TypedValue::vector(std::array{1.}),scalarSchema));});
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
        for(std::size_t c=0;c<2;++c){bool varies=false;for(const auto& row:rows)if(row.target.at(c)!=rows[0].target.at(c))varies=true;check(varies,"campaign output components vary");}
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
    auto state=std::make_shared<OverlapState>();auto objective=std::make_shared<OverlapObjective>(p,state);auto prep=std::make_shared<TypedScoring>(objective);
    auto e=TypedExpression::variable(0,schema);RandomContext random(1,{2});
    Population<TypedExpressionCandidate,TypedRegressionOrdering> population({TypedExpressionCandidate(e,p,prep),TypedExpressionCandidate(e,p,prep)},random,ObjectiveDirection::Minimize);
    check(state->peak==2 && objective->work().lossAttempts==2,"typed worker preparation forced overlap");
}
int main(int argc,char**){try{if(argc==1){lossContracts();variationContracts();}overlapContracts();std::cout<<"typed regression contracts passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
