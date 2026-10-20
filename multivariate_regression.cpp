#include "examples/MultivariateData.hpp"
#include "examples/RegressionWorkflow.hpp"
#include <chrono>

using namespace regression_example;
int main(int argc,char** argv) {
    std::cout<<std::setprecision(17)<<std::unitbuf;
    try {
        if(argc==3 && std::string(argv[1])=="--generate") {writeMultivariateFiles(argv[2]);return 0;}
        if(argc!=1)throw std::invalid_argument("Usage: genetik_multivariate_demo [--generate DIRECTORY]");
        const auto train=multivariateRows(0);
        const auto problem=RegressionProblem::create(train,{31,6,8});
        const auto config=searchConfig(100000);
        printConfig(config);
        std::cout<<"campaign=initial seeds=0,1,2,3,42 x0=first x1=second target=target training_rows=25"
                 <<" recovery_max_abs=1e-6 representative_rule=training_mse_nodes_text_lowest_seed\n";
        struct Run {unsigned seed;SymbolicRegressionResult result;PredictionReport train;};
        std::vector<Run> runs;
        bool missed=false;
        for(unsigned seed:{0u,1u,2u,3u,42u}) {
            RandomContext random(seed);
            const auto start=std::chrono::steady_clock::now();
            auto result=symbolicRegression(problem,config,random);
            const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            std::cout<<"seed="<<seed<<' ';printResult(result,100000,seconds);
            PredictionReport metrics;
            if(result.best)metrics=predictionReport(result.best->expression(),train);
            std::cout<<"seed="<<seed<<' ';printPredictionReport("training",metrics);
            missed|=!metrics.maximum || *metrics.maximum>1e-6;
            runs.push_back({seed,std::move(result),metrics});
        }
        if(missed) {
            std::cout<<"training_adjustment_decision=inspect_training_only_diagnostics evaluation_not_performed\n";
            return 2;
        }
        std::cout<<"training_adjustment_decision=not_needed\n";
        const Run* representative=nullptr;
        RegressionOrdering ordering;
        for(const auto& run:runs)if(run.result.best && (!representative ||
            ordering.better(*run.result.best,*representative->result.best,ObjectiveDirection::Minimize)))representative=&run;
        std::cout<<"selection_finalized representative_seed="<<representative->seed<<" expression="<<representative->result.best->text()<<'\n';
        const auto interior=multivariateRows(1),exterior=multivariateRows(2);
        for(const auto& run:runs)if(run.result.best) {
            std::cout<<"seed="<<run.seed<<' ';printPredictionReport("interior",predictionReport(run.result.best->expression(),interior));
            std::cout<<"seed="<<run.seed<<' ';printPredictionReport("exterior",predictionReport(run.result.best->expression(),exterior));
        }
    }catch(const std::exception& error){printError(error);return 1;}
}
