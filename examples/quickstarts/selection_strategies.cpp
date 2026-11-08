#include <SymbolicRegression.hpp>

#include <cmath>
#include <iostream>
#include <vector>

int main() {
    std::vector<RegressionRow> rows;
    for (int point = -4; point <= 4; ++point) {
        const double x = static_cast<double>(point) / 2.0;
        rows.push_back({{x}, x * x});
    }
    auto problem = RegressionProblem::create(rows, {31, 6, 4});

    SymbolicRegressionConfig config;
    config.population = 48;
    config.elites = 2;
    config.maxGenerationAttempts = 40;
    config.maxEvaluations = 1968;
    config.grammar.operators = {ExpressionKind::Add, ExpressionKind::Subtract,
                                ExpressionKind::Multiply};
    config.selectionStrategy = SymbolicRegressionConfig::SelectionStrategy::Pareto;
    config.maximumParetoFrontier = 8;
    RandomContext random(2026);
    const auto result = symbolicRegression(problem, config, random);

    std::cout << "stop=" << searchStopName(result.stoppingReason)
              << " candidates=" << result.candidateEvaluations
              << " dominance_comparisons=" << result.paretoWork.dominanceComparisons
              << " selection_comparisons=" << result.paretoWork.selectionComparisons
              << " estimated_selection_bytes=" << result.paretoWork.peakEstimatedMemoryBytes
              << '\n';
    for (const auto& point : result.paretoFrontier) {
        std::cout << point.candidate.text() << " mse=" << point.predictionLoss
                  << " nodes=" << point.structuralComplexity << '\n';
    }
    return 0;
}
