#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
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
    // Zero inherits maximumPredicateNodes; a smaller value caps each integer
    // operand tree of a comparison and can keep predicate enumeration compact.
    std::size_t maximumPredicateIntegerNodes = 0;
    // Zero preserves the atomic-predicate grammar. Positive values bound the
    // enabled Boolean productions below, represented with lazy IfThenElse trees.
    std::size_t maximumBooleanNesting = 0;
    bool allowBooleanAnd = false;
    bool allowBooleanOr = false;
    bool allowBooleanNot = false;
    // Zero preserves the single output-selector grammar. Positive values allow
    // bounded Int64 IfThenElse productions inside generated output branches.
    std::size_t maximumConditionalNesting = 0;
    // Optional pruning from finite training behavior; defaults off to preserve
    // the original selector traversal.
    bool enableSpecificationGuidedPruning = false;
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
    std::size_t predicateCandidatesBeforeReduction = 0;
    std::size_t branchCandidatesBeforeReduction = 0;
    std::size_t trainingTraceEvaluations = 0;
    // Compositional trace accounting. Cached values are finite-training
    // observations only; these counters do not imply universal equivalence.
    std::size_t cachedTraceNodeVisits = 0;
    std::size_t cachedTraceOperations = 0;
    std::size_t cachedTraceArithmeticComparisonOperations = 0;
    std::size_t cachedTraceConditionalOperations = 0;
    std::size_t signatureRowTouches = 0;
    std::size_t cacheLookups = 0;
    std::size_t cacheConstructionWork = 0;
    std::size_t cachedTraceReuses = 0;
    std::size_t evaluatorNodeVisits = 0;
    std::size_t evaluatorOperations = 0;
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
    std::size_t booleanNestingRemaining;
    std::size_t conditionalNestingRemaining;
    bool operator<(const PoolKey& other) const noexcept {
        return std::tie(predicate, type, nodes, booleanNestingRemaining,
                        conditionalNestingRemaining) <
               std::tie(other.predicate, other.type, other.nodes, other.booleanNestingRemaining,
                        other.conditionalNestingRemaining);
    }
};

using BehaviorSignature = genetik_conditional_composition_detail::TrainingTrace;

inline bool componentFitsConditionalDepth(std::size_t expressionDepth,
                                          std::size_t maximumConditionalDepth) noexcept {
    return maximumConditionalDepth > 1 && expressionDepth < maximumConditionalDepth;
}

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
                                                     bool predicate,
                                                     std::size_t booleanNestingRemaining,
                                                     std::size_t conditionalNestingRemaining) {
        // Boolean and output-conditional nesting are independent grammar
        // dimensions. Integer predicate operands remain shared across both.
        const PoolKey key{
            predicate, type, nodes, type == DiscreteType::Bool ? booleanNestingRemaining : 0,
            !predicate && type == DiscreteType::Int64 ? conditionalNestingRemaining : 0};
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
                for (const auto& child :
                     enumerate(DiscreteType::Int64, nodes - 1, predicate, booleanNestingRemaining,
                               conditionalNestingRemaining)) {
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
                const auto operandLimit = grammar_.maximumPredicateIntegerNodes
                                              ? grammar_.maximumPredicateIntegerNodes
                                              : grammar_.maximumPredicateNodes;
                if (type == DiscreteType::Bool && predicate &&
                    (leftNodes > operandLimit || rightNodes > operandLimit))
                    continue;
                const auto& leftPool =
                    enumerate(leftType, leftNodes, predicate, booleanNestingRemaining,
                              conditionalNestingRemaining);
                const auto& rightPool =
                    enumerate(rightType, rightNodes, predicate, booleanNestingRemaining,
                              conditionalNestingRemaining);
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
        // Boolean composition is deliberately opt-in and reuses the evaluator's
        // lazy IfThenElse node: AND = if(p,q,false), OR = if(p,true,q), and
        // NOT = if(p,false,true). Keeping the connective forms explicit avoids
        // enumerating arbitrary three-predicate Boolean conditionals.
        if (predicate && type == DiscreteType::Bool && booleanNestingRemaining > 0 &&
            (grammar_.allowBooleanAnd || grammar_.allowBooleanOr || grammar_.allowBooleanNot)) {
            const auto& constants =
                enumerate(DiscreteType::Bool, 1, true, booleanNestingRemaining - 1, 0);
            const auto findConstant = [&](bool value) -> const DiscreteExpression* {
                const auto found = std::find_if(
                    constants.begin(), constants.end(), [value](const auto& expression) {
                        return expression.nodes().front().kind() == DiscreteKind::Boolean &&
                               expression.nodes().front().boolean() == value;
                    });
                return found == constants.end() ? nullptr : &*found;
            };
            const auto* falseValue = findConstant(false);
            const auto* trueValue = findConstant(true);
            const auto appendConditional = [&](const DiscreteExpression& condition,
                                               const DiscreteExpression& whenTrue,
                                               const DiscreteExpression& whenFalse) {
                if (cancelled() || exhausted_)
                    return false;
                if (!attempt() || !admitTransient(nodes))
                    return false;
                try {
                    auto expression = DiscreteExpression::conditional(
                        condition, whenTrue, whenFalse, grammar_.expressionLimits);
                    if (expression.depth() <= grammar_.maximumExpressionDepth)
                        return append(result, std::move(expression));
                } catch (const std::length_error&) {
                    ++invalid_;
                } catch (const std::invalid_argument&) {
                    ++invalid_;
                }
                return true;
            };
            for (std::size_t conditionNodes = 1; conditionNodes + 3 <= nodes; ++conditionNodes) {
                const auto valueNodes = nodes - conditionNodes - 2;
                if (!valueNodes)
                    continue;
                const auto& conditions = enumerate(DiscreteType::Bool, conditionNodes, true,
                                                   booleanNestingRemaining - 1, 0);
                const auto& values =
                    enumerate(DiscreteType::Bool, valueNodes, true, booleanNestingRemaining - 1, 0);
                for (const auto& condition : conditions) {
                    for (const auto& value : values) {
                        // Declared connective order is AND, then OR.
                        if (grammar_.allowBooleanAnd && falseValue &&
                            !appendConditional(condition, value, *falseValue))
                            return result;
                        if (grammar_.allowBooleanOr && trueValue &&
                            !appendConditional(condition, *trueValue, value))
                            return result;
                    }
                }
                // NOT is unary in its source grammar but retains the same
                // lazy conditional evaluator and is emitted after binary forms.
                if (grammar_.allowBooleanNot && conditionNodes + 3 == nodes && falseValue &&
                    trueValue) {
                    for (const auto& condition : conditions)
                        if (!appendConditional(condition, *falseValue, *trueValue))
                            return result;
                }
            }
        }
        // Output branches may opt into bounded nested Int64 conditionals.
        // Conditions use the existing declared predicate grammar; both arms
        // consume one level of conditional nesting and all node/work limits
        // remain shared with ordinary branch enumeration.
        if (!predicate && type == DiscreteType::Int64 && conditionalNestingRemaining > 0) {
            for (std::size_t conditionNodes = 1; conditionNodes + 3 <= nodes; ++conditionNodes) {
                for (std::size_t yesNodes = 1; yesNodes + conditionNodes + 2 <= nodes; ++yesNodes) {
                    const auto noNodes = nodes - conditionNodes - yesNodes - 1;
                    if (!noNodes)
                        continue;
                    const auto& conditions = enumerate(DiscreteType::Bool, conditionNodes, true,
                                                       grammar_.maximumBooleanNesting, 0);
                    const auto& yesValues = enumerate(DiscreteType::Int64, yesNodes, false, 0,
                                                      conditionalNestingRemaining - 1);
                    const auto& noValues = enumerate(DiscreteType::Int64, noNodes, false, 0,
                                                     conditionalNestingRemaining - 1);
                    for (const auto& condition : conditions)
                        for (const auto& yes : yesValues)
                            for (const auto& no : noValues) {
                                if (cancelled() || exhausted_)
                                    return result;
                                if (!attempt() || !admitTransient(nodes))
                                    return result;
                                try {
                                    auto expression = DiscreteExpression::conditional(
                                        condition, yes, no, grammar_.expressionLimits);
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
        // Finish memoized construction before collecting roots. Moving roots
        // preserves their node storage and avoids a deep copy of every tree.
        for (std::size_t nodes = 1; nodes <= maximumNodes; ++nodes) {
            if (cancelled() || exhausted_)
                break;
            const auto& sizePool = enumerate(
                type, nodes, predicate, predicate ? grammar_.maximumBooleanNesting : 0,
                !predicate && type == DiscreteType::Int64 ? grammar_.maximumConditionalNesting : 0);
            const auto remainingPoolSlots = limits_.maximumPoolExpressions -
                                            std::min(generated_, limits_.maximumPoolExpressions);
            if (sizePool.size() >
                remainingPoolSlots - std::min(pendingRootExpressions_, remainingPoolSlots)) {
                exhausted_ = true;
                break;
            }
            pendingRootExpressions_ += sizePool.size();
        }
        if (cancelled() || exhausted_)
            return output;
        std::size_t total = 0;
        for (std::size_t nodes = 1; nodes <= maximumNodes; ++nodes) {
            const PoolKey key{
                predicate, type, nodes,
                type == DiscreteType::Bool ? grammar_.maximumBooleanNesting : 0,
                !predicate && type == DiscreteType::Int64 ? grammar_.maximumConditionalNesting : 0};
            const auto found = pools_.find(key);
            if (found != pools_.end()) {
                if (!addWithin(total, found->second.size(),
                               std::numeric_limits<std::size_t>::max())) {
                    exhausted_ = true;
                    return output;
                }
                total += found->second.size();
            }
        }
        const auto collected = total;
        if (collected > std::numeric_limits<std::size_t>::max() / sizeof(DiscreteExpression)) {
            exhausted_ = true;
            return output;
        }
        const auto transferBytes = collected * sizeof(DiscreteExpression);
        if (!addWithin(currentBytes_, transferBytes, limits_.maximumEstimatedBytes)) {
            exhausted_ = true;
            return output;
        }
        peakBytes_ = std::max(peakBytes_, currentBytes_ + transferBytes);
        output.reserve(collected);
        for (std::size_t nodes = 1; nodes <= maximumNodes; ++nodes) {
            if (cancelled())
                return output;
            const PoolKey key{
                predicate, type, nodes,
                type == DiscreteType::Bool ? grammar_.maximumBooleanNesting : 0,
                !predicate && type == DiscreteType::Int64 ? grammar_.maximumConditionalNesting : 0};
            const auto found = pools_.find(key);
            if (found == pools_.end())
                continue;
            for (auto& expression : found->second) {
                if (generated_ == limits_.maximumPoolExpressions) {
                    exhausted_ = true;
                    break;
                }
                output.push_back(std::move(expression));
                ++generated_;
                --pendingRootExpressions_;
            }
            if (exhausted_)
                break;
        }
        pools_.clear();
        std::size_t outputBytes = 0;
        for (const auto& expression : output) {
            std::size_t bytes = 0;
            if (!expressionStorageEstimate(expression.nodeCount(), limits_.maximumEstimatedBytes,
                                           bytes) ||
                !addWithin(outputBytes, bytes, limits_.maximumEstimatedBytes)) {
                exhausted_ = true;
                return {};
            }
            outputBytes += bytes;
        }
        if (!addWithin(heldBytes_, outputBytes, limits_.maximumEstimatedBytes)) {
            exhausted_ = true;
            return {};
        }
        heldBytes_ += outputBytes;
        currentBytes_ = heldBytes_;
        peakBytes_ = std::max(peakBytes_, currentBytes_);
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
        if (generated_ >= limits_.maximumPoolExpressions ||
            pendingRootExpressions_ >= limits_.maximumPoolExpressions - generated_) {
            exhausted_ = true;
            return false;
        }
        std::size_t bytes = 0;
        if (!expressionStorageEstimate(expression.nodeCount(), limits_.maximumEstimatedBytes,
                                       bytes)) {
            exhausted_ = true;
            return false;
        }
        // The expression is constructed as a transient, then retained once.
        // Transient peak storage was admitted by admitTransient().
        if (!addWithin(currentBytes_, bytes, limits_.maximumEstimatedBytes)) {
            exhausted_ = true;
            return false;
        }
        currentBytes_ += bytes;
        peakBytes_ = std::max(peakBytes_, currentBytes_);
        into.push_back(std::move(expression));
        ++generated_;
        return true;
    }

private:
    const DiscreteConditionalGrammar& grammar_;
    const DiscreteConditionalGrammarLimits& limits_;
    std::size_t dimensions_;
    const std::atomic<bool>* cancellation_;
    std::map<PoolKey, std::vector<DiscreteExpression>> pools_;
    std::size_t attempts_ = 0, generated_ = 0, invalid_ = 0, currentBytes_ = 0, heldBytes_ = 0,
                pendingRootExpressions_ = 0, peakBytes_ = 0;
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
        (grammar.maximumPredicateIntegerNodes &&
         grammar.maximumPredicateIntegerNodes > grammar.maximumPredicateNodes) ||
        grammar.maximumBooleanNesting > 16 || grammar.maximumConditionalNesting > 8 ||
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

inline BehaviorSignature behaviorKey(const DiscreteExpression& expression,
                                     const DiscreteSpecification& specification,
                                     const DiscreteEvaluationLimits& limits, std::size_t& work,
                                     std::size_t workCap, const std::atomic<bool>* cancellation,
                                     bool& valid, bool& exhausted,
                                     std::size_t* traceEvaluations = nullptr) {
    BehaviorSignature key;
    key.boolean = expression.type() == DiscreteType::Bool;
    key.values.reserve(specification.observations().size());
    key.valid.reserve(specification.observations().size());
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
        if (traceEvaluations)
            ++*traceEvaluations;
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
            key.valid.push_back(0);
            key.values.push_back(0);
            continue;
        }
        key.valid.push_back(1);
        if (evaluated.value->type() == DiscreteType::Bool) {
            key.values.push_back(evaluated.value->boolean() ? 1 : 0);
        } else {
            key.values.push_back(static_cast<std::uint64_t>(evaluated.value->integer()));
        }
    }
    return key;
}

// Memoized compositional semantics for the fixed, ordered training rows. The
// key is a typed syntax-tree node whose child IDs identify exact subtrees.
// Cache state is local to one synthesis call/specification.
class TrainingTraceCache {
    struct NodeKey {
        DiscreteKind kind{};
        DiscreteType type{};
        std::uint64_t atom = 0;
        std::array<std::size_t, 3> children{};
        std::uint8_t arity = 0;
        bool operator<(const NodeKey& other) const noexcept {
            return std::tie(kind, type, atom, arity, children) <
                   std::tie(other.kind, other.type, other.atom, other.arity, other.children);
        }
    };
    struct Entry {
        BehaviorSignature signature;
        std::vector<std::size_t> evaluationNodeVisits;
        std::vector<std::size_t> evaluationOperations;
        std::vector<std::size_t> evaluationFrameDepth;
    };

public:
    enum class Status { Ready, ResourceExhausted, Cancelled };

    TrainingTraceCache(const DiscreteSpecification& specification, std::size_t maximumWork,
                       std::size_t maximumBytes, std::size_t& aggregateWork,
                       DiscreteConditionalGrammarWork& counters, std::size_t& peakBytes,
                       DiscreteEvaluationLimits evaluationLimits,
                       const std::atomic<bool>* cancellation)
        : specification_(specification), maximumWork_(maximumWork), maximumBytes_(maximumBytes),
          aggregateWork_(aggregateWork), counters_(counters), peakBytes_(peakBytes),
          evaluationLimits_(evaluationLimits), cancellation_(cancellation), baseBytes_(peakBytes) {}

    void setBaseEstimate(std::size_t bytes) noexcept {
        baseBytes_ = bytes;
        if (retainedBytes_ <= std::numeric_limits<std::size_t>::max() - baseBytes_)
            peakBytes_ = std::max(peakBytes_, baseBytes_ + retainedBytes_);
    }

    Status signature(const DiscreteExpression& expression, BehaviorSignature& output, bool& valid) {
        DiscreteExpression::validateEvaluationLimits(evaluationLimits_);
        std::size_t idsBytes = 0;
        if (expression.nodeCount() > std::numeric_limits<std::size_t>::max() / sizeof(std::size_t))
            return Status::ResourceExhausted;
        idsBytes = expression.nodeCount() * sizeof(std::size_t);
        if (baseBytes_ > maximumBytes_ || retainedBytes_ > maximumBytes_ - baseBytes_ ||
            idsBytes > maximumBytes_ - baseBytes_ - retainedBytes_)
            return Status::ResourceExhausted;
        peakBytes_ = std::max(peakBytes_, baseBytes_ + retainedBytes_ + idsBytes);
        std::vector<std::size_t> ids(expression.nodeCount());
        const auto nodes = expression.nodes();
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (isCancelled())
                return Status::Cancelled;
            const auto& node = nodes[i];
            NodeKey key;
            key.kind = node.kind();
            key.type = expression.subtreeType(i);
            key.arity = static_cast<std::uint8_t>(discreteArity(node.kind()));
            if (node.kind() == DiscreteKind::Variable)
                key.atom = static_cast<std::uint64_t>(node.variable());
            else if (node.kind() == DiscreteKind::Integer)
                key.atom = static_cast<std::uint64_t>(node.integer());
            else if (node.kind() == DiscreteKind::Boolean)
                key.atom = node.boolean() ? 1 : 0;
            for (std::size_t j = 0; j < key.arity; ++j)
                key.children[j] = ids[expression.children(i)[j]];
            if (!charge(counters_.cacheLookups, 1))
                return Status::ResourceExhausted;
            const auto found = index_.find(key);
            if (found != index_.end()) {
                ++counters_.cachedTraceReuses;
                ids[i] = found->second;
                continue;
            }
            if (!insert(key, key.type, node, expression, i, ids, idsBytes, ids[i]))
                return isCancelled() ? Status::Cancelled : Status::ResourceExhausted;
        }
        if (ids.back() >= entries_.size())
            return Status::ResourceExhausted;
        const auto& root = entries_[ids.back()];
        output.boolean = root.signature.boolean;
        output.values.clear();
        output.valid.clear();
        output.values.reserve(root.signature.values.size());
        output.valid.reserve(root.signature.valid.size());
        valid = true;
        for (std::size_t row = 0; row < root.evaluationNodeVisits.size(); ++row) {
            if (isCancelled())
                return Status::Cancelled;
            if (!charge(counters_.signatureRowTouches, 1))
                return Status::ResourceExhausted;
            if (counters_.trainingTraceEvaluations == std::numeric_limits<std::size_t>::max())
                return Status::ResourceExhausted;
            ++counters_.trainingTraceEvaluations;
            if (root.evaluationNodeVisits[row] > evaluationLimits_.maxNodeVisits ||
                root.evaluationOperations[row] > evaluationLimits_.maxOperations ||
                root.evaluationFrameDepth[row] > evaluationLimits_.maxWorkspaceBytes /
                                                     DiscreteExpression::evaluationFrameBytes())
                return Status::ResourceExhausted;
            const auto rowValid = root.signature.valid[row];
            valid = valid && rowValid != 0;
            output.valid.push_back(rowValid);
            output.values.push_back(root.signature.values[row]);
        }
        return Status::Ready;
    }

    std::size_t retainedBytes() const noexcept {
        return retainedBytes_;
    }

private:
    bool isCancelled() const noexcept {
        return cancellation_ && cancellation_->load(std::memory_order_relaxed);
    }
    bool charge(std::size_t& counter, std::size_t amount) {
        if (!addWork(aggregateWork_, amount, maximumWork_))
            return false;
        counter += amount;
        return true;
    }
    bool reserveBytes(std::size_t amount, std::size_t scratchBytes) {
        if (baseBytes_ > maximumBytes_ || retainedBytes_ > maximumBytes_ - baseBytes_ ||
            scratchBytes > maximumBytes_ - baseBytes_ - retainedBytes_ ||
            amount > maximumBytes_ - baseBytes_ - retainedBytes_ - scratchBytes)
            return false;
        retainedBytes_ += amount;
        peakBytes_ = std::max(peakBytes_, baseBytes_ + retainedBytes_ + scratchBytes);
        return true;
    }
    static bool safeAdd(std::size_t& value, std::size_t amount) noexcept {
        if (amount > std::numeric_limits<std::size_t>::max() - value)
            return false;
        value += amount;
        return true;
    }
    bool chargeOperation(bool mathematical) {
        if (!charge(counters_.cachedTraceOperations, 1))
            return false;
        if (mathematical)
            ++counters_.cachedTraceArithmeticComparisonOperations;
        else
            ++counters_.cachedTraceConditionalOperations;
        return true;
    }
    bool insert(const NodeKey& key, DiscreteType type, const DiscreteNode& node,
                const DiscreteExpression& expression, std::size_t nodeIndex,
                const std::vector<std::size_t>& ids, std::size_t scratchBytes, std::size_t& id) {
        constexpr std::size_t entryOverhead = sizeof(NodeKey) + sizeof(Entry) + 192;
        const auto rows = specification_.observations().size();
        constexpr auto rowBytes =
            sizeof(std::uint64_t) + sizeof(std::uint8_t) + 3 * sizeof(std::size_t);
        if (rows > (std::numeric_limits<std::size_t>::max() - entryOverhead) / rowBytes)
            return false;
        const auto bytes = entryOverhead + rows * rowBytes;
        if (!reserveBytes(bytes, scratchBytes) || !charge(counters_.cacheConstructionWork, 1))
            return false;

        Entry entry;
        entry.signature.boolean = type == DiscreteType::Bool;
        entry.signature.values.reserve(rows);
        entry.signature.valid.reserve(rows);
        entry.evaluationNodeVisits.reserve(rows);
        entry.evaluationOperations.reserve(rows);
        entry.evaluationFrameDepth.reserve(rows);
        const auto arity = key.arity;
        for (const auto& row : specification_.observations()) {
            if (isCancelled())
                return false;
            if (!charge(counters_.cacheConstructionWork, 1) ||
                !charge(counters_.cachedTraceNodeVisits, 1))
                return false;
            bool rowValid = true;
            std::uint64_t value = 0;
            std::size_t virtualVisits = 1, virtualOperations = 0;
            std::size_t frameDepth = 1;
            const auto child = [&](std::size_t j) -> std::pair<bool, std::uint64_t> {
                const auto childId = ids[expression.children(nodeIndex)[j]];
                const auto& signature = entries_[childId].signature;
                const auto rowIndex = entry.signature.values.size();
                return {signature.valid[rowIndex] != 0, signature.values[rowIndex]};
            };
            const auto childVisits = [&](std::size_t j) {
                const auto childId = ids[expression.children(nodeIndex)[j]];
                return entries_[childId].evaluationNodeVisits[entry.signature.values.size()];
            };
            const auto childOperations = [&](std::size_t j) {
                const auto childId = ids[expression.children(nodeIndex)[j]];
                return entries_[childId].evaluationOperations[entry.signature.values.size()];
            };
            const auto childFrameDepth = [&](std::size_t j) {
                const auto childId = ids[expression.children(nodeIndex)[j]];
                return entries_[childId].evaluationFrameDepth[entry.signature.values.size()];
            };
            if (!arity) {
                if (node.kind() == DiscreteKind::Variable)
                    value = static_cast<std::uint64_t>(row.inputs[node.variable()]);
                else if (node.kind() == DiscreteKind::Integer)
                    value = static_cast<std::uint64_t>(node.integer());
                else if (node.kind() == DiscreteKind::Boolean)
                    value = node.boolean() ? 1 : 0;
                else
                    return false;
            } else if (node.kind() == DiscreteKind::IfThenElse) {
                const auto condition = child(0);
                virtualVisits += childVisits(0);
                virtualOperations += childOperations(0);
                frameDepth = 1 + childFrameDepth(0);
                if (!condition.first)
                    rowValid = false;
                else {
                    const auto selectedChild = condition.second ? 1 : 2;
                    const auto selected = child(selectedChild);
                    virtualVisits += childVisits(selectedChild);
                    frameDepth = std::max(frameDepth, 1 + childFrameDepth(selectedChild));
                    if (!safeAdd(virtualOperations, childOperations(selectedChild)) ||
                        !safeAdd(virtualOperations, 1))
                        return false;
                    rowValid = selected.first;
                    value = selected.second;
                    if (!chargeOperation(false))
                        return false;
                }
            } else {
                const auto left = child(0);
                virtualVisits += childVisits(0);
                virtualOperations += childOperations(0);
                frameDepth = 1 + childFrameDepth(0);
                if (!left.first) {
                    entry.signature.valid.push_back(0);
                    entry.signature.values.push_back(0);
                    entry.evaluationNodeVisits.push_back(virtualVisits);
                    entry.evaluationOperations.push_back(virtualOperations);
                    entry.evaluationFrameDepth.push_back(frameDepth);
                    continue;
                }
                const auto right = arity > 1 ? child(1) : std::pair<bool, std::uint64_t>{true, 0};
                if (arity > 1) {
                    virtualVisits += childVisits(1);
                    virtualOperations += childOperations(1);
                    frameDepth = std::max(frameDepth, 1 + childFrameDepth(1));
                }
                rowValid = left.first && right.first;
                if (rowValid) {
                    if (!safeAdd(virtualOperations, 1) || !chargeOperation(true))
                        return false;
                    const auto a = std::bit_cast<std::int64_t>(left.second);
                    const auto b = std::bit_cast<std::int64_t>(right.second);
                    if (integerOutputOperator(node.kind())) {
                        std::int64_t answer = 0;
                        if (!genetik_discrete_detail::arithmetic(node.kind(), a, b, answer))
                            rowValid = false;
                        else
                            value = static_cast<std::uint64_t>(answer);
                    } else {
                        bool answer = false;
                        switch (node.kind()) {
                        case DiscreteKind::Equal:
                            answer = a == b;
                            break;
                        case DiscreteKind::NotEqual:
                            answer = a != b;
                            break;
                        case DiscreteKind::LessThan:
                            answer = a < b;
                            break;
                        case DiscreteKind::LessEqual:
                            answer = a <= b;
                            break;
                        case DiscreteKind::GreaterThan:
                            answer = a > b;
                            break;
                        case DiscreteKind::GreaterEqual:
                            answer = a >= b;
                            break;
                        default:
                            return false;
                        }
                        value = answer ? 1 : 0;
                    }
                }
            }
            entry.signature.valid.push_back(rowValid ? 1 : 0);
            entry.signature.values.push_back(rowValid ? value : 0);
            entry.evaluationNodeVisits.push_back(virtualVisits);
            entry.evaluationOperations.push_back(virtualOperations);
            entry.evaluationFrameDepth.push_back(frameDepth);
        }
        const auto newId = entries_.size();
        entries_.push_back(std::move(entry));
        index_.emplace(key, newId);
        id = newId;
        return true;
    }

    const DiscreteSpecification& specification_;
    std::size_t maximumWork_;
    std::size_t maximumBytes_;
    std::size_t& aggregateWork_;
    DiscreteConditionalGrammarWork& counters_;
    std::size_t& peakBytes_;
    DiscreteEvaluationLimits evaluationLimits_;
    const std::atomic<bool>* cancellation_;
    std::map<NodeKey, std::size_t> index_;
    std::vector<Entry> entries_;
    std::size_t retainedBytes_ = 0;
    std::size_t baseBytes_ = 0;
};
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
    result.work.predicateCandidatesBeforeReduction = predicates.size();
    result.work.branchCandidatesBeforeReduction = branches.size();
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

    std::size_t nonCachePeakBytes = result.work.estimatedPeakBytes;
    TrainingTraceCache traceCache(specification, limits.maximumEvaluationWork,
                                  limits.maximumEstimatedBytes, result.work.poolEvaluationWork,
                                  result.work, result.work.estimatedPeakBytes, limits.evaluation,
                                  cancellation);
    bool depthExcluded = false;
    std::vector<BehaviorSignature> branchTrainingTraces;
    // Collapse expressions with identical typed behavior on training rows. Keep
    // the first shortest representative that can fit below the selector's outer
    // conditional node. If a shorter expression is too deep, a larger shallower
    // expression with the same trace remains eligible instead.
    const auto reduce = [&](std::vector<DiscreteExpression>& candidates,
                            std::vector<BehaviorSignature>* retainedTraces = nullptr) -> bool {
        std::size_t reductionBytes = 0;
        for (const auto& candidate : candidates) {
            std::size_t nodeBytes = 0;
            if (!expressionStorageEstimate(candidate.nodeCount(), limits.maximumEstimatedBytes,
                                           nodeBytes))
                return false;
            constexpr auto perRowSignatureBytes = sizeof(std::uint64_t) + sizeof(std::uint8_t);
            if (specification.observations().size() >
                (limits.maximumEstimatedBytes -
                 std::min(limits.maximumEstimatedBytes, sizeof(BehaviorSignature) + 128)) /
                    perRowSignatureBytes)
                return false;
            const auto signatureBytes = sizeof(BehaviorSignature) + 128 +
                                        specification.observations().size() * perRowSignatureBytes;
            if (!addWithin(reductionBytes, nodeBytes, limits.maximumEstimatedBytes))
                return false;
            reductionBytes += nodeBytes;
            if (!addWithin(reductionBytes, signatureBytes, limits.maximumEstimatedBytes))
                return false;
            reductionBytes += signatureBytes;
            if (retainedTraces && grammar.enableSpecificationGuidedPruning) {
                if (!addWithin(reductionBytes, signatureBytes, limits.maximumEstimatedBytes))
                    return false;
                reductionBytes += signatureBytes;
            }
        }
        if (!addWithin(nonCachePeakBytes, reductionBytes, limits.maximumEstimatedBytes))
            return false;
        nonCachePeakBytes += reductionBytes;
        traceCache.setBaseEstimate(nonCachePeakBytes);
        if (traceCache.retainedBytes() > limits.maximumEstimatedBytes - nonCachePeakBytes)
            return false;
        result.work.estimatedPeakBytes = std::max(result.work.estimatedPeakBytes,
                                                  nonCachePeakBytes + traceCache.retainedBytes());
        std::set<BehaviorSignature> seen;
        std::vector<DiscreteExpression> representatives;
        representatives.reserve(candidates.size());
        for (const auto& candidate : candidates) {
            if (!componentFitsConditionalDepth(candidate.depth(), limits.maximumConditionalDepth)) {
                depthExcluded = true;
                continue;
            }
            bool valid = false;
            BehaviorSignature key;
            const auto traceStatus = traceCache.signature(candidate, key, valid);
            if (traceStatus != TrainingTraceCache::Status::Ready)
                return false;
            if (!valid)
                ++result.work.invalidExpressions;
            const auto [position, inserted] = seen.insert(std::move(key));
            if (!inserted) {
                ++result.work.equivalentExpressions;
                continue;
            }
            if (retainedTraces && grammar.enableSpecificationGuidedPruning) {
                if (specification.observations().size() >
                    std::numeric_limits<std::size_t>::max() / 2)
                    return false;
                const auto rowCopies = specification.observations().size() * 2;
                if (!addWork(result.work.poolEvaluationWork, rowCopies,
                             limits.maximumEvaluationWork) ||
                    !addWork(result.work.signatureRowTouches, rowCopies,
                             limits.maximumEvaluationWork))
                    return false;
                retainedTraces->push_back(*position);
            }
            representatives.push_back(candidate);
        }
        candidates = std::move(representatives);
        return true;
    };
    if (!reduce(predicates) ||
        !reduce(branches,
                grammar.enableSpecificationGuidedPruning ? &branchTrainingTraces : nullptr)) {
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
        result.status = depthExcluded ? DiscreteConditionalGrammarStatus::ResourceExhausted
                                      : DiscreteConditionalGrammarStatus::Invalid;
        result.diagnostic =
            depthExcluded ? "conditional depth bound excludes all generated components"
                          : "declared grammar generated no valid predicate or branch candidates";
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
    auto selected =
        genetik_conditional_composition_detail::composeDiscreteConditionalWithBranchTraces(
            specification, predicates, branches, compositionLimits, cancellation,
            branchTrainingTraces, grammar.enableSpecificationGuidedPruning);
    result.work.composition = selected.work;
    result.work.evaluatorNodeVisits = selected.work.nodeVisits;
    result.work.evaluatorOperations = selected.work.operations;
    std::size_t selectorWork = 0;
    const bool selectorWorkValid =
        addWork(selectorWork, selected.work.nodeVisits, limits.maximumEvaluationWork) &&
        addWork(selectorWork, selected.work.operations, limits.maximumEvaluationWork) &&
        addWork(selectorWork, selected.work.specificationComparisons,
                limits.maximumEvaluationWork) &&
        addWork(selectorWork, selected.work.pruningWork, limits.maximumEvaluationWork);
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
