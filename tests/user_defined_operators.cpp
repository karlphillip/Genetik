#include <SymbolicRegression.hpp>
#include <ScalarExpressionCheckpoint.hpp>
#include <ScalarMathematicsRun.hpp>
#include <ExpressionFormatting.hpp>
#include <ExpressionParsing.hpp>
#include <ExpressionSecondDerivative.hpp>
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
                const double small = std::exp(-std::abs(d));
                const double denominator = 1 + small;
                return d >= 0 ? std::vector<double>{1 / denominator, small / denominator}
                              : std::vector<double>{small / denominator, 1 / denominator};
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
        const auto checkReservedExport = [](std::string id, std::size_t arity,
                                            std::vector<ExpressionNode> nodes,
                                            std::span<const double> input, double expected) {
            CustomScalarOperator operation{
                std::move(id),
                arity,
                [arity](std::span<const double> args) -> std::optional<double> {
                    return arity == 1 ? args[0] * args[0] : args[0] + args[1];
                },
                {}};
            nodes.push_back(ExpressionNode::custom(operation));
            Expression expression(nodes, input.size());
            const auto text = expression.toString();
            const auto value = expression.evaluate(input);
            check(value.valid() && value.value() == expected,
                  "custom ID example keeps its original value");
            check(text.starts_with("custom(\"") && text.find(operation.id) != std::string::npos,
                  "custom export uses reserved ID-bearing form");
            bool rejected = false;
            try {
                (void)parseExpression(text, input.size());
            } catch (const ExpressionParseError& error) {
                rejected = error.category() == ExpressionParseCategory::UnknownSymbol;
            }
            check(rejected, "built-in parser rejects custom export instead of reinterpreting it");
        };
        checkReservedExport("x0-sin", 1, {ExpressionNode::variable(0)}, std::array{2.0}, 4.0);
        checkReservedExport("1-exp", 1, {ExpressionNode::variable(0)}, std::array{2.0}, 4.0);
        checkReservedExport("x0/sin", 1, {ExpressionNode::variable(0)}, std::array{2.0}, 4.0);
        checkReservedExport("example.logaddexp/1", 2,
                            {ExpressionNode::constant(2.0), ExpressionNode::constant(3.0)},
                            std::array<double, 0>{}, 5.0);
        const auto squareNamedSin = CustomScalarOperator{
            "sin",
            1,
            [](std::span<const double> a) -> std::optional<double> { return a[0] * a[0]; },
            {}};
        const auto squareValue = squareNamedSin.evaluate(std::array{2.0});
        check(squareValue && *squareValue == 4.0,
              "collision regression's custom square callback is 4 at x=2");
        bool ambiguousRejected = false;
        try {
            (void)ExpressionNode::custom(squareNamedSin);
        } catch (const std::invalid_argument&) {
            ambiguousRejected = true;
        }
        check(ambiguousRejected,
              "custom ID cannot shadow built-in sin and re-export as ordinary sine");
        auto ambiguousAlias = squareNamedSin;
        ambiguousAlias.id = "ln";
        bool aliasRejected = false;
        try {
            (void)ExpressionNode::custom(ambiguousAlias);
        } catch (const std::invalid_argument&) {
            aliasRejected = true;
        }
        check(aliasRejected, "custom ID cannot shadow formatted built-in aliases");
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
        for (const auto& [a, b, smallIndex] :
             {std::tuple{40.0, 0.0, std::size_t{1}}, std::tuple{0.0, 40.0, std::size_t{0}}}) {
            const auto partial = evaluatePartial(e, std::array{a, b}, smallIndex);
            const double actual = partial.derivative();
            const double expected = std::exp(-40.0) / (1.0 + std::exp(-40.0));
            check(partial.derivativeAvailable() && actual != 0.0 &&
                      std::abs((actual - expected) / expected) < 1e-14,
                  "small logaddexp partial retained without cancellation");
        }
        const auto pretty = formatExpression(e, ExpressionTextFormat::Unicode);
        const auto latex = formatExpression(e, ExpressionTextFormat::LaTeX);
        check(pretty.find(op.id) != std::string::npos && latex.find(op.id) != std::string::npos &&
                  latex.find("\\operatorname{") != std::string::npos,
              "custom identity is preserved in Unicode and LaTeX");
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
        const Expression constantUnary(
            std::array{ExpressionNode::constant(2), ExpressionNode::custom(primalOnly)}, 1);
        const auto unaryConstantSecond =
            evaluateSecondPartial(constantUnary, std::array{0.0}, 0, 0);
        check(unaryConstantSecond.status == ScientificStatus::Success &&
                  unaryConstantSecond.derivatives->second == 0.0,
              "variable-independent custom unary subtree has zero second partial");
        const auto unaryActiveSecond = evaluateSecondPartial(primalOnlyExpr, std::array{2.0}, 0, 0);
        check(unaryActiveSecond.status == ScientificStatus::Unavailable,
              "active custom unary second partial reports unavailable rule");
        CustomScalarOperator binaryPrimalOnly{
            "example.binary_primal_only/1",
            2,
            [](std::span<const double> a) -> std::optional<double> { return a[0] + a[1]; },
            {}};
        const Expression binaryConstant(std::array{ExpressionNode::constant(2),
                                                   ExpressionNode::constant(3),
                                                   ExpressionNode::custom(binaryPrimalOnly)},
                                        1);
        const auto binaryConstantSecond =
            evaluateSecondPartial(binaryConstant, std::array{0.0}, 0, 0);
        check(binaryConstantSecond.status == ScientificStatus::Success &&
                  binaryConstantSecond.derivatives->second == 0.0,
              "variable-independent custom binary subtree has zero second partial");
        const Expression binaryActive(std::array{ExpressionNode::variable(0),
                                                 ExpressionNode::constant(3),
                                                 ExpressionNode::custom(binaryPrimalOnly)},
                                      1);
        const auto binaryActiveSecond = evaluateSecondPartial(binaryActive, std::array{2.0}, 0, 0);
        check(binaryActiveSecond.status == ScientificStatus::Unavailable,
              "active custom binary second partial reports unavailable rule");
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
        auto customRunConfig = cfg;
        customRunConfig.maxGenerationAttempts = 0;
        customRunConfig.maxTrainingLossAttempts = 1000;
        auto customRunProblem = ScalarMathematicsProblem::scientific(rows, {7, 4, 4});
        ExecutionContext customExecution({1});
        RandomContext customRunRandom(29, customExecution);
        ScalarMathematicsRun customRun(customRunProblem, customRunConfig, customRunRandom);
        bool runContainsCustom = false;
        const auto customRunMembers = customRun.population().creatures();
        for (const auto& member : *customRunMembers)
            for (const auto& node : member.expression().nodes())
                runContainsCustom |= node.kind() == ExpressionKind::CustomBinary &&
                                     node.customOperator().id == op.id;
        check(runContainsCustom, "full-run checkpoint fixture contains a custom candidate");
        RandomContext customResumeRandom(29, customExecution);
        auto customResumed = ScalarMathematicsRun::restore(customRun.checkpoint(), customRunProblem,
                                                           customRunConfig, customResumeRandom);
        check(customResumed->population().creatures()->size() ==
                  customRun.population().creatures()->size(),
              "full-run checkpoint restores custom expression candidates");
        // Full-run restore carries the application registry. Depth one ensures
        // this checkpoint's population contains terminals only, so the grammar
        // compatibility guard cannot rely on inspecting stored expressions.
        auto runRows = std::vector<RegressionRow>{{{0}, 0.3}, {{1}, 2.3}};
        auto runProblem = ScalarMathematicsProblem::scientific(runRows, {12, 4, 4});
        SymbolicRegressionConfig runConfig;
        runConfig.population = 6;
        runConfig.maxEvaluations = 30;
        runConfig.maxTrainingLossAttempts = 100;
        runConfig.maxGenerationAttempts = 1;
        runConfig.crossoverRate = 0;
        runConfig.mutationRate = 0;
        runConfig.grammar.operators.clear();
        runConfig.grammar.constants = {0};
        runConfig.grammar.initialMaxDepth = 1;
        runConfig.grammar.mutationMaxDepth = 1;
        runConfig.grammar.customOperators = {op, unary};
        ExecutionContext runExecution({1});
        RandomContext runRandom(177, runExecution);
        ScalarMathematicsRun scalarRun(runProblem, runConfig, runRandom);
        const auto terminalMembers = scalarRun.population().creatures();
        for (const auto& member : *terminalMembers)
            check(member.expression().nodeCount() == 1,
                  "custom-grammar checkpoint fixture has terminal-only population");
        const auto runCheckpoint = scalarRun.checkpoint();
        RandomContext resumedRandom(177, runExecution);
        auto resumedRun =
            ScalarMathematicsRun::restore(runCheckpoint, runProblem, runConfig, resumedRandom);
        (void)scalarRun.step();
        (void)resumedRun->step();
        const auto uninterrupted = scalarRun.population().creatures();
        const auto resumedMembers = resumedRun->population().creatures();
        check(uninterrupted->size() == resumedMembers->size(),
              "full custom-grammar run resumed population size");
        for (std::size_t i = 0; i < uninterrupted->size(); ++i)
            check((*uninterrupted)[i].text() == (*resumedMembers)[i].text() &&
                      (*uninterrupted)[i].fitness() == (*resumedMembers)[i].fitness(),
                  "full custom-grammar run checkpoint/resume equivalence");
        auto changedGrammar = runConfig;
        auto changedOperator = op;
        changedOperator.id = "example.logaddexp/changed";
        changedGrammar.grammar.customOperators = {changedOperator, unary};
        bool grammarRejected = false;
        try {
            RandomContext changedRandom(177, runExecution);
            (void)ScalarMathematicsRun::restore(runCheckpoint, runProblem, changedGrammar,
                                                changedRandom);
        } catch (const CheckpointError&) {
            grammarRejected = true;
        }
        check(grammarRejected,
              "run checkpoint rejects changed custom grammar with terminal-only population");
        auto reorderedGrammar = runConfig;
        reorderedGrammar.grammar.customOperators = {unary, op};
        bool orderRejected = false;
        try {
            RandomContext orderRandom(177, runExecution);
            (void)ScalarMathematicsRun::restore(runCheckpoint, runProblem, reorderedGrammar,
                                                orderRandom);
        } catch (const CheckpointError&) {
            orderRejected = true;
        }
        check(orderRejected, "run checkpoint binds custom operator registration order");
        auto changedArityGrammar = runConfig;
        auto changedArityOperator = op;
        changedArityOperator.arity = 1;
        changedArityGrammar.grammar.customOperators[0] = changedArityOperator;
        bool arityRejected = false;
        try {
            RandomContext arityRandom(177, runExecution);
            (void)ScalarMathematicsRun::restore(runCheckpoint, runProblem, changedArityGrammar,
                                                arityRandom);
        } catch (const CheckpointError&) {
            arityRejected = true;
        }
        check(arityRejected, "run checkpoint binds each custom operator arity");
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
