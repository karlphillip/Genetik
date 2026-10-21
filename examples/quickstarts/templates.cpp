#include <ScalarTemplateRegression.hpp>
#include <ExpressionParsing.hpp>
#include <array>
#include <iostream>

int main(int argc,char** argv) {
    try {
        ExecutionConfig execution;
        if(argc==3 && std::string_view(argv[1])=="--threads")execution=ExecutionConfig::parse(argv[2]);
        else if(argc!=1)throw std::invalid_argument("usage: templates_quickstart [--threads N]");
        const auto source=parseExpression("((-1.0)*x1)*x0",2);
        ScalarExpressionTemplate scaffold(source,1,{9,4,8},{13,6,8});
        auto data=RegressionProblem::create({{{1,0},-1},{{2,1},-4},{{0,2},0}},{13,6,8});
        auto domain=ScalarTemplateProblem::create(scaffold,data,{1},{ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply});
        const auto supplied=parseExpression("x1*x1+1",2); // demonstration only, never search initialization
        const auto full=domain->materialize(supplied);
        if(full.evaluate(std::array{2.,1.}).value()!=-4 || !full.sameRepresentation(parseExpression(full.toString(),2)))return 1;
        std::cout<<"x0=v x1=T allowed={1}\nsupplied="<<supplied.toString()<<"\nmodel="<<full.toString()<<'\n';
        try{(void)domain->materialize(parseExpression("x0-x0",2));return 1;}catch(const std::invalid_argument& e){std::cout<<"rejected: "<<e.what()<<'\n';}
        SymbolicRegressionConfig cfg;cfg.population=8;cfg.elites=1;cfg.maxEvaluations=32;cfg.maxTrainingLossAttempts=128;cfg.maxGenerationAttempts=3;
        cfg.fitting=ConstantFitConfig{};cfg.fitting->maxLossAttempts=4;
        RandomContext random(42,execution);auto result=scalarTemplateRegression(domain,cfg,random);
        if(!result.best)return 1;
        std::cout<<"threads_requested="<<execution.threads<<" threads_effective="<<std::min(std::size_t(execution.threads),cfg.population)<<" stop="<<searchStopName(result.stoppingReason)
                 <<" loss_calls="<<result.trainingWork.lossAttempts<<"\nevolved="<<domain->substitution(result.best->expression()).toString()<<"\ncomplete="<<result.best->text()<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
