#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "DiscreteExpression.hpp"

// A bounded construction-time DAG of exact discrete expression components.
// Materialization returns an ordinary DiscreteExpression; evaluation therefore
// retains the existing checked Int64, lazy-conditional and scoped-iteration
// semantics. Shared components are expanded per occurrence at that boundary.
struct DiscreteComponentLimits {
    std::size_t maxVariables = 1024;
    std::size_t maxComponents = 4096;
    std::size_t maxEdges = 16384;
    std::size_t maxRetainedBytes = 8 * 1024 * 1024;
    std::size_t maxExpandedNodes = 4096;
    std::size_t maxExpandedDepth = 256;
    std::size_t maxExpandedConstants = 4096;
    std::size_t maxExpandedBinders = 128;
    std::size_t maxMaterializationBytes = 4 * 1024 * 1024;
    std::size_t maxMaterializationWork = 1000000;
    std::size_t maxConstructionWork = 1000000;
};

struct DiscreteComponentStorageStats {
    std::size_t components = 0;
    std::size_t edges = 0;
    std::size_t retainedBytesEstimate = 0;
    std::size_t constructionWork = 0;
};

struct DiscreteComponentExpansionStats {
    std::size_t nodes = 0;
    std::size_t depth = 0;
    std::size_t constants = 0;
    std::size_t binders = 0;
    std::size_t expandedEdges = 0;
    std::size_t iterationDepth = 0;
    std::size_t binderReferences = 0;
    std::size_t iteratorReferences = 0;
    std::size_t binderLookupComparisonsUpperBound = 0;
    std::size_t expressionValidationWorkUpperBound = 0;
    std::size_t componentOccurrences = 0;
    std::size_t materializationWorkUpperBound = 0;
};

enum class DiscreteComponentMaterializationStatus { Success, ResourceExhausted };

struct DiscreteComponentMaterializationWork {
    std::size_t nodesEmitted = 0;
    std::size_t componentOccurrencesExpanded = 0;
    std::size_t componentEdgesExpanded = 0;
    std::size_t binderMappings = 0;
    std::size_t binderLookups = 0;
    std::size_t binderLookupComparisons = 0;
    std::size_t totalUnits = 0;
    std::size_t expressionValidationWorkUpperBound = 0;
    std::size_t totalWorkUpperBound = 0;
};

struct DiscreteComponentMaterialization {
    DiscreteComponentMaterializationStatus status =
        DiscreteComponentMaterializationStatus::ResourceExhausted;
    std::optional<DiscreteExpression> expression;
    DiscreteComponentExpansionStats expansion;
    DiscreteComponentMaterializationWork work;
    std::string diagnostic;

    bool success() const noexcept {
        return status == DiscreteComponentMaterializationStatus::Success && expression.has_value();
    }
};

class DiscreteComponentHandle {
    struct Identity {};
    friend class DiscreteExpressionComponents;

    DiscreteComponentHandle(std::weak_ptr<const Identity> identity, std::size_t index)
        : identity_(std::move(identity)), index_(index) {}

    std::weak_ptr<const Identity> identity_;
    std::size_t index_;

  public:
    DiscreteComponentHandle() = delete;
};

// Components are immutable once appended. Each expression component is a
// closed DiscreteExpression over the arena's common Int64 input schema. Operator
// components refer only to earlier handles, so dependencies are acyclic.
class DiscreteExpressionComponents {
    struct Component {
        bool expressionComponent = false;
        std::optional<DiscreteExpression> expression;
        DiscreteKind kind = DiscreteKind::Integer;
        std::array<std::size_t, 3> children{};
        std::size_t arity = 0;
        DiscreteType type = DiscreteType::Int64;
        DiscreteComponentExpansionStats expansion;
        std::size_t edges = 0;
        std::size_t retainedBytesEstimate = 0;
        std::uint64_t maximumIntegerMagnitude = 0;
    };

  public:
    explicit DiscreteExpressionComponents(std::size_t dimensions,
                                          DiscreteComponentLimits limits = {})
        : dimensions_(dimensions), limits_(limits), identity_(std::make_shared<const DiscreteComponentHandle::Identity>()) {
        if (!limits_.maxComponents || !limits_.maxExpandedNodes || !limits_.maxExpandedDepth ||
            !limits_.maxRetainedBytes || !limits_.maxMaterializationBytes ||
            dimensions_ > limits_.maxVariables)
            throw std::invalid_argument("invalid discrete component limits or schema");
    }

    DiscreteExpressionComponents(const DiscreteExpressionComponents&) = delete;
    DiscreteExpressionComponents& operator=(const DiscreteExpressionComponents&) = delete;
    DiscreteExpressionComponents(DiscreteExpressionComponents&&) = delete;
    DiscreteExpressionComponents& operator=(DiscreteExpressionComponents&&) = delete;

    std::size_t dimensions() const noexcept {
        return dimensions_;
    }

    DiscreteComponentStorageStats storageStats() const noexcept {
        return {components_.size(), edges_, retainedBytesEstimate_, constructionWork_};
    }

    DiscreteComponentHandle addExpression(const DiscreteExpression& expression) {
        (void)expression.type();
        if (expression.dimensions() != dimensions_)
            throw std::invalid_argument("component expression schema mismatch");
        if (!expression.outerBindings().empty())
            throw std::invalid_argument("component expression must be closed");

        Component component;
        component.expressionComponent = true;
        component.type = expression.type();
        component.expansion.nodes = expression.nodeCount();
        component.expansion.depth = expression.depth();
        component.expansion.constants = expression.constantCount();
        component.expansion.binders = expression.binders().size();
        component.expansion.iterationDepth = expression.iterationDepth();
        component.expansion.componentOccurrences = 1;
        component.retainedBytesEstimate =
            boundedAdd(sizeof(Component),
                       boundedMultiply(expression.nodeCount(), DiscreteExpression::storageBytesPerNode(),
                                       "component storage estimate overflow"),
                       "component storage estimate overflow");
        checkAdmission(component);
        const auto copyWork = boundedAdd(
            boundedAdd(boundedMultiply(expression.nodeCount(), 2,
                                       "component construction work overflow"),
                       expression.binders().size(), "component construction work overflow"),
            1, "component construction work overflow");
        checkConstructionWork(boundedAdd(expression.nodeCount(), copyWork,
                                         "component construction work overflow"));
        for (const auto& node : expression.nodes()) {
            ++constructionWork_;
            if (node.kind() == DiscreteKind::Integer)
                component.maximumIntegerMagnitude = std::max(
                    component.maximumIntegerMagnitude, genetik_discrete_detail::magnitude(node.integer()));
            if (node.kind() == DiscreteKind::Iterator) {
                ++component.expansion.binderReferences;
                ++component.expansion.iteratorReferences;
            } else if (discreteIteration(node.kind())) {
                ++component.expansion.binderReferences;
            }
        }
        component.expansion.binderLookupComparisonsUpperBound = boundedMultiply(
            component.expansion.binderReferences, component.expansion.binders,
            "component binder comparison bound overflow");
        computeMaterializationWorkBound(component.expansion);
        checkAdmission(component);
        checkExpansionAdmission(component.expansion);
        constructionWork_ = boundedAdd(constructionWork_, copyWork, "component work counter overflow");
        component.expression = expression;
        publish(std::move(component));
        return handle(components_.size() - 1);
    }

    DiscreteComponentHandle unary(DiscreteKind kind, DiscreteComponentHandle child) {
        const std::array children{child};
        return compose(kind, children);
    }

    DiscreteComponentHandle binary(DiscreteKind kind, DiscreteComponentHandle left,
                                   DiscreteComponentHandle right) {
        const std::array children{left, right};
        return compose(kind, children);
    }

    DiscreteComponentHandle ternary(DiscreteKind kind, DiscreteComponentHandle first,
                                    DiscreteComponentHandle second, DiscreteComponentHandle third) {
        const std::array children{first, second, third};
        return compose(kind, children);
    }

    DiscreteComponentHandle conditional(DiscreteComponentHandle condition,
                                        DiscreteComponentHandle whenTrue,
                                        DiscreteComponentHandle whenFalse) {
        return ternary(DiscreteKind::IfThenElse, condition, whenTrue, whenFalse);
    }

    DiscreteType type(DiscreteComponentHandle component) const {
        return components_.at(index(component)).type;
    }

    DiscreteComponentExpansionStats expansionStats(DiscreteComponentHandle component) const {
        return components_.at(index(component)).expansion;
    }

    DiscreteComponentMaterialization materialize(
        DiscreteComponentHandle root, DiscreteExpressionLimits expressionLimits = {}) const {
        DiscreteComponentMaterialization result;
        const auto rootIndex = index(root);
        const auto& rootComponent = components_[rootIndex];
        result.expansion = rootComponent.expansion;

        // Validate policy without constructing a sentinel expression: a valid
        // zero-constant policy must accept a variable-only materialization.
        genetik_discrete_detail::validateLimits(expressionLimits);
        if (dimensions_ > expressionLimits.maxVariables)
            throw std::invalid_argument("component materialization schema exceeds expression variables");
        const auto& expansion = rootComponent.expansion;
        std::size_t expressionBytes = 0;
        std::size_t requiredBytes = 0;
        try {
            expressionBytes = boundedMultiply(expansion.nodes,
                                              DiscreteExpression::storageBytesPerNode(),
                                              "component expression storage overflow");
            requiredBytes = boundedAdd(
                expressionBytes,
                boundedMultiply(expansion.nodes, sizeof(DiscreteNode),
                                "component output staging estimate overflow"),
                "component materialization storage overflow");
            requiredBytes = boundedAdd(
                requiredBytes,
                boundedMultiply(expansion.depth, sizeof(std::size_t) * 3,
                                "component traversal stack estimate overflow"),
                "component materialization storage overflow");
            requiredBytes = boundedAdd(
                requiredBytes,
                boundedMultiply(expansion.binders, sizeof(std::pair<std::size_t, std::size_t>),
                                "component binder remapping estimate overflow"),
                "component materialization storage overflow");
        } catch (const std::length_error& error) {
            result.status = DiscreteComponentMaterializationStatus::ResourceExhausted;
            result.diagnostic = error.what();
            return result;
        }
        if (expansion.nodes > limits_.maxExpandedNodes || expansion.depth > limits_.maxExpandedDepth ||
            expansion.constants > limits_.maxExpandedConstants ||
            expansion.binders > limits_.maxExpandedBinders ||
            expansion.iterationDepth > expressionLimits.maxIterationDepth ||
            expansion.nodes > expressionLimits.maxNodes || expansion.depth > expressionLimits.maxDepth ||
            expansion.constants > expressionLimits.maxConstants ||
            expansion.binders > expressionLimits.maxBinders ||
            rootComponent.maximumIntegerMagnitude > expressionLimits.maxConstantMagnitude ||
            requiredBytes > limits_.maxMaterializationBytes ||
            expressionBytes > expressionLimits.maxStorageBytes ||
            expansion.materializationWorkUpperBound > limits_.maxMaterializationWork) {
            result.status = DiscreteComponentMaterializationStatus::ResourceExhausted;
            result.diagnostic = "component expansion exceeds a configured structural or work limit";
            return result;
        }

        std::vector<DiscreteNode> nodes;
        nodes.reserve(expansion.nodes);
        struct Frame {
            std::size_t component;
            std::size_t nextChild;
            bool entered;
        };
        std::vector<Frame> stack;
        stack.reserve(expansion.depth);
        stack.push_back({rootIndex, 0, false});
        std::size_t nextBinder = 0;

        while (!stack.empty()) {
            auto& frame = stack.back();
            const auto& component = components_[frame.component];
            if (component.expressionComponent) {
                ++result.work.componentOccurrencesExpanded;
                const auto& expression = *component.expression;
                const auto binders = expression.binders();
                std::vector<std::pair<std::size_t, std::size_t>> remapping;
                remapping.reserve(binders.size());
                for (const auto oldBinder : binders) {
                    if (nextBinder == std::numeric_limits<std::size_t>::max()) {
                        result.status = DiscreteComponentMaterializationStatus::ResourceExhausted;
                        result.diagnostic = "component binder identity space exhausted";
                        return result;
                    }
                    remapping.emplace_back(oldBinder, nextBinder++);
                    ++result.work.binderMappings;
                }
                for (const auto& node : expression.nodes()) {
                    if (node.kind() == DiscreteKind::Iterator || discreteIteration(node.kind())) {
                        auto mapping = remapping.end();
                        for (auto candidate = remapping.begin(); candidate != remapping.end(); ++candidate) {
                            ++result.work.binderLookupComparisons;
                            if (candidate->first == node.binder()) {
                                mapping = candidate;
                                break;
                            }
                        }
                        if (mapping == remapping.end())
                            throw std::logic_error("component binder map is inconsistent");
                        ++result.work.binderLookups;
                        nodes.push_back(node.kind() == DiscreteKind::Iterator
                                            ? DiscreteNode::iterator(mapping->second)
                                            : DiscreteNode::iteration(node.kind(), mapping->second));
                    } else {
                        nodes.push_back(node);
                    }
                    ++result.work.nodesEmitted;
                }
                stack.pop_back();
                continue;
            }

            if (!frame.entered) {
                frame.entered = true;
                ++result.work.componentOccurrencesExpanded;
            }
            if (frame.nextChild < component.arity) {
                const auto child = component.children[frame.nextChild++];
                ++result.work.componentEdgesExpanded;
                stack.push_back({child, 0, false});
                continue;
            }
            nodes.push_back(DiscreteNode::operation(component.kind));
            ++result.work.nodesEmitted;
            stack.pop_back();
        }

        if (result.work.nodesEmitted != expansion.nodes ||
            result.work.componentOccurrencesExpanded != expansion.componentOccurrences ||
            result.work.componentEdgesExpanded != expansion.expandedEdges) {
            throw std::logic_error("component expansion accounting mismatch");
        }
        result.work.totalUnits = boundedAdd(
            boundedAdd(result.work.nodesEmitted, result.work.componentOccurrencesExpanded,
                       "component work counter overflow"),
            result.work.componentEdgesExpanded, "component work counter overflow");
        result.work.totalUnits = boundedAdd(
            result.work.totalUnits, result.work.binderMappings, "component work counter overflow");
        result.work.totalUnits = boundedAdd(
            result.work.totalUnits, result.work.binderLookupComparisons,
            "component work counter overflow");
        result.work.expressionValidationWorkUpperBound = expansion.expressionValidationWorkUpperBound;
        result.work.totalWorkUpperBound = boundedAdd(
            result.work.totalUnits, result.work.expressionValidationWorkUpperBound,
            "component total work bound overflow");
        if (result.work.totalWorkUpperBound > expansion.materializationWorkUpperBound)
            throw std::logic_error("component materialization work exceeded its admitted upper bound");
        if (result.work.totalWorkUpperBound > limits_.maxMaterializationWork) {
            result.status = DiscreteComponentMaterializationStatus::ResourceExhausted;
            result.diagnostic = "component materialization work exhausted";
            return result;
        }
        result.expression.emplace(nodes, dimensions_, expressionLimits);
        result.status = DiscreteComponentMaterializationStatus::Success;
        return result;
    }

    std::string toString(DiscreteComponentHandle root,
                         std::size_t maxBytes = 1024 * 1024) const {
        const auto rootIndex = index(root);
        std::string output = "components-v1[";
        const auto append = [&](std::string_view text) {
            if (output.size() > maxBytes || text.size() > maxBytes - output.size())
                throw std::length_error("component export byte limit");
            output.append(text);
        };
        for (std::size_t i = 0; i <= rootIndex; ++i) {
            const auto& component = components_[i];
            append("c");
            append(std::to_string(i));
            append("=");
            if (component.expressionComponent) {
                append(component.expression->toString(maxBytes - output.size()));
            } else {
                append("op");
                append(std::to_string(static_cast<unsigned>(component.kind)));
                append("(");
                for (std::size_t j = 0; j < component.arity; ++j) {
                    if (j)
                        append(",");
                    append("c");
                    append(std::to_string(component.children[j]));
                }
                append(")");
            }
            append(";");
        }
        append("root=c");
        append(std::to_string(rootIndex));
        append("]");
        return output;
    }

  private:
    static std::size_t boundedAdd(std::size_t left, std::size_t right, const char* error) {
        if (right > std::numeric_limits<std::size_t>::max() - left)
            throw std::length_error(error);
        return left + right;
    }

    static std::size_t boundedMultiply(std::size_t left, std::size_t right, const char* error) {
        if (right && left > std::numeric_limits<std::size_t>::max() / right)
            throw std::length_error(error);
        return left * right;
    }

    void checkConstructionWork(std::size_t additional) const {
        if (additional > limits_.maxConstructionWork ||
            constructionWork_ > limits_.maxConstructionWork - additional)
            throw std::length_error("discrete component construction work exhausted");
    }

    std::size_t index(const DiscreteComponentHandle& handle) const {
        const auto identity = handle.identity_.lock();
        if (!identity || identity.get() != identity_.get())
            throw std::invalid_argument("foreign or stale discrete component handle");
        if (handle.index_ >= components_.size())
            throw std::out_of_range("discrete component handle outside arena");
        return handle.index_;
    }

    DiscreteComponentHandle handle(std::size_t index) const {
        return {identity_, index};
    }

    DiscreteComponentHandle compose(DiscreteKind kind,
                                    std::span<const DiscreteComponentHandle> children) {
        const auto arity = discreteArity(kind);
        if (!arity || discreteIteration(kind) || arity != children.size())
            throw std::invalid_argument("ordinary discrete operator arity required");
        const auto constructionCost = boundedAdd(arity, 2, "component construction work overflow");
        std::array<std::size_t, 3> childIndices{};
        for (std::size_t j = 0; j < arity; ++j)
            childIndices[j] = index(children[j]);
        checkConstructionWork(constructionCost);
        constructionWork_ = boundedAdd(constructionWork_, arity,
                                       "component construction work counter overflow");
        Component component;
        component.kind = kind;
        component.arity = arity;
        component.edges = arity;
        component.expansion.nodes = 1;
        component.expansion.depth = 1;
        component.expansion.componentOccurrences = 1;
        component.expansion.expandedEdges = arity;
        component.expansion.materializationWorkUpperBound = 0;
        std::array<DiscreteType, 3> inputTypes{};
        for (std::size_t j = 0; j < arity; ++j) {
            const auto childIndex = childIndices[j];
            if (childIndex >= components_.size())
                throw std::logic_error("component dependency is not topological");
            const auto& source = components_[childIndex];
            component.children[j] = childIndex;
            inputTypes[j] = source.type;
            component.maximumIntegerMagnitude =
                std::max(component.maximumIntegerMagnitude, source.maximumIntegerMagnitude);
            component.expansion.nodes = boundedAdd(component.expansion.nodes, source.expansion.nodes,
                                                   "component expanded-node count overflow");
            component.expansion.depth = std::max(
                component.expansion.depth,
                boundedAdd(source.expansion.depth, 1, "component depth overflow"));
            component.expansion.constants = boundedAdd(component.expansion.constants,
                                                       source.expansion.constants,
                                                       "component constant count overflow");
            component.expansion.binders = boundedAdd(component.expansion.binders,
                                                     source.expansion.binders,
                                                     "component binder count overflow");
            component.expansion.iterationDepth =
                std::max(component.expansion.iterationDepth, source.expansion.iterationDepth);
            component.expansion.expandedEdges = boundedAdd(
                component.expansion.expandedEdges, source.expansion.expandedEdges,
                "component expanded-edge count overflow");
            component.expansion.binderReferences = boundedAdd(
                component.expansion.binderReferences, source.expansion.binderReferences,
                "component binder reference count overflow");
            component.expansion.iteratorReferences = boundedAdd(
                component.expansion.iteratorReferences, source.expansion.iteratorReferences,
                "component iterator reference count overflow");
            component.expansion.binderLookupComparisonsUpperBound = boundedAdd(
                component.expansion.binderLookupComparisonsUpperBound,
                source.expansion.binderLookupComparisonsUpperBound,
                "component binder comparison bound overflow");
            component.expansion.componentOccurrences = boundedAdd(
                component.expansion.componentOccurrences, source.expansion.componentOccurrences,
                "component occurrence count overflow");
        }
        constructionWork_ = boundedAdd(constructionWork_, 1,
                                       "component construction work counter overflow");
        component.type = resultType(kind, std::span<const DiscreteType>(inputTypes.data(), arity));
        component.retainedBytesEstimate = sizeof(Component);
        computeMaterializationWorkBound(component.expansion);
        checkAdmission(component);
        checkExpansionAdmission(component.expansion);
        constructionWork_ = boundedAdd(constructionWork_, 1,
                                       "component construction work counter overflow");
        publish(std::move(component));
        return handle(components_.size() - 1);
    }

    static DiscreteType resultType(DiscreteKind kind, std::span<const DiscreteType> inputs) {
        if (kind == DiscreteKind::IfThenElse) {
            if (inputs.size() != 3 || inputs[0] != DiscreteType::Bool || inputs[1] != inputs[2])
                throw std::invalid_argument("if component requires Bool,T,T");
            return inputs[1];
        }
        for (const auto input : inputs)
            if (input != DiscreteType::Int64)
                throw std::invalid_argument("integer component operands required");
        if (kind >= DiscreteKind::Equal && kind <= DiscreteKind::GreaterEqual)
            return DiscreteType::Bool;
        return DiscreteType::Int64;
    }

    void checkAdmission(const Component& component) const {
        const auto newCount = boundedAdd(components_.size(), 1, "component count overflow");
        const auto newEdges = boundedAdd(edges_, component.edges, "component edge count overflow");
        const auto newBytes = boundedAdd(retainedBytesEstimate_, component.retainedBytesEstimate,
                                         "component storage estimate overflow");
        if (newCount > limits_.maxComponents || newEdges > limits_.maxEdges ||
            component.expansion.nodes > limits_.maxExpandedNodes ||
            component.expansion.depth > limits_.maxExpandedDepth ||
            component.expansion.constants > limits_.maxExpandedConstants ||
            component.expansion.binders > limits_.maxExpandedBinders ||
            newBytes > limits_.maxRetainedBytes)
            throw std::length_error("discrete component arena limit");
    }

    void checkExpansionAdmission(const DiscreteComponentExpansionStats& expansion) const {
        if (expansion.nodes > limits_.maxExpandedNodes || expansion.depth > limits_.maxExpandedDepth ||
            expansion.constants > limits_.maxExpandedConstants ||
            expansion.binders > limits_.maxExpandedBinders ||
            expansion.materializationWorkUpperBound > limits_.maxMaterializationWork)
            throw std::length_error("discrete component expansion limit");
    }

    void publish(Component component) {
        const auto newEdges = boundedAdd(edges_, component.edges, "component edge count overflow");
        const auto newBytes = boundedAdd(retainedBytesEstimate_, component.retainedBytesEstimate,
                                         "component storage estimate overflow");
        components_.push_back(std::move(component));
        edges_ = newEdges;
        retainedBytesEstimate_ = newBytes;
    }

    static void computeMaterializationWorkBound(DiscreteComponentExpansionStats& expansion) {
        // The canonical expression constructor makes one postfix validation pass
        // with up to three child-edge checks per node, copies nodes/metadata, then
        // performs a scope walk with at most one node and one edge event per
        // postfix token. Binder declaration and iterator-scope searches are
        // quadratic/linear respectively, so charge their conservative bounds too.
        auto validation = boundedMultiply(expansion.nodes, 16,
                                         "component validation work overflow");
        validation = boundedAdd(
            validation,
            boundedMultiply(boundedMultiply(expansion.binders, expansion.binders,
                                            "component binder validation bound overflow"),
                            2, "component binder validation bound overflow"),
            "component validation work overflow");
        validation = boundedAdd(
            validation,
            boundedMultiply(expansion.iteratorReferences, expansion.binders,
                            "component scope lookup bound overflow"),
                                "component validation work overflow");
        expansion.expressionValidationWorkUpperBound = validation;

        auto materializer = expansion.nodes;
        materializer = boundedAdd(materializer, expansion.componentOccurrences,
                                  "component materialization work overflow");
        materializer = boundedAdd(materializer, expansion.expandedEdges,
                                  "component materialization work overflow");
        materializer = boundedAdd(materializer, expansion.binders,
                                  "component materialization work overflow");
        materializer = boundedAdd(materializer, expansion.binderLookupComparisonsUpperBound,
                                  "component materialization work overflow");
        expansion.materializationWorkUpperBound = boundedAdd(
            materializer, validation, "component total work bound overflow");
    }

    std::size_t dimensions_;
    DiscreteComponentLimits limits_;
    std::shared_ptr<const DiscreteComponentHandle::Identity> identity_;
    std::vector<Component> components_;
    std::size_t edges_ = 0;
    std::size_t retainedBytesEstimate_ = 0;
    std::size_t constructionWork_ = 0;
};
