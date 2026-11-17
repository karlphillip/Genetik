#include <RationalSymbolicRegression.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main() {
    std::vector<RegressionRow> rows;
    for (int i = -12; i <= 12; ++i) {
        const double x = i / 6.0;
        rows.push_back({{x}, (1.0 + .4 * x) / (1.0 + .2 * x)});
    }
    auto problem = RegressionProblem::create(rows, {127, 16, 100, 1024});
    SymbolicRegressionConfig cfg;
    cfg.population = 8;
    cfg.elites = 1;
    cfg.maxGenerationAttempts = 3;
    cfg.maxEvaluations = 32;
    cfg.grammar.operators = {ExpressionKind::Add, ExpressionKind::Subtract,
                             ExpressionKind::Multiply, ExpressionKind::Divide,
                             ExpressionKind::Powi};
    cfg.grammar.constants = {-1, 0, 1};
    cfg.grammar.exponents = {0, 1, 2, 3};
    RationalConfig rational;
    rational.maximumNumeratorDegree = 1;
    rational.maximumDenominatorDegree = 1;
    rational.maximumParameters = 3;
    rational.maximumAbsoluteCoefficient = 10;
    rational.complexityWeight = 0;
    cfg.initialExpressionSlots = 2;
    cfg.maximumInitializerWorkUnits = 50'000'000;
    RandomContext hybridRandom(19);
    const auto hybrid = symbolicRegression(problem, cfg, hybridRandom, {},
                                           rationalInitialExpressionProvider(rational));
    check(hybrid.best.has_value(), "hybrid returned no model");
    check(hybrid.rawInitialExpressions.size() == cfg.population, "wrong initial population size");
    check(hybrid.variation.initializations == cfg.population,
          "seeded slots were omitted from initialization accounting");
    check(hybrid.initializerWorkUnits > 0 &&
              hybrid.initializerWorkUnits <= cfg.maximumInitializerWorkUnits,
          "initializer work not bounded/reported");
    check(hybrid.candidateEvaluations >= cfg.population,
          "prepared candidates not charged to candidate budget");
    check(hybrid.rawInitialExpressions.front().find('/') != std::string::npos,
          "rational expression missing from reserved slot");
    auto bad = cfg;
    bad.grammar.operators = {ExpressionKind::Add, ExpressionKind::Multiply};
    RandomContext invalidRandom(19);
    bool rejected = false;
    try {
        (void)symbolicRegression(problem, bad, invalidRandom, {},
                                 rationalInitialExpressionProvider(rational));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "out-of-grammar initializer was accepted");
    auto oversized = cfg;
    oversized.initialExpressionSlots = 3;
    RandomContext oversizedRandom(21);
    rejected = false;
    try {
        (void)symbolicRegression(problem, oversized, oversizedRandom, {},
                                 rationalInitialExpressionProvider(rational));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "initializer reserved more than the bounded population fraction");
    auto zeroWork = [](const RegressionProblem&, std::size_t, std::uint64_t) {
        InitialExpressionBatch batch;
        batch.expressions.push_back(Expression::variable(0, 1));
        return batch;
    };
    RandomContext noWorkRandom(20);
    rejected = false;
    try {
        (void)symbolicRegression(problem, cfg, noWorkRandom, {}, zeroWork);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "unaccounted initializer work was accepted");
    RandomContext repeatRandom(19);
    const auto repeat = symbolicRegression(problem, cfg, repeatRandom, {},
                                           rationalInitialExpressionProvider(rational));
    check(repeat.best && repeat.best->text() == hybrid.best->text() &&
              repeat.candidateEvaluations == hybrid.candidateEvaluations &&
              repeat.initializerWorkUnits == hybrid.initializerWorkUnits &&
              repeatRandom.engine() == hybridRandom.engine(),
          "enabled initializer path is not reproducible");
    auto ordinary = cfg;
    ordinary.initialExpressionSlots = 0;
    ordinary.maximumInitializerWorkUnits = 0;
    RandomContext a(33), b(33);
    const auto x = symbolicRegression(problem, ordinary, a),
               y = symbolicRegression(problem, ordinary, b, {}, {});
    check(x.best && y.best && x.best->text() == y.best->text() &&
              x.candidateEvaluations == y.candidateEvaluations,
          "disabled initializer changed ordinary outputs");
    check(a.engine() == b.engine(), "disabled initializer changed RNG draw order");
    std::cout << "rational symbolic initializer contracts passed\n";
}
