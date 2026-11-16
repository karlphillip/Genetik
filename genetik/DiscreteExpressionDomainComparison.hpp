#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "DiscreteExpression.hpp"

// One inclusive coordinate range in the finite integer box searched by
// compareDiscreteExpressionsOnBox(). Coordinates advance in lexicographic
// order, with the final coordinate changing fastest.
struct DiscreteIntegerDomainRange {
    std::int64_t lower = 0;
    std::int64_t upper = 0;
};

enum class DiscreteExpressionDomainComparisonStatus {
    EquivalentOnBoundedDomain,
    DisagreementFound,
    InconclusiveResourceExhausted,
    InconclusiveCancelled,
    InvalidConfiguration
};

struct DiscreteExpressionDomainComparisonLimits {
    std::size_t maxDomainPoints = 100000;
    std::size_t maxCandidateEvaluations = 200000;
    std::size_t maxNodeVisits = 10000000;
    std::size_t maxOperations = 10000000;
    std::size_t maxLogicalBytes = 2 * 1024 * 1024;
    DiscreteEvaluationLimits evaluation{};
};

struct DiscreteExpressionDomainComparisonWork {
    std::size_t domainPoints = 0;
    std::size_t candidateEvaluations = 0;
    std::size_t nodeVisits = 0;
    std::size_t operations = 0;
    std::size_t iterations = 0;
    std::size_t iterationOperations = 0;
    std::size_t peakLogicalBytesEstimate = 0;
};

struct DiscreteExpressionDomainCounterexample {
    std::vector<std::int64_t> inputs;
    DiscreteEvaluationStatus leftStatus = DiscreteEvaluationStatus::MathematicalInvalid;
    DiscreteEvaluationStatus rightStatus = DiscreteEvaluationStatus::MathematicalInvalid;
    std::optional<DiscreteValue> leftValue;
    std::optional<DiscreteValue> rightValue;
};

struct DiscreteExpressionDomainComparisonResult {
    DiscreteExpressionDomainComparisonStatus status =
        DiscreteExpressionDomainComparisonStatus::InvalidConfiguration;
    std::optional<DiscreteExpressionDomainCounterexample> counterexample;
    DiscreteExpressionDomainComparisonWork work;
};

namespace genetik_discrete_domain_comparison_detail {
inline bool addWithin(std::size_t current, std::size_t amount, std::size_t limit) noexcept {
    return amount <= limit && current <= limit - amount;
}
} // namespace genetik_discrete_domain_comparison_detail

// Compare two closed expressions on every point in an explicitly bounded
// Int64 box. Equal values agree; two mathematical-invalid outcomes also agree
// as observable evaluator behavior. NoWitness is also a completed status and
// agrees only with NoWitness. A completed non-value outcome and a value
// disagree. Resource exhaustion and cancellation never count as mathematical
// disagreement. Completion establishes equivalence only on the
// enumerated finite box, not on all Int64 inputs.
inline DiscreteExpressionDomainComparisonResult
compareDiscreteExpressionsOnBox(const DiscreteExpression& left, const DiscreteExpression& right,
                                std::span<const DiscreteIntegerDomainRange> ranges,
                                DiscreteExpressionDomainComparisonLimits limits = {},
                                const std::atomic<bool>* stop = nullptr) {
    using Status = DiscreteExpressionDomainComparisonStatus;
    using EvaluationStatus = DiscreteEvaluationStatus;
    using namespace genetik_discrete_domain_comparison_detail;
    DiscreteExpressionDomainComparisonResult result;
    try {
        if (left.dimensions() != right.dimensions() || ranges.size() != left.dimensions() ||
            left.outerBindings().size() || right.outerBindings().size()) {
            result.status = Status::InvalidConfiguration;
            return result;
        }
        try {
            DiscreteExpression::validateEvaluationLimits(limits.evaluation);
        } catch (const std::invalid_argument&) {
            result.status = Status::InvalidConfiguration;
            return result;
        }
        if (!limits.maxDomainPoints || limits.maxCandidateEvaluations < 2 ||
            !limits.maxLogicalBytes) {
            result.status = Status::InconclusiveResourceExhausted;
            return result;
        }
        for (const auto& range : ranges) {
            if (range.lower > range.upper) {
                result.status = Status::InvalidConfiguration;
                return result;
            }
        }
        constexpr auto tupleValueBytes = sizeof(std::int64_t);
        if (ranges.size() >
            (std::numeric_limits<std::size_t>::max() - sizeof(result)) / tupleValueBytes) {
            result.status = Status::InconclusiveResourceExhausted;
            return result;
        }
        const auto tupleBytes = ranges.size() * tupleValueBytes;
        const auto retainedBytes = sizeof(result) + tupleBytes;
        if (limits.evaluation.maxWorkspaceBytes >
            std::numeric_limits<std::size_t>::max() - retainedBytes) {
            result.status = Status::InconclusiveResourceExhausted;
            return result;
        }
        const auto logicalBytes = retainedBytes + limits.evaluation.maxWorkspaceBytes;
        if (logicalBytes > limits.maxLogicalBytes) {
            result.work.peakLogicalBytesEstimate = logicalBytes;
            result.status = Status::InconclusiveResourceExhausted;
            return result;
        }
        result.work.peakLogicalBytesEstimate = logicalBytes;
        std::vector<std::int64_t> inputs;
        if (ranges.size() > inputs.max_size()) {
            result.status = Status::InconclusiveResourceExhausted;
            return result;
        }
        inputs.reserve(ranges.size());
        for (const auto& range : ranges)
            inputs.push_back(range.lower);

        const auto advance = [&]() {
            for (std::size_t i = ranges.size(); i > 0; --i) {
                const auto index = i - 1;
                if (inputs[index] < ranges[index].upper) {
                    ++inputs[index];
                    for (std::size_t reset = index + 1; reset < ranges.size(); ++reset)
                        inputs[reset] = ranges[reset].lower;
                    return true;
                }
            }
            return false;
        };
        if (ranges.empty()) {
            // A zero-dimensional box contains its single empty tuple.
            inputs.clear();
        }
        bool more = true;
        while (more) {
            if (stop && stop->load(std::memory_order_relaxed)) {
                result.status = Status::InconclusiveCancelled;
                return result;
            }
            if (result.work.domainPoints == limits.maxDomainPoints ||
                !addWithin(result.work.candidateEvaluations, 2, limits.maxCandidateEvaluations)) {
                result.status = Status::InconclusiveResourceExhausted;
                return result;
            }
            ++result.work.domainPoints;
            DiscreteEvaluationResult a, b;
            const auto evaluate = [&](const DiscreteExpression& expression,
                                      DiscreteEvaluationResult& out) {
                if (result.work.candidateEvaluations == limits.maxCandidateEvaluations)
                    return false;
                if (result.work.nodeVisits >= limits.maxNodeVisits)
                    return false;
                ++result.work.candidateEvaluations;
                auto evaluationLimits = limits.evaluation;
                evaluationLimits.maxNodeVisits = std::min(
                    evaluationLimits.maxNodeVisits, limits.maxNodeVisits - result.work.nodeVisits);
                evaluationLimits.maxOperations = std::min(
                    evaluationLimits.maxOperations, limits.maxOperations - result.work.operations);
                out = expression.evaluate(inputs, evaluationLimits, stop);
                result.work.nodeVisits += out.nodeVisits;
                result.work.operations += out.operations;
                result.work.iterations += out.iterations;
                result.work.iterationOperations += out.iterationOperations;
                return true;
            };
            if (!evaluate(left, a) || a.status == EvaluationStatus::ResourceExhausted ||
                a.status == EvaluationStatus::Cancelled) {
                result.status = a.status == EvaluationStatus::Cancelled
                                    ? Status::InconclusiveCancelled
                                    : Status::InconclusiveResourceExhausted;
                return result;
            }
            if (!evaluate(right, b) || b.status == EvaluationStatus::ResourceExhausted ||
                b.status == EvaluationStatus::Cancelled) {
                result.status = b.status == EvaluationStatus::Cancelled
                                    ? Status::InconclusiveCancelled
                                    : Status::InconclusiveResourceExhausted;
                return result;
            }
            if (a.status != EvaluationStatus::Valid &&
                a.status != EvaluationStatus::MathematicalInvalid &&
                a.status != EvaluationStatus::NoWitness) {
                result.status = Status::InconclusiveResourceExhausted;
                return result;
            }
            if (b.status != EvaluationStatus::Valid &&
                b.status != EvaluationStatus::MathematicalInvalid &&
                b.status != EvaluationStatus::NoWitness) {
                result.status = Status::InconclusiveResourceExhausted;
                return result;
            }
            const bool same =
                a.status == b.status && (a.status != EvaluationStatus::Valid || a.value == b.value);
            if (!same) {
                if (!addWithin(logicalBytes, tupleBytes, limits.maxLogicalBytes)) {
                    result.status = Status::InconclusiveResourceExhausted;
                    return result;
                }
                result.work.peakLogicalBytesEstimate = logicalBytes + tupleBytes;
                result.status = Status::DisagreementFound;
                result.counterexample = DiscreteExpressionDomainCounterexample{
                    inputs, a.status, b.status, a.value, b.value};
                return result;
            }
            more = ranges.empty() ? false : advance();
        }
        result.status = Status::EquivalentOnBoundedDomain;
        return result;
    } catch (const std::bad_alloc&) {
        result.status = Status::InconclusiveResourceExhausted;
        return result;
    }
}
