#include <SymbolicRegression.hpp>

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {
using K = ExpressionKind;

std::vector<RegressionRow> rows(std::span<const double> xs) {
    std::vector<RegressionRow> out;
    out.reserve(xs.size());
    for (double x : xs)
        out.push_back({{x}, 2 * x + 3});
    return out;
}

ConstantFitConfig ordinaryLm() {
    ConstantFitConfig f;
    f.method = ConstantFitMethod::LevenbergMarquardt;
    f.lower = -8;
    f.upper = 8;
    f.maxLossAttempts = 512;
    f.rankAware = false;
    f.rankTolerance = 1e-10;
    f.levenbergMarquardt.maxIterations = 64;
    f.levenbergMarquardt.maxObjectiveEvaluations = 128;
    f.levenbergMarquardt.maxJacobianEvaluations = 128;
    f.levenbergMarquardt.maxMathematicalOperations = 1000000;
    f.levenbergMarquardt.lossTolerance = 1e-24;
    return f;
}

double mse(const RegressionProblem& p, const Expression& e) {
    return p.measure(e).loss.score();
}

std::string csv(std::string_view s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '"';
        out += c;
    }
    out += '"';
    return out;
}
}

int main(int argc, char** argv) {
    if (argc != 4)
        return 2;
    const auto seed = static_cast<std::uint32_t>(std::stoul(argv[1]));
    const auto initialDepth = static_cast<std::size_t>(std::stoul(argv[2]));
    const auto mutationDepth = static_cast<std::size_t>(std::stoul(argv[3]));

    const std::array trainX{-2., -1., 0., 1., 2.};
    const auto training = RegressionProblem::create(rows(trainX), {32, 8, 8});
    SymbolicRegressionConfig config;
    config.population = 10;
    config.elites = 1;
    config.maxGenerationAttempts = 4;
    config.maxEvaluations = 50;
    config.maxTrainingLossAttempts = 50000;
    config.grammar.operators = {K::Add, K::Multiply};
    config.grammar.continuousProbability = 1;
    config.grammar.continuousLower = -2;
    config.grammar.continuousUpper = 2;
    config.grammar.initialMaxDepth = initialDepth;
    config.grammar.mutationMaxDepth = mutationDepth;
    config.grammar.proposalLimit = 16;
    config.fitting = ordinaryLm();
    config.trainingLossThreshold = 0;
    config.selectionStrategy = SymbolicRegressionConfig::SelectionStrategy::PredictionLoss;

    RandomContext random(seed);
    const auto start = std::chrono::steady_clock::now();
    const auto result = symbolicRegression(training, config, random);
    const auto stop = std::chrono::steady_clock::now();
    const double elapsedSeconds = std::chrono::duration<double>(stop - start).count();

    // Held-out data are created and first observed only after the search has returned.
    const std::array heldX{-.5, 2.5, 4.};
    const auto heldout = RegressionProblem::create(rows(heldX), {32, 8, 8});
    if (!result.best || result.best->fitnessState() != FitnessState::Valid)
        throw std::runtime_error("search returned no valid selected expression");
    const double heldMse = mse(*heldout, result.best->expression());
    std::cout << std::setprecision(17)
              << "seed," << seed << ",initial_depth," << initialDepth
              << ",mutation_depth," << mutationDepth
              << ",candidate_evaluations," << result.candidateEvaluations
              << ",train_mse," << result.best->fitness()
              << ",heldout_mse," << heldMse
              << ",numerical_recovery," << (heldMse <= 1e-12 ? 1 : 0)
              << ",objective_calls," << result.trainingWork.lossAttempts
              << ",fitting_loss_calls," << result.trainingWork.fittingLossAttempts
              << ",lm_math_ops," << result.optimizerWork.mathematicalOperations
              << ",lm_linear_ops," << result.optimizerWork.linearAlgebraOperations
              << ",lm_objective_evals," << result.optimizerWork.objectiveEvaluations
              << ",lm_jacobian_evals," << result.optimizerWork.jacobianEvaluations
              << ",rank_selector_evals," << result.rankSelectionWork.derivativeEvaluations
              << ",search_elapsed_seconds," << elapsedSeconds
              << ",stop," << searchStopName(result.stoppingReason)
              << ",equation," << csv(result.best->text()) << '\n';
}
