#include "common.hpp"
// Overlapping complete public-header use in a separately compiled translation unit.
bool symbolic_check() {
    if(formatExpression(parseExpression("log(x0)",1),ExpressionTextFormat::LaTeX)!="\\ln\\left(x_{0}\\right)")return false;
    auto ip=IntegralRegressionProblem::create(1,parseExpression("0",1),parseExpression("x0",1),{{{1},1.25},{{2},4}}, {9,4,8});
    IntegralTrainingObjective io(ip,32);ConstantFitConfig iff;iff.maxLossAttempts=8;
    auto ifit=fitConstants(parseExpression("1*x1+1",2),io,iff);
    if(ip->measure(ifit.expression).loss!=ifit.loss)return false;
    SymbolicRegressionConfig ic;ic.population=4;ic.elites=1;ic.maxEvaluations=8;ic.maxTrainingLossAttempts=8;ic.maxGenerationAttempts=1;
    RandomContext ir(2,{2});if(!integralRegression(ip,ic,ir).search.best)return false;
    auto dp=DerivativeRegressionProblem::create(1,{{0,{{{1},2}},1,1}});
    DerivativeTrainingObjective dobj(dp,8);ConstantFitConfig df;df.maxLossAttempts=8;
    if(fitConstants(parseExpression("1*x0",1),dobj,df).loss!=Fitness::valid(0))return false;
    if(evaluatePartial(parseExpression("exp(x0)",1),std::array{0.},0).value()!=1)return false;
    auto templateData=RegressionProblem::create({{{1},2},{{2},3}},{3,2,8});
    auto templateDomain=ScalarTemplateProblem::create(ScalarExpressionTemplate(Expression::constant(0,1),0,{3,2,8},{3,2,8}),templateData,{0},{ExpressionKind::Add});
    SymbolicRegressionConfig tc;tc.grammar.operators={ExpressionKind::Add};tc.population=4;tc.elites=1;tc.maxGenerationAttempts=1;tc.maxEvaluations=8;tc.maxTrainingLossAttempts=32;
    RandomContext tr(2,{2});if(!scalarTemplateRegression(templateDomain,tc,tr).best)return false;
    const auto problem=RegressionProblem::create({{{1,2},3},{{2,1},3},{{0,0},0}},{15,5,8});
    const auto x=Expression::variable(0,2),y=Expression::variable(1,2);
    if(Expression::unary(ExpressionKind::Exp,x).evaluate(std::array{0.,0.}).value()!=1)return false;
    if(Expression::power(x,3).evaluate(std::array{-2.,0.}).value()!=-8)return false;
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
    if(parseTypedExpression("dot(vector[2](1,2),vector[2](3,4))",{},TypedShape::scalar()).evaluate({}).value().scalarValue()!=11)return false;
    const std::array scalarSchema{TypedShape::scalar()};
    const auto scalar=TypedExpression::variable(0,scalarSchema);
    const auto root=TypedExpression::unary(TypedExpressionKind::Sqrt,TypedExpression::power(scalar,2));
    if(root.evaluate(std::array{TypedValue::scalar(-3)}).value().scalarValue()!=3)return false;
    const std::array schema{TypedShape::matrix(2,3),TypedShape::vector(3)};
    const auto product=TypedExpression::binary(TypedExpressionKind::MatVec,
        TypedExpression::variable(0,schema),TypedExpression::variable(1,schema));
    const auto f=TypedExpression::binary(TypedExpressionKind::Multiply,
        TypedExpression::unary(TypedExpressionKind::Exp,TypedExpression::constant(TypedValue::scalar(0),schema)),product);
    const std::array bindings{TypedValue::matrix(2,3,std::array{1.,2.,3.,-1.,0.,4.}),
                              TypedValue::vector(std::array{2.,-1.,3.})};
    const auto value=f.evaluate(bindings);
    auto p=TypedRegressionProblem::create(schema,TypedShape::vector(2),{{{bindings[0],bindings[1]},TypedValue::vector(std::array{9.,10.})}});
    TypedSymbolicRegressionConfig config;config.population=4;config.elites=1;config.maxGenerationAttempts=2;
    config.maxEvaluations=12;config.maxTrainingLossAttempts=64;config.fitting=ConstantFitConfig{};config.fitting->maxLossAttempts=4;config.fitEveryGenerations=2;
    RandomContext random(8,{2});auto result=typedSymbolicRegression(p,config,random);
    if(!result.best || p->measure(result.best->expression()).loss!=result.best->evaluation())return false;
    return value.valid() && value.value().at(0)==9 && value.value().at(1)==10 &&
        value.arithmeticOperations()==13 && f.schemaString()=="schema{x0:matrix[2,3],x1:vector[3]}";
}
