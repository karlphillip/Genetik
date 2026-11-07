#include <SymbolicRegression.hpp>

#include <iostream>
#include <vector>

int main() {
    // Each row contains one input followed by its measured target.
    const std::vector<RegressionRow> trainingRows{
        {{-1.0}, 1.0},
        {{0.0}, 1.0},
        {{1.0}, 3.0},
        {{2.0}, 7.0},
    };

    const auto problem = RegressionProblem::create(trainingRows, {15, 5, 8});

    SymbolicRegressionConfig config;
    config.population = 12;
    config.elites = 1;
    config.maxGenerationAttempts = 3;
    config.maxEvaluations = 36;
    config.maxTrainingLossAttempts = 144;

    // The explicit seed makes this short example repeatable on the same build.
    RandomContext random(42);
    const auto result = symbolicRegression(problem, config, random);

    std::cout << "stop=" << searchStopName(result.stoppingReason)
              << " loss_calls=" << result.trainingWork.lossAttempts << '\n';
    if (!result.best) {
        std::cerr << "No valid expression was produced within the configured search.\n";
        return 1;
    }

    std::cout << "best_expression=" << result.best->expression().toString()
              << " training_mse=" << result.best->fitness() << '\n';
    return 0;
}
