#include <AutomaticSeparableRegression.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

int main() {
    // Generate a complete rectangular training grid. The fitter receives only
    // these observations and the allowed component grammars.
    std::vector<RegressionRow> training;
    for (const double x : {-1.0, -0.5, 0.0, 0.5, 1.0})
        for (const double y : {-1.0, -0.25, 0.5, 1.0})
            training.push_back({{x, y}, std::sin(x) + y * y});

    AutomaticSeparableConfig config;
    config.grammars[0].operators = {ExpressionKind::Sin, ExpressionKind::Add,
                                    ExpressionKind::Multiply};
    config.grammars[0].constants = {-1.0, 0.0, 1.0};
    config.grammars[1].operators = {ExpressionKind::Add, ExpressionKind::Multiply,
                                    ExpressionKind::Powi};
    config.grammars[1].constants = {-1.0, 0.0, 1.0};
    config.grammars[1].exponents = {2};
    config.grammars[0].initialMaxDepth = config.grammars[1].initialMaxDepth = 2;
    config.grammars[0].mutationMaxDepth = config.grammars[1].mutationMaxDepth = 2;
    config.search.population = 48;
    config.search.elites = 2;
    config.search.maxGenerationAttempts = 12;
    config.search.maxEvaluations = 624;
    config.search.maxTrainingLossAttempts = 624 * training.size();

    RandomContext random(20261108);
    const auto result = automaticSeparableRegression(training, config, random);
    if (!result.search || !result.search->best) {
        std::cout << "no unique supported decomposition: " << result.detection.diagnostic << '\n';
        return 1;
    }

    // Keep assessment observations out of detection and search.
    const std::vector<std::array<double, 2>> heldOut{
        {{-0.75, 0.75}}, {{0.25, -0.75}}, {{1.25, 0.5}}};
    double squaredError = 0.0;
    bool valid = true;
    for (const auto& input : heldOut) {
        const auto prediction = result.search->best->expression().evaluate(input);
        const double reference = std::sin(input[0]) + input[1] * input[1];
        if (!prediction.valid()) {
            valid = false;
            break;
        }
        const double error = prediction.value() - reference;
        squaredError += error * error;
    }

    std::cout << "detected="
              << (*result.detection.selectedForm == SeparableForm::Additive ? "additive"
                                                                            : "multiplicative")
              << " model=" << result.search->best->text() << '\n'
              << "training_mse=" << result.search->best->fitness() << " heldout_rmse="
              << (valid ? std::sqrt(squaredError / heldOut.size())
                        : std::numeric_limits<double>::infinity())
              << " valid=" << (valid ? "yes" : "no") << '\n'
              << "detection_ops=" << result.detection.work.mathematicalOperations
              << " candidates=" << result.search->candidateEvaluations
              << " training_rows=" << result.search->trainingWork.rowAttempts << '\n';
    return valid ? 0 : 1;
}
