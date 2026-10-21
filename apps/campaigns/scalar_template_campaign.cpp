#include "examples/ScalarTemplateData.hpp"
#include "examples/ExecutionOptions.hpp"
#include "examples/RegressionWorkflow.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>

int main(int argc,char** argv) {
    using namespace scalar_template_example;
    try {
        const auto execution=genetik_example::executionOptions(argc,argv);std::filesystem::path output;bool smoke=false;
        for(int i=1;i<argc;++i) {
            const std::string_view option=argv[i];
            if(option=="--smoke"){smoke=true;continue;}
            if(option!="--output" || !output.empty() || ++i==argc)throw std::invalid_argument("usage: genetik_scalar_template_campaign --output NEW_DIR [--smoke] [--threads N]");
            output=argv[i];
        }
        if(output.empty() || std::filesystem::exists(output))throw std::invalid_argument("explicit new output directory required");
        std::filesystem::create_directories(output);
        std::ofstream protocol(output/"PROTOCOL.txt");
        protocol<<"x0=v,x1=T; allowed_variables={1}; allowed_ops=add,sub,mul; constants={-2,-1,0,1,2}; continuous=0; fitting=off\n"
            <<"population="<<(smoke?8:128)<<" loss_limit="<<(smoke?32:10000)<<" candidate_limit=same; generation_limit="<<(smoke?3:2000)<<"; seed_order="<<(smoke?"0":"0,1,2,3,42")<<'\n'
            <<"local_nodes=9 local_depth=4 full_nodes=13 full_depth=6 abs_constant=2 materialization_bytes=1048576\n"
            <<"rank_selector; elites=2; crossover=.9; mutation=.35; init_depth=4; mutation_depth=3; proposal_limit=8; grow_probability=.65; full_probability=.5; feasible_ops_uniform\n"
            <<"training v,T={-2,-1,0,1,2} Cartesian(25); interior v={-1.5,-.5,0,.5,1.5},T={-1.5,-.5,.5,1.5}(20); exterior v={-3,0,3},T={-3,3}(6); v outer/T inner\n"
            <<"independent target=-(T*T+1)*v; stop training MSE=0; recovery max_error<=1e-6 per set; representative=MSE,nodes,full_text,first_declared_seed; freeze all models before assessment\n"
            <<"threads="<<execution.threads<<"; seeds sequential; no target injection; supplied source subtree discarded\n";
        protocol.close();
        const std::array datasets{data(0),data(1),data(2)};
        for(std::size_t i=0;i<datasets.size();++i) {
            std::ofstream table(output/("data-"+std::to_string(i)+".csv"));table<<std::setprecision(17)<<"v,T,F\n";
            for(const auto& row:datasets[i])table<<row.inputs[0]<<','<<row.inputs[1]<<','<<row.target<<'\n';
        }
        auto domain=problem(datasets[0]);auto cfg=config();if(smoke){cfg.population=8;cfg.maxEvaluations=32;cfg.maxTrainingLossAttempts=32;cfg.maxGenerationAttempts=3;}
        genetik_example::printExecution(execution,cfg.population);
        struct Frozen{unsigned seed;SymbolicRegressionResult result;double seconds;};std::vector<Frozen> frozen;
        std::ofstream records(output/"search.txt");records<<std::setprecision(17);
        const std::vector<unsigned> seeds=smoke?std::vector<unsigned>{0}:std::vector<unsigned>{0,1,2,3,42};
        for(auto seed:seeds) {
            RandomContext random(seed,execution);const auto start=std::chrono::steady_clock::now();auto result=scalarTemplateRegression(domain,cfg,random);
            const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            records<<"seed="<<seed<<" initial="<<(result.initialBestMSE?*result.initialBestMSE:-1)<<" published="<<result.publishedGenerations<<" attempts="<<result.attemptedGenerations
                <<" loss_calls="<<result.trainingWork.lossAttempts<<" rows="<<result.trainingWork.rowAttempts<<" unused="<<cfg.maxTrainingLossAttempts-result.trainingWork.lossAttempts
                <<" discovery="<<(result.bestGeneration?std::to_string(*result.bestGeneration):"none")<<" seconds="<<seconds<<" stop="<<searchStopName(result.stoppingReason)
                <<" mutation_proposals="<<result.variation.mutationProposals<<" crossover_proposals="<<result.variation.crossoverProposals<<" rejections="<<result.variation.structuralRejections
                <<" mutation_changes="<<result.variation.mutationChanges<<" crossover_changes="<<result.variation.crossoverChanges<<" mutation_fallbacks="<<result.variation.mutationFallbacks<<" crossover_fallbacks="<<result.variation.crossoverFallbacks<<'\n';
            if(result.best) {
                const auto hole=domain->substitution(result.best->expression());
                records<<"hole="<<hole.toString()<<"\nmodel="<<result.best->text()<<"\nfinal_loss="<<result.best->fitness()<<" hole_nodes="<<hole.nodeCount()<<" hole_depth="<<hole.depth()<<" model_nodes="<<result.best->expression().nodeCount()<<" model_depth="<<result.best->expression().depth()<<'\n';
            }
            for(const auto& t:result.improvements)records<<"trace="<<seed<<','<<t.lossAttempts<<','<<t.generation<<','<<t.mse<<'\n';
            frozen.push_back({seed,std::move(result),seconds});
        }
        std::optional<std::size_t> representative;
        RegressionOrdering order;
        for(std::size_t i=0;i<frozen.size();++i)if(frozen[i].result.best && (!representative || order.better(*frozen[i].result.best,*frozen[*representative].result.best,ObjectiveDirection::Minimize)))representative=i;
        records<<"models_frozen representative_seed="<<(representative?std::to_string(frozen[*representative].seed):"none")<<'\n';records.close();
        std::ofstream metrics(output/"assessment.txt");metrics<<std::setprecision(17);
        for(const auto& f:frozen)if(f.result.best)for(int stage=0;stage<3;++stage) {
            const auto& rows=datasets[stage];const auto assessed=regression_example::evaluationReport(f.result.best->expression(),rows,domain->problem()->limits());
            metrics<<"seed="<<f.seed<<" set="<<stage<<" mse=";
            if(assessed.metrics.loss.state()==FitnessState::Valid)metrics<<assessed.metrics.loss.score();else metrics<<"invalid";
            metrics<<" max_error="<<(assessed.metrics.maxAbsoluteError?*assessed.metrics.maxAbsoluteError:std::numeric_limits<double>::infinity())<<" invalid_predictions="<<assessed.predictions.invalid<<" rows="<<assessed.predictions.rows<<'\n';
        }
        // Supplied-structure coefficient evidence is separate, not campaign initialization.
        auto fittingDomain=problem(data(0,true),true);
        const auto initial=Expression::binary(ExpressionKind::Add,Expression::binary(ExpressionKind::Multiply,Expression::constant(1,2),Expression::variable(1,2)),Expression::constant(1,2));
        TrainingObjective objective(fittingDomain->problem(),512);ConstantFitConfig fit;fit.maxLossAttempts=512;
        auto fitted=fitScalarTemplateConstants(*fittingDomain,initial,objective,fit);
        std::ofstream fixture(output/"fitting.txt");fixture<<std::setprecision(17)<<"supplied_structure initial="<<initial.toString()<<" initial_loss="<<fitted.startingLoss.score()<<"\nhole="<<fittingDomain->substitution(fitted.expression).toString()<<"\nmodel="<<fitted.expression.toString()<<"\nloss="<<fitted.loss.score()<<" calls="<<objective.work().lossAttempts<<" rows="<<objective.work().rowAttempts<<" stop="<<constantFitStopName(fitted.stop)<<'\n';
        for(int stage=0;stage<3;++stage)fixture<<"stage="<<stage<<" max_error="<<*regression_example::evaluationReport(fitted.expression,data(stage,true),fittingDomain->problem()->limits()).metrics.maxAbsoluteError<<'\n';
        std::cout<<"records="<<output<<" frozen_runs="<<frozen.size()<<'\n';return 0;
    }catch(const std::exception& e){regression_example::printError(e);return 1;}
}
