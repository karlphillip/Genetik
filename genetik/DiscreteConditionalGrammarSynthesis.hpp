#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "DiscreteConditionalComposition.hpp"

// Deterministic finite-grammar construction for the optional conditional API.
// Grammar enumeration is independent of GA state, RNG state and target models.
struct DiscreteConditionalGrammar {
    std::vector<std::size_t> variables; // original input indices, in enumeration order
    std::vector<std::int64_t> branchConstants;
    std::vector<std::int64_t> predicateConstants;
    std::vector<bool> booleanConstants{false, true};
    std::vector<DiscreteKind> branchOperators{DiscreteKind::Add, DiscreteKind::Subtract,
                                              DiscreteKind::Multiply, DiscreteKind::Negate,
                                              DiscreteKind::Modulo};
    std::vector<DiscreteKind> predicateOperators{
        DiscreteKind::Add,         DiscreteKind::Subtract,    DiscreteKind::Multiply,
        DiscreteKind::Negate,      DiscreteKind::Modulo,      DiscreteKind::Equal,
        DiscreteKind::NotEqual,    DiscreteKind::LessThan,    DiscreteKind::LessEqual,
        DiscreteKind::GreaterThan, DiscreteKind::GreaterEqual};
    std::size_t maximumPredicateNodes = 5;
    std::size_t maximumBranchNodes = 5;
    std::size_t maximumExpressionDepth = 8;
    DiscreteExpressionLimits expressionLimits{};
};

struct DiscreteConditionalGrammarLimits {
    std::size_t maximumPoolExpressions = 100000;
    std::size_t maximumConstructionAttempts = 1000000;
    std::size_t maximumCandidateCombinations = 1000000;
    std::size_t maximumConditionalNodes = 128;
    std::size_t maximumConditionalDepth = 32;
    std::size_t maximumEvaluationWork = 10000000;
    std::size_t maximumEstimatedBytes = 64 * 1024 * 1024;
    DiscreteEvaluationLimits evaluation{};
};

struct DiscreteConditionalGrammarWork {
    std::size_t specificationValidationWork = 0;
    std::size_t constructionAttempts = 0;
    std::size_t generatedExpressions = 0;
    std::size_t equivalentExpressions = 0;
    std::size_t invalidExpressions = 0;
    std::size_t predicatePoolSize = 0;
    std::size_t branchPoolSize = 0;
    std::size_t poolTraceEvaluationWork = 0;
    std::size_t poolEvaluationWork = 0;
    std::size_t estimatedPeakBytes = 0;
    DiscreteConditionalCompositionWork composition;
};

enum class DiscreteConditionalGrammarStatus {
    ExactTraining,
    Partial,
    Invalid,
    ResourceExhausted,
    Cancelled
};

struct DiscreteConditionalGrammarResult {
    DiscreteConditionalGrammarStatus status = DiscreteConditionalGrammarStatus::Invalid;
    std::optional<DiscreteExpression> expression;
    std::size_t matchingRows = 0;
    std::size_t totalRows = 0;
    DiscreteConditionalGrammarWork work;
    std::string diagnostic;
};

namespace genetik_conditional_grammar_detail {
inline bool addWithin(std::size_t a, std::size_t b, std::size_t cap) noexcept {
    return b <= cap && a <= cap - b;
}

inline bool integerOutputOperator(DiscreteKind kind) noexcept {
    return kind == DiscreteKind::Add || kind == DiscreteKind::Subtract ||
           kind == DiscreteKind::Multiply || kind == DiscreteKind::Negate ||
           kind == DiscreteKind::Modulo;
}

inline bool booleanOutputOperator(DiscreteKind kind) noexcept {
    return kind >= DiscreteKind::Equal && kind <= DiscreteKind::GreaterEqual;
}

inline std::size_t arity(DiscreteKind kind) noexcept {
    return discreteArity(kind);
}

inline bool expressionStorageEstimate(std::size_t nodes, std::size_t cap,
                                      std::size_t& bytes) noexcept {
    constexpr auto fixedBytes = sizeof(DiscreteExpression) + 128;
    if (nodes > cap / DiscreteExpression::storageBytesPerNode())
        return false;
    const auto nodeBytes = nodes * DiscreteExpression::storageBytesPerNode();
    if (!addWithin(nodeBytes, fixedBytes, cap))
        return false;
    bytes = nodeBytes + fixedBytes;
    return true;
}

inline bool isComparison(DiscreteKind kind) noexcept {
    return booleanOutputOperator(kind);
}

struct PoolKey {
    bool predicate;
    DiscreteType type;
    std::size_t nodes;
    bool operator<(const PoolKey& other) const noexcept {
        return std::tie(predicate, type, nodes) <
               std::tie(other.predicate, other.type, other.nodes);
    }
};

class Enumerator {
public:
    Enumerator(const DiscreteConditionalGrammar& grammar,
               const DiscreteConditionalGrammarLimits& limits, std::size_t dimensions,
               const std::atomic<bool>* cancellation)
        : grammar_(grammar), limits_(limits), dimensions_(dimensions), cancellation_(cancellation) {
        if (grammar.maximumPredicateNodes > maximumRecursiveNodes ||
            grammar.maximumBranchNodes > maximumRecursiveNodes)
            exhausted_ = true;
    }

    bool cancelled() const noexcept {
        return cancellation_ && cancellation_->load(std::memory_order_relaxed);
    }

    bool exhausted() const noexcept {
        return exhausted_;
    }
    std::size_t peakBytes() const noexcept {
        return peakBytes_;
    }
    std::size_t attempts() const noexcept {
        return attempts_;
    }
    std::size_t generated() const noexcept {
        return generated_;
    }

    const std::vector<DiscreteExpression>& enumerate(DiscreteType type, std::size_t nodes,
                                                     bool predicate) {
        const PoolKey key{predicate, type, nodes};
        if (const auto found = pools_.find(key); found != pools_.end())
            return found->second;
        constexpr std::size_t mapEntryBytes = 128;
        if (!addWithin(currentBytes_, mapEntryBytes, limits_.maximumEstimatedBytes)) {
            exhausted_ = true;
            static const std::vector<DiscreteExpression> empty;
            return empty;
        }
        currentBytes_ += mapEntryBytes;
        peakBytes_ = std::max(peakBytes_, currentBytes_);
        auto [entry, inserted] = pools_.emplace(key, std::vector<DiscreteExpression>{});
        (void)inserted;
        auto& result = entry->second;
        if (cancelled() || exhausted_ || !nodes)
            return result;

        if (nodes == 1) {
            if (type == DiscreteType::Int64) {
                for (const auto variable : grammar_.variables) {
                    if (!admitTransient(1))
                        return result;
                    if (!append(result, DiscreteExpression::variable(variable, dimensions_,
                                                                     grammar_.expressionLimits)))
                        return result;
                }
                const auto& constants =
                    predicate ? grammar_.predicateConstants : grammar_.branchConstants;
                for (const auto value : constants) {
                    if (!admitTransient(1))
                        return result;
                    if (!append(result, DiscreteExpression::integer(value, dimensions_,
                                                                    grammar_.expressionLimits)))
                        return result;
                }
            } else {
                for (const auto value : grammar_.booleanConstants) {
                    if (!admitTransient(1))
                        return result;
                    if (!append(result, DiscreteExpression::boolean(value, dimensions_,
                                                                    grammar_.expressionLimits)))
                        return result;
                }
            }
            return result;
        }

        const auto& productions =
            predicate ? grammar_.predicateOperators : grammar_.branchOperators;
        for (const auto kind : productions) {
            if (cancelled() || exhausted_)
                return result;
            const bool returnsBool = isComparison(kind);
            if (returnsBool != (type == DiscreteType::Bool))
                continue;
            const auto operandCount = arity(kind);
            if (!operandCount || operandCount > 2 || (!integerOutputOperator(kind) && !returnsBool))
                continue;
            if (operandCount == 1) {
                for (const auto& child : enumerate(DiscreteType::Int64, nodes - 1, predicate)) {
                    if (cancelled() || exhausted_)
                        return result;
                    if (!attempt())
                        return result;
                    if (!admitTransient(nodes))
                        return result;
                    try {
                        auto expression =
                            DiscreteExpression::unary(kind, child, grammar_.expressionLimits);
                        if (expression.depth() <= grammar_.maximumExpressionDepth &&
                            !append(result, std::move(expression)))
                            return result;
                    } catch (const std::length_error&) {
                        ++invalid_;
                    } catch (const std::invalid_argument&) {
                        ++invalid_;
                    }
                }
                continue;
            }

            const auto leftType = DiscreteType::Int64;
            const auto rightType = DiscreteType::Int64;
            for (std::size_t leftNodes = 1; leftNodes < nodes - 1; ++leftNodes) {
                const auto rightNodes = nodes - 1 - leftNodes;
                const auto& leftPool = enumerate(leftType, leftNodes, predicate);
                const auto& rightPool = enumerate(rightType, rightNodes, predicate);
                for (const auto& left : leftPool) {
                    for (const auto& right : rightPool) {
                        if (cancelled() || exhausted_)
                            return result;
                        if (!attempt())
                            return result;
                        if (!admitTransient(nodes))
                            return result;
                        try {
                            auto expression = DiscreteExpression::binary(kind, left, right,
                                                                         grammar_.expressionLimits);
                            if (expression.depth() <= grammar_.maximumExpressionDepth &&
                                !append(result, std::move(expression)))
                                return result;
                        } catch (const std::length_error&) {
                            ++invalid_;
                        } catch (const std::invalid_argument&) {
                            ++invalid_;
                        }
                    }
                }
            }
        }
        return result;
    }

    std::vector<DiscreteExpression> all(DiscreteType type, std::size_t maximumNodes,
                                        bool predicate) {
        std::vector<DiscreteExpression> output;
        for (std::size_t nodes = 1; nodes <= maximumNodes; ++nodes) {
            if (cancelled() || exhausted_)
                break;
            const auto& sizePool = enumerate(type, nodes, predicate);
            for (const auto& expression : sizePool) {
                if (cancelled())
                    return output;
                if (!append(output, expression))
                    return output;
            }
        }
        return output;
    }

    std::size_t invalid() const noexcept {
        return invalid_;
    }

private:
    static constexpr std::size_t maximumRecursiveNodes = 256;

    bool admitTransient(std::size_t nodes) {
        std::size_t bytes = 0;
        if (!expressionStorageEstimate(nodes, limits_.maximumEstimatedBytes, bytes) ||
            !addWithin(currentBytes_, bytes, limits_.maximumEstimatedBytes)) {
            exhausted_ = true;
            return false;
        }
        peakBytes_ = std::max(peakBytes_, currentBytes_ + bytes);
        return true;
    }

    bool attempt() {
        if (attempts_ == limits_.maximumConstructionAttempts) {
            exhausted_ = true;
            return false;
        }
        ++attempts_;
        return true;
    }

    bool append(std::vector<DiscreteExpression>& into, DiscreteExpression&& expression) {
        if (cancelled())
            return false;
        if (generated_ == limits_.maximumPoolExpressions) {
            exhausted_ = true;
            return false;
        }
        std::size_t bytes = 0;
        if (!expressionStorageEstimate(expression.nodeCount(), limits_.maximumEstimatedBytes,
                                       bytes)) {
            exhausted_ = true;
            return false;
        }
        // Account both the memoized size pools and the final output pool copies.
        constexpr std::size_t multiplier = 2;
        if (bytes > limits_.maximumEstimatedBytes / multiplier ||
            !addWithin(currentBytes_, bytes * multiplier, limits_.maximumEstimatedBytes)) {
            exhausted_ = true;
            return false;
        }
        currentBytes_ += bytes * multiplier;
        peakBytes_ = std::max(peakBytes_, currentBytes_);
        into.push_back(std::move(expression));
        ++generated_;
        return true;
    }

    bool append(std::vector<DiscreteExpression>& into, const DiscreteExpression& expression) {
        if (!admitTransient(expression.nodeCount()))
            return false;
        return append(into, DiscreteExpression(expression));
    }

private:
    const DiscreteConditionalGrammar& grammar_;
    const DiscreteConditionalGrammarLimits& limits_;
    std::size_t dimensions_;
    const std::atomic<bool>* cancellation_;
    std::map<PoolKey, std::vector<DiscreteExpression>> pools_;
    std::size_t attempts_ = 0, generated_ = 0, invalid_ = 0, currentBytes_ = 0, peakBytes_ = 0;
    bool exhausted_ = false;
};

inline bool validGrammar(const DiscreteSpecification& specification,
                         const DiscreteConditionalGrammar& grammar,
                         const DiscreteConditionalGrammarLimits& limits, std::string& diagnostic,
                         const std::atomic<bool>* cancellation) {
    constexpr std::size_t maximumGrammarSourceItems = 4096;
    if (specification.observations().empty() || !limits.maximumPoolExpressions ||
        !limits.maximumConstructionAttempts || !limits.maximumCandidateCombinations ||
        !limits.maximumConditionalNodes || !limits.maximumConditionalDepth ||
        !limits.maximumEvaluationWork || !limits.maximumEstimatedBytes ||
        !grammar.maximumPredicateNodes || !grammar.maximumBranchNodes ||
        !grammar.maximumExpressionDepth ||
        specification.dimensions() > grammar.expressionLimits.maxVariables ||
        std::max(grammar.maximumPredicateNodes, grammar.maximumBranchNodes) >
            grammar.expressionLimits.maxNodes ||
        grammar.maximumExpressionDepth > grammar.expressionLimits.maxDepth) {
        diagnostic = "empty training specification or zero/inconsistent grammar limit";
        return false;
    }
    if (grammar.variables.size() > grammar.expressionLimits.maxVariables ||
        grammar.variables.size() > maximumGrammarSourceItems ||
        grammar.branchConstants.size() > grammar.expressionLimits.maxConstants ||
        grammar.branchConstants.size() > maximumGrammarSourceItems ||
        grammar.predicateConstants.size() > grammar.expressionLimits.maxConstants ||
        grammar.predicateConstants.size() > maximumGrammarSourceItems ||
        grammar.booleanConstants.size() > 2 ||
        grammar.booleanConstants.size() >
            grammar.expressionLimits.maxConstants - std::min(grammar.expressionLimits.maxConstants,
                                                             grammar.predicateConstants.size()) ||
        grammar.branchOperators.size() > 5 || grammar.predicateOperators.size() > 11 ||
        !grammar.expressionLimits.maxNodes || !grammar.expressionLimits.maxDepth ||
        !grammar.expressionLimits.maxStorageBytes ||
        grammar.expressionLimits.maxConstantMagnitude > (std::uint64_t{1} << 63) ||
        grammar.expressionLimits.maxIterationDepth > 128 ||
        grammar.expressionLimits.maxBinders > 4096) {
        diagnostic = "grammar source exceeds expression limits";
        return false;
    }
    if (cancellation && cancellation->load(std::memory_order_relaxed)) {
        diagnostic = "conditional grammar validation cancelled";
        return false;
    }
    for (const auto& row : specification.observations()) {
        if (cancellation && cancellation->load(std::memory_order_relaxed)) {
            diagnostic = "conditional grammar validation cancelled";
            return false;
        }
        if (row.inputs.size() != specification.dimensions() ||
            row.expected.type() != DiscreteType::Int64) {
            diagnostic = "conditional grammar synthesis requires an Int64 specification";
            return false;
        }
    }
    std::set<std::size_t> seenVariables;
    for (const auto variable : grammar.variables) {
        if (cancellation && cancellation->load(std::memory_order_relaxed)) {
            diagnostic = "conditional grammar validation cancelled";
            return false;
        }
        if (variable >= specification.dimensions() || !seenVariables.insert(variable).second) {
            diagnostic = "grammar variable is outside the input schema or duplicated";
            return false;
        }
    }
    const auto validProductions = [&](const auto& operators, bool allowComparisons) {
        std::set<DiscreteKind> seen;
        for (const auto kind : operators) {
            if (cancellation && cancellation->load(std::memory_order_relaxed)) {
                diagnostic = "conditional grammar validation cancelled";
                return false;
            }
            if ((!integerOutputOperator(kind) &&
                 !(allowComparisons && booleanOutputOperator(kind))) ||
                !seen.insert(kind).second)
                return false;
        }
        return true;
    };
    if (!validProductions(grammar.branchOperators, false) ||
        !validProductions(grammar.predicateOperators, true)) {
        diagnostic = "grammar contains an unsupported or duplicate typed production";
        return false;
    }
    const auto validConstants = [&](const auto& constants) {
        std::set<std::int64_t> seen;
        for (const auto value : constants) {
            if (cancellation && cancellation->load(std::memory_order_relaxed)) {
                diagnostic = "conditional grammar validation cancelled";
                return false;
            }
            const auto magnitude =
                value < 0 ? std::uint64_t(-(value + 1)) + 1 : std::uint64_t(value);
            if (magnitude > grammar.expressionLimits.maxConstantMagnitude ||
                !seen.insert(value).second)
                return false;
        }
        return true;
    };
    if (!validConstants(grammar.branchConstants) || !validConstants(grammar.predicateConstants)) {
        diagnostic = "grammar integer constant is outside limits or duplicated";
        return false;
    }
    std::set<bool> seenBooleans;
    for (const auto value : grammar.booleanConstants) {
        if (cancellation && cancellation->load(std::memory_order_relaxed)) {
            diagnostic = "conditional grammar validation cancelled";
            return false;
        }
        if (!seenBooleans.insert(value).second) {
            diagnostic = "grammar boolean constant is duplicated";
            return false;
        }
    }
    return true;
}

inline bool addWork(std::size_t& work, std::size_t amount, std::size_t cap) noexcept {
    if (!addWithin(work, amount, cap))
        return false;
    work += amount;
    return true;
}

inline std::string behaviorKey(const DiscreteExpression& expression,
                               const DiscreteSpecification& specification,
                               const DiscreteEvaluationLimits& limits, std::size_t& work,
                               std::size_t workCap, const std::atomic<bool>* cancellation,
                               bool& valid, bool& exhausted) {
    std::string key;
    key.reserve(specification.observations().size() * 24 + 1);
    key.push_back(expression.type() == DiscreteType::Bool ? 'b' : 'i');
    valid = true;
    for (const auto& row : specification.observations()) {
        if (cancellation && cancellation->load(std::memory_order_relaxed)) {
            exhausted = true;
            return {};
        }
        if (work >= workCap) {
            exhausted = true;
            return {};
        }
        auto perRowLimits = limits;
        const auto remaining = workCap - work;
        const auto visits = remaining / 2;
        perRowLimits.maxNodeVisits = std::min(perRowLimits.maxNodeVisits, visits);
        perRowLimits.maxOperations = std::min(perRowLimits.maxOperations, remaining - visits);
        auto evaluated = expression.evaluate(row.inputs, perRowLimits, cancellation);
        if (evaluated.nodeVisits > std::numeric_limits<std::size_t>::max() - evaluated.operations) {
            exhausted = true;
            return {};
        }
        const auto rowWork = evaluated.nodeVisits + evaluated.operations;
        if (!addWork(work, rowWork, workCap)) {
            exhausted = true;
            return {};
        }
        if (evaluated.status == DiscreteEvaluationStatus::ResourceExhausted) {
            exhausted = true;
            return {};
        }
        if (evaluated.status == DiscreteEvaluationStatus::Cancelled) {
            exhausted = true;
            return {};
        }
        if (!evaluated.valid()) {
            valid = false;
            // Preserve partial validity: a branch may overflow on rows where a
            // lazy conditional never selects it.
            key.push_back('!');
            continue;
        }
        if (evaluated.value->type() == DiscreteType::Bool) {
            key.push_back(evaluated.value->boolean() ? 'B' : 'b');
        } else {
            const auto value = evaluated.value->integer();
            key.push_back('I');
            key.append(std::to_string(value));
            key.push_back(';');
        }
    }
    return key;
}
} // namespace genetik_conditional_grammar_detail

inline DiscreteConditionalGrammarResult synthesizeDiscreteConditionalFromGrammar(
    const DiscreteSpecification& specification, const DiscreteConditionalGrammar& grammar,
    DiscreteConditionalGrammarLimits limits = {}, const std::atomic<bool>* cancellation = nullptr) {
    using namespace genetik_conditional_grammar_detail;
    DiscreteConditionalGrammarResult result;
    result.totalRows = specification.observations().size();
    if (cancellation && cancellation->load(std::memory_order_relaxed)) {
        result.status = DiscreteConditionalGrammarStatus::Cancelled;
        result.diagnostic = "conditional grammar validation cancelled";
        return result;
    }
    if (specification.observations().size() > limits.maximumEvaluationWork) {
        result.status = DiscreteConditionalGrammarStatus::ResourceExhausted;
        result.diagnostic = "specification validation work exceeds the aggregate evaluation limit";
        return result;
    }
    result.work.specificationValidationWork = specification.observations().size();
    result.work.poolEvaluationWork = result.work.specificationValidationWork;
    constexpr std::size_t maximumGrammarSourceItems = 4096;
    if (grammar.variables.size() > maximumGrammarSourceItems ||
        grammar.branchConstants.size() > maximumGrammarSourceItems ||
        grammar.predicateConstants.size() > maximumGrammarSourceItems ||
        grammar.booleanConstants.size() > 2 || grammar.branchOperators.size() > 5 ||
        grammar.predicateOperators.size() > 11) {
        result.diagnostic = "grammar source exceeds fixed validation bounds";
        return result;
    }
    constexpr std::size_t setNodeEstimate = 64;
    constexpr std::size_t setOverheadEstimate = 512;
    const auto maximumConstants =
        std::max(grammar.branchConstants.size(), grammar.predicateConstants.size());
    std::size_t validationBytes = setOverheadEstimate;
    if (grammar.variables.size() >
            (std::numeric_limits<std::size_t>::max() - validationBytes) / setNodeEstimate ||
        !addWithin(validationBytes, grammar.variables.size() * setNodeEstimate,
                   std::numeric_limits<std::size_t>::max()) ||
        maximumConstants >
            (std::numeric_limits<std::size_t>::max() - validationBytes) / setNodeEstimate ||
        !addWithin(validationBytes, maximumConstants * setNodeEstimate,
                   std::numeric_limits<std::size_t>::max()) ||
        !addWithin(validationBytes, 11 * setNodeEstimate,
                   std::numeric_limits<std::size_t>::max())) {
        result.status = DiscreteConditionalGrammarStatus::ResourceExhausted;
        result.diagnostic = "grammar source validation estimate overflow";
        return result;
    }
    result.work.estimatedPeakBytes = validationBytes;
    if (validationBytes > limits.maximumEstimatedBytes) {
        result.status = DiscreteConditionalGrammarStatus::ResourceExhausted;
        result.diagnostic = "grammar source validation memory estimate exhausted";
        return result;
    }
    if (!validGrammar(specification, grammar, limits, result.diagnostic, cancellation)) {
        if (cancellation && cancellation->load(std::memory_order_relaxed))
            result.status = DiscreteConditionalGrammarStatus::Cancelled;
        return result;
    }

    Enumerator enumerator(grammar, limits, specification.dimensions(), cancellation);
    auto predicates = enumerator.all(DiscreteType::Bool, grammar.maximumPredicateNodes, true);
    auto branches = enumerator.all(DiscreteType::Int64, grammar.maximumBranchNodes, false);
    result.work.constructionAttempts = enumerator.attempts();
    result.work.generatedExpressions = enumerator.generated();
    result.work.invalidExpressions = enumerator.invalid();
    result.work.estimatedPeakBytes =
        std::max(result.work.estimatedPeakBytes, enumerator.peakBytes());
    if (enumerator.cancelled() || (cancellation && cancellation->load(std::memory_order_relaxed))) {
        result.status = DiscreteConditionalGrammarStatus::Cancelled;
        result.diagnostic = "conditional grammar enumeration cancelled";
        return result;
    }
    if (enumerator.exhausted()) {
        result.status = DiscreteConditionalGrammarStatus::ResourceExhausted;
        result.diagnostic = "conditional grammar enumeration limit exhausted";
        return result;
    }

    // Collapse only expressions with identical behavior on training rows. The
    // first expression is the shortest and stable grammar-order representative;
    // replacing another member cannot change any finite-training conditional.
    const auto reduce = [&](std::vector<DiscreteExpression>& candidates) -> bool {
        std::size_t reductionBytes = 0;
        for (const auto& candidate : candidates) {
            std::size_t nodeBytes = 0;
            if (!expressionStorageEstimate(candidate.nodeCount(), limits.maximumEstimatedBytes,
                                           nodeBytes))
                return false;
            constexpr auto perRowSignatureBytes = std::size_t{24};
            if (specification.observations().size() >
                (limits.maximumEstimatedBytes -
                 std::min(limits.maximumEstimatedBytes, sizeof(std::string) + 64)) /
                    perRowSignatureBytes)
                return false;
            const auto signatureBytes = sizeof(std::string) + 64 +
                                        specification.observations().size() * perRowSignatureBytes;
            if (!addWithin(reductionBytes, nodeBytes, limits.maximumEstimatedBytes))
                return false;
            reductionBytes += nodeBytes;
            if (!addWithin(reductionBytes, signatureBytes, limits.maximumEstimatedBytes))
                return false;
            reductionBytes += signatureBytes;
        }
        if (!addWithin(result.work.estimatedPeakBytes, reductionBytes,
                       limits.maximumEstimatedBytes))
            return false;
        result.work.estimatedPeakBytes += reductionBytes;
        std::set<std::string> seen;
        std::vector<DiscreteExpression> representatives;
        representatives.reserve(candidates.size());
        for (const auto& candidate : candidates) {
            bool valid = false, exhausted = false;
            auto key = behaviorKey(candidate, specification, limits.evaluation,
                                   result.work.poolEvaluationWork, limits.maximumEvaluationWork,
                                   cancellation, valid, exhausted);
            if (exhausted)
                return false;
            if (!valid)
                ++result.work.invalidExpressions;
            if (!seen.insert(std::move(key)).second) {
                ++result.work.equivalentExpressions;
                continue;
            }
            representatives.push_back(candidate);
        }
        candidates = std::move(representatives);
        return true;
    };
    if (!reduce(predicates) || !reduce(branches)) {
        result.status = cancellation && cancellation->load(std::memory_order_relaxed)
                            ? DiscreteConditionalGrammarStatus::Cancelled
                            : DiscreteConditionalGrammarStatus::ResourceExhausted;
        result.diagnostic = result.status == DiscreteConditionalGrammarStatus::Cancelled
                                ? "conditional grammar reduction cancelled"
                                : "conditional grammar pool evaluation work exhausted";
        return result;
    }
    result.work.poolTraceEvaluationWork =
        result.work.poolEvaluationWork - result.work.specificationValidationWork;
    result.work.predicatePoolSize = predicates.size();
    result.work.branchPoolSize = branches.size();
    if (predicates.empty() || branches.empty()) {
        result.status = DiscreteConditionalGrammarStatus::Invalid;
        result.diagnostic = "declared grammar generated no valid predicate or branch candidates";
        return result;
    }

    if (result.work.poolEvaluationWork >= limits.maximumEvaluationWork) {
        result.status = DiscreteConditionalGrammarStatus::ResourceExhausted;
        result.diagnostic =
            "aggregate conditional grammar evaluation work exhausted before composition";
        return result;
    }

    const auto usedMemory = result.work.estimatedPeakBytes;
    if (usedMemory >= limits.maximumEstimatedBytes) {
        result.status = DiscreteConditionalGrammarStatus::ResourceExhausted;
        result.diagnostic = "conditional grammar memory estimate exhausted before composition";
        return result;
    }
    DiscreteConditionalCompositionLimits compositionLimits;
    compositionLimits.maxCandidateCombinations = limits.maximumCandidateCombinations;
    compositionLimits.maxNodes = limits.maximumConditionalNodes;
    compositionLimits.maxDepth = limits.maximumConditionalDepth;
    compositionLimits.maxEvaluationWork =
        limits.maximumEvaluationWork - result.work.poolEvaluationWork;
    compositionLimits.maxEstimatedBytes = limits.maximumEstimatedBytes - usedMemory;
    compositionLimits.evaluation = limits.evaluation;
    auto selected = composeDiscreteConditional(specification, predicates, branches,
                                               compositionLimits, cancellation);
    result.work.composition = selected.work;
    std::size_t selectorWork = 0;
    const bool selectorWorkValid =
        addWork(selectorWork, selected.work.nodeVisits, limits.maximumEvaluationWork) &&
        addWork(selectorWork, selected.work.operations, limits.maximumEvaluationWork) &&
        addWork(selectorWork, selected.work.specificationComparisons, limits.maximumEvaluationWork);
    if (!selectorWorkValid ||
        !addWork(result.work.poolEvaluationWork, selectorWork, limits.maximumEvaluationWork)) {
        result.status = DiscreteConditionalGrammarStatus::ResourceExhausted;
        result.diagnostic = "aggregate conditional grammar evaluation work exhausted";
        return result;
    }
    if (selected.work.estimatedBytes > std::numeric_limits<std::size_t>::max() - usedMemory) {
        result.status = DiscreteConditionalGrammarStatus::ResourceExhausted;
        result.diagnostic = "conditional grammar peak memory estimate overflow";
        return result;
    }
    result.work.estimatedPeakBytes =
        std::max(result.work.estimatedPeakBytes, usedMemory + selected.work.estimatedBytes);
    result.matchingRows = selected.matchingRows;
    result.expression = std::move(selected.expression);
    switch (selected.status) {
    case DiscreteConditionalCompositionStatus::ExactTraining:
        result.status = DiscreteConditionalGrammarStatus::ExactTraining;
        break;
    case DiscreteConditionalCompositionStatus::Partial:
        result.status = DiscreteConditionalGrammarStatus::Partial;
        break;
    case DiscreteConditionalCompositionStatus::Cancelled:
        result.status = DiscreteConditionalGrammarStatus::Cancelled;
        break;
    case DiscreteConditionalCompositionStatus::ResourceExhausted:
        result.status = DiscreteConditionalGrammarStatus::ResourceExhausted;
        break;
    case DiscreteConditionalCompositionStatus::Invalid:
        result.status = DiscreteConditionalGrammarStatus::Invalid;
        break;
    }
    result.diagnostic = selected.diagnostic;
    return result;
}
