#pragma once
// Prime-demo policy only: shared GA/preparation/evaluator APIs remain unchanged.
#include "PrimeRegression.hpp"
#include "PrimeExactDisplay.hpp"
#include "PrimeExactCheckpoint.hpp"
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
    Handler previous_,previousTerm_;
public:
    ExactSignalGuard():previous_(SIG_DFL),previousTerm_(SIG_DFL) {
        exactInterrupt.store(false,std::memory_order_relaxed);
        previous_=std::signal(SIGINT,exactSignal);
        if(previous_==SIG_ERR)throw std::runtime_error("cannot install SIGINT handler");
        previousTerm_=std::signal(SIGTERM,exactSignal);
        if(previousTerm_==SIG_ERR){(void)std::signal(SIGINT,previous_);throw std::runtime_error("cannot install SIGTERM handler");}
    }
    ~ExactSignalGuard(){(void)std::signal(SIGINT,previous_);(void)std::signal(SIGTERM,previousTerm_);}
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
struct ExactSearchResult {
    SearchStop stop=SearchStop::GenerationLimit;
    std::optional<ExpressionCandidate> best;
    std::vector<ExpressionCandidate> population;
    std::uint64_t attempts=0,published=0,candidateRequests=0,unpublishedLosses=0;
    std::optional<std::uint64_t> bestGeneration;
    TrainingWork training;
    VariationStatistics variation;
    ExactReportingWork reporting;
    PreparationStatistics preparation;
    std::vector<ExactImprovement> history;
    std::filesystem::path checkpoint;
};
inline ExactSearchResult runExactSearch(std::shared_ptr<const ExactPrefixProblem> problem,
        const SymbolicRegressionConfig& cfg,RandomContext& random,std::ostream& out,std::size_t top,
        ExactClock::duration interval,bool report=true,
        const std::function<ExactClock::time_point()>& now=[] {return ExactClock::now();},
        const ExactRunOptions& io={}) {
    if(!problem || !top || random.evaluationAttempts()!=0 || cfg.population==0 ||
       cfg.population>std::numeric_limits<unsigned int>::max() || cfg.elites>=cfg.population || cfg.fitEveryGenerations==0 ||
       !std::isfinite(cfg.crossoverRate) || !std::isfinite(cfg.mutationRate) || cfg.crossoverRate<0 || cfg.crossoverRate>1 ||
       cfg.mutationRate<0 || cfg.mutationRate>1 || cfg.diagnostics)
        throw std::invalid_argument("invalid exact-prefix run configuration");
    if(!io.resume && !io.checkpointDirectory.empty() && std::filesystem::exists(io.checkpointDirectory/"latest.gtkc"))throw CheckpointError("checkpoint directory already contains a run; use --resume or a fresh directory");
    const auto start=now();ExactReportSchedule schedule(start,interval),checkpointSchedule(start,io.checkpointInterval);
    if(!io.checkpointKeep || io.checkpointKeep>10000)throw std::invalid_argument("checkpoint retention must be1..10000");
    auto objective=std::make_shared<TrainingObjective>(problem,cfg.maxTrainingLossAttempts);
    auto preparation=std::make_shared<ConstantRefinement>(objective,cfg.fitting,cfg.fitEveryGenerations);
    ExpressionVariation variation(problem,cfg.grammar,preparation);
    using P=Population<ExpressionCandidate,RegressionOrdering>;
    std::unique_ptr<P> population;
    GeneticAlgorithm<P,ExpressionCandidate,ExpressionVariation> ga(cfg.crossoverRate,cfg.mutationRate,random,std::move(variation),cfg.elites);
    ExactSearchResult result;std::uint64_t committedLosses=0;double priorElapsed=0,lastCheckpointElapsed=0;
    ExactDisplayState display;std::optional<std::uint64_t> checkpointGeneration;
    const auto identity=(!io.checkpointDirectory.empty()||io.resume)?exactContextIdentity(problem->referenceValues(),problem->limits(),cfg)+";seed="+std::to_string(random.seed())+";threads="+std::to_string(random.execution().requested()):std::string{};
    if(io.resume){
        const auto saved=decodeExactRun(*io.resume);
        if(io.resume->identity!=identity)throw CheckpointError("checkpoint problem/dataset/grammar/configuration identity differs");
        if(saved.seed!=random.seed()||saved.threads!=random.execution().requested())throw CheckpointError("checkpoint seed/worker setting differs");
        if(cfg.maxTrainingLossAttempts<saved.training.lossAttempts || cfg.maxEvaluations<saved.random.candidateAttempts)throw CheckpointError("total resumed budget is below consumed work");
        CheckpointLimits loadLimits;loadLimits.maxCandidates=cfg.population;CheckpointReader reader(saved.populationBytes,loadLimits);auto state=readCommittedPopulation<ExpressionCandidate>(reader,ScalarCheckpointContext{problem,preparation});reader.finish();
        if(!io.resume->generation || *io.resume->generation!=state.generation || state.members.size()!=cfg.population || state.generation!=saved.algorithm.published || state.direction!=ObjectiveDirection::Minimize || saved.variation.initializations!=cfg.population ||
           saved.random.candidateAttempts/cfg.population<static_cast<std::uint64_t>(saved.algorithm.published)+1 ||
           !saved.random.candidateAttempts || (saved.random.candidateAttempts-1)/cfg.population>saved.algorithm.attempts)
            throw CheckpointError("checkpoint generation/population/candidate counters differ");
        validateExactWork(saved,problem->sampleCount());
        for(const auto& candidate:state.members)if(candidate.fitnessState()==FitnessState::Valid)(void)exactPresentationFitness(candidate.fitness());
        (void)exactPresentationFitness(state.best.fitness());
        objective->restoreWork(saved.training);preparation->restoreStatistics(saved.preparation,saved.rawInitialBest);
        ga.variation().restoreStatistics(saved.variation);RunCheckpointAccess::restoreAlgorithm(ga,saved.algorithm);
        population=RunCheckpointAccess::restorePopulation<ExpressionCandidate,RegressionOrdering>(state,random);
        committedLosses=saved.committedLosses;priorElapsed=saved.elapsed;result.history=saved.history;result.reporting=saved.reporting;display.previousLoss=saved.previousDisplayedLoss;display.previousMatches=saved.previousDisplayedMatches;
        if(result.history.empty() || result.history.front().generation!=0 || result.history.back().generation!=state.bestGeneration || result.history.back().canonical!=state.best.text() || result.history.back().loss!=state.best.fitness())throw CheckpointError("checkpoint best/history differs");
        RunCheckpointAccess::restoreRandom(random,saved.random);
        if(io.pretty)out<<"Resuming checkpoint\nGeneration        "<<state.generation<<"\nPrevious attempts "<<saved.training.lossAttempts<<"\nSeed              "<<saved.seed<<"\nRNG state         restored\nProblem           verified\nCheckpoint        checksum OK\nContinuing with generation "<<state.generation+1<<"\n";
    }
    const auto elapsed=[&]{return priorElapsed+std::chrono::duration<double>(now()-start).count();};
    const auto recordImprovement=[&]{if(!population||!population->generation())return;const auto best=population->bestSeen();if(result.history.empty()||result.history.back().loss!=best->fitness()||result.history.back().canonical!=best->text())result.history.push_back({objective->work().lossAttempts,*population->generation(),best->fitness(),best->text()});};
    const auto save=[&](bool force){
        if(io.checkpointDirectory.empty()||!population||!population->generation()||!checkpointSchedule.due(now(),force))return;
        ExactSavedRun saved;saved.seed=random.seed();saved.threads=random.execution().requested();saved.allowance=cfg.maxTrainingLossAttempts;saved.fitEvery=cfg.fitEveryGenerations;saved.fitting=cfg.fitting.has_value();
        saved.random=RunCheckpointAccess::random(random);saved.algorithm=RunCheckpointAccess::algorithm(ga);saved.training=objective->work();saved.preparation=preparation->statistics();saved.variation=ga.variation().statistics();saved.rawInitialBest=preparation->rawInitialBest;
        saved.lastStop=force?searchStopName(result.stop):"running";saved.committedLosses=committedLosses;saved.elapsed=elapsed();saved.history=result.history;saved.reporting=result.reporting;saved.previousDisplayedLoss=display.previousLoss;saved.previousDisplayedMatches=display.previousMatches;
        CheckpointWriter writer;writeCommittedPopulation(writer,RunCheckpointAccess::population(*population));saved.populationBytes=writer.data();
        result.checkpoint=saveCheckpointSeries(io.checkpointDirectory,*population->generation(),encodeExactRun(saved,identity),io.checkpointKeep);
        checkpointGeneration=population->generation();lastCheckpointElapsed=saved.elapsed;
    };
    const auto snapshot=[&](std::string_view kind,std::string_view state) {
        const auto time=now();
        if(!report || !schedule.due(time,kind!="periodic"))return;
        const ExactProgress progress{population?population->generation():std::nullopt,ga.attempts(),ga.evolutions(),
            objective->work().lossAttempts,objective->remaining(),cfg.maxTrainingLossAttempts,random.evaluationAttempts(),
            objective->work().lossAttempts-committedLosses,cfg.population,random.seed(),random.execution().requested(),random.execution().effective(cfg.population),population?population->bestGeneration():std::nullopt};
        const std::vector<ExpressionCandidate> empty;
        exactHumanSnapshot(out,population && population->generation()?population->selectionView():empty,
                      problem->referenceValues(),progress,top,priorElapsed+std::chrono::duration<double>(time-start).count(),kind,state,result.reporting,display,
                      io.metrics,population?population->bestSeen():std::nullopt,result.checkpoint.string(),checkpointGeneration,
                      elapsed()-lastCheckpointElapsed,!io.pretty);
    };
    try {
        if(problem->cancelled())throw EvaluationCancelled();
        if(!population){
            if(cfg.maxEvaluations<cfg.population || objective->remaining()<cfg.population)throw TrainingLossBudgetExceeded();
            std::vector<ExpressionCandidate> initial;initial.reserve(cfg.population);
            for(std::size_t i=0;i<cfg.population;++i)initial.push_back(ga.variation().initialize(random.engine()));
            population=std::make_unique<P>(initial,random,ObjectiveDirection::Minimize);
            preparation->initializing=false;
            if(population->generation())committedLosses=objective->work().lossAttempts;
        }
        if(population->generation()){recordImprovement();save(false);snapshot(io.resume?"resumed":"initial","running");}
        for(;;) {
            const auto best=population->bestSeen();
            if(!best){result.stop=SearchStop::NoViableCandidates;break;}
            if(best->fitness()==0){result.stop=SearchStop::ExactTrainingFit;break;}
            if(problem->cancelled())throw EvaluationCancelled();
            if(ga.attempts()>=cfg.maxGenerationAttempts){result.stop=SearchStop::GenerationLimit;break;}
            if(cfg.maxEvaluations-random.evaluationAttempts()<cfg.population || objective->remaining()<cfg.population)
                throw TrainingLossBudgetExceeded();
            if(ga.evolve(*population)==GenerationOutcome::NoViableCandidates){result.stop=SearchStop::NoViableCandidates;break;}
            committedLosses=objective->work().lossAttempts;recordImprovement();save(false);
            if(population->bestSeen()->fitness()==0){result.stop=SearchStop::ExactTrainingFit;break;}
            snapshot("periodic","running");
        }
    } catch(...) {
        const auto failure=std::current_exception();
        if(isEvaluationBudget(failure))result.stop=SearchStop::EvaluationBudget;
        else if(isEvaluationCancelled(failure))result.stop=SearchStop::Cancelled;
        else throw;
    }
    try{save(true);}catch(...){out<<"CHECKPOINT FAILURE: final checkpoint could not be written; previous known-good checkpoint retained if replacement had not committed.\n";out.flush();throw;}
    snapshot(result.stop==SearchStop::ExactTrainingFit?"winning":"final",searchStopName(result.stop));
    try{save(true);}catch(...){out<<"CHECKPOINT FAILURE after final report: "<<result.checkpoint.string()<<" may still name the previous known-good checkpoint.\n";out.flush();throw;}
    result.best=population?population->bestSeen():std::nullopt;
    if(population && population->generation())result.population=population->selectionView();
    result.bestGeneration=population?population->bestGeneration():std::nullopt;
    result.attempts=ga.attempts();result.published=ga.evolutions();result.candidateRequests=random.evaluationAttempts();
    result.training=objective->work();result.variation=ga.variation().statistics();result.preparation=preparation->statistics();
    result.unpublishedLosses=result.training.lossAttempts-committedLosses;
    return result;
}
} // namespace prime_example
