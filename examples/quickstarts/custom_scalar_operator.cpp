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
            const double p = 1.0 / (1.0 + std::exp(-(x[0] - x[1])));
            return std::vector<double>{p, 1.0 - p};
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
