#pragma once

#include "Regression.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

enum class InformationCriterion { Akaike, Bayesian };
enum class InformationCriterionStatus {
    Success,
    NoCandidates,
    UnsupportedObjective,
    InvalidModel,
    InvalidPrediction,
    InsufficientObservations,
    ZeroResidualVariance,
    ResourceExhausted,
    NumericalFailure
};

struct InformationCriterionCandidate {
    Expression expression;
    // Number of independently estimated parameters in this model. Constants
    // that were fixed or sampled but never fitted do not contribute here.
    std::size_t fittedParameterCount = 0;
};

struct InformationCriterionConfig {
    std::size_t maximumCandidates = 256;
    std::uint64_t maximumRowEvaluations = 1'000'000;
    std::uint64_t maximumExpressionOperations = 100'000'000;
    std::size_t maximumEstimatedMemoryBytes = 64 * 1024 * 1024;
};

struct InformationCriterionCandidateResult {
    InformationCriterionStatus status = InformationCriterionStatus::InvalidModel;
    std::optional<double> mse;
    std::optional<double> criterion;
    std::size_t fittedParameterCount = 0;
    std::size_t likelihoodParameterCount = 0;
    std::size_t structuralComplexity = 0;
};

struct InformationCriterionWork {
    std::uint64_t candidatesEvaluated = 0;
    std::uint64_t rowEvaluations = 0;
    std::uint64_t expressionOperations = 0;
    std::size_t peakEstimatedMemoryBytes = 0;
};

struct InformationCriterionResult {
    InformationCriterion criterion = InformationCriterion::Akaike;
    InformationCriterionStatus status = InformationCriterionStatus::NoCandidates;
    std::optional<std::size_t> selectedIndex;
    std::vector<InformationCriterionCandidateResult> candidates;
    InformationCriterionWork work;
};

inline InformationCriterionResult selectInformationCriterion(
    const RegressionProblem& problem, std::span<const InformationCriterionCandidate> candidates,
    InformationCriterion criterion, const InformationCriterionConfig& config = {}) {
    if (criterion != InformationCriterion::Akaike && criterion != InformationCriterion::Bayesian)
        throw std::invalid_argument("unknown information criterion");
    if (!config.maximumCandidates || !config.maximumRowEvaluations ||
        !config.maximumExpressionOperations || !config.maximumEstimatedMemoryBytes)
        throw std::invalid_argument("information-criterion limits must be positive");

    InformationCriterionResult result;
    result.criterion = criterion;
    if (candidates.empty()) {
        result.status = InformationCriterionStatus::NoCandidates;
        return result;
    }
    if (candidates.size() > config.maximumCandidates ||
        candidates.size() >
            std::numeric_limits<std::size_t>::max() / sizeof(InformationCriterionCandidateResult)) {
        result.status = InformationCriterionStatus::ResourceExhausted;
        return result;
    }
    if (problem.objective().loss != RegressionLoss::MeanSquaredError ||
        typeid(problem) != typeid(RegressionProblem) || problem.trainingRows().empty()) {
        result.status = InformationCriterionStatus::UnsupportedObjective;
        return result;
    }
    const auto outputBytes = candidates.size() * sizeof(InformationCriterionCandidateResult);
    if (outputBytes > config.maximumEstimatedMemoryBytes) {
        result.status = InformationCriterionStatus::ResourceExhausted;
        return result;
    }
    result.candidates.reserve(candidates.size());
    result.work.peakEstimatedMemoryBytes = outputBytes;
    bool sawInsufficient = false;
    bool sawZeroVariance = false;
    bool sawInvalid = false;
    bool sawInvalidPrediction = false;

    for (const auto& candidate : candidates) {
        InformationCriterionCandidateResult candidateResult;
        candidateResult.fittedParameterCount = candidate.fittedParameterCount;
        candidateResult.structuralComplexity = candidate.expression.nodeCount();
        result.candidates.push_back(candidateResult);
        auto& scored = result.candidates.back();

        const auto inputNodes = candidate.expression.nodeCount();
        constexpr std::size_t bytesPerNode = 3 * sizeof(ExpressionNode) + 4 * sizeof(double);
        if (inputNodes > (std::numeric_limits<std::size_t>::max() - outputBytes) / bytesPerNode) {
            scored.status = InformationCriterionStatus::ResourceExhausted;
            result.status = InformationCriterionStatus::ResourceExhausted;
            result.selectedIndex.reset();
            return result;
        }
        const auto estimatedBytes = outputBytes + inputNodes * bytesPerNode;
        result.work.peakEstimatedMemoryBytes =
            std::max(result.work.peakEstimatedMemoryBytes, estimatedBytes);
        if (estimatedBytes > config.maximumEstimatedMemoryBytes) {
            scored.status = InformationCriterionStatus::ResourceExhausted;
            result.status = InformationCriterionStatus::ResourceExhausted;
            result.selectedIndex.reset();
            return result;
        }

        std::optional<Expression> checked;
        try {
            checked.emplace(problem.checkedExpression(candidate.expression));
        } catch (const std::invalid_argument&) {
            sawInvalid = true;
            scored.status = InformationCriterionStatus::InvalidModel;
            continue;
        } catch (const std::length_error&) {
            sawInvalid = true;
            scored.status = InformationCriterionStatus::InvalidModel;
            continue;
        }
        const auto checkedNodes = checked->nodes();
        const auto constants = static_cast<std::size_t>(
            std::count_if(checkedNodes.begin(), checkedNodes.end(), [](const ExpressionNode& node) {
                return node.kind() == ExpressionKind::Constant;
            }));
        if (candidate.fittedParameterCount > constants ||
            candidate.fittedParameterCount == std::numeric_limits<std::size_t>::max()) {
            scored.status = InformationCriterionStatus::InvalidModel;
            sawInvalid = true;
            continue;
        }
        scored.likelihoodParameterCount = candidate.fittedParameterCount + 1; // fitted variance
        const auto observations = problem.trainingRows().size();
        if (observations <= scored.likelihoodParameterCount) {
            scored.status = InformationCriterionStatus::InsufficientObservations;
            sawInsufficient = true;
            continue;
        }

        if (observations > config.maximumRowEvaluations - result.work.rowEvaluations ||
            checked->arithmeticCost() > std::numeric_limits<std::uint64_t>::max() / observations) {
            scored.status = InformationCriterionStatus::ResourceExhausted;
            result.status = InformationCriterionStatus::ResourceExhausted;
            result.selectedIndex.reset();
            return result;
        }
        const auto maximumCandidateOperations =
            static_cast<std::uint64_t>(checked->arithmeticCost()) *
            static_cast<std::uint64_t>(observations);
        if (maximumCandidateOperations >
            config.maximumExpressionOperations - result.work.expressionOperations) {
            scored.status = InformationCriterionStatus::ResourceExhausted;
            result.status = InformationCriterionStatus::ResourceExhausted;
            result.selectedIndex.reset();
            return result;
        }

        ++result.work.candidatesEvaluated;
        double squaredError = 0.0;
        bool valid = true;
        genetik_detail::EvaluationWorkspace workspace;
        for (const auto& row : problem.trainingRows()) {
            ++result.work.rowEvaluations;
            const auto prediction = workspace.evaluate(*checked, row.inputs);
            result.work.expressionOperations += prediction.arithmeticOperations();
            if (!prediction.valid()) {
                valid = false;
                break;
            }
            volatile double residual = prediction.value() - row.target;
            if (!std::isfinite(residual)) {
                valid = false;
                break;
            }
            volatile double square = residual * residual;
            if (!std::isfinite(square)) {
                valid = false;
                break;
            }
            volatile double next = squaredError + square;
            if (!std::isfinite(next)) {
                valid = false;
                break;
            }
            squaredError = next;
        }
        if (!valid) {
            scored.status = InformationCriterionStatus::InvalidPrediction;
            sawInvalidPrediction = true;
            continue;
        }
        const double mse = squaredError / static_cast<double>(observations);
        if (!std::isfinite(mse)) {
            scored.status = InformationCriterionStatus::NumericalFailure;
            continue;
        }
        scored.mse = mse;
        if (mse == 0.0) {
            scored.status = InformationCriterionStatus::ZeroResidualVariance;
            sawZeroVariance = true;
            continue;
        }
        const double n = static_cast<double>(observations);
        const double logLikelihood =
            -0.5 * n * (std::log(2.0 * std::acos(-1.0)) + 1.0 + std::log(mse));
        const double parameterPenalty =
            (criterion == InformationCriterion::Akaike ? 2.0 : std::log(n)) *
            static_cast<double>(scored.likelihoodParameterCount);
        const double value = parameterPenalty - 2.0 * logLikelihood;
        if (!std::isfinite(value)) {
            scored.status = InformationCriterionStatus::NumericalFailure;
            continue;
        }
        scored.status = InformationCriterionStatus::Success;
        scored.criterion = value;
        if (!result.selectedIndex) {
            result.selectedIndex = result.candidates.size() - 1;
        } else {
            const auto& incumbent = result.candidates[*result.selectedIndex];
            if (value < *incumbent.criterion ||
                (value == *incumbent.criterion &&
                 scored.structuralComplexity < incumbent.structuralComplexity))
                result.selectedIndex = result.candidates.size() - 1;
        }
    }

    if (result.selectedIndex) {
        result.status = InformationCriterionStatus::Success;
    } else if (sawInsufficient) {
        result.status = InformationCriterionStatus::InsufficientObservations;
    } else if (sawZeroVariance) {
        result.status = InformationCriterionStatus::ZeroResidualVariance;
    } else if (sawInvalidPrediction) {
        result.status = InformationCriterionStatus::InvalidPrediction;
    } else if (sawInvalid) {
        result.status = InformationCriterionStatus::InvalidModel;
    } else {
        result.status = InformationCriterionStatus::NumericalFailure;
    }
    return result;
}
