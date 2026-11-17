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
    ExpressionGrammar powerGrammar;
    powerGrammar.operators = {ExpressionKind::Powi, ExpressionKind::NegativePowi};
    powerGrammar.exponents = {2};
    powerGrammar.negativeExponents = {3};
    ExpressionVariation powerVariation(problem, powerGrammar);
    const auto variable = Expression::variable(0, 1, problem->limits());
    const auto positivePower = Expression::integerPower(variable, 2, problem->limits());
    const auto negativePower = Expression::integerPower(variable, -3, problem->limits());
    const auto wrongPositivePower = Expression::integerPower(variable, 3, problem->limits());
    const auto wrongNegativePower = Expression::integerPower(variable, -2, problem->limits());
    check(powerVariation.grammarAccepts(positivePower), "configured positive power rejected");
    check(powerVariation.grammarAccepts(negativePower), "configured negative power rejected");
    check(!powerVariation.grammarAccepts(wrongPositivePower), "unconfigured positive power accepted");
    check(!powerVariation.grammarAccepts(wrongNegativePower), "unconfigured negative power accepted");
    ExpressionGrammar generalPowerGrammar;
    generalPowerGrammar.operators = {ExpressionKind::Pow};
    ExpressionVariation generalPowerVariation(problem, generalPowerGrammar);
    const auto constantExponentPower = Expression::binary(
        ExpressionKind::Pow, variable, Expression::constant(2, 1, problem->limits()),
        problem->limits());
    const auto variableExponentPower = Expression::binary(
        ExpressionKind::Pow, variable, variable, problem->limits());
    check(generalPowerVariation.grammarAccepts(constantExponentPower),
          "allowed constant general-power exponent rejected");
    check(!generalPowerVariation.grammarAccepts(variableExponentPower),
          "variable general-power exponent accepted");
    const auto rejectsSeed = [](ExpressionVariation& variation, const Expression& expression) {
        try {
            (void)variation.initializeFrom(expression);
            return false;
        } catch (const std::invalid_argument&) {
            return true;
        } catch (const std::length_error&) {
            return true;
        }
    };
    check(rejectsSeed(powerVariation, Expression::variable(1, 2, problem->limits())),
          "seed with a mismatched input schema was accepted");
    check(rejectsSeed(powerVariation,
                      Expression::unary(ExpressionKind::Log, variable, problem->limits())),
          "seed with a disallowed operator was accepted");
    check(rejectsSeed(powerVariation, Expression::constant(101, 1, {127, 16, 200, 1024})),
          "seed outside the constant bound was accepted");
    auto smallLimitProblem = RegressionProblem::create(rows, {2, 16, 100, 1024});
    ExpressionVariation smallLimitVariation(smallLimitProblem, powerGrammar);
    check(rejectsSeed(smallLimitVariation,
                      Expression::binary(ExpressionKind::Add, variable, variable,
                                         problem->limits())),
          "seed outside the node limit was accepted");
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
    auto reusedProvider = rationalInitialExpressionProvider(rational);
    const auto lowAllowance = reusedProvider(*problem, 2, 50'000);
    const auto highAllowance = reusedProvider(*problem, 2, 50'000'000);
    const auto freshHighAllowance = rationalInitialExpressionProvider(rational)(
        *problem, 2, 50'000'000);
    check(!lowAllowance.expressions.empty(), "low-budget provider call returned no fit");
    check(highAllowance.status == freshHighAllowance.status &&
              highAllowance.detail == freshHighAllowance.detail &&
              highAllowance.workUnits == freshHighAllowance.workUnits &&
              highAllowance.expressions.size() == freshHighAllowance.expressions.size(),
          "provider budget leaked across invocations");
    for (std::size_t i = 0; i < highAllowance.expressions.size(); ++i)
        check(highAllowance.expressions[i].toString() == freshHighAllowance.expressions[i].toString(),
              "provider result changed after low-budget invocation");
    auto cannotInitialize = cfg;
    cannotInitialize.maxEvaluations = cannotInitialize.population - 1;
    bool providerCalled = false;
    auto observeProvider = [&](const RegressionProblem&, std::size_t, std::uint64_t) {
        providerCalled = true;
        return InitialExpressionBatch{};
    };
    RandomContext noAdmissionRandom(19);
    const auto noAdmission = symbolicRegression(problem, cannotInitialize, noAdmissionRandom,
                                                {}, observeProvider);
    check(noAdmission.stoppingReason == SearchStop::EvaluationBudget && !providerCalled,
          "provider ran without budget to admit initialization");
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
