#include "common.hpp"
// Overlapping complete public-header use in a separately compiled translation unit.
bool symbolic_check() {
    const auto problem=RegressionProblem::create({{{1,2},3},{{2,1},3},{{0,0},0}},{15,5,8});
    const auto x=Expression::variable(0,2),y=Expression::variable(1,2);
    if(problem->measure(Expression::binary(ExpressionKind::Add,x,y)).loss.score()!=0)return false;
    TrainingObjective objective(problem,32);
    ConstantFitConfig fit;fit.maxLossAttempts=8;
    const auto fitted=fitConstants(Expression::constant(1,2),objective,fit);
    if(problem->measure(fitted.expression).loss!=fitted.loss)return false;
    SymbolicRegressionConfig cfg;cfg.population=4;cfg.elites=1;cfg.maxGenerationAttempts=4;
    cfg.maxEvaluations=20;cfg.maxTrainingLossAttempts=64;cfg.fitting=fit;cfg.fitEveryGenerations=4;
    RandomContext run(42,{2});const auto result=symbolicRegression(problem,cfg,run);
    return result.best && result.trainingWork.lossAttempts<=64 &&
        problem->measure(result.best->expression()).loss==result.best->evaluation();
}

bool typed_check() {
    const std::array schema{TypedShape::matrix(2,3),TypedShape::vector(3)};
    const auto f=TypedExpression::binary(TypedExpressionKind::MatVec,
        TypedExpression::variable(0,schema),TypedExpression::variable(1,schema));
    const std::array bindings{TypedValue::matrix(2,3,std::array{1.,2.,3.,-1.,0.,4.}),
                              TypedValue::vector(std::array{2.,-1.,3.})};
    const auto value=f.evaluate(bindings);
    auto p=TypedRegressionProblem::create(schema,TypedShape::vector(2),{{{bindings[0],bindings[1]},TypedValue::vector(std::array{9.,10.})}});
    TypedSymbolicRegressionConfig config;config.population=4;config.elites=1;config.maxGenerationAttempts=2;
    config.maxEvaluations=12;config.maxTrainingLossAttempts=64;config.fitting=ConstantFitConfig{};config.fitting->maxLossAttempts=4;config.fitEveryGenerations=2;
    RandomContext random(8,{2});auto result=typedSymbolicRegression(p,config,random);
    if(!result.best || p->measure(result.best->expression()).loss!=result.best->evaluation())return false;
    return value.valid() && value.value().at(0)==9 && value.value().at(1)==10 &&
        value.arithmeticOperations()==10 && f.schemaString()=="schema{x0:matrix[2,3],x1:vector[3]}";
}
