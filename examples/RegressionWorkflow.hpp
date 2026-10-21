#pragma once
#include <SymbolicRegression.hpp>
#include <iomanip>
#include <iostream>
#include <set>

namespace regression_example {
inline SymbolicRegressionConfig searchConfig(std::uint64_t budget, bool fitting = true) {
    SymbolicRegressionConfig config;
    config.population=64;config.maxGenerationAttempts=2000;
    config.maxEvaluations=budget;config.maxTrainingLossAttempts=budget;
    config.trainingLossThreshold=1e-14;config.grammar.continuousProbability=.5;
    if (fitting) {config.fitting=ConstantFitConfig{};config.fitting->lossThreshold=1e-14;}
    return config;
}
inline const char* operatorName(ExpressionKind kind) {
    switch (kind) {
    case ExpressionKind::Add: return "add";
    case ExpressionKind::Subtract: return "sub";
    case ExpressionKind::Multiply: return "mul";
    case ExpressionKind::Divide: return "div";
    case ExpressionKind::Log: return "log";
    case ExpressionKind::Sin:return "sin";case ExpressionKind::Cos:return "cos";
    case ExpressionKind::Exp:return "exp";case ExpressionKind::Sqrt:return "sqrt";case ExpressionKind::Powi:return "powi";
    default: throw std::invalid_argument("expected an operator kind");
    }
}
inline std::vector<ExpressionKind> selectOperators(const std::vector<std::string>& names) {
    std::vector<ExpressionKind> kinds;
    std::set<std::string> seen;
    if (names.empty()) throw std::invalid_argument("--operators requires a nonempty list");
    for (const auto& name : names) {
        if (name.empty() || !seen.insert(name).second) throw std::invalid_argument("--operators requires nonempty unique names");
        bool found = false;
        for (auto kind : {ExpressionKind::Add, ExpressionKind::Subtract, ExpressionKind::Multiply, ExpressionKind::Divide, ExpressionKind::Log, ExpressionKind::Sin, ExpressionKind::Cos, ExpressionKind::Exp, ExpressionKind::Sqrt, ExpressionKind::Powi})
            if (name == operatorName(kind)) { kinds.push_back(kind); found = true; break; }
        if (!found) throw std::invalid_argument("--operators: unsupported operator '" + name + "'");
    }
    return kinds;
}
inline void printConfig(const SymbolicRegressionConfig& config) {
    std::cout << "population=" << config.population << " generation_limit=" << config.maxGenerationAttempts
              << " training_loss_limit=" << config.maxTrainingLossAttempts << " outer_request_limit=" << config.maxEvaluations
              << " operators=";
    for (std::size_t i=0;i<config.grammar.operators.size();++i) {
        if (i) std::cout << ',';
        std::cout << operatorName(config.grammar.operators[i]);
    }
    std::cout << " operator_selection=uniform_over_structurally_feasible_entries"
              << " selector=linear_rank elites=2 crossover=.9 mutation=.35 init_depth=4 mutation_depth=3"
              << " full_probability=.5 grow_probability=.65 variation_proposal_limit=8"
              << " constants_discrete={-2,-1,0,1,2} continuous_probability=" << config.grammar.continuousProbability << " continuous_interval=[-8,8) training_threshold=" << config.trainingLossThreshold
              << " fitting=" << static_cast<bool>(config.fitting) << " fit_every=" << config.fitEveryGenerations;
    if(config.fitting) {
        std::cout<<" fit_bounds=[-8,8] fit_step=1 fit_min_step=1e-8 fit_loss_limit=256 fit_sweeps=128 fit_proposals=8192 fit_threshold=";
        if(config.fitting->lossThreshold)std::cout<<*config.fitting->lossThreshold;
        else std::cout<<"disabled";
        std::cout<<" fit_slots=all_including_elites_and_clones";
    }
    std::cout << " exponents=";
    for(std::size_t i=0;i<config.grammar.exponents.size();++i){if(i)std::cout<<',';std::cout<<config.grammar.exponents[i];}
    std::cout<<'\n';
}
// Prediction-only diagnostics: never recompute training MSE outside its allowance.
struct PredictionReport { std::size_t rows=0, invalid=0, invalidErrors=0; std::optional<double> maximum; };
inline PredictionReport predictionReport(const Expression& expression,const std::vector<RegressionRow>& rows) {
    PredictionReport report; double maximum=0;
    genetik_detail::EvaluationWorkspace workspace;
    for(const auto& row:rows) {
        ++report.rows;
        const auto value=workspace.evaluate(expression,row.inputs);
        if(!value.valid()){++report.invalid;continue;}
        volatile double residual=value.value()-row.target;
        if(!std::isfinite(residual)){++report.invalidErrors;continue;}
        maximum=std::max(maximum,std::abs(static_cast<double>(residual)));
    }
    if(!report.invalid && !report.invalidErrors)report.maximum=maximum;
    return report;
}
struct EvaluationReport { RegressionMetrics metrics; PredictionReport predictions; };
// One checked full-dataset loss (which may stop early), then an independent ALL-row
// validity scan. Build both from the same rows; this is reporting, never fitting.
inline EvaluationReport evaluationReport(const Expression& expression, const std::vector<RegressionRow>& rows,
                                         ExpressionLimits limits = {31,6,8}) {
    const auto problem=RegressionProblem::create(rows,limits);
    return {problem->measure(expression),predictionReport(expression,rows)};
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
             <<" invalid_training_losses="<<w.invalidLosses<<" fits="<<w.fittingInvocations<<" fitting_loss_attempts="<<w.fittingLossAttempts
             <<" fit_proposals="<<w.proposals<<" accepted="<<w.accepted<<" invalid_probes="<<w.invalid
             <<" infeasible="<<w.infeasible<<" duplicates="<<w.duplicates
             <<" variation_rejections="<<result.variation.structuralRejections
             <<" variation_fallbacks="<<result.variation.mutationFallbacks+result.variation.crossoverFallbacks
             <<" mutation_proposals="<<result.variation.mutationProposals<<" crossover_proposals="<<result.variation.crossoverProposals
             <<" mutation_changes="<<result.variation.mutationChanges<<" crossover_changes="<<result.variation.crossoverChanges
             <<" ordinary_loss_attempts="<<w.ordinaryLossAttempts<<" fitting_baselines="<<w.fittingBaselineAttempts<<" fitting_probes="<<w.fittingProbeAttempts
             <<" fitting_improved="<<w.fittingImproved<<" schedule_skips="<<result.preparationWork.skippedBySchedule
             <<" unpublished_losses="<<result.unpublishedLossAttempts<<" unpublished_rows="<<result.unpublishedRowAttempts
             <<" seconds="<<seconds<<" stop="<<searchStopName(result.stoppingReason)<<'\n';
    if(result.best)std::cout<<"nodes="<<result.best->expression().nodeCount()<<" depth="<<result.best->expression().depth()
                            <<" expression="<<result.best->text()<<'\n';
}
inline void printPredictionReport(const std::string& label,const PredictionReport& report) {
    std::cout<<"dataset="<<label<<" max_abs_error=";
    if(report.maximum)std::cout<<*report.maximum;else std::cout<<"invalid";
    std::cout<<" recovery="<<(report.maximum && *report.maximum<=1e-6)<<" reporting_prediction_rows="<<report.rows
             <<" invalid_predictions_total="<<report.invalid<<" invalid_errors_total="<<report.invalidErrors<<'\n';
}
inline void printError(const std::exception& error) {
    std::cerr<<error.what()<<'\n';
    try {std::rethrow_if_nested(error);}catch(const std::exception& cause){printError(cause);}catch(...){std::cerr<<"non-standard cause\n";}
}
} // namespace regression_example
