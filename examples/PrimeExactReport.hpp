#pragma once
#include "PrimeRegression.hpp"
namespace prime_example {
inline double exactPresentationFitness(double loss){
    if(!std::isfinite(loss)||loss<0||loss>1001)throw std::invalid_argument("not an exact-prefix loss");
    const double value=1.0-loss/1001.0;
    // Actual nonzero exact-prefix losses are >=1. This also preserves the
    // advertised zero-loss/one-score distinction for tiny synthetic inputs.
    return loss>0 && value==1.0?std::nextafter(1.0,0.0):value;
}

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
    std::optional<std::uint64_t> bestGeneration{};
    std::uint64_t generationLimit=0;
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
    out<<" generation_limit="<<progress.generationLimit<<" attempted_generations="<<progress.attempts<<" published_generations="<<progress.published
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
           <<" presentation_fitness="<<exactPresentationFitness(candidate.fitness())<<" exact_matches="<<m.matches<<'/'<<m.count<<" mismatches="<<m.count-m.matches
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
}
