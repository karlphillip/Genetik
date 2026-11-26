#include <SymbolicRegression.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
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
    config.maximumParetoObjectives = 2;
    config.paretoObjectives = [](const Expression& expression, const RegressionProblem& training) {
        double squared = 0.0, maximumAbsolute = 0.0;
        for (const auto& row : training.trainingRows()) {
            const auto prediction = expression.evaluate(row.inputs);
            if (!prediction.valid())
                throw std::domain_error("valid Pareto candidate became invalid during assessment");
            const double residual = prediction.value() - row.target;
            squared += residual * residual;
            maximumAbsolute = std::max(maximumAbsolute, std::abs(residual));
        }
        return std::vector<double>{squared / training.trainingRows().size(), maximumAbsolute};
    };
    RandomContext random(2026);
    const auto result = symbolicRegression(problem, config, random);

    std::cout << "stop=" << searchStopName(result.stoppingReason)
              << " candidates=" << result.candidateEvaluations
              << " dominance_comparisons=" << result.paretoWork.dominanceComparisons
              << " selection_comparisons=" << result.paretoWork.selectionComparisons
              << " vector_objective_evaluations=" << result.paretoWork.objectiveVectorEvaluations
              << " estimated_selection_bytes=" << result.paretoWork.peakEstimatedMemoryBytes
              << '\n';
    for (const auto& point : result.paretoFrontier) {
        std::cout << point.candidate.text() << " mse=" << point.predictionLoss
                  << " nodes=" << point.structuralComplexity << " objectives=";
        for (const auto objective : point.objectives)
            std::cout << objective << ',';
        std::cout << '\n';
    }
    return 0;
}
