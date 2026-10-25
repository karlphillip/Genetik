#pragma once
#include <algorithm>
#include "detail/ScientificScalar.hpp"
#include "detail/ScalarOperators.hpp"
#include <bit>
#include <cstdint>
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

// Checked primal kernels shared by ordinary and partial-derivative evaluation.
namespace genetik_detail {
inline ScientificKind scientificKind(ExpressionKind kind) {
    switch (kind) {
    case ExpressionKind::Sin: return ScientificKind::Sin;
    case ExpressionKind::Cos: return ScientificKind::Cos;
    case ExpressionKind::Exp: return ScientificKind::Exp;
    case ExpressionKind::Sqrt: return ScientificKind::Sqrt;
    case ExpressionKind::Powi: return ScientificKind::Powi;
    default: throw std::logic_error("not a scientific scalar operation");
    }
}
template<class Charge>
std::optional<double> scalarBinary(ExpressionKind kind, double left, double right, Charge charge) {
    if (kind == ExpressionKind::Divide && right == 0.0) return {};
    if(kind==ExpressionKind::Pow&&((left<0&&std::trunc(right)!=right)||(left==0&&!(right>0))))return {};
    if(kind==ExpressionKind::Atan2&&left==0&&right==0)return {};
    charge(); volatile double result = 0;
    switch (kind) {
    case ExpressionKind::Add: result = left + right; break;
    case ExpressionKind::Subtract: result = left - right; break;
    case ExpressionKind::Multiply: result = left * right; break;
    case ExpressionKind::Divide: result = left / right; break;
    case ExpressionKind::Pow:result=std::pow(left,right);break;
    case ExpressionKind::Atan2:result=std::atan2(left,right);break;
    case ExpressionKind::Min:result=right<left?right:left;break;
    case ExpressionKind::Max:result=left<right?right:left;break;
    default: throw std::logic_error("not a binary scalar operation");
    }
    if (!std::isfinite(result)) return {};
    return result;
}
template<class Charge>
std::optional<double> scalarTernary(ExpressionKind kind,double x,double low,double high,Charge charge){
    if(kind!=ExpressionKind::Clamp)throw std::logic_error("unsupported ternary scalar operation");
    if(low>high)return {};
    charge();return x<low?low:x>high?high:x;
}
template<class Charge, class PowerStep = IgnorePowerStep>
std::optional<double> scalarUnary(ExpressionKind kind, double argument, unsigned exponent,
                                  Charge charge, PowerStep step = {}) {
    if (kind == ExpressionKind::Log) {
        if (!(argument > 0.0)) return {};
        charge(); volatile double result = std::log(argument);
        if (!std::isfinite(result)) return {};
        return result;
    }
    if(kind==ExpressionKind::NegativePowi){
        if(!exponent||exponent>8)throw std::invalid_argument("negative power magnitude1..8");
        auto inverse=scalarBinary(ExpressionKind::Divide,1,argument,charge);if(!inverse)return {};
        return scientificScalar(ScientificKind::Powi,*inverse,exponent,charge);
    }
    if(kind>=ExpressionKind::Cbrt){
        if((kind==ExpressionKind::Log1p&&!(argument>-1))||((kind==ExpressionKind::Log2||kind==ExpressionKind::Log10)&&!(argument>0))||((kind==ExpressionKind::Asin||kind==ExpressionKind::Acos)&&(argument< -1||argument>1)))return {};
        charge();volatile double value=0;
        switch(kind){case ExpressionKind::Cbrt:value=std::cbrt(argument);break;case ExpressionKind::Log1p:value=std::log1p(argument);break;case ExpressionKind::Log2:value=std::log2(argument);break;case ExpressionKind::Log10:value=std::log10(argument);break;case ExpressionKind::Expm1:value=std::expm1(argument);break;case ExpressionKind::Tan:value=std::tan(argument);break;case ExpressionKind::Asin:value=std::asin(argument);break;case ExpressionKind::Acos:value=std::acos(argument);break;case ExpressionKind::Atan:value=std::atan(argument);break;case ExpressionKind::Sinh:value=std::sinh(argument);break;case ExpressionKind::Cosh:value=std::cosh(argument);break;case ExpressionKind::Tanh:value=std::tanh(argument);break;case ExpressionKind::Abs:value=std::abs(argument);break;default:throw std::logic_error("not an advanced unary operation");}
        return std::isfinite(value)?std::optional<double>(value):std::nullopt;
    }
    return scientificScalar(scientificKind(kind), argument, exponent, charge, step);
}
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
        if (expressionPowerMetadata(kind) || expressionArity(kind) != 1) throw std::invalid_argument("expected a supported unary expression operator");
        return {kind, 0, 0};
    }
    static ExpressionNode power(unsigned exponent) {
        genetik_detail::validateExponent(exponent);
        return {ExpressionKind::Powi, exponent, 0};
    }
    static ExpressionNode signedPower(int exponent){if(exponent>=0)return power(static_cast<unsigned>(exponent));if(exponent< -8)throw std::invalid_argument("negative powi exponent must be -8..-1");return {ExpressionKind::NegativePowi,static_cast<unsigned>(-exponent),0};}
    static ExpressionNode ternary(ExpressionKind kind){if(expressionArity(kind)!=3)throw std::invalid_argument("ternary operation required");return {kind,0,0};}
    unsigned exponent() const {
        if (!expressionPowerMetadata(kind_)) throw std::logic_error("node is not integer power");
        return static_cast<unsigned>(index_);
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
    std::size_t maxArithmeticOperations = std::numeric_limits<std::size_t>::max();
};

enum class ExpressionStatus { Success, MathematicalInvalidity };
class ExpressionResult {
public:
    ExpressionStatus status() const noexcept { return failedNode_ ? ExpressionStatus::MathematicalInvalidity : ExpressionStatus::Success; }
    bool valid() const noexcept { return !failedNode_; }
    std::size_t arithmeticOperations() const noexcept { return work_; }
    double value() const {
        if (!valid()) throw std::logic_error("invalid expression evaluation has no value");
        return value_;
    }
    std::optional<std::size_t> failedNode() const noexcept { return failedNode_; }
private:
    friend class Expression;
    explicit ExpressionResult(double value, std::size_t work) : value_(value), work_(work) {}
    explicit ExpressionResult(std::size_t failedNode, std::size_t work) : value_(0), failedNode_(failedNode), work_(work) {}
    double value_;
    std::optional<std::size_t> failedNode_;
    std::size_t work_;
};

namespace genetik_detail { class EvaluationWorkspace; class DerivativeEvaluation; }

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
    // Exact typed representation, including signed-zero constant bits and binding schema.
    // Limits are constraints on a value, not expression content. Moved-from values are not usable.
    bool sameRepresentation(const Expression& other) const {
        requireUsable();other.requireUsable();
        if(variables_!=other.variables_ || nodes_.size()!=other.nodes_.size())return false;
        for(std::size_t i=0;i<nodes_.size();++i) {
            const auto& a=nodes_[i];const auto& b=other.nodes_[i];
            if(a.kind()!=b.kind())return false;
            if(expressionPowerMetadata(a.kind()) && a.exponent()!=b.exponent())return false;
            if(a.kind()==ExpressionKind::Variable && a.variableIndex()!=b.variableIndex())return false;
            if(a.kind()==ExpressionKind::Constant && std::bit_cast<std::uint64_t>(a.constantValue())!=std::bit_cast<std::uint64_t>(b.constantValue()))return false;
        }
        return true;
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
    static Expression power(const Expression& child, unsigned exponent, ExpressionLimits limits = {}) {
        genetik_detail::validateExponent(exponent);
        auto nodes = child.nodes(); nodes.push_back(ExpressionNode::power(exponent));
        return Expression(nodes, child.variableCount(), limits);
    }
    static Expression integerPower(const Expression& child,int exponent,ExpressionLimits limits={}){
        if(exponent>=0)return power(child,static_cast<unsigned>(exponent),limits);
        auto nodes=child.nodes();nodes.push_back(ExpressionNode::signedPower(exponent));return Expression(nodes,child.variableCount(),limits);
    }
    static Expression ternary(ExpressionKind kind,const Expression& a,const Expression& b,const Expression& c,ExpressionLimits limits={}){
        validateLimits(limits);if(a.variableCount()!=b.variableCount()||a.variableCount()!=c.variableCount())throw std::invalid_argument("ternary schema mismatch");std::size_t total=1;for(const auto* e:{&a,&b,&c}){if(e->nodeCount()>limits.maxNodes-total)throw std::length_error("ternary node limit");total+=e->nodeCount();}std::vector<ExpressionNode> nodes;nodes.reserve(total);for(const auto* e:{&a,&b,&c}){auto part=e->nodes();nodes.insert(nodes.end(),part.begin(),part.end());}nodes.push_back(ExpressionNode::ternary(kind));return Expression(nodes,a.variableCount(),limits);
    }
    Expression withExponent(std::size_t index, unsigned exponent) const {
        genetik_detail::validateExponent(exponent);
        auto changed = nodes();
        if (changed.at(index).kind() != ExpressionKind::Powi) throw std::invalid_argument("exponent replacement requires Powi");
        changed[index] = ExpressionNode::power(exponent);
        return Expression(changed, variables_, limits_);
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
        std::swap(variables_, other.variables_); std::swap(depth_, other.depth_); std::swap(work_, other.work_); std::swap(limits_, other.limits_);
    }
    // Moved-from objects may be assigned/destroyed; all inspection/execution detects emptiness.
    std::vector<ExpressionNode> nodes() const { requireUsable(); return nodes_; }
    std::size_t nodeCount() const { requireUsable(); return nodes_.size(); }
    std::size_t depth() const { requireUsable(); return depth_; }
    std::size_t variableCount() const { requireUsable(); return variables_; }
    std::size_t arithmeticCost() const { requireUsable(); return work_; }
    ExpressionLimits limits() const { requireUsable(); return limits_; }
    Expression withConstant(std::size_t postfixIndex, double value) const {
        auto changed = nodes();
        if (changed.at(postfixIndex).kind() != ExpressionKind::Constant)
            throw std::invalid_argument("constant replacement requires a constant node");
        changed[postfixIndex] = ExpressionNode::constant(value);
        return Expression(changed, variables_, limits_);
    }

    ExpressionResult evaluate(std::span<const double> bindings) const {
        std::vector<double> stack;
        return evaluateWithStack(bindings, stack);
    }

private:
    friend class genetik_detail::EvaluationWorkspace;
    friend class genetik_detail::DerivativeEvaluation;
    // One interpreter for convenience calls and invocation-local batch evaluation.
    // Validate before allocating, retain capacity, and reset logical contents even
    // after an invalid prior row. reserve does not initialize or index raw capacity.
    ExpressionResult evaluateWithStack(std::span<const double> bindings, std::vector<double>& stack) const {
        requireUsable();
        if (bindings.size() != variables_) throw std::invalid_argument("binding count differs from expression arity");
        for (double input : bindings)
            if (!std::isfinite(input)) throw std::invalid_argument("variable bindings must all be finite");
        stack.clear();
        stack.reserve(nodes_.size());
        std::size_t executed=0;
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            const auto& node = nodes_[i];
            if (node.kind() == ExpressionKind::Constant) stack.push_back(node.constantValue());
            else if (node.kind() == ExpressionKind::Variable) stack.push_back(bindings[node.variableIndex()]);
            else if (expressionArity(node.kind()) == 1) {
                const double argument = stack.back();
                const auto value = genetik_detail::scalarUnary(node.kind(), argument,
                    expressionPowerMetadata(node.kind()) ? node.exponent() : 0, [&executed]{++executed;});
                if (!value) return ExpressionResult(i, executed);
                stack.back() = *value;
            } else if(expressionArity(node.kind())==3){
                const double high=stack.back();stack.pop_back();const double low=stack.back();stack.pop_back();const double x=stack.back();
                const auto value=genetik_detail::scalarTernary(node.kind(),x,low,high,[&executed]{++executed;});if(!value)return ExpressionResult(i,executed);stack.back()=*value;
            } else {
                const double right = stack.back(); stack.pop_back();
                const double left = stack.back();
                const auto value = genetik_detail::scalarBinary(node.kind(), left, right, [&executed]{++executed;});
                if (!value) return ExpressionResult(i, executed);
                stack.back() = *value;
            }
        }
        return ExpressionResult(stack.back(), executed);
    }

public:
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
            } else if(node.kind()>=ExpressionKind::NegativePowi){
                const auto arity=expressionArity(node.kind());
                if(frame.phase==0){out+=(node.kind()==ExpressionKind::NegativePowi?"powi":std::string(scalarOperatorDescription(node.kind()).name))+"(";frame.phase=1;std::size_t child=frame.root-1;for(std::size_t j=1;j<arity;++j)child-=subtreeSizes_[child];stack.push_back({child,0});}
                else if(frame.phase<arity){out+=", ";const auto next=frame.phase++;std::size_t child=frame.root-1;for(std::size_t j=next+1;j<arity;++j)child-=subtreeSizes_[child];stack.push_back({child,0});}
                else {if(node.kind()==ExpressionKind::NegativePowi)out+=",-"+std::to_string(node.exponent());out+=')';stack.pop_back();}
            } else if (expressionArity(node.kind()) == 1) {
                if (frame.phase == 0) {
                    out += node.kind() == ExpressionKind::Log ? "log(" : genetik_detail::scientificName(genetik_detail::scientificKind(node.kind())) + "("; frame.phase = 1; stack.push_back({frame.root - 1, 0});
                } else { if (node.kind() == ExpressionKind::Powi) out += ',' + std::to_string(node.exponent()); out += ')'; stack.pop_back(); }
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
                const std::size_t cost = expressionOperationCost(node.kind(),expressionPowerMetadata(node.kind())?node.exponent():0);
                if (cost > limits_.maxArithmeticOperations - work_) throw std::length_error("expression arithmetic work limit exceeded");
                work_ += cost;
                if (stack.size() < arity) throw std::invalid_argument("expression operator lacks operands");
                auto childDepth = stack.back().depth;
                for(std::size_t j=1;j<arity;++j){stack.pop_back();childDepth=std::max(childDepth,stack.back().depth);}
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
    std::size_t depth_ = 0, work_ = 0;
};

// Internal implementation detail, not a public workspace/lifetime API. Owners keep
// this lexical to one batch/loss invocation; no expression, binding or result alias
// is retained. Growth happens before interpreting any node of the current row.
namespace genetik_detail {
class EvaluationWorkspace final {
public:
    ExpressionResult evaluate(const Expression& expression, std::span<const double> bindings) {
        return expression.evaluateWithStack(bindings, stack_);
    }
private:
    std::vector<double> stack_;
};
}
