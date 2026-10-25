#pragma once
#include "Expression.hpp"
#include "RunCheckpoint.hpp"
#include "detail/AdvancedInteger.hpp"
#include <numeric>
#include <map>
#include <tuple>
#include <atomic>

// Mathematical-only bounded rational coefficients, deliberately distinct from binary64.
class SymbolicResourceExhausted : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class ExactRational {
  public:
    ExactRational(std::int64_t numerator = 0, std::int64_t denominator = 1) : n_(numerator), d_(denominator) {
        if (denominator <= 0)
            throw std::invalid_argument("positive rational denominator required");
        auto g = std::gcd(genetik_discrete_detail::integerMagnitude(numerator),
                          static_cast<std::uint64_t>(denominator));
        n_ /= static_cast<std::int64_t>(g);
        d_ /= static_cast<std::int64_t>(g);
    }
    std::int64_t numerator() const noexcept { return n_; }
    std::int64_t denominator() const noexcept { return d_; }
    bool operator==(const ExactRational &) const = default;
    bool zero() const noexcept { return n_ == 0; }
    ExactRational operator-() const {
        if (n_ == INT64_MIN)
            throw SymbolicResourceExhausted("rational numerator range");
        return {-n_, d_};
    }
    friend ExactRational operator+(ExactRational a, ExactRational b) {
        auto g = std::gcd(a.d_, b.d_);
        auto x = product(a.n_, b.d_ / g), y = product(b.n_, a.d_ / g);
        if ((y > 0 && x > INT64_MAX - y) || (y < 0 && x < INT64_MIN - y))
            throw SymbolicResourceExhausted("rational sum intermediate");
        return {x + y, product(a.d_, b.d_ / g)};
    }
    friend ExactRational operator-(ExactRational a, ExactRational b) { return a + (-b); }
    friend ExactRational operator*(ExactRational a, ExactRational b) {
        auto g1 = std::gcd(genetik_discrete_detail::integerMagnitude(a.n_), static_cast<std::uint64_t>(b.d_)),
             g2 = std::gcd(genetik_discrete_detail::integerMagnitude(b.n_), static_cast<std::uint64_t>(a.d_));
        return {product(a.n_ / static_cast<std::int64_t>(g1), b.n_ / static_cast<std::int64_t>(g2)),
                product(a.d_ / static_cast<std::int64_t>(g2), b.d_ / static_cast<std::int64_t>(g1))};
    }
    friend ExactRational operator/(ExactRational a, ExactRational b) {
        if (!b.n_)
            throw std::domain_error("zero rational denominator");
        if (b.n_ == INT64_MIN)
            throw SymbolicResourceExhausted("reciprocal rational range");
        return a * ExactRational(b.n_ < 0 ? -b.d_ : b.d_, b.n_ < 0 ? -b.n_ : b.n_);
    }
    static ExactRational fromBinary64(double value) {
        if (!std::isfinite(value))
            throw std::invalid_argument("finite binary64 import required");
        if (value == 0)
            return {};
        const auto bits = std::bit_cast<std::uint64_t>(value);
        auto m = bits & ((std::uint64_t{1} << 52) - 1);
        const auto raw = static_cast<int>((bits >> 52) & 2047);
        int shift = raw ? raw - 1023 - 52 : -1074;
        if (raw)
            m |= std::uint64_t{1} << 52;
        while (!(m & 1)) {
            m >>= 1;
            ++shift;
        }
        std::uint64_t denominator = 1;
        if (shift < 0) {
            if (shift < -62)
                throw SymbolicResourceExhausted("exact dyadic coefficient denominator exceeds rational64");
            denominator = std::uint64_t{1} << (-shift);
        } else {
            if (shift > 63 ||
                m > ((bits >> 63) ? (std::uint64_t{1} << 63) : static_cast<std::uint64_t>(INT64_MAX)) >>
                    shift)
                throw SymbolicResourceExhausted("exact coefficient numerator exceeds rational64");
            m <<= shift;
        }
        const bool negative = bits >> 63;
        if (!negative && m > static_cast<std::uint64_t>(INT64_MAX))
            throw SymbolicResourceExhausted("positive exact coefficient range");
        auto n = negative ? (m == (std::uint64_t{1} << 63) ? INT64_MIN : -static_cast<std::int64_t>(m))
                          : static_cast<std::int64_t>(m);
        return {n, static_cast<std::int64_t>(denominator)};
    }
    double approximate() const noexcept { return static_cast<double>(n_) / static_cast<double>(d_); }
    std::string toString() const {
        return d_ == 1 ? std::to_string(n_)
                       : "rational(" + std::to_string(n_) + "," + std::to_string(d_) + ")";
    }

  private:
    static std::int64_t product(std::int64_t a, std::int64_t b) {
        auto v = genetik_discrete_detail::integerMultiply(a, b);
        if (!v)
            throw SymbolicResourceExhausted("rational product range");
        return *v;
    }
    std::int64_t n_, d_;
};
struct SymbolicLimits {
    std::size_t maxNodes = 4096, maxDepth = 128, maxSteps = 100000, maxWorkspaceBytes = 8 * 1024 * 1024,
                maxTerms = 256, maxDegree = 16, maxVariables = 16, maxAssumptions = 128,
                maxOutputBytes = 1048576;
};
struct SymbolicNode {
    ExpressionKind kind = ExpressionKind::Constant;
    ExactRational value;
    std::size_t variable = 0;
    unsigned exponent = 0;
    std::array<std::size_t, 3> children{};
    bool operator==(const SymbolicNode &) const = default;
};
enum class SymbolicAssumptionKind {
    Defined,
    NonZero,
    Positive,
    NonNegative,
    InsideUnitInterval,
    AwayFromAtan2Cut
};
struct SymbolicAssumption {
    SymbolicAssumptionKind kind;
    std::size_t first, second = 0;
    bool operator==(const SymbolicAssumption &) const = default;
};
enum class SymbolicStatus { Success, ResourceExhausted, Unsupported, Undefined, Unknown, Cancelled };
enum class SymbolicRelation { NoClaim, ConditionalEquality, ConditionalDerivative };
struct SymbolicWork {
    std::size_t steps = 0, coefficientOperations = 0, nodes = 0, workspaceBytes = 0;
};
struct SymbolicTransformResult;
struct SymbolicConversion;
class SymbolicExpression {
  public:
    SymbolicExpression(std::span<const SymbolicNode> nodes, std::size_t root, std::size_t variables,
                       SymbolicLimits limits = {}, std::span<const SymbolicAssumption> assumptions = {})
        : root_(root), variables_(variables), limits_(limits) {
        validate(limits);
        if (nodes.empty() || root >= nodes.size() || variables > limits.maxVariables)
            throw std::invalid_argument("symbolic schema/root");
        if (nodes.size() > limits.maxNodes ||
            nodes.size() > limits.maxWorkspaceBytes / (sizeof(SymbolicNode) + sizeof(std::size_t)) ||
            assumptions.size() > limits.maxAssumptions ||
            assumptions.size() >
                (limits.maxWorkspaceBytes - nodes.size() * (sizeof(SymbolicNode) + sizeof(std::size_t))) /
                    sizeof(SymbolicAssumption))
            throw SymbolicResourceExhausted("symbolic construction storage");
        std::vector<std::size_t> depths;
        depths.reserve(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const auto &n = nodes[i];
            const auto arity = expressionArity(n.kind);
            std::size_t depth = 1;
            if (n.kind == ExpressionKind::Variable && n.variable >= variables)
                throw std::invalid_argument("symbolic variable binding");
            if (expressionPowerMetadata(n.kind) &&
                (n.exponent > 64 || (n.kind == ExpressionKind::NegativePowi && !n.exponent)))
                throw std::invalid_argument("symbolic discrete exponent range");
            for (std::size_t j = 0; j < arity; ++j) {
                if (n.children[j] >= i)
                    throw std::invalid_argument("symbolic DAG must be topological");
                if (depths[n.children[j]] >= limits.maxDepth)
                    throw SymbolicResourceExhausted("symbolic depth");
                depth = std::max(depth, depths[n.children[j]] + 1);
            }
            depths.push_back(depth);
        }
        for (auto a : assumptions)
            if (a.first >= nodes.size() || a.second >= nodes.size() || static_cast<unsigned>(a.kind) > 5)
                throw std::invalid_argument("symbolic assumption reference");
        nodes_.assign(nodes.begin(), nodes.end());
        assumptions_.assign(assumptions.begin(), assumptions.end());
    }
    static SymbolicExpression fromExpression(const Expression &expression, SymbolicLimits limits = {}) {
        validate(limits);
        if (expression.nodeCount() > limits.maxNodes ||
            expression.nodeCount() >
                limits.maxWorkspaceBytes /
                    (2 * sizeof(SymbolicNode) + 2 * sizeof(std::size_t) + sizeof(SymbolicAssumption)))
            throw SymbolicResourceExhausted("symbolic import storage");
        std::vector<SymbolicNode> nodes;
        std::vector<std::size_t> stack;
        nodes.reserve(expression.nodeCount());
        stack.reserve(expression.nodeCount());
        for (const auto &token : expression.nodes()) {
            SymbolicNode n;
            n.kind = token.kind();
            auto arity = expressionArity(n.kind);
            if (n.kind == ExpressionKind::Constant)
                n.value = ExactRational::fromBinary64(token.constantValue());
            else if (n.kind == ExpressionKind::Variable)
                n.variable = token.variableIndex();
            else {
                for (std::size_t j = 0; j < arity; ++j)
                    n.children[j] = stack[stack.size() - arity + j];
                stack.resize(stack.size() - arity);
                if (expressionPowerMetadata(n.kind))
                    n.exponent = token.exponent();
            }
            nodes.push_back(n);
            stack.push_back(nodes.size() - 1);
        }
        const std::array assumptions{
            SymbolicAssumption{SymbolicAssumptionKind::Defined, stack.back(), stack.back()}};
        return {nodes, stack.back(), expression.variableCount(), limits, assumptions};
    }
    std::span<const SymbolicNode> nodes() const noexcept { return nodes_; }
    std::span<const SymbolicAssumption> assumptions() const noexcept { return assumptions_; }
    std::size_t root() const noexcept { return root_; }
    std::size_t variableCount() const noexcept { return variables_; }
    SymbolicLimits limits() const noexcept { return limits_; }
    bool sameRepresentation(const SymbolicExpression &x) const {
        return root_ == x.root_ && variables_ == x.variables_ && nodes_ == x.nodes_ &&
               assumptions_ == x.assumptions_;
    }
    std::string toString(std::size_t outputLimit = 1048576) const {
        struct Event {
            std::size_t node, stage;
        };
        std::vector<Event> stack{{root_, 0}};
        std::string out;
        std::size_t visits = 0;
        const auto emit = [&](std::string_view s) {
            if (s.size() > outputLimit - out.size())
                throw SymbolicResourceExhausted("symbolic text limit");
            out += s;
        };
        while (!stack.empty()) {
            if (++visits > limits_.maxSteps)
                throw SymbolicResourceExhausted("symbolic expanded-text work");
            auto &f = stack.back();
            const auto &n = nodes_[f.node];
            const auto arity = expressionArity(n.kind);
            if (!arity) {
                emit(n.kind == ExpressionKind::Constant ? n.value.toString()
                                                        : "x" + std::to_string(n.variable));
                stack.pop_back();
                continue;
            }
            if (!f.stage) {
                emit(n.kind == ExpressionKind::NegativePowi ? "powi"
                                                            : scalarOperatorDescription(n.kind).name);
                emit("(");
                f.stage = 1;
                stack.push_back({n.children[0], 0});
            } else if (f.stage < arity) {
                emit(",");
                auto j = f.stage++;
                stack.push_back({n.children[j], 0});
            } else {
                if (expressionPowerMetadata(n.kind)) {
                    emit(",");
                    if (n.kind == ExpressionKind::NegativePowi)
                        emit("-");
                    emit(std::to_string(n.exponent));
                }
                emit(")");
                stack.pop_back();
            }
        }
        return out;
    }
    SymbolicTransformResult differentiate(std::size_t input, SymbolicLimits limits = {},
                                          const std::atomic<bool> *cancelled = nullptr) const;
    SymbolicTransformResult simplify(SymbolicLimits limits = {},
                                     const std::atomic<bool> *cancelled = nullptr) const;
    SymbolicTransformResult expandPolynomial(SymbolicLimits limits = {},
                                             const std::atomic<bool> *cancelled = nullptr) const;
    SymbolicTransformResult factorPolynomial(SymbolicLimits limits = {},
                                             const std::atomic<bool> *cancelled = nullptr) const;
    std::vector<std::vector<std::size_t>>
    commonSubexpressions(SymbolicLimits limits = {}, const std::atomic<bool> *cancelled = nullptr) const;
    SymbolicConversion toExecutable(ExpressionLimits limits = {},
                                    const std::atomic<bool> *cancelled = nullptr) const;
    CheckpointDocument checkpoint() const;
    static SymbolicExpression restore(const CheckpointDocument &document);
    static void validate(SymbolicLimits l) {
        if (!l.maxNodes || !l.maxDepth || !l.maxSteps || !l.maxWorkspaceBytes || !l.maxTerms ||
            !l.maxVariables || !l.maxAssumptions || !l.maxOutputBytes || l.maxVariables > 64 ||
            l.maxDegree > 64 || l.maxDepth > 4096)
            throw std::invalid_argument("invalid bounded symbolic limits");
    }

  private:
    std::vector<SymbolicNode> nodes_;
    std::vector<SymbolicAssumption> assumptions_;
    std::size_t root_, variables_;
    SymbolicLimits limits_;
};
struct SymbolicTransformResult {
    SymbolicStatus status = SymbolicStatus::Unknown;
    std::optional<SymbolicExpression> expression;
    SymbolicWork work;
    std::string reason;
    SymbolicRelation relation = SymbolicRelation::NoClaim;
    bool bitExactExecutableEquivalence = false;
};
struct SymbolicConversion {
    Expression expression;
    bool coefficientRounding = false;
    std::vector<SymbolicAssumption> requiredAssumptions;
    bool bitExactEquivalence = false;
    std::shared_ptr<const SymbolicExpression> assumptionContext;
};
namespace genetik_symbolic_detail {
struct Stop {
    SymbolicStatus status;
    const char *why;
};
struct Unsupported {};
class Builder {
  public:
    SymbolicLimits limits;
    SymbolicWork &work;
    const std::atomic<bool> *cancelled;
    std::vector<SymbolicNode> nodes;
    std::vector<SymbolicAssumption> assumptions;
    std::vector<std::size_t> depths;
    Builder(const SymbolicExpression &e, SymbolicLimits l, SymbolicWork &w,
            const std::atomic<bool> *c = nullptr)
        : limits(l), work(w), cancelled(c) {
        SymbolicExpression::validate(l);
        capacity_ =
            std::min(l.maxNodes, l.maxWorkspaceBytes / (4 * (sizeof(SymbolicNode) + sizeof(std::size_t))));
        assumptionCapacity_ =
            std::min(l.maxAssumptions, l.maxWorkspaceBytes / (16 * sizeof(SymbolicAssumption)));
        reserveAux(capacity_ * (sizeof(SymbolicNode) + sizeof(std::size_t)));
        reserveAux(assumptionCapacity_ * sizeof(SymbolicAssumption));
        nodes.reserve(capacity_);
        depths.reserve(capacity_);
        assumptions.reserve(assumptionCapacity_);
        if (e.variableCount() > l.maxVariables)
            throw Stop{SymbolicStatus::ResourceExhausted, "symbolic variable cap"};
        for (const auto &n : e.nodes())
            append(n);
        for (auto a : e.assumptions())
            assume(a.kind, a.first, a.second);
    }
    void charge() {
        if (cancelled && cancelled->load(std::memory_order_relaxed))
            throw Stop{SymbolicStatus::Cancelled, "symbolic transformation cancelled"};
        if (work.steps == limits.maxSteps)
            throw Stop{SymbolicStatus::ResourceExhausted, "symbolic step limit"};
        ++work.steps;
    }
    void reserveAux(std::size_t bytes) {
        if (bytes > limits.maxWorkspaceBytes - work.workspaceBytes)
            throw Stop{SymbolicStatus::ResourceExhausted, "symbolic accounted workspace"};
        work.workspaceBytes += bytes;
    }
    void reserveElements(std::size_t count, std::size_t bytes) {
        if (count > limits.maxWorkspaceBytes / bytes)
            throw Stop{SymbolicStatus::ResourceExhausted, "symbolic count/byte cap"};
        reserveAux(count * bytes);
    }
    std::vector<bool> reachable(const SymbolicExpression &e) {
        reserveElements(e.nodes().size(), 4 * sizeof(std::size_t) + 1);
        std::vector<bool> needed(e.nodes().size(), false);
        std::vector<std::size_t> pending;
        pending.reserve(e.nodes().size() * 3);
        pending.push_back(e.root());
        while (!pending.empty()) {
            charge();
            auto i = pending.back();
            pending.pop_back();
            if (needed[i])
                continue;
            needed[i] = true;
            for (std::size_t j = 0; j < expressionArity(e.nodes()[i].kind); ++j)
                pending.push_back(e.nodes()[i].children[j]);
        }
        return needed;
    }
    bool sameTree(std::size_t a, std::size_t c) {
        std::vector<std::pair<std::size_t, std::size_t>> pending{{a, c}};
        while (!pending.empty()) {
            charge();
            auto [x, y] = pending.back();
            pending.pop_back();
            if (x == y)
                continue;
            const auto &left = nodes[x];
            const auto &right = nodes[y];
            if (left.kind != right.kind ||
                (left.kind == ExpressionKind::Variable && left.variable != right.variable) ||
                (left.kind == ExpressionKind::Constant && left.value != right.value) ||
                (expressionPowerMetadata(left.kind) && left.exponent != right.exponent))
                return false;
            for (std::size_t j = 0; j < expressionArity(left.kind); ++j) {
                if (pending.size() >=
                    limits.maxWorkspaceBytes / (16 * sizeof(std::pair<std::size_t, std::size_t>)))
                    throw Stop{SymbolicStatus::ResourceExhausted, "structural comparison workspace"};
                pending.push_back({left.children[j], right.children[j]});
            }
        }
        return true;
    }
    void coefficient() {
        charge();
        ++work.coefficientOperations;
    }
    std::size_t append(SymbolicNode n) {
        charge();
        if (nodes.size() == capacity_)
            throw Stop{SymbolicStatus::ResourceExhausted, "symbolic node/workspace limit"};
        std::size_t depth = 1;
        for (std::size_t i = 0; i < expressionArity(n.kind); ++i) {
            if (n.children[i] >= nodes.size())
                throw std::logic_error("symbolic builder child");
            if (depths[n.children[i]] >= limits.maxDepth)
                throw Stop{SymbolicStatus::ResourceExhausted, "symbolic depth limit"};
            depth = std::max(depth, depths[n.children[i]] + 1);
        }
        nodes.push_back(n);
        depths.push_back(depth);
        work.nodes = nodes.size();
        return nodes.size() - 1;
    }
    std::size_t constant(ExactRational r) {
        SymbolicNode n;
        n.value = r;
        return append(n);
    }
    std::size_t op(ExpressionKind k, std::size_t a, std::size_t b = 0, std::size_t c = 0,
                   unsigned exponent = 0) {
        SymbolicNode n;
        n.kind = k;
        n.children = {a, b, c};
        n.exponent = exponent;
        return append(n);
    }
    void assume(SymbolicAssumptionKind kind, std::size_t first, std::size_t second = 0) {
        SymbolicAssumption a{kind, first, second};
        if (std::find(assumptions.begin(), assumptions.end(), a) != assumptions.end())
            return;
        if (assumptions.size() == assumptionCapacity_)
            throw Stop{SymbolicStatus::ResourceExhausted, "assumption limit"};
        assumptions.push_back(a);
    }
    SymbolicExpression finish(std::size_t root, std::size_t variables) {
        return {nodes, root, variables, limits, assumptions};
    }

  private:
    std::size_t capacity_ = 0, assumptionCapacity_ = 0;
};
inline SymbolicTransformResult failed(SymbolicWork work, const Stop &stop) {
    return {stop.status, {}, work, stop.why};
}
inline SymbolicTransformResult failed(SymbolicWork work, const SymbolicResourceExhausted &e) {
    return {SymbolicStatus::ResourceExhausted, {}, work, e.what()};
}
} // namespace genetik_symbolic_detail
inline SymbolicTransformResult SymbolicExpression::differentiate(std::size_t target, SymbolicLimits limits,
                                                                 const std::atomic<bool> *cancelled) const {
    if (target >= variables_)
        throw std::invalid_argument("symbolic derivative target outside schema");
    SymbolicWork work;
    try {
        using K = ExpressionKind;
        using A = SymbolicAssumptionKind;
        genetik_symbolic_detail::Builder b(*this, limits, work, cancelled);
        if (nodes_.size() > limits.maxWorkspaceBytes / (4 * sizeof(std::size_t)))
            throw genetik_symbolic_detail::Stop{SymbolicStatus::ResourceExhausted, "derivative maps"};
        auto needed = b.reachable(*this);
        b.reserveElements(nodes_.size(), sizeof(std::size_t) + 1);
        std::vector<std::size_t> derivative;
        std::vector<bool> active;
        derivative.reserve(nodes_.size());
        active.reserve(nodes_.size());
        const auto zero = b.constant(0), one = b.constant(1), two = b.constant(2);
        const auto neg = [&](std::size_t a) { return b.op(K::Multiply, b.constant(-1), a); };
        for (const auto &n : nodes_) {
            b.charge();
            const auto position = static_cast<std::size_t>(&n - nodes_.data());
            if (!needed[position]) {
                derivative.push_back(zero);
                active.push_back(false);
                continue;
            }
            const auto arity = expressionArity(n.kind);
            bool on = n.kind == K::Variable && n.variable == target;
            for (std::size_t j = 0; j < arity; ++j)
                on = on || active[n.children[j]];
            std::size_t d = zero;
            if (!on) {
                derivative.push_back(d);
                active.push_back(false);
                continue;
            }
            if (n.kind == K::Variable)
                d = one;
            else {
                const auto a = n.children[0], da = derivative[a], c = arity > 1 ? n.children[1] : 0,
                           dc = arity > 1 ? derivative[c] : zero;
                const auto mul = [&](std::size_t x, std::size_t y) { return b.op(K::Multiply, x, y); };
                const auto div = [&](std::size_t x, std::size_t y) { return b.op(K::Divide, x, y); };
                const auto power = [&](std::size_t x, ExactRational exponent) {
                    if (exponent.zero())
                        return one;
                    return b.op(K::Pow, x, b.constant(exponent));
                };
                const auto root = static_cast<std::size_t>(&n - nodes_.data());
                switch (n.kind) {
                case K::Add:
                case K::Subtract:
                    d = b.op(n.kind, da, dc);
                    break;
                case K::Multiply:
                    d = b.op(K::Add, mul(da, c), mul(a, dc));
                    break;
                case K::Divide:
                    b.assume(A::NonZero, c);
                    d = div(b.op(K::Subtract, mul(da, c), mul(a, dc)), mul(c, c));
                    break;
                case K::Powi:
                case K::NegativePowi: {
                    auto exponent =
                        static_cast<std::int64_t>(n.exponent) * (n.kind == K::NegativePowi ? -1 : 1);
                    if (exponent < 0)
                        b.assume(A::NonZero, a);
                    d = exponent ? mul(mul(b.constant(exponent), power(a, exponent - 1)), da) : zero;
                    break;
                }
                case K::Pow:
                    if (nodes_[c].kind == K::Constant) {
                        auto exponent = nodes_[c].value;
                        if (exponent.denominator() != 1)
                            b.assume(A::Positive, a);
                        else if (exponent.numerator() <= 0)
                            b.assume(A::NonZero, a);
                        b.coefficient();
                        d = mul(mul(b.constant(exponent), power(a, exponent - ExactRational(1))), da);
                    } else {
                        b.assume(A::Positive, a);
                        d = mul(root, b.op(K::Add, mul(dc, b.op(K::Log, a)), div(mul(c, da), a)));
                    }
                    break;
                case K::Log:
                    b.assume(A::Positive, a);
                    d = div(da, a);
                    break;
                case K::Log1p:
                    b.assume(A::Positive, b.op(K::Add, one, a));
                    d = div(da, b.op(K::Add, one, a));
                    break;
                case K::Log2:
                case K::Log10:
                    b.assume(A::Positive, a);
                    d = div(da, mul(a, b.op(K::Log, b.constant(n.kind == K::Log2 ? 2 : 10))));
                    break;
                case K::Exp:
                    d = mul(root, da);
                    break;
                case K::Expm1:
                    d = mul(b.op(K::Exp, a), da);
                    break;
                case K::Sin:
                    d = mul(b.op(K::Cos, a), da);
                    break;
                case K::Cos:
                    d = neg(mul(b.op(K::Sin, a), da));
                    break;
                case K::Tan:
                    b.assume(A::NonZero, b.op(K::Cos, a));
                    d = div(da, mul(b.op(K::Cos, a), b.op(K::Cos, a)));
                    break;
                case K::Sqrt:
                    b.assume(A::Positive, a);
                    d = div(da, mul(two, root));
                    break;
                case K::Cbrt:
                    b.assume(A::NonZero, a);
                    d = div(da, mul(b.constant(3), mul(root, root)));
                    break;
                case K::Asin:
                case K::Acos:
                    b.assume(A::InsideUnitInterval, a);
                    d = div(da, b.op(K::Sqrt, b.op(K::Subtract, one, mul(a, a))));
                    if (n.kind == K::Acos)
                        d = neg(d);
                    break;
                case K::Atan:
                    d = div(da, b.op(K::Add, one, mul(a, a)));
                    break;
                case K::Atan2:
                    b.assume(A::AwayFromAtan2Cut, a, c);
                    d = div(b.op(K::Subtract, mul(c, da), mul(a, dc)), b.op(K::Add, mul(a, a), mul(c, c)));
                    break;
                case K::Sinh:
                    d = mul(b.op(K::Cosh, a), da);
                    break;
                case K::Cosh:
                    d = mul(b.op(K::Sinh, a), da);
                    break;
                case K::Tanh:
                    d = mul(b.op(K::Subtract, one, mul(root, root)), da);
                    break;
                case K::Abs:
                    b.assume(A::NonZero, a);
                    d = mul(div(a, root), da);
                    break;
                default:
                    return {SymbolicStatus::Unsupported,
                            {},
                            work,
                            "piecewise symbolic derivative not implemented"};
                }
            }
            derivative.push_back(d);
            active.push_back(on);
        }
        return {SymbolicStatus::Success,
                b.finish(derivative[root_], variables_),
                work,
                {},
                SymbolicRelation::ConditionalDerivative};
    } catch (const genetik_symbolic_detail::Stop &e) {
        return genetik_symbolic_detail::failed(work, e);
    } catch (const SymbolicResourceExhausted &e) {
        return genetik_symbolic_detail::failed(work, e);
    }
}
inline SymbolicTransformResult SymbolicExpression::simplify(SymbolicLimits limits,
                                                            const std::atomic<bool> *cancelled) const {
    SymbolicWork work;
    try {
        using K = ExpressionKind;
        using A = SymbolicAssumptionKind;
        genetik_symbolic_detail::Builder b(*this, limits, work, cancelled);
        auto needed = b.reachable(*this);
        b.reserveElements(nodes_.size(), sizeof(std::size_t));
        std::vector<std::size_t> map;
        map.reserve(nodes_.size());
        for (const auto &n : nodes_) {
            b.charge();
            if (!needed[static_cast<std::size_t>(&n - nodes_.data())]) {
                map.push_back(static_cast<std::size_t>(&n - nodes_.data()));
                continue;
            }
            const auto arity = expressionArity(n.kind);
            auto root = static_cast<std::size_t>(&n - nodes_.data());
            if (!arity) {
                map.push_back(root);
                continue;
            }
            auto next = n;
            for (std::size_t j = 0; j < arity; ++j)
                next.children[j] = map[n.children[j]];
            const auto a = next.children[0], c = arity > 1 ? next.children[1] : 0;
            const auto constant = [&](std::size_t id, std::int64_t value) {
                return b.nodes[id].kind == K::Constant && b.nodes[id].value == ExactRational(value);
            };
            if ((n.kind == K::Add || n.kind == K::Subtract) && constant(c, 0))
                root = a;
            else if (n.kind == K::Add && constant(a, 0))
                root = c;
            else if ((n.kind == K::Multiply || n.kind == K::Divide) && constant(c, 1))
                root = a;
            else if (n.kind == K::Multiply && constant(a, 1))
                root = c;
            else if (n.kind == K::Divide && b.sameTree(a, c)) {
                b.assume(A::NonZero, a);
                root = b.constant(1);
            } else if (n.kind == K::Subtract && b.sameTree(a, c)) {
                b.assume(A::Defined, a);
                root = b.constant(0);
            } else if (arity == 2 && b.nodes[a].kind == K::Constant && b.nodes[c].kind == K::Constant &&
                       (n.kind == K::Add || n.kind == K::Subtract || n.kind == K::Multiply ||
                        n.kind == K::Divide)) {
                auto x = b.nodes[a].value, y = b.nodes[c].value;
                b.coefficient();
                if (n.kind == K::Divide && y.zero())
                    return {SymbolicStatus::Undefined, {}, work, "constant zero denominator"};
                auto value = n.kind == K::Add        ? x + y
                             : n.kind == K::Subtract ? x - y
                             : n.kind == K::Multiply ? x * y
                                                     : x / y;
                root = b.constant(value);
            } else
                root = b.append(next);
            map.push_back(root);
        }
        return {SymbolicStatus::Success,
                b.finish(map[root_], variables_),
                work,
                {},
                SymbolicRelation::ConditionalEquality};
    } catch (const genetik_symbolic_detail::Stop &e) {
        return genetik_symbolic_detail::failed(work, e);
    } catch (const SymbolicResourceExhausted &e) {
        return genetik_symbolic_detail::failed(work, e);
    }
}
namespace genetik_symbolic_detail {
using Exponents = std::vector<unsigned>;
using Polynomial = std::map<Exponents, ExactRational>;
struct PolynomialBuilder {
    Builder &b;
    std::size_t variables, allocatedTerms = 0;
    void insert(Polynomial &p, Exponents powers, ExactRational value) {
        b.coefficient();
        if (value.zero())
            return;
        unsigned degree = 0;
        for (auto n : powers) {
            if (n > b.limits.maxDegree - degree)
                throw Stop{SymbolicStatus::ResourceExhausted, "polynomial degree cap"};
            degree += n;
        }
        auto it = p.find(powers);
        if (it != p.end()) {
            value = it->second + value;
            if (value.zero())
                p.erase(it);
            else
                it->second = value;
        } else {
            const auto bytes = variables * sizeof(unsigned) + sizeof(ExactRational) + 96;
            if (p.size() == b.limits.maxTerms || allocatedTerms >= b.limits.maxWorkspaceBytes / bytes)
                throw Stop{SymbolicStatus::ResourceExhausted, "polynomial term/workspace cap"};
            ++allocatedTerms;
            b.reserveAux(bytes);
            p.emplace(std::move(powers), value);
        }
    }
    Polynomial multiply(const Polynomial &a, const Polynomial &c) {
        Polynomial out;
        for (const auto &[x, xc] : a)
            for (const auto &[y, yc] : c) {
                b.coefficient();
                auto powers = x;
                for (std::size_t j = 0; j < variables; ++j) {
                    if (y[j] > b.limits.maxDegree - powers[j])
                        throw Stop{SymbolicStatus::ResourceExhausted, "polynomial exponent cap"};
                    powers[j] += y[j];
                }
                insert(out, std::move(powers), xc * yc);
            }
        return out;
    }
    Polynomial extract(const SymbolicExpression &expression) {
        auto needed = b.reachable(expression);
        b.reserveElements(expression.nodes().size(), sizeof(Polynomial));
        std::vector<Polynomial> values;
        values.reserve(expression.nodes().size());
        for (const auto &n : expression.nodes()) {
            b.charge();
            Polynomial value;
            if (!needed[static_cast<std::size_t>(&n - expression.nodes().data())]) {
                values.push_back(std::move(value));
                continue;
            }
            const auto zero = Exponents(variables, 0);
            using K = ExpressionKind;
            if (n.kind == K::Constant)
                insert(value, zero, n.value);
            else if (n.kind == K::Variable) {
                auto powers = zero;
                powers[n.variable] = 1;
                insert(value, std::move(powers), 1);
            } else if (n.kind == K::Add || n.kind == K::Subtract) {
                for (const auto &[k, c] : values[n.children[0]])
                    insert(value, k, c);
                for (const auto &[k, c] : values[n.children[1]])
                    insert(value, k, n.kind == K::Add ? c : -c);
            } else if (n.kind == K::Multiply)
                value = multiply(values[n.children[0]], values[n.children[1]]);
            else if (n.kind == K::Powi) {
                insert(value, zero, 1);
                for (unsigned i = 0; i < n.exponent; ++i)
                    value = multiply(value, values[n.children[0]]);
            } else if (n.kind == K::Divide) {
                const auto &denominator = values[n.children[1]];
                if (denominator.empty())
                    throw std::domain_error("zero polynomial divisor");
                if (denominator.size() != 1 || denominator.begin()->first != zero)
                    throw Unsupported{};
                for (const auto &[k, c] : values[n.children[0]])
                    insert(value, k, c / denominator.begin()->second);
            } else
                throw Unsupported{};
            values.push_back(std::move(value));
        }
        return std::move(values[expression.root()]);
    }
    std::size_t emit(const Polynomial &p) {
        using K = ExpressionKind;
        if (p.empty())
            return b.constant(0);
        std::optional<std::size_t> total;
        for (const auto &[powers, coefficient] : p) {
            auto term = b.constant(coefficient);
            for (std::size_t j = 0; j < variables; ++j)
                if (powers[j]) {
                    SymbolicNode variable;
                    variable.kind = K::Variable;
                    variable.variable = j;
                    auto x = b.append(variable);
                    if (powers[j] > 1)
                        x = b.op(K::Powi, x, 0, 0, powers[j]);
                    term = b.op(K::Multiply, term, x);
                }
            total = total ? b.op(K::Add, *total, term) : term;
        }
        return *total;
    }
};
} // namespace genetik_symbolic_detail
inline SymbolicTransformResult
SymbolicExpression::expandPolynomial(SymbolicLimits limits, const std::atomic<bool> *cancelled) const {
    SymbolicWork work;
    try {
        genetik_symbolic_detail::Builder b(*this, limits, work, cancelled);
        genetik_symbolic_detail::PolynomialBuilder polynomial{b, variables_};
        auto p = polynomial.extract(*this);
        auto root = polynomial.emit(p);
        return {SymbolicStatus::Success,
                b.finish(root, variables_),
                work,
                {},
                SymbolicRelation::ConditionalEquality};
    } catch (const genetik_symbolic_detail::Unsupported &) {
        return {SymbolicStatus::Unsupported, {}, work, "nonpolynomial operation"};
    } catch (const genetik_symbolic_detail::Stop &e) {
        return genetik_symbolic_detail::failed(work, e);
    } catch (const SymbolicResourceExhausted &e) {
        return genetik_symbolic_detail::failed(work, e);
    } catch (const std::domain_error &) {
        return {SymbolicStatus::Undefined, {}, work, "zero polynomial denominator"};
    }
}
inline SymbolicTransformResult
SymbolicExpression::factorPolynomial(SymbolicLimits limits, const std::atomic<bool> *cancelled) const {
    SymbolicWork work;
    try {
        using namespace genetik_symbolic_detail;
        Builder b(*this, limits, work, cancelled);
        PolynomialBuilder p{b, variables_};
        auto terms = p.extract(*this);
        if (terms.empty())
            return {SymbolicStatus::Success,
                    b.finish(b.constant(0), variables_),
                    work,
                    {},
                    SymbolicRelation::ConditionalEquality};
        if (variables_ != 1)
            return {SymbolicStatus::Unsupported, {}, work, "factorization supports one variable"};
        std::vector<ExactRational> coefficients(limits.maxDegree + 1);
        std::size_t degree = 0;
        for (auto [powers, c] : terms) {
            degree = std::max(degree, static_cast<std::size_t>(powers[0]));
            coefficients[powers[0]] = c;
        }
        std::vector<std::int64_t> roots;
        for (std::int64_t candidate = -16; candidate <= 16 && degree; ++candidate) {
            for (;;) {
                b.coefficient();
                auto value = coefficients[degree];
                for (std::size_t i = degree; i > 0; --i) {
                    b.coefficient();
                    value = value * ExactRational(candidate) + coefficients[i - 1];
                }
                if (!value.zero())
                    break;
                std::vector<ExactRational> quotient(degree);
                quotient[degree - 1] = coefficients[degree];
                for (std::size_t i = degree - 1; i > 0; --i) {
                    b.coefficient();
                    quotient[i - 1] = coefficients[i] + ExactRational(candidate) * quotient[i];
                }
                coefficients = std::move(quotient);
                --degree;
                roots.push_back(candidate);
                if (!degree)
                    break;
            }
        }
        if (degree > 1)
            return {SymbolicStatus::Unknown,
                    {},
                    work,
                    "no complete rational factorization within bounded integer-root rule"};
        std::size_t root;
        if (degree == 1) {
            SymbolicNode x;
            x.kind = ExpressionKind::Variable;
            x.variable = 0;
            root = b.op(ExpressionKind::Add,
                        b.op(ExpressionKind::Multiply, b.constant(coefficients[1]), b.append(x)),
                        b.constant(coefficients[0]));
        } else
            root = b.constant(coefficients[0]);
        for (auto value : roots) {
            SymbolicNode x;
            x.kind = ExpressionKind::Variable;
            x.variable = 0;
            root = b.op(ExpressionKind::Multiply, root,
                        b.op(ExpressionKind::Subtract, b.append(x), b.constant(value)));
        }
        return {SymbolicStatus::Success,
                b.finish(root, variables_),
                work,
                {},
                SymbolicRelation::ConditionalEquality};
    } catch (const genetik_symbolic_detail::Unsupported &) {
        return {SymbolicStatus::Unsupported, {}, work, "nonpolynomial factorization"};
    } catch (const genetik_symbolic_detail::Stop &e) {
        return genetik_symbolic_detail::failed(work, e);
    } catch (const SymbolicResourceExhausted &e) {
        return genetik_symbolic_detail::failed(work, e);
    } catch (const std::domain_error &) {
        return {SymbolicStatus::Undefined, {}, work, "zero denominator"};
    }
}
inline std::vector<std::vector<std::size_t>>
SymbolicExpression::commonSubexpressions(SymbolicLimits limits, const std::atomic<bool> *cancelled) const
    try {
    SymbolicWork work;
    genetik_symbolic_detail::Builder budget(*this, limits, work, cancelled);
    using Key =
        std::tuple<unsigned, std::int64_t, std::int64_t, std::size_t, unsigned, std::array<std::size_t, 3>>;
    budget.reserveElements(nodes_.size(), sizeof(Key) + 128);
    auto needed = budget.reachable(*this);
    std::map<Key, std::pair<std::size_t, std::vector<std::size_t>>> unique;
    std::vector<std::size_t> classes;
    classes.reserve(nodes_.size());
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        budget.charge();
        if (!needed[i]) {
            classes.push_back(0);
            continue;
        }
        const auto &n = nodes_[i];
        auto children = n.children;
        for (std::size_t j = 0; j < expressionArity(n.kind); ++j)
            children[j] = classes[children[j]];
        Key key{static_cast<unsigned>(n.kind),
                n.kind == ExpressionKind::Constant ? n.value.numerator() : 0,
                n.kind == ExpressionKind::Constant ? n.value.denominator() : 1,
                n.kind == ExpressionKind::Variable ? n.variable : 0,
                expressionPowerMetadata(n.kind) ? n.exponent : 0,
                children};
        auto [it, inserted] =
            unique.try_emplace(key, std::make_pair(unique.size(), std::vector<std::size_t>{}));
        (void)inserted;
        classes.push_back(it->second.first);
        it->second.second.push_back(i);
    }
    std::vector<std::vector<std::size_t>> result;
    for (auto &[key, value] : unique) {
        (void)key;
        if (value.second.size() > 1)
            result.push_back(std::move(value.second));
    }
    return result;
} catch (const genetik_symbolic_detail::Stop &e) {
    if (e.status == SymbolicStatus::Cancelled)
        throw EvaluationCancelled();
    throw SymbolicResourceExhausted(e.why);
}
inline SymbolicConversion SymbolicExpression::toExecutable(ExpressionLimits limits,
                                                           const std::atomic<bool> *cancelled) const {
    (void)Expression::constant(0, variables_, limits);
    struct Event {
        std::size_t node, stage;
    };
    std::vector<Event> stack{{root_, 0}};
    std::vector<ExpressionNode> output;
    bool rounded = false;
    std::size_t visits = 0;
    while (!stack.empty()) {
        if (cancelled && cancelled->load(std::memory_order_relaxed))
            throw EvaluationCancelled();
        if (++visits > limits_.maxSteps)
            throw SymbolicResourceExhausted("symbolic executable expansion work");
        auto &f = stack.back();
        const auto &n = nodes_[f.node];
        auto arity = expressionArity(n.kind);
        if (f.stage < arity) {
            auto j = f.stage++;
            stack.push_back({n.children[j], 0});
            continue;
        }
        if (output.size() == limits.maxNodes)
            throw SymbolicResourceExhausted("expanded executable node cap");
        if (n.kind == ExpressionKind::Constant) {
            auto v = n.value.approximate();
            try {
                rounded = rounded || ExactRational::fromBinary64(v) != n.value;
            } catch (const SymbolicResourceExhausted &) {
                rounded = true;
            }
            output.push_back(ExpressionNode::constant(v));
        } else if (n.kind == ExpressionKind::Variable)
            output.push_back(ExpressionNode::variable(n.variable));
        else if (expressionPowerMetadata(n.kind)) {
            if (n.exponent > 8) {
                if (output.size() + 2 > limits.maxNodes)
                    throw SymbolicResourceExhausted("expanded power node cap");
                output.push_back(ExpressionNode::constant(static_cast<double>(n.exponent) *
                                                          (n.kind == ExpressionKind::NegativePowi ? -1 : 1)));
                output.push_back(ExpressionNode::binary(ExpressionKind::Pow));
            } else
                output.push_back(n.kind == ExpressionKind::Powi
                                     ? ExpressionNode::power(n.exponent)
                                     : ExpressionNode::signedPower(-static_cast<int>(n.exponent)));
        } else if (arity == 1)
            output.push_back(ExpressionNode::unary(n.kind));
        else if (arity == 2)
            output.push_back(ExpressionNode::binary(n.kind));
        else
            output.push_back(ExpressionNode::ternary(n.kind));
        stack.pop_back();
    }
    return {Expression(output, variables_, limits), rounded,
            std::vector<SymbolicAssumption>(assumptions_.begin(), assumptions_.end()), false,
            std::make_shared<const SymbolicExpression>(*this)};
}
inline CheckpointDocument SymbolicExpression::checkpoint() const {
    CheckpointWriter w;
    for (auto v :
         {limits_.maxNodes, limits_.maxDepth, limits_.maxSteps, limits_.maxWorkspaceBytes, limits_.maxTerms,
          limits_.maxDegree, limits_.maxVariables, limits_.maxAssumptions, limits_.maxOutputBytes})
        w.integer(v);
    w.integer(variables_);
    w.integer(root_);
    w.integer(nodes_.size());
    for (const auto &n : nodes_) {
        w.integer(static_cast<unsigned>(n.kind));
        w.text(std::to_string(n.value.numerator()));
        w.text(std::to_string(n.value.denominator()));
        w.integer(n.variable);
        w.integer(n.exponent);
        for (auto child : n.children)
            w.integer(child);
    }
    w.integer(assumptions_.size());
    for (auto a : assumptions_) {
        w.integer(static_cast<unsigned>(a.kind));
        w.integer(a.first);
        w.integer(a.second);
    }
    CheckpointDocument d;
    d.codec = "genetik.symbolic-rational-dag/1";
    d.identity = "mathematical-only;rational64;assumptions/1";
    d.payload = w.data();
    return d;
}
inline SymbolicExpression SymbolicExpression::restore(const CheckpointDocument &d) {
    if (d.codec != "genetik.symbolic-rational-dag/1" ||
        d.identity != "mathematical-only;rational64;assumptions/1" || d.runtime != checkpointRuntime())
        throw CheckpointError("symbolic codec/runtime differs");
    CheckpointReader r(d.payload);
    SymbolicLimits l;
    l.maxNodes = r.count(100000);
    l.maxDepth = r.count(4096);
    l.maxSteps = r.count(1000000000);
    l.maxWorkspaceBytes = r.count(64 * 1024 * 1024);
    l.maxTerms = r.count(100000);
    l.maxDegree = r.count(64);
    l.maxVariables = r.count(64);
    l.maxAssumptions = r.count(100000);
    l.maxOutputBytes = r.count(64 * 1024 * 1024);
    validate(l);
    auto variables = r.count(l.maxVariables), root = r.count(l.maxNodes),
         count = r.count(
             std::min(l.maxNodes, l.maxWorkspaceBytes / (2 * sizeof(SymbolicNode) + sizeof(std::size_t))));
    std::vector<SymbolicNode> nodes;
    nodes.reserve(count);
    const auto integer = [&] {
        auto text = r.text(20);
        std::int64_t n = 0;
        auto parsed = std::from_chars(text.data(), text.data() + text.size(), n);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            throw CheckpointError("symbolic rational integer");
        return n;
    };
    for (std::size_t i = 0; i < count; ++i) {
        SymbolicNode n;
        n.kind = static_cast<ExpressionKind>(r.count(scalarOperators.size() - 1));
        auto numerator = integer(), denominator = integer();
        n.value = ExactRational(numerator, denominator);
        n.variable = r.count(l.maxVariables);
        n.exponent = static_cast<unsigned>(r.count(64));
        for (auto &child : n.children)
            child = r.count(l.maxNodes);
        nodes.push_back(n);
    }
    auto size = r.count(std::min(
        l.maxAssumptions, (l.maxWorkspaceBytes - count * (2 * sizeof(SymbolicNode) + sizeof(std::size_t))) /
                              (2 * sizeof(SymbolicAssumption))));
    std::vector<SymbolicAssumption> assumptions;
    assumptions.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        auto kind = static_cast<SymbolicAssumptionKind>(r.count(5));
        auto first = r.count(l.maxNodes), second = r.count(l.maxNodes);
        assumptions.push_back({kind, first, second});
    }
    r.finish();
    return {nodes, root, variables, l, assumptions};
}

enum class SymbolicEquivalence { EqualUnderAssumptions, DistinctPolynomialForms, Unknown, ResourceLimit };
struct SymbolicEquivalenceResult {
    SymbolicEquivalence conclusion;
    SymbolicWork work;
    std::vector<SymbolicAssumption> leftAssumptions, rightAssumptions;
    bool bitExactExecutableEquivalence = false;
};
inline SymbolicEquivalenceResult compareSymbolicPolynomials(const SymbolicExpression &left,
                                                            const SymbolicExpression &right,
                                                            SymbolicLimits limits = {}) {
    if (left.variableCount() != right.variableCount())
        throw std::invalid_argument("symbolic comparison schemas differ");
    SymbolicWork work;
    SymbolicEquivalence conclusion = SymbolicEquivalence::Unknown;
    try {
        genetik_symbolic_detail::Builder a(left, limits, work);
        genetik_symbolic_detail::PolynomialBuilder pa{a, left.variableCount()};
        auto first = pa.extract(left);
        genetik_symbolic_detail::Builder b(right, limits, work);
        genetik_symbolic_detail::PolynomialBuilder pb{b, right.variableCount()};
        auto second = pb.extract(right);
        conclusion = first == second ? SymbolicEquivalence::EqualUnderAssumptions
                                     : SymbolicEquivalence::DistinctPolynomialForms;
    } catch (const genetik_symbolic_detail::Unsupported &) {
    } catch (const genetik_symbolic_detail::Stop &) {
        conclusion = SymbolicEquivalence::ResourceLimit;
    } catch (const SymbolicResourceExhausted &) {
        conclusion = SymbolicEquivalence::ResourceLimit;
    } catch (const std::domain_error &) {
    }
    return {conclusion, work,
            std::vector<SymbolicAssumption>(left.assumptions().begin(), left.assumptions().end()),
            std::vector<SymbolicAssumption>(right.assumptions().begin(), right.assumptions().end()), false};
}
