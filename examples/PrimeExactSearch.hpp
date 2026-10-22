#pragma once
// Prime-demo policy only: shared GA/preparation/evaluator APIs remain unchanged.
#include "PrimeRegression.hpp"
#include <atomic>
#include <chrono>
#include <csignal>

namespace prime_example {
using ExactClock=std::chrono::steady_clock;
inline std::atomic<bool> exactInterrupt{false};
static_assert(std::atomic<bool>::is_always_lock_free);
inline void exactSignal(int) noexcept {exactInterrupt.store(true,std::memory_order_relaxed);}
class ExactSignalGuard {
    using Handler=void(*)(int);
    Handler previous_;
public:
    ExactSignalGuard():previous_(SIG_DFL) {
        exactInterrupt.store(false,std::memory_order_relaxed);
        previous_=std::signal(SIGINT,exactSignal);
        if(previous_==SIG_ERR)throw std::runtime_error("cannot install SIGINT handler");
    }
    ~ExactSignalGuard(){(void)std::signal(SIGINT,previous_);}
    ExactSignalGuard(const ExactSignalGuard&)=delete;
    ExactSignalGuard& operator=(const ExactSignalGuard&)=delete;
};
// At most1000 observations makes the bounded secondary sum unable to reverse
// integer mismatch ordering. The ordinary expression never sees these labels.
class ExactPrefixProblem : public RegressionProblem {
    const std::vector<std::uint32_t> reference_;
    const std::atomic<bool>* cancelled_;
public:
    ExactPrefixProblem(const std::vector<std::uint32_t>& reference,ExpressionLimits limits={31,6,8},
                       const std::atomic<bool>* cancelled=nullptr)
        :RegressionProblem(MeasurementSchema{1,limits}),reference_(reference),cancelled_(cancelled) {
        if(reference_.empty() || reference_.size()>1000)throw std::invalid_argument("exact prefix requires1..1000 targets");
        for(auto p:reference_)if(!p)throw std::invalid_argument("exact prefix targets must be positive integers");
    }
    bool parallelEvaluationSafe() const noexcept override {return typeid(*this)==typeid(ExactPrefixProblem);}
    std::size_t sampleCount() const noexcept override {return reference_.size();}
    const std::vector<std::uint32_t>& referenceValues() const noexcept {return reference_;}
    bool cancelled() const noexcept {return cancelled_ && cancelled_->load(std::memory_order_relaxed);}
    RegressionMetrics measure(const Expression& expression,std::uint64_t* rowAttempts=nullptr) const override {
        if(expression.variableCount()!=1)throw std::invalid_argument("exact prefix requires x0");
        genetik_detail::EvaluationWorkspace workspace;
        std::size_t mismatches=0,attempted=0;double secondary=0,maximum=0;
        for(std::size_t i=0;i<reference_.size();++i) {
            if(cancelled())throw EvaluationCancelled();
            ++attempted;if(rowAttempts)countTrainingWork(*rowAttempts);
            const auto value=workspace.evaluate(expression,std::array{input(i+1)});
            if(!value.valid())return {Fitness::invalid(),{},attempted};
            volatile double raw=10000.0*value.value();
            const auto integer=rounded(raw);
            if(!integer)return {Fitness::invalid(),{},attempted};
            volatile double residual=raw-static_cast<double>(reference_[i]);
            if(!std::isfinite(residual))return {Fitness::invalid(),{},attempted};
            const double error=std::abs(static_cast<double>(residual));maximum=std::max(maximum,error);
            if(*integer!=static_cast<std::int64_t>(reference_[i])) {
                ++mismatches;
                volatile double denominator=1.0+error;
                volatile double ratio=error/denominator;
                volatile double next=secondary+ratio;
                if(!std::isfinite(denominator) || !std::isfinite(ratio) || !std::isfinite(next))
                    return {Fitness::invalid(),{},attempted};
                secondary=next;
            }
        }
        volatile double fraction=secondary/1000.0;
        volatile double loss=static_cast<double>(mismatches)+fraction;
        if(!std::isfinite(loss))return {Fitness::invalid(),{},attempted};
        return {Fitness::valid(loss),maximum,attempted};
    }
};

// Caller supplies monotonic timestamps; catch up in one step, never a print burst.
// Deadlines stay aligned with start, including time spent formatting/reporting.
class ExactReportSchedule {
    ExactClock::duration interval_;
    ExactClock::time_point next_;
    bool initial_=true;
public:
    ExactReportSchedule(ExactClock::time_point start,ExactClock::duration interval):interval_(interval),next_(start) {
        if(interval<=ExactClock::duration::zero() || start>ExactClock::time_point::max()-interval)
            throw std::invalid_argument("unrepresentable reporting interval");
        next_+=interval;
    }
    bool due(ExactClock::time_point now,bool force=false) {
        const bool report=initial_ || force || now>=next_;initial_=false;
        if(now>=next_) {
            const auto advance=interval_-(now-next_)%interval_;
            next_=now>ExactClock::time_point::max()-advance?ExactClock::time_point::max():now+advance;
        }
        return report;
    }
    ExactClock::time_point nextDeadline() const noexcept {return next_;}
};
struct ExactRanking {
    std::vector<std::size_t> indices;
    std::size_t invalid=0,unevaluated=0,valid=0;
};
inline ExactRanking exactRanking(const std::vector<ExpressionCandidate>& members,std::size_t top) {
    if(!top)throw std::invalid_argument("top must be positive");
    ExactRanking ranked;
    for(std::size_t i=0;i<members.size();++i) {
        if(members[i].fitnessState()==FitnessState::Valid)ranked.indices.push_back(i);
        else if(members[i].fitnessState()==FitnessState::Invalid)++ranked.invalid;
        else ++ranked.unevaluated;
    }
    ranked.valid=ranked.indices.size();
    std::stable_sort(ranked.indices.begin(),ranked.indices.end(),[&](auto a,auto b){
        return RegressionOrdering{}.better(members[a],members[b],ObjectiveDirection::Minimize);
    });
    if(ranked.indices.size()>top)ranked.indices.resize(top);
    return ranked;
}
struct ExactReportingWork {std::uint64_t snapshots=0,predictionRows=0;};
struct ExactProgress {
    std::optional<std::uint64_t> generation;
    std::uint64_t attempts=0,published=0,losses=0,remaining=0,allowance=0,candidateRequests=0,unpublishedLosses=0;
    std::size_t population=0;
    std::uint32_t seed=0;
    unsigned requested=1,effective=1;
};
inline void exactSnapshot(std::ostream& out,const std::vector<ExpressionCandidate>& members,
        const std::vector<std::uint32_t>& ref,const ExactProgress& progress,std::size_t top,
        double elapsed,std::string_view kind,std::string_view state,ExactReportingWork& work) {
    const auto ranked=exactRanking(members,top);
    std::vector<Metrics> measurements;measurements.reserve(ranked.indices.size());
    // Prediction-only all-row diagnostics, never TrainingObjective::measure/fitting.
    for(auto slot:ranked.indices) {
        std::vector<Prediction> points;points.reserve(ref.size());
        for(std::size_t i=0;i<ref.size();++i) {
            countTrainingWork(work.predictionRows);
            points.push_back(prediction(i+1,ref[i],decode(members[slot].expression(),i+1)));
        }
        measurements.push_back(metrics(points));
    }
    countTrainingWork(work.snapshots);
    out<<"snapshot="<<kind<<" elapsed_seconds="<<elapsed<<" committed_generation=";
    if(progress.generation)out<<*progress.generation;else out<<"none";
    out<<" attempted_generations="<<progress.attempts<<" published_generations="<<progress.published
       <<" actual_loss_attempts="<<progress.losses<<" loss_budget="<<progress.allowance<<" remaining_allowance="<<progress.remaining
       <<" unpublished_loss_attempts="<<progress.unpublishedLosses<<" candidate_requests="<<progress.candidateRequests
       <<" threads_requested="<<progress.requested<<" threads_effective="<<progress.effective
       <<" seed="<<progress.seed<<" population="<<progress.population<<" committed_size="<<members.size()
       <<" valid="<<ranked.valid<<" invalid="<<ranked.invalid<<" unevaluated="<<ranked.unevaluated
       <<" displayed="<<ranked.indices.size()<<" reporting_prediction_rows="<<work.predictionRows
       <<" reporting_loss_attempts=0 stopping_state="<<state<<'\n';
    std::set<std::string> seen;
    for(std::size_t i=0;i<ranked.indices.size();++i) {
        const auto& candidate=members[ranked.indices[i]];const auto& m=measurements[i];
        std::size_t constants=0;for(auto node:candidate.expression().nodes())if(node.kind()==ExpressionKind::Constant)++constants;
        out<<"rank="<<i+1<<" slot="<<ranked.indices[i]<<" exact_prefix_loss="<<candidate.fitness()
           <<" exact_matches="<<m.matches<<'/'<<m.count<<" mismatches="<<m.count-m.matches
           <<" raw_rmse=";if(m.rmse)out<<*m.rmse;else out<<"invalid";
        out<<" raw_mae=";if(m.mae)out<<*m.mae;else out<<"invalid";
        out<<" raw_max_abs=";if(m.maximum)out<<*m.maximum;else out<<"invalid";
        out<<" mean_relative=";if(m.meanRelative)out<<*m.meanRelative;else out<<"invalid";
        out<<" max_relative=";if(m.maxRelative)out<<*m.maxRelative;else out<<"invalid";
        out<<" duplicates="<<m.duplicates<<" strict_increase_violations="<<m.roundedViolations
           <<" raw_increase_violations="<<m.rawViolations<<" invalid_predictions="<<m.invalid
           <<" unrepresentable_predictions="<<m.unrepresentable<<" nodes="<<candidate.expression().nodeCount()
           <<" depth="<<candidate.expression().depth()<<" constant_count="<<constants
           <<" duplicate_formula="<<!seen.insert(candidate.text()).second<<'\n'
           <<"g(x0) = "<<candidate.text()<<'\n'
           <<"p_hat(n) = 10000 * ("<<candidate.text()<<") [x0 = double(n) / 1000.0]\n";
    }
    out.flush();
    if(!out)throw std::runtime_error("exact-prefix snapshot output failed after publication");
}
struct ExactSearchResult {
    SearchStop stop=SearchStop::GenerationLimit;
    std::optional<ExpressionCandidate> best;
    std::vector<ExpressionCandidate> population;
    std::uint64_t attempts=0,published=0,candidateRequests=0,unpublishedLosses=0;
    TrainingWork training;
    VariationStatistics variation;
    ExactReportingWork reporting;
};
inline ExactSearchResult runExactSearch(std::shared_ptr<const ExactPrefixProblem> problem,
        const SymbolicRegressionConfig& cfg,RandomContext& random,std::ostream& out,std::size_t top,
        ExactClock::duration interval,bool report=true,
        const std::function<ExactClock::time_point()>& now=[] {return ExactClock::now();}) {
    if(!problem || !top || random.evaluationAttempts()!=0 || cfg.population==0 ||
       cfg.population>std::numeric_limits<unsigned int>::max() || cfg.elites>=cfg.population || cfg.fitEveryGenerations==0 ||
       !std::isfinite(cfg.crossoverRate) || !std::isfinite(cfg.mutationRate) || cfg.crossoverRate<0 || cfg.crossoverRate>1 ||
       cfg.mutationRate<0 || cfg.mutationRate>1 || cfg.diagnostics)
        throw std::invalid_argument("invalid exact-prefix run configuration");
    const auto start=now();ExactReportSchedule schedule(start,interval);
    auto objective=std::make_shared<TrainingObjective>(problem,cfg.maxTrainingLossAttempts);
    auto preparation=std::make_shared<ConstantRefinement>(objective,cfg.fitting,cfg.fitEveryGenerations);
    ExpressionVariation variation(problem,cfg.grammar,preparation);
    using P=Population<ExpressionCandidate,RegressionOrdering>;
    std::unique_ptr<P> population;
    GeneticAlgorithm<P,ExpressionCandidate,ExpressionVariation> ga(cfg.crossoverRate,cfg.mutationRate,random,std::move(variation),cfg.elites);
    ExactSearchResult result;std::uint64_t committedLosses=0;
    const auto snapshot=[&](std::string_view kind,std::string_view state) {
        const auto time=now();
        if(!report || !schedule.due(time,kind!="periodic"))return;
        const ExactProgress progress{population?population->generation():std::nullopt,ga.attempts(),ga.evolutions(),
            objective->work().lossAttempts,objective->remaining(),cfg.maxTrainingLossAttempts,random.evaluationAttempts(),
            objective->work().lossAttempts-committedLosses,cfg.population,random.seed(),random.execution().requested(),random.execution().effective(cfg.population)};
        const std::vector<ExpressionCandidate> empty;
        exactSnapshot(out,population && population->generation()?population->selectionView():empty,
                      problem->referenceValues(),progress,top,std::chrono::duration<double>(time-start).count(),kind,state,result.reporting);
    };
    try {
        if(problem->cancelled())throw EvaluationCancelled();
        if(cfg.maxEvaluations<cfg.population || objective->remaining()<cfg.population)throw TrainingLossBudgetExceeded();
        std::vector<ExpressionCandidate> initial;initial.reserve(cfg.population);
        for(std::size_t i=0;i<cfg.population;++i)initial.push_back(ga.variation().initialize(random.engine()));
        population=std::make_unique<P>(initial,random,ObjectiveDirection::Minimize);
        preparation->initializing=false;
        if(population->generation()){committedLosses=objective->work().lossAttempts;snapshot("initial","running");}
        for(;;) {
            const auto best=population->bestSeen();
            if(!best){result.stop=SearchStop::NoViableCandidates;break;}
            if(best->fitness()==0){result.stop=SearchStop::ExactTrainingFit;break;}
            if(problem->cancelled())throw EvaluationCancelled();
            if(ga.attempts()>=cfg.maxGenerationAttempts){result.stop=SearchStop::GenerationLimit;break;}
            if(cfg.maxEvaluations-random.evaluationAttempts()<cfg.population || objective->remaining()<cfg.population)
                throw TrainingLossBudgetExceeded();
            if(ga.evolve(*population)==GenerationOutcome::NoViableCandidates){result.stop=SearchStop::NoViableCandidates;break;}
            committedLosses=objective->work().lossAttempts;
            if(population->bestSeen()->fitness()==0){result.stop=SearchStop::ExactTrainingFit;break;}
            snapshot("periodic","running");
        }
    } catch(...) {
        const auto failure=std::current_exception();
        if(isEvaluationBudget(failure))result.stop=SearchStop::EvaluationBudget;
        else if(isEvaluationCancelled(failure))result.stop=SearchStop::Cancelled;
        else throw;
    }
    snapshot(result.stop==SearchStop::ExactTrainingFit?"winning":"final",searchStopName(result.stop));
    result.best=population?population->bestSeen():std::nullopt;
    if(population && population->generation())result.population=population->selectionView();
    result.attempts=ga.attempts();result.published=ga.evolutions();result.candidateRequests=random.evaluationAttempts();
    result.training=objective->work();result.variation=ga.variation().statistics();
    result.unpublishedLosses=result.training.lossAttempts-committedLosses;
    return result;
}
} // namespace prime_example
