#include <ExpressionParsing.hpp>
#include <ScientificRegression.hpp>
#include <iostream>

int main() {
    using C = std::complex<double>;
    std::vector<ComplexObservation> rows;
    for (C z : {C{1, 0}, C{-1, 0}, C{0, 1}, C{0, -1}})
        rows.push_back({{z}, C{1, 2} * z + C{3, -1}});

    ComplexRegressionProblem problem(rows);
    auto initial = ComplexExpression::fromScalarGenome(parseExpression("0.5*x0+0.5", 1));
    ComplexCoefficientFitConfig config;
    config.lower = -10;
    config.upper = 10;
    config.optimizer.maxMathematicalOperations = 1'000'000;
    auto fitted = fitComplexCoefficients(problem, initial, config);
    if (fitted.trainingLoss.state() != FitnessState::Valid)
        return 1;
    std::cout << fitted.expression.format() << " training_mse=" << fitted.trainingLoss.score()
              << '\n';
}
