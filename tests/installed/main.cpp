#include "common.hpp"
#include <array>
extern "C" int mixed_c(void);
class Scalar final : public Creature_i<double> {
public:
    explicit Scalar(std::mt19937& rng):Creature_i<double>({std::uniform_real_distribution<double>(-1,1)(rng)}){}
    bool parallelEvaluationSafe() const noexcept override {return true;}
    std::string stats() override{return std::to_string(gene(0));}
protected:
    Fitness evaluate() const override{return Fitness::valid(gene(0)*gene(0));}
    double mutatedGene(unsigned int,std::mt19937& rng) const override{return std::uniform_real_distribution<double>(-1,1)(rng);}
};
int main() {
    {auto e=parseDiscreteExpression("sum(k0,1,x0,k0)",1);auto value=e.evaluate(std::array<std::int64_t,1>{5});if(!value.valid()||value.value->integer()!=15)return 1;}
    { auto e=parseDiscreteExpression("if(x0 == 0, 0, 10 % x0)",1);
      if(!e.evaluate(std::array<std::int64_t,1>{0}).valid() || parseDiscreteExpression(e.toString(),1).toString()!=e.toString())return 1;
      auto p=DiscreteRegressionProblem::create(1,{{{0},0},{{2},0}});
      if(p->measure(e).loss!=Fitness::valid(0))return 1;
    }
    IntegralExpression integral(1,parseExpression("0",1),parseExpression("x0",1),parseExpression("2*x1+1",2),1);
    auto estimate=integral.evaluate(std::array{2.});
    if(!estimate.accepted() || std::abs(*estimate.estimate-6)>1e-10)return 1;
    const auto observed=DerivativeRegressionProblem::create(1,{{0,{{{1},2}},1,1}});
    if(observed->measure(parseExpression("x0*x0",1)).loss!=Fitness::valid(0))return 1;
    if(evaluatePartial(parseExpression("sin(x0)",1),std::array{0.},0).derivative()!=1)return 1;
    if(parseExpression("powi(2,3)",0).evaluate({}).value()!=8)return 1;
    const ScalarExpressionTemplate rootTemplate(Expression::constant(0,1),0,{3,2,8},{3,2,8});
    if(rootTemplate.materialize(parseExpression("x0+1",1)).evaluate(std::array{2.}).value()!=3)return 1;
    RandomContext rng(7,{3});
    Population<Scalar> p(8,rng,ObjectiveDirection::Minimize);
    GeneticAlgorithm<decltype(p),Scalar> ga(.9f,.3f,rng);
    for(int i=0;i<3;++i)if(ga.evolve(p)!=GenerationOutcome::Committed)return 1;
    const auto state=RunCheckpointAccess::population(p);RandomContext fresh(7,{3});RunCheckpointAccess::restoreRandom(fresh,RunCheckpointAccess::random(rng));
    auto restored=RunCheckpointAccess::restorePopulation<Scalar>(state,fresh);
    if(restored->generation()!=p.generation()||fresh.engine()!=rng.engine()||fresh.evaluationAttempts()!=rng.evaluationAttempts())return 1;
    const auto typedScalar=TypedExpression::constant(TypedValue::scalar(-0.0)).evaluate({});
    if(!typedScalar.valid() || !std::signbit(typedScalar.value().scalarValue()))return 1;
    const auto x=Expression::variable(0);
    const auto log=Expression::unary(ExpressionKind::Log,x);
    const auto div=Expression::binary(ExpressionKind::Divide,x,Expression::constant(-0.0,1));
    if(log.evaluate(std::array{-1.}).valid() || div.evaluate(std::array{1.}).valid())return 1;
    return p.bestSeen() && rng.evaluationAttempts()==32 && mixed_c()==3 && symbolic_check() && typed_check() ? 0:1;
}
