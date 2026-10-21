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
    if(evaluatePartial(parseExpression("sin(x0)",1),std::array{0.},0).derivative()!=1)return 1;
    if(parseExpression("powi(2,3)",0).evaluate({}).value()!=8)return 1;
    const ScalarExpressionTemplate rootTemplate(Expression::constant(0,1),0,{3,2,8},{3,2,8});
    if(rootTemplate.materialize(parseExpression("x0+1",1)).evaluate(std::array{2.}).value()!=3)return 1;
    RandomContext rng(7,{3});
    Population<Scalar> p(8,rng,ObjectiveDirection::Minimize);
    GeneticAlgorithm<decltype(p),Scalar> ga(.9f,.3f,rng);
    for(int i=0;i<3;++i)if(ga.evolve(p)!=GenerationOutcome::Committed)return 1;
    const auto typedScalar=TypedExpression::constant(TypedValue::scalar(-0.0)).evaluate({});
    if(!typedScalar.valid() || !std::signbit(typedScalar.value().scalarValue()))return 1;
    const auto x=Expression::variable(0);
    const auto log=Expression::unary(ExpressionKind::Log,x);
    const auto div=Expression::binary(ExpressionKind::Divide,x,Expression::constant(-0.0,1));
    if(log.evaluate(std::array{-1.}).valid() || div.evaluate(std::array{1.}).valid())return 1;
    return p.bestSeen() && rng.evaluationAttempts()==32 && mixed_c()==3 && symbolic_check() && typed_check() ? 0:1;
}
