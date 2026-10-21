#include <DerivativeRegression.hpp>
#include <ExpressionParsing.hpp>
#include <array>
#include <iostream>

int main(int argc,char** argv) {
    try {
        ExecutionConfig execution;
        if(argc==3 && std::string_view(argv[1])=="--threads")execution=ExecutionConfig::parse(argv[2]);
        else if(argc!=1)throw std::invalid_argument("usage: derivative_regression_quickstart [--threads N]");
        auto problem=DerivativeRegressionProblem::create(1,{
            {std::nullopt,{{{0},1}},1,1},
            {0,{{{-1},-2},{{0},0},{{1},2}},1,1}}, {9,4,8});
        const auto supplied=parseExpression("x0*x0+1",1); // example, not a population seed
        auto assessment=problem->assess(supplied);
        if(assessment.loss!=Fitness::valid(0) || !assessment.complete)return 1;
        auto fitProblem=DerivativeRegressionProblem::create(1,{
            {std::nullopt,{{{0},1.25}},1,1},
            {0,{{{-1},-3.5},{{0},-.5},{{1},2.5}},1,1}}, {11,5,8});
        DerivativeTrainingObjective objective(fitProblem,512);
        ConstantFitConfig fit;fit.maxLossAttempts=512;
        const auto fitted=fitConstants(parseExpression("1*x0*x0+1*x0+1",1),objective,fit);
        if(fitProblem->measureObservations(fitted.expression).loss!=fitted.loss)return 1;
        std::cout<<"supplied="<<supplied.toString()<<" weighted_normalized_objective="<<assessment.loss.score()
                 <<"\nfitted="<<fitted.expression.toString()<<" objective="<<fitted.loss.score()<<" attempts="<<objective.work().lossAttempts<<'\n';
        SymbolicRegressionConfig cfg;cfg.population=8;cfg.elites=1;cfg.maxGenerationAttempts=3;
        cfg.maxEvaluations=32;cfg.maxTrainingLossAttempts=32;
        RandomContext run(42,execution);auto result=derivativeRegression(problem,cfg,run);
        if(!result.search.best || result.search.trainingWork.rowAttempts!=result.observationWork.observations)return 1;
        std::cout<<"threads_requested="<<execution.threads<<" threads_effective="<<std::min<std::size_t>(execution.threads,cfg.population)
            <<" stop="<<searchStopName(result.search.stoppingReason)<<" objective="<<result.search.best->fitness()
            <<"\nmodel="<<result.search.best->text()<<"\nNote: the anchor is an observation; finite samples do not identify a unique global function.\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
