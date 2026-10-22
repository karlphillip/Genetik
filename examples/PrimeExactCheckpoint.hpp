#pragma once
#include "PrimeExactReport.hpp"
#include <ScalarExpressionCheckpoint.hpp>
namespace prime_example {
struct ExactImprovement {
    std::uint64_t lossAttempts,generation;double loss;std::string canonical;
    bool operator==(const ExactImprovement&)const=default;
};
struct ExactSavedRun {
    std::uint32_t seed=0;unsigned threads=1;std::uint64_t allowance=0,fitEvery=1;bool fitting=false;
    RandomCheckpoint random;AlgorithmCheckpoint algorithm;
    TrainingWork training;PreparationStatistics preparation;VariationStatistics variation;
    std::optional<double> rawInitialBest;
    std::uint64_t committedLosses=0;double elapsed=0;
    std::vector<ExactImprovement> history;
    ExactReportingWork reporting;
    std::optional<double> previousDisplayedLoss;
    std::optional<std::size_t> previousDisplayedMatches;
    std::string lastStop="running",populationBytes;
};
inline std::string exactContextIdentity(const std::vector<std::uint32_t>& reference,ExpressionLimits limits,const SymbolicRegressionConfig& cfg){
    CheckpointWriter w;w.text("prime-exact/1;x0=double(n)/1000;decode=10000*g;round=std::round,int64;loss=mismatches+sum_wrong(error/(1+error))/1000;ordering=RegressionOrdering-v1");
    w.integer(reference.size());for(auto p:reference)w.integer(p);
    w.integer(limits.maxNodes);w.integer(limits.maxDepth);w.real(limits.maxAbsConstant);w.integer(limits.maxArithmeticOperations);
    w.integer(cfg.population);w.integer(cfg.elites);w.real(static_cast<float>(cfg.crossoverRate));w.real(static_cast<float>(cfg.mutationRate));
    const auto& g=cfg.grammar;w.integer(g.operators.size());for(auto k:g.operators)w.integer(static_cast<unsigned>(k));
    w.integer(g.constants.size());for(auto c:g.constants)w.real(c);w.integer(g.exponents.size());for(auto e:g.exponents)w.integer(e);
    w.integer(g.initialMaxDepth);w.integer(g.mutationMaxDepth);w.real(g.growBinaryProbability);w.integer(g.proposalLimit);w.real(g.continuousProbability);w.real(g.continuousLower);w.real(g.continuousUpper);
    w.integer(cfg.fitEveryGenerations);w.integer(cfg.fitting.has_value());if(cfg.fitting){const auto& f=*cfg.fitting;w.real(f.lower);w.real(f.upper);w.real(f.initialStep);w.real(f.minimumStep);w.integer(f.maxLossAttempts);w.integer(f.maxSweeps);w.integer(f.maxProposals);w.integer(f.lossThreshold.has_value());if(f.lossThreshold)w.real(*f.lossThreshold);}
    // Store the complete canonical identity bytes, not just a collision-prone hash.
    return w.data();
}
inline CheckpointDocument encodeExactRun(const ExactSavedRun& s,std::string identity){
    CheckpointWriter w;w.integer(s.seed);w.integer(s.threads);w.integer(s.allowance);w.integer(s.fitEvery);w.integer(s.fitting);
    w.integer(s.random.seed);w.integer(s.random.candidateAttempts);w.text(s.random.engine);w.integer(s.algorithm.attempts);w.integer(s.algorithm.published);w.integer(static_cast<unsigned>(s.algorithm.lastOutcome));
    writeTrainingWork(w,s.training);writePreparationStatistics(w,s.preparation);writeVariationStatistics(w,s.variation);
    w.integer(s.rawInitialBest.has_value());if(s.rawInitialBest)w.real(*s.rawInitialBest);
    w.integer(s.committedLosses);w.real(s.elapsed);w.integer(s.history.size());for(const auto& h:s.history){w.integer(h.lossAttempts);w.integer(h.generation);w.real(h.loss);w.text(h.canonical);}
    w.integer(s.reporting.snapshots);w.integer(s.reporting.predictionRows);w.integer(s.previousDisplayedLoss.has_value());if(s.previousDisplayedLoss)w.real(*s.previousDisplayedLoss);w.integer(s.previousDisplayedMatches.has_value());if(s.previousDisplayedMatches)w.integer(*s.previousDisplayedMatches);
    w.text(s.lastStop);w.text(s.populationBytes);
    CheckpointDocument result{std::string(CandidateCheckpointCodec<ExpressionCandidate>::identifier),std::move(identity),w.data()};result.generation=s.algorithm.published;return result;
}
inline ExactSavedRun decodeExactRun(const CheckpointDocument& d){
    if(d.runtime!=checkpointRuntime()||d.codec!=CandidateCheckpointCodec<ExpressionCandidate>::identifier)throw CheckpointError("wrong checkpoint runtime/candidate codec");
    CheckpointReader r(d.payload);ExactSavedRun s;
    s.seed=static_cast<std::uint32_t>(r.count(std::numeric_limits<std::uint32_t>::max()));s.threads=static_cast<unsigned>(r.count(256));if(!s.threads)throw CheckpointError("zero checkpoint worker count");
    s.allowance=r.integer();s.fitEvery=r.integer();s.fitting=r.count(1)!=0;
    s.random.seed=static_cast<std::uint32_t>(r.count(std::numeric_limits<std::uint32_t>::max()));s.random.candidateAttempts=r.integer();s.random.engine=r.text();s.random.threads=s.threads;
    s.algorithm.attempts=r.integer();s.algorithm.published=static_cast<unsigned>(r.count(std::numeric_limits<unsigned>::max()));s.algorithm.lastOutcome=static_cast<GenerationOutcome>(r.count(2));
    s.training=readTrainingWork(r);s.preparation=readPreparationStatistics(r);s.variation=readVariationStatistics(r);
    if(r.count(1))s.rawInitialBest=r.real();
    s.committedLosses=r.integer();s.elapsed=r.real();const auto count=r.count(100000);s.history.reserve(count);
    for(std::size_t i=0;i<count;++i){ExactImprovement h;h.lossAttempts=r.integer();h.generation=r.integer();h.loss=r.real();h.canonical=r.text();s.history.push_back(std::move(h));}
    s.reporting.snapshots=r.integer();s.reporting.predictionRows=r.integer();if(r.count(1))s.previousDisplayedLoss=r.real();if(r.count(1))s.previousDisplayedMatches=r.count(1000);
    if(s.previousDisplayedLoss && (!std::isfinite(*s.previousDisplayedLoss)||*s.previousDisplayedLoss<0||*s.previousDisplayedLoss>1001))throw CheckpointError("invalid prior display loss");
    s.lastStop=r.text();s.populationBytes=r.text();r.finish();
    if(s.lastStop!="running"&&s.lastStop!="cancelled"&&s.lastStop!="generation_limit"&&s.lastStop!="evaluation_budget"&&s.lastStop!="no_viable_candidates"&&s.lastStop!="exact_training_fit")throw CheckpointError("unknown saved stopping state");
    if(s.seed!=s.random.seed || !s.fitEvery || !s.allowance || s.training.lossAttempts>s.allowance ||
       s.committedLosses>s.training.lossAttempts || s.algorithm.published>s.algorithm.attempts || !std::isfinite(s.elapsed)||s.elapsed<0 || s.elapsed>1e12 ||
       (s.rawInitialBest && !std::isfinite(*s.rawInitialBest)))throw CheckpointError("invalid checkpoint run counters");
    std::uint64_t lastGeneration=0,lastLosses=0;
    for(std::size_t i=0;i<s.history.size();++i){const auto& h=s.history[i];if(!std::isfinite(h.loss)||h.loss<0||h.loss>1001||h.generation>s.algorithm.published||h.lossAttempts>s.committedLosses|| (i&&(h.generation<=lastGeneration||h.lossAttempts<lastLosses)))throw CheckpointError("invalid improvement history");lastGeneration=h.generation;lastLosses=h.lossAttempts;}
    return s;
}
inline void validateExactWork(const ExactSavedRun& s,std::size_t rows) {
    const auto add=[](std::uint64_t& total,std::uint64_t value){if(value>std::numeric_limits<std::uint64_t>::max()-total)throw CheckpointError("checkpoint counter sum overflow");total+=value;};
    std::uint64_t requests=0,ordinary=0,baselines=0,probes=0,observations=0,improved=0,skipped=0,completions=0,terminations=0;
    for(const auto& o:s.preparation.origins){
        if(o.fits>o.requests||o.skipped>o.requests-o.fits||o.ordinary>o.requests||o.baselines>o.fits||o.improved>o.fits)throw CheckpointError("invalid preparation counters");
        add(requests,o.requests);add(ordinary,o.ordinary);add(baselines,o.baselines);add(probes,o.probes);add(observations,o.rows);add(improved,o.improved);add(skipped,o.skipped);
    }
    for(auto n:{s.preparation.completedImproved,s.preparation.completedUnchanged,s.preparation.noConstants,s.preparation.invalidStarts})add(completions,n);
    for(auto n:s.preparation.terminations)add(terminations,n);
    if(requests!=s.random.candidateAttempts||ordinary!=s.training.ordinaryLossAttempts||baselines!=s.training.fittingBaselineAttempts||probes!=s.training.fittingProbeAttempts||observations!=s.training.rowAttempts||improved!=s.training.fittingImproved||skipped!=s.preparation.skippedBySchedule||completions!=terminations||s.training.fittingInvocations>requests||s.training.invalid>s.training.fittingProbeAttempts||s.training.infeasible>s.training.proposals||s.training.duplicates>s.training.proposals||s.training.fittingImproved>s.training.accepted||!rows|| (s.training.rowAttempts && (s.training.rowAttempts-1)/rows>=s.training.lossAttempts))throw CheckpointError("inconsistent checkpoint work/preparation totals");
}
struct ExactRunOptions {
    bool pretty=false;
    std::ostream* metrics=nullptr;
    std::filesystem::path checkpointDirectory;
    std::chrono::steady_clock::duration checkpointInterval=std::chrono::seconds(300);
    std::size_t checkpointKeep=3;
    std::optional<CheckpointDocument> resume;
};
} // namespace prime_example
