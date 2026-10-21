#pragma once
#include "TypedValue.hpp"
#include "detail/ScientificScalar.hpp"
#include <algorithm>
#include <optional>
#include <cstdint>

// Operators are explicit; vector products never use an ambiguous Multiply token.
enum class TypedExpressionKind { Variable, Constant, Add, Subtract, Multiply, Divide, Log, Dot, MatVec, MatMul, Transpose, Sin, Cos, Exp, Sqrt, Powi };
inline std::size_t typedExpressionArity(TypedExpressionKind kind) {
    switch (kind) {
    case TypedExpressionKind::Variable: case TypedExpressionKind::Constant: return 0;
    case TypedExpressionKind::Log: case TypedExpressionKind::Transpose:
    case TypedExpressionKind::Sin: case TypedExpressionKind::Cos: case TypedExpressionKind::Exp:
    case TypedExpressionKind::Sqrt: case TypedExpressionKind::Powi: return 1;
    case TypedExpressionKind::Add: case TypedExpressionKind::Subtract:
    case TypedExpressionKind::Multiply: case TypedExpressionKind::Divide:
    case TypedExpressionKind::Dot: case TypedExpressionKind::MatVec: case TypedExpressionKind::MatMul: return 2;
    }
    throw std::invalid_argument("unsupported typed expression operator");
}
namespace genetik_typed_detail {
// The validator and constructive grammar share exactly one operand-shape rule.
// Incompatibility is an ordinary planning rejection; public construction still throws.
inline std::optional<TypedShape> operationShape(TypedExpressionKind kind,TypedShape a,TypedShape b) {
    using K=TypedExpressionKind;using V=TypedValueKind;

    if ((kind == K::Add || kind == K::Subtract) && a == b) return a;
    if (kind == K::Multiply) {
        if (a.kind() == V::Scalar) return b;
        if (b.kind() == V::Scalar) return a;
    }
    if ((kind == K::Divide || kind == K::Log || kind == K::Sin || kind == K::Cos || kind == K::Exp || kind == K::Sqrt || kind == K::Powi) && a.kind() == V::Scalar && b.kind() == V::Scalar) return TypedShape::scalar();
    if (kind == K::Dot && a.kind() == V::Vector && a == b) return TypedShape::scalar();
    if (kind == K::MatVec && a.kind() == V::Matrix && b.kind() == V::Vector && a.columns() == b.length()) return TypedShape::vector(a.rows());
    if (kind == K::MatMul && a.kind() == V::Matrix && b.kind() == V::Matrix && a.columns() == b.rows()) return TypedShape::matrix(a.rows(), b.columns());
    if (kind == K::Transpose && a.kind() == V::Matrix) return TypedShape::matrix(a.columns(), a.rows());
    return {};
}
}
class TypedExpressionNode {
public:
    static TypedExpressionNode variable(std::size_t index) { return {TypedExpressionKind::Variable, index, std::nullopt}; }
    static TypedExpressionNode constant(const TypedValue& value) { return {TypedExpressionKind::Constant, 0, value}; }
    static TypedExpressionNode unary(TypedExpressionKind kind) {
        if (kind == TypedExpressionKind::Powi || typedExpressionArity(kind) != 1) throw std::invalid_argument("expected unary typed operator");
        return {kind, 0, std::nullopt};
    }
    static TypedExpressionNode binary(TypedExpressionKind kind) {
        if (typedExpressionArity(kind) != 2) throw std::invalid_argument("expected binary typed operator");
        return {kind, 0, std::nullopt};
    }
    static TypedExpressionNode power(unsigned exponent) {
        genetik_detail::validateExponent(exponent);
        return {TypedExpressionKind::Powi, exponent, std::nullopt};
    }
    unsigned exponent() const {
        if (kind_ != TypedExpressionKind::Powi) throw std::logic_error("typed node is not integer power");
        return static_cast<unsigned>(index_);
    }
    TypedExpressionNode(const TypedExpressionNode&) = default;
    TypedExpressionNode(TypedExpressionNode&&) noexcept = default;
    TypedExpressionNode& operator=(TypedExpressionNode other) noexcept { swap(other); return *this; }
    void swap(TypedExpressionNode& other) noexcept {
        std::swap(kind_, other.kind_); std::swap(index_, other.index_); constant_.swap(other.constant_);
    }
    TypedExpressionKind kind() const noexcept { return kind_; }
    std::size_t variableIndex() const {
        if (kind_ != TypedExpressionKind::Variable) throw std::logic_error("typed node is not a variable");
        return index_;
    }
    const TypedValue& constantValue() const {
        if (kind_ != TypedExpressionKind::Constant) throw std::logic_error("typed node is not a constant");
        return *constant_;
    }
private:
    TypedExpressionNode(TypedExpressionKind kind, std::size_t index, std::optional<TypedValue> value)
        : kind_(kind), index_(index), constant_(std::move(value)) {}
    TypedExpressionKind kind_;
    std::size_t index_;
    std::optional<TypedValue> constant_;
};
struct TypedExpressionLimits {
    TypedValueLimits values;
    std::size_t maxNodes = 1023;
    std::size_t maxDepth = 64;
    std::size_t maxVariables = 64;
    std::size_t maxBindingCoefficients = 1000000;
    std::size_t maxConstantNodes = 256;
    std::size_t maxConstantCoefficients = 65536;
    double maxAbsConstant = std::numeric_limits<double>::max();
    std::size_t maxTemporaryCoefficients = 262144;
    std::size_t maxArithmeticOperations = 1000000;
};
struct TypedEvaluationLimits {
    std::size_t maxArithmeticOperations = std::numeric_limits<std::size_t>::max();
    std::size_t maxTemporaryCoefficients = std::numeric_limits<std::size_t>::max();
};
enum class TypedEvaluationStatus { Success, MathematicalInvalidity, ResourceLimit };
class TypedEvaluationResult {
public:
    TypedEvaluationResult(const TypedEvaluationResult&) = default;
    TypedEvaluationResult(TypedEvaluationResult&& other) noexcept
        : status_(other.status_), value_(std::move(other.value_)), work_(other.work_),
          node_(other.node_), coefficient_(other.coefficient_), usable_(std::exchange(other.usable_, false)) {}
    TypedEvaluationResult& operator=(TypedEvaluationResult other) noexcept { swap(other); return *this; }
    void swap(TypedEvaluationResult& other) noexcept {
        std::swap(status_, other.status_); value_.swap(other.value_); std::swap(work_, other.work_);
        node_.swap(other.node_); coefficient_.swap(other.coefficient_); std::swap(usable_, other.usable_);
    }
    TypedEvaluationStatus status() const { usable(); return status_; }
    bool valid() const { return status() == TypedEvaluationStatus::Success; }
    const TypedValue& value() const {
        if (!valid()) throw std::logic_error("unsuccessful typed evaluation has no value");
        return *value_;
    }
    std::size_t arithmeticOperations() const { usable(); return work_; }
    std::optional<std::size_t> failedNode() const { usable(); return node_; }
    std::optional<std::size_t> failedCoefficient() const { usable(); return coefficient_; }
private:
    friend class TypedExpression;
    explicit TypedEvaluationResult(TypedValue&& value, std::size_t work)
        : status_(TypedEvaluationStatus::Success), value_(std::move(value)), work_(work) {}
    TypedEvaluationResult(TypedEvaluationStatus status, std::size_t work,
                          std::optional<std::size_t> node = {}, std::optional<std::size_t> coefficient = {})
        : status_(status), work_(work), node_(node), coefficient_(coefficient) {}
    void usable() const { if (!usable_) throw std::logic_error("moved-from typed evaluation result"); }
    TypedEvaluationStatus status_;
    std::optional<TypedValue> value_;
    std::size_t work_;
    std::optional<std::size_t> node_, coefficient_;
    bool usable_ = true;
};

// Immutable, owned flat postfix expression with a completely checked execution plan.
class TypedExpression {
    using K = TypedExpressionKind;
    using V = TypedValueKind;
    struct Info { TypedShape shape; std::size_t depth, start; };
public:
    explicit TypedExpression(std::span<const TypedExpressionNode> postfix,
                             std::span<const TypedShape> schema = {}, TypedExpressionLimits limits = {})
        : limits_(limits) {
        validateLimits(limits);
        if (postfix.empty()) throw std::invalid_argument("typed expression requires one complete tree");
        using namespace genetik_typed_detail;
        bound(postfix.size(), limits.maxNodes, "typed node limit");
        bound(postfix.size(), nodes_.max_size(), "typed node storage capacity");
        bound(schema.size(), limits.maxVariables, "typed schema variable limit");
        bound(schema.size(), schema_.max_size(), "typed schema storage capacity");
        for (auto shape : schema) {
            validateShape(shape, limits.values);
            bindingCoefficients_ = add(bindingCoefficients_, shape.coefficients(), "typed binding count overflow");
            bound(bindingCoefficients_, limits.maxBindingCoefficients, "typed binding coefficient limit");
        }
        analyze(postfix, schema); // shape, constant and full-work bounds before payload copying
        schema_.assign(schema.begin(), schema.end());
        nodes_.assign(postfix.begin(), postfix.end());
    }
    static TypedExpression variable(std::size_t index, std::span<const TypedShape> schema,
                                    TypedExpressionLimits limits = {}) {
        const auto node = TypedExpressionNode::variable(index);
        return TypedExpression(std::span(&node, 1), schema, limits);
    }
    static TypedExpression constant(const TypedValue& value, std::span<const TypedShape> schema = {},
                                    TypedExpressionLimits limits = {}) {
        // Check coefficient limits before creating the owned token copy.
        validateLimits(limits); checkConstant(value, limits);
        genetik_typed_detail::bound(1, limits.maxConstantNodes, "typed constant node limit");
        genetik_typed_detail::bound(value.shape().coefficients(), limits.maxConstantCoefficients, "typed constant coefficient limit");
        const auto node = TypedExpressionNode::constant(value);
        return TypedExpression(std::span(&node, 1), schema, limits);
    }
    static TypedExpression unary(K kind, const TypedExpression& child, TypedExpressionLimits limits = {}) {
        (void)TypedExpressionNode::unary(kind);
        return compose(kind, child, nullptr, limits);
    }
    static TypedExpression power(const TypedExpression& child, unsigned exponent, TypedExpressionLimits limits = {}) {
        genetik_detail::validateExponent(exponent);
        return compose(K::Powi, child, nullptr, limits, exponent);
    }
    TypedExpression withExponent(std::size_t index, unsigned exponent) const {
        genetik_detail::validateExponent(exponent);
        auto changed = nodes();
        if (changed.at(index).kind() != K::Powi) throw std::invalid_argument("typed exponent replacement requires Powi");
        changed[index] = TypedExpressionNode::power(exponent);
        return TypedExpression(changed, schema_, limits_);
    }
    static TypedExpression binary(K kind, const TypedExpression& left, const TypedExpression& right,
                                  TypedExpressionLimits limits = {}) {
        return compose(kind, left, &right, limits);
    }
    TypedExpression(const TypedExpression& other)
        : TypedExpression(other.nodes(), other.schema(), other.limits()) {}
    TypedExpression(TypedExpression&&) noexcept = default;
    TypedExpression& operator=(TypedExpression other) noexcept { swap(other); return *this; }
    void swap(TypedExpression& other) noexcept {
        nodes_.swap(other.nodes_); schema_.swap(other.schema_); info_.swap(other.info_);
        std::swap(limits_, other.limits_); std::swap(bindingCoefficients_, other.bindingCoefficients_);
        std::swap(constants_, other.constants_); std::swap(constantCoefficients_, other.constantCoefficients_);
        std::swap(work_, other.work_); std::swap(temporary_, other.temporary_); std::swap(stackValues_, other.stackValues_);
    }
    std::vector<TypedExpressionNode> nodes() const { usable(); return nodes_; }
    std::vector<TypedShape> schema() const { usable(); return schema_; }
    TypedShape shape() const { usable(); return info_.back().shape; }
    TypedShape nodeShape(std::size_t index) const { usable(); return info_.at(index).shape; }
    std::size_t nodeCount() const { usable(); return nodes_.size(); }
    std::size_t depth() const { usable(); return info_.back().depth; }
    std::size_t constantCount() const { usable(); return constants_; }
    std::size_t constantCoefficients() const { usable(); return constantCoefficients_; }
    std::size_t bindingCoefficients() const { usable(); return bindingCoefficients_; }
    // Full valid execution cost, distinct from a call's actually executed primitives.
    std::size_t arithmeticCost() const { usable(); return work_; }
    std::size_t temporaryCoefficients() const { usable(); return temporary_; }
    TypedExpressionLimits limits() const { usable(); return limits_; }

    // Borrow bindings only for this synchronous call. Return an owned value;
    // validate all bindings before plan admission, then use lexical private scratch.
    TypedEvaluationResult evaluate(std::span<const TypedValue> bindings, TypedEvaluationLimits budget = {}) const {
        return evaluateImpl(bindings,budget,nullptr);
    }
    // Optional invocation-exclusive diagnostic. Charge primitives as they begin,
    // including work before an operational exception; no engine admission or RNG.
    TypedEvaluationResult evaluateWithWork(std::span<const TypedValue> bindings,std::uint64_t& arithmetic,
                                           TypedEvaluationLimits budget = {}) const {
        return evaluateImpl(bindings,budget,&arithmetic);
    }
private:
    TypedEvaluationResult evaluateImpl(std::span<const TypedValue> bindings,TypedEvaluationLimits budget,
                                       std::uint64_t* arithmetic) const {
        usable();
        if (bindings.size() != schema_.size()) throw std::invalid_argument("typed binding count differs from schema");
        for (std::size_t i = 0; i < bindings.size(); ++i) {
            if (bindings[i].shape() != schema_[i]) throw std::invalid_argument("typed binding shape differs from schema");
            for (double x : bindings[i].coefficients())
                if (!std::isfinite(x)) throw std::invalid_argument("typed bindings must be finite");
        }
        if (work_ > budget.maxArithmeticOperations || temporary_ > budget.maxTemporaryCoefficients)
            return TypedEvaluationResult(TypedEvaluationStatus::ResourceLimit, 0);
        std::vector<TypedValue> stack;
        stack.reserve(stackValues_);
        std::size_t executed = 0;
        // All operands are finite. Domain checks precede the corresponding primitive.
        auto charge = [&executed,arithmetic] {
            if(arithmetic) {
                if(*arithmetic==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("typed primitive diagnostic counter exhausted");
                ++*arithmetic;
            }
            ++executed; // bounded by the validated full plan
        };
        auto primitive = [&charge](K kind, double a, double b) -> std::optional<double> {
            if ((kind == K::Divide && b == 0.0) || (kind == K::Log && !(a > 0.0))) return {};
            charge();
            volatile double result = 0;
            switch (kind) {
            case K::Add: result = a + b; break;
            case K::Subtract: result = a - b; break;
            case K::Multiply: result = a * b; break;
            case K::Divide: result = a / b; break;
            case K::Log: result = std::log(a); break;
            default: throw std::logic_error("unreachable typed primitive");
            }
            const double value = result;
            if (!std::isfinite(value)) return {};
            return value;
        };
        for (std::size_t index = 0; index < nodes_.size(); ++index) {
            const auto& node = nodes_[index];
            if (node.kind() == K::Constant) { stack.push_back(node.constantValue()); continue; }
            if (node.kind() == K::Variable) { stack.push_back(bindings[node.variableIndex()]); continue; }
            const auto arity = typedExpressionArity(node.kind());
            const auto first = stack.size() - arity;
            const auto& left = stack[first];
            const auto& right = stack.back();
            const auto outShape = info_[index].shape;
            std::vector<double> output(outShape.coefficients()); // size, not only reserve
            auto invalid = [&](std::size_t coefficient) {
                return TypedEvaluationResult(TypedEvaluationStatus::MathematicalInvalidity, executed, index, coefficient);
            };
            const auto a = left.coefficients(), b = right.coefficients();
            for (std::size_t element = 0; element < output.size(); ++element) {
                std::optional<double> value;
                if (isScientific(node.kind())) {
                    value = genetik_detail::scientificScalar(scientificKind(node.kind()), a[0],
                        node.kind() == K::Powi ? node.exponent() : 0, charge);
                    if (!value) return invalid(element);
                    output[element] = *value; continue;
                }
                if (node.kind() == K::Transpose) {
                    const auto row = element / outShape.columns(), col = element % outShape.columns();
                    output[element] = a[col * left.shape().columns() + row];
                    continue;
                }
                if (node.kind() == K::Dot || node.kind() == K::MatVec || node.kind() == K::MatMul) {
                    const auto inner = node.kind() == K::Dot ? left.shape().length() : left.shape().columns();
                    const auto row = element / outShape.columns(), col = element % outShape.columns();
                    auto product = [&](std::size_t k) {
                        const auto ai = node.kind() == K::Dot ? k : row * inner + k;
                        const auto bi = node.kind() == K::MatMul ? k * outShape.columns() + col : k;
                        return primitive(K::Multiply, a[ai], b[bi]);
                    };
                    value = product(0); // no artificial addition to +0
                    if (!value) return invalid(element);
                    for (std::size_t k = 1; k < inner; ++k) {
                        const auto next = product(k);
                        if (!next) return invalid(element);
                        value = primitive(K::Add, *value, *next);
                        if (!value) return invalid(element);
                    }
                } else {
                    const auto ai = left.shape().kind() == V::Scalar ? 0 : element;
                    const auto bi = right.shape().kind() == V::Scalar ? 0 : element;
                    value = primitive(node.kind(), a[ai], b[bi]);
                    if (!value) return invalid(element);
                }
                output[element] = *value;
            }
            // No operand references are used after these pops. Output is privately adopted.
            for (std::size_t i = 0; i < arity; ++i) stack.pop_back();
            stack.push_back(TypedValue(outShape, std::move(output)));
        }
        return TypedEvaluationResult(std::move(stack.back()), executed);
    }

public:
    std::string schemaString() const {
        usable(); std::string out = "schema{";
        for (std::size_t i = 0; i < schema_.size(); ++i) {
            if (i) out += ',';
            out += 'x' + genetik_typed_detail::integerText(i) + ':' + schema_[i].toString();
        }
        return out + '}';
    }
    std::string toString() const {
        usable();
        struct Frame { std::size_t node; unsigned phase; };
        std::vector<Frame> frames; frames.reserve(depth()); frames.push_back({nodes_.size() - 1, 0});
        std::string out;
        while (!frames.empty()) {
            auto& frame = frames.back(); const auto& node = nodes_[frame.node];
            if (node.kind() == K::Constant) { out += node.constantValue().toString(); frames.pop_back(); }
            else if (node.kind() == K::Variable) { out += 'x' + genetik_typed_detail::integerText(node.variableIndex()); frames.pop_back(); }
            else if (typedExpressionArity(node.kind()) == 1) {
                if (!frame.phase) { out += node.kind() == K::Log ? "log(" : node.kind() == K::Transpose ? "transpose(" : genetik_detail::scientificName(scientificKind(node.kind())) + "("; frame.phase = 1; frames.push_back({frame.node - 1, 0}); }
                else { if (node.kind() == K::Powi) out += ',' + genetik_typed_detail::integerText(node.exponent()); out += ')'; frames.pop_back(); }
            } else {
                const bool infix = node.kind() == K::Add || node.kind() == K::Subtract || node.kind() == K::Multiply || node.kind() == K::Divide;
                if (!frame.phase) {
                    if (infix) out += '(';
                    else out += node.kind() == K::Dot ? "dot(" : node.kind() == K::MatVec ? "matvec(" : "matmul(";
                    frame.phase = 1; frames.push_back({info_[frame.node - 1].start - 1, 0});
                } else if (frame.phase == 1) {
                    if (infix) out += node.kind() == K::Add ? " + " : node.kind() == K::Subtract ? " - " : node.kind() == K::Multiply ? " * " : " / ";
                    else out += ',';
                    frame.phase = 2; frames.push_back({frame.node - 1, 0});
                } else { out += ')'; frames.pop_back(); }
            }
        }
        return out;
    }
private:
    void usable() const { if (nodes_.empty()) throw std::logic_error("moved-from typed expression"); }
    static void validateLimits(TypedExpressionLimits limits) {
        genetik_typed_detail::validate(limits.values);
        if (!limits.maxNodes || !limits.maxDepth || !std::isfinite(limits.maxAbsConstant) || limits.maxAbsConstant < 0)
            throw std::invalid_argument("invalid typed expression limits");
    }
    static void checkConstant(const TypedValue& value, TypedExpressionLimits limits) {
        genetik_typed_detail::validateShape(value.shape(), limits.values);
        for (double x : value.coefficients())
            if (!std::isfinite(x) || std::abs(x) > limits.maxAbsConstant) throw std::invalid_argument("typed constant outside finite bound");
    }
    static TypedShape infer(K kind,TypedShape a,TypedShape b) {
        if(auto shape=genetik_typed_detail::operationShape(kind,a,b))return *shape;
        throw std::invalid_argument("unsupported typed operand kinds or incompatible shapes");
    }
    static bool isScientific(K kind) {return kind==K::Sin || kind==K::Cos || kind==K::Exp || kind==K::Sqrt || kind==K::Powi;}
    static genetik_detail::ScientificKind scientificKind(K kind) {
        using S=genetik_detail::ScientificKind;
        switch(kind) {
        case K::Sin:return S::Sin;case K::Cos:return S::Cos;case K::Exp:return S::Exp;
        case K::Sqrt:return S::Sqrt;case K::Powi:return S::Powi;
        default:throw std::logic_error("not scientific typed operator");
        }
    }
    static std::size_t cost(K kind, TypedShape a, TypedShape out, unsigned exponent=0) {
        using namespace genetik_typed_detail;
        if (kind == K::Powi) { genetik_detail::validateExponent(exponent); return exponent ? exponent-1 : 0; }
        if (kind == K::Transpose) return 0;
        if (kind == K::Dot || kind == K::MatVec || kind == K::MatMul) {
            const auto k = kind == K::Dot ? a.length() : a.columns();
            return multiply(out.coefficients(), add(k, k - 1, "typed reduction cost overflow"), "typed matrix work overflow");
        }
        return out.coefficients();
    }
    void analyze(std::span<const TypedExpressionNode> postfix, std::span<const TypedShape> schema) {
        using namespace genetik_typed_detail;
        std::vector<Info> stack;
        bound(postfix.size(), stack.max_size(), "typed validation stack capacity");
        bound(postfix.size(), info_.max_size(), "typed plan storage capacity");
        stack.reserve(postfix.size()); info_.reserve(postfix.size());
        std::size_t live = 0;
        for (std::size_t i = 0; i < postfix.size(); ++i) {
            const auto& node = postfix[i]; const auto arity = typedExpressionArity(node.kind());
            Info item{TypedShape::scalar(), 1, i};
            if (!arity) {
                if (node.kind() == K::Variable) {
                    if (node.variableIndex() >= schema.size()) throw std::invalid_argument("typed variable outside schema");
                    item.shape = schema[node.variableIndex()];
                } else {
                    checkConstant(node.constantValue(), limits_); item.shape = node.constantValue().shape();
                    constants_ = add(constants_, 1, "typed constant count overflow");
                    constantCoefficients_ = add(constantCoefficients_, item.shape.coefficients(), "typed constant coefficient overflow");
                    bound(constants_, limits_.maxConstantNodes, "typed constant node limit");
                    bound(constantCoefficients_, limits_.maxConstantCoefficients, "typed constant coefficient limit");
                }
                live = add(live, item.shape.coefficients(), "typed temporary count overflow");
            } else {
                if (stack.size() < arity) throw std::invalid_argument("typed operator lacks operands");
                const auto first = stack.size() - arity;
                const auto& a = stack[first]; const auto& b = stack.back();
                item.shape = infer(node.kind(), a.shape, b.shape);
                validateShape(item.shape, limits_.values);
                const auto childDepth = std::max(a.depth, b.depth);
                if (childDepth >= limits_.maxDepth) throw TypedLimitError("typed depth limit");
                item.depth = childDepth + 1; item.start = a.start;
                work_ = add(work_, cost(node.kind(), a.shape, item.shape, node.kind()==K::Powi ? node.exponent() : 0), "typed total work overflow");
                bound(work_, limits_.maxArithmeticOperations, "typed arithmetic work limit");
                temporary_ = std::max(temporary_, add(live, item.shape.coefficients(), "typed temporary count overflow"));
                for (std::size_t n = first; n < stack.size(); ++n) live -= stack[n].shape.coefficients();
                live = add(live, item.shape.coefficients(), "typed temporary count overflow");
                for (std::size_t n = 0; n < arity; ++n) stack.pop_back();
            }
            temporary_ = std::max(temporary_, live);
            bound(temporary_, limits_.maxTemporaryCoefficients, "typed temporary payload limit");
            stack.push_back(item); info_.push_back(item); stackValues_ = std::max(stackValues_, stack.size());
        }
        if (stack.size() != 1) throw std::invalid_argument("typed postfix has unused fragments");
    }
    static TypedExpression compose(K kind, const TypedExpression& left, const TypedExpression* right,
                                   TypedExpressionLimits limits, unsigned exponent=0) {
        using namespace genetik_typed_detail;
        left.usable(); if (right) right->usable(); validateLimits(limits);
        if (typedExpressionArity(kind) != (right ? 2u : 1u)) throw std::invalid_argument("incorrect typed composition arity");
        if (right && left.schema_ != right->schema_) throw std::invalid_argument("typed composition schemas differ");
        // All payload/count/work checks precede copies of coefficient storage.
        const auto count = add(add(left.nodeCount(), right ? right->nodeCount() : 0, "typed node count overflow"), 1, "typed node count overflow");
        bound(count, limits.maxNodes, "typed node limit");
        const auto childDepth = std::max(left.depth(), right ? right->depth() : 0);
        if (childDepth >= limits.maxDepth) throw TypedLimitError("typed depth limit");
        bound(left.schema_.size(), limits.maxVariables, "typed schema variable limit");
        bound(left.bindingCoefficients_, limits.maxBindingCoefficients, "typed binding coefficient limit");
        bound(add(left.constants_, right ? right->constants_ : 0, "typed constant count overflow"), limits.maxConstantNodes, "typed constant node limit");
        bound(add(left.constantCoefficients_, right ? right->constantCoefficients_ : 0, "typed coefficient overflow"), limits.maxConstantCoefficients, "typed constant coefficient limit");
        const auto output = infer(kind, left.shape(), right ? right->shape() : left.shape());
        validateShape(output, limits.values);
        const auto work = add(add(left.work_, right ? right->work_ : 0, "typed total work overflow"), cost(kind, left.shape(), output, exponent), "typed total work overflow");
        bound(work, limits.maxArithmeticOperations, "typed arithmetic work limit");
        const auto inputs = add(left.shape().coefficients(), right ? right->shape().coefficients() : 0, "typed temporary overflow");
        const auto peak = std::max({left.temporary_, right ? add(left.shape().coefficients(), right->temporary_, "typed temporary overflow") : 0,
                                    add(inputs, output.coefficients(), "typed temporary overflow")});
        bound(peak, limits.maxTemporaryCoefficients, "typed temporary payload limit");
        for (auto shape : left.schema_) validateShape(shape, limits.values);
        for (const auto* child : {&left, right}) if (child) {
            for (const auto& item : child->info_) validateShape(item.shape, limits.values);
            for (const auto& node : child->nodes_) if (node.kind() == K::Constant) checkConstant(node.constantValue(), limits);
        }
        std::vector<TypedExpressionNode> nodes;
        bound(count, nodes.max_size(), "typed node storage capacity"); nodes.reserve(count);
        nodes.insert(nodes.end(), left.nodes_.begin(), left.nodes_.end());
        if (right) nodes.insert(nodes.end(), right->nodes_.begin(), right->nodes_.end());
        nodes.push_back(kind==K::Powi ? TypedExpressionNode::power(exponent) : right ? TypedExpressionNode::binary(kind) : TypedExpressionNode::unary(kind));
        return TypedExpression(nodes, left.schema_, limits);
    }
    TypedExpressionLimits limits_;
    std::vector<TypedExpressionNode> nodes_;
    std::vector<TypedShape> schema_;
    std::vector<Info> info_;
    std::size_t bindingCoefficients_ = 0, constants_ = 0, constantCoefficients_ = 0;
    std::size_t work_ = 0, temporary_ = 0, stackValues_ = 0;
};
