#pragma once
#include "ExpressionDerivative.hpp"
#include "ExpressionVariation.hpp"
#include "ScalarExpressionCheckpoint.hpp"

struct VariableDomain {
    std::optional<double> lower, upper;
    bool positive = false, nonzero = false;
};
struct OperatorArgumentConstraint {
    ExpressionKind operation;
    std::size_t argument = 0;
    bool positive = false, nonzero = false;
};
struct OperatorNestingConstraint {
    ExpressionKind operation;
    std::size_t maximum = 1;
};
struct MathematicalConstraintLimits {
    std::size_t maxNodes = 4096, maxDepth = 128, maxChecks = 100000, maxExpressionWork = 1000000,
                maxWorkspaceBytes = 4 * 1024 * 1024;
    DerivativeLimits derivative;
};
enum class ConstraintStatus {
    Satisfied,
    Violated,
    DerivativeUnavailable,
    MathematicallyInvalid,
    ResourceExhausted,
    Cancelled
};
struct ConstraintReport {
    ConstraintStatus status = ConstraintStatus::Satisfied;
    std::size_t checks = 0, expressionWork = 0;
    bool structuralProof = false;
    bool sampledOnly = false;
};
// Immutable policy. Numerical positivity/nonzero/monotonicity assessments are
// explicitly sample-local, never a proof over an interval or the real line.
class MathematicalConstraints {
  public:
    MathematicalConstraints(const std::vector<VariableDomain> &domains = {},
                            const std::vector<OperatorArgumentConstraint> &arguments = {},
                            const std::vector<OperatorNestingConstraint> &nesting = {},
                            std::optional<std::size_t> monotonicInput = {}, bool increasing = true,
                            MathematicalConstraintLimits limits = {})
        : monotonic_(monotonicInput), increasing_(increasing), limits_(limits) {
        if (!limits.maxNodes || !limits.maxDepth || !limits.maxChecks || !limits.maxExpressionWork ||
            !limits.maxWorkspaceBytes || domains.size() > limits.maxNodes ||
            arguments.size() > limits.maxNodes || nesting.size() > limits.maxNodes)
            throw std::invalid_argument("constraint limits");
        if (domains.size() > limits.maxWorkspaceBytes / sizeof(VariableDomain) ||
            arguments.size() > limits.maxWorkspaceBytes / sizeof(OperatorArgumentConstraint) ||
            nesting.size() > limits.maxWorkspaceBytes / sizeof(OperatorNestingConstraint))
            throw std::length_error("constraint policy storage");
        for (auto d : domains) {
            if ((d.lower && !std::isfinite(*d.lower)) || (d.upper && !std::isfinite(*d.upper)) ||
                (d.lower && d.upper && *d.lower > *d.upper))
                throw std::invalid_argument("variable domain interval");
        }
        for (auto a : arguments)
            if (a.argument >= expressionArity(a.operation) || (!a.positive && !a.nonzero))
                throw std::invalid_argument("operator argument constraint");
        for (auto a : nesting)
            if (!expressionArity(a.operation))
                throw std::invalid_argument("nesting requires an operator");
        std::size_t remaining = limits.maxWorkspaceBytes;
        if (domains.size() > remaining / sizeof(VariableDomain))
            throw std::length_error("constraint domains storage");
        remaining -= domains.size() * sizeof(VariableDomain);
        if (arguments.size() > remaining / sizeof(OperatorArgumentConstraint))
            throw std::length_error("constraint argument storage");
        remaining -= arguments.size() * sizeof(OperatorArgumentConstraint);
        if (nesting.size() > remaining / sizeof(OperatorNestingConstraint))
            throw std::length_error("constraint nesting storage");
        domains_ = domains;
        arguments_ = arguments;
        nesting_ = nesting;
    }
    void validateSchema(std::size_t dimensions) const {
        if ((!domains_.empty() && domains_.size() != dimensions) || (monotonic_ && *monotonic_ >= dimensions))
            throw std::invalid_argument("constraint schema mismatch");
    }
    ConstraintReport structural(const Expression &expression) const {
        validateSchema(expression.variableCount());
        ConstraintReport result;
        result.structuralProof = true;
        if (expression.nodeCount() > limits_.maxNodes || expression.depth() > limits_.maxDepth) {
            result.status = ConstraintStatus::Violated;
            return result;
        }
        if (expression.nodeCount() >
            limits_.maxWorkspaceBytes / (sizeof(ExpressionNode) + 4 * sizeof(std::size_t))) {
            result.status = ConstraintStatus::ResourceExhausted;
            return result;
        }
        auto nodes = expression.nodes();
        for (auto rule : nesting_) {
            std::vector<std::size_t> stack;
            stack.reserve(nodes.size());
            for (auto n : nodes) {
                if (result.checks == limits_.maxChecks) {
                    result.status = ConstraintStatus::ResourceExhausted;
                    return result;
                }
                ++result.checks;
                auto arity = expressionArity(n.kind());
                std::size_t depth = 0;
                for (std::size_t j = 0; j < arity; ++j) {
                    depth = std::max(depth, stack.back());
                    stack.pop_back();
                }
                depth += n.kind() == rule.operation;
                if (depth > rule.maximum) {
                    result.status = ConstraintStatus::Violated;
                    return result;
                }
                stack.push_back(depth);
            }
        }
        return result;
    }
    ConstraintReport assess(const Expression &expression, std::span<const double> bindings,
                            const std::atomic<bool> *cancelled = nullptr) const {
        auto result = structural(expression);
        result.sampledOnly = true;
        result.structuralProof = false;
        if (result.status != ConstraintStatus::Satisfied)
            return result;
        if (bindings.size() != expression.variableCount())
            throw std::invalid_argument("constraint input dimension");
        auto charge = [&](std::size_t work) {
            if (cancelled && cancelled->load(std::memory_order_relaxed)) {
                result.status = ConstraintStatus::Cancelled;
                return false;
            }
            if (result.checks == limits_.maxChecks ||
                work > limits_.maxExpressionWork - result.expressionWork) {
                result.status = ConstraintStatus::ResourceExhausted;
                return false;
            }
            ++result.checks;
            result.expressionWork += work;
            return true;
        };
        for (std::size_t i = 0; i < bindings.size(); ++i) {
            if (!charge(0))
                return result;
            auto value = bindings[i];
            if (!std::isfinite(value))
                throw std::invalid_argument("constraint nonfinite binding");
            if (!domains_.empty()) {
                auto d = domains_[i];
                if ((d.lower && value < *d.lower) || (d.upper && value > *d.upper) ||
                    (d.positive && value <= 0) || (d.nonzero && value == 0)) {
                    result.status = ConstraintStatus::Violated;
                    return result;
                }
            }
        }
        auto nodes = expression.nodes();
        for (std::size_t i = 0; i < nodes.size(); ++i)
            for (auto rule : arguments_) {
                if (!charge(0))
                    return result;
                if (nodes[i].kind() != rule.operation)
                    continue;
                auto arity = expressionArity(rule.operation);
                std::size_t root = i - 1;
                for (std::size_t j = arity - 1; j > rule.argument; --j)
                    root = expressionSubtree(expression, root).first - 1;
                auto [begin, end] = expressionSubtree(expression, root);
                if (!charge(end - begin))
                    return result;
                Expression child{std::span<const ExpressionNode>(nodes).subspan(begin, end - begin),
                                 expression.variableCount(), expression.limits()};
                if (!charge(child.arithmeticCost()))
                    return result;
                auto evaluated = child.evaluate(bindings);
                if (!evaluated.valid()) {
                    result.status = ConstraintStatus::MathematicallyInvalid;
                    return result;
                }
                if ((rule.positive && evaluated.value() <= 0) || (rule.nonzero && evaluated.value() == 0)) {
                    result.status = ConstraintStatus::Violated;
                    return result;
                }
            }
        if (monotonic_) {
            if (!charge(expression.nodeCount()))
                return result;
            auto derivativeLimits = limits_.derivative;
            derivativeLimits.maxOperations =
                std::min(derivativeLimits.maxOperations, limits_.maxExpressionWork - result.expressionWork);
            if (!derivativeLimits.maxOperations) {
                result.status = ConstraintStatus::ResourceExhausted;
                return result;
            }
            auto derivative = evaluatePartial(expression, bindings, *monotonic_, derivativeLimits);
            const auto work = derivative.work().operations();
            // The derivative allowance was admitted before evaluation. Retain
            // consumed arithmetic even if a later check/cancellation stops us.
            result.expressionWork += work;
            if (!charge(0))
                return result;
            if (derivative.status() == DerivativeStatus::ResourceLimit) {
                result.status = ConstraintStatus::ResourceExhausted;
                return result;
            }
            if (!derivative.valueAvailable()) {
                result.status = ConstraintStatus::MathematicallyInvalid;
                return result;
            }
            if (!derivative.derivativeAvailable()) {
                result.status = ConstraintStatus::DerivativeUnavailable;
                return result;
            }
            if (increasing_ ? derivative.derivative() < 0 : derivative.derivative() > 0)
                result.status = ConstraintStatus::Violated;
        }
        return result;
    }
    std::string identity() const {
        CheckpointWriter w;
        w.text("mathematical-constraints;sample-monotonicity/1");
        w.integer(domains_.size());
        for (auto d : domains_) {
            w.integer(d.lower.has_value());
            if (d.lower)
                w.real(*d.lower);
            w.integer(d.upper.has_value());
            if (d.upper)
                w.real(*d.upper);
            w.integer(d.positive);
            w.integer(d.nonzero);
        }
        w.integer(arguments_.size());
        for (auto a : arguments_) {
            w.integer(static_cast<unsigned>(a.operation));
            w.integer(a.argument);
            w.integer(a.positive);
            w.integer(a.nonzero);
        }
        w.integer(nesting_.size());
        for (auto a : nesting_) {
            w.integer(static_cast<unsigned>(a.operation));
            w.integer(a.maximum);
        }
        w.integer(monotonic_.has_value());
        if (monotonic_)
            w.integer(*monotonic_);
        w.integer(increasing_);
        for (auto n : {limits_.maxNodes, limits_.maxDepth, limits_.maxChecks, limits_.maxExpressionWork,
                       limits_.maxWorkspaceBytes})
            w.integer(n);
        w.integer(limits_.derivative.maxOperations);
        w.integer(limits_.derivative.maxNodeVisits);
        w.integer(limits_.derivative.maxBindings);
        w.integer(limits_.derivative.maxWorkspaceBytes);
        return w.data();
    }

  private:
    std::vector<VariableDomain> domains_;
    std::vector<OperatorArgumentConstraint> arguments_;
    std::vector<OperatorNestingConstraint> nesting_;
    std::optional<std::size_t> monotonic_;
    bool increasing_;
    MathematicalConstraintLimits limits_;
};

class ConstraintResourceExhausted : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class ConstrainedRegressionProblem final : public RegressionProblem {
  public:
    ConstrainedRegressionProblem(const std::vector<RegressionRow> &rows,
                                 std::shared_ptr<const MathematicalConstraints> policy,
                                 ExpressionLimits limits = {})
        : RegressionProblem(rows, limits), rows_(rows), policy_(std::move(policy)) {
        if (!policy_)
            throw std::invalid_argument("constraint policy required");
        policy_->validateSchema(dimensions());
    }
    bool parallelEvaluationSafe() const noexcept override { return true; }
    RegressionMetrics measure(const Expression &expression,
                              std::uint64_t *rowAttempts = nullptr) const override {
        std::size_t attempted = 0;
        for (const auto &row : rows_) {
            if (rowAttempts) {
                if (*rowAttempts == UINT64_MAX)
                    throw std::overflow_error("constraint row counter");
                ++*rowAttempts;
            }
            ++attempted;
            auto result = policy_->assess(expression, row.inputs);
            if (result.status == ConstraintStatus::ResourceExhausted)
                throw ConstraintResourceExhausted("sample constraint evaluation allowance exhausted");
            if (result.status == ConstraintStatus::Cancelled)
                throw EvaluationCancelled();
            if (result.status != ConstraintStatus::Satisfied)
                return {Fitness::invalid(), {}, attempted};
        }
        auto result = RegressionProblem::measure(expression, rowAttempts);
        result.samplesAttempted += attempted;
        return result;
    }

  private:
    std::vector<RegressionRow> rows_;
    std::shared_ptr<const MathematicalConstraints> policy_;
};
