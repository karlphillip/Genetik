#pragma once
#include "Expression.hpp"
#include "DiscreteExpression.hpp"
#include "ScalarExpressionCheckpoint.hpp"
#include <numeric>
#include <array>

class DimensionResourceExhausted : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class DimensionExponent {
  public:
    DimensionExponent(std::int64_t n = 0, std::int64_t d = 1) {
        if (d <= 0 || d > 4096 || n < -16777216 || n > 16777216)
            throw DimensionResourceExhausted("dimensional rational intermediate cap");
        auto g = std::gcd(n < 0 ? -n : n, d);
        n /= g;
        d /= g;
        if (n < -4096 || n > 4096 || d > 64)
            throw DimensionResourceExhausted("dimensional exponent cap numerator4096/denominator64");
        n_ = static_cast<std::int32_t>(n);
        d_ = static_cast<std::int32_t>(d);
    }
    std::int32_t numerator() const noexcept { return n_; }
    std::int32_t denominator() const noexcept { return d_; }
    bool operator==(const DimensionExponent &) const = default;
    friend DimensionExponent operator+(DimensionExponent a, DimensionExponent b) {
        return {static_cast<std::int64_t>(a.n_) * b.d_ + static_cast<std::int64_t>(b.n_) * a.d_,
                static_cast<std::int64_t>(a.d_) * b.d_};
    }
    friend DimensionExponent operator-(DimensionExponent a, DimensionExponent b) {
        return a + DimensionExponent(-b.n_, b.d_);
    }
    friend DimensionExponent operator*(DimensionExponent a, DimensionExponent b) {
        return {static_cast<std::int64_t>(a.n_) * b.n_, static_cast<std::int64_t>(a.d_) * b.d_};
    }
    static DimensionExponent exactBinary64(double x) {
        if (!std::isfinite(x) || std::abs(x) > 4096)
            throw DimensionResourceExhausted("dimensionful real exponent range");
        for (std::int64_t d = 1; d <= 64; d *= 2) {
            const auto n = x * static_cast<double>(d);
            if (std::trunc(n) == n)
                return {static_cast<std::int64_t>(n), d};
        }
        throw std::invalid_argument("dimensionful exponent must be an exact bounded dyadic rational");
    }

  private:
    std::int32_t n_ = 0, d_ = 1;
};
enum class SIBaseDimension {
    Length,
    Mass,
    Time,
    ElectricCurrent,
    Temperature,
    AmountOfSubstance,
    LuminousIntensity
};
class PhysicalDimension {
  public:
    PhysicalDimension() = default;
    explicit PhysicalDimension(const std::array<DimensionExponent, 7> &exponents) : exponents_(exponents) {}
    static PhysicalDimension base(SIBaseDimension axis) {
        const auto i = static_cast<std::size_t>(axis);
        if (i >= 7)
            throw std::invalid_argument("SI dimension axis");
        PhysicalDimension d;
        d.exponents_[i] = 1;
        return d;
    }
    bool dimensionless() const noexcept { return *this == PhysicalDimension{}; }
    const auto &exponents() const noexcept { return exponents_; }
    bool operator==(const PhysicalDimension &) const = default;
    friend PhysicalDimension operator*(const PhysicalDimension &a, const PhysicalDimension &b) {
        auto p = a;
        for (std::size_t i = 0; i < 7; ++i)
            p.exponents_[i] = a.exponents_[i] + b.exponents_[i];
        return p;
    }
    friend PhysicalDimension operator/(const PhysicalDimension &a, const PhysicalDimension &b) {
        auto p = a;
        for (std::size_t i = 0; i < 7; ++i)
            p.exponents_[i] = a.exponents_[i] - b.exponents_[i];
        return p;
    }
    PhysicalDimension power(DimensionExponent exponent) const {
        auto p = *this;
        for (auto &e : p.exponents_)
            e = e * exponent;
        return p;
    }
    std::string toString() const {
        constexpr std::array names{"L", "M", "T", "I", "Theta", "N", "J"};
        std::string out = "[";
        for (std::size_t i = 0; i < 7; ++i)
            if (exponents_[i].numerator()) {
                if (out.size() > 1)
                    out += ' ';
                out += names[i];
                out += '^';
                out += std::to_string(exponents_[i].numerator());
                if (exponents_[i].denominator() != 1)
                    out += '/' + std::to_string(exponents_[i].denominator());
            }
        return out + "]";
    }

  private:
    std::array<DimensionExponent, 7> exponents_{};
};
inline PhysicalDimension scalarResultDimension(ExpressionKind kind,
                                               std::span<const PhysicalDimension> operands,
                                               std::optional<double> realExponent = {},
                                               unsigned discreteExponent = 0) {
    using K = ExpressionKind;
    if (operands.size() != expressionArity(kind) || operands.empty())
        throw std::invalid_argument("dimension operand arity");
    const auto &a = operands[0];
    const auto same = [&] {
        for (auto d : operands)
            if (d != a)
                throw std::invalid_argument("addition/comparison/branch dimensions differ");
        return a;
    };
    switch (kind) {
    case K::Add:
    case K::Subtract:
    case K::Min:
    case K::Max:
    case K::Clamp:
        return same();
    case K::Multiply:
        return a * operands[1];
    case K::Divide:
        return a / operands[1];
    case K::Powi:
    case K::NegativePowi:
        return a.power(DimensionExponent(static_cast<std::int64_t>(discreteExponent) *
                                         (kind == K::NegativePowi ? -1 : 1)));
    case K::Sqrt:
        return a.power({1, 2});
    case K::Cbrt:
        return a.power({1, 3});
    case K::Abs:
        return a;
    case K::Atan2:
        (void)same();
        return {};
    case K::Pow:
        if (!operands[1].dimensionless())
            throw std::invalid_argument("power exponent must be dimensionless");
        if (a.dimensionless())
            return {};
        if (!realExponent)
            throw std::invalid_argument("dimensionful power needs constant exponent");
        return a.power(DimensionExponent::exactBinary64(*realExponent));
    default:
        if (!a.dimensionless())
            throw std::invalid_argument(
                "log/exp/trigonometric arguments must be dimensionless (radians convention)");
        return {};
    }
}
struct DimensionalLimits {
    std::size_t maxNodes = 4096, maxWorkspaceBytes = 4 * 1024 * 1024, maxInferenceSteps = 100000;
};
inline std::vector<PhysicalDimension> inferDimensions(const Expression &expression,
                                                      std::span<const PhysicalDimension> variables,
                                                      std::span<const PhysicalDimension> constants = {},
                                                      DimensionalLimits limits = {}) {
    if (variables.size() != expression.variableCount())
        throw std::invalid_argument("dimensional schema differs");
    if (!limits.maxNodes || !limits.maxWorkspaceBytes || !limits.maxInferenceSteps)
        throw std::invalid_argument("dimensional inference limits");
    if (expression.nodeCount() > limits.maxNodes ||
        expression.nodeCount() >
            limits.maxWorkspaceBytes /
                (2 * sizeof(PhysicalDimension) + 4 * sizeof(std::size_t) + sizeof(ExpressionNode)))
        throw DimensionResourceExhausted("dimensional inference node/workspace cap");
    if (expression.nodeCount() > limits.maxInferenceSteps)
        throw DimensionResourceExhausted("dimensional inference steps");
    std::size_t remaining = limits.maxWorkspaceBytes;
    if (variables.size() > remaining / sizeof(PhysicalDimension))
        throw DimensionResourceExhausted("dimension schema storage");
    remaining -= variables.size() * sizeof(PhysicalDimension);
    if (constants.size() > remaining / sizeof(PhysicalDimension))
        throw DimensionResourceExhausted("dimensioned coefficient storage");
    remaining -= constants.size() * sizeof(PhysicalDimension);
    if (expression.nodeCount() >
        remaining / (2 * sizeof(PhysicalDimension) + 4 * sizeof(std::size_t) + 2 * sizeof(ExpressionNode)))
        throw DimensionResourceExhausted("combined dimension inference storage");
    auto nodes = expression.nodes();
    std::vector<PhysicalDimension> result;
    result.reserve(nodes.size());
    std::vector<std::size_t> stack;
    stack.reserve(nodes.size());
    std::size_t constant = 0;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        auto n = nodes[i];
        auto arity = expressionArity(n.kind());
        PhysicalDimension output;
        if (n.kind() == ExpressionKind::Variable)
            output = variables[n.variableIndex()];
        else if (n.kind() == ExpressionKind::Constant) {
            if (!constants.empty()) {
                if (constant >= constants.size())
                    throw std::invalid_argument("dimensioned constant count");
                output = constants[constant];
            }
            ++constant;
        } else {
            std::array<PhysicalDimension, 3> args{};
            for (std::size_t j = 0; j < arity; ++j)
                args[j] = result[stack[stack.size() - arity + j]];
            std::optional<double> exponent;
            if (n.kind() == ExpressionKind::Pow && nodes[i - 1].kind() == ExpressionKind::Constant)
                exponent = nodes[i - 1].constantValue();
            output = scalarResultDimension(n.kind(), std::span(args).first(arity), exponent,
                                           expressionPowerMetadata(n.kind()) ? n.exponent() : 0);
            stack.resize(stack.size() - arity);
        }
        result.push_back(output);
        stack.push_back(i);
    }
    if (!constants.empty() && constant != constants.size())
        throw std::invalid_argument("dimensioned constant count differs");
    return result;
}
class DimensionalExpression {
  public:
    DimensionalExpression(const Expression &expression, const std::vector<PhysicalDimension> &variables,
                          const std::vector<PhysicalDimension> &constants = {}, DimensionalLimits limits = {})
        : dimensions_(inferDimensions(expression, variables, constants, limits)), expression_(expression),
          variables_(variables), constants_(constants), limits_(limits) {}
    DimensionalLimits inferenceLimits() const noexcept { return limits_; }
    const Expression &expression() const noexcept { return expression_; }
    const PhysicalDimension &dimension() const { return dimensions_.back(); }
    std::span<const PhysicalDimension> variableDimensions() const noexcept { return variables_; }
    std::span<const PhysicalDimension> constantDimensions() const noexcept { return constants_; }
    std::span<const PhysicalDimension> nodeDimensions() const noexcept { return dimensions_; }
    ExpressionResult evaluate(std::span<const double> bindings) const {
        return expression_.evaluate(bindings);
    }
    std::string toString() const { return expression_.toString() + " " + dimension().toString(); }
    std::string identity() const {
        CheckpointWriter w;
        w.text("SI7;exact-rational;trig-radians/1");
        w.integer(variables_.size());
        for (auto d : variables_)
            writeDimension(w, d);
        w.integer(constants_.size());
        for (auto d : constants_)
            writeDimension(w, d);
        writeDimension(w, dimension());
        w.integer(limits_.maxNodes);
        w.integer(limits_.maxWorkspaceBytes);
        w.integer(limits_.maxInferenceSteps);
        return w.data();
    }
    static void writeDimension(CheckpointWriter &w, const PhysicalDimension &d) {
        for (auto e : d.exponents()) {
            w.text(std::to_string(e.numerator()));
            w.integer(e.denominator());
        }
    }
    static PhysicalDimension readDimension(CheckpointReader &r) {
        std::array<DimensionExponent, 7> values{};
        for (auto &e : values) {
            auto text = r.text(8);
            std::int64_t n = 0;
            auto v = std::from_chars(text.data(), text.data() + text.size(), n);
            if (v.ec != std::errc{} || v.ptr != text.data() + text.size())
                throw CheckpointError("dimension exponent numerator");
            auto d = r.count(64);
            e = DimensionExponent(n, static_cast<std::int64_t>(d));
        }
        return PhysicalDimension(values);
    }

  private:
    std::vector<PhysicalDimension> dimensions_;
    Expression expression_;
    std::vector<PhysicalDimension> variables_, constants_;
    DimensionalLimits limits_;
};
// Dimensions for discrete comparisons and lazy branches. Number-theoretic
// primitives require dimensionless integers; binders inherit their bound unit.
inline std::vector<PhysicalDimension> inferDiscreteDimensions(const DiscreteExpression &expression,
                                                              std::span<const PhysicalDimension> variables,
                                                              DimensionalLimits limits = {}) {
    if (variables.size() != expression.dimensions() || !expression.outerBindings().empty())
        throw std::invalid_argument("closed dimensional discrete schema required");
    if (expression.nodeCount() > limits.maxNodes ||
        expression.nodeCount() >
            limits.maxWorkspaceBytes / (3 * sizeof(PhysicalDimension) + 4 * sizeof(std::size_t)))
        throw DimensionResourceExhausted("discrete dimensional inference workspace");
    if (!limits.maxNodes || !limits.maxWorkspaceBytes || !limits.maxInferenceSteps)
        throw std::invalid_argument("dimensional inference limits");
    std::size_t steps = 0;
    auto charge = [&] {
        if (steps == limits.maxInferenceSteps)
            throw DimensionResourceExhausted("discrete dimension inference steps");
        ++steps;
    };
    std::vector<PhysicalDimension> result;
    result.reserve(expression.nodeCount());
    for (std::size_t i = 0; i < expression.nodeCount(); ++i) {
        charge();
        const auto n = expression.nodes()[i];
        const auto arity = discreteArity(n.kind());
        PhysicalDimension d;
        if (n.kind() == DiscreteKind::Variable)
            d = variables[n.variable()];
        else if (n.kind() == DiscreteKind::Iterator) {
            bool found = false;
            for (std::size_t j = i + 1; j < expression.nodeCount(); ++j) {
                charge();
                if (discreteIteration(expression.nodes()[j].kind()) &&
                    expression.nodes()[j].binder() == n.binder()) {
                    d = result[expression.children(j)[0]];
                    found = true;
                    break;
                }
            }
            if (!found)
                throw std::invalid_argument("dimension of open binder");
        } else if (arity) {
            const auto children = expression.children(i);
            const auto &a = result[children[0]];
            const auto same = [&](std::size_t j) {
                if (result[children[j]] != a)
                    throw std::invalid_argument("discrete comparison/branch/bounds dimensions differ");
            };
            if (n.kind() == DiscreteKind::IfThenElse) {
                if (result[children[1]] != result[children[2]])
                    throw std::invalid_argument("conditional branch dimensions differ");
                d = result[children[1]];
            } else if (discreteIteration(n.kind())) {
                same(1);
                if (n.kind() == DiscreteKind::First)
                    d = a;
                else if (n.kind() == DiscreteKind::Sum)
                    d = result[children[2]];
                else {
                    if (!result[children[2]].dimensionless())
                        throw std::invalid_argument("variable-length product requires dimensionless body");
                }
            } else if (n.kind() == DiscreteKind::Add || n.kind() == DiscreteKind::Subtract ||
                       n.kind() == DiscreteKind::Modulo) {
                same(1);
                d = a;
            } else if (n.kind() == DiscreteKind::Multiply)
                d = a * result[children[1]];
            else if (n.kind() == DiscreteKind::Negate)
                d = a;
            else if (n.kind() >= DiscreteKind::Equal && n.kind() <= DiscreteKind::GreaterEqual) {
                same(1);
            } else {
                for (std::size_t j = 0; j < arity; ++j)
                    if (!result[children[j]].dimensionless())
                        throw std::invalid_argument("number theory requires dimensionless integer arguments");
            }
        }
        result.push_back(d);
    }
    return result;
}

// Distinct model codec; ordinary scalar nodes keep their original numeric IDs.
inline CheckpointDocument checkpointDimensionalExpression(const DimensionalExpression &model) {
    CheckpointWriter w;
    auto e = model.expression();
    auto l = e.limits();
    w.integer(e.variableCount());
    w.integer(l.maxNodes);
    w.integer(l.maxDepth);
    w.real(l.maxAbsConstant);
    w.integer(l.maxArithmeticOperations);
    w.integer(e.nodeCount());
    for (const auto &n : e.nodes()) {
        w.integer(static_cast<unsigned>(n.kind()));
        if (n.kind() == ExpressionKind::Constant)
            w.real(n.constantValue());
        else if (n.kind() == ExpressionKind::Variable)
            w.integer(n.variableIndex());
        else if (expressionPowerMetadata(n.kind()))
            w.integer(n.exponent());
    }
    w.integer(model.variableDimensions().size());
    for (auto unit : model.variableDimensions())
        DimensionalExpression::writeDimension(w, unit);
    w.integer(model.constantDimensions().size());
    for (auto unit : model.constantDimensions())
        DimensionalExpression::writeDimension(w, unit);
    auto limits = model.inferenceLimits();
    w.integer(limits.maxNodes);
    w.integer(limits.maxWorkspaceBytes);
    w.integer(limits.maxInferenceSteps);
    CheckpointDocument document;
    document.codec = "genetik.dimensional-scalar-model/1";
    document.identity = "SI7;exact-rational;trig-radians/1";
    document.payload = w.data();
    return document;
}
inline DimensionalExpression restoreDimensionalExpression(const CheckpointDocument &document) {
    if (document.codec != "genetik.dimensional-scalar-model/1" ||
        document.identity != "SI7;exact-rational;trig-radians/1" || document.runtime != checkpointRuntime())
        throw CheckpointError("dimensional model codec/runtime");
    CheckpointReader r(document.payload);
    auto variables = r.count(10000);
    ExpressionLimits l;
    l.maxNodes = r.count(100000);
    l.maxDepth = r.count(100000);
    l.maxAbsConstant = r.real();
    l.maxArithmeticOperations = r.integer();
    auto count = r.count(std::min(l.maxNodes, r.limits().maxBytes / sizeof(ExpressionNode)));
    std::vector<ExpressionNode> nodes;
    nodes.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto kind = static_cast<ExpressionKind>(r.count(scalarOperators.size() - 1));
        if (kind == ExpressionKind::Constant)
            nodes.push_back(ExpressionNode::constant(r.real()));
        else if (kind == ExpressionKind::Variable)
            nodes.push_back(ExpressionNode::variable(r.count(variables ? variables - 1 : 0)));
        else if (expressionPowerMetadata(kind)) {
            auto power = static_cast<unsigned>(r.count(8));
            if (kind == ExpressionKind::NegativePowi && !power)
                throw CheckpointError("negative power magnitude");
            nodes.push_back(kind == ExpressionKind::Powi
                                ? ExpressionNode::power(power)
                                : ExpressionNode::signedPower(-static_cast<int>(power)));
        } else if (expressionArity(kind) == 1)
            nodes.push_back(ExpressionNode::unary(kind));
        else if (expressionArity(kind) == 2)
            nodes.push_back(ExpressionNode::binary(kind));
        else
            nodes.push_back(ExpressionNode::ternary(kind));
    }
    auto unitCount = r.count(variables);
    if (unitCount != variables)
        throw CheckpointError("dimensional external schema");
    std::vector<PhysicalDimension> external;
    external.reserve(unitCount);
    for (std::size_t i = 0; i < unitCount; ++i)
        external.push_back(DimensionalExpression::readDimension(r));
    unitCount = r.count(count);
    std::vector<PhysicalDimension> constants;
    constants.reserve(unitCount);
    for (std::size_t i = 0; i < unitCount; ++i)
        constants.push_back(DimensionalExpression::readDimension(r));
    DimensionalLimits inference;
    inference.maxNodes = r.count(100000);
    inference.maxWorkspaceBytes = r.count(64 * 1024 * 1024);
    inference.maxInferenceSteps = r.count(100000000);
    r.finish();
    return {Expression(nodes, variables, l), external, constants, inference};
}
