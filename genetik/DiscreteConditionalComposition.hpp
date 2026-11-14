#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "DiscreteExpressionSpecification.hpp"

// Deterministically composes caller-supplied branch and predicate expressions.
// Agreement is only with the finite specification supplied to this invocation.
enum class DiscreteConditionalCompositionStatus {
    ExactTraining, Partial, Invalid, ResourceExhausted, Cancelled
};

struct DiscreteConditionalCompositionLimits {
    std::size_t maxCandidateCombinations = 100000;
    std::size_t maxNodes = 128;
    std::size_t maxDepth = 32;
    std::size_t maxEvaluationWork = 10000000;
    std::size_t maxEstimatedBytes = 64 * 1024 * 1024;
    DiscreteEvaluationLimits evaluation{};
};

struct DiscreteConditionalCompositionWork {
    std::size_t combinations = 0;
    std::size_t specificationComparisons = 0;
    std::size_t nodeVisits = 0;
    std::size_t operations = 0;
    std::size_t evaluatedRows = 0;
    std::size_t estimatedBytes = 0;
};

struct DiscreteConditionalCompositionResult {
    DiscreteConditionalCompositionStatus status = DiscreteConditionalCompositionStatus::Invalid;
    std::optional<DiscreteExpression> expression;
    std::size_t matchingRows = 0;
    std::size_t totalRows = 0;
    DiscreteConditionalCompositionWork work;
    std::string diagnostic;
};

namespace genetik_conditional_composition_detail {
inline bool addWithin(std::size_t a, std::size_t b, std::size_t cap) noexcept {
    return b <= cap && a <= cap - b;
}
inline bool multiplyWithin(std::size_t a, std::size_t b, std::size_t cap,
                           std::size_t& result) noexcept {
    if (b && a > cap / b) return false;
    result = a * b;
    return true;
}
}

inline DiscreteConditionalCompositionResult composeDiscreteConditional(
    const DiscreteSpecification& specification,
    std::span<const DiscreteExpression> predicates,
    std::span<const DiscreteExpression> branches,
    DiscreteConditionalCompositionLimits limits = {},
    const std::atomic<bool>* cancellation = nullptr) {
    using namespace genetik_conditional_composition_detail;
    DiscreteConditionalCompositionResult result;
    result.totalRows = specification.observations().size();
    const auto exhausted = [&]() {
        result.status = DiscreteConditionalCompositionStatus::ResourceExhausted;
        result.diagnostic = "conditional composition resource limit exhausted";
        return result;
    };
    const auto cancelled = [&]() {
        result.status = DiscreteConditionalCompositionStatus::Cancelled;
        result.diagnostic = "conditional composition cancelled";
        return result;
    };
    if (predicates.empty() || branches.empty() || !limits.maxCandidateCombinations ||
        !limits.maxNodes || !limits.maxDepth || !limits.maxEvaluationWork ||
        !limits.maxEstimatedBytes || result.totalRows == 0) {
        result.diagnostic = "invalid conditional composition inputs or limits";
        return result;
    }
    if (!std::all_of(specification.observations().begin(), specification.observations().end(),
                     [&](const auto& row) { return row.inputs.size() == specification.dimensions() &&
                         row.expected.type() == DiscreteType::Int64; })) {
        result.diagnostic = "conditional composition requires a valid Int64 specification";
        return result;
    }
    std::size_t inputBytes = sizeof(result);
    for (const auto& row : specification.observations()) {
        std::size_t payload = 0;
        if (!multiplyWithin(row.inputs.size(), sizeof(std::int64_t), limits.maxEstimatedBytes, payload) ||
            !addWithin(sizeof(row), payload, limits.maxEstimatedBytes)) return exhausted();
        const auto rowBytes = sizeof(row) + payload;
        if (!addWithin(inputBytes, rowBytes, limits.maxEstimatedBytes)) return exhausted();
        inputBytes += rowBytes;
    }
    for (const auto& expression : predicates) {
        std::size_t bytes = 0;
        if (!multiplyWithin(expression.nodeCount(), DiscreteExpression::storageBytesPerNode(),
                            limits.maxEstimatedBytes, bytes)) return exhausted();
        if (!addWithin(inputBytes, bytes, limits.maxEstimatedBytes)) return exhausted();
        inputBytes += bytes;
    }
    for (const auto& expression : branches) {
        std::size_t bytes = 0;
        if (!multiplyWithin(expression.nodeCount(), DiscreteExpression::storageBytesPerNode(),
                            limits.maxEstimatedBytes, bytes)) return exhausted();
        if (!addWithin(inputBytes, bytes, limits.maxEstimatedBytes)) return exhausted();
        inputBytes += bytes;
    }
    DiscreteSpecificationValidationLimits validationDefaults;
    const auto scratchBytes = std::max(limits.evaluation.maxWorkspaceBytes,
                                       validationDefaults.maxPeakRowWorkspaceBytes);
    if (!addWithin(inputBytes, scratchBytes, limits.maxEstimatedBytes)) return exhausted();
    inputBytes += scratchBytes;
    result.work.estimatedBytes = inputBytes;
    for (std::size_t i = 0; i < result.totalRows; ++i) {
        if (cancellation && cancellation->load(std::memory_order_relaxed)) return cancelled();
        for (std::size_t j = 0; j < i; ++j) {
            const auto& left = specification.observations()[i].inputs;
            const auto& right = specification.observations()[j].inputs;
            for (std::size_t k = 0; k < left.size(); ++k) {
                if (result.work.specificationComparisons == limits.maxEvaluationWork) return exhausted();
                ++result.work.specificationComparisons;
                if (left[k] != right[k]) break;
                if (k + 1 == left.size()) {
                    result.status = DiscreteConditionalCompositionStatus::Invalid;
                    result.diagnostic = "finite specification contains duplicate input rows";
                    return result;
                }
            }
            if (left.empty()) {
                if (result.work.specificationComparisons == limits.maxEvaluationWork) return exhausted();
                ++result.work.specificationComparisons;
                result.status = DiscreteConditionalCompositionStatus::Invalid;
                result.diagnostic = "finite specification contains duplicate input rows";
                return result;
            }
        }
    }
    std::size_t combinations = 0;
    if (predicates.size() > std::numeric_limits<std::size_t>::max() / branches.size() ||
        predicates.size() * branches.size() >
            std::numeric_limits<std::size_t>::max() / branches.size())
        return exhausted();
    combinations = predicates.size() * branches.size() * branches.size();
    if (combinations > limits.maxCandidateCombinations) return exhausted();
    result.work.combinations = combinations;

    std::optional<DiscreteExpression> best;
    std::size_t bestMatches = 0;
    std::size_t bestNodes = std::numeric_limits<std::size_t>::max();
    std::size_t bestPredicate = 0, bestTrue = 0, bestFalse = 0;
    bool structureExcluded = false;
    for (std::size_t p = 0; p < predicates.size(); ++p) {
        const auto& predicate = predicates[p];
        if (predicate.dimensions() != specification.dimensions() ||
            predicate.type() != DiscreteType::Bool || !predicate.outerBindings().empty()) {
            result.status = DiscreteConditionalCompositionStatus::Invalid;
            result.diagnostic = "predicate schema or type is invalid";
            return result;
        }
        for (std::size_t t = 0; t < branches.size(); ++t) {
            const auto& yes = branches[t];
            if (yes.dimensions() != specification.dimensions() ||
                yes.type() != DiscreteType::Int64 || !yes.outerBindings().empty()) {
                result.status = DiscreteConditionalCompositionStatus::Invalid;
                result.diagnostic = "branch schema or type is invalid";
                return result;
            }
            for (std::size_t f = 0; f < branches.size(); ++f) {
                if (cancellation && cancellation->load(std::memory_order_relaxed)) return cancelled();
                const auto& no = branches[f];
                if (predicate.nodeCount() > limits.maxNodes || yes.nodeCount() > limits.maxNodes ||
                    no.nodeCount() > limits.maxNodes || predicate.nodeCount() > limits.maxNodes - yes.nodeCount() ||
                    predicate.nodeCount() + yes.nodeCount() > limits.maxNodes - no.nodeCount() ||
                    predicate.nodeCount() + yes.nodeCount() + no.nodeCount() == limits.maxNodes) {
                    structureExcluded = true;
                    continue;
                }
                const auto nodes = predicate.nodeCount() + yes.nodeCount() + no.nodeCount() + 1;
                const auto depth = std::max({predicate.depth() + 1, yes.depth() + 1, no.depth() + 1});
                if (nodes > limits.maxNodes || depth > limits.maxDepth) { structureExcluded = true; continue; }
                std::size_t candidateBytes = 0;
                if (!multiplyWithin(nodes, DiscreteExpression::storageBytesPerNode(),
                                    limits.maxEstimatedBytes, candidateBytes)) return exhausted();
                if (!addWithin(candidateBytes, candidateBytes, limits.maxEstimatedBytes) ||
                    !addWithin(inputBytes, candidateBytes * 2, limits.maxEstimatedBytes)) return exhausted();
                result.work.estimatedBytes = std::max(result.work.estimatedBytes,
                                                      inputBytes + candidateBytes * 2);
                std::optional<DiscreteExpression> candidate;
                try { candidate.emplace(DiscreteExpression::conditional(predicate, yes, no)); }
                catch (const std::invalid_argument&) { continue; }
                catch (const std::length_error&) { continue; }
                std::size_t matches = 0;
                bool valid = true;
                for (const auto& row : specification.observations()) {
                    if (cancellation && cancellation->load(std::memory_order_relaxed)) return cancelled();
                    const auto workUsed = result.work.specificationComparisons +
                                          result.work.nodeVisits + result.work.operations;
                    if (workUsed >= limits.maxEvaluationWork) return exhausted();
                    auto evaluationLimits = limits.evaluation;
                    const auto remaining = limits.maxEvaluationWork - workUsed;
                    const auto visitAllowance = remaining / 2;
                    evaluationLimits.maxNodeVisits = std::min(evaluationLimits.maxNodeVisits, visitAllowance);
                    evaluationLimits.maxOperations = std::min(evaluationLimits.maxOperations,
                                                              remaining - visitAllowance);
                    auto evaluated = candidate->evaluate(row.inputs, evaluationLimits, cancellation);
                    const auto rowWork = evaluated.nodeVisits + evaluated.operations;
                    if (!addWithin(workUsed, rowWork, limits.maxEvaluationWork)) return exhausted();
                    result.work.nodeVisits += evaluated.nodeVisits;
                    result.work.operations += evaluated.operations;
                    ++result.work.evaluatedRows;
                    if (evaluated.status == DiscreteEvaluationStatus::Cancelled) return cancelled();
                    if (evaluated.status == DiscreteEvaluationStatus::ResourceExhausted) return exhausted();
                    if (!evaluated.valid()) { valid = false; break; }
                    matches += (*evaluated.value == row.expected) ? 1U : 0U;
                }
                if (!valid) continue;
                if (!best || matches > bestMatches ||
                    (matches == bestMatches && (nodes < bestNodes ||
                     (nodes == bestNodes && std::array{p,t,f} < std::array{bestPredicate,bestTrue,bestFalse})))) {
                    best = std::move(candidate); bestMatches = matches; bestNodes = nodes;
                    bestPredicate = p; bestTrue = t; bestFalse = f;
                }
            }
        }
    }
    result.matchingRows = bestMatches;
    result.expression = std::move(best);
    if (!result.expression) {
        result.status = structureExcluded ? DiscreteConditionalCompositionStatus::ResourceExhausted
                                          : DiscreteConditionalCompositionStatus::Invalid;
        result.diagnostic = structureExcluded ? "all possible compositions exceed structural limits"
                                              : "no mathematically valid conditional composition";
    } else if (bestMatches == result.totalRows) {
        DiscreteSpecificationValidationLimits validationLimits;
        validationLimits.maxRows = result.totalRows;
        const auto remaining = limits.maxEvaluationWork - result.work.specificationComparisons -
                               result.work.nodeVisits - result.work.operations;
        validationLimits.maxNodeVisits = remaining / 3;
        validationLimits.maxOperations = remaining / 3;
        validationLimits.maxSpecificationComparisons =
            remaining - validationLimits.maxNodeVisits - validationLimits.maxOperations;
        validationLimits.maxPeakRowWorkspaceBytes =
            std::min(validationLimits.maxPeakRowWorkspaceBytes, scratchBytes);
        validationLimits.maxRetainedBytes = limits.maxEstimatedBytes - result.work.estimatedBytes;
        validationLimits.evaluation = limits.evaluation;
        auto validation = validateDiscreteSpecification(specification, *result.expression, validationLimits,
                                                        cancellation);
        result.work.estimatedBytes += validation.work.retainedBytesEstimate;
        result.work.nodeVisits += validation.work.nodeVisits;
        result.work.operations += validation.work.operations;
        result.work.specificationComparisons += validation.work.specificationComparisons;
        if (validation.status == DiscreteSpecificationValidationStatus::InconclusiveCancelled) return cancelled();
        if (validation.status == DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted)
            return exhausted();
        if (validation.status != DiscreteSpecificationValidationStatus::ValidatedFiniteDomain) {
            result.status = DiscreteConditionalCompositionStatus::Invalid;
            result.diagnostic = "finite-specification validator rejected the selected expression";
            return result;
        }
        result.status = DiscreteConditionalCompositionStatus::ExactTraining;
        result.diagnostic = "exact agreement on the finite training specification";
    } else {
        result.status = DiscreteConditionalCompositionStatus::Partial;
        result.diagnostic = "best valid composition only partially agrees with the finite training specification";
    }
    return result;
}
