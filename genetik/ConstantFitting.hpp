#pragma once
#include <typeinfo>
#include "Regression.hpp"
#include "LevenbergMarquardt.hpp"
#include <exception>
#include <set>
#include <array>
#include <map>
#include "detail/ScopedTiming.hpp"

inline void countTrainingWork(std::uint64_t& counter) {
    if (counter == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("training work counter exhausted");
    ++counter;
}
inline void mergeTrainingCounter(std::uint64_t& destination,std::uint64_t source,bool& overflow) noexcept {
    if(source>std::numeric_limits<std::uint64_t>::max()-destination){destination=std::numeric_limits<std::uint64_t>::max();overflow=true;}
    else destination+=source;
}
struct TrainingWork {
    std::uint64_t lossAttempts = 0, rowAttempts = 0, fittingInvocations = 0, fittingLossAttempts = 0;
    std::uint64_t proposals = 0, accepted = 0, invalid = 0, infeasible = 0, duplicates = 0;
    std::uint64_t invalidLosses = 0; // all invalid objective results, including starting losses
    // Disjoint subsets of lossAttempts; legacy fittingLossAttempts remains baseline+probe.
    std::uint64_t ordinaryLossAttempts=0, fittingBaselineAttempts=0, fittingProbeAttempts=0;
    std::uint64_t fittingImproved=0; // invocations accepting >=1 step, including abandoned fits
    bool operator==(const TrainingWork&) const = default;
};
enum class TrainingLossKind { Ordinary, FittingBaseline, FittingProbe };
struct PreparationWork {
    std::uint64_t requests=0,fits=0,skipped=0,ordinary=0,baselines=0,probes=0,rows=0,improved=0;
    bool operator==(const PreparationWork&) const = default;
};
struct PreparationStatistics {
    std::array<PreparationWork,5> origins{}; // Initialization, Changed, Unchanged, Elite, Unspecified
    std::uint64_t skippedBySchedule=0,completedImproved=0,completedUnchanged=0,noConstants=0,invalidStarts=0,abortedFits=0;
    std::array<std::uint64_t,7> terminations{}; // ConstantFitStop enum order
    bool operator==(const PreparationStatistics&) const = default;
};
inline void mergeLMWork(LMWork& a,const LMWork& b){
    auto add=[](std::size_t& x,std::size_t y){if(y>std::numeric_limits<std::size_t>::max()-x)throw std::overflow_error("optimizer work counter overflow");x+=y;};
    add(a.objectiveEvaluations,b.objectiveEvaluations);add(a.jacobianEvaluations,b.jacobianEvaluations);add(a.forwardEvaluations,b.forwardEvaluations);add(a.derivativeEvaluations,b.derivativeEvaluations);add(a.rowsProcessed,b.rowsProcessed);add(a.parameterUpdates,b.parameterUpdates);add(a.linearSolves,b.linearSolves);add(a.acceptedSteps,b.acceptedSteps);add(a.rejectedSteps,b.rejectedSteps);add(a.mathematicalOperations,b.mathematicalOperations);add(a.linearAlgebraOperations,b.linearAlgebraOperations);a.peakWorkspaceBytes=std::max(a.peakWorkspaceBytes,b.peakWorkspaceBytes);a.seconds+=b.seconds;if(!std::isfinite(a.seconds))throw std::overflow_error("optimizer runtime accumulator overflow");
}
// Optional bounded, run-owned observation. It NEVER supplies fitness or avoids computation.
struct RefinementDiagnostics {
    struct Entry {Expression expression;std::uint64_t requests=0;bool priorOutput=false;};
    explicit RefinementDiagnostics(std::size_t capacity=1024):capacity(capacity) {
        if(capacity==0 || capacity>4096)throw std::invalid_argument("diagnostic capacity must be 1..4096");
    }
    const std::size_t capacity;
    bool bound=false;
    std::map<std::string,Entry> expressions;
    std::uint64_t observed=0,untracked=0,capacityDrops=0,repeated=0,repeatLosses=0,repeatNoChangeLosses=0,repeatImproved=0;
    std::uint64_t previousOutput=0,previousOutputImproved=0,previousOutputUnchanged=0,previousOutputLosses=0;
    double datasetSeconds=0,fittingDatasetSeconds=0,fittingSeconds=0,preparationSeconds=0,generationSeconds=0,publicationSeconds=0;
    LMWork optimizerWork{};
    void recordOptimizerWork(const LMWork& w){mergeLMWork(optimizerWork,w);}
    Entry* findOrInsert(const Expression& expression) {
        const auto key=expression.toString();auto it=expressions.find(key);
        if(it!=expressions.end()) {
            if(!it->second.expression.sameRepresentation(expression))throw std::logic_error("diagnostic expression key collision");
            return &it->second;
        }
        if(expressions.size()==capacity){++capacityDrops;return nullptr;}
        return &expressions.emplace(key,Entry{expression}).first->second;
    }
};
class TrainingLossBudgetExceeded : public EvaluationBudgetExceeded {
public: TrainingLossBudgetExceeded() : EvaluationBudgetExceeded("global training-loss allowance exhausted") {}
};
// Shared only for admission. A permit is charged as measure() begins, not when
// a candidate is queued. No reservation survives without a corresponding attempt.
class TrainingLossAdmission {
public:
    TrainingLossAdmission(std::uint64_t maximum,std::uint64_t used):maximum_(maximum),used_(used){if(used>maximum)throw std::invalid_argument("invalid loss admission state");}
    void start(const ExecutionContext* execution) {
        std::lock_guard lock(mutex_);
        if(execution && execution->stopRequested())throw EvaluationCancelled();
        if(used_==maximum_)throw TrainingLossBudgetExceeded();
        ++used_;
    }
private:
public:
    std::uint64_t remaining() const {std::lock_guard lock(mutex_);return maximum_-used_;}
private:
    mutable std::mutex mutex_;
    const std::uint64_t maximum_;
    std::uint64_t used_;
};
struct ConstantFitConfig;
struct ConstantFitResult;
class TrainingObjective;
inline ConstantFitResult fitSelectedConstants(const Expression&, TrainingObjective&,
                     std::span<const std::size_t>, const ConstantFitConfig&);
inline ConstantFitResult fitConstants(const Expression&, TrainingObjective&, const ConstantFitConfig&);
// Run-owned accounting and an optional fault/optimizer integration seam. Charge before compute,
// including invalid results and exceptions. Reporting calls to problem.measure are separate.
class TrainingObjective {
public:
    explicit TrainingObjective(std::shared_ptr<const RegressionProblem> problem,
                              std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max(),
                              std::shared_ptr<RefinementDiagnostics> diagnostics = {})
        : problem_(std::move(problem)), maximum_(maximum), diagnostics_(std::move(diagnostics)) {
        if (!problem_) throw std::invalid_argument("training objective requires a problem");
    }
    TrainingObjective(const TrainingObjective&) = delete;
    TrainingObjective& operator=(const TrainingObjective&) = delete;
    TrainingObjective(TrainingObjective&&) = delete;
    TrainingObjective& operator=(TrainingObjective&&) = delete;
    virtual ~TrainingObjective() = default;
    virtual bool parallelEvaluationSafe() const noexcept {
        return typeid(*this)==typeid(TrainingObjective) && !diagnostics_ && problem_->parallelEvaluationSafe();
    }
    std::shared_ptr<TrainingObjective> forkParallel(ExecutionContext& execution) {
        if(!parallelEvaluationSafe())throw std::invalid_argument("objective requires an independent parallel factory");
        auto local=cloneForParallel();
        if(!local || local.get()==this || local.use_count()!=1 || local->problem_!=problem_ || local->maximum_!=maximum_ || local->work_!=TrainingWork{} || local->diagnostics_)
            throw std::invalid_argument("invalid parallel objective factory result");
        if(!admission_)admission_=std::make_shared<TrainingLossAdmission>(maximum_,work_.lossAttempts);
        local->admission_=admission_;local->execution_=&execution;
        return local;
    }
    void mergeParallel(const TrainingObjective& local) {
        bool overflow=false;
#define GENETIK_ADD_WORK(field) mergeTrainingCounter(work_.field,local.work_.field,overflow);
        GENETIK_ADD_WORK(lossAttempts) GENETIK_ADD_WORK(rowAttempts) GENETIK_ADD_WORK(fittingInvocations)
        GENETIK_ADD_WORK(fittingLossAttempts) GENETIK_ADD_WORK(proposals) GENETIK_ADD_WORK(accepted)
        GENETIK_ADD_WORK(invalid) GENETIK_ADD_WORK(infeasible) GENETIK_ADD_WORK(duplicates)
        GENETIK_ADD_WORK(invalidLosses) GENETIK_ADD_WORK(ordinaryLossAttempts)
        GENETIK_ADD_WORK(fittingBaselineAttempts) GENETIK_ADD_WORK(fittingProbeAttempts) GENETIK_ADD_WORK(fittingImproved)
#undef GENETIK_ADD_WORK
        mergeTrainingCounter(supplemental_.mathematicalOperations,local.supplemental_.mathematicalOperations,overflow);
        mergeTrainingCounter(supplemental_.fieldEvaluations,local.supplemental_.fieldEvaluations,overflow);
        mergeWorker(local);
        if(overflow)throw std::overflow_error("parallel training accounting exceeded uint64 range");
        validateMergedObjectiveWork();
    }
    RegressionMetrics measure(const Expression& expression, bool fitting = false) {
        return measure(expression,fitting ? TrainingLossKind::FittingBaseline:TrainingLossKind::Ordinary);
    }
    RegressionMetrics measure(const Expression& expression,TrainingLossKind kind) {
        if(kind!=TrainingLossKind::Ordinary && kind!=TrainingLossKind::FittingBaseline && kind!=TrainingLossKind::FittingProbe)
            throw std::invalid_argument("unknown training loss kind");
        if(admission_)admission_->start(execution_);
        else if (work_.lossAttempts >= maximum_) throw TrainingLossBudgetExceeded();
        countTrainingWork(work_.lossAttempts);
        if(kind==TrainingLossKind::Ordinary)countTrainingWork(work_.ordinaryLossAttempts);
        else {
            countTrainingWork(work_.fittingLossAttempts);
            countTrainingWork(kind==TrainingLossKind::FittingBaseline ? work_.fittingBaselineAttempts:work_.fittingProbeAttempts);
        }
        genetik_detail::ScopedTiming timer(diagnostics_ ? &diagnostics_->datasetSeconds:nullptr,
            diagnostics_ && kind!=TrainingLossKind::Ordinary ? &diagnostics_->fittingDatasetSeconds:nullptr);
        try {
            auto result = compute(expression);
            if (result.loss.state() == FitnessState::Invalid) countTrainingWork(work_.invalidLosses);
            return result;
        }
        catch (...) { std::throw_with_nested(std::runtime_error("training loss attempt " + std::to_string(work_.lossAttempts) + " failed")); }
    }
    const std::shared_ptr<const RegressionProblem>& problem() const noexcept { return problem_; }
    std::uint64_t maximum() const noexcept { return maximum_; }
    std::uint64_t remaining() const { return admission_ ? admission_->remaining():maximum_ - work_.lossAttempts; }
    const std::shared_ptr<RefinementDiagnostics>& diagnostics() const noexcept {return diagnostics_;}
    const TrainingWork& work() const noexcept { return work_; }
    const RegressionSupplementalWork& supplementalWork()const noexcept{return supplemental_;}
    void recordFittingInvocation(){countTrainingWork(work_.fittingInvocations);}
    void addSupplementalWork(RegressionSupplementalWork work) {
        if(work.mathematicalOperations>UINT64_MAX-supplemental_.mathematicalOperations ||
           work.fieldEvaluations>UINT64_MAX-supplemental_.fieldEvaluations)
            throw std::overflow_error("supplemental training work overflow");
        supplemental_.mathematicalOperations+=work.mathematicalOperations;
        supplemental_.fieldEvaluations+=work.fieldEvaluations;
    }
    void restoreSupplementalWork(RegressionSupplementalWork work){
        if(supplemental_!=RegressionSupplementalWork{}||admission_||execution_)throw std::invalid_argument("fresh supplemental accounting required");
        supplemental_=work;
    }
    // Fresh, idle objective only; context identity is verified by the checkpoint adapter.
    void restoreWork(const TrainingWork& w) {
        if(work_!=TrainingWork{} || admission_ || execution_ || diagnostics_ || w.lossAttempts>maximum_ ||
           w.ordinaryLossAttempts>w.lossAttempts || w.fittingBaselineAttempts>w.lossAttempts-w.ordinaryLossAttempts ||
           w.fittingProbeAttempts!=w.lossAttempts-w.ordinaryLossAttempts-w.fittingBaselineAttempts ||
           w.fittingLossAttempts!=w.lossAttempts-w.ordinaryLossAttempts || w.invalidLosses>w.lossAttempts ||
           w.accepted>w.proposals || w.fittingImproved>w.fittingInvocations)
            throw std::invalid_argument("invalid training checkpoint counters/fresh objective");
        work_=w;
    }

protected:
    virtual std::shared_ptr<TrainingObjective> cloneForParallel() const {
        if(typeid(*this)!=typeid(TrainingObjective))throw std::invalid_argument("derived objective must override cloneForParallel");
        return std::make_shared<TrainingObjective>(problem_,maximum_);
    }
    virtual void mergeWorker(const TrainingObjective&) noexcept {}
    virtual void validateMergedObjectiveWork() {}
    // Narrow trusted compute seam: count observations without exposing fitness/genome state.
    std::uint64_t& rowAttemptsCounter() noexcept {return work_.rowAttempts;}
    const ExecutionContext* executionContext() const noexcept {return execution_;}
    virtual RegressionMetrics compute(const Expression& expression) {
        return problem_->measureWithWork(expression, &work_.rowAttempts,supplemental_);
    }
private:
    friend ConstantFitResult fitSelectedConstants(const Expression&,TrainingObjective&,std::span<const std::size_t>,const ConstantFitConfig&);
    friend ConstantFitResult fitConstants(const Expression&, TrainingObjective&, const ConstantFitConfig&);
    std::shared_ptr<TrainingLossAdmission> admission_;
    const ExecutionContext* execution_=nullptr; // worker-local borrow, never installed in committed candidates
    TrainingWork work_;
    RegressionSupplementalWork supplemental_;
    std::shared_ptr<const RegressionProblem> problem_;
    std::uint64_t maximum_;
    std::shared_ptr<RefinementDiagnostics> diagnostics_;
};
enum class ConstantFitMethod { Coordinate, LevenbergMarquardt };
struct ConstantFitConfig {
    double lower = -8, upper = 8, initialStep = 1, minimumStep = 1e-8;
    std::uint64_t maxLossAttempts = 256, maxSweeps = 128, maxProposals = 8192;
    std::optional<double> lossThreshold;
    ConstantFitMethod method=ConstantFitMethod::Coordinate;
    LMConfig levenbergMarquardt{};
};
inline void validateConstantFitConfigForLimit(const ConstantFitConfig& c, double maximumConstant) {
    if (!std::isfinite(c.lower) || !std::isfinite(c.upper) || c.lower > c.upper ||
        std::abs(c.lower) > maximumConstant || std::abs(c.upper) > maximumConstant ||
        !std::isfinite(c.initialStep) || !std::isfinite(c.minimumStep) || c.minimumStep <= 0 ||
        c.initialStep < c.minimumStep || c.maxLossAttempts == 0 || c.maxSweeps == 0 || c.maxProposals == 0 ||
        (c.lossThreshold && (!std::isfinite(*c.lossThreshold) || *c.lossThreshold < 0)) ||
        (c.method!=ConstantFitMethod::Coordinate&&c.method!=ConstantFitMethod::LevenbergMarquardt))
        throw std::invalid_argument("invalid constant fit configuration");
}
inline void validateConstantFitConfig(const ConstantFitConfig& c,const RegressionProblem& problem) {
    validateConstantFitConfigForLimit(c,problem.limits().maxAbsConstant);
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
    LMWork optimizerWork{};
};
// Shared deterministic reference algorithm. Representation adapters enumerate parameters,
// read one coefficient and construct one checked replacement. No RNG or publication here.
namespace genetik_detail {
template<class Result,class ExpressionValue,class Objective,class Parameters,class Coefficient,class Replace>
Result fitCoordinates(ExpressionValue expression,Objective& objective,const ConstantFitConfig& config,
                      const Parameters& parameters,TrainingWork& work,Coefficient coefficient,Replace replace) {
    countTrainingWork(work.fittingInvocations);
    Result result{std::move(expression), {}, {}};
    const auto begin = work.lossAttempts;
    auto finish = [&](ConstantFitStop stop) {
        result.stop = stop; result.lossAttempts = work.lossAttempts - begin;
        return std::move(result);
    };
    result.loss = result.startingLoss = objective.measure(result.expression, TrainingLossKind::FittingBaseline).loss;
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
            const double origin = coefficient(result.expression,parameter);
            for (double direction : {1., -1.}) {
                if (result.proposals >= config.maxProposals) return finish(ConstantFitStop::ProposalLimit);
                ++result.proposals; countTrainingWork(work.proposals);
                volatile double proposed = origin + direction * step;
                if (!std::isfinite(proposed) || proposed < config.lower || proposed > config.upper ||
                    std::abs(proposed) > result.expression.limits().maxAbsConstant) {
                    countTrainingWork(work.infeasible); continue;
                }
                if (proposed == origin) { countTrainingWork(work.duplicates); continue; }
                auto trial = replace(result.expression,parameter,proposed);
                if (!visited.insert(trial.toString()).second) { countTrainingWork(work.duplicates); continue; }
                if (work.lossAttempts - begin >= config.maxLossAttempts) return finish(ConstantFitStop::EvaluationLimit);
                const auto score = objective.measure(trial, TrainingLossKind::FittingProbe).loss;
                if (score.state() == FitnessState::Invalid) { countTrainingWork(work.invalid); continue; }
                if (score.score() < result.loss.score()) {
                    result.expression.swap(trial); result.loss = score;
                    if(!result.improved)countTrainingWork(work.fittingImproved);
                    result.improved = sweepImproved = true; countTrainingWork(work.accepted);
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
}

inline ConstantFitResult fitSelectedConstantsLevenbergMarquardt(
    const Expression& input,TrainingObjective& objective,
    std::span<const std::size_t> selected,const ConstantFitConfig& config) {
    validateConstantFitConfig(config,*objective.problem());
    (void)objective.problem()->checkedExpression(input);
    const auto& rows=objective.problem()->trainingRows();
    if(rows.size()!=objective.problem()->sampleCount())
        throw std::invalid_argument("Levenberg-Marquardt requires native scalar training rows");
    if(selected.size()>config.levenbergMarquardt.maxParameters)
        throw std::length_error("LM parameter limit");
    const auto original=input.nodes();std::vector<double> initial;initial.reserve(selected.size());
    for(std::size_t i=0;i<selected.size();++i){const auto n=selected[i];if(n>=original.size()||original[n].kind()!=ExpressionKind::Constant||(i&&n<=selected[i-1]))throw std::invalid_argument("LM constant indices must be increasing, distinct constants");const double v=original[n].constantValue();if(v<config.lower||v>config.upper)throw std::invalid_argument("LM starting constant outside fitting bounds");initial.push_back(v);}
    ConstantFitResult result{input,{},{}};
    objective.recordFittingInvocation();
    if(selected.empty()){auto baseline=objective.measure(input,TrainingLossKind::FittingBaseline);result.loss=result.startingLoss=baseline.loss;result.stop=baseline.loss.state()==FitnessState::Invalid?ConstantFitStop::InvalidStart:ConstantFitStop::NoConstants;result.lossAttempts=1;return result;}
    if(rows.empty())throw std::invalid_argument("LM requires nonempty native observations");
    std::vector<ExpressionNode> nodes;nodes.reserve(original.size());for(std::size_t node=0;node<original.size();++node){auto found=std::lower_bound(selected.begin(),selected.end(),node);if(found!=selected.end()&&*found==node)nodes.push_back(ExpressionNode::variable(input.variableCount()+static_cast<std::size_t>(found-selected.begin())));else nodes.push_back(original[node]);}
    Expression parameterExpression(nodes,input.variableCount()+selected.size(),input.limits());
    std::vector<LMExpressionObservation> observations;observations.reserve(rows.size());for(const auto& r:rows){auto x=r.inputs;x.resize(input.variableCount()+selected.size(),0);observations.push_back({std::move(x),r.target});}
    std::vector<std::size_t> bindings(selected.size());std::vector<double> lower(selected.size(),config.lower),upper(selected.size(),config.upper);for(std::size_t i=0;i<selected.size();++i)bindings[i]=input.variableCount()+i;
    auto raw=expressionLeastSquares(parameterExpression,std::move(observations),bindings);
    Fitness starting=Fitness::invalid(),last=Fitness::invalid();std::vector<double> lastTheta;std::uint64_t calls=0;
    LMResidualFunction measured=[&](std::span<const double> theta,bool jac,std::size_t allowance){Expression candidate=input;for(std::size_t i=0;i<selected.size();++i)candidate=candidate.withConstant(selected[i],theta[i]);const auto before=objective.supplementalWork();auto score=objective.measure(candidate,calls?TrainingLossKind::FittingProbe:TrainingLossKind::FittingBaseline);const auto after=objective.supplementalWork();if(calls==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("LM objective call count overflow");++calls;if(score.loss.state()!=FitnessState::Valid)return LMResidualEvaluation{};if(calls==1)starting=score.loss;auto eval=raw(theta,jac,allowance);if(rows.size()>std::numeric_limits<std::size_t>::max()-eval.forwardEvaluations||rows.size()>std::numeric_limits<std::size_t>::max()-eval.rowsProcessed)throw std::overflow_error("LM row accounting overflow");eval.forwardEvaluations+=rows.size();eval.rowsProcessed+=rows.size();eval.mathematicalOperations+=after.mathematicalOperations-before.mathematicalOperations;objective.addSupplementalWork({eval.mathematicalOperations-(after.mathematicalOperations-before.mathematicalOperations),eval.rowsProcessed-rows.size()});if(eval.valid){last=score.loss;lastTheta.assign(theta.begin(),theta.end());}return eval;};
    auto lm=config.levenbergMarquardt;lm.maxObjectiveEvaluations=std::min<std::uint64_t>(lm.maxObjectiveEvaluations,std::min(config.maxLossAttempts,objective.remaining()));
    if(!lm.maxObjectiveEvaluations)throw TrainingLossBudgetExceeded();
    const auto fit=levenbergMarquardt(initial,lower,upper,measured,lm);
    result.expression=input;for(std::size_t i=0;i<selected.size();++i)result.expression=result.expression.withConstant(selected[i],fit.parameters[i]);
    result.startingLoss=starting;result.loss=lastTheta==fit.parameters?last:starting;result.improved=result.loss.state()==FitnessState::Valid&&starting.state()==FitnessState::Valid&&result.loss.score()<starting.score();result.lossAttempts=calls;result.optimizerWork=fit.work;
    switch(fit.stop){case LMStop::LossTolerance:result.stop=ConstantFitStop::LossThreshold;break;case LMStop::GradientTolerance:case LMStop::StepTolerance:result.stop=ConstantFitStop::MinimumStep;break;case LMStop::EvaluationLimit:case LMStop::DerivativeLimit:case LMStop::WorkspaceLimit:result.stop=ConstantFitStop::EvaluationLimit;break;case LMStop::IterationLimit:result.stop=ConstantFitStop::SweepLimit;break;default:result.stop=fit.sumSquaredResiduals?ConstantFitStop::MinimumStep:ConstantFitStop::InvalidStart;break;}
    return result;
}

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
    if(config.method==ConstantFitMethod::LevenbergMarquardt)return fitSelectedConstantsLevenbergMarquardt(expression,objective,parameters,config);
    return genetik_detail::fitCoordinates<ConstantFitResult>(std::move(expression),objective,config,parameters,objective.work_,
        [](const Expression& e,std::size_t parameter){return e.nodes()[parameter].constantValue();},
        [](const Expression& e,std::size_t parameter,double value){return e.withConstant(parameter,value);});
}

// An explicit ordered subset. No frozen constant is bounds-checked as a fitted
// parameter; all expression constants still meet the expression's own limits.
inline ConstantFitResult fitSelectedConstants(const Expression& input,TrainingObjective& objective,
        std::span<const std::size_t> selected,const ConstantFitConfig& config={}) {
    validateConstantFitConfig(config,*objective.problem());
    (void)objective.problem()->checkedExpression(input);
    auto expression=input;const auto nodes=expression.nodes();
    if(selected.size()>nodes.size())throw std::invalid_argument("too many selected constants");
    std::vector<std::size_t> parameters;parameters.reserve(selected.size());
    for(auto i:selected) {
        if(i>=nodes.size() || nodes[i].kind()!=ExpressionKind::Constant ||
           (!parameters.empty() && i<=parameters.back()))
            throw std::invalid_argument("constant selection must be increasing distinct constant indices");
        const auto value=nodes[i].constantValue();
        if(value<config.lower || value>config.upper)throw std::invalid_argument("selected constant outside fitting bounds");
        parameters.push_back(i);
    }
    if(config.method==ConstantFitMethod::LevenbergMarquardt)return fitSelectedConstantsLevenbergMarquardt(expression,objective,parameters,config);
    return genetik_detail::fitCoordinates<ConstantFitResult>(std::move(expression),objective,config,parameters,objective.work_,
        [](const Expression& e,std::size_t i){return e.nodes()[i].constantValue();},
        [](const Expression& e,std::size_t i,double v){return e.withConstant(i,v);});
}
class ConstantParameterSelection {
public:
    virtual ~ConstantParameterSelection()=default;
    virtual std::vector<std::size_t> parameters(const Expression&) const=0;
    virtual bool parallelEvaluationSafe() const noexcept {return false;}
};

// Shared only for run accounting, never for mutable genomes. No loss cache is reused, including
// elites and unchanged clones; baseline and accepted probes already certify the installed value.
class ConstantRefinement final : public RegressionPreparation {
public:
    ConstantRefinement(std::shared_ptr<TrainingObjective> objective, std::optional<ConstantFitConfig> config = {},
                       std::uint64_t fitEveryGenerations=1, std::shared_ptr<const ConstantParameterSelection> selection={})
        : selection_(std::move(selection)), objective_(std::move(objective)), config_(config), interval_(fitEveryGenerations) {
        if (!objective_ || interval_==0) throw std::invalid_argument("refinement requires objective and positive cadence");
        if (config_) validateConstantFitConfig(*config_, *objective_->problem());
        if(auto d=objective_->diagnostics()) {
            if(d->bound)throw std::invalid_argument("refinement diagnostics require a fresh run");
            d->bound=true;
        }
    }
    bool parallelEvaluationSafe() const noexcept override {return objective_->parallelEvaluationSafe() && !objective_->diagnostics() && (!selection_ || selection_->parallelEvaluationSafe());}
    std::shared_ptr<RegressionPreparation> forkParallel(ExecutionContext& execution) override {
        auto child=std::make_shared<ConstantRefinement>(objective_->forkParallel(execution),config_,interval_,selection_);
        child->initializing=initializing;
        return child;
    }
    void mergeParallel(const RegressionPreparation& base) override {
        const auto& child=static_cast<const ConstantRefinement&>(base);
        std::exception_ptr failure;
        try{objective_->mergeParallel(*child.objective_);}catch(...){failure=std::current_exception();}
        bool overflow=false;
        const auto& b=child.statistics_;
        for(std::size_t i=0;i<statistics_.origins.size();++i){
            auto& a=statistics_.origins[i];const auto& c=b.origins[i];
            mergeTrainingCounter(a.requests,c.requests,overflow);mergeTrainingCounter(a.fits,c.fits,overflow);mergeTrainingCounter(a.skipped,c.skipped,overflow);mergeTrainingCounter(a.ordinary,c.ordinary,overflow);
            mergeTrainingCounter(a.baselines,c.baselines,overflow);mergeTrainingCounter(a.probes,c.probes,overflow);mergeTrainingCounter(a.rows,c.rows,overflow);mergeTrainingCounter(a.improved,c.improved,overflow);
        }
        mergeTrainingCounter(statistics_.skippedBySchedule,b.skippedBySchedule,overflow);mergeTrainingCounter(statistics_.completedImproved,b.completedImproved,overflow);
        mergeTrainingCounter(statistics_.completedUnchanged,b.completedUnchanged,overflow);mergeTrainingCounter(statistics_.noConstants,b.noConstants,overflow);
        mergeTrainingCounter(statistics_.invalidStarts,b.invalidStarts,overflow);mergeTrainingCounter(statistics_.abortedFits,b.abortedFits,overflow);
        for(std::size_t i=0;i<b.terminations.size();++i)mergeTrainingCounter(statistics_.terminations[i],b.terminations[i],overflow);
        try{mergeLMWork(optimizerWork_,child.optimizerWork_);}catch(...){failure=std::current_exception();}
        if(child.rawInitialBest && (!rawInitialBest || *child.rawInitialBest<*rawInitialBest))rawInitialBest=child.rawInitialBest;
        if(failure)std::rethrow_exception(failure);
        if(overflow)throw std::overflow_error("parallel preparation accounting exceeded uint64 range");
    }
    PreparedExpression prepare(const Expression& expression,const RegressionProblem& problem) override {
        return prepare(expression,problem,{});
    }
    PreparedExpression prepare(const Expression& expression,const RegressionProblem& problem,PreparationContext context) override {
        if (&problem != objective_->problem().get()) throw std::invalid_argument("refinement problem identity differs");
        const auto diagnostic=objective_->diagnostics();
        genetik_detail::ScopedTiming preparationTimer(diagnostic ? &diagnostic->preparationSeconds:nullptr);
        auto& stage=statistics_.origins.at(static_cast<std::size_t>(context.generation==0 ? PreparationOrigin::Initialization:context.origin));
        ++stage.requests;
        const auto before=objective_->work();
        const auto recordWork=[&] {
            const auto& after=objective_->work();
            stage.ordinary+=after.ordinaryLossAttempts-before.ordinaryLossAttempts;
            stage.baselines+=after.fittingBaselineAttempts-before.fittingBaselineAttempts;
            stage.probes+=after.fittingProbeAttempts-before.fittingProbeAttempts;
            stage.rows+=after.rowAttempts-before.rowAttempts;
            stage.improved+=after.fittingImproved-before.fittingImproved;
        };
        if(!config_ || context.generation%interval_!=0) {
            if(config_){++statistics_.skippedBySchedule;++stage.skipped;}
            Fitness loss;
            try{loss=objective_->measure(expression).loss;}catch(...){recordWork();throw;}
            recordWork();recordInitial(loss);return {expression,loss};
        }
        ++stage.fits;
        RefinementDiagnostics::Entry* entry=nullptr;bool repeat=false,priorOutput=false;
        if(diagnostic) {
            ++diagnostic->observed;entry=diagnostic->findOrInsert(expression);
            if(!entry)++diagnostic->untracked;
            else {
                repeat=entry->requests++!=0;priorOutput=entry->priorOutput;
                if(repeat)++diagnostic->repeated;
                if(priorOutput)++diagnostic->previousOutput;
            }
        }
        const auto recordDiagnostic=[&](bool completed,bool improved) {
            if(!diagnostic)return;
            const auto losses=objective_->work().lossAttempts-before.lossAttempts;
            if(repeat) {
                diagnostic->repeatLosses+=losses;
                if(completed && !improved)diagnostic->repeatNoChangeLosses+=losses;
                if(improved)++diagnostic->repeatImproved;
            }
            if(priorOutput) {
                diagnostic->previousOutputLosses+=losses;
                if(improved)++diagnostic->previousOutputImproved;
                else if(completed)++diagnostic->previousOutputUnchanged;
            }
        };
        auto result=[&] {
            genetik_detail::ScopedTiming fitTimer(diagnostic ? &diagnostic->fittingSeconds:nullptr);
            try{
                if(selection_)return fitSelectedConstants(expression,*objective_,selection_->parameters(expression),*config_);
                return fitConstants(expression,*objective_,*config_);
            }
            catch(...) {
                recordWork();++statistics_.abortedFits;
                recordDiagnostic(false,objective_->work().fittingImproved!=before.fittingImproved);throw;
            }
        }();
        if(config_->method==ConstantFitMethod::LevenbergMarquardt){mergeLMWork(optimizerWork_,result.optimizerWork);if(diagnostic)diagnostic->recordOptimizerWork(result.optimizerWork);}
        recordWork();recordDiagnostic(true,result.improved);
        ++statistics_.terminations[static_cast<std::size_t>(result.stop)];
        if(result.improved)++statistics_.completedImproved;
        else if(result.stop==ConstantFitStop::NoConstants)++statistics_.noConstants;
        else if(result.stop==ConstantFitStop::InvalidStart)++statistics_.invalidStarts;
        else ++statistics_.completedUnchanged;
        if(diagnostic)if(auto output=diagnostic->findOrInsert(result.expression))output->priorOutput=true;
        recordInitial(result.startingLoss);
        return {std::move(result.expression),result.loss};
    }
    const PreparationStatistics& statistics() const noexcept {return statistics_;}
    const LMWork& optimizerWork()const noexcept{return optimizerWork_;}
    void restoreOptimizerWork(const LMWork& work){if(optimizerWork_!=LMWork{}||!initializing)throw std::invalid_argument("optimizer work restore requires fresh refinement");optimizerWork_=work;}
    void restoreStatistics(const PreparationStatistics& state,std::optional<double> initial) {
        if(statistics_!=PreparationStatistics{} || !initializing || (initial && !std::isfinite(*initial)))
            throw std::invalid_argument("refinement restore requires fresh state");
        statistics_=state;rawInitialBest=initial;initializing=false;
    }

    bool initializing = true;
    std::optional<double> rawInitialBest;
private:
    void recordInitial(Fitness loss) {
        if (initializing && loss.state() == FitnessState::Valid && (!rawInitialBest || loss.score() < *rawInitialBest))
            rawInitialBest = loss.score();
    }
    std::shared_ptr<const ConstantParameterSelection> selection_;
    std::shared_ptr<TrainingObjective> objective_;
    const std::optional<ConstantFitConfig> config_;
    const std::uint64_t interval_;
    PreparationStatistics statistics_;
    LMWork optimizerWork_{};
};
