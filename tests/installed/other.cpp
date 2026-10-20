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
