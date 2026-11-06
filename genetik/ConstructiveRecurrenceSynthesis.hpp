#pragma once

#include <IntegerRecurrence.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

// Optional deterministic recurrence synthesis, independent of GA execution.
namespace genetik::constructive {

struct SequencePoint {
    std::int64_t index = 0;
    std::int64_t value = 0;
};

struct RecurrenceSynthesisConfig {
    std::vector<DiscreteKind> operators{DiscreteKind::Add, DiscreteKind::Subtract,
                                        DiscreteKind::Multiply, DiscreteKind::Modulo};
    std::vector<std::int64_t> constants{-1, 0, 1, 2, 3};
    std::size_t maximumNodes = 7;
    std::size_t maximumStructures = 60000;
    std::size_t maximumCompositionAttempts = 250000;
    std::uint64_t maximumTrainingPointEvaluations = 1000000;
    std::uint64_t maximumTransitionSteps = 10000000;
    std::uint64_t maximumNodeVisits = 20000000;
    std::uint64_t maximumArithmeticOperations = 10000000;
    std::size_t maximumEstimatedStorageBytes = 32 * 1024 * 1024;
    std::size_t maximumSteps = 1000;
    DiscreteExpressionLimits expressionLimits{};
    DiscreteEvaluationLimits evaluationLimits{};
    const std::atomic<bool> *cancelled = nullptr;
};

enum class SynthesisStop {
    ExactTrainingMatch,
    NodeLimit,
    StructureLimit,
    CompositionLimit,
    TrainingPointLimit,
    TransitionLimit,
    NodeVisitLimit,
    ArithmeticWorkLimit,
    StorageLimit,
    Cancelled,
    EvaluationResourceLimit
};
enum class TrainingRecoveryStatus { None, Partial, Exact };

constexpr std::string_view toString(SynthesisStop stop) noexcept {
    switch (stop) {
    case SynthesisStop::ExactTrainingMatch: return "exact_training_match";
    case SynthesisStop::NodeLimit: return "node_limit_complete";
    case SynthesisStop::StructureLimit: return "structure_limit";
    case SynthesisStop::CompositionLimit: return "composition_limit";
    case SynthesisStop::TrainingPointLimit: return "training_point_limit";
    case SynthesisStop::TransitionLimit: return "transition_limit";
    case SynthesisStop::NodeVisitLimit: return "node_visit_limit";
    case SynthesisStop::ArithmeticWorkLimit: return "arithmetic_work_limit";
    case SynthesisStop::StorageLimit: return "storage_limit";
    case SynthesisStop::Cancelled: return "cancelled";
    case SynthesisStop::EvaluationResourceLimit: return "evaluation_resource_limit";
    }
    return "unknown";
}

constexpr std::string_view toString(TrainingRecoveryStatus status) noexcept {
    switch (status) {
    case TrainingRecoveryStatus::None: return "none";
    case TrainingRecoveryStatus::Partial: return "partial";
    case TrainingRecoveryStatus::Exact: return "exact";
    }
    return "unknown";
}

struct SynthesisStats {
    std::uint64_t compositionAttempts = 0;
    std::uint64_t uniqueStructures = 0;
    std::uint64_t duplicateStructures = 0;
    std::uint64_t candidateRollouts = 0;
    std::uint64_t trainingPoints = 0;
    std::uint64_t transitionSteps = 0;
    std::uint64_t nodeVisits = 0;
    std::uint64_t arithmeticOperations = 0;
    std::uint64_t invalidRollouts = 0;
    std::uint64_t resourceLimitedRollouts = 0;
    std::size_t estimatedStoredBytes = 0;
    std::size_t selectedModelBytes = 0;
    std::size_t estimatedPeakBytes = 0;
    std::uint64_t selectedModelReplacements = 0;
    std::size_t maximumNodesReached = 0;
};

struct SynthesisResult {
    std::optional<DiscreteExpression> transition;
    std::string canonicalTransition;
    TrainingRecoveryStatus trainingRecovery = TrainingRecoveryStatus::None;
    std::size_t matchedTrainingPoints = 0;
    std::size_t expressionNodes = 0;
    SynthesisStats stats;
    SynthesisStop stop = SynthesisStop::NodeLimit;
};

namespace detail {
inline bool synthesisOperator(DiscreteKind kind) {
    switch (kind) {
    case DiscreteKind::Add:
    case DiscreteKind::Subtract:
    case DiscreteKind::Multiply:
    case DiscreteKind::Modulo:
        return true;
    default:
        return false;
    }
}

inline std::size_t retainedEstimate(const DiscreteExpression &expression, std::string_view text) {
    // Conservative payload estimate for owned nodes/metadata, the identity
    // string, and a hash-table entry. Includes separate tree/metadata ownership, identity
    // string allocation, hash node/bucket share, and deque block overhead.
    constexpr std::size_t overhead = sizeof(DiscreteExpression) + sizeof(std::string) * 2 +
        sizeof(void *) * 16 + 256;
    const auto nodes = expression.nodeCount();
    const auto perNode = DiscreteExpression::storageBytesPerNode();
    if (text.size() > (std::numeric_limits<std::size_t>::max() - overhead) / 2)
        return std::numeric_limits<std::size_t>::max();
    const auto textBytes = text.size() * 2;
    if (textBytes > std::numeric_limits<std::size_t>::max() - overhead ||
        nodes > (std::numeric_limits<std::size_t>::max() - textBytes - overhead) / perNode)
        return std::numeric_limits<std::size_t>::max();
    return nodes * perNode + textBytes + overhead;
}

inline std::size_t selectedEstimate(const DiscreteExpression &expression, std::string_view text) {
    constexpr std::size_t overhead = sizeof(DiscreteExpression) + sizeof(std::string) + 64;
    const auto nodes = expression.nodeCount();
    const auto perNode = DiscreteExpression::storageBytesPerNode();
    if (text.size() > std::numeric_limits<std::size_t>::max() - overhead ||
        nodes > (std::numeric_limits<std::size_t>::max() - text.size() - overhead) / perNode)
        return std::numeric_limits<std::size_t>::max();
    return nodes * perNode + text.size() + overhead;
}

inline bool checkedCounterAdd(std::uint64_t &target, std::size_t amount) {
    if (amount > std::numeric_limits<std::uint64_t>::max() - target) return false;
    target += static_cast<std::uint64_t>(amount);
    return true;
}
} // namespace detail

inline SynthesisResult synthesizeRecurrenceTransition(
    std::size_t order, const std::vector<std::int64_t> &initial,
    const std::vector<SequencePoint> &training,
    const RecurrenceSynthesisConfig &config = {}) {
    if ((order != 1 && order != 2) || initial.size() != order || training.size() <= order)
        throw std::invalid_argument("recurrence synthesis requires order one/two, matching initial state, and training points");
    genetik_discrete_detail::validateLimits(config.expressionLimits);
    if (!config.maximumNodes || config.maximumNodes > 7 ||
        config.maximumNodes > config.expressionLimits.maxNodes || !config.maximumStructures ||
        config.maximumStructures > 10000000 || config.maximumCompositionAttempts > 100000000 ||
        !config.maximumCompositionAttempts || !config.maximumTrainingPointEvaluations ||
        config.maximumTrainingPointEvaluations > 100000000 || training.size() > 1000000 ||
        !config.maximumTransitionSteps || !config.maximumNodeVisits ||
        !config.maximumArithmeticOperations || !config.maximumEstimatedStorageBytes ||
        config.maximumTransitionSteps > 1000000000ULL ||
        config.maximumNodeVisits > 1000000000000ULL ||
        config.maximumArithmeticOperations > 1000000000000ULL ||
        config.maximumEstimatedStorageBytes > (std::size_t{1} << 30) ||
        !config.maximumSteps || config.maximumSteps > 1000000000)
        throw std::invalid_argument("recurrence synthesis limits must be positive and bounded");
    if (order + 1 > config.expressionLimits.maxVariables)
        throw std::invalid_argument("recurrence bindings exceed expression variable limit");
    for (std::size_t i = 0; i < training.size(); ++i) {
        if (training[i].index != static_cast<std::int64_t>(i))
            throw std::invalid_argument("training points must be contiguous from index zero");
        if (i < order && training[i].value != initial[i])
            throw std::invalid_argument("training prefix differs from immutable initial state");
        if (static_cast<std::uint64_t>(training[i].index) > config.maximumSteps + order - 1)
            throw std::invalid_argument("training index exceeds recurrence step limit");
    }
    if (config.constants.size() > config.expressionLimits.maxConstants)
        throw std::invalid_argument("constant vocabulary exceeds expression limit");
    for (std::size_t i = 0; i < config.constants.size(); ++i) {
        if (genetik_discrete_detail::magnitude(config.constants[i]) >
            config.expressionLimits.maxConstantMagnitude)
            throw std::invalid_argument("constant exceeds literal magnitude limit");
        if (std::find(config.constants.begin(), config.constants.begin() + static_cast<std::ptrdiff_t>(i),
                      config.constants[i]) != config.constants.begin() + static_cast<std::ptrdiff_t>(i))
            throw std::invalid_argument("duplicate constant terminal");
    }
    for (std::size_t i = 0; i < config.operators.size(); ++i) {
        const auto kind = config.operators[i];
        if (!detail::synthesisOperator(kind) || (discreteArity(kind) != 1 && discreteArity(kind) != 2))
            throw std::invalid_argument("grammar contains an unsupported operator");
        if (std::find(config.operators.begin(), config.operators.begin() + static_cast<std::ptrdiff_t>(i), kind) !=
            config.operators.begin() + static_cast<std::ptrdiff_t>(i))
            throw std::invalid_argument("duplicate operator");
    }

    SynthesisResult result;
    const auto dimensions = order + 1; // x0 is the zero-based step; x1.. are oldest-to-newest state.
    std::vector<std::deque<DiscreteExpression>> bySize(config.maximumNodes + 1);
    std::unordered_set<std::string> identities;

    const auto score = [&](const DiscreteExpression &candidate) -> bool {
        ++result.stats.candidateRollouts;
        std::size_t exactPoints = 0;
        for (const auto &point : training) {
            if (config.cancelled && config.cancelled->load(std::memory_order_relaxed)) {
                result.stop = SynthesisStop::Cancelled;
                return true;
            }
            if (result.stats.trainingPoints == config.maximumTrainingPointEvaluations) {
                result.stop = SynthesisStop::TrainingPointLimit;
                return true;
            }
            ++result.stats.trainingPoints;
            const auto remainingSteps = config.maximumTransitionSteps -
                std::min(result.stats.transitionSteps, config.maximumTransitionSteps);
            const auto boundedSteps = static_cast<std::size_t>(std::min<std::uint64_t>(
                config.maximumSteps, remainingSteps));
            IntegerRecurrence model(0, initial, candidate,
                {boundedSteps, config.evaluationLimits.maxWorkspaceBytes});
            auto evaluation = config.evaluationLimits;
            evaluation.maxNodeVisits = static_cast<std::size_t>(std::min<std::uint64_t>(
                evaluation.maxNodeVisits, config.maximumNodeVisits -
                    std::min(result.stats.nodeVisits, config.maximumNodeVisits)));
            evaluation.maxOperations = static_cast<std::size_t>(std::min<std::uint64_t>(
                evaluation.maxOperations, config.maximumArithmeticOperations -
                    std::min(result.stats.arithmeticOperations, config.maximumArithmeticOperations)));
            evaluation.maxAggregateIterations = static_cast<std::size_t>(std::min<std::uint64_t>(
                evaluation.maxAggregateIterations, remainingSteps));
            // evaluate(index) always starts with the declared initial values and
            // autonomously generates every intervening transition.
            const auto value = model.evaluate(point.index, {}, evaluation, config.cancelled);
            if (!detail::checkedCounterAdd(result.stats.transitionSteps, value.iterations) ||
                result.stats.transitionSteps > config.maximumTransitionSteps) {
                result.stop = SynthesisStop::TransitionLimit;
                return true;
            }
            if (!detail::checkedCounterAdd(result.stats.nodeVisits, value.nodeVisits) ||
                result.stats.nodeVisits > config.maximumNodeVisits) {
                result.stop = SynthesisStop::NodeVisitLimit;
                return true;
            }
            if (!detail::checkedCounterAdd(result.stats.arithmeticOperations, value.operations) ||
                result.stats.arithmeticOperations > config.maximumArithmeticOperations) {
                result.stop = SynthesisStop::ArithmeticWorkLimit;
                return true;
            }
            if (!value.valid()) {
                if (value.status == DiscreteEvaluationStatus::ResourceExhausted ||
                    value.status == DiscreteEvaluationStatus::Cancelled) {
                    ++result.stats.resourceLimitedRollouts;
                    if (value.status == DiscreteEvaluationStatus::Cancelled)
                        result.stop = SynthesisStop::Cancelled;
                    else if (result.stats.transitionSteps >= config.maximumTransitionSteps)
                        result.stop = SynthesisStop::TransitionLimit;
                    else if (result.stats.nodeVisits >= config.maximumNodeVisits)
                        result.stop = SynthesisStop::NodeVisitLimit;
                    else if (result.stats.arithmeticOperations >= config.maximumArithmeticOperations)
                        result.stop = SynthesisStop::ArithmeticWorkLimit;
                    else
                        result.stop = SynthesisStop::EvaluationResourceLimit;
                    return true;
                }
                ++result.stats.invalidRollouts;
                return false;
            }
            if (value.value->type() == DiscreteType::Int64 && value.value->integer() == point.value)
                ++exactPoints;
            else
                break;
        }
        // Keep the best valid prefix-matching partial model for useful
        // diagnostics; exact recovery always supersedes it and stops search.
        if (exactPoints > order && exactPoints > result.matchedTrainingPoints &&
            exactPoints < training.size()) {
            const auto text = candidate.toString();
            const auto selected = detail::selectedEstimate(candidate, text);
            const auto retained = result.stats.estimatedStoredBytes;
            const auto oldSelected = result.stats.selectedModelBytes;
            if (retained > config.maximumEstimatedStorageBytes ||
                oldSelected > config.maximumEstimatedStorageBytes - retained ||
                selected > config.maximumEstimatedStorageBytes - retained - oldSelected) {
                result.stop = SynthesisStop::StorageLimit;
                return true;
            }
            // Replacement transiently keeps both selected copies alive (the
            // assignment's by-value temporary owns the former model).
            result.stats.estimatedPeakBytes = std::max(result.stats.estimatedPeakBytes,
                retained + oldSelected + selected);
            if (result.transition) ++result.stats.selectedModelReplacements;
            result.transition = candidate;
            result.canonicalTransition = text;
            result.trainingRecovery = TrainingRecoveryStatus::Partial;
            result.matchedTrainingPoints = exactPoints;
            result.expressionNodes = candidate.nodeCount();
            result.stats.selectedModelBytes = selected;
        }
        if (exactPoints == training.size()) {
            const auto text = candidate.toString();
            const auto selected = detail::selectedEstimate(candidate, text);
            const auto retained = result.stats.estimatedStoredBytes;
            const auto oldSelected = result.stats.selectedModelBytes;
            if (retained > config.maximumEstimatedStorageBytes ||
                oldSelected > config.maximumEstimatedStorageBytes - retained ||
                selected > config.maximumEstimatedStorageBytes - retained - oldSelected) {
                result.stop = SynthesisStop::StorageLimit;
                return true;
            }
            result.stats.estimatedPeakBytes = std::max(result.stats.estimatedPeakBytes,
                retained + oldSelected + selected);
            if (result.transition) ++result.stats.selectedModelReplacements;
            result.transition = candidate;
            result.canonicalTransition = text;
            result.trainingRecovery = TrainingRecoveryStatus::Exact;
            result.matchedTrainingPoints = exactPoints;
            result.expressionNodes = candidate.nodeCount();
            result.stats.selectedModelBytes = selected;
            result.stop = SynthesisStop::ExactTrainingMatch;
            return true;
        }
        return false;
    };

    const auto consider = [&](DiscreteExpression candidate) -> bool {
        const auto text = candidate.toString();
        if (!identities.insert(text).second) {
            ++result.stats.duplicateStructures;
            return false;
        }
        if (result.stats.uniqueStructures == config.maximumStructures) {
            identities.erase(text);
            result.stop = SynthesisStop::StructureLimit;
            return true;
        }
        const auto retained = detail::retainedEstimate(candidate, text);
        const auto selected = result.stats.selectedModelBytes;
        if (result.stats.estimatedStoredBytes > config.maximumEstimatedStorageBytes ||
            selected > config.maximumEstimatedStorageBytes - result.stats.estimatedStoredBytes ||
            retained > config.maximumEstimatedStorageBytes - result.stats.estimatedStoredBytes - selected) {
            identities.erase(text);
            result.stop = SynthesisStop::StorageLimit;
            return true;
        }
        ++result.stats.uniqueStructures;
        result.stats.estimatedStoredBytes += retained;
        result.stats.estimatedPeakBytes = std::max(result.stats.estimatedPeakBytes,
            result.stats.estimatedStoredBytes + selected);
        result.stats.maximumNodesReached = std::max(result.stats.maximumNodesReached, candidate.nodeCount());
        const auto size = candidate.nodeCount();
        if (score(candidate)) return true;
        bySize[size].push_back(std::move(candidate));
        return false;
    };

    for (std::size_t variable = 0; variable < dimensions; ++variable) {
        if (result.stats.compositionAttempts >= config.maximumCompositionAttempts) {
            result.stop = SynthesisStop::CompositionLimit;
            return result;
        }
        ++result.stats.compositionAttempts;
        if (consider(DiscreteExpression::variable(variable, dimensions, config.expressionLimits))) return result;
    }
    for (const auto constant : config.constants) {
        if (result.stats.compositionAttempts >= config.maximumCompositionAttempts) {
            result.stop = SynthesisStop::CompositionLimit;
            return result;
        }
        ++result.stats.compositionAttempts;
        if (consider(DiscreteExpression::integer(constant, dimensions, config.expressionLimits))) return result;
    }

    for (std::size_t nodes = 2; nodes <= config.maximumNodes; ++nodes) {
        for (const auto kind : config.operators) {
            const auto arity = discreteArity(kind);
            if (arity == 1) {
                for (const auto &child : bySize[nodes - 1]) {
                    if (result.stats.compositionAttempts >= config.maximumCompositionAttempts) {
                        result.stop = SynthesisStop::CompositionLimit;
                        return result;
                    }
                    ++result.stats.compositionAttempts;
                    try {
                        if (consider(DiscreteExpression::unary(kind, child, config.expressionLimits))) return result;
                    } catch (const std::invalid_argument &) {
                    } catch (const std::length_error &) {
                    }
                }
            } else {
                for (std::size_t leftNodes = 1; leftNodes + 1 < nodes; ++leftNodes) {
                    const auto rightNodes = nodes - leftNodes - 1;
                    for (const auto &left : bySize[leftNodes]) {
                        for (const auto &right : bySize[rightNodes]) {
                            if (result.stats.compositionAttempts >= config.maximumCompositionAttempts) {
                                result.stop = SynthesisStop::CompositionLimit;
                                return result;
                            }
                            ++result.stats.compositionAttempts;
                            try {
                                if (consider(DiscreteExpression::binary(kind, left, right,
                                                                        config.expressionLimits))) return result;
                            } catch (const std::invalid_argument &) {
                            } catch (const std::length_error &) {
                            }
                        }
                    }
                }
            }
        }
    }
    result.stop = SynthesisStop::NodeLimit;
    return result;
}

enum class AssessmentStatus { ExactHeldOut, PartialHeldOut, InvalidCandidate, ResourceExhausted, NoCandidate };
constexpr std::string_view toString(AssessmentStatus status) noexcept {
    switch (status) {
    case AssessmentStatus::ExactHeldOut: return "exact_held_out";
    case AssessmentStatus::PartialHeldOut: return "partial_held_out";
    case AssessmentStatus::InvalidCandidate: return "invalid_candidate";
    case AssessmentStatus::ResourceExhausted: return "resource_exhausted";
    case AssessmentStatus::NoCandidate: return "no_candidate";
    }
    return "unknown";
}
struct RecurrenceAssessmentConfig {
    std::size_t maximumObservations = 1000000;
    std::uint64_t maximumTransitionSteps = 10000000;
    std::uint64_t maximumNodeVisits = 20000000;
    std::uint64_t maximumArithmeticOperations = 10000000;
    std::size_t maximumSteps = 1000;
    DiscreteEvaluationLimits evaluationLimits{};
    const std::atomic<bool> *cancelled = nullptr;
};
struct RecurrenceAssessment {
    AssessmentStatus status = AssessmentStatus::NoCandidate;
    std::size_t matchedObservations = 0;
    std::size_t observationCount = 0;
    std::uint64_t transitionSteps = 0;
    std::uint64_t nodeVisits = 0;
    std::uint64_t arithmeticOperations = 0;
    std::uint64_t invalidEvaluations = 0;
    std::uint64_t resourceLimitedEvaluations = 0;
    std::string diagnostic;
};

// Post-selection only: this function never mutates the synthesis result or
// influences candidate ordering/selection.
inline RecurrenceAssessment assessRecurrenceTransition(
    const SynthesisResult &selected, std::size_t order,
    const std::vector<std::int64_t> &initial,
    const std::vector<SequencePoint> &heldOut,
    const RecurrenceAssessmentConfig &config = {}) {
    RecurrenceAssessment result;
    result.observationCount = heldOut.size();
    if ((order != 1 && order != 2) || initial.size() != order || heldOut.empty() ||
        !config.maximumObservations || config.maximumObservations > 10000000 ||
        heldOut.size() > config.maximumObservations ||
        !config.maximumTransitionSteps || config.maximumTransitionSteps > 1000000000ULL ||
        !config.maximumNodeVisits || config.maximumNodeVisits > 1000000000000ULL ||
        !config.maximumArithmeticOperations || config.maximumArithmeticOperations > 1000000000000ULL ||
        !config.maximumSteps || config.maximumSteps > 1000000000 ||
        !config.evaluationLimits.maxWorkspaceBytes ||
        config.evaluationLimits.maxIterationDepth > 128 ||
        config.evaluationLimits.maxIterationsPerOperation > 1000000000 ||
        config.evaluationLimits.maxAggregateIterations > 1000000000)
        throw std::invalid_argument("invalid recurrence assessment data or limits");
    std::int64_t previousIndex = -1;
    for (const auto &point : heldOut) {
        if (point.index < 0 || point.index <= previousIndex ||
            static_cast<std::uint64_t>(point.index) > config.maximumSteps + order - 1)
            throw std::invalid_argument("held-out indices must increase within recurrence step limit");
        previousIndex = point.index;
    }
    if (!selected.transition) {
        result.status = selected.stop == SynthesisStop::NodeLimit
            ? AssessmentStatus::NoCandidate : AssessmentStatus::ResourceExhausted;
        result.diagnostic = result.status == AssessmentStatus::NoCandidate
            ? "synthesis completed without a candidate transition"
            : "synthesis stopped before selecting a complete candidate";
        return result;
    }
    if (selected.transition->dimensions() != order + 1 ||
        selected.transition->type() != DiscreteType::Int64 ||
        !selected.transition->outerBindings().empty()) {
        result.status = AssessmentStatus::InvalidCandidate;
        result.diagnostic = "selected transition does not match recurrence schema";
        return result;
    }
    std::optional<IntegerRecurrence> model;
    try {
        model.emplace(0, initial, *selected.transition,
            IntegerRecurrenceLimits{config.maximumSteps, config.evaluationLimits.maxWorkspaceBytes});
    } catch (const std::length_error &error) {
        result.status = AssessmentStatus::ResourceExhausted;
        result.diagnostic = error.what();
        return result;
    } catch (const std::invalid_argument &error) {
        result.status = AssessmentStatus::InvalidCandidate;
        result.diagnostic = error.what();
        return result;
    }
    for (const auto &point : heldOut) {
        if (config.cancelled && config.cancelled->load(std::memory_order_relaxed)) {
            result.status = AssessmentStatus::ResourceExhausted;
            result.diagnostic = "assessment cancelled";
            return result;
        }
        const auto remainingSteps = config.maximumTransitionSteps -
            std::min(result.transitionSteps, config.maximumTransitionSteps);
        auto evaluation = config.evaluationLimits;
        evaluation.maxNodeVisits = static_cast<std::size_t>(std::min<std::uint64_t>(
            evaluation.maxNodeVisits, config.maximumNodeVisits -
                std::min(result.nodeVisits, config.maximumNodeVisits)));
        evaluation.maxOperations = static_cast<std::size_t>(std::min<std::uint64_t>(
            evaluation.maxOperations, config.maximumArithmeticOperations -
                std::min(result.arithmeticOperations, config.maximumArithmeticOperations)));
        evaluation.maxAggregateIterations = static_cast<std::size_t>(
            std::min<std::uint64_t>(evaluation.maxAggregateIterations, remainingSteps));
        const auto value = model->evaluate(point.index, {}, evaluation, config.cancelled);
        if (!detail::checkedCounterAdd(result.transitionSteps, value.iterations) ||
            !detail::checkedCounterAdd(result.nodeVisits, value.nodeVisits) ||
            !detail::checkedCounterAdd(result.arithmeticOperations, value.operations)) {
            result.status = AssessmentStatus::ResourceExhausted;
            result.diagnostic = "assessment work counter exhausted";
            return result;
        }
        if (!value.valid()) {
            if (value.status == DiscreteEvaluationStatus::ResourceExhausted ||
                value.status == DiscreteEvaluationStatus::Cancelled) {
                ++result.resourceLimitedEvaluations;
                result.status = AssessmentStatus::ResourceExhausted;
                result.diagnostic = value.reason;
            } else {
                ++result.invalidEvaluations;
                result.status = AssessmentStatus::InvalidCandidate;
                result.diagnostic = value.reason;
            }
            return result;
        }
        if (value.value->type() == DiscreteType::Int64 && value.value->integer() == point.value)
            ++result.matchedObservations;
    }
    result.status = result.matchedObservations == heldOut.size()
        ? AssessmentStatus::ExactHeldOut : AssessmentStatus::PartialHeldOut;
    result.diagnostic = result.status == AssessmentStatus::ExactHeldOut
        ? "all held-out observations recovered exactly"
        : "held-out observations only partially recovered";
    return result;
}

} // namespace genetik::constructive
