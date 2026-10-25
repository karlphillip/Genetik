#pragma once
#include "DiscreteRunCheckpoint.hpp"

struct IntegerRecurrenceLimits {
    std::size_t maxSteps = 100000, maxWorkspaceBytes = 1048576;
};
// External slots[0,N), step index N, oldest previous state N+1, then N+2.
// The model, never the caller, binds previous-state slots on every transition.
class IntegerRecurrence {
  public:
    IntegerRecurrence(std::size_t external, const std::vector<std::int64_t> &initial,
                      const DiscreteExpression &transition, IntegerRecurrenceLimits limits = {})
        : external_(external), initial_(checkedInitial(external, initial, transition, limits)),
          transition_(transition), limits_(limits) {
        if (initial.empty() || initial.size() > 2 || external > 1024 || limits.maxSteps > 1000000000 ||
            !limits.maxWorkspaceBytes)
            throw std::invalid_argument("recurrence requires order1or2 and bounded workspace/steps");
        if (transition.dimensions() != external + initial.size() + 1 ||
            transition.type() != DiscreteType::Int64 || !transition.outerBindings().empty())
            throw std::invalid_argument("recurrence transition schema/type differs");
    }
    std::size_t externalDimensions() const noexcept { return external_; }
    std::size_t order() const noexcept { return initial_.size(); }
    std::span<const std::int64_t> initial() const noexcept { return initial_; }
    const DiscreteExpression &transition() const noexcept { return transition_; }
    IntegerRecurrenceLimits limits() const noexcept { return limits_; }
    std::size_t indexSlot() const noexcept { return external_; }
    std::size_t previousSlot(std::size_t state) const {
        if (state >= order())
            throw std::out_of_range("recurrence previous state");
        return external_ + 1 + state;
    }
    DiscreteExpression previous(std::size_t state) const {
        return DiscreteExpression::variable(previousSlot(state), transition_.dimensions(),
                                            transition_.limits());
    }
    IntegerRecurrence withTransition(const DiscreteExpression &expression) const {
        return {external_, initial_, expression, limits_};
    }
    DiscreteEvaluationResult evaluate(std::int64_t n, std::span<const std::int64_t> external = {},
                                      DiscreteEvaluationLimits limits = {},
                                      const std::atomic<bool> *cancelled = nullptr) const {
        return evaluateWithCancellation(
            n, external, limits, [&] { return cancelled && cancelled->load(std::memory_order_relaxed); });
    }
    template <class Stop>
    DiscreteEvaluationResult evaluateWithCancellation(std::int64_t n, std::span<const std::int64_t> external,
                                                      DiscreteEvaluationLimits limits, Stop &&stop) const {
        return evaluateTransition(transition_, n, external, limits, std::forward<Stop>(stop));
    }

  private:
    static std::vector<std::int64_t> checkedInitial(std::size_t external,
                                                    const std::vector<std::int64_t> &initial,
                                                    const DiscreteExpression &transition,
                                                    IntegerRecurrenceLimits limits) {
        if (initial.empty() || initial.size() > 2 || external > 1024 || limits.maxSteps > 1000000000 ||
            !limits.maxWorkspaceBytes)
            throw std::invalid_argument("recurrence order/schema/resource policy");
        if (transition.dimensions() != external + initial.size() + 1 ||
            transition.type() != DiscreteType::Int64 || !transition.outerBindings().empty())
            throw std::invalid_argument("recurrence transition schema/type");
        if (transition.nodeCount() > limits.maxWorkspaceBytes / (2 * sizeof(DiscreteNode)))
            throw std::length_error("recurrence owned model storage");
        return initial;
    }
    friend class IntegerRecurrenceMeasurement;
    template <class Stop>
    DiscreteEvaluationResult evaluateTransition(const DiscreteExpression &transition, std::int64_t n,
                                                std::span<const std::int64_t> external,
                                                DiscreteEvaluationLimits limits, Stop &&stop) const {
        if (n < 0 || external.size() != external_)
            throw std::invalid_argument("nonnegative recurrence index and complete external schema required");
        DiscreteEvaluationResult result;
        const auto fail = [&](DiscreteEvaluationStatus status, const char *why) {
            result.status = status;
            result.reason = why;
            return result;
        };
        if (stop())
            return fail(DiscreteEvaluationStatus::Cancelled, "recurrence cancelled");
        if (static_cast<std::uint64_t>(n) < order()) {
            result.status = DiscreteEvaluationStatus::Valid;
            result.value = DiscreteValue::integer(initial_[static_cast<std::size_t>(n)]);
            return result;
        }
        const auto steps = static_cast<std::uint64_t>(n) - order() + 1;
        if (steps > limits_.maxSteps || steps > limits.maxAggregateIterations)
            return fail(DiscreteEvaluationStatus::ResourceExhausted, "recurrence step allowance exhausted");
        const auto slots = transition.dimensions();
        const auto cap = std::min(limits_.maxWorkspaceBytes, limits.maxWorkspaceBytes);
        if (slots > cap / sizeof(std::int64_t) ||
            order() > (cap - slots * sizeof(std::int64_t)) / sizeof(std::int64_t))
            return fail(DiscreteEvaluationStatus::ResourceExhausted,
                        "recurrence binding workspace exhausted");
        const auto bytes = (slots + order()) * sizeof(std::int64_t);
        std::vector<std::int64_t> bindings(slots);
        std::copy(external.begin(), external.end(), bindings.begin());
        auto state = initial_;
        for (std::uint64_t i = 0; i < steps; ++i) {
            if (stop())
                return fail(DiscreteEvaluationStatus::Cancelled, "recurrence cancelled");
            if (result.operations == limits.maxOperations ||
                result.iterations == limits.maxAggregateIterations)
                return fail(DiscreteEvaluationStatus::ResourceExhausted,
                            "recurrence aggregate work exhausted");
            ++result.operations;
            ++result.iterations;
            bindings[indexSlot()] = static_cast<std::int64_t>(i);
            for (std::size_t j = 0; j < order(); ++j)
                bindings[previousSlot(j)] = state[j];
            auto local = limits;
            local.maxWorkspaceBytes = cap - bytes;
            local.maxNodeVisits -= result.nodeVisits;
            local.maxOperations -= result.operations;
            local.maxAggregateIterations -= result.iterations;
            auto value = transition.evaluateWithCancellation(bindings, local, stop);
            result.nodeVisits += value.nodeVisits;
            result.operations += value.operations;
            result.iterations += value.iterations;
            result.iterationOperations += value.iterationOperations;
            if (!value.valid())
                return fail(value.status, value.reason);
            for (std::size_t j = 1; j < order(); ++j) {
                state[j - 1] = state[j];
            }
            state.back() = value.value->integer();
        }
        result.status = DiscreteEvaluationStatus::Valid;
        result.value = DiscreteValue::integer(state.back());
        return result;
    }

  public:
    std::string toString() const {
        std::string out = "recurrence(external=" + std::to_string(external_) + ", initial=[";
        for (std::size_t i = 0; i < order(); ++i) {
            if (i)
                out += ", ";
            out += std::to_string(initial_[i]);
        }
        return out + "], transition=" + transition_.toString() + ")";
    }
    CheckpointDocument checkpoint() const {
        CheckpointWriter w;
        w.integer(external_);
        w.integer(order());
        for (auto n : initial_)
            w.text(std::to_string(n));
        w.integer(limits_.maxSteps);
        w.integer(limits_.maxWorkspaceBytes);
        const auto l = transition_.limits();
        for (auto n : {l.maxNodes, l.maxDepth, l.maxVariables, l.maxConstants, l.maxStorageBytes,
                       l.maxIterationDepth, l.maxBinders})
            w.integer(n);
        w.integer(l.maxConstantMagnitude);
        w.integer(transition_.nodeCount());
        for (auto n : transition_.nodes()) {
            w.integer(static_cast<unsigned>(n.kind()));
            if (n.kind() == DiscreteKind::Integer)
                w.text(std::to_string(n.integer()));
            else if (n.kind() == DiscreteKind::Boolean)
                w.integer(n.boolean());
            else if (n.kind() == DiscreteKind::Variable)
                w.integer(n.variable());
            else if (n.kind() == DiscreteKind::Iterator || discreteIteration(n.kind()))
                w.integer(n.binder());
        }
        CheckpointDocument d;
        d.codec = "genetik.integer-recurrence/1";
        d.identity = "Int64;ordered-oldest-state;index-zero/1";
        d.payload = w.data();
        return d;
    }
    static IntegerRecurrence restore(const CheckpointDocument &d) {
        if (d.codec != "genetik.integer-recurrence/1" ||
            d.identity != "Int64;ordered-oldest-state;index-zero/1" || d.runtime != checkpointRuntime())
            throw CheckpointError("recurrence codec/context differs");
        CheckpointReader r(d.payload);
        auto external = r.count(1024), order = r.count(2);
        if (!order)
            throw CheckpointError("zero recurrence order");
        std::vector<std::int64_t> initial;
        for (std::size_t i = 0; i < order; ++i)
            initial.push_back(readInteger(r));
        IntegerRecurrenceLimits policy;
        policy.maxSteps = r.count(1000000000);
        policy.maxWorkspaceBytes = r.count(64 * 1024 * 1024);
        DiscreteExpressionLimits l;
        l.maxNodes = r.count(100000);
        l.maxDepth = r.count(4096);
        l.maxVariables = r.count(4096);
        l.maxConstants = r.count(100000);
        l.maxStorageBytes = r.count(64 * 1024 * 1024);
        l.maxIterationDepth = r.count(128);
        l.maxBinders = r.count(4096);
        l.maxConstantMagnitude = r.integer();
        genetik_discrete_detail::validateLimits(l);
        auto count = r.count(std::min({l.maxNodes, r.limits().maxBytes / 8,
                                       l.maxStorageBytes / DiscreteExpression::storageBytesPerNode()}));
        std::vector<DiscreteNode> nodes;
        nodes.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto k = static_cast<DiscreteKind>(r.count(25));
            if (k == DiscreteKind::Integer)
                nodes.push_back(DiscreteNode::integer(readInteger(r)));
            else if (k == DiscreteKind::Boolean)
                nodes.push_back(DiscreteNode::boolean(r.count(1)));
            else if (k == DiscreteKind::Variable)
                nodes.push_back(DiscreteNode::variable(r.count(external + order)));
            else if (k == DiscreteKind::Iterator || discreteIteration(k)) {
                auto id = r.integer();
                nodes.push_back(k == DiscreteKind::Iterator ? DiscreteNode::iterator(id)
                                                            : DiscreteNode::iteration(k, id));
            } else
                nodes.push_back(DiscreteNode::operation(k));
        }
        r.finish();
        return {external, initial, DiscreteExpression(nodes, external + order + 1, l), policy};
    }

  private:
    static std::int64_t readInteger(CheckpointReader &r) {
        auto text = r.text(20);
        std::int64_t n = 0;
        auto result = std::from_chars(text.data(), text.data() + text.size(), n);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
            throw CheckpointError("invalid recurrence integer");
        return n;
    }
    std::size_t external_;
    std::vector<std::int64_t> initial_;
    DiscreteExpression transition_;
    IntegerRecurrenceLimits limits_;
};
// The recurrence order/initial values are prescribed problem context; transition
// expression structure evolves through DiscreteVariation and the existing GA.
class IntegerRecurrenceMeasurement final : public DiscreteMeasurement {
  public:
    IntegerRecurrenceMeasurement(std::size_t external, std::vector<std::int64_t> initial,
                                 IntegerRecurrenceLimits limits, DiscreteExpressionLimits expressionLimits)
        : prototype_(external, initial,
                     DiscreteExpression::variable(external, external + initial.size() + 1, expressionLimits),
                     limits) {}
    bool parallelEvaluationSafe() const noexcept override { return true; }
    std::string identity() const override {
        auto d = prototype_.checkpoint();
        CheckpointWriter w;
        w.text(d.codec);
        w.text(d.payload);
        return w.data();
    }
    DiscreteEvaluationResult evaluate(const DiscreteExpression &transition,
                                      std::span<const std::int64_t> bindings, DiscreteEvaluationLimits limits,
                                      const ExecutionContext *context) const override {
        if (bindings.size() != transition.dimensions())
            throw std::invalid_argument("recurrence observation schema differs");
        for (std::size_t j = prototype_.indexSlot() + 1; j < bindings.size(); ++j)
            if (bindings[j] != 0)
                throw std::invalid_argument("previous-state observation slots must be zero placeholders");
        return prototype_.evaluateTransition(transition, bindings[prototype_.indexSlot()],
                                             bindings.first(prototype_.externalDimensions()), limits,
                                             [&] { return context && context->stopRequested(); });
    }

  private:
    IntegerRecurrence prototype_;
};

struct IntegerRecurrenceObservation {
    std::int64_t index;
    std::vector<std::int64_t> external;
    std::int64_t target;
};
inline std::shared_ptr<const DiscreteRegressionProblem>
recurrenceRegressionProblem(std::size_t external, const std::vector<std::int64_t> &initial,
                            const std::vector<IntegerRecurrenceObservation> &observations,
                            DiscreteExpressionLimits expression = {},
                            DiscreteEvaluationLimits evaluation = {}, DiscreteDatasetLimits data = {},
                            IntegerRecurrenceLimits recurrence = {}) {
    if (initial.empty() || initial.size() > 2 || external > 1024)
        throw std::invalid_argument("recurrence problem schema");
    std::vector<DiscreteRegressionRow> rows;
    rows.reserve(observations.size());
    for (const auto &observation : observations) {
        if (observation.external.size() != external || observation.index < 0)
            throw std::invalid_argument("recurrence observation input/index");
        if (static_cast<std::uint64_t>(observation.index) >= initial.size() &&
            static_cast<std::uint64_t>(observation.index) - initial.size() + 1 > recurrence.maxSteps)
            throw std::invalid_argument("recurrence observation cannot fit the prescribed step policy");
        auto bindings = observation.external;
        bindings.push_back(observation.index);
        bindings.resize(external + initial.size() + 1, 0);
        rows.push_back({std::move(bindings), observation.target});
    }
    auto measurement =
        std::make_shared<const IntegerRecurrenceMeasurement>(external, initial, recurrence, expression);
    return DiscreteRegressionProblem::create(external + initial.size() + 1, rows, expression, evaluation,
                                             data, measurement);
}
