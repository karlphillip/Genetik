#include <ScalarMathematicsRun.hpp>
#include <ExpressionParsing.hpp>
#include <DiscreteExpressionParsing.hpp>
#include <barrier>
#include <iostream>
namespace {
void check(bool x, const char *reason) {
    if (!x)
        throw std::runtime_error(reason);
}
const auto L = PhysicalDimension::base(SIBaseDimension::Length),
           M = PhysicalDimension::base(SIBaseDimension::Mass),
           T = PhysicalDimension::base(SIBaseDimension::Time), V = L / T, E = M * V * V;
void contracts() {
    auto velocity = parseExpression("x0/x1", 2);
    check(DimensionalExpression(velocity, {L, T}).dimension() == V, "velocity units");
    bool rejected = false;
    try {
        (void)DimensionalExpression(parseExpression("x0+x1", 2), {L, T});
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    check(rejected, "incompatible addition rejected structurally");
    for (auto text : {"log(x0)", "sin(x0)", "exp(x0)"}) {
        rejected = false;
        try {
            (void)DimensionalExpression(parseExpression(text, 1), {L});
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        check(rejected, "dimensionless scientific arguments");
    }
    check(DimensionalExpression(parseExpression("sqrt(x0)", 1), {L.power(2)}).dimension() == L,
          "exact rational roots");
    check(DimensionalExpression(parseExpression("pow(x0,0.5)", 1), {L.power(2)}).dimension() == L,
          "dimensionful constant dyadic power");
    check(DimensionalExpression(parseExpression("atan2(x0,x1)", 2), {L, L}).dimension().dimensionless(),
          "atan2 compatible dimensional operands");
    auto conditional = parseDiscreteExpression("if((x0 == x1), x0, x1)", 2);
    check(inferDiscreteDimensions(conditional, std::array{L, L}).back() == L,
          "comparison and matching branch units");
    rejected = false;
    try {
        (void)inferDiscreteDimensions(conditional, std::array{L, T});
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    check(rejected, "discrete comparison units");
    MathematicalConstraints constraints({VariableDomain{0., 4., false, true}},
                                        {{ExpressionKind::Log, 0, true, false}}, {{ExpressionKind::Log, 1}},
                                        0, true);
    auto good = parseExpression("log(x0)", 1);
    auto report = constraints.assess(good, std::array{2.});
    check(report.status == ConstraintStatus::Satisfied && report.sampledOnly && !report.structuralProof,
          "sample-local positivity and monotonicity");
    check(constraints.assess(good, std::array{0.}).status == ConstraintStatus::Violated,
          "explicit variable domain");
    check(constraints.structural(parseExpression("log(log(x0))", 1)).status == ConstraintStatus::Violated,
          "bounded nesting structural proof");
    MathematicalConstraints decreasing({}, {}, {}, 0, true);
    check(decreasing.assess(parseExpression("(-1)*x0", 1), std::array{1.}).status ==
              ConstraintStatus::Violated,
          "sample derivative direction");
    check(decreasing.assess(parseExpression("abs(x0)", 1), std::array{0.}).status ==
              ConstraintStatus::DerivativeUnavailable,
          "no manufactured monotonicity proof at unavailable derivative");
    auto p = std::make_shared<const DimensionalRegressionProblem>(
        std::vector<RegressionRow>{{{1, 2}, 2}, {{2, 3}, 9}}, std::vector{M, V}, E,
        ExpressionLimits{31, 7, 10, 1000});
    ExpressionGrammar g;
    g.operators = {ExpressionKind::Add, ExpressionKind::Multiply, ExpressionKind::Powi};
    g.exponents = {2};
    g.initialMaxDepth = 5;
    DimensionalVariation variation(p, g);
    std::mt19937 rng(3);
    for (int i = 0; i < 50; ++i) {
        auto a = variation.initialize(rng), b = variation.initialize(rng);
        auto before = a.text();
        auto x = variation.mutation(a, 1, rng), y = b;
        variation.crossover(a, b, x, y, 1, rng);
        check(a.text() == before && p->accepts(x.expression()) && p->accepts(y.expression()),
              "owned typed mutation/crossover");
    }
    auto exponent = parseExpression("pow(x0,2)+3", 1);
    DimensionalConstantSelection selection({{}});
    check(selection.parameters(exponent).size() == 2, "dimensionless exponent remains a coefficient");
    DimensionalConstantSelection dimensionedSelection({L});
    auto power = parseExpression("2*pow(x0,2)", 1);
    check(dimensionedSelection.parameters(power).size() == 1, "dimensionful power exponent frozen");
}
void resume(bool dimensioned, unsigned workers) {
    auto p = dimensioned
                 ? ScalarMathematicsProblem::dimensional({{{1, 2}, 2.1}, {{2, 3}, 9.2}}, {M, V}, E,
                                                         {23, 6, 10, 1000})
                 : ScalarMathematicsProblem::scientific({{{.25}, .4}, {{1.5}, 2.3}}, {23, 6, 10, 1000});
    SymbolicRegressionConfig c;
    c.population = 12;
    c.maxGenerationAttempts = 10;
    c.maxEvaluations = 132;
    c.maxTrainingLossAttempts = 5000;
    c.grammar.initialMaxDepth = 4;
    c.grammar.operators =
        dimensioned ? std::vector{ExpressionKind::Add, ExpressionKind::Multiply}
                    : std::vector{ExpressionKind::Add, ExpressionKind::Multiply, ExpressionKind::Tanh};
    ConstantFitConfig fitting;
    fitting.lower = -8;
    fitting.upper = 8;
    fitting.maxLossAttempts = 8;
    c.fitting = fitting;
    RandomContext continuousRandom(19, {workers});
    ScalarMathematicsRun continuous(p, c, continuousRandom);
    while (!continuous.stoppingReason())
        continuous.step();
    RandomContext splitRandom(19, {workers});
    auto split = std::make_unique<ScalarMathematicsRun>(p, c, splitRandom);
    while (split->algorithm().attempts() < 4 && !split->stoppingReason())
        split->step();
    auto document = decodeCheckpoint(encodeCheckpoint(split->checkpoint()));
    split.reset();
    RandomContext resumedRandom(19, {workers});
    auto resumed = ScalarMathematicsRun::restore(document, p, c, resumedRandom);
    while (!resumed->stoppingReason())
        resumed->step();
    check(continuous.checkpoint().payload == resumed->checkpoint().payload,
          "complete population/fitness/RNG/best/counters resumed trajectory");
    check(continuous.work() == resumed->work(), "restored fitting counters");
    auto changed = c;
    changed.grammar.realExponents.push_back(1.7);
    RandomContext invalidRandom(19, {workers});
    bool rejected = false;
    try {
        (void)ScalarMathematicsRun::restore(document, p, changed, invalidRandom);
    } catch (const CheckpointError &) {
        rejected = true;
    }
    check(rejected, "new exponent grammar identity mismatch");
}
void overlap() {
    auto p = std::make_shared<const DimensionalRegressionProblem>(
        std::vector<RegressionRow>{{{1, 2}, 2.1}, {{2, 3}, 9.2}}, std::vector{M, V}, E,
        ExpressionLimits{23, 6, 10, 1000});
    auto e = parseExpression("0.5*x0*x1*x1", 2, {}, {23, 6, 10, 1000});
    std::barrier gate(4);
    ExecutionContext execution({4});
    execution.run(4, [&](std::size_t) {
        gate.arrive_and_wait();
        TrainingObjective objective(p, 16);
        ConstantFitConfig fit;
        fit.maxLossAttempts = 16;
        for (int i = 0; i < 1; ++i) {
            auto result = fitConstants(e, objective, fit);
            check(result.loss.state() != FitnessState::Unevaluated,
                  "private dimensional preparation/fitting");
        }
    });
    check(execution.peakTasks() == 4, "dimensional forced overlap");
}
} // namespace
int main(int argc, char **) {
    try {
        if (argc == 1) {
            contracts();
            for (unsigned workers : {1U, 2U, 4U}) {
                resume(false, workers);
                resume(true, workers);
            }
        }
        overlap();
        std::cout << "D5 dimensional/constraint/run contracts passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
