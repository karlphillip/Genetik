#include <TypedRegression.hpp>
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
int main(){try{lossContracts();std::cout<<"typed regression contracts passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
