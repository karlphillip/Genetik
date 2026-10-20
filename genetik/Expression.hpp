#pragma once
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cfloat>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0)
#error "Genetik expressions require strict floating point: disable fast/finite-only math"
#endif
static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53 &&
              std::numeric_limits<double>::max_exponent == 1024 && FLT_EVAL_METHOD == 0,
              "Genetik expression evaluation requires native IEEE binary64 arithmetic");

enum class ExpressionKind { Variable, Constant, Add, Subtract, Multiply, Divide, Log };

// One arity definition for construction, validation, traversal and genetic operators.
inline std::size_t expressionArity(ExpressionKind kind) {
    switch (kind) {
    case ExpressionKind::Variable: case ExpressionKind::Constant: return 0;
    case ExpressionKind::Log: return 1;
    case ExpressionKind::Add: case ExpressionKind::Subtract:
    case ExpressionKind::Multiply: case ExpressionKind::Divide: return 2;
    }
    throw std::invalid_argument("unsupported expression node kind");
}

// Typed postfix tokens. Only an Expression validates/completes a token sequence.
class ExpressionNode {
public:
    static ExpressionNode variable(std::size_t index) { return {ExpressionKind::Variable, index, 0}; }
    static ExpressionNode constant(double value) { return {ExpressionKind::Constant, 0, value}; }
    static ExpressionNode binary(ExpressionKind kind) {
        if (expressionArity(kind) != 2)
            throw std::invalid_argument("expected a supported binary expression operator");
        return {kind, 0, 0};
    }
    static ExpressionNode unary(ExpressionKind kind) {
        if (expressionArity(kind) != 1) throw std::invalid_argument("expected a supported unary expression operator");
        return {kind, 0, 0};
    }
    ExpressionKind kind() const noexcept { return kind_; }
    std::size_t variableIndex() const {
        if (kind_ != ExpressionKind::Variable) throw std::logic_error("node is not a variable");
        return index_;
    }
    double constantValue() const {
        if (kind_ != ExpressionKind::Constant) throw std::logic_error("node is not a constant");
        return value_;
    }
private:
    ExpressionNode(ExpressionKind kind, std::size_t index, double value) : kind_(kind), index_(index), value_(value) {}
    ExpressionKind kind_;
    std::size_t index_;
    double value_;
};

struct ExpressionLimits {
    std::size_t maxNodes = 1023;
    std::size_t maxDepth = 64; // leaf depth = 1; count all variable/constant/operator occurrences
    double maxAbsConstant = std::numeric_limits<double>::max();
};

enum class ExpressionStatus { Success, MathematicalInvalidity };
class ExpressionResult {
public:
    ExpressionStatus status() const noexcept { return failedNode_ ? ExpressionStatus::MathematicalInvalidity : ExpressionStatus::Success; }
    bool valid() const noexcept { return !failedNode_; }
    double value() const {
        if (!valid()) throw std::logic_error("invalid expression evaluation has no value");
        return value_;
    }
    std::optional<std::size_t> failedNode() const noexcept { return failedNode_; }
private:
    friend class Expression;
    explicit ExpressionResult(double value) : value_(value) {}
    explicit ExpressionResult(std::size_t failedNode) : value_(0), failedNode_(failedNode) {}
    double value_;
    std::optional<std::size_t> failedNode_;
};

class Expression {
public:
    // Copy caller tokens even for rvalues. No caller-owned writable storage is adopted.
    explicit Expression(std::span<const ExpressionNode> postfix, std::size_t variableCount,
                        ExpressionLimits limits = {}) : variables_(variableCount), limits_(limits) {
        validateLimits(limits);
        if (postfix.empty()) throw std::invalid_argument("expression must contain one complete tree");
        if (postfix.size() > limits.maxNodes) throw std::length_error("expression node limit exceeded");
        nodes_.assign(postfix.begin(), postfix.end());
        validate();
    }
    static Expression constant(double value, std::size_t variables = 0, ExpressionLimits limits = {}) {
        const auto node = ExpressionNode::constant(value);
        return Expression(std::span(&node, 1), variables, limits);
    }
    static Expression variable(std::size_t index, std::size_t variables = 1, ExpressionLimits limits = {}) {
        const auto node = ExpressionNode::variable(index);
        return Expression(std::span(&node, 1), variables, limits);
    }
    static Expression unary(ExpressionKind kind, const Expression& child, ExpressionLimits limits = {}) {
        child.requireUsable(); validateLimits(limits);
        const auto op = ExpressionNode::unary(kind);
        if (child.nodes_.size() >= limits.maxNodes) throw std::length_error("expression node limit exceeded");
        if (child.depth_ >= limits.maxDepth) throw std::length_error("expression depth limit exceeded");
        auto nodes = child.nodes(); nodes.push_back(op);
        return Expression(nodes, child.variables_, limits);
    }
    static Expression binary(ExpressionKind kind, const Expression& left, const Expression& right,
                             ExpressionLimits limits = {}) {
        left.requireUsable(); right.requireUsable();
        validateLimits(limits);
        const auto op = ExpressionNode::binary(kind);
        if (left.variables_ != right.variables_) throw std::invalid_argument("expression variable schemas differ");
        // Subtract before adding, so even SIZE_MAX configuration cannot wrap node counts.
        if (left.nodes_.size() > limits.maxNodes - 1 ||
            right.nodes_.size() > limits.maxNodes - 1 - left.nodes_.size())
            throw std::length_error("expression node limit exceeded");
        if (std::max(left.depth_, right.depth_) >= limits.maxDepth)
            throw std::length_error("expression depth limit exceeded");
        std::vector<ExpressionNode> nodes;
        nodes.reserve(left.nodes_.size() + right.nodes_.size() + 1);
        nodes.insert(nodes.end(), left.nodes_.begin(), left.nodes_.end());
        nodes.insert(nodes.end(), right.nodes_.begin(), right.nodes_.end());
        nodes.push_back(op);
        return Expression(nodes, left.variables_, limits);
    }
    Expression(const Expression& other) : Expression(other.nodes(), other.variableCount(), other.limits()) {}
    Expression(Expression&& other) noexcept = default;
    Expression& operator=(Expression other) noexcept { swap(other); return *this; }
    void swap(Expression& other) noexcept {
        nodes_.swap(other.nodes_); subtreeSizes_.swap(other.subtreeSizes_);
        std::swap(variables_, other.variables_); std::swap(depth_, other.depth_); std::swap(limits_, other.limits_);
    }
    // Moved-from objects may be assigned/destroyed; all inspection/execution detects emptiness.
    std::vector<ExpressionNode> nodes() const { requireUsable(); return nodes_; }
    std::size_t nodeCount() const { requireUsable(); return nodes_.size(); }
    std::size_t depth() const { requireUsable(); return depth_; }
    std::size_t variableCount() const { requireUsable(); return variables_; }
    ExpressionLimits limits() const { requireUsable(); return limits_; }
    Expression withConstant(std::size_t postfixIndex, double value) const {
        auto changed = nodes();
        if (changed.at(postfixIndex).kind() != ExpressionKind::Constant)
            throw std::invalid_argument("constant replacement requires a constant node");
        changed[postfixIndex] = ExpressionNode::constant(value);
        return Expression(changed, variables_, limits_);
    }

    ExpressionResult evaluate(std::span<const double> bindings) const {
        requireUsable();
        if (bindings.size() != variables_) throw std::invalid_argument("binding count differs from expression arity");
        for (double input : bindings)
            if (!std::isfinite(input)) throw std::invalid_argument("variable bindings must all be finite");
        std::vector<double> stack;
        stack.reserve(nodes_.size());
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            const auto& node = nodes_[i];
            if (node.kind() == ExpressionKind::Constant) stack.push_back(node.constantValue());
            else if (node.kind() == ExpressionKind::Variable) stack.push_back(bindings[node.variableIndex()]);
            else if (node.kind() == ExpressionKind::Log) {
                const double argument = stack.back();
                if (!(argument > 0.0)) return ExpressionResult(i);
                volatile double result = std::log(argument);
                if (!std::isfinite(result)) return ExpressionResult(i);
                stack.back() = result;
            } else {
                const double right = stack.back(); stack.pop_back();
                const double left = stack.back();
                // Each node rounds/stores as binary64 before testing or consuming the next node.
                // This barrier plus -ffp-contract=off prevents a cross-node fused operation.
                volatile double result = 0;
                switch (node.kind()) {
                case ExpressionKind::Add: result = left + right; break;
                case ExpressionKind::Subtract: result = left - right; break;
                case ExpressionKind::Multiply: result = left * right; break;
                case ExpressionKind::Divide:
                    if (right == 0.0) return ExpressionResult(i); // includes either signed zero
                    result = left / right; break;
                default: throw std::logic_error("unreachable validated expression operator");
                }
                const double value = result;
                if (!std::isfinite(value)) return ExpressionResult(i);
                stack.back() = value;
            }
        }
        return ExpressionResult(stack.back());
    }

    std::string toString() const {
        requireUsable();
        struct Frame { std::size_t root; unsigned int phase; };
        std::vector<Frame> stack;
        stack.reserve(depth_);
        stack.push_back({nodes_.size() - 1, 0});
        std::string out;
        while (!stack.empty()) {
            auto& frame = stack.back();
            const auto& node = nodes_[frame.root];
            if (node.kind() == ExpressionKind::Constant) {
                out += constantText(node.constantValue()); stack.pop_back();
            } else if (node.kind() == ExpressionKind::Variable) {
                char buffer[std::numeric_limits<std::size_t>::digits10 + 2];
                const auto result = std::to_chars(buffer, buffer + sizeof buffer, node.variableIndex());
                if (result.ec != std::errc{}) throw std::runtime_error("variable export failed");
                out += 'x'; out.append(buffer, result.ptr); stack.pop_back();
            } else if (node.kind() == ExpressionKind::Log) {
                if (frame.phase == 0) {
                    out += "log("; frame.phase = 1; stack.push_back({frame.root - 1, 0});
                } else { out += ')'; stack.pop_back(); }
            } else if (frame.phase == 0) {
                out += '('; frame.phase = 1;
                const auto right = frame.root - 1;
                stack.push_back({right - subtreeSizes_[right], 0});
            } else if (frame.phase == 1) {
                out += node.kind() == ExpressionKind::Add ? " + " : node.kind() == ExpressionKind::Subtract ? " - " : node.kind() == ExpressionKind::Multiply ? " * " : " / ";
                frame.phase = 2;
                stack.push_back({frame.root - 1, 0});
            } else { out += ')'; stack.pop_back(); }
        }
        return out;
    }
private:
    static void validateLimits(ExpressionLimits limits) {
        if (limits.maxNodes == 0 || limits.maxDepth == 0 || !std::isfinite(limits.maxAbsConstant) || limits.maxAbsConstant < 0)
            throw std::invalid_argument("expression limits must be positive counts and a finite nonnegative constant bound");
    }
    void requireUsable() const {
        if (nodes_.empty()) throw std::logic_error("moved-from expression has no executable tree");
    }
    void validate() {
        struct Item { std::size_t depth; std::size_t start; };
        std::vector<Item> stack;
        stack.reserve(nodes_.size());
        subtreeSizes_.reserve(nodes_.size());
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            const auto& node = nodes_[i];
            Item item{1, i};
            switch (node.kind()) {
            case ExpressionKind::Constant:
                if (!std::isfinite(node.constantValue()) || std::abs(node.constantValue()) > limits_.maxAbsConstant)
                    throw std::invalid_argument("expression constant is non-finite or outside its bound");
                stack.push_back(item); break;
            case ExpressionKind::Variable:
                if (node.variableIndex() >= variables_) throw std::invalid_argument("expression variable index outside schema");
                stack.push_back(item); break;
            default: {
                const auto arity = expressionArity(node.kind());
                if (stack.size() < arity) throw std::invalid_argument("expression operator lacks operands");
                auto childDepth = stack.back().depth;
                if (arity == 2) {
                    stack.pop_back(); childDepth = std::max(childDepth, stack.back().depth);
                }
                if (childDepth >= limits_.maxDepth) throw std::length_error("expression depth limit exceeded");
                item = {childDepth + 1, stack.back().start};
                stack.back() = item;
                break;
            }
            }
            subtreeSizes_.push_back(i - item.start + 1);
        }
        if (stack.size() != 1) throw std::invalid_argument("postfix input has unused expression fragments");
        depth_ = stack.back().depth;
    }
    static std::string constantText(double value) {
        char buffer[128];
        const auto result = std::to_chars(buffer, buffer + sizeof buffer, value,
            std::chars_format::general, std::numeric_limits<double>::max_digits10);
        if (result.ec != std::errc{}) throw std::runtime_error("constant export failed");
        std::string text(buffer, result.ptr);
        if (text.find_first_of(".eE") == std::string::npos) text += ".0";
        if (std::signbit(value)) text = '(' + text + ')';
        return text;
    }
    std::vector<ExpressionNode> nodes_;
    std::vector<std::size_t> subtreeSizes_;
    std::size_t variables_;
    ExpressionLimits limits_;
    std::size_t depth_ = 0;
};
