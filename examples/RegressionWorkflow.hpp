#pragma once
#include <SymbolicRegression.hpp>
#include <iomanip>
#include <iostream>

namespace regression_example {
inline SymbolicRegressionConfig searchConfig(std::uint64_t budget, bool fitting = true) {
    SymbolicRegressionConfig config;
    config.population=64;config.maxGenerationAttempts=2000;
    config.maxEvaluations=budget;config.maxTrainingLossAttempts=budget;
    config.trainingLossThreshold=1e-14;config.grammar.continuousProbability=.5;
    if (fitting) {config.fitting=ConstantFitConfig{};config.fitting->lossThreshold=1e-14;}
    return config;
}
inline void printConfig(const SymbolicRegressionConfig& config) {
    std::cout << "execution=serial_cpu population=" << config.population << " generation_limit=" << config.maxGenerationAttempts
              << " training_loss_limit=" << config.maxTrainingLossAttempts << " outer_request_limit=" << config.maxEvaluations
              << " nodes_limit=31 depth_limit=6 absolute_constant_limit=8 grammar=+,-,*"
              << " selector=linear_rank elites=2 crossover=.9 mutation=.35 init_depth=4 mutation_depth=3"
              << " full_probability=.5 grow_probability=.65 variation_proposal_limit=8"
              << " constants=half_discrete{-2,-1,0,1,2},half_uniform[-8,8) training_threshold=1e-14"
              << " fitting=" << static_cast<bool>(config.fitting);
    if(config.fitting)std::cout<<" fit_bounds=[-8,8] fit_step=1 fit_min_step=1e-8 fit_loss_limit=256 fit_sweeps=128 fit_proposals=8192 fit_threshold=1e-14 fit_slots=all_including_elites_and_clones";
    std::cout<<'\n';
}
// Prediction-only diagnostics: never recompute training MSE outside its allowance.
struct PredictionReport { std::size_t rows=0, invalid=0; std::optional<double> maximum; };
inline PredictionReport predictionReport(const Expression& expression,const std::vector<RegressionRow>& rows) {
    PredictionReport report; double maximum=0;
    for(const auto& row:rows) {
        ++report.rows;
        const auto value=expression.evaluate(row.inputs);
        if(!value.valid()){++report.invalid;continue;}
        volatile double residual=value.value()-row.target;
        if(!std::isfinite(residual)){++report.invalid;continue;}
        maximum=std::max(maximum,std::abs(static_cast<double>(residual)));
    }
    if(!report.invalid)report.maximum=maximum;
    return report;
}
inline void printResult(const SymbolicRegressionResult& result,std::uint64_t allowance,double seconds) {
    const auto w=result.trainingWork;
    std::cout<<"raw_initial_mse="<<result.rawInitialBestMSE.value_or(-1)<<" refined_initial_mse="<<result.initialBestMSE.value_or(-1)
             <<" training_mse="<<(result.best?result.best->fitness():-1)
             <<" discovery_stage="<<(result.bestGeneration ? (*result.bestGeneration==0?"initialization":"offspring"):"none")
             <<" discovery_generation="<<result.bestGeneration.value_or(0)
             <<" attempted_generations="<<result.attemptedGenerations<<" published_generations="<<result.publishedGenerations
             <<" candidate_requests="<<result.candidateEvaluations<<" training_loss_attempts="<<w.lossAttempts
             <<" unused_allowance="<<allowance-w.lossAttempts<<" row_attempts="<<w.rowAttempts
             <<" fits="<<w.fittingInvocations<<" fitting_loss_attempts="<<w.fittingLossAttempts
             <<" fit_proposals="<<w.proposals<<" accepted="<<w.accepted<<" invalid_probes="<<w.invalid
             <<" infeasible="<<w.infeasible<<" duplicates="<<w.duplicates
             <<" variation_rejections="<<result.variation.structuralRejections
             <<" variation_fallbacks="<<result.variation.mutationFallbacks+result.variation.crossoverFallbacks
             <<" mutation_proposals="<<result.variation.mutationProposals<<" crossover_proposals="<<result.variation.crossoverProposals
             <<" mutation_changes="<<result.variation.mutationChanges<<" crossover_changes="<<result.variation.crossoverChanges
             <<" seconds="<<seconds<<" stop="<<searchStopName(result.stoppingReason)<<'\n';
    if(result.best)std::cout<<"nodes="<<result.best->expression().nodeCount()<<" depth="<<result.best->expression().depth()
                            <<" expression="<<result.best->text()<<'\n';
}
inline void printPredictionReport(const std::string& label,const PredictionReport& report) {
    std::cout<<"dataset="<<label<<" max_abs_error=";
    if(report.maximum)std::cout<<*report.maximum;else std::cout<<"invalid";
    std::cout<<" recovery="<<(report.maximum && *report.maximum<=1e-6)<<" reporting_prediction_rows="<<report.rows
             <<" invalid_predictions="<<report.invalid<<'\n';
}
inline void printError(const std::exception& error) {
    std::cerr<<error.what()<<'\n';
    try {std::rethrow_if_nested(error);}catch(const std::exception& cause){printError(cause);}catch(...){std::cerr<<"non-standard cause\n";}
}
} // namespace regression_example
