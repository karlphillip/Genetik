#pragma once
#include <typeinfo>
#include <type_traits>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include "EvaluatedCandidate.hpp"
#include "Expression.hpp"
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

struct RegressionRow {
    std::vector<double> inputs;
    double target;
};
enum class RegressionLoss { MeanSquaredError, MeanAbsoluteError, Huber, WeightedMeanSquaredError };
struct RegressionObjectiveConfig {
    RegressionLoss loss = RegressionLoss::MeanSquaredError;
    double huberDelta = 1.0;
    std::vector<double> observationWeights;
};
struct RegressionSupplementalWork {
    std::uint64_t mathematicalOperations = 0;
    std::uint64_t fieldEvaluations = 0;
    bool operator==(const RegressionSupplementalWork& supplemental) const = default;
};
struct RegressionMetrics {
    Fitness loss;
    std::optional<double> maxAbsoluteError;
    std::size_t samplesAttempted = 0;
};
struct RegressionReport {
    RegressionMetrics metrics;
    std::optional<double> rmse;
};

// Immutable, copied inputs and configuration. Replacing the problem requires a new population/run.
class RegressionProblem {
    struct ObjectiveTag {};

public:
    RegressionProblem(const std::vector<RegressionRow>& rows, ExpressionLimits limits = {31, 6, 2},
                      RegressionLoss loss = RegressionLoss::MeanSquaredError)
        : RegressionProblem(rows, limits, RegressionObjectiveConfig{loss, 1.0, {}},
                            ObjectiveTag{}) {}

private:
    RegressionProblem(const std::vector<RegressionRow>& rows, ExpressionLimits limits,
                      RegressionObjectiveConfig objective, ObjectiveTag)
        : rows_(validate(rows)), dimensions_(rows_.front().inputs.size()), limits_(limits),
          objective_(validateObjective(std::move(objective), rows_.size())) {
        (void)Expression::constant(0, dimensions_, limits_); // validate limits, independently of any genome
    }

public:
    static std::shared_ptr<const RegressionProblem>
    create(const std::vector<RegressionRow>& rows, ExpressionLimits limits = {31, 6, 2},
           RegressionLoss loss = RegressionLoss::MeanSquaredError) {
        return std::make_shared<const RegressionProblem>(rows, limits, loss);
    }
    static std::shared_ptr<const RegressionProblem>
    createWithObjective(const std::vector<RegressionRow>& rows, RegressionObjectiveConfig objective,
                        ExpressionLimits limits = {31, 6, 2}) {
        return std::shared_ptr<const RegressionProblem>(
            new RegressionProblem(rows, limits, std::move(objective), ObjectiveTag{}));
    }
    virtual ~RegressionProblem() = default;
    virtual bool parallelEvaluationSafe() const noexcept {
        return typeid(*this) == typeid(RegressionProblem);
    }
    std::size_t dimensions() const noexcept { return dimensions_; }
    virtual std::size_t sampleCount() const noexcept { return rows_.size(); }
    // Native scalar rows are exposed read-only for optional fixed-topology
    // numerical methods; full-model adapters keep their own observation types.
    const std::vector<RegressionRow>& trainingRows() const noexcept { return rows_; }
    ExpressionLimits limits() const noexcept { return limits_; }
    const RegressionObjectiveConfig& objective() const noexcept {
        return objective_;
    }
    Expression checkedExpression(const Expression& expression) const {
        if (expression.variableCount() != dimensions_) throw std::invalid_argument("expression/problem dimensions differ");
        return Expression(expression.nodes(), dimensions_, limits_);
    }
    // Optional per-invocation accounting. The immutable objective never owns
    // mutable counters; supplied counters belong to the worker objective.
    virtual RegressionMetrics measureWithWork(const Expression& expression, std::uint64_t* rows,
                                              RegressionSupplementalWork& supplemental) const {
        if (typeid(*this) == typeid(RegressionProblem))
            return measureValue(expression, rows, &supplemental, false).metrics;
        return measure(expression, rows);
    }
    virtual RegressionMetrics measure(const Expression& expression, std::uint64_t* rowAttempts = nullptr) const {
        return measureValue(expression, rowAttempts, nullptr, false).metrics;
    }
    RegressionReport measureDetailed(const Expression& expression) const {
        if (typeid(*this) != typeid(RegressionProblem))
            return {measure(expression), {}};
        return measureValue(expression, nullptr, nullptr, true);
    }

private:
    RegressionReport measureValue(const Expression& expression, std::uint64_t* rowAttempts,
                                  RegressionSupplementalWork* supplemental,
                                  bool includeRmse) const {
        if (expression.variableCount() != dimensions_) throw std::invalid_argument("expression/problem dimensions differ");
        if (rows_.empty())
            throw std::logic_error("schema-only problem requires a measurement override");
        // Preserve the original MSE arithmetic sequence exactly for the default API.
        if (objective_.loss == RegressionLoss::MeanSquaredError)
            return measureMse(expression, rowAttempts, supplemental, includeRmse);

        double sum = 0, maximum = 0, squaredSum = 0;
        double weightSum = 0;
        std::size_t attempted = 0;
        // Empty construction cannot allocate. First growth stays after the first
        // attempted-row charge, within the already charged training-loss call.
        genetik_detail::EvaluationWorkspace workspace;
        for (std::size_t rowIndex = 0; rowIndex < rows_.size(); ++rowIndex) {
            const auto& row = rows_[rowIndex];
            ++attempted;
            if (rowAttempts) {
                if (*rowAttempts == std::numeric_limits<std::uint64_t>::max())
                    throw std::overflow_error("row counter exhausted");
                ++*rowAttempts;
            }
            const auto prediction = workspace.evaluate(expression, row.inputs);
            if (supplemental) {
                auto cost = prediction.arithmeticOperations();
                if (cost > UINT64_MAX - supplemental->mathematicalOperations ||
                    supplemental->fieldEvaluations == UINT64_MAX)
                    throw std::overflow_error("regression supplemental work");
                supplemental->mathematicalOperations += cost;
                ++supplemental->fieldEvaluations;
            }
            if (!prediction.valid())
                return {{Fitness::invalid(), {}, attempted}, {}};
            volatile double residual = prediction.value() - row.target;
            if (!std::isfinite(residual))
                return {{Fitness::invalid(), {}, attempted}, {}};
            const double magnitude = std::abs(static_cast<double>(residual));
            double square = 0.0;
            if (includeRmse || objective_.loss == RegressionLoss::Huber ||
                objective_.loss == RegressionLoss::WeightedMeanSquaredError) {
                volatile double checkedSquare = residual * residual;
                square = checkedSquare;
            }
            if (includeRmse) {
                if (std::isfinite(square)) {
                    volatile double nextSquared = squaredSum + square;
                    if (std::isfinite(nextSquared))
                        squaredSum = nextSquared;
                    else
                        squaredSum = std::numeric_limits<double>::infinity();
                } else {
                    squaredSum = std::numeric_limits<double>::infinity();
                }
            }
            double contribution = magnitude;
            if (objective_.loss == RegressionLoss::Huber) {
                if (magnitude <= objective_.huberDelta) {
                    volatile double halfSquare = 0.5 * square;
                    contribution = halfSquare;
                } else {
                    volatile double linear = magnitude - 0.5 * objective_.huberDelta;
                    volatile double tail = objective_.huberDelta * linear;
                    contribution = tail;
                }
            } else if (objective_.loss == RegressionLoss::WeightedMeanSquaredError) {
                const double weight = objective_.observationWeights[rowIndex];
                if (weight == 0.0) {
                    contribution = 0.0;
                } else {
                    volatile double weighted = weight * square;
                    contribution = weighted;
                }
                volatile double nextWeight = weightSum + weight;
                if (!std::isfinite(nextWeight))
                    return {{Fitness::invalid(), {}, attempted}, {}};
                weightSum = nextWeight;
            }
            if (!std::isfinite(contribution))
                return {{Fitness::invalid(), {}, attempted}, {}};
            volatile double next = sum + contribution;
            if (!std::isfinite(next))
                return {{Fitness::invalid(), {}, attempted}, {}};
            sum = next;
            maximum = std::max(maximum, magnitude);
        }
        const double divisor = objective_.loss == RegressionLoss::WeightedMeanSquaredError
                                   ? weightSum
                                   : static_cast<double>(rows_.size());
        volatile double score = sum / divisor;
        if (!std::isfinite(score))
            return {{Fitness::invalid(), {}, attempted}, {}};
        std::optional<double> rmse;
        if (includeRmse && std::isfinite(squaredSum)) {
            volatile double meanSquare = squaredSum / static_cast<double>(rows_.size());
            if (std::isfinite(meanSquare)) {
                volatile double root = std::sqrt(meanSquare);
                if (std::isfinite(root))
                    rmse = root;
            }
        }
        return {{Fitness::valid(score), maximum, attempted}, rmse};
    }

    RegressionReport measureMse(const Expression& expression, std::uint64_t* rowAttempts,
                                RegressionSupplementalWork* supplemental, bool includeRmse) const {
        double sum = 0, maximum = 0;
        std::size_t attempted = 0;
        genetik_detail::EvaluationWorkspace workspace;
        for (const auto& row : rows_) {
            ++attempted;
            if (rowAttempts) {
                if (*rowAttempts == std::numeric_limits<std::uint64_t>::max())
                    throw std::overflow_error("row counter exhausted");
                ++*rowAttempts;
            }
            const auto prediction = workspace.evaluate(expression, row.inputs);
            if (supplemental) {
                auto cost = prediction.arithmeticOperations();
                if (cost > UINT64_MAX - supplemental->mathematicalOperations ||
                    supplemental->fieldEvaluations == UINT64_MAX)
                    throw std::overflow_error("regression supplemental work");
                supplemental->mathematicalOperations += cost;
                ++supplemental->fieldEvaluations;
            }
            if (!prediction.valid())
                return {{Fitness::invalid(), {}, attempted}, {}};
            volatile double residual = prediction.value() - row.target;
            if (!std::isfinite(residual))
                return {{Fitness::invalid(), {}, attempted}, {}};
            volatile double square = residual * residual;
            if (!std::isfinite(square))
                return {{Fitness::invalid(), {}, attempted}, {}};
            volatile double next = sum + square;
            if (!std::isfinite(next))
                return {{Fitness::invalid(), {}, attempted}, {}};
            sum = next;
            maximum = std::max(maximum, std::abs(static_cast<double>(residual)));
        }
        volatile double mse = sum / static_cast<double>(rows_.size());
        if (!std::isfinite(mse))
            return {{Fitness::invalid(), {}, attempted}, {}};
        std::optional<double> rmse;
        if (includeRmse) {
            volatile double root = std::sqrt(mse);
            if (std::isfinite(root))
                rmse = root;
        }
        return {{Fitness::valid(mse), maximum, attempted}, rmse};
    }

protected:
    // Mathematical objective adapter: no fictitious value labels. Derived problems
    // override measure and explicitly qualify concurrent immutable measurement.
    struct MeasurementSchema {std::size_t dimensions;ExpressionLimits limits;};
    // A constrained single-argument constructor does not participate in existing
    // braced row-list calls: its Schema cannot be deduced from a bare {}.
    template <class Schema>
        requires std::is_same_v<Schema, MeasurementSchema>
    explicit RegressionProblem(Schema schema)
        : dimensions_(schema.dimensions), limits_(schema.limits), objective_{} {
        if(!dimensions_)throw std::invalid_argument("objective schema must be positive");
        (void)Expression::constant(0, dimensions_, limits_);
    }

private:
    static std::vector<RegressionRow> validate(const std::vector<RegressionRow>& input) {
        if (input.empty()) throw std::invalid_argument("regression rows must be nonempty");
        auto copy = input; // detach rows and every nested input vector, even for caller rvalues
        const auto dimensions = copy.front().inputs.size();
        for (const auto& row : copy) {
            if (row.inputs.size() != dimensions || !std::isfinite(row.target))
                throw std::invalid_argument("inconsistent or non-finite regression rows");
            for (double value : row.inputs)
                if (!std::isfinite(value)) throw std::invalid_argument("non-finite regression binding");
        }
        return copy;
    }
    static RegressionObjectiveConfig validateObjective(RegressionObjectiveConfig objective,
                                                       std::size_t observations) {
        switch (objective.loss) {
        case RegressionLoss::MeanSquaredError:
        case RegressionLoss::MeanAbsoluteError:
            if (!objective.observationWeights.empty())
                throw std::invalid_argument("observation weights require weighted MSE");
            break;
        case RegressionLoss::Huber:
            if (!objective.observationWeights.empty() || !std::isfinite(objective.huberDelta) ||
                objective.huberDelta <= 0.0)
                throw std::invalid_argument(
                    "Huber loss requires a finite positive delta and no weights");
            break;
        case RegressionLoss::WeightedMeanSquaredError: {
            if (objective.observationWeights.size() != observations)
                throw std::invalid_argument("weighted MSE requires one weight per observation");
            double sum = 0.0;
            for (const double weight : objective.observationWeights) {
                if (!std::isfinite(weight) || weight < 0.0)
                    throw std::invalid_argument(
                        "observation weights must be finite and nonnegative");
                volatile double next = sum + weight;
                if (!std::isfinite(next))
                    throw std::invalid_argument("observation weight sum overflow");
                sum = next;
            }
            if (!(sum > 0.0))
                throw std::invalid_argument("observation weights must have positive total");
            break;
        }
        default:
            throw std::invalid_argument("unsupported regression objective");
        }
        return objective;
    }
    const std::vector<RegressionRow> rows_;
    const std::size_t dimensions_;
    const ExpressionLimits limits_;
    const RegressionObjectiveConfig objective_{};
};

enum class PreparationOrigin { Initialization, ChangedOffspring, UnchangedOffspring, Elite, Unspecified };
struct PreparationContext {std::uint64_t generation=0;PreparationOrigin origin=PreparationOrigin::Unspecified;};

struct PreparedExpression { Expression expression; Fitness loss; };
// Trusted preparation extension, analogous to the evaluator: must return the exact value scored
// against this immutable problem. Implementations must not modify the input expression.
class RegressionPreparation {
public:
    virtual ~RegressionPreparation() = default;
    virtual bool parallelEvaluationSafe() const noexcept {return false;}
    virtual std::shared_ptr<RegressionPreparation> forkParallel(ExecutionContext&) {
        throw std::invalid_argument("preparation has no independent parallel factory");
    }
    virtual void mergeParallel(const RegressionPreparation&) {}
    virtual PreparedExpression prepare(const Expression&, const RegressionProblem&) = 0;
    // Existing two-argument custom preparations remain source compatible.
    virtual PreparedExpression prepare(const Expression& expression,const RegressionProblem& problem,PreparationContext) {
        return prepare(expression,problem);
    }
};

// Optional immutable mathematical context. Implementations validate without editing;
// explicit parallel opt-in covers validation only, not the preparation service.
class RegressionCandidateContext {
public:
    virtual ~RegressionCandidateContext() = default;
    virtual std::shared_ptr<const RegressionProblem> problem() const = 0;
    virtual std::shared_ptr<RegressionPreparation> preparation() const = 0;
    virtual void validate(const Expression&) const = 0;
    virtual bool parallelEvaluationSafe() const noexcept {return false;}
};

class ExpressionCandidate : public EvaluatedCandidate {
    friend struct CandidateCheckpointCodec<ExpressionCandidate>;
public:
    ExpressionCandidate(const Expression& expression, std::shared_ptr<const RegressionProblem> problem,
                        std::shared_ptr<RegressionPreparation> preparation = {})
        : problem_(requireProblem(std::move(problem))), expression_(problem_->checkedExpression(expression)),
          text_(expression_.toString()), preparation_(std::move(preparation)) {}
    static ExpressionCandidate constrained(const Expression& expression,
                          std::shared_ptr<const RegressionCandidateContext> context) {
        if(!context)throw std::invalid_argument("candidate context is required");
        ExpressionCandidate candidate(expression,context->problem(),context->preparation());
        candidate.contextOwner_=std::move(context);candidate.contextOwner_->validate(candidate.expression_);
        return candidate;
    }
    ExpressionCandidate(const ExpressionCandidate&) = default;
    ExpressionCandidate(ExpressionCandidate&&) noexcept = default;
    ExpressionCandidate& operator=(ExpressionCandidate other) noexcept { swap(other); return *this; }
    void swap(ExpressionCandidate& other) noexcept {
        swapEvaluation(other); contextOwner_.swap(other.contextOwner_); problem_.swap(other.problem_); expression_.swap(other.expression_); text_.swap(other.text_); preparation_.swap(other.preparation_); parentPreparation_.swap(other.parentPreparation_); std::swap(context_,other.context_);
    }
    bool parallelEvaluationSafe() const noexcept override {
        return typeid(*this)==typeid(ExpressionCandidate) && problem_ && problem_->parallelEvaluationSafe() && (!contextOwner_ || contextOwner_->parallelEvaluationSafe()) && (!preparation_ || preparation_->parallelEvaluationSafe());
    }
    void beginParallelEvaluation(ExecutionContext& execution) override {
        if(preparation_){
            auto local=preparation_->forkParallel(execution);
            if(!local || local.get()==preparation_.get() || local.use_count()!=1)
                throw std::invalid_argument("parallel preparation factory must return an exclusively owned independent instance");
            parentPreparation_=preparation_;preparation_=std::move(local);
        }
    }
    void finishParallelEvaluation() override {
        if(parentPreparation_){auto local=std::move(preparation_);preparation_=std::move(parentPreparation_);preparation_->mergeParallel(*local);}
    }
    // All throwing work precedes installation/invalidation, preserving old genome/cache on failure.
    void replaceExpression(const Expression& expression) {
        if (!problem_) throw std::logic_error("moved-from expression candidate");
        auto ready = problem_->checkedExpression(expression);
        if(contextOwner_)contextOwner_->validate(ready);
        auto text = ready.toString();
        expression_.swap(ready); text_.swap(text); invalidateFitness();
    }
    // Population supplies the authoritative prospective generation before evaluation.
    // Origin is diagnostic only; it never decides cadence or permits fitness reuse.
    void setPreparationGeneration(std::uint64_t generation) noexcept {
        context_.generation=generation;
        if(generation==0)context_.origin=PreparationOrigin::Initialization;
        else if(context_.origin==PreparationOrigin::Initialization)context_.origin=PreparationOrigin::Unspecified;
    }
    void setPreparationOrigin(PreparationOrigin origin) noexcept {context_.origin=origin;}
    const Expression& expression() const noexcept { return expression_; }
    const std::string& text() const noexcept { return text_; }
    std::shared_ptr<const RegressionProblem> problem() const noexcept { return problem_; }
    std::shared_ptr<const void> problemIdentity() const noexcept final { return contextOwner_ ? std::shared_ptr<const void>(contextOwner_):std::shared_ptr<const void>(problem_); }
    std::string stats() override { return text_; }
protected:
    Fitness prepareEvaluation() override {
        if (!preparation_) return evaluate(); // preserve numerical/custom evaluator API
        auto ready = preparation_->prepare(expression_, *problem_,context_);
        if (ready.loss.state() == FitnessState::Unevaluated)
            throw std::logic_error("preparation returned unevaluated fitness");
        replaceExpression(ready.expression); // validate and detach before installation
        return ready.loss;
    }
    Fitness evaluate() const override {
        if (!problem_) throw std::logic_error("moved-from expression candidate");
        return problem_->measure(expression_).loss;
    }
private:
    static std::shared_ptr<const RegressionProblem> requireProblem(std::shared_ptr<const RegressionProblem> problem) {
        if (!problem) throw std::invalid_argument("regression problem is required");
        return problem;
    }
    std::shared_ptr<const RegressionCandidateContext> contextOwner_;
    std::shared_ptr<const RegressionProblem> problem_;
    Expression expression_;
    std::string text_;
    std::shared_ptr<RegressionPreparation> preparation_;
    std::shared_ptr<RegressionPreparation> parentPreparation_;
    PreparationContext context_;
};

// Exact objective comparison, then hard complexity, then deterministic representation order.
struct RegressionOrdering {
    void validateDirection(ObjectiveDirection direction) const {
        if (direction != ObjectiveDirection::Minimize) throw std::invalid_argument("regression requires minimization");
    }
    template<class C> bool better(const C& a, const C& b, ObjectiveDirection direction) const {
        validateDirection(direction);
        if (a.fitness() != b.fitness()) return a.fitness() < b.fitness();
        if (a.expression().nodeCount() != b.expression().nodeCount())
            return a.expression().nodeCount() < b.expression().nodeCount();
        return a.text() < b.text();
    }
};
