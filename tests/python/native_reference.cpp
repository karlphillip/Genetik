#include "RegressionFacade.hpp"
#include <iomanip>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    if (argc > 2) return 2;
    PythonRegressionConfig config;
    config.seed = 42;
    config.workers = argc == 2 ? static_cast<unsigned>(std::stoul(argv[1])) : 1U;
    config.evaluationBudget = 40;
    config.lossBudget = 256;
    config.population = 8;
    config.maxGenerations = 4;
    config.maxNodes = 15;
    config.maxDepth = 5;
    config.maxAbsConstant = 8.0;
    config.fitLossAttempts = 8;
    config.operators = {ExpressionKind::Add, ExpressionKind::Subtract, ExpressionKind::Multiply};
    std::vector<RegressionRow> rows{{{-1.0}, 1.0}, {{0.0}, 1.0}, {{1.0}, 3.0}, {{2.0}, 7.0}};
    PythonRegressionFacade model(config);
    model.fit(rows);
    const auto& stats = model.stats();
    std::cout << std::setprecision(17);
    std::cout << model.expression() << '\n'
              << stats.rngState << '\n';
    for (const auto& expression : stats.rawInitialExpressions) std::cout << expression << '\t';
    std::cout << '\n';
    for (const auto& expression : stats.preparedInitialExpressions) std::cout << expression << '\t';
    std::cout << '\n'
              << stats.lossAttempts << '\t' << stats.rowAttempts << '\t'
              << stats.fittingInvocations << '\t' << stats.fittingLossAttempts << '\t'
              << stats.candidateEvaluations << '\t' << stats.publishedGenerations << '\t'
              << stats.trainingMse.value_or(-1.0) << '\n';
    for (const auto& prediction : model.predict({{-1.0}, {0.0}, {1.0}, {2.0}}))
        std::cout << prediction << '\t';
    std::cout << '\n';
}
