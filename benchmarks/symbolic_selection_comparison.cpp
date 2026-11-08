#include <SymbolicRegression.hpp>

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct Case {
    std::string name;
    bool quadratic;
    bool noisy;
};

std::vector<double> frozenNoise() {
    return {0.08, -0.04, 0.03, -0.07, 0.02, 0.05, -0.06, 0.01, -0.02};
}

double reference(double x, bool quadratic) {
    return quadratic ? x * x + x : 2.0 * x + 1.0;
}

std::vector<RegressionRow> training(const Case& testCase) {
    const auto noise = frozenNoise();
    std::vector<RegressionRow> rows;
    for (std::size_t index = 0; index < noise.size(); ++index) {
        const double x = -1.0 + 0.25 * static_cast<double>(index);
        rows.push_back(
            {{x}, reference(x, testCase.quadratic) + (testCase.noisy ? noise[index] : 0.0)});
    }
    return rows;
}

std::vector<RegressionRow> assessment(const Case& testCase) {
    const std::vector<double> points{-1.25, -0.625, 0.125, 0.625, 1.25};
    const std::vector<double> independentNoise{0.03, -0.02, 0.04, -0.01, 0.02};
    std::vector<RegressionRow> rows;
    for (std::size_t index = 0; index < points.size(); ++index)
        rows.push_back({{points[index]},
                        reference(points[index], testCase.quadratic) +
                            (testCase.noisy ? independentNoise[index] : 0.0)});
    return rows;
}

void run(const Case& testCase, SymbolicRegressionConfig::SelectionStrategy strategy,
         const char* strategyName) {
    const auto rows = training(testCase);
    const auto heldOut = assessment(testCase);
    auto problem = RegressionProblem::create(rows, {63, 8, 4});
    auto assessmentProblem = RegressionProblem::create(heldOut, {63, 8, 4});
    SymbolicRegressionConfig config;
    config.population = 48;
    config.elites = 2;
    config.maxGenerationAttempts = 24;
    config.maxEvaluations = 1200;
    config.maxTrainingLossAttempts = 1200;
    config.grammar.operators = {ExpressionKind::Add, ExpressionKind::Subtract,
                                ExpressionKind::Multiply};
    config.selectionStrategy = strategy;
    config.complexityPenalty = 0.01;
    config.maximumParetoComparisons = 3'000'000;
    config.maximumParetoEstimatedMemoryBytes = 32 * 1024 * 1024;
    config.maximumParetoFrontier = 16;
    RandomContext random(104729);
    const auto begin = std::chrono::steady_clock::now();
    const auto result = symbolicRegression(problem, config, random);
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - begin)
                             .count();
    std::cout << testCase.name << ',' << strategyName << ','
              << searchStopName(result.stoppingReason) << ','
              << (result.best ? result.best->text() : "") << ','
              << (result.best ? result.best->fitness() : -1.0) << ','
              << (result.best ? result.best->expression().nodeCount() : 0) << ','
              << (result.best ? assessmentProblem->measure(result.best->expression()).loss.score()
                              : -1.0)
              << ',' << result.candidateEvaluations << ',' << result.trainingWork.rowAttempts << ','
              << result.publishedGenerations << ','
              << (result.paretoWork.dominanceComparisons + result.paretoWork.selectionComparisons)
              << ',' << result.paretoWork.peakEstimatedMemoryBytes << ',' << elapsed << '\n';
}
} // namespace

int main() {
    std::cout << "case,strategy,stop,expression,training_mse,nodes,heldout_mse,candidates,"
                 "training_rows,generations,pareto_comparisons,pareto_peak_estimated_bytes,"
                 "elapsed_us\n";
    for (const Case& testCase :
         {Case{"exact_linear", false, false}, Case{"exact_quadratic", true, false},
          Case{"noisy_linear", false, true}, Case{"noisy_quadratic", true, true}}) {
        run(testCase, SymbolicRegressionConfig::SelectionStrategy::PredictionLoss, "loss_only");
        run(testCase, SymbolicRegressionConfig::SelectionStrategy::ComplexityPenalty,
            "complexity_penalty");
        run(testCase, SymbolicRegressionConfig::SelectionStrategy::Pareto, "pareto");
    }
}
