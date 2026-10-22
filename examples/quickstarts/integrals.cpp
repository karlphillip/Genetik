#include <IntegralRegression.hpp>
#include <ExpressionParsing.hpp>
#include <array>
#include <iostream>
int main(int argc,char** argv) {
    try {
        ExecutionConfig execution;
        if(argc==3 && std::string_view(argv[1])=="--threads")execution=ExecutionConfig::parse(argv[2]);
        else if(argc!=1)throw std::invalid_argument("usage: integrals_quickstart [--threads N]");
        const auto lower=parseExpression("0",1),upper=parseExpression("x0",1);
        // x0 is external x; appended x1 is the local integration variable t.
        IntegralExpression supplied(1,lower,upper,parseExpression("2*x1+1",2),1);
        auto value=supplied.evaluate(std::array{2.});
        if(!value.accepted() || std::abs(*value.estimate-6)>1e-10)return 1;
        IntegralExpression parameter(1,lower,upper,parseExpression("x0*x1",2),1);
        if(std::abs(*parameter.evaluate(std::array{2.}).estimate-4)>1e-10)return 1;
        auto problem=IntegralRegressionProblem::create(1,lower,upper,{{{-1},1.25},{{1},.25},{{2},2}}, {9,4,8});
        IntegralTrainingObjective objective(problem,512);ConstantFitConfig fit;fit.maxLossAttempts=512;
        const auto fitted=fitConstants(parseExpression("1*x1+1",2),objective,fit);
        if(fitted.loss!=problem->measure(fitted.expression).loss || fitted.loss.score()>1e-12)return 1;
        SymbolicRegressionConfig cfg;cfg.population=8;cfg.elites=1;cfg.maxEvaluations=32;cfg.maxTrainingLossAttempts=32;cfg.maxGenerationAttempts=3;
        RandomContext run(42,execution);auto search=integralRegression(problem,cfg,run);
        if(!search.search.best)return 1; // declared stopping is success, not exact discovery
        std::cout<<supplied.toString()<<"\nestimated_convergence value="<<*value.estimate<<" estimated_error="<<*value.estimatedAbsoluteError
          <<" samples="<<value.work.integrandEvaluations<<"\nfitted="<<fitted.expression.toString()<<" MSE="<<fitted.loss.score()<<" attempts="<<objective.work().lossAttempts
          <<"\nthreads_requested="<<execution.threads<<" threads_effective="<<std::min<std::size_t>(execution.threads,cfg.population)
          <<" stop="<<searchStopName(search.search.stoppingReason)<<"\n"<<problem->model(search.search.best->expression()).toString()
          <<"\nEstimated error is heuristic; finite observations do not identify a unique global integrand.\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
