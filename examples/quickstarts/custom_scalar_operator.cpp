#include <SymbolicRegression.hpp>
#include <cmath>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

int main() {
    CustomScalarOperator fma{"example.fma/1", 3,
                             [](std::span<const double> x) -> std::optional<double> {
                                 return std::fma(x[0], x[1], x[2]);
                             },
                             [](std::span<const double> x) -> std::optional<std::vector<double>> {
                                 return std::vector<double>{x[1], x[0], 1.0};
                             }};
    const std::vector<RegressionRow> training{{{2, 3, 4}, 10}, {{1, 4, 2}, 6}};
    auto problem = RegressionProblem::create(training, {15, 5, 8});
    SymbolicRegressionConfig config;
    config.population = 32;
    config.maxEvaluations = 32;
    config.maxGenerationAttempts = 0;
    config.grammar.operators.clear();
    config.grammar.customOperators.push_back(fma);
    config.grammar.initialMaxDepth = 2;
    config.grammar.constants = {0};
    RandomContext random(29);
    const auto result = symbolicRegression(problem, config, random);
    if (!result.best)
        return 1;
    std::cout << result.best->text() << " -> " << result.best->fitness() << '\n';
}
