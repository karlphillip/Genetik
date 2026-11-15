#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include "DiscreteExpressionSpecification.hpp"

// Deterministically composes caller-supplied branch and predicate expressions.
// Agreement is only with the finite specification supplied to this invocation.
enum class DiscreteConditionalCompositionStatus {
    ExactTraining,
    Partial,
    Invalid,
    ResourceExhausted,
    Cancelled
};

struct DiscreteConditionalCompositionLimits {
    std::size_t maxCandidateCombinations = 100000;
    std::size_t maxNodes = 128;
    std::size_t maxDepth = 32;
    std::size_t maxEvaluationWork = 10000000;
    std::size_t maxEstimatedBytes = 64 * 1024 * 1024;
    DiscreteEvaluationLimits evaluation{};
    // Separate opt-in contract: limits the number of exact structural-class
    // joins in partition-directed selection. It does not change maxCandidateCombinations.
    std::size_t maxPartitionDirectedJoins = 1000000;
};

struct DiscreteConditionalCompositionWork {
    std::size_t combinations = 0;
    std::size_t specificationComparisons = 0;
    std::size_t nodeVisits = 0;
    std::size_t operations = 0;
    std::size_t evaluatedRows = 0;
    std::size_t estimatedBytes = 0;
    std::size_t pruningWork = 0;
    std::size_t branchPairBoundChecks = 0;
    std::size_t prunedCombinations = 0;
    std::size_t evaluatedCombinations = 0;
    std::size_t partitionRowsScanned = 0;
    std::size_t partitionIndexComparisons = 0;
    std::size_t partitionJoinPairs = 0;
    std::size_t cartesianCombinationsAvoided = 0;
    std::size_t candidateExpressionsConstructed = 0;
    std::size_t candidateConstructionWork = 0;
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
    if (b && a > cap / b)
        return false;
    result = a * b;
    return true;
}
struct TrainingTrace {
    bool boolean = false;
    std::vector<std::uint64_t> values;
    std::vector<std::uint8_t> valid;
    bool operator==(const TrainingTrace& other) const noexcept {
        return boolean == other.boolean && values == other.values && valid == other.valid;
    }
    bool operator<(const TrainingTrace& other) const noexcept {
        return std::tie(boolean, valid, values) <
               std::tie(other.boolean, other.valid, other.values);
    }
};

// Exact finite-training selector over partition-indexed branch representatives.
// For a fixed predicate and exact (node-count, depth) class, branch feasibility
// and score depend only on rows selecting that branch. Keeping the highest
// regional score, then earliest original index, preserves every global winner:
// paired structural feasibility is identical within a class and the global
// rank is score, total nodes, then original (predicate,true,false) tuple.
inline DiscreteConditionalCompositionResult composeDiscreteConditionalPartitionDirected(
    const DiscreteSpecification& specification, std::span<const DiscreteExpression> predicates,
    std::span<const DiscreteExpression> branches, std::span<const TrainingTrace> predicateTraces,
    std::span<const TrainingTrace> branchTraces, DiscreteConditionalCompositionLimits limits = {},
    const std::atomic<bool>* cancellation = nullptr) {
    using namespace genetik_conditional_composition_detail;
    DiscreteConditionalCompositionResult result;
    const auto rows = specification.observations().size();
    result.totalRows = rows;
    std::size_t peakEstimatedBytes = 0;
    const auto stop = [&](DiscreteConditionalCompositionStatus status, const char* message) {
        result.status = status;
        result.diagnostic = message;
        result.work.estimatedBytes = std::max(result.work.estimatedBytes, peakEstimatedBytes);
        return result;
    };
    if (predicates.empty() || branches.empty() || rows == 0 ||
        predicateTraces.size() != predicates.size() || branchTraces.size() != branches.size() ||
        !limits.maxPartitionDirectedJoins || !limits.maxNodes || !limits.maxDepth ||
        !limits.maxEvaluationWork || !limits.maxEstimatedBytes)
        return stop(DiscreteConditionalCompositionStatus::Invalid,
                    "partition-directed composition inputs or limits are invalid");
    if (!std::all_of(specification.observations().begin(), specification.observations().end(),
                     [&](const auto& row) {
                         return row.inputs.size() == specification.dimensions() &&
                                row.expected.type() == DiscreteType::Int64;
                     }))
        return stop(DiscreteConditionalCompositionStatus::Invalid,
                    "partition-directed composition requires an Int64 specification");

    std::size_t total = sizeof(result);
    auto reserveEstimate = [&](std::size_t bytes) {
        if (!addWithin(total, bytes, limits.maxEstimatedBytes))
            return false;
        total += bytes;
        peakEstimatedBytes = std::max(peakEstimatedBytes, total);
        return true;
    };
    auto expressionBytes = [&](std::span<const DiscreteExpression> expressions) {
        for (const auto& expression : expressions) {
            if (expression.nodeCount() > limits.maxNodes ||
                expression.nodeCount() > limits.maxEstimatedBytes /
                                             DiscreteExpression::storageBytesPerNode() ||
                !reserveEstimate(expression.nodeCount() *
                                 DiscreteExpression::storageBytesPerNode()))
                return false;
        }
        return true;
    };
    if (!expressionBytes(predicates) || !expressionBytes(branches))
        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                    "partition-directed expression storage limit exhausted");
    for (const auto& row : specification.observations()) {
        std::size_t inputBytes = 0;
        if (!multiplyWithin(row.inputs.size(), sizeof(std::int64_t),
                            limits.maxEstimatedBytes, inputBytes) ||
            !addWithin(inputBytes, sizeof(row), limits.maxEstimatedBytes) ||
            !reserveEstimate(inputBytes + sizeof(row)))
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "partition-directed specification memory exhausted");
    }
    if (!reserveEstimate(limits.evaluation.maxWorkspaceBytes))
        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                    "partition-directed evaluator workspace estimate exhausted");
    if (branches.size() > std::numeric_limits<std::size_t>::max() - predicates.size())
        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                    "partition-directed trace count overflow");
    std::size_t traceRows = 0, traceBytes = 0;
    if (!multiplyWithin(predicates.size() + branches.size(), rows,
                        std::numeric_limits<std::size_t>::max(), traceRows) ||
        !multiplyWithin(traceRows, sizeof(std::uint64_t) + sizeof(std::uint8_t),
                        limits.maxEstimatedBytes, traceBytes) ||
        !reserveEstimate(traceBytes))
        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                    "partition-directed trace estimate exhausted");
    std::size_t traceHeaders = 0;
    if (!multiplyWithin(predicates.size() + branches.size(), sizeof(TrainingTrace) + 128,
                        limits.maxEstimatedBytes, traceHeaders) ||
        !reserveEstimate(traceHeaders))
        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                    "partition-directed trace-header estimate exhausted");

    // The index is intentionally small: entries are exact structural classes,
    // one best row-partition representative per class, and class joins.
    struct Group {
        std::size_t nodes, depth, branch, score;
    };
    // Each branch can contribute at most one group per side. This bound is
    // independent of the join cap: an empty opposite region does not bound the
    // number of indexed classes on the populated side.
    const auto maxGroups = branches.size();
    std::size_t groupBytes = 0;
    if (!multiplyWithin(maxGroups, (sizeof(Group) + 64) * 2,
                        limits.maxEstimatedBytes, groupBytes) ||
        !addWithin(groupBytes, 2 * sizeof(std::vector<Group>) + 128,
                   limits.maxEstimatedBytes))
        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                    "partition-directed index memory limit exhausted");
    groupBytes += 2 * sizeof(std::vector<Group>) + 128;

    std::size_t work = 0;
    auto charge = [&](std::size_t amount) {
        if (!addWithin(work, amount, limits.maxEvaluationWork))
            return false;
        work += amount;
        result.work.pruningWork += amount;
        return true;
    };
    const auto isCancelled = [&]() {
        return cancellation && cancellation->load(std::memory_order_relaxed);
    };
    for (std::size_t i = 0; i < rows; ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (isCancelled())
                return stop(DiscreteConditionalCompositionStatus::Cancelled,
                            "partition-directed duplicate-row validation cancelled");
            const auto& left = specification.observations()[i].inputs;
            const auto& right = specification.observations()[j].inputs;
            bool duplicate = left.size() == right.size();
            for (std::size_t k = 0; duplicate && k < left.size(); ++k) {
                if (!charge(1))
                    return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                "partition-directed duplicate-row work exhausted");
                ++result.work.specificationComparisons;
                duplicate = left[k] == right[k];
            }
            if (duplicate)
                return stop(DiscreteConditionalCompositionStatus::Invalid,
                            "finite specification contains duplicate input rows");
        }
    }
    std::optional<DiscreteExpression> best;
    std::size_t bestScore = 0, bestNodes = std::numeric_limits<std::size_t>::max();
    std::array<std::size_t, 3> bestOrder{std::numeric_limits<std::size_t>::max(),
                                         std::numeric_limits<std::size_t>::max(),
                                         std::numeric_limits<std::size_t>::max()};
    bool structuralExclusion = false;
    std::size_t joins = 0;
    std::size_t bestStorageBytes = 0;
    for (std::size_t p = 0; p < predicates.size(); ++p) {
        if (isCancelled())
            return stop(DiscreteConditionalCompositionStatus::Cancelled,
                        "partition-directed composition cancelled");
        const auto& predicate = predicates[p];
        const auto& pt = predicateTraces[p];
        if (predicate.type() != DiscreteType::Bool ||
            predicate.dimensions() != specification.dimensions() ||
            !predicate.outerBindings().empty() || pt.boolean == false || pt.values.size() != rows ||
            pt.valid.size() != rows)
            return stop(DiscreteConditionalCompositionStatus::Invalid,
                        "predicate schema or partition trace is invalid");
        bool predicateValid = true;
        if (rows > limits.maxEstimatedBytes - std::min<std::size_t>(64, limits.maxEstimatedBytes))
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "partition mask memory limit exhausted");
        const auto maskBytes = rows + 64;
        std::size_t temporaryBytes = 0;
        if (!addWithin(groupBytes, maskBytes, limits.maxEstimatedBytes) ||
            !addWithin(total, groupBytes + maskBytes, limits.maxEstimatedBytes) ||
            !addWithin(total + groupBytes + maskBytes, bestStorageBytes,
                       limits.maxEstimatedBytes))
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "partition-directed temporary memory limit exhausted");
        temporaryBytes = groupBytes + maskBytes;
        peakEstimatedBytes =
            std::max(peakEstimatedBytes, total + temporaryBytes + bestStorageBytes);
        std::vector<std::uint8_t> selected;
        try {
            selected.resize(rows);
        } catch (const std::bad_alloc&) {
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "partition mask allocation failed");
        } catch (const std::length_error&) {
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "partition mask size is invalid");
        }
        for (std::size_t row = 0; row < rows; ++row) {
            if (isCancelled())
                return stop(DiscreteConditionalCompositionStatus::Cancelled,
                            "partition-directed composition cancelled");
            if (!charge(2))
                return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                            "partition-directed work limit exhausted");
            ++result.work.partitionRowsScanned;
            predicateValid = predicateValid && pt.valid[row] != 0;
            selected[row] = pt.values[row] ? 1 : 0;
        }
        if (!predicateValid)
            continue; // Invalid condition at any observation invalidates the whole ITE.

        std::size_t groupInitializationWork = 0;
        if (!multiplyWithin(branches.size(), 2, limits.maxEvaluationWork,
                            groupInitializationWork) ||
            !charge(groupInitializationWork))
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "partition-directed index initialization work exhausted");
        std::vector<Group> yesGroups, noGroups;
        try {
            yesGroups.reserve(branches.size());
            noGroups.reserve(branches.size());
        } catch (const std::bad_alloc&) {
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "partition-directed index allocation failed");
        } catch (const std::length_error&) {
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "partition-directed index capacity is invalid");
        }
        for (std::size_t branch = 0; branch < branches.size(); ++branch) {
            if (isCancelled())
                return stop(DiscreteConditionalCompositionStatus::Cancelled,
                            "partition-directed composition cancelled");
            const auto& expression = branches[branch];
            const auto& trace = branchTraces[branch];
            if (expression.type() != DiscreteType::Int64 ||
                expression.dimensions() != specification.dimensions() ||
                !expression.outerBindings().empty() || trace.boolean ||
                trace.values.size() != rows ||
                trace.valid.size() != rows)
                return stop(DiscreteConditionalCompositionStatus::Invalid,
                            "branch schema or partition trace is invalid");
            for (unsigned side = 0; side < 2; ++side) {
                bool valid = true;
                std::size_t score = 0;
                for (std::size_t row = 0; row < rows; ++row) {
                    if (isCancelled())
                        return stop(DiscreteConditionalCompositionStatus::Cancelled,
                                    "partition-directed composition cancelled");
                    if (!charge(1))
                        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                    "partition-directed work limit exhausted");
                    ++result.work.partitionRowsScanned;
                    if (static_cast<bool>(selected[row]) != (side == 0))
                        continue;
                    if (!charge(1))
                        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                    "partition-directed work limit exhausted");
                    if (!trace.valid[row])
                        valid = false;
                    else {
                        if (!charge(1))
                            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                        "partition-directed work limit exhausted");
                        if (trace.values[row] == static_cast<std::uint64_t>(
                                                     specification.observations()[row]
                                                         .expected.integer()))
                            ++score;
                    }
                }
                if (!valid)
                    continue;
                auto& groups = side == 0 ? yesGroups : noGroups;
                const auto depth = expression.depth();
                auto found = groups.end();
                for (auto it = groups.begin(); it != groups.end(); ++it) {
                    if (!charge(2))
                        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                    "partition-directed work limit exhausted");
                    result.work.partitionIndexComparisons += 2;
                    if (it->nodes == expression.nodeCount() && it->depth == depth) {
                        found = it;
                        break;
                    }
                }
                if (found == groups.end()) {
                    if (!charge(1))
                        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                    "partition-directed index insertion work exhausted");
                    groups.push_back({expression.nodeCount(), depth, branch, score});
                } else {
                    if (!charge(2))
                        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                    "partition-directed index comparison work exhausted");
                    if (score > found->score ||
                        (score == found->score && branch < found->branch))
                        *found = {expression.nodeCount(), depth, branch, score};
                }
            }
        }
        for (const auto& yes : yesGroups) {
            for (const auto& no : noGroups) {
                if (isCancelled())
                    return stop(DiscreteConditionalCompositionStatus::Cancelled,
                                "partition-directed composition cancelled");
                if (joins == limits.maxPartitionDirectedJoins)
                    return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                "partition-directed join limit exhausted");
                if (!charge(10))
                    return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                "partition-directed work limit exhausted");
                ++joins;
                ++result.work.partitionJoinPairs;
                const auto& trueBranch = branches[yes.branch];
                const auto& falseBranch = branches[no.branch];
                std::size_t totalNodes = predicate.nodeCount();
                if (trueBranch.nodeCount() > limits.maxNodes - totalNodes) {
                    structuralExclusion = true;
                    continue;
                }
                totalNodes += trueBranch.nodeCount();
                if (falseBranch.nodeCount() > limits.maxNodes - totalNodes) {
                    structuralExclusion = true;
                    continue;
                }
                totalNodes += falseBranch.nodeCount();
                if (totalNodes >= limits.maxNodes || predicate.depth() >= limits.maxDepth ||
                    trueBranch.depth() >= limits.maxDepth ||
                    falseBranch.depth() >= limits.maxDepth) {
                    structuralExclusion = true;
                    continue;
                }
                ++totalNodes; // The enclosing conditional node.
                const auto score = yes.score + no.score;
                const std::array<std::size_t, 3> order{p, yes.branch, no.branch};
                if (!charge(5))
                    return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                "partition-directed selection work limit exhausted");
                if (!best || score > bestScore ||
                    (score == bestScore &&
                     (totalNodes < bestNodes ||
                      (totalNodes == bestNodes && order < bestOrder)))) {
                    std::size_t candidateStorage = 0;
                    if (!multiplyWithin(totalNodes, DiscreteExpression::storageBytesPerNode(),
                                        limits.maxEstimatedBytes, candidateStorage) ||
                        !addWithin(candidateStorage, sizeof(DiscreteExpression),
                                   limits.maxEstimatedBytes))
                        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                    "partitioned result expression memory exhausted");
                    candidateStorage += sizeof(DiscreteExpression);
                    std::size_t transientBytes = 0;
                    if (!addWithin(candidateStorage, bestStorageBytes,
                                   limits.maxEstimatedBytes))
                        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                    "partitioned result expression memory exhausted");
                    transientBytes = candidateStorage + bestStorageBytes;
                    if (!addWithin(total, temporaryBytes, limits.maxEstimatedBytes) ||
                        !addWithin(total + temporaryBytes, transientBytes,
                                   limits.maxEstimatedBytes))
                        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                    "partitioned result expression memory exhausted");
                    peakEstimatedBytes = std::max(peakEstimatedBytes,
                                                  total + temporaryBytes + transientBytes);
                    result.work.estimatedBytes = peakEstimatedBytes;
                    if (!charge(totalNodes))
                        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                                    "partition-directed construction work limit exhausted");
                    result.work.candidateConstructionWork += totalNodes;
                    std::optional<DiscreteExpression> replacement;
                    try {
                        replacement.emplace(DiscreteExpression::conditional(
                            predicate, trueBranch, falseBranch));
                    } catch (const std::invalid_argument&) {
                        continue;
                    } catch (const std::length_error&) {
                        continue;
                    }
                    best.swap(replacement);
                    ++result.work.candidateExpressionsConstructed;
                    bestScore = score;
                    bestNodes = totalNodes;
                    bestOrder = order;
                    bestStorageBytes = candidateStorage;
                }
            }
        }
    }
    std::size_t predicateBranchPairs = 0, theoretical = 0;
    if (!multiplyWithin(predicates.size(), branches.size(),
                        std::numeric_limits<std::size_t>::max(), predicateBranchPairs) ||
        !multiplyWithin(predicateBranchPairs, branches.size(),
                        std::numeric_limits<std::size_t>::max(), theoretical))
        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                    "partition-directed Cartesian reference count overflow");
    result.work.combinations = theoretical;
    result.work.cartesianCombinationsAvoided = theoretical > joins ? theoretical - joins : 0;
    if (!addWithin(total, bestStorageBytes, limits.maxEstimatedBytes))
        return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                    "partitioned result expression memory exhausted");
    result.work.estimatedBytes = std::max(peakEstimatedBytes, total + bestStorageBytes);
    result.work.evaluatedCombinations = joins;
    result.matchingRows = bestScore;
    result.expression = std::move(best);
    if (!result.expression) {
        return stop(structuralExclusion ? DiscreteConditionalCompositionStatus::ResourceExhausted
                                       : DiscreteConditionalCompositionStatus::Invalid,
                    structuralExclusion ? "all partitioned compositions exceed structural limits"
                                        : "no mathematically valid partitioned composition");
    }
    // Re-evaluate only the selected expression with the production evaluator;
    // this verifies checked arithmetic, lazy invalidity and combined quotas.
    std::size_t verifiedMatches = 0;
    for (const auto& row : specification.observations()) {
        if (isCancelled())
            return stop(DiscreteConditionalCompositionStatus::Cancelled,
                        "partition-directed result validation cancelled");
        if (work >= limits.maxEvaluationWork)
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "partition-directed work limit exhausted during verification");
        auto evaluationLimits = limits.evaluation;
        const auto remaining = limits.maxEvaluationWork - work;
        evaluationLimits.maxNodeVisits = std::min(evaluationLimits.maxNodeVisits, remaining / 2);
        evaluationLimits.maxOperations = std::min(evaluationLimits.maxOperations,
                                                  remaining - remaining / 2);
        const auto evaluated =
            result.expression->evaluate(row.inputs, evaluationLimits, cancellation);
        std::size_t rowWork = 0;
        if (!addWithin(evaluated.nodeVisits, evaluated.operations,
                       limits.maxEvaluationWork) ||
            !addWithin(work, evaluated.nodeVisits + evaluated.operations,
                       limits.maxEvaluationWork))
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "partition-directed work limit exhausted during verification");
        rowWork = evaluated.nodeVisits + evaluated.operations;
        work += rowWork;
        result.work.nodeVisits += evaluated.nodeVisits;
        result.work.operations += evaluated.operations;
        ++result.work.evaluatedRows;
        if (evaluated.status == DiscreteEvaluationStatus::Cancelled)
            return stop(DiscreteConditionalCompositionStatus::Cancelled,
                        "partition-directed result verification cancelled");
        if (evaluated.status == DiscreteEvaluationStatus::ResourceExhausted)
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "selected partitioned expression exhausted evaluator limits");
        if (!evaluated.valid())
            return stop(DiscreteConditionalCompositionStatus::Invalid,
                        "selected partitioned expression is mathematically invalid");
        if (!charge(1))
            return stop(DiscreteConditionalCompositionStatus::ResourceExhausted,
                        "partition-directed comparison work limit exhausted");
        ++result.work.specificationComparisons;
        verifiedMatches += *evaluated.value == row.expected ? 1U : 0U;
    }
    if (verifiedMatches != result.matchingRows)
        return stop(DiscreteConditionalCompositionStatus::Invalid,
                    "partition scores disagree with deterministic evaluator verification");
    result.status = result.matchingRows == rows
                        ? DiscreteConditionalCompositionStatus::ExactTraining
                        : DiscreteConditionalCompositionStatus::Partial;
    result.diagnostic = result.status == DiscreteConditionalCompositionStatus::ExactTraining
                            ? "exact agreement on the finite training specification"
                            : "best valid partition-directed composition partially agrees";
    return result;
}

inline DiscreteConditionalCompositionResult composeDiscreteConditionalWithBranchTraces(
    const DiscreteSpecification& specification, std::span<const DiscreteExpression> predicates,
    std::span<const DiscreteExpression> branches, DiscreteConditionalCompositionLimits limits = {},
    const std::atomic<bool>* cancellation = nullptr,
    std::span<const TrainingTrace> branchTraces = {}, bool specificationGuidedPruning = false) {
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
                     [&](const auto& row) {
                         return row.inputs.size() == specification.dimensions() &&
                                row.expected.type() == DiscreteType::Int64;
                     })) {
        result.diagnostic = "conditional composition requires a valid Int64 specification";
        return result;
    }
    if (specificationGuidedPruning && branchTraces.size() != branches.size()) {
        result.diagnostic = "specification-guided pruning requires one trace per branch";
        return result;
    }
    std::size_t inputBytes = sizeof(result);
    for (const auto& row : specification.observations()) {
        std::size_t payload = 0;
        if (!multiplyWithin(row.inputs.size(), sizeof(std::int64_t), limits.maxEstimatedBytes,
                            payload) ||
            !addWithin(sizeof(row), payload, limits.maxEstimatedBytes))
            return exhausted();
        const auto rowBytes = sizeof(row) + payload;
        if (!addWithin(inputBytes, rowBytes, limits.maxEstimatedBytes))
            return exhausted();
        inputBytes += rowBytes;
    }
    for (const auto& expression : predicates) {
        std::size_t bytes = 0;
        if (!multiplyWithin(expression.nodeCount(), DiscreteExpression::storageBytesPerNode(),
                            limits.maxEstimatedBytes, bytes))
            return exhausted();
        if (!addWithin(inputBytes, bytes, limits.maxEstimatedBytes))
            return exhausted();
        inputBytes += bytes;
    }
    for (const auto& expression : branches) {
        std::size_t bytes = 0;
        if (!multiplyWithin(expression.nodeCount(), DiscreteExpression::storageBytesPerNode(),
                            limits.maxEstimatedBytes, bytes))
            return exhausted();
        if (!addWithin(inputBytes, bytes, limits.maxEstimatedBytes))
            return exhausted();
        inputBytes += bytes;
    }
    if (specificationGuidedPruning) {
        for (const auto& trace : branchTraces) {
            if (trace.boolean || trace.values.size() != result.totalRows ||
                trace.valid.size() != result.totalRows) {
                result.diagnostic = "branch training trace has an invalid type or row count";
                return result;
            }
        }
    }
    DiscreteSpecificationValidationLimits validationDefaults;
    const auto scratchBytes =
        std::max(limits.evaluation.maxWorkspaceBytes, validationDefaults.maxPeakRowWorkspaceBytes);
    if (!addWithin(inputBytes, scratchBytes, limits.maxEstimatedBytes))
        return exhausted();
    inputBytes += scratchBytes;
    result.work.estimatedBytes = inputBytes;
    std::size_t candidateBaseBytes = inputBytes;
    std::vector<std::size_t> pairUpperBounds;
    std::vector<std::uint8_t> pairFeasible;
    std::size_t aggregateWork = 0;
    const auto charge = [&](std::size_t& counter, std::size_t amount) {
        if (!addWithin(aggregateWork, amount, limits.maxEvaluationWork) ||
            !addWithin(counter, amount, limits.maxEvaluationWork))
            return false;
        aggregateWork += amount;
        counter += amount;
        return true;
    };
    for (std::size_t i = 0; i < result.totalRows; ++i) {
        if (cancellation && cancellation->load(std::memory_order_relaxed))
            return cancelled();
        for (std::size_t j = 0; j < i; ++j) {
            const auto& left = specification.observations()[i].inputs;
            const auto& right = specification.observations()[j].inputs;
            for (std::size_t k = 0; k < left.size(); ++k) {
                if (!charge(result.work.specificationComparisons, 1))
                    return exhausted();
                if (left[k] != right[k])
                    break;
                if (k + 1 == left.size()) {
                    result.status = DiscreteConditionalCompositionStatus::Invalid;
                    result.diagnostic = "finite specification contains duplicate input rows";
                    return result;
                }
            }
            if (left.empty()) {
                if (!charge(result.work.specificationComparisons, 1))
                    return exhausted();
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
    if (combinations > limits.maxCandidateCombinations)
        return exhausted();
    result.work.combinations = combinations;

    if (specificationGuidedPruning) {
        std::size_t maskCount = 0, maskBytes = 0;
        if (!multiplyWithin(branches.size(), result.totalRows,
                            std::numeric_limits<std::size_t>::max(), maskCount) ||
            !multiplyWithin(maskCount, sizeof(std::uint8_t) * 2, limits.maxEstimatedBytes,
                            maskBytes))
            return exhausted();
        struct BranchMask {
            std::vector<std::uint8_t> valid, matches;
        };
        std::size_t maskOverhead = sizeof(std::vector<BranchMask>) + 128;
        std::size_t perBranchOverhead = 0;
        std::size_t maskTotal = 0;
        if (!multiplyWithin(branches.size(), sizeof(BranchMask) + 64, limits.maxEstimatedBytes,
                            perBranchOverhead) ||
            !addWithin(maskBytes, maskOverhead, limits.maxEstimatedBytes))
            return exhausted();
        maskTotal = maskBytes + maskOverhead;
        if (!addWithin(maskTotal, perBranchOverhead, limits.maxEstimatedBytes))
            return exhausted();
        maskBytes = maskTotal + perBranchOverhead;
        if (!addWithin(inputBytes, maskBytes, limits.maxEstimatedBytes))
            return exhausted();
        result.work.estimatedBytes = inputBytes + maskBytes;
        std::vector<BranchMask> masks(branches.size());
        for (std::size_t b = 0; b < branches.size(); ++b) {
            masks[b].valid.resize(result.totalRows);
            masks[b].matches.resize(result.totalRows);
            for (std::size_t row = 0; row < result.totalRows; ++row) {
                if (cancellation && cancellation->load(std::memory_order_relaxed))
                    return cancelled();
                if (!charge(result.work.pruningWork, 2))
                    return exhausted();
                const auto valid = branchTraces[b].valid[row] != 0;
                const auto matches =
                    valid && branchTraces[b].values[row] ==
                                 static_cast<std::uint64_t>(
                                     specification.observations()[row].expected.integer());
                masks[b].valid[row] = valid ? 1 : 0;
                masks[b].matches[row] = matches ? 1 : 0;
            }
        }
        std::size_t pairCount = 0;
        if (!multiplyWithin(branches.size(), branches.size(), limits.maxEstimatedBytes, pairCount))
            return exhausted();
        std::size_t pairBytes = 0;
        if (!multiplyWithin(pairCount, sizeof(std::size_t) + sizeof(std::uint8_t),
                            limits.maxEstimatedBytes, pairBytes) ||
            !addWithin(result.work.estimatedBytes, pairBytes, limits.maxEstimatedBytes))
            return exhausted();
        const auto pairOverhead =
            sizeof(std::vector<std::size_t>) + sizeof(std::vector<std::uint8_t>) + 128;
        if (!addWithin(pairBytes, pairOverhead, limits.maxEstimatedBytes) ||
            !addWithin(result.work.estimatedBytes, pairBytes + pairOverhead,
                       limits.maxEstimatedBytes))
            return exhausted();
        pairBytes += pairOverhead;
        result.work.estimatedBytes += pairBytes;
        candidateBaseBytes = result.work.estimatedBytes;
        pairUpperBounds.resize(pairCount);
        pairFeasible.resize(pairCount, 1);
        for (std::size_t t = 0; t < branches.size(); ++t) {
            for (std::size_t f = 0; f < branches.size(); ++f) {
                const auto pair = t * branches.size() + f;
                for (std::size_t row = 0; row < result.totalRows; ++row) {
                    if (cancellation && cancellation->load(std::memory_order_relaxed))
                        return cancelled();
                    if (!charge(result.work.pruningWork, 2))
                        return exhausted();
                    if (!masks[t].valid[row] && !masks[f].valid[row])
                        pairFeasible[pair] = 0;
                    if (masks[t].matches[row] || masks[f].matches[row])
                        ++pairUpperBounds[pair];
                }
                if (!charge(result.work.pruningWork, 1))
                    return exhausted();
                ++result.work.branchPairBoundChecks;
            }
        }
    }

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
                if (cancellation && cancellation->load(std::memory_order_relaxed))
                    return cancelled();
                const auto& no = branches[f];
                if (predicate.nodeCount() > limits.maxNodes || yes.nodeCount() > limits.maxNodes ||
                    no.nodeCount() > limits.maxNodes ||
                    predicate.nodeCount() > limits.maxNodes - yes.nodeCount() ||
                    predicate.nodeCount() + yes.nodeCount() > limits.maxNodes - no.nodeCount() ||
                    predicate.nodeCount() + yes.nodeCount() + no.nodeCount() == limits.maxNodes) {
                    structureExcluded = true;
                    continue;
                }
                const auto nodes = predicate.nodeCount() + yes.nodeCount() + no.nodeCount() + 1;
                const auto depth =
                    std::max({predicate.depth() + 1, yes.depth() + 1, no.depth() + 1});
                if (nodes > limits.maxNodes || depth > limits.maxDepth) {
                    structureExcluded = true;
                    continue;
                }
                std::size_t candidateBytes = 0;
                if (!multiplyWithin(nodes, DiscreteExpression::storageBytesPerNode(),
                                    limits.maxEstimatedBytes, candidateBytes))
                    return exhausted();
                if (!addWithin(candidateBytes, candidateBytes, limits.maxEstimatedBytes) ||
                    !addWithin(candidateBaseBytes, candidateBytes * 2, limits.maxEstimatedBytes))
                    return exhausted();
                const auto frameBytes = DiscreteExpression::evaluationFrameBytes();
                const bool expressionQuotaCannotExhaust =
                    limits.evaluation.maxNodeVisits >= nodes &&
                    limits.evaluation.maxOperations >= nodes &&
                    depth <= limits.evaluation.maxWorkspaceBytes / frameBytes;
                std::size_t worstCasePerRow = 0, worstCaseEvaluation = 0;
                const bool aggregateQuotaCannotExhaust =
                    multiplyWithin(nodes, 2, limits.maxEvaluationWork, worstCasePerRow) &&
                    multiplyWithin(worstCasePerRow, result.totalRows, limits.maxEvaluationWork,
                                   worstCaseEvaluation);
                if (specificationGuidedPruning && expressionQuotaCannotExhaust &&
                    !charge(result.work.pruningWork, 1))
                    return exhausted();
                if (specificationGuidedPruning && expressionQuotaCannotExhaust &&
                    aggregateQuotaCannotExhaust &&
                    addWithin(aggregateWork, worstCaseEvaluation, limits.maxEvaluationWork)) {
                    const auto pair = t * branches.size() + f;
                    // Strict inequality is required: an equal-score candidate
                    // may win on node count or deterministic tuple ordering.
                    if (!pairFeasible[pair] || (best && pairUpperBounds[pair] < bestMatches)) {
                        if (result.work.prunedCombinations ==
                            std::numeric_limits<std::size_t>::max())
                            return exhausted();
                        ++result.work.prunedCombinations;
                        continue;
                    }
                }
                result.work.estimatedBytes =
                    std::max(result.work.estimatedBytes, candidateBaseBytes + candidateBytes * 2);
                std::optional<DiscreteExpression> candidate;
                try {
                    candidate.emplace(DiscreteExpression::conditional(predicate, yes, no));
                } catch (const std::invalid_argument&) {
                    continue;
                } catch (const std::length_error&) {
                    continue;
                }
                if (result.work.evaluatedCombinations == std::numeric_limits<std::size_t>::max())
                    return exhausted();
                ++result.work.evaluatedCombinations;
                std::size_t matches = 0;
                bool valid = true;
                for (const auto& row : specification.observations()) {
                    if (cancellation && cancellation->load(std::memory_order_relaxed))
                        return cancelled();
                    if (aggregateWork >= limits.maxEvaluationWork)
                        return exhausted();
                    auto evaluationLimits = limits.evaluation;
                    const auto remaining = limits.maxEvaluationWork - aggregateWork;
                    const auto visitAllowance = remaining / 2;
                    evaluationLimits.maxNodeVisits =
                        std::min(evaluationLimits.maxNodeVisits, visitAllowance);
                    evaluationLimits.maxOperations =
                        std::min(evaluationLimits.maxOperations, remaining - visitAllowance);
                    auto evaluated =
                        candidate->evaluate(row.inputs, evaluationLimits, cancellation);
                    std::size_t rowWork = 0;
                    if (!addWithin(evaluated.nodeVisits, evaluated.operations,
                                   limits.maxEvaluationWork))
                        return exhausted();
                    rowWork = evaluated.nodeVisits + evaluated.operations;
                    if (!addWithin(aggregateWork, rowWork, limits.maxEvaluationWork) ||
                        !addWithin(result.work.nodeVisits, evaluated.nodeVisits,
                                   limits.maxEvaluationWork) ||
                        !addWithin(result.work.operations, evaluated.operations,
                                   limits.maxEvaluationWork))
                        return exhausted();
                    aggregateWork += rowWork;
                    result.work.nodeVisits += evaluated.nodeVisits;
                    result.work.operations += evaluated.operations;
                    ++result.work.evaluatedRows;
                    if (evaluated.status == DiscreteEvaluationStatus::Cancelled)
                        return cancelled();
                    if (evaluated.status == DiscreteEvaluationStatus::ResourceExhausted)
                        return exhausted();
                    if (!evaluated.valid()) {
                        valid = false;
                        break;
                    }
                    matches += (*evaluated.value == row.expected) ? 1U : 0U;
                }
                if (!valid)
                    continue;
                if (!best || matches > bestMatches ||
                    (matches == bestMatches &&
                     (nodes < bestNodes ||
                      (nodes == bestNodes &&
                       std::array{p, t, f} < std::array{bestPredicate, bestTrue, bestFalse})))) {
                    best = std::move(candidate);
                    bestMatches = matches;
                    bestNodes = nodes;
                    bestPredicate = p;
                    bestTrue = t;
                    bestFalse = f;
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
        const auto remaining = limits.maxEvaluationWork - aggregateWork;
        validationLimits.maxNodeVisits = remaining / 3;
        validationLimits.maxOperations = remaining / 3;
        validationLimits.maxSpecificationComparisons =
            remaining - validationLimits.maxNodeVisits - validationLimits.maxOperations;
        validationLimits.maxPeakRowWorkspaceBytes =
            std::min(validationLimits.maxPeakRowWorkspaceBytes, scratchBytes);
        validationLimits.maxRetainedBytes = limits.maxEstimatedBytes - result.work.estimatedBytes;
        validationLimits.evaluation = limits.evaluation;
        auto validation = validateDiscreteSpecification(specification, *result.expression,
                                                        validationLimits, cancellation);
        if (!charge(result.work.nodeVisits, validation.work.nodeVisits) ||
            !charge(result.work.operations, validation.work.operations) ||
            !charge(result.work.specificationComparisons, validation.work.specificationComparisons))
            return exhausted();
        if (!addWithin(result.work.estimatedBytes, validation.work.retainedBytesEstimate,
                       limits.maxEstimatedBytes))
            return exhausted();
        result.work.estimatedBytes += validation.work.retainedBytesEstimate;
        if (validation.status == DiscreteSpecificationValidationStatus::InconclusiveCancelled)
            return cancelled();
        if (validation.status ==
            DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted)
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
        result.diagnostic =
            "best valid composition only partially agrees with the finite training specification";
    }
    return result;
}
} // namespace genetik_conditional_composition_detail

inline DiscreteConditionalCompositionResult composeDiscreteConditional(
    const DiscreteSpecification& specification, std::span<const DiscreteExpression> predicates,
    std::span<const DiscreteExpression> branches, DiscreteConditionalCompositionLimits limits = {},
    const std::atomic<bool>* cancellation = nullptr) {
    return genetik_conditional_composition_detail::composeDiscreteConditionalWithBranchTraces(
        specification, predicates, branches, limits, cancellation, {});
}
