#include "examples/ExecutionOptions.hpp"
#include <SymbolicRegression.hpp>
#include <chrono>
#include <iomanip>
#include <iostream>

static std::vector<RegressionRow> samples(const std::vector<double>& xs) {
    std::vector<RegressionRow> rows;
    for (double x:xs) rows.push_back({{x},2.5*x*x-1.75*x+.625});
    return rows;
}
// Reporting predictions only: no recomputed training loss outside the global allowance.
static double maximumError(const Expression& expression,const std::vector<RegressionRow>& rows) {
    double maximum=0;
    for (const auto& row:rows) {
        const auto prediction=expression.evaluate(row.inputs);
        if (!prediction.valid()) return std::numeric_limits<double>::infinity();
        const double residual=prediction.value()-row.target;
        if (!std::isfinite(residual)) return std::numeric_limits<double>::infinity();
        maximum=std::max(maximum,std::abs(residual));
    }
    return maximum;
}
int main(int argc,char** argv) {
    try {
    const auto execution=genetik_example::executionOptions(argc,argv);
    if(argc!=1)throw std::invalid_argument("only --threads N is supported");
    genetik_example::printExecution(execution, 64);
    std::cout<<std::setprecision(17)<<std::unitbuf;
    std::vector<double> xs,inside;
    for(int k=0;k<=16;++k)xs.push_back(-2.+k/4.);
    for(int k=0;k<16;++k)inside.push_back(-15./8+k/4.);
    const auto train=samples(xs);
    const auto problem=RegressionProblem::create(train,{31,6,8});
    SymbolicRegressionConfig config;
    config.population=64;config.maxEvaluations=100000;config.maxTrainingLossAttempts=100000;
    config.maxGenerationAttempts=2000;config.trainingLossThreshold=1e-14;
    config.grammar.continuousProbability=.5;
    ConstantFitConfig fit;fit.lossThreshold=1e-14;
    std::cout<<"campaign=initial configured_cpu seeds=0,1,2,3,42 population=64 loss_limit=100000 candidate_limit=100000 generations=2000"
             <<" nodes=31 depth=6 constant_bound=8 continuous_probability=.5 continuous_interval=[-8,8) discrete=-2,-1,0,1,2"
             <<" crossover=.9 mutation=.35 elites=2 linear_rank init_depth=4 mutation_depth=3 full_probability=.5 grow=.65 variation_proposals=8"
             <<" fit_step=1 fit_min_step=1e-8 fit_loss_limit=256 fit_sweeps=128 fit_proposals=8192 fit_bounds=[-8,8]"
             <<" training_threshold=1e-14 recovery_max_abs=1e-6 refinement=all_slots_including_elites_and_clones fresh_baselines=true\n";
    struct Run {unsigned seed; bool enabled; SymbolicRegressionResult result; double trainMax,seconds;};
    std::vector<Run> runs;
    bool enabledRecovery=false;
    for(unsigned seed:{0u,1u,2u,3u,42u}) {
        std::vector<std::string> raw;
        std::optional<double> baseline;
        for(bool enabled:{false,true}) {
            config.fitting=enabled?std::optional(fit):std::nullopt;
            RandomContext random(seed,execution);
            const auto start=std::chrono::steady_clock::now();
            auto result=symbolicRegression(problem,config,random);
            const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            if(!enabled){raw=result.rawInitialExpressions;baseline=result.rawInitialBestMSE;}
            else if(raw!=result.rawInitialExpressions || baseline!=result.rawInitialBestMSE)throw std::logic_error("paired raw initialization mismatch");
            const double error=result.best?maximumError(result.best->expression(),train):std::numeric_limits<double>::infinity();
            enabledRecovery|=enabled && error<=1e-6;
            const auto w=result.trainingWork;
            const auto v=result.variation;
            std::cout<<"run seed="<<seed<<" fit="<<enabled<<" raw_initial_mse="<<result.rawInitialBestMSE.value_or(-1)
                     <<" initial_mse="<<result.initialBestMSE.value_or(-1)<<" mse="<<(result.best?result.best->fitness():-1)
                     <<" training_max_abs="<<error<<" discovery_stage="<<(result.bestGeneration==0?"initialization":"offspring")
                     <<" discovery_generation="<<result.bestGeneration.value_or(0)<<" attempts="<<result.attemptedGenerations
                     <<" published="<<result.publishedGenerations<<" candidate_requests="<<result.candidateEvaluations
                     <<" loss_attempts="<<w.lossAttempts<<" unused="<<100000-w.lossAttempts<<" row_attempts="<<w.rowAttempts
                     <<" fits="<<w.fittingInvocations<<" fitting_loss_attempts="<<w.fittingLossAttempts<<" proposals="<<w.proposals
                     <<" accepted="<<w.accepted<<" invalid="<<w.invalid<<" infeasible="<<w.infeasible<<" duplicates="<<w.duplicates
                     <<" variation_rejections="<<v.structuralRejections<<" variation_fallbacks="<<v.mutationFallbacks+v.crossoverFallbacks
                     <<" seconds="<<seconds<<" stop="<<searchStopName(result.stoppingReason)<<'\n';
            if(result.best)std::cout<<"expression seed="<<seed<<" fit="<<enabled<<" nodes="<<result.best->expression().nodeCount()
                                    <<" depth="<<result.best->expression().depth()<<" "<<result.best->text()<<'\n';
            std::cout<<"trace seed="<<seed<<" fit="<<enabled<<" (loss_calls,generation,mse)=";
            for(const auto& point:result.improvements)std::cout<<'('<<point.lossAttempts<<','<<point.generation<<','<<point.mse<<") ";
            std::cout<<'\n';
            runs.push_back({seed,enabled,std::move(result),error,seconds});
        }
    }
    // Decision made from training only, before any evaluation-only predictions.
    if(!enabledRecovery){std::cout<<"training_adjustment_decision=requires_training_only_diagnostics heldout_not_evaluated\n";return 2;}
    std::cout<<"training_adjustment_decision=not_needed paired_raw_populations=identical representative_rule=mse_nodes_text_lowest_seed\n";
    const Run* representative=nullptr;
    RegressionOrdering ordering;
    for(const auto& r:runs)if(r.result.best && (!representative || ordering.better(*r.result.best,*representative->result.best,ObjectiveDirection::Minimize)))representative=&r;
    if(representative)std::cout<<"representative seed="<<representative->seed<<" fit="<<representative->enabled<<" expression="<<representative->result.best->text()<<'\n';
    const auto interior=samples(inside),exterior=samples({-4,-3,3,4});
    for(const auto& r:runs)if(r.result.best){
        const double a=maximumError(r.result.best->expression(),interior),b=maximumError(r.result.best->expression(),exterior);
        std::cout<<"evaluation seed="<<r.seed<<" fit="<<r.enabled<<" interior_max_abs="<<a<<" exterior_max_abs="<<b
                 <<" training_recovery="<<(r.trainMax<=1e-6)<<" interior_recovery="<<(a<=1e-6)<<" exterior_recovery="<<(b<=1e-6)
                 <<" postsearch_prediction_rows=37 postsearch_training_loss_calls=0\n";
    }
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
