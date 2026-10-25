#include <SymbolicAlgebra.hpp>
#include <ExpressionParsing.hpp>
#include <ExpressionDerivative.hpp>
#include <barrier>
#include <iostream>
namespace {
void check(bool x, const char *reason) {
    if (!x)
        throw std::runtime_error(reason);
}
auto symbolic(std::string_view text) { return SymbolicExpression::fromExpression(parseExpression(text, 1)); }
void contracts() {
    auto original = symbolic("(x0+2)*(x0-2)");
    auto before = original.toString();
    auto expanded = original.expandPolynomial();
    check(expanded.status == SymbolicStatus::Success, "bounded polynomial expansion");
    auto factor = expanded.expression->factorPolynomial();
    check(factor.status == SymbolicStatus::Success, "supported integer-root factorization");
    check(compareSymbolicPolynomials(original, *factor.expression).conclusion ==
              SymbolicEquivalence::EqualUnderAssumptions,
          "independent polynomial coefficient comparison");
    check(original.toString() == before && !expanded.bitExactExecutableEquivalence,
          "symbolic transforms leave executable/owned input unchanged");
    auto collected = symbolic("x0+x0+3").expandPolynomial();
    auto explicitPolynomial = symbolic("2*x0+3");
    check(compareSymbolicPolynomials(*collected.expression, explicitPolynomial).conclusion ==
              SymbolicEquivalence::EqualUnderAssumptions,
          "exact rational constant and term collection");
    check(compareSymbolicPolynomials(symbolic("sin(x0)"), symbolic("sin(x0)")).conclusion ==
              SymbolicEquivalence::Unknown,
          "unsupported equality is unknown");
    check(symbolic("x0*x0+1").factorPolynomial().status == SymbolicStatus::Unknown,
          "unsupported rational factorization unknown");
    auto simplified = symbolic("x0/x0").simplify();
    check(simplified.status == SymbolicStatus::Success &&
              simplified.expression->nodes()[simplified.expression->root()].value == ExactRational(1),
          "conditional x/x simplification");
    bool nonzero = false;
    for (auto a : simplified.expression->assumptions())
        nonzero |= a.kind == SymbolicAssumptionKind::NonZero;
    check(nonzero && !simplified.bitExactExecutableEquivalence, "nonzero obligation retained");
    check(!symbolic("(x0+1)*(x0+1)").commonSubexpressions().empty(),
          "collision-free common structure identification");
    const std::array<std::pair<const char *, double>, 24> cases{
        {{"x0*x0", .75},       {"powi(x0,-2)", 2}, {"pow(x0,1.7)", 2}, {"log(x0)", 2},     {"log1p(x0)", .5},
         {"log2(x0)", 2},      {"log10(x0)", 2},   {"exp(x0)", .25},   {"expm1(x0)", .25}, {"sin(x0)", .25},
         {"cos(x0)", .25},     {"tan(x0)", .25},   {"asin(x0)", .25},  {"acos(x0)", .25},  {"atan(x0)", .25},
         {"atan2(x0,2)", .25}, {"sinh(x0)", .25},  {"cosh(x0)", .25},  {"tanh(x0)", .25},  {"sqrt(x0)", 2},
         {"cbrt(x0)", -2},     {"abs(x0)", -2},    {"x0/(x0+2)", .25}, {"powi(x0,3)", 2}}};
    for (auto [text, x] : cases) {
        auto expression = parseExpression(text, 1);
        auto derivative = SymbolicExpression::fromExpression(expression).differentiate(0);
        check(derivative.status == SymbolicStatus::Success &&
                  derivative.relation == SymbolicRelation::ConditionalDerivative,
              "documented symbolic derivative subset");
        auto executable = derivative.expression->toExecutable().expression;
        auto value = executable.evaluate(std::array{x});
        auto forward = evaluatePartial(expression, std::array{x}, 0);
        check(value.valid() && forward.derivativeAvailable() &&
                  std::abs(value.value() - forward.derivative()) < 1e-12,
              "symbolic and analytic-qualified numerical derivative comparison");
    }
    auto derivative = original.differentiate(0).expression->toExecutable().expression;
    for (double x : {-3., 0., 3.})
        check(derivative.evaluate(std::array{x}).value() == 2 * x,
              "independent analytic polynomial derivative");
    check(symbolic("max(0,x0)").differentiate(0).status == SymbolicStatus::Unsupported,
          "piecewise symbolic derivative explicit unsupported");
    auto linear = symbolic("powi(x0,1)").differentiate(0);
    check(linear.expression->toExecutable().expression.evaluate(std::array{0.}).value() == 1,
          "symbolic unit power derivative at zero");
    check(symbolic("1/0").expandPolynomial().status == SymbolicStatus::Undefined,
          "zero polynomial divisor distinguished");
    SymbolicLimits tiny;
    tiny.maxSteps = 1;
    auto exhausted = original.expandPolynomial(tiny);
    check(exhausted.status == SymbolicStatus::ResourceExhausted && !exhausted.expression,
          "no partial symbolic result after resource failure");
    std::atomic<bool> cancelled{true};
    check(original.differentiate(0, {}, &cancelled).status == SymbolicStatus::Cancelled,
          "symbolic cancellation");
    auto restored = SymbolicExpression::restore(decodeCheckpoint(encodeCheckpoint(original.checkpoint())));
    check(restored.sameRepresentation(original), "explicit symbolic rational DAG/assumptions codec");
    check(ExactRational(2, 3) * ExactRational(3, 4) == ExactRational(1, 2), "independent rational reference");
}
void overlap() {
    auto expression = symbolic("sin(x0)*exp(x0)");
    std::barrier gate(4);
    ExecutionContext execution({4});
    execution.run(4, [&](std::size_t) {
        gate.arrive_and_wait();
        for (int i = 0; i < 10; ++i) {
            auto result = expression.differentiate(0);
            check(result.status == SymbolicStatus::Success, "private symbolic transformation state");
        }
    });
    check(execution.peakTasks() == 4, "symbolic forced overlap");
}
} // namespace
int main(int argc, char **) {
    try {
        if (argc == 1)
            contracts();
        overlap();
        std::cout << "D5 symbolic contracts passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
