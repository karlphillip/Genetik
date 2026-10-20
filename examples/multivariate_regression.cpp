#include "examples/StrictArithmeticData.hpp"
#include "examples/RegressionWorkflow.hpp"
#include <chrono>

using namespace regression_example;
int main(int argc,char** argv) {
    std::cout<<std::setprecision(17)<<std::unitbuf;
    try {
        bool arithmetic=false;std::uint64_t fitEvery=1;
        std::optional<std::filesystem::path> directory;
        for(int i=1;i<argc;++i) {
            const std::string option=argv[i];
            if(option=="--arithmetic" && !arithmetic)arithmetic=true;
            else if(option=="--fit-every" && i+1<argc) {
                const std::string value=argv[++i];auto parsed=std::from_chars(value.data(),value.data()+value.size(),fitEvery);
                if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || fitEvery==0)throw std::invalid_argument("--fit-every: expected positive integer");
            }
            else if(option=="--generate" && !directory && i+1<argc)directory=argv[++i];
            else throw std::invalid_argument("Usage: genetik_multivariate_demo [--arithmetic] [--generate DIRECTORY] [--fit-every N]");
        }
        if(directory){if(arithmetic)writeArithmeticFiles(*directory);else writeMultivariateFiles(*directory);return 0;}
        const auto rows=arithmetic?arithmeticRows:multivariateRows;
        const auto train=rows(0);
        const auto problem=RegressionProblem::create(train,{31,6,8});
        auto config=searchConfig(100000);config.fitEveryGenerations=fitEvery;
        if(arithmetic)config.grammar.operators={ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply,ExpressionKind::Divide,ExpressionKind::Log};
        printConfig(config);
        std::cout<<"campaign="<<(arithmetic?"strict_arithmetic":"initial")<<" seeds=0,1,2,3,42 x0=first x1=second target=target training_rows=25"
                 <<" recovery_max_abs=1e-6 representative_rule=training_mse_nodes_text_lowest_seed\n";
        struct Run {unsigned seed;SymbolicRegressionResult result;PredictionReport train;};
        std::vector<Run> runs;
        bool missed=false;
        bool thresholdReached=false;
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
            thresholdReached|=result.best && result.best->fitness()<=config.trainingLossThreshold;
            runs.push_back({seed,std::move(result),metrics});
        }
        if(arithmetic ? !thresholdReached : missed) {
            std::cout<<"training_adjustment_decision=inspect_training_only_diagnostics evaluation_not_performed\n";
            return 2;
        }
        std::cout<<"training_adjustment_decision=not_needed\n";
        const Run* representative=nullptr;
        RegressionOrdering ordering;
        for(const auto& run:runs)if(run.result.best && (!representative ||
            ordering.better(*run.result.best,*representative->result.best,ObjectiveDirection::Minimize)))representative=&run;
        std::cout<<"selection_finalized representative_seed="<<representative->seed<<" expression="<<representative->result.best->text()<<'\n';
        const auto interior=rows(1),exterior=rows(2);
        for(const auto& run:runs)if(run.result.best) {
            if(arithmetic) {
                for(double x:{0.,-1.}) {
                    const auto diagnostic=run.result.best->expression().evaluate(std::array{x,1.});
                    std::cout<<"diagnostic seed="<<run.seed<<" x0="<<x<<" x1=1 prediction=";
                    if(diagnostic.valid())std::cout<<diagnostic.value();else std::cout<<"invalid";
                    std::cout<<" no_target_label=true\n";
                }
            }
            std::cout<<"seed="<<run.seed<<' ';printPredictionReport("interior",predictionReport(run.result.best->expression(),interior));
            std::cout<<"seed="<<run.seed<<' ';printPredictionReport("exterior",predictionReport(run.result.best->expression(),exterior));
        }
    }catch(const std::exception& error){printError(error);return 1;}
}
