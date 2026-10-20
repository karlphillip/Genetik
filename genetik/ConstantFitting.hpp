#pragma once
#include "Regression.hpp"
#include <exception>
#include <set>

inline void countTrainingWork(std::uint64_t& counter) {
    if (counter == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("training work counter exhausted");
    ++counter;
}
struct TrainingWork {
    std::uint64_t lossAttempts = 0, rowAttempts = 0, fittingInvocations = 0, fittingLossAttempts = 0;
    std::uint64_t proposals = 0, accepted = 0, invalid = 0, infeasible = 0, duplicates = 0;
    std::uint64_t invalidLosses = 0; // all invalid objective results, including starting losses
    bool operator==(const TrainingWork&) const = default;
};
class TrainingLossBudgetExceeded : public std::runtime_error {
public: TrainingLossBudgetExceeded() : std::runtime_error("global training-loss allowance exhausted") {}
};
struct ConstantFitConfig;
struct ConstantFitResult;
class TrainingObjective;
inline ConstantFitResult fitConstants(const Expression&, TrainingObjective&, const ConstantFitConfig&);
// Run-owned accounting and an optional fault/optimizer integration seam. Charge before compute,
// including invalid results and exceptions. Reporting calls to problem.measure are separate.
class TrainingObjective {
public:
    explicit TrainingObjective(std::shared_ptr<const RegressionProblem> problem,
                              std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max())
        : problem_(std::move(problem)), maximum_(maximum) {
        if (!problem_) throw std::invalid_argument("training objective requires a problem");
    }
    TrainingObjective(const TrainingObjective&) = delete;
    TrainingObjective& operator=(const TrainingObjective&) = delete;
    TrainingObjective(TrainingObjective&&) = delete;
    TrainingObjective& operator=(TrainingObjective&&) = delete;
    virtual ~TrainingObjective() = default;
    RegressionMetrics measure(const Expression& expression, bool fitting = false) {
        if (work_.lossAttempts >= maximum_) throw TrainingLossBudgetExceeded();
        countTrainingWork(work_.lossAttempts);
        if (fitting) countTrainingWork(work_.fittingLossAttempts);
        try {
            auto result = compute(expression);
            if (result.loss.state() == FitnessState::Invalid) countTrainingWork(work_.invalidLosses);
            return result;
        }
        catch (...) { std::throw_with_nested(std::runtime_error("training loss attempt " + std::to_string(work_.lossAttempts) + " failed")); }
    }
    const std::shared_ptr<const RegressionProblem>& problem() const noexcept { return problem_; }
    std::uint64_t maximum() const noexcept { return maximum_; }
    std::uint64_t remaining() const noexcept { return maximum_ - work_.lossAttempts; }
    const TrainingWork& work() const noexcept { return work_; }
protected:
    virtual RegressionMetrics compute(const Expression& expression) {
        return problem_->measure(expression, &work_.rowAttempts);
    }
private:
    friend ConstantFitResult fitConstants(const Expression&, TrainingObjective&, const ConstantFitConfig&);
    TrainingWork work_;
    std::shared_ptr<const RegressionProblem> problem_;
    std::uint64_t maximum_;
};
struct ConstantFitConfig {
    double lower = -8, upper = 8, initialStep = 1, minimumStep = 1e-8;
    std::uint64_t maxLossAttempts = 256, maxSweeps = 128, maxProposals = 8192;
    std::optional<double> lossThreshold;
};
inline void validateConstantFitConfig(const ConstantFitConfig& c, const RegressionProblem& problem) {
    if (!std::isfinite(c.lower) || !std::isfinite(c.upper) || c.lower > c.upper ||
        std::abs(c.lower) > problem.limits().maxAbsConstant || std::abs(c.upper) > problem.limits().maxAbsConstant ||
        !std::isfinite(c.initialStep) || !std::isfinite(c.minimumStep) || c.minimumStep <= 0 ||
        c.initialStep < c.minimumStep || c.maxLossAttempts == 0 || c.maxSweeps == 0 || c.maxProposals == 0 ||
        (c.lossThreshold && (!std::isfinite(*c.lossThreshold) || *c.lossThreshold < 0)))
        throw std::invalid_argument("invalid constant fit configuration");
}
enum class ConstantFitStop { NoConstants, LossThreshold, InvalidStart, MinimumStep, SweepLimit, ProposalLimit, EvaluationLimit };
inline const char* constantFitStopName(ConstantFitStop s) {
    switch (s) {
    case ConstantFitStop::NoConstants: return "no_constants";
    case ConstantFitStop::LossThreshold: return "loss_threshold";
    case ConstantFitStop::InvalidStart: return "invalid_start";
    case ConstantFitStop::MinimumStep: return "minimum_step";
    case ConstantFitStop::SweepLimit: return "sweep_limit";
    case ConstantFitStop::ProposalLimit: return "proposal_limit";
    case ConstantFitStop::EvaluationLimit: return "evaluation_limit";
    }
    throw std::logic_error("unknown fit stop");
}
struct ConstantFitResult {
    Expression expression;
    Fitness loss, startingLoss;
    bool improved = false;
    ConstantFitStop stop = ConstantFitStop::SweepLimit;
    std::uint64_t lossAttempts = 0, sweeps = 0, proposals = 0;
};
// Always obtains a fresh baseline: no caller-provided numerical cache can certify a different tree.
// Each occurrence is independent, traversed in postfix order; each coordinate probes +step then
// -step relative to its value at coordinate entry. Equal/invalid trials keep the incumbent.
inline ConstantFitResult fitConstants(const Expression& input, TrainingObjective& objective,
                                     const ConstantFitConfig& config = {}) {
    validateConstantFitConfig(config, *objective.problem());
    (void)objective.problem()->checkedExpression(input);
    auto expression = input; // preserve the value's own limits as well as enforcing problem limits
    std::vector<std::size_t> parameters;
    const auto nodes = expression.nodes();
    for (std::size_t i = 0; i < nodes.size(); ++i) if (nodes[i].kind() == ExpressionKind::Constant) {
        const auto value = nodes[i].constantValue();
        if (value < config.lower || value > config.upper) throw std::invalid_argument("starting constant outside fitting bounds");
        parameters.push_back(i);
    }
    countTrainingWork(objective.work_.fittingInvocations);
    ConstantFitResult result{std::move(expression), {}, {}};
    const auto begin = objective.work_.lossAttempts;
    auto finish = [&](ConstantFitStop stop) {
        result.stop = stop; result.lossAttempts = objective.work_.lossAttempts - begin;
        return std::move(result);
    };
    result.loss = result.startingLoss = objective.measure(result.expression, true).loss;
    if (result.loss.state() == FitnessState::Invalid) return finish(ConstantFitStop::InvalidStart);
    auto reached = [&] { return config.lossThreshold && result.loss.score() <= *config.lossThreshold; };
    if (reached()) return finish(ConstantFitStop::LossThreshold);
    if (parameters.empty()) return finish(ConstantFitStop::NoConstants);
    double step = config.initialStep;
    std::set<std::string> visited{result.expression.toString()}; // bounded by maxProposals+1
    while (result.sweeps < config.maxSweeps) {
        ++result.sweeps;
        bool sweepImproved = false;
        for (auto parameter : parameters) {
            const double origin = result.expression.nodes()[parameter].constantValue();
            for (double direction : {1., -1.}) {
                if (result.proposals >= config.maxProposals) return finish(ConstantFitStop::ProposalLimit);
                ++result.proposals; countTrainingWork(objective.work_.proposals);
                volatile double proposed = origin + direction * step;
                if (!std::isfinite(proposed) || proposed < config.lower || proposed > config.upper ||
                    std::abs(proposed) > result.expression.limits().maxAbsConstant) {
                    countTrainingWork(objective.work_.infeasible); continue;
                }
                if (proposed == origin) { countTrainingWork(objective.work_.duplicates); continue; }
                auto trial = result.expression.withConstant(parameter, proposed);
                if (!visited.insert(trial.toString()).second) { countTrainingWork(objective.work_.duplicates); continue; }
                if (objective.work_.lossAttempts - begin >= config.maxLossAttempts) return finish(ConstantFitStop::EvaluationLimit);
                const auto score = objective.measure(trial, true).loss;
                if (score.state() == FitnessState::Invalid) { countTrainingWork(objective.work_.invalid); continue; }
                if (score.score() < result.loss.score()) {
                    result.expression.swap(trial); result.loss = score;
                    result.improved = sweepImproved = true; countTrainingWork(objective.work_.accepted);
                    if (reached()) return finish(ConstantFitStop::LossThreshold);
                }
            }
        }
        if (!sweepImproved) {
            const double next = step * .5;
            if (next < config.minimumStep || next == step) return finish(ConstantFitStop::MinimumStep);
            step = next;
        }
    }
    return finish(ConstantFitStop::SweepLimit);
}

// Shared only for run accounting, never for mutable genomes. No loss cache is reused, including
// elites and unchanged clones; baseline and accepted probes already certify the installed value.
class ConstantRefinement final : public RegressionPreparation {
public:
    ConstantRefinement(std::shared_ptr<TrainingObjective> objective, std::optional<ConstantFitConfig> config = {})
        : objective_(std::move(objective)), config_(config) {
        if (!objective_) throw std::invalid_argument("refinement requires an objective");
        if (config_) validateConstantFitConfig(*config_, *objective_->problem());
    }
    PreparedExpression prepare(const Expression& expression, const RegressionProblem& problem) override {
        if (&problem != objective_->problem().get()) throw std::invalid_argument("refinement problem identity differs");
        if (!config_) {
            const auto loss = objective_->measure(expression).loss;
            recordInitial(loss); return {expression, loss};
        }
        auto result = fitConstants(expression, *objective_, *config_);
        recordInitial(result.startingLoss);
        return {std::move(result.expression), result.loss};
    }
    bool initializing = true;
    std::optional<double> rawInitialBest;
private:
    void recordInitial(Fitness loss) {
        if (initializing && loss.state() == FitnessState::Valid && (!rawInitialBest || loss.score() < *rawInitialBest))
            rawInitialBest = loss.score();
    }
    std::shared_ptr<TrainingObjective> objective_;
    const std::optional<ConstantFitConfig> config_;
};
