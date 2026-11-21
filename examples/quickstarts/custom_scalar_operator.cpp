#include <SymbolicRegression.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

int main() {
    CustomScalarOperator logaddexp{
        "example.logaddexp/1", 2,
        [](std::span<const double> x) -> std::optional<double> {
            const double m = std::max(x[0], x[1]);
            return m + std::log1p(std::exp(-std::abs(x[0] - x[1])));
        },
        [](std::span<const double> x) -> std::optional<std::vector<double>> {
            const double difference = x[0] - x[1];
            const double small = std::exp(-std::abs(difference));
            if (difference >= 0.0) {
                const double denominator = 1.0 + small;
                return std::vector<double>{1.0 / denominator, small / denominator};
            }
            const double denominator = 1.0 + small;
            return std::vector<double>{small / denominator, 1.0 / denominator};
        }};
    const std::vector<RegressionRow> training{{{0, 0}, std::log(2.0)},
                                              {{1, 0}, 1.3132616875182228}};
    auto problem = RegressionProblem::create(training, {15, 5, 8});
    SymbolicRegressionConfig config;
    config.population = 32;
    config.maxEvaluations = 32;
    config.maxGenerationAttempts = 0;
    config.grammar.operators.clear();
    config.grammar.customOperators.push_back(logaddexp);
    config.grammar.initialMaxDepth = 2;
    config.grammar.constants = {0};
    RandomContext random(29);
    const auto result = symbolicRegression(problem, config, random);
    if (!result.best)
        return 1;
    std::cout << result.best->text() << '\n';
}
