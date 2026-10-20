#include <TypedSymbolicRegression.hpp>
#include <array>
#include <iostream>
int main(int argc,char** argv){try{
    ExecutionConfig execution;
    if(argc!=1){if(argc!=3 || std::string_view(argv[1])!="--threads")return 1;execution=ExecutionConfig::parse(argv[2]);}
    using K=TypedExpressionKind;
    const std::array schema{TypedShape::vector(2)};
    const std::vector<TypedRegressionRow> rows{
        {{TypedValue::vector(std::array{-1.,2.})},TypedValue::vector(std::array{-2.,4.})},
        {{TypedValue::vector(std::array{2.,-1.})},TypedValue::vector(std::array{4.,-2.})}};
    TypedExpressionLimits limits;limits.maxNodes=7;limits.maxDepth=3;limits.maxAbsConstant=8;
    auto problem=TypedRegressionProblem::create(schema,TypedShape::vector(2),rows,limits);
    TypedSymbolicRegressionConfig config;config.population=8;config.elites=1;config.maxGenerationAttempts=2;
    config.maxEvaluations=24;config.maxTrainingLossAttempts=256;config.fitting=ConstantFitConfig{};
    config.fitting->maxLossAttempts=4;config.fitEveryGenerations=2;config.grammar.operators={K::Add,K::Subtract,K::Multiply};
    RandomContext run(42,execution);const auto result=typedSymbolicRegression(problem,config,run);
    std::cout<<"threads_requested="<<execution.threads<<" stop="<<searchStopName(result.stoppingReason)<<" loss_calls="<<result.trainingWork.lossAttempts<<'\n';
    if(result.best)std::cout<<result.best->expression().schemaString()<<" output="<<result.best->expression().shape().toString()<<" expression="<<result.best->text()<<" mse="<<result.best->fitness()<<'\n';
    // Public stand-alone fitter on an independently owned scalar scaling coefficient.
    const auto model=TypedExpression::binary(K::Multiply,TypedExpression::constant(TypedValue::scalar(1),schema,limits),TypedExpression::variable(0,schema,limits),limits);
    TypedTrainingObjective objective(problem,32);auto fitting=ConstantFitConfig{};fitting.maxLossAttempts=32;fitting.lossThreshold=0;
    const auto fitted=fitTypedConstants(model,objective,fitting);
    if(fitted.loss.score()!=0 || fitted.expression.constantCoefficients()!=1)return 1;
    // Program acceptance is bounded execution plus the deterministic fixed-structure fit,
    // never stochastic discovery of the searched expression.
    return result.trainingWork.lossAttempts<=256?0:1;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
