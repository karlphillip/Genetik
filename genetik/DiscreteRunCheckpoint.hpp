#pragma once
#include "DiscreteExpressionCheckpoint.hpp"
#include "DiscreteSymbolicRegression.hpp"

namespace genetik_discrete_detail {
inline void signedField(CheckpointWriter& w,std::int64_t n){w.text(std::to_string(n));}
inline std::string runIdentity(const DiscreteRegressionProblem& p,const DiscreteSearchConfig& c,const RandomContext& random){
    CheckpointWriter w(1024*1024);w.text("discrete-evolution/2;unique-binders;closed-crossover-alpha/1");w.integer(random.seed());w.integer(random.execution().requested());
    const auto l=p.limits();for(auto n:{l.maxNodes,l.maxDepth,l.maxVariables,l.maxConstants,l.maxStorageBytes,l.maxIterationDepth,l.maxBinders})w.integer(n);w.integer(l.maxConstantMagnitude);
    const auto e=p.evaluationLimits();for(auto n:{e.maxNodeVisits,e.maxOperations,e.maxWorkspaceBytes,e.maxIterationsPerOperation,e.maxAggregateIterations,e.maxIterationDepth})w.integer(n);
    const auto d=p.datasetLimits();for(auto n:{d.maxRows,d.maxInputValues,d.maxNodeVisitsPerLoss,d.maxOperationsPerLoss,d.maxIterationsPerLoss})w.integer(n);
    w.integer(p.dimensions());w.integer(p.sampleCount());for(const auto& row:p.rows()){for(auto n:row.inputs)signedField(w,n);signedField(w,row.target);}
    w.integer(c.population);w.integer(c.elites);w.integer(c.maxGenerations);w.integer(c.maxLossAttempts);w.integer(std::bit_cast<std::uint32_t>(c.crossoverRate));w.integer(std::bit_cast<std::uint32_t>(c.mutationRate));
    const auto& g=c.grammar;w.integer(g.operators.size());for(auto k:g.operators)w.integer(static_cast<unsigned>(k));w.integer(g.constants.size());for(auto n:g.constants)signedField(w,n);
    w.integer(g.booleanConstants.size());for(bool b:g.booleanConstants)w.integer(b);w.integer(g.allowedVariables.has_value());if(g.allowedVariables){w.integer(g.allowedVariables->size());for(auto n:*g.allowedVariables)w.integer(n);}
    w.integer(g.positiveConstantDivisorsOnly);w.integer(g.positiveDivisors.size());for(auto n:g.positiveDivisors)signedField(w,n);
    for(auto n:{g.initialMaxDepth,g.mutationMaxDepth,g.proposalLimit,g.maxIterationNesting})w.integer(n);
    w.integer(g.iterationBoundsAreTerminals);if(g.iteratorDiscovery||g.recordIterationDiagnostics){w.text("iterator-discovery/1");w.integer(g.iteratorDiscovery);w.integer(g.recordIterationDiagnostics);}if(!p.measurementIdentity().empty()){w.text("discrete-measurement/1");w.text(p.measurementIdentity());}if(!c.contextIdentity.empty()){w.text("application-context/1");w.text(c.contextIdentity);}return w.data();
}
inline void writeWork(CheckpointWriter& w,const DiscreteTrainingWork& x){for(auto n:{x.lossAttempts,x.rowAttempts,x.nodeVisits,x.operations,x.invalidLosses,x.invalidRows,x.iterations,x.iterationOperations})w.integer(n);}
inline DiscreteTrainingWork readWork(CheckpointReader& r){DiscreteTrainingWork w;w.lossAttempts=r.integer();w.rowAttempts=r.integer();w.nodeVisits=r.integer();w.operations=r.integer();w.invalidLosses=r.integer();w.invalidRows=r.integer();w.iterations=r.integer();w.iterationOperations=r.integer();return w;}
}
// One coordinator, caller-owned RandomContext. Only step() invokes the shared GA.
class DiscreteEvolutionRun {
public:
    using PopulationType=Population<DiscreteExpressionCandidate,DiscreteRegressionOrdering>;
    using Algorithm=GeneticAlgorithm<PopulationType,DiscreteExpressionCandidate,DiscreteVariation>;
    DiscreteEvolutionRun(std::shared_ptr<const DiscreteRegressionProblem> p,const DiscreteSearchConfig& c,RandomContext& random)
        :DiscreteEvolutionRun(std::move(p),c,random,RestoreTag{}) {
        if(c.maxLossAttempts<c.population)throw DiscreteLossBudgetExceeded();
        if(random.execution().stopRequested())throw EvaluationCancelled();
        std::vector<DiscreteExpressionCandidate> initial;initial.reserve(c.population);
        for(std::size_t i=0;i<c.population;++i)initial.push_back(algorithm_->variation().initialize(random.engine()));
        population_=std::make_unique<PopulationType>(initial,random,ObjectiveDirection::Minimize);
    }
    const PopulationType& population() const {if(!population_)throw std::logic_error("no committed discrete population");return *population_;}
    const DiscreteTrainingWork& work() const noexcept{return objective_->work();}
    const Algorithm& algorithm() const noexcept{return *algorithm_;}
    std::optional<DiscreteSearchStop> stoppingReason() const {
        const auto best=population().bestSeen();if(!best)return DiscreteSearchStop::NoViableCandidates;
        if(algorithm_->attempts()&&algorithm_->lastOutcome()==GenerationOutcome::NoViableCandidates)return DiscreteSearchStop::NoViableCandidates;
        if(best->fitness()==0)return DiscreteSearchStop::ExactFit;
        if(algorithm_->attempts()>=config_.maxGenerations)return DiscreteSearchStop::GenerationLimit;
        if(objective_->remaining()<config_.population)return DiscreteSearchStop::LossBudget;
        if(random_.execution().stopRequested())return DiscreteSearchStop::Cancelled;
        return {};
    }
    GenerationOutcome step(){if(stoppingReason())throw std::logic_error("discrete run has reached a stopping boundary");return algorithm_->evolve(*population_);}
    CheckpointDocument checkpoint() const {
        const auto state=RunCheckpointAccess::population(population());const auto rng=RunCheckpointAccess::random(random_);const auto a=RunCheckpointAccess::algorithm(*algorithm_);const auto v=algorithm_->variation().statistics();
        CheckpointWriter w;w.text(CandidateCheckpointCodec<DiscreteExpressionCandidate>::identifier);w.integer(rng.seed);w.integer(rng.candidateAttempts);w.text(rng.engine);w.integer(rng.threads);
        w.integer(a.attempts);w.integer(a.published);w.integer(static_cast<unsigned>(a.lastOutcome));
        for(auto n:{v.initializations,v.mutations,v.crossovers,v.rejections,v.fallbacks})w.integer(n);
        if(config_.grammar.iteratorDiscovery||config_.grammar.recordIterationDiagnostics)for(auto n:{v.iterationGenerated,v.dependentGenerated,v.bodyRepairs,v.alignedCrossovers})w.integer(n);
        genetik_discrete_detail::writeWork(w,objective_->work());writeCommittedPopulation(w,state);
        CheckpointDocument document;document.codec="genetik.discrete-evolution/2";document.identity=identity_;document.payload=w.data();document.generation=state.generation;return document;
    }
    void save(const std::filesystem::path& path) const {saveCheckpointAtomic(path,checkpoint());}
    static std::unique_ptr<DiscreteEvolutionRun> restore(const CheckpointDocument& document,std::shared_ptr<const DiscreteRegressionProblem> problem,const DiscreteSearchConfig& config,RandomContext& random){
        auto result=std::unique_ptr<DiscreteEvolutionRun>(new DiscreteEvolutionRun(std::move(problem),config,random,RestoreTag{}));
        if(document.codec!="genetik.discrete-evolution/2"||document.runtime!=checkpointRuntime()||document.identity!=result->identity_)throw CheckpointError("discrete run configuration, data, iteration policy or runtime differs");
        CheckpointReader r(document.payload);if(r.text()!=CandidateCheckpointCodec<DiscreteExpressionCandidate>::identifier)throw CheckpointError("discrete candidate codec differs");
        RandomCheckpoint rng;rng.seed=static_cast<std::uint32_t>(r.count(std::numeric_limits<std::uint32_t>::max()));rng.candidateAttempts=r.integer();rng.engine=r.text(16000);rng.threads=static_cast<unsigned>(r.count(256));
        AlgorithmCheckpoint a;a.attempts=r.integer();a.published=static_cast<unsigned>(r.count(std::numeric_limits<unsigned>::max()));a.lastOutcome=static_cast<GenerationOutcome>(r.count(2));
        DiscreteVariationStatistics v;v.initializations=r.integer();v.mutations=r.integer();v.crossovers=r.integer();v.rejections=r.integer();v.fallbacks=r.integer();
        if(config.grammar.iteratorDiscovery||config.grammar.recordIterationDiagnostics){v.iterationGenerated=r.integer();v.dependentGenerated=r.integer();v.bodyRepairs=r.integer();v.alignedCrossovers=r.integer();}
        const auto work=genetik_discrete_detail::readWork(r);const auto state=readCommittedPopulation<DiscreteExpressionCandidate>(r,DiscreteCheckpointContext{result->problem_,result->objective_});r.finish();
        if(!document.generation||*document.generation!=state.generation||state.generation!=a.published||state.members.size()!=config.population||state.direction!=ObjectiveDirection::Minimize||a.attempts>config.maxGenerations||a.attempts<a.published||v.initializations!=config.population||rng.candidateAttempts<work.lossAttempts)throw CheckpointError("inconsistent discrete run counters");
        if(a.attempts==std::numeric_limits<std::uint64_t>::max()||rng.candidateAttempts/(a.attempts+1)>config.population||(rng.candidateAttempts/(a.attempts+1)==config.population&&rng.candidateAttempts%(a.attempts+1)))throw CheckpointError("excess candidate requests");
        if(v.mutations>a.attempts*config.population||v.crossovers>a.attempts*config.population||v.fallbacks>v.mutations+v.crossovers||v.rejections>(v.mutations+v.crossovers)*config.grammar.proposalLimit)throw CheckpointError("invalid discrete variation counters");
        if(v.dependentGenerated>v.iterationGenerated||v.iterationGenerated>v.initializations+v.mutations+2*v.crossovers||v.alignedCrossovers>v.crossovers*config.grammar.proposalLimit)throw CheckpointError("invalid iterator diagnostic counters");
        auto ready=RunCheckpointAccess::restorePopulation<DiscreteExpressionCandidate,DiscreteRegressionOrdering>(state,random);
        result->objective_->restoreWork(work);RunCheckpointAccess::restoreAlgorithm(*result->algorithm_,a);result->algorithm_->variation().restoreStatistics(v);
        RunCheckpointAccess::restoreRandom(random,rng);result->population_=std::move(ready);return result;
    }
private:
    struct RestoreTag{};
    DiscreteEvolutionRun(std::shared_ptr<const DiscreteRegressionProblem> p,const DiscreteSearchConfig& c,RandomContext& random,RestoreTag)
        :problem_(std::move(p)),config_(c),random_(random) {
        if(!problem_||random.evaluationAttempts()!=0||c.population<2||c.population>100000||c.elites>=c.population||!std::isfinite(c.crossoverRate)||!std::isfinite(c.mutationRate)||c.crossoverRate<0||c.crossoverRate>1||c.mutationRate<0||c.mutationRate>1)throw std::invalid_argument("invalid discrete run configuration");
        objective_=std::make_shared<DiscreteTrainingObjective>(problem_,c.maxLossAttempts,&random.execution());DiscreteVariation variation(problem_,c.grammar,objective_);
        identity_=genetik_discrete_detail::runIdentity(*problem_,c,random); // exact bounded identity before RNG/work or worker creation
        algorithm_=std::make_unique<Algorithm>(c.crossoverRate,c.mutationRate,random,std::move(variation),c.elites);
    }
    std::shared_ptr<const DiscreteRegressionProblem> problem_;DiscreteSearchConfig config_;RandomContext& random_;std::shared_ptr<DiscreteTrainingObjective> objective_;std::unique_ptr<PopulationType> population_;std::unique_ptr<Algorithm> algorithm_;std::string identity_;
};

// Forward import of the unchanged D2/D3 research profile, under identical runtime.
inline CheckpointDocument loadDiscreteRunCheckpoint(const std::filesystem::path& path,CheckpointLimits limits={}){
    std::optional<CheckpointReadCompatibility> compatibility;
    if(std::string_view(genetikVersion)=="0.4.0")compatibility=CheckpointReadCompatibility{"0.3.0","genetik.discrete-evolution/2"};
    return loadCompatibleCheckpoint(path,limits,std::move(compatibility));
}
