#include <SymbolicRegression.hpp>
#include <ScalarExpressionCheckpoint.hpp>
#include <array>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <optional>
#include <span>
#include <tuple>
#include <vector>

static void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
static CustomScalarOperator logaddexp() {
    return {"example.logaddexp/1", 2,
            [](std::span<const double> x) -> std::optional<double> {
                const auto m = std::max(x[0], x[1]);
                return m + std::log1p(std::exp(-std::abs(x[0] - x[1])));
            },
            [](std::span<const double> x) -> std::optional<std::vector<double>> {
                const double d = x[0] - x[1];
                const double p = 1 / (1 + std::exp(-d));
                return std::vector<double>{p, 1 - p};
            }};
}
int main() {
    try {
        ExpressionGrammar defaults;
        check(defaults.customOperators.empty() &&
                  defaults.operators == std::vector<ExpressionKind>{ExpressionKind::Add,
                                                                    ExpressionKind::Subtract,
                                                                    ExpressionKind::Multiply},
              "custom operators are opt-in and default grammar is unchanged");
        const auto op = logaddexp();
        const auto x = Expression::variable(0, 2), y = Expression::variable(1, 2);
        const auto expX = Expression::unary(ExpressionKind::Exp, x),
                   expY = Expression::unary(ExpressionKind::Exp, y);
        const auto direct = Expression::unary(ExpressionKind::Log,
                                              Expression::binary(ExpressionKind::Add, expX, expY));
        const auto difference = Expression::binary(ExpressionKind::Subtract, x, y);
        const auto stable = Expression::binary(
            ExpressionKind::Add, Expression::binary(ExpressionKind::Max, x, y),
            Expression::unary(
                ExpressionKind::Log1p,
                Expression::unary(
                    ExpressionKind::Exp,
                    Expression::binary(ExpressionKind::Subtract, Expression::constant(0, 2),
                                       Expression::unary(ExpressionKind::Abs, difference)))));
        const Expression e(std::array{ExpressionNode::variable(0), ExpressionNode::variable(1),
                                      ExpressionNode::custom(op)},
                           2);
        check(direct.nodeCount() == 6 && stable.nodeCount() == 12 && e.nodeCount() == 3,
              "operator-node complexity comparison");
        check(e.toString().find(op.id) != std::string::npos, "custom operator export identity");
        for (auto [a, b, expected] :
             {std::tuple{0., 0., std::log(2.)}, std::tuple{1000., 1000., 1000. + std::log(2.)},
              std::tuple{-1000., -1000., -1000. + std::log(2.)},
              std::tuple{1000., -1000., 1000.}}) {
            auto got = e.evaluate(std::array{a, b});
            check(got.valid() && std::abs(got.value() - expected) < 1e-12, "stable logaddexp");
        }
        std::cout << std::setprecision(17) << "a,b,direct,stable,custom,nodes=6/12/3\n";
        for (auto [a, b, expected] :
             {std::tuple{0., 0., std::log(2.)}, std::tuple{1000., 1000., 1000. + std::log(2.)},
              std::tuple{-1000., -1000., -1000. + std::log(2.)},
              std::tuple{1000., -1000., 1000.}}) {
            auto d = direct.evaluate(std::array{a, b}), s = stable.evaluate(std::array{a, b}),
                 c = e.evaluate(std::array{a, b});
            check(std::abs(c.value() - expected) < 1e-12 && s.valid() &&
                      std::abs(s.value() - expected) < 1e-12,
                  "stable comparison agrees with reference");
            if (a == 0 && b == 0)
                check(d.valid() && std::abs(d.value() - expected) < 1e-12,
                      "direct formula ordinary range");
            else
                check(!d.valid(), "direct formula exposes overflow or underflow");
            std::cout << a << ',' << b << ',';
            if (d.valid())
                std::cout << d.value();
            else
                std::cout << "invalid";
            std::cout << ',' << s.value() << ',' << c.value() << '\n';
        }
        auto deriv = evaluatePartial(e, std::array{0., 0.}, 0);
        check(deriv.derivativeAvailable() && std::abs(deriv.derivative() - .5) < 1e-15,
              "custom analytic derivative");
        CustomScalarOperator unary{
            "example.softplus/1", 1,
            [](std::span<const double> a) -> std::optional<double> {
                return std::max(a[0], 0.0) + std::log1p(std::exp(-std::abs(a[0])));
            },
            [](std::span<const double> a) -> std::optional<std::vector<double>> {
                return std::vector<double>{1 / (1 + std::exp(-a[0]))};
            }};
        const Expression unaryExpr(
            std::array{ExpressionNode::variable(0), ExpressionNode::custom(unary)}, 1);
        auto unaryDerivative = evaluatePartial(unaryExpr, std::array{0.}, 0);
        check(unaryExpr.evaluate(std::array{0.}).value() == std::log(2.) &&
                  unaryDerivative.derivativeAvailable() && unaryDerivative.derivative() == .5,
              "custom unary operator and derivative");
        CustomScalarOperator primalOnly{
            "example.primal_only/1",
            1,
            [](std::span<const double> a) -> std::optional<double> { return std::abs(a[0]); },
            {}};
        const Expression primalOnlyExpr(
            std::array{ExpressionNode::variable(0), ExpressionNode::custom(primalOnly)}, 1);
        const auto unavailable = evaluatePartial(primalOnlyExpr, std::array{2.}, 0);
        check(unavailable.valueAvailable() && !unavailable.derivativeAvailable() &&
                  unavailable.derivativeFailure() == DerivativeFailure::UnsupportedRule,
              "missing custom partials preserve primal value and report unavailable derivative");
        Expression fitExpr(std::array{ExpressionNode::variable(0), ExpressionNode::constant(.5),
                                      ExpressionNode::custom(op)},
                           1);
        std::vector<RegressionRow> fitRows{{{-2}, std::log1p(std::exp(-2.))},
                                           {{0}, std::log(2.)},
                                           {{2}, 2 + std::log1p(std::exp(-2.))}};
        auto fitProblem = RegressionProblem::create(fitRows, {7, 4, 4});
        auto fitObj = std::make_shared<TrainingObjective>(fitProblem, 1000);
        ConstantFitConfig fitCfg;
        fitCfg.lower = -4;
        fitCfg.upper = 4;
        auto coordinateFit = fitConstants(fitExpr, *fitObj, fitCfg);
        check(coordinateFit.expression.nodeCount() == 3 && coordinateFit.improved &&
                  coordinateFit.loss.score() < coordinateFit.startingLoss.score(),
              "default coordinate fitter fits a custom operator constant");
        auto lmObjective = std::make_shared<TrainingObjective>(fitProblem, 1000);
        fitCfg.method = ConstantFitMethod::LevenbergMarquardt;
        auto fit = fitConstants(fitExpr, *lmObjective, fitCfg);
        check(fit.expression.nodeCount() == 3 && fit.improved &&
                  fit.loss.score() < fit.startingLoss.score(),
              "existing LM fits a custom operator constant");
        auto rows = std::vector<RegressionRow>{{{0, 0}, std::log(2.)},
                                               {{1, 0}, 1 + std::log1p(std::exp(-1.))},
                                               {{0, 1}, 1 + std::log1p(std::exp(-1.))}};
        auto problem = RegressionProblem::create(rows, {7, 4, 4});
        SymbolicRegressionConfig cfg;
        cfg.population = 32;
        cfg.elites = 1;
        cfg.maxEvaluations = 32;
        cfg.maxGenerationAttempts = 0;
        cfg.grammar.operators.clear();
        cfg.grammar.customOperators = {op};
        cfg.grammar.initialMaxDepth = 2;
        cfg.grammar.constants = {0};
        RandomContext rng(29);
        auto result = symbolicRegression(problem, cfg, rng);
        bool seen = false;
        for (const auto& text : result.rawInitialExpressions)
            if (text.find(op.id) != std::string::npos)
                seen = true;
        check(seen, "search generated and scored a custom operator");
        auto unaryProblem =
            RegressionProblem::create({{{0}, std::log(2.)}, {{1}, 1.3132616875182228}}, {7, 4, 4});
        auto unaryCfg = cfg;
        unaryCfg.grammar.customOperators = {unary};
        RandomContext unaryRng(29);
        auto unarySearch = symbolicRegression(unaryProblem, unaryCfg, unaryRng);
        bool unarySeen = false;
        for (const auto& text : unarySearch.rawInitialExpressions)
            if (text.find(unary.id) != std::string::npos)
                unarySeen = true;
        check(unarySeen, "search generated and scored a custom unary operator");
        ExpressionCandidate candidate(e, problem);
        candidate.calculateFitness();
        CheckpointWriter writer;
        CandidateCheckpointCodec<ExpressionCandidate>::encode(writer, candidate);
        CheckpointReader reader(writer.data());
        ScalarCheckpointContext context{problem, {}, {op}};
        auto restored = CandidateCheckpointCodec<ExpressionCandidate>::decode(reader, context);
        reader.finish();
        check(restored.expression().sameRepresentation(e),
              "checkpoint restores registered operator identity");
        bool missing = false;
        try {
            CheckpointReader noRegistry(writer.data());
            ScalarCheckpointContext empty{problem, {}, {}};
            (void)CandidateCheckpointCodec<ExpressionCandidate>::decode(noRegistry, empty);
        } catch (const CheckpointError&) {
            missing = true;
        }
        check(missing, "checkpoint rejects missing custom registration");
        std::cout << "user-defined operator contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
