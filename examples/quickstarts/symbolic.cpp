#include <SymbolicRegression.hpp>
#include <iostream>
int main(int argc,char** argv) {
    try {
    ExecutionConfig execution;
    if(argc!=1){if(argc!=3 || std::string_view(argv[1])!="--threads")return 1;execution=ExecutionConfig::parse(argv[2]);}
    std::cout<<"threads_requested="<<execution.threads<<" threads_effective="<<std::min(execution.threads,8u)<<'\n';
    // Caller-ordered inputs: x0 is the first input column; one scalar target.
    const auto problem=RegressionProblem::create({{{-1},1},{{0},1},{{1},3},{{2},7}}, {15,5,8});
    SymbolicRegressionConfig config;
    config.population=8;config.elites=1;config.maxGenerationAttempts=4;
    config.maxEvaluations=40;config.maxTrainingLossAttempts=256;
    config.fitting=ConstantFitConfig{}; // optional: reset() for discrete-only search
    config.fitting->maxLossAttempts=8;config.fitting->lossThreshold=0;
    config.fitEveryGenerations=1;config.trainingLossThreshold=0;
    RandomContext random(42,execution);
    const auto result=symbolicRegression(problem,config,random);
    std::cout<<"seed="<<random.seed()<<" binding=x0:first_input_column stop="<<searchStopName(result.stoppingReason)
             <<" loss_calls="<<result.trainingWork.lossAttempts<<" budget=256\n";
    if(result.best)std::cout<<"expression="<<result.best->expression().toString()
        <<" mse="<<result.best->fitness()<<" nodes="<<result.best->expression().nodeCount()<<'\n';
    // Selection/fitting saw only problem. Separate evaluation rows would be scored now.
    return result.trainingWork.lossAttempts<=256 ? 0:1;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
