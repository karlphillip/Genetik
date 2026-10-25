#include <ScalarMathematicsRun.hpp>
#include <IntegerRecurrence.hpp>
#include <ExpressionParsing.hpp>
#include <DiscreteExpressionParsing.hpp>
#include <ExpressionFormatting.hpp>
#include <DiscreteExpressionFormatting.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
namespace {
std::string jsonString(std::string_view s) {
    std::string r = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\')
            r += '\\';
        if (c == '\n') {
            r += "\\n";
            continue;
        }
        r += c;
    }
    return r + '"';
}
std::int64_t fib(std::int64_t n) {
    std::int64_t a = 0, b = 1;
    for (std::int64_t i = 0; i < n; ++i) {
        auto next = a + b;
        a = b;
        b = next;
    }
    return a;
}
std::string discreteStop(DiscreteSearchStop s) {
    switch (s) {
    case DiscreteSearchStop::ExactFit:
        return "exact-fit";
    case DiscreteSearchStop::GenerationLimit:
        return "generation-limit";
    case DiscreteSearchStop::LossBudget:
        return "loss-budget";
    case DiscreteSearchStop::Cancelled:
        return "cancelled";
    default:
        return "no-viable";
    }
}
void discrete(const std::string &task, unsigned seed, unsigned workers, const std::filesystem::path &output) {
    const bool recurrence = task == "fibonacci";
    DiscreteExpressionLimits limits;
    limits.maxNodes = 21;
    limits.maxDepth = 6;
    DiscreteEvaluationLimits evaluation;
    evaluation.maxNodeVisits = evaluation.maxOperations = 100000;
    evaluation.maxAggregateIterations = 4096;
    evaluation.maxIterationsPerOperation = 64;
    std::shared_ptr<const DiscreteRegressionProblem> problem;
    std::vector<std::pair<std::vector<std::int64_t>, std::int64_t>> held;
    if (recurrence) {
        std::vector<IntegerRecurrenceObservation> rows;
        for (std::int64_t n = 0; n <= 10; ++n)
            rows.push_back({n, {}, fib(n)});
        problem = recurrenceRegressionProblem(0, {0, 1}, rows, limits, evaluation);
        for (std::int64_t n = 11; n <= 16; ++n)
            held.push_back({{n}, fib(n)});
    } else {
        std::vector<DiscreteRegressionRow> rows;
        for (std::int64_t a = 1; a <= 4; ++a)
            for (std::int64_t b = 1; b <= 4; ++b)
                rows.push_back({{a, b}, a * b});
        problem = DiscreteRegressionProblem::create(2, rows, limits, evaluation);
        for (std::int64_t a = 5; a <= 7; ++a)
            for (std::int64_t b = 5; b <= 7; ++b)
                held.push_back({{a, b}, a * b});
    }
    DiscreteSearchConfig config;
    config.population = 24;
    config.maxGenerations = 40;
    config.maxLossAttempts = 984;
    config.elites = 2;
    config.grammar.operators =
        recurrence ? std::vector{DiscreteKind::Add, DiscreteKind::Subtract, DiscreteKind::Multiply}
                   : std::vector{DiscreteKind::Add, DiscreteKind::Subtract, DiscreteKind::Multiply,
                                 DiscreteKind::Gcd, DiscreteKind::Lcm};
    config.grammar.constants = {0, 1, 2};
    config.grammar.initialMaxDepth = 4;
    config.grammar.mutationMaxDepth = 3;
    config.contextIdentity = "D5-benchmarks-fixed/1;" + task;
    // Direct reference construction is validated separately and never seeded.
    auto constructed =
        parseDiscreteExpression(recurrence ? "x1+x2" : "gcd(x0,x1)*lcm(x0,x1)", recurrence ? 3 : 2, limits);
    auto direct = problem->measure(constructed);
    if (direct.loss != Fitness::valid(0))
        throw std::runtime_error("constructed discrete benchmark reference failed");
    RandomContext random(seed, {workers});
    DiscreteEvolutionRun run(problem, config, random);
    std::string stop;
    try {
        while (!run.stoppingReason())
            run.step();
    } catch (const DiscreteResourceExhausted &) {
        stop = "expression-resource";
    }
    if (stop.empty())
        stop = discreteStop(*run.stoppingReason());
    run.save(output / "final.gtkc");
    auto best = run.population().bestSeen();
    if (best) {
        auto independent = problem->measure(best->expression(), true);
        if (independent.loss != best->evaluation())
            throw std::runtime_error(
                "selected discrete score does not match independent complete assessment");
    }

    std::size_t matches = 0, invalid = 0, operations = 0;
    for (auto &[input, target] : held)
        if (best) {
            DiscreteEvaluationResult result =
                recurrence
                    ? IntegerRecurrence(0, {0, 1}, best->expression()).evaluate(input[0], {}, evaluation)
                    : best->expression().evaluate(input, evaluation);
            operations += result.operations;
            if (result.valid())
                matches += result.value->integer() == target;
            else
                ++invalid;
        }
    std::cout << "{\"task\":" << jsonString(task) << ",\"seed\":" << seed
              << ",\"constructed_training_loss\":" << direct.loss.score()
              << ",\"training_loss\":" << (best ? best->fitness() : -1)
              << ",\"canonical\":" << jsonString(best ? best->text() : "")
              << ",\"pretty\":" << jsonString(best ? formatDiscreteExpression(best->expression()) : "")
              << ",\"nodes\":" << (best ? best->expression().nodeCount() : 0)
              << ",\"depth\":" << (best ? best->expression().depth() : 0)
              << ",\"generation\":" << *run.population().generation()
              << ",\"best_generation\":" << run.population().bestGeneration().value_or(0)
              << ",\"attempts\":" << run.work().lossAttempts << ",\"row_attempts\":" << run.work().rowAttempts
              << ",\"operations\":" << run.work().operations << ",\"iterations\":" << run.work().iterations
              << ",\"invalid_losses\":" << run.work().invalidLosses << ",\"held_matches\":" << matches
              << ",\"held_count\":" << held.size() << ",\"held_invalid\":" << invalid
              << ",\"held_operations\":" << operations << ",\"stop\":" << jsonString(stop) << "}";
}
double reference(const std::string &task, double x, double y = 0) {
    if (task == "powers")
        return 3.5 * std::pow(x, 1.7);
    if (task == "trig")
        return 1.5 * std::sin(.75 * x + .25);
    if (task == "hyperbolic")
        return std::tanh(x);
    if (task == "polynomial")
        return (x + 2) * (x - 2);
    if (task == "energy")
        return .5 * x * y * y;
    return std::max(0., x);
}
void scalar(const std::string &task, unsigned seed, unsigned workers, const std::filesystem::path &output) {
    std::vector<RegressionRow> train, held;
    const bool energy = task == "energy";
    if (energy) {
        for (double m : {1., 2., 3.})
            for (double v : {.5, 1., 2.})
                train.push_back({{m, v}, reference(task, m, v)});
        for (double m : {.75, 2.5})
            for (double v : {.75, 1.5, 2.5})
                held.push_back({{m, v}, reference(task, m, v)});
    } else {
        for (double x :
             task == "powers" ? std::vector{.25, .5, 1., 1.5, 2.} : std::vector{-2., -1., 0., 1., 2.})
            train.push_back({{x}, reference(task, x)});
        for (double x : task == "powers" ? std::vector{.125, .625, 1.375, 2.5, 3.}
                                         : std::vector{-3., -1.5, -.5, .5, 1.5, 3.})
            held.push_back({{x}, reference(task, x)});
    }
    ExpressionLimits limits{23, 6, 8, 1000};
    auto mass = PhysicalDimension::base(SIBaseDimension::Mass),
         velocity = PhysicalDimension::base(SIBaseDimension::Length) /
                    PhysicalDimension::base(SIBaseDimension::Time);
    auto problem = energy ? ScalarMathematicsProblem::dimensional(train, {mass, velocity},
                                                                  mass * velocity * velocity, limits)
                          : ScalarMathematicsProblem::scientific(train, limits);
    SymbolicRegressionConfig config;
    config.population = 24;
    config.elites = 2;
    config.maxGenerationAttempts = 30;
    config.maxEvaluations = 744;
    config.maxTrainingLossAttempts = 2048;
    config.trainingLossThreshold = 1e-12;
    config.grammar.constants = {-2, -1, 0, .5, 1, 2};
    config.grammar.initialMaxDepth = 4;
    config.grammar.mutationMaxDepth = 3;
    config.grammar.operators = {ExpressionKind::Add, ExpressionKind::Subtract, ExpressionKind::Multiply};
    if (task == "powers") {
        config.grammar.operators.push_back(ExpressionKind::Pow);
        config.grammar.realExponents = {.5, 1, 1.5, 1.7, 2, 3};
    }
    if (task == "trig")
        config.grammar.operators.push_back(ExpressionKind::Sin);
    if (task == "hyperbolic")
        config.grammar.operators.push_back(ExpressionKind::Tanh);
    if (task == "piecewise")
        config.grammar.operators.push_back(ExpressionKind::Max);
    ConstantFitConfig fitting;
    fitting.maxLossAttempts = 64;
    config.fitting = fitting;
    const std::string formula = task == "powers"       ? "3.5*pow(x0,1.7)"
                                : task == "trig"       ? "1.5*sin(.75*x0+.25)"
                                : task == "hyperbolic" ? "tanh(x0)"
                                : task == "polynomial" ? "(x0+2)*(x0-2)"
                                : energy               ? ".5*x0*x1*x1"
                                                       : "max(0,x0)";
    auto constructed = parseExpression(formula, energy ? 2 : 1, {}, limits);
    auto direct = problem.problem()->measure(constructed);
    if (direct.loss.state() != FitnessState::Valid || direct.loss.score() > 1e-27)
        throw std::runtime_error("constructed scalar analytic reference failed");
    RandomContext random(seed, {workers});
    ScalarMathematicsRun run(problem, config, random);
    std::string stop;
    try {
        while (!run.stoppingReason())
            run.step();
    } catch (...) {
        auto error = std::current_exception();
        if (!isEvaluationStop(error))
            throw;
        stop = isEvaluationBudget(error) ? "loss-budget" : "cancelled";
    }
    if (stop.empty())
        stop = searchStopName(*run.stoppingReason());
    run.save(output / "final.gtkc");
    auto best = run.population().bestSeen();
    if (best) {
        auto independent = problem.problem()->measure(best->expression());
        if (independent.loss != best->evaluation())
            throw std::runtime_error("selected scalar score does not match independent complete assessment");
    }

    double maxError = 0;
    std::size_t matches = 0, invalid = 0, operations = 0;
    for (const auto &row : held)
        if (best) {
            auto value = best->expression().evaluate(row.inputs);
            operations += value.arithmeticOperations();
            if (value.valid()) {
                auto error = std::abs(value.value() - row.target);
                maxError = std::max(maxError, error);
                matches += error <= 1e-6;
            } else
                ++invalid;
        }
    std::cout << "{\"task\":" << jsonString(task) << ",\"seed\":" << seed
              << ",\"constructed_training_loss\":" << direct.loss.score()
              << ",\"training_loss\":" << (best ? best->fitness() : -1)
              << ",\"canonical\":" << jsonString(best ? best->text() : "")
              << ",\"pretty\":" << jsonString(best ? formatExpression(best->expression()) : "")
              << ",\"nodes\":" << (best ? best->expression().nodeCount() : 0)
              << ",\"depth\":" << (best ? best->expression().depth() : 0)
              << ",\"generation\":" << *run.population().generation()
              << ",\"best_generation\":" << run.population().bestGeneration().value_or(0)
              << ",\"attempts\":" << run.work().lossAttempts << ",\"row_attempts\":" << run.work().rowAttempts
              << ",\"fitting_attempts\":" << run.work().fittingLossAttempts
              << ",\"invalid_losses\":" << run.work().invalidLosses << ",\"held_matches\":" << matches
              << ",\"held_count\":" << held.size() << ",\"held_invalid\":" << invalid
              << ",\"held_max_error\":" << maxError << ",\"held_operations\":" << operations
              << ",\"stop\":" << jsonString(stop) << "}";
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 6 || std::string_view(argv[4]) != "--threads")
            throw std::invalid_argument("usage: d5_mathematical_benchmarks TASK SEED OUTPUT --threads N");
        std::string task = argv[1];
        if (task != "gcd-lcm" && task != "fibonacci" && task != "powers" && task != "trig" &&
            task != "hyperbolic" && task != "polynomial" && task != "energy" && task != "piecewise")
            throw std::invalid_argument("unknown fixed benchmark");
        auto seed = static_cast<unsigned>(std::stoul(argv[2]));
        auto workers = static_cast<unsigned>(std::stoul(argv[5]));
        std::filesystem::path output = argv[3];
        std::filesystem::create_directories(output);
        auto start = std::chrono::steady_clock::now();
        std::cout.precision(17);
        if (task == "gcd-lcm" || task == "fibonacci")
            discrete(task, seed, workers, output);
        else
            scalar(task, seed, workers, output);
        std::cerr << "seconds "
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << '\n';
        std::cout << '\n';
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
