#include "examples/ExecutionOptions.hpp"
#include "examples/RegressionData.hpp"
#include "examples/RegressionWorkflow.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace regression_example;
struct ScientificCase {const char* name;std::vector<ExpressionKind> operators;};
static SymbolicRegressionConfig configuration(const ScientificCase& c,bool smoke){
    auto config=searchConfig(smoke?256:10000,false);config.population=smoke?8:128;
    config.trainingLossThreshold=1e-24;config.grammar.continuousProbability=0;
    config.grammar.operators=c.operators;config.grammar.exponents={2,3};return config;
}
struct Run {std::string name;unsigned seed;SymbolicRegressionResult result;double seconds;};
int main(int argc,char** argv){try{
    const auto execution=genetik_example::executionOptions(argc,argv);
    std::filesystem::path data,output;bool smoke=false;
    for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--smoke"){smoke=true;continue;}
        if((arg!="--data" && arg!="--output") || i+1==argc)throw std::invalid_argument("use --data DIR --output NEW_DIR [--smoke] [--threads N]");
        if(arg=="--data")data=argv[++i];else output=argv[++i];}
    if(data.empty() || output.empty() || std::filesystem::exists(output))throw std::invalid_argument("data and a new output directory are required");
    std::filesystem::create_directories(output);
    const std::vector<ScientificCase> cases{{"decay",{ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply,ExpressionKind::Exp}},
        {"oscillation",{ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply,ExpressionKind::Sin,ExpressionKind::Cos}},
        {"root",{ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply,ExpressionKind::Sqrt,ExpressionKind::Powi}}};
    // Search sees fitting rows only. All selected models are frozen before loading evaluation data.
    std::vector<Run> runs;
    for(const auto& c:cases){
        const auto training=loadData((data/(std::string(c.name)+"-training.csv")).string(),{"x"},"y");
        const auto config=configuration(c,smoke);auto problem=RegressionProblem::create(training.rows,{15,5,8});
        for(unsigned seed:{0u,1u,2u,3u,42u}){
            RandomContext random(seed,execution);const auto start=std::chrono::steady_clock::now();
            auto result=symbolicRegression(problem,config,random);
            const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            runs.push_back({c.name,seed,std::move(result),seconds});
        }
    }
    std::ofstream frozen(output/"frozen-models.txt");frozen<<std::setprecision(17);
    for(const auto& c:cases){const Run* representative=nullptr;
        for(const auto& run:runs)if(run.name==c.name && run.result.best){
            const auto& best=*run.result.best;
            if(!representative || RegressionOrdering{}.better(best,*representative->result.best,ObjectiveDirection::Minimize))representative=&run;
            frozen<<run.name<<" seed="<<run.seed<<" mse="<<best.fitness()<<" expression="<<best.text()<<'\n';
        }
        if(representative)frozen<<"representative "<<c.name<<" seed="<<representative->seed<<'\n';
    }
    frozen.close();if(!frozen)throw std::runtime_error("cannot write frozen models");
    for(const auto& c:cases){
        const auto training=loadData((data/(std::string(c.name)+"-training.csv")).string(),{"x"},"y");
        const auto interior=loadData((data/(std::string(c.name)+"-interior.csv")).string(),{"x"},"y");
        const auto exterior=loadData((data/(std::string(c.name)+"-exterior.csv")).string(),{"x"},"y");
        const auto config=configuration(c,smoke);
        std::ofstream report(output/(std::string(c.name)+".txt"));report<<std::setprecision(17);
        // Existing application reporting, with a lexical stream redirect and restoration.
        struct Restore {std::streambuf* old;~Restore(){std::cout.rdbuf(old);}} restore{std::cout.rdbuf(report.rdbuf())};
        std::cout<<std::setprecision(17)<<"case="<<c.name<<" data="<<data.string()<<" nodes_limit=15 depth_limit=5\n";
        printConfig(config);genetik_example::printExecution(execution,config.population);
        double sum=0;for(const auto& row:training.rows)sum+=row.target;
        const auto mean=Expression::constant(sum/static_cast<double>(training.rows.size()),1);
        std::cout<<"training_mean_baseline="<<mean.toString()<<" mse="<<RegressionProblem::create(training.rows,{15,5,8})->measure(mean).loss.score()<<'\n';
        for(const auto& run:runs)if(run.name==c.name){
            std::cout<<"seed="<<run.seed<<'\n';printResult(run.result,config.maxTrainingLossAttempts,run.seconds);
            if(run.result.best){
                const auto& expression=run.result.best->expression();std::size_t constants=0;
                std::cout<<"constant_values=";for(const auto& n:expression.nodes())if(n.kind()==ExpressionKind::Constant){++constants;std::cout<<n.constantValue()<<',';}
                std::cout<<" constant_count="<<constants<<" exponent_values=";for(const auto& n:expression.nodes())if(n.kind()==ExpressionKind::Powi)std::cout<<n.exponent()<<',';
                std::cout<<" full_valid_expression_work_per_prediction="<<expression.arithmeticCost()<<'\n';
                printPredictionReport("training",predictionReport(expression,training.rows));
                printPredictionReport("interior",predictionReport(expression,interior.rows));
                printPredictionReport("exterior",predictionReport(expression,exterior.rows));
            }
            for(const auto& t:run.result.improvements)std::cout<<"trace loss_attempts="<<t.lossAttempts<<" generation="<<t.generation<<" mse="<<t.mse<<'\n';
        }
        report.flush();if(!report)throw std::runtime_error("cannot write scientific results");
    }
    return 0;
}catch(const std::exception& error){printError(error);return 1;}}
