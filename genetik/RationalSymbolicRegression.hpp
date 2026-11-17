#pragma once

// Application-level bridge: the symbolic driver sees only an initial-expression
// provider; the generic GA and its variation policy know nothing about rational fits.
#include "RationalFunctionRegression.hpp"
#include "SymbolicRegression.hpp"

inline std::uint64_t rationalInitializerWorkUnits(const RationalFitResult& fit) {
    const auto linear = genetik_rational_detail::combinedLinearAlgebraWork(fit);
    const auto arithmetic = fit.arithmeticOperations + fit.lmWork.mathematicalOperations;
    return std::max<std::uint64_t>(linear, arithmetic);
}
inline InitialExpressionBatch rationalInitialExpressions(const RationalFitResult& fit,
                                                         std::size_t maximumExpressions) {
    InitialExpressionBatch batch;
    batch.status = rationalStatusName(fit.status);
    batch.detail = fit.detail;
    batch.workUnits = rationalInitializerWorkUnits(fit);
    if (fit.status != RationalStatus::Success || !fit.selected)
        return batch;
    std::vector<const RationalModel*> ranked;
    ranked.reserve(fit.models.size());
    for (const auto& model : fit.models)
        ranked.push_back(&model);
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto* a, const auto* b) {
        if (a->trainingSSE != b->trainingSSE)
            return a->trainingSSE < b->trainingSSE;
        return a->expression.nodeCount() < b->expression.nodeCount();
    });
    for (const auto* model : ranked) {
        if (batch.expressions.size() == maximumExpressions)
            break;
        if (std::none_of(batch.expressions.begin(), batch.expressions.end(), [&](const auto& e) {
                return genetik_rational_detail::sameNodes(e, model->expression);
            }))
            batch.expressions.push_back(model->expression);
    }
    return batch;
}
inline InitialExpressionProvider rationalInitialExpressionProvider(RationalConfig config = {}) {
    return [config = std::move(config)](const RegressionProblem& problem,
                                        std::size_t maximumExpressions,
                                        std::uint64_t maximumWorkUnits) {
        if (problem.dimensions() != 1 ||
            problem.objective().loss != RegressionLoss::MeanSquaredError)
            throw std::invalid_argument(
                "rational initializer requires one-input mean-squared-error training data");
        if (!maximumExpressions || !maximumWorkUnits)
            throw std::invalid_argument(
                "rational initializer requires positive candidate and work budgets");
        auto localConfig = config;
        localConfig.expressionLimits = problem.limits();
        localConfig.maximumLinearAlgebraOperations =
            std::min<std::size_t>(localConfig.maximumLinearAlgebraOperations, maximumWorkUnits);
        localConfig.maximumArithmeticOperations =
            std::min<std::size_t>(localConfig.maximumArithmeticOperations, maximumWorkUnits);
        const auto rows = problem.trainingRows();
        std::vector<RationalObservation> observations;
        observations.reserve(rows.size());
        for (const auto& row : rows)
            observations.push_back({row.inputs[0], row.target});
        const auto fit = fitRationalFunction(observations, localConfig);
        auto batch = rationalInitialExpressions(fit, maximumExpressions);
        if (batch.workUnits > maximumWorkUnits)
            throw std::runtime_error("rational initializer exhausted its declared work allowance");
        return batch;
    };
}
