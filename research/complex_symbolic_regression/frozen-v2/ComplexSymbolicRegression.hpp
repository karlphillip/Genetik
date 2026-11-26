#pragma once

#include "ScientificRegression.hpp"
#include "SymbolicRegression.hpp"

#include <algorithm>
#include <memory>
#include <limits>
#include <stdexcept>

// Autonomous structure search uses the existing real-coefficient genome and
// complex-interpreted regression objective. After structure selection, the
// chosen topology is converted to ComplexExpression and every complex
// coefficient's real and imaginary coordinates are refined independently.
struct ComplexSymbolicRegressionResult {
    SymbolicRegressionResult structureSearch;
    std::optional<ComplexCoefficientFitResult> coefficientFit;
    std::optional<ComplexExpression> fittedExpression;
    std::optional<Expression> selectedTopology;
    std::size_t fittedTopologies = 0;
    std::size_t coefficientFitObjectiveEvaluations = 0;
};

inline ComplexSymbolicRegressionResult
complexSymbolicRegression(std::shared_ptr<const ComplexRegressionProblem> problem,
                          const SymbolicRegressionConfig& config, RandomContext& random,
                          const ComplexCoefficientFitConfig& fitConfig = {}) {
    if (!problem)
        throw std::invalid_argument("complex symbolic regression problem required");
    if (config.fitting)
        throw std::invalid_argument(
            "complex symbolic regression uses fitComplexCoefficients after structure selection");
    ComplexSymbolicRegressionResult result;
    result.structureSearch = symbolicRegression(problem, config, random);
    if (!result.structureSearch.best)
        return result;
    std::vector<Expression> topologies{result.structureSearch.best->expression()};
    for (const auto& point : result.structureSearch.paretoFrontier)
        if (std::none_of(topologies.begin(), topologies.end(), [&](const Expression& e) {
                return e.sameRepresentation(point.candidate.expression());
            }))
            topologies.push_back(point.candidate.expression());
    for (const auto& topology : topologies) {
        auto initial = ComplexExpression::fromScalarGenome(topology);
        if (std::none_of(initial.nodes().begin(), initial.nodes().end(),
                         [](const ComplexNode& node) { return node.kind == ComplexKind::Constant; }))
            continue;
        auto fit = fitComplexCoefficients(*problem, initial, fitConfig);
        ++result.fittedTopologies;
        if (fit.objectiveEvaluations > std::numeric_limits<std::size_t>::max() -
                                           result.coefficientFitObjectiveEvaluations)
            throw std::overflow_error("complex symbolic fit evaluation counter overflow");
        result.coefficientFitObjectiveEvaluations += fit.objectiveEvaluations;
        if (fit.trainingLoss.state() != FitnessState::Valid)
            continue;
        if (!result.coefficientFit ||
            fit.trainingLoss.score() < result.coefficientFit->trainingLoss.score() ||
            (fit.trainingLoss.score() == result.coefficientFit->trainingLoss.score() &&
             topology.toString() < result.selectedTopology->toString())) {
            result.selectedTopology = topology;
            result.fittedExpression = fit.expression;
            result.coefficientFit = std::move(fit);
        }
    }
    return result;
}
