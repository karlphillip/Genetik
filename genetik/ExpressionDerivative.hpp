#pragma once
#include "Expression.hpp"

// Invocation-local limits, independent of engine loss-attempt budgets.
struct DerivativeLimits {
    std::size_t maxOperations = 1000000;
    std::size_t maxNodeVisits = 65536;
    std::size_t maxBindings = 65536;
    std::size_t maxWorkspaceBytes = 8 * 1024 * 1024;
};
enum class DerivativeStatus { Success, PrimalInvalidity, Unavailable, ResourceLimit };
enum class DerivativeFailure { SingularRule, NonfiniteArithmetic, NonDifferentiable, UnsupportedRule };
enum class DerivativeResource { Operations, NodeVisits, Bindings, Workspace };
struct DerivativeWork {
    std::size_t primalOperations = 0;
    std::size_t derivativeArithmetic = 0;
    std::size_t derivativeMathCalls = 0;
    std::size_t nodesVisited = 0;
    std::size_t bindingsValidated = 0;
    std::size_t workspaceBytes = 0;
    // Admission guarantees that this sum cannot overflow.
    std::size_t operations() const noexcept {
        return primalOperations + derivativeArithmetic + derivativeMathCalls;
    }
    bool operator==(const DerivativeWork&) const = default;
};
class DerivativeResult {
public:
    DerivativeStatus status() const noexcept { return status_; }
    bool valueAvailable() const noexcept {
        return status_ == DerivativeStatus::Success || status_ == DerivativeStatus::Unavailable;
    }
    bool derivativeAvailable() const noexcept { return status_ == DerivativeStatus::Success; }
    double value() const {
        if (!valueAvailable()) throw std::logic_error("partial evaluation has no complete value");
        return value_;
    }
    double derivative() const {
        if (!derivativeAvailable()) throw std::logic_error("partial derivative is unavailable");
        return derivative_;
    }
    const DerivativeWork& work() const noexcept { return work_; }
    std::optional<std::size_t> primalFailedNode() const noexcept { return primalFailedNode_; }
    std::optional<std::size_t> derivativeFailedNode() const noexcept { return derivativeFailedNode_; }
    std::optional<DerivativeFailure> derivativeFailure() const noexcept { return failure_; }
    std::optional<DerivativeResource> exhaustedResource() const noexcept { return resource_; }
    std::optional<std::size_t> resourceNode() const noexcept { return resourceNode_; }
private:
    friend class genetik_detail::DerivativeEvaluation;
    DerivativeResult() = default;
    DerivativeStatus status_ = DerivativeStatus::ResourceLimit;
    double value_ = 0, derivative_ = 0;
    DerivativeWork work_;
    std::optional<std::size_t> primalFailedNode_, derivativeFailedNode_, resourceNode_;
    std::optional<DerivativeFailure> failure_;
    std::optional<DerivativeResource> resource_;
};

namespace genetik_detail {
// A private control signal, never a catch-all over user/allocator exceptions.
struct DerivativeLimitStop {};
class DerivativeEvaluation final {
    struct Slot { double value, derivative; bool active; };
    DerivativeLimits limits_;
    DerivativeResult result_;
    std::optional<std::size_t> node_;
    void stop(DerivativeResource resource) {
        result_.resource_ = resource; result_.resourceNode_ = node_;
        throw DerivativeLimitStop{};
    }
    void charge(std::size_t& category) {
        if (result_.work_.operations() == limits_.maxOperations) stop(DerivativeResource::Operations);
        ++category;
    }
    void fail(DerivativeFailure reason) {
        if (!result_.failure_) { result_.failure_ = reason; result_.derivativeFailedNode_ = node_; }
    }
    double checked(std::optional<double> value) {
        if (!value) { fail(DerivativeFailure::NonfiniteArithmetic); return 0; }
        return *value;
    }
    double binary(ExpressionKind kind, double a, double b) {
        // Do not execute subsequent derivative primitives after the first failure.
        if (result_.failure_) return 0;
        return checked(scalarBinary(kind, a, b, [&]{ charge(result_.work_.derivativeArithmetic); }));
    }
    double negate(double a) {
        if (result_.failure_) return 0;
        charge(result_.work_.derivativeArithmetic);
        volatile double value = -a;
        return checked(std::isfinite(value) ? std::optional<double>(value) : std::nullopt);
    }
    double math(ScientificKind kind, double a) {
        if (result_.failure_) return 0;
        return checked(scientificScalar(kind, a, 0, [&]{ charge(result_.work_.derivativeMathCalls); }));
    }
    double native(ExpressionKind kind,double a){
        if(result_.failure_)return 0;
        return checked(scalarUnary(kind,a,0,[&]{charge(result_.work_.derivativeMathCalls);}));
    }
    double derivativeBinary(ExpressionKind kind, Slot a, Slot b, double q) {
        using K = ExpressionKind;
        if(kind==K::Min||kind==K::Max){if(a.value==b.value){fail(DerivativeFailure::NonDifferentiable);return 0;}const bool chooseA=kind==K::Min?a.value<b.value:a.value>b.value;return chooseA?a.derivative:b.derivative;}
        if(kind==K::Pow){
            if(a.value<=0&&b.active){if(a.value==0&&!a.active&&b.value>0)return 0;fail(DerivativeFailure::UnsupportedRule);return 0;}
            if(a.value==0){if(std::trunc(b.value)!=b.value||b.value<1){fail(DerivativeFailure::SingularRule);return 0;}return b.value==1?a.derivative:0;}
            double d=0;
            if(a.active)d=binary(K::Multiply,binary(K::Multiply,b.value,binary(K::Divide,q,a.value)),a.derivative);
            if(b.active){auto term=binary(K::Multiply,binary(K::Multiply,q,native(K::Log,a.value)),b.derivative);d=a.active?binary(K::Add,d,term):term;}
            return d;
        }
        if(kind==K::Atan2){
            if(a.active&&a.value==0&&b.value<0){fail(DerivativeFailure::NonDifferentiable);return 0;}
            const double scale=std::max(std::abs(a.value),std::abs(b.value));
            const double u=binary(K::Divide,a.value,scale),v=binary(K::Divide,b.value,scale);
            const double denominator=binary(K::Add,binary(K::Multiply,u,u),binary(K::Multiply,v,v));double d=0;
            if(a.active)d=binary(K::Multiply,binary(K::Divide,binary(K::Divide,v,denominator),scale),a.derivative);
            if(b.active){const auto term=binary(K::Multiply,binary(K::Divide,binary(K::Divide,negate(u),denominator),scale),b.derivative);d=a.active?binary(K::Add,d,term):term;}return d;
        }
        if (kind == K::Add || kind == K::Subtract) {
            if (a.active && b.active) return binary(kind, a.derivative, b.derivative);
            if (a.active) return a.derivative;
            return kind == K::Add ? b.derivative : negate(b.derivative);
        }
        if (kind == K::Multiply) {
            if (!a.active) return binary(K::Multiply, a.value, b.derivative);
            const double left = binary(K::Multiply, a.derivative, b.value);
            if (!b.active) return left;
            const double right = binary(K::Multiply, a.value, b.derivative);
            return binary(K::Add, left, right);
        }
        if (!b.active) return binary(K::Divide, a.derivative, b.value);
        const double product = binary(K::Multiply, q, b.derivative);
        const double numerator = a.active ? binary(K::Subtract, a.derivative, product) : negate(product);
        return binary(K::Divide, numerator, b.value);
    }
    double derivativeUnary(ExpressionKind kind, Slot a, double primal,unsigned exponent=0) {
        using K = ExpressionKind;
        switch (kind) {
        case K::NegativePowi:return binary(K::Multiply,binary(K::Divide,binary(K::Multiply,-static_cast<double>(exponent),primal),a.value),a.derivative);
        case K::Cbrt:if(a.value==0){fail(DerivativeFailure::SingularRule);return 0;}return binary(K::Divide,a.derivative,binary(K::Multiply,binary(K::Multiply,3,primal),primal));
        case K::Log1p:return binary(K::Divide,a.derivative,binary(K::Add,1,a.value));
        case K::Log2:case K::Log10:return binary(K::Divide,binary(K::Divide,a.derivative,a.value),native(K::Log,kind==K::Log2?2:10));
        case K::Expm1:return binary(K::Multiply,math(ScientificKind::Exp,a.value),a.derivative);
        case K::Tan:return binary(K::Multiply,binary(K::Add,1,binary(K::Multiply,primal,primal)),a.derivative);
        case K::Asin:case K::Acos:{if(std::abs(a.value)==1){fail(DerivativeFailure::SingularRule);return 0;}const auto denominator=math(ScientificKind::Sqrt,binary(K::Multiply,binary(K::Subtract,1,a.value),binary(K::Add,1,a.value)));const auto d=binary(K::Divide,a.derivative,denominator);return kind==K::Acos?negate(d):d;}
        case K::Atan:{if(std::abs(a.value)>1){const auto inv=binary(K::Divide,1,a.value);return binary(K::Multiply,binary(K::Multiply,binary(K::Divide,inv,binary(K::Add,1,binary(K::Multiply,inv,inv))),inv),a.derivative);}return binary(K::Divide,a.derivative,binary(K::Add,1,binary(K::Multiply,a.value,a.value)));}
        case K::Sinh:return binary(K::Multiply,native(K::Cosh,a.value),a.derivative);
        case K::Cosh:return binary(K::Multiply,native(K::Sinh,a.value),a.derivative);
        case K::Tanh:return binary(K::Multiply,binary(K::Multiply,binary(K::Subtract,1,primal),binary(K::Add,1,primal)),a.derivative);
        case K::Abs:if(a.value==0){fail(DerivativeFailure::NonDifferentiable);return 0;}return a.value<0?negate(a.derivative):a.derivative;
        case K::Log: return binary(K::Divide, a.derivative, a.value);
        case K::Exp: return binary(K::Multiply, primal, a.derivative);
        case K::Sin: { const double c = math(ScientificKind::Cos, a.value); return binary(K::Multiply, c, a.derivative); }
        case K::Cos: { const double s = math(ScientificKind::Sin, a.value); const double n = negate(s); return binary(K::Multiply, n, a.derivative); }
        case K::Sqrt:
            if (!(a.value > 0)) { fail(DerivativeFailure::SingularRule); return 0; }
            { const double denominator = binary(K::Multiply, 2, primal); return binary(K::Divide, a.derivative, denominator); }
        default: throw std::logic_error("unreachable derivative unary operation");
        }
    }
public:
    explicit DerivativeEvaluation(DerivativeLimits limits) : limits_(limits) {}
    DerivativeResult evaluate(const Expression& expression, std::span<const double> bindings, std::size_t target) {
        expression.requireUsable();
        if (bindings.size() != expression.variables_) throw std::invalid_argument("binding count differs from expression arity");
        if (target >= expression.variables_) throw std::invalid_argument("partial target outside expression schema");
        try {
            for (double binding : bindings) {
                if (result_.work_.bindingsValidated == limits_.maxBindings) stop(DerivativeResource::Bindings);
                ++result_.work_.bindingsValidated;
                if (!std::isfinite(binding)) throw std::invalid_argument("variable bindings must all be finite");
            }
            const std::size_t count = expression.nodes_.size();
            if (count > limits_.maxWorkspaceBytes / sizeof(Slot)) stop(DerivativeResource::Workspace);
            result_.work_.workspaceBytes = count * sizeof(Slot);
            std::vector<Slot> stack;
            if (count > stack.max_size()) stop(DerivativeResource::Workspace);
            stack.reserve(count); // active entries are initialized only by push_back
            for (std::size_t i = 0; i < count; ++i) {
                node_ = i;
                if (result_.work_.nodesVisited == limits_.maxNodeVisits) stop(DerivativeResource::NodeVisits);
                ++result_.work_.nodesVisited;
                const auto& node = expression.nodes_[i];
                const auto kind = node.kind();
                if (kind == ExpressionKind::Constant) stack.push_back({node.constantValue(), 0, false});
                else if (kind == ExpressionKind::Variable) {
                    const bool active = node.variableIndex() == target;
                    stack.push_back({bindings[node.variableIndex()], active ? 1.0 : 0.0, active});
                } else {
                    const auto primalCharge = [&]{ charge(result_.work_.primalOperations); };
                    const Slot right = stack.back();
                    std::optional<double> value;
                    Slot output{0, 0, right.active};
                    if (expressionArity(kind) == 1) {
                        double derivative = right.derivative;
                        // Observe each CHECKED primal multiplication in the existing Powi kernel.
                        // Its previous value and the original base define the sequential recurrence.
                        const auto step = [&](double previous, double) {
                            if (!right.active || result_.failure_) return;
                            const double left = binary(ExpressionKind::Multiply, derivative, right.value);
                            const double term = binary(ExpressionKind::Multiply, previous, right.derivative);
                            derivative = binary(ExpressionKind::Add, left, term);
                        };
                        value = scalarUnary(kind, right.value,
                            expressionPowerMetadata(kind) ? node.exponent() : 0, primalCharge, step);
                        if (value && right.active && !result_.failure_) {
                            output.derivative = kind == ExpressionKind::Powi
                                ? (node.exponent() == 0 ? 0.0 : derivative)
                                : derivativeUnary(kind, right, *value,expressionPowerMetadata(kind)?node.exponent():0);
                        }
                    } else if(expressionArity(kind)==3){
                        stack.pop_back();const Slot low=stack.back();stack.pop_back();const Slot x=stack.back();output.active=x.active||low.active||right.active;
                        value=scalarTernary(kind,x.value,low.value,right.value,primalCharge);
                        if(value&&output.active&&!result_.failure_){if(x.value==low.value||x.value==right.value||low.value==right.value)fail(DerivativeFailure::NonDifferentiable);else output.derivative=x.value<low.value?low.derivative:x.value>right.value?right.derivative:x.derivative;}
                    } else {
                        stack.pop_back(); const Slot left = stack.back();
                        output.active = left.active || right.active;
                        value = scalarBinary(kind, left.value, right.value, primalCharge);
                        if (value && output.active && !result_.failure_)
                            output.derivative = derivativeBinary(kind, left, right, *value);
                    }
                    if (!value) {
                        result_.status_ = DerivativeStatus::PrimalInvalidity; result_.primalFailedNode_ = i;
                        return result_;
                    }
                    output.value = *value; stack.back() = output;
                }
            }
            result_.value_ = stack.back().value;
            result_.derivative_ = stack.back().derivative;
            result_.status_ = result_.failure_ ? DerivativeStatus::Unavailable : DerivativeStatus::Success;
        } catch (const DerivativeLimitStop&) { result_.status_ = DerivativeStatus::ResourceLimit; }
        return result_;
    }
};
}

// Synchronous, reentrant first-order chain-rule evaluation; selected input retains
// its original schema index. No expression, input or result cache is modified.
inline DerivativeResult evaluatePartial(const Expression& expression, std::span<const double> bindings,
                                        std::size_t inputIndex, DerivativeLimits limits = {}) {
    return genetik_detail::DerivativeEvaluation(limits).evaluate(expression, bindings, inputIndex);
}
