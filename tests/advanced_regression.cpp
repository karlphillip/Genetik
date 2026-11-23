#include <ScientificLevenbergMarquardt.hpp>
#include <ExpressionParsing.hpp>
#include <ConstantFitting.hpp>
#include <array>
#include <iostream>
#include <tuple>

namespace {
void check(bool x,const char* m){if(!x)throw std::runtime_error(m);}
void near(double x,double y,double tol=2e-8){if(!std::isfinite(x)||std::abs(x-y)>tol*std::max(1.0,std::abs(y)))throw std::runtime_error("numeric mismatch: "+std::to_string(x)+" vs "+std::to_string(y));}
Expression parse(std::string_view s,std::size_t n){return parseExpression(s,n,{},ExpressionLimits{128,32,32,10000});}
void derivatives(){
    const std::array<std::size_t,3> ids{1,2,3};
    auto e=Expression::binary(ExpressionKind::Multiply,Expression::variable(1,4),Expression::unary(ExpressionKind::Sin,Expression::binary(ExpressionKind::Add,Expression::binary(ExpressionKind::Multiply,Expression::variable(2,4),Expression::variable(0,4)),Expression::variable(3,4))));
    for(auto [x,a,b,c]:{std::tuple{.4,1.3,.8,-.2},std::tuple{1.2,-.7,1.1,.3}}){std::array<double,4> q{x,a,b,c};auto d=evaluateParameterJacobian(e,q,ids);check(d.status==ParameterJacobianStatus::Success,"sine parameter Jacobian");near(d.value,a*std::sin(b*x+c));near(d.derivatives[0],std::sin(b*x+c));near(d.derivatives[1],a*x*std::cos(b*x+c));near(d.derivatives[2],a*std::cos(b*x+c));}
    std::array<std::size_t,1> one{1};auto shared=Expression::binary(ExpressionKind::Multiply,Expression::variable(1,2),Expression::unary(ExpressionKind::Sin,Expression::binary(ExpressionKind::Multiply,Expression::variable(1,2),Expression::variable(0,2))));std::array<double,2> q{.7,1.2};auto d=evaluateParameterJacobian(shared,q,one);near(d.derivatives[0],std::sin(q[0]*q[1])+q[0]*q[1]*std::cos(q[0]*q[1]));
    std::array<std::size_t,2> ab{1,2};auto x=Expression::variable(0,3),a=Expression::variable(1,3),b=Expression::variable(2,3);auto pow=Expression::binary(ExpressionKind::Multiply,a,Expression::binary(ExpressionKind::Pow,x,b));for(double xx:{.4,1.7}){std::array<double,3> v{xx,2.1,1.4};auto j=evaluateParameterJacobian(pow,v,ab);near(j.derivatives[0],std::pow(xx,1.4));near(j.derivatives[1],2.1*std::pow(xx,1.4)*std::log(xx));}
    auto exp=parse("x1*exp((-1)*x2*x0)",3);for(double x:{.1,1.2}){std::array<double,3> v{x,2.2,.6};auto j=evaluateParameterJacobian(exp,v,ab);near(j.derivatives[0],std::exp(-v[2]*x));near(j.derivatives[1],-v[1]*x*std::exp(-v[2]*x));}
    auto rational=Expression::binary(ExpressionKind::Divide,a,Expression::binary(ExpressionKind::Add,Expression::constant(1,3),Expression::binary(ExpressionKind::Multiply,b,Expression::power(x,2))));for(double xx:{-.4,1.3}){std::array<double,3> v{xx,1.7,.8};auto j=evaluateParameterJacobian(rational,v,ab);const double den=1+v[2]*xx*xx;near(j.derivatives[0],1/den);near(j.derivatives[1],-v[1]*xx*xx/(den*den));}
    // Boundary, singular denominator, nonfinite primal, and explicit resource stop.
    auto boundary=Expression::binary(ExpressionKind::Multiply,a,Expression::unary(ExpressionKind::Sin,x));auto atZero=evaluateParameterJacobian(boundary,std::array{0.,2.,0.},one);check(atZero.status==ParameterJacobianStatus::Success,"zero boundary remains differentiable");near(atZero.derivatives[0],0);near(atZero.value,0);
    auto singular=evaluateParameterJacobian(rational,std::array{1.,1.,-1.},ab);check(singular.status==ParameterJacobianStatus::PrimalInvalidity,"zero rational denominator is primal invalidity");
    auto overflow=parse("exp(x0)",1);auto nonfinite=evaluateParameterJacobian(overflow,std::array{1000.},std::array<std::size_t,1>{0});check(nonfinite.status==ParameterJacobianStatus::PrimalInvalidity,"nonfinite primal intermediate is rejected");
    DerivativeLimits tiny;tiny.maxOperations=1;auto exhausted=evaluateParameterJacobian(boundary,std::array{.2,2.,0.},one,tiny);check(exhausted.status==ParameterJacobianStatus::ResourceLimit&&exhausted.failedParameter==0,"Jacobian work exhaustion is explicit");
    auto nonsmooth=Expression::binary(ExpressionKind::Pow,Expression::constant(-2,2),Expression::variable(1,2));check(evaluateParameterJacobian(nonsmooth,std::array{1.,2.},one).status==ParameterJacobianStatus::Unavailable,"unsupported parameter derivative is unavailable");
    check(evaluateParameterJacobian(parse("log(x0)",1),std::array{-1.},std::array<std::size_t,1>{0}).status==ParameterJacobianStatus::PrimalInvalidity,"invalid domain is primal invalidity");
}
void scalarFit(){
    LMResidualFunction outward = [](std::span<const double> t, bool jac, std::size_t) {
        LMResidualEvaluation e;
        e.valid = true;
        e.residuals = {t[0] - 2};
        if (jac)
            e.jacobian = {1};
        return e;
    };
    LMConfig legacyConfig;
    auto legacyBoundary =
        levenbergMarquardt(std::array{0.}, std::array{-1.}, std::array{1.}, outward, legacyConfig);
    auto projectedConfig = legacyConfig;
    projectedConfig.projectStepsToBounds = true;
    auto projectedBoundary = levenbergMarquardt(std::array{0.}, std::array{-1.}, std::array{1.},
                                                outward, projectedConfig);
    check(legacyBoundary.work.rejectedSteps > 0 && projectedBoundary.work.rejectedSteps == 0 &&
              std::abs(projectedBoundary.parameters[0] - 1) < 1e-12,
          "bounded-step projection is opt-in and legacy LM retains rejected-trial behavior");

    // Reproduces the post-acceptance Jacobian budget hole: the initial
    // Jacobian and successful trial consume the entire objective allowance.
    std::size_t boundedCalls=0;
    LMResidualFunction boundedObjective=[&](std::span<const double> t,bool jac,std::size_t){
        ++boundedCalls;LMResidualEvaluation e;e.valid=true;e.residuals={t[0]-2};if(jac)e.jacobian={1};return e;
    };
    LMConfig twoCalls;twoCalls.maxObjectiveEvaluations=2;twoCalls.maxJacobianEvaluations=4;
    auto budgeted=levenbergMarquardt(std::array{0.},std::array{-4.},std::array{4.},boundedObjective,twoCalls);
    check(boundedCalls==2&&budgeted.work.objectiveEvaluations==2&&budgeted.work.jacobianEvaluations==1&&budgeted.stop==LMStop::EvaluationLimit,
          "LM objective budget includes Jacobians after accepted trials");

    // A callback that reports more mathematical work than its allowance must
    // stop at the initial evaluation without another invocation.
    std::size_t overBudgetCalls=0;
    LMResidualFunction overBudget=[&](std::span<const double>,bool jac,std::size_t allowance){
        ++overBudgetCalls;LMResidualEvaluation e;e.valid=true;e.residuals={1};if(jac)e.jacobian={1};e.mathematicalOperations=allowance+1;return e;
    };
    LMConfig tinyMath;tinyMath.maxMathematicalOperations=5;
    auto exhaustedInitially=levenbergMarquardt(std::array{0.},std::array{-4.},std::array{4.},overBudget,tinyMath);
    check(overBudgetCalls==1&&exhaustedInitially.work.objectiveEvaluations==1&&exhaustedInitially.work.jacobianEvaluations==1&&exhaustedInitially.stop==LMStop::EvaluationLimit,
          "LM preserves initial mathematical resource exhaustion");

    // A genuine failure in a later Jacobian must retain InvalidStart.
    std::size_t jacobianCalls=0;
    LMResidualFunction failedJacobian=[&](std::span<const double> t,bool jac,std::size_t){
        LMResidualEvaluation e;++jacobianCalls;
        if(jac&&jacobianCalls>1)return e;
        e.valid=true;e.residuals={t[0]-2};if(jac)e.jacobian={1};return e;
    };
    auto invalidAfterStep=levenbergMarquardt(std::array{0.},std::array{-4.},std::array{4.},failedJacobian);
    check(invalidAfterStep.stop==LMStop::InvalidStart&&invalidAfterStep.work.acceptedSteps==1&&invalidAfterStep.parameters[0]>0,
          "LM preserves failed subsequent Jacobian status and accepted parameters");

    // Invalid trial work is private: it must not replace the accepted start.
    LMResidualFunction invalidTrial=[](std::span<const double> t,bool jac,std::size_t){
        LMResidualEvaluation e;if(jac){e.valid=true;e.residuals={t[0]-2};e.jacobian={1};}return e;
    };
    auto rolledBack=levenbergMarquardt(std::array{0.},std::array{-4.},std::array{4.},invalidTrial);
    check(rolledBack.parameters==std::vector<double>{0.}&&rolledBack.sumSquaredResiduals==4.0&&rolledBack.work.acceptedSteps==0,
          "LM failed trial preserves accepted parameter and loss ownership");

    auto x=Expression::variable(0,3),a=Expression::variable(1,3),b=Expression::variable(2,3);auto e=Expression::binary(ExpressionKind::Multiply,a,Expression::binary(ExpressionKind::Pow,x,b));std::vector<LMExpressionObservation> rows;for(int i=1;i<=30;++i){double xx=.2+i*.1;rows.push_back({{xx,0,0},3.5*std::pow(xx,1.7)});}std::array<double,2> initial{2.,1.2},lo{.1,.1},hi{8.,3.};LMConfig c;c.maxIterations=100;c.maxObjectiveEvaluations=300;c.maxJacobianEvaluations=3000;c.lossTolerance=1e-22;auto result=levenbergMarquardt(initial,lo,hi,expressionLeastSquares(e,rows,{1,2}),c);check(result.sumSquaredResiduals&&*result.sumSquaredResiduals<1e-17,"LM recovers fractional-power coefficients");near(result.parameters[0],3.5,1e-7);near(result.parameters[1],1.7,1e-7);check(result.work.derivativeEvaluations>0&&result.work.linearSolves>0&&result.work.acceptedSteps>0,"LM work accounting populated");
    auto constantModel=Expression::binary(ExpressionKind::Multiply,Expression::constant(2,1),Expression::binary(ExpressionKind::Pow,Expression::variable(0,1),Expression::constant(1.2,1)));auto problem=RegressionProblem::create({{{.5},3.5*std::pow(.5,1.7)},{{1.},3.5},{{2.},3.5*std::pow(2.,1.7)},{{3.},3.5*std::pow(3.,1.7)}},{31,8,8});TrainingObjective objective(problem,100);ConstantFitConfig fit;fit.method=ConstantFitMethod::LevenbergMarquardt;fit.maxLossAttempts=50;fit.levenbergMarquardt.maxIterations=50;fit.levenbergMarquardt.maxObjectiveEvaluations=50;fit.levenbergMarquardt.maxJacobianEvaluations=500;fit.levenbergMarquardt.lossTolerance=1e-22;auto trained=fitConstants(constantModel,objective,fit);check(trained.loss.state()==FitnessState::Valid&&trained.loss.score()<1e-17&&trained.optimizerWork.acceptedSteps>0,"LM opt-in constant fitter integrates with TrainingObjective");
    LMResidualFunction deficient=[](std::span<const double> t,bool jac,std::size_t){LMResidualEvaluation e;e.valid=true;for(double x:{1.,2.,3.}){e.residuals.push_back((t[0]+t[1])*x-2*x);if(jac){e.jacobian.push_back(x);e.jacobian.push_back(x);}}e.rowsProcessed=3;e.forwardEvaluations=3;return e;};std::array<double,2> q0{.5,.5},lo2{-4,-4},hi2{4,4};auto singular=levenbergMarquardt(q0,lo2,hi2,deficient);check(singular.stop==LMStop::SingularSystem&&singular.work.linearAlgebraOperations>0,"rank-deficient parameters rejected explicitly before damping");
    LMResidualFunction oversized=[](std::span<const double>,bool jac,std::size_t){LMResidualEvaluation e;e.valid=true;e.residuals={1};if(jac)e.jacobian={1};e.workspaceBytes=4096;return e;};auto memoryConfig=LMConfig{};memoryConfig.maxWorkspaceBytes=128;auto bounded=levenbergMarquardt(std::array{1.},std::array{-2.},std::array{2.},oversized,memoryConfig);check(bounded.stop==LMStop::WorkspaceLimit,"workspace ceiling is an explicit LM stop");
}
void weightedScalarFit(){
    auto fitConfig=[] { ConstantFitConfig c;c.method=ConstantFitMethod::LevenbergMarquardt;c.lower=-20;c.upper=20;c.maxLossAttempts=100;c.levenbergMarquardt.maxIterations=100;c.levenbergMarquardt.lossTolerance=1e-24;return c; };
    auto weightedProblem=[](const std::vector<RegressionRow>& rows,std::vector<double> weights){RegressionObjectiveConfig o;o.loss=RegressionLoss::WeightedMeanSquaredError;o.observationWeights=std::move(weights);return RegressionProblem::createWithObjective(rows,std::move(o),{64,16,100});};
    auto constant=Expression::constant(0,1);
    auto runConstant=[&](std::vector<double> weights){auto p=weightedProblem({{{0},0},{{0},10}},std::move(weights));TrainingObjective objective(p,1000);return fitConstants(constant,objective,fitConfig());};
    const auto equal=runConstant({1,1}), weighted=runConstant({9,1}), scaled=runConstant({90,10});
    near(equal.expression.nodes()[0].constantValue(),5);near(weighted.expression.nodes()[0].constantValue(),1);near(weighted.loss.score(),9);near(scaled.expression.nodes()[0].constantValue(),1);near(scaled.loss.score(),9);
    auto comparisonProblem=weightedProblem({{{0},0},{{0},10}},{9,1});auto coordinateConfig=fitConfig();coordinateConfig.method=ConstantFitMethod::Coordinate;TrainingObjective coordinateObjective(comparisonProblem,1000);auto coordinate=fitConstants(constant,coordinateObjective,coordinateConfig);
    near(coordinate.expression.nodes()[0].constantValue(),1,1e-6);near(coordinate.loss.score(),9,1e-10);
    std::cout<<"weighted constant comparison: coordinate c="<<coordinate.expression.nodes()[0].constantValue()<<" mse="<<coordinate.loss.score()<<" loss_attempts="<<coordinate.lossAttempts<<" proposals="<<coordinate.proposals<<"; LM c="<<weighted.expression.nodes()[0].constantValue()<<" mse="<<weighted.loss.score()<<" objective_evaluations="<<weighted.optimizerWork.objectiveEvaluations<<" rows="<<weighted.optimizerWork.rowsProcessed<<'\n';

    auto affine=Expression::binary(ExpressionKind::Add,
        Expression::binary(ExpressionKind::Multiply,Expression::constant(0,1),Expression::variable(0,1)),Expression::constant(0,1));
    auto affineProblem=weightedProblem({{{0},0},{{1},0},{{2},6}},{1,2,3});TrainingObjective affineObjective(affineProblem,1000);
    auto affineFit=fitConstants(affine,affineObjective,fitConfig());near(affineFit.expression.nodes()[0].constantValue(),3.6);near(affineFit.expression.nodes()[3].constantValue(),-1.8);near(affineFit.loss.score(),1.8);
    double independent=0;const auto weights=affineProblem->objective().observationWeights;const std::array<double,3> xs{0,1,2},ys{0,0,6};double total=0;
    for(std::size_t i=0;i<xs.size();++i){const auto prediction=affineFit.expression.evaluate(std::array{xs[i]});check(prediction.valid(),"weighted affine returned prediction valid");const double r=prediction.value()-ys[i];independent+=weights[i]*r*r;total+=weights[i];}near(independent/total,affineFit.loss.score());

    // Nonlinear dependence on the fitted exponent exercises the public fitting API.
    auto exponent=Expression::binary(ExpressionKind::Pow,Expression::variable(0,1),Expression::constant(1,1));
    auto nonlinearProblem=weightedProblem({{{1},1},{{2},4},{{3},9}},{1,2,3});TrainingObjective nonlinearObjective(nonlinearProblem,1000);
    auto nonlinear=fitConstants(exponent,nonlinearObjective,fitConfig());near(nonlinear.expression.nodes()[1].constantValue(),2,1e-6);check(nonlinear.loss.score()<1e-12,"nonlinear weighted coefficient fit");

    // Weighted scoring still requires every row to have a valid prediction.
    auto invalidZeroRow=Expression::binary(ExpressionKind::Pow,Expression::variable(0,1),Expression::constant(.5,1));
    auto zeroProblem=weightedProblem({{{-1},1},{{4},2}},{0,1});check(zeroProblem->measure(invalidZeroRow).loss.state()==FitnessState::Invalid,"zero weight does not waive prediction validity");
    TrainingObjective zeroObjective(zeroProblem,100);auto invalidFit=fitConstants(invalidZeroRow,zeroObjective,fitConfig());check(invalidFit.stop==ConstantFitStop::InvalidStart,"LM preserves invalid zero-weight prediction contract");

    // A valid zero-weight row is still evaluated for scoring, but its derivative
    // cannot constrain the fit (abs is nondifferentiable at the initial c=0 row).
    auto absModel=Expression::unary(ExpressionKind::Abs,Expression::binary(ExpressionKind::Add,Expression::variable(0,1),Expression::constant(0,1)));
    auto zeroWeightFitProblem=weightedProblem({{{0},0},{{1},3}},{0,1});TrainingObjective zeroWeightFitObjective(zeroWeightFitProblem,100);
    auto zeroWeightFit=fitConstants(absModel,zeroWeightFitObjective,fitConfig());near(zeroWeightFit.expression.nodes()[1].constantValue(),2,1e-7);near(zeroWeightFit.loss.score(),0,1e-12);

    // The small positive weight remains representable after square-root scaling,
    // even though weight/total underflows to zero in binary64.
    const double hugeTarget=std::ldexp(1.0,500),largeWeight=std::ldexp(1.0,600),smallWeight=std::ldexp(1.0,-600);
    auto extremeProblem=weightedProblem({{{0},0},{{0},hugeTarget}},{largeWeight,smallWeight});auto extremeScore=extremeProblem->measure(constant).loss;
    check(extremeScore.score()==std::ldexp(1.0,-200),"extreme weighted public score is exact");
    auto scaledRows=expressionLeastSquares(Expression::variable(0,1),{{{0},0},{{0},hugeTarget}},{0},100,{largeWeight,smallWeight});
    auto scaledResiduals=scaledRows(std::array{0.0},false,100);
    check(scaledResiduals.valid&&scaledResiduals.residuals.size()==2,"extreme weights produce valid scaled residuals");
    const double tinyScaledResidual=-std::ldexp(1.0,-100);
    check(std::abs(scaledResiduals.residuals[1]-tinyScaledResidual)<=std::abs(tinyScaledResidual)*1e-12,"tiny weighted residual remains nonzero at its expected scale");
    TrainingObjective extremeObjective(extremeProblem,100);auto extremeFit=fitConstants(constant,extremeObjective,fitConfig());
    near(extremeFit.loss.score(),extremeScore.score(),1e-12);near(extremeFit.loss.score(),extremeProblem->measure(extremeFit.expression).loss.score(),1e-12);
    check(!extremeFit.improved&&extremeFit.expression.nodes()[0].constantValue()==0,"unchanged weighted LM model cannot report improvement");

    // Score tracking is bounded while objective calls grow, including a strict workspace cap.
    auto square=Expression::binary(ExpressionKind::Pow,Expression::constant(1,1),Expression::constant(2,1));
    auto runSquare=[&](bool weightedObjective){RegressionObjectiveConfig objectiveConfig;if(weightedObjective){objectiveConfig.loss=RegressionLoss::WeightedMeanSquaredError;objectiveConfig.observationWeights={1};}auto p=RegressionProblem::createWithObjective({{{0},0}},objectiveConfig,{64,16,100});TrainingObjective o(p,1000);auto c=fitConfig();c.levenbergMarquardt.maxWorkspaceBytes=1024;c.levenbergMarquardt.maxObjectiveEvaluations=200;c.levenbergMarquardt.maxIterations=100;c.levenbergMarquardt.lossTolerance=1e-24;return fitSelectedConstants(square,o,std::array<std::size_t,1>{0},c);};
    auto unweightedSquare=runSquare(false),weightedSquare=runSquare(true);
    for(const auto* fitted:{&unweightedSquare,&weightedSquare}){check(fitted->loss.state()==FitnessState::Valid&&fitted->optimizerWork.peakWorkspaceBytes<=1024,"c-squared fit respects 1,024-byte LM workspace cap");check(fitted->optimizerWork.objectiveEvaluations>2,"score tracking remains bounded across repeated objective evaluations");near(fitted->loss.score(),std::pow(fitted->expression.nodes()[0].constantValue()*fitted->expression.nodes()[0].constantValue(),2),1e-12);}
    // The last trial is worse and exhausts the objective budget; returned loss stays with c=1.
    auto rejectedProblem=RegressionProblem::create({{{0},10}},{64,16,100});TrainingObjective rejectedObjective(rejectedProblem,100);auto rejectedConfig=fitConfig();rejectedConfig.levenbergMarquardt.maxObjectiveEvaluations=2;rejectedConfig.levenbergMarquardt.maxIterations=10;auto rejectedFit=fitSelectedConstants(square,rejectedObjective,std::array<std::size_t,1>{0},rejectedConfig);check(rejectedFit.optimizerWork.objectiveEvaluations==2&&rejectedFit.optimizerWork.rejectedSteps==1,"final LM trial is rejected at the budget boundary");near(rejectedFit.loss.score(),rejectedProblem->measure(rejectedFit.expression).loss.score());near(rejectedFit.expression.nodes()[0].constantValue(),1);

    // The ordinary symbolic-regression preparation path uses the same weighted LM objective.
    SymbolicRegressionConfig search;search.population=4;search.elites=1;search.maxGenerationAttempts=1;search.maxEvaluations=8;search.maxTrainingLossAttempts=1000;search.initialExpressionSlots=1;search.maximumInitializerWorkUnits=1;search.fitting=fitConfig();search.fitting->levenbergMarquardt.maxObjectiveEvaluations=20;search.fitting->levenbergMarquardt.maxJacobianEvaluations=20;
    auto symbolicProblem=weightedProblem({{{0},0},{{1},2},{{2},4}},{1,2,1});RandomContext random(782);
    auto symbolic=symbolicRegression(symbolicProblem,search,random,{},[](const RegressionProblem&,std::size_t,std::uint64_t){InitialExpressionBatch b;b.expressions={Expression::binary(ExpressionKind::Multiply,Expression::constant(0,1),Expression::variable(0,1))};b.workUnits=1;return b;});
    check(symbolic.best&&symbolic.optimizerWork.objectiveEvaluations>0,"symbolic regression invokes weighted LM refinement");
}
void coupled(){
    auto xdot=parse("x2",5),vdot=parse("((-1)*x3*x2)-x4*x1",5);std::vector<ScientificObservation> dx,dv;for(double x:{-2.,-.5,1.,2.})for(double v:{-1.,0.,1.}){dx.push_back({{0,x,v},v});dv.push_back({{0,x,v},-.25*v-2*x});}
    ScientificEquationSystem s(3,{xdot,vdot},{{0,1,1,dx},{1,2,1,dv}},{{1,0,4},{1.4,0,4}});LMConfig c;c.maxIterations=100;c.maxObjectiveEvaluations=500;c.maxJacobianEvaluations=500;c.lossTolerance=1e-22;auto r=fitScientificParametersLevenbergMarquardt(s,c);check(r.sumSquaredResiduals&&*r.sumSquaredResiduals<1e-16,"D6 coupled-system LM loss");near(r.parameters[0],.25,1e-7);near(r.parameters[1],2,1e-7);
}
void continuation(){
    std::vector<RegressionRow> rows;for(int i=1;i<=8;++i){double x=.2*i;rows.push_back({{x},2.5*x*x-.75*x+.4});}
    auto problem=ScalarMathematicsProblem::scientific(rows,{31,8,8});SymbolicRegressionConfig config;config.population=6;config.elites=1;config.maxGenerationAttempts=4;config.maxEvaluations=30;config.maxTrainingLossAttempts=10000;config.grammar.operators={ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply};config.grammar.continuousProbability=.5;config.grammar.continuousLower=-3;config.grammar.continuousUpper=3;config.fitting=ConstantFitConfig{};config.fitting->method=ConstantFitMethod::LevenbergMarquardt;config.fitting->maxLossAttempts=10;config.fitting->levenbergMarquardt.maxIterations=8;config.fitting->levenbergMarquardt.maxObjectiveEvaluations=8;config.fitting->levenbergMarquardt.maxJacobianEvaluations=8;config.fitting->levenbergMarquardt.maxMathematicalOperations=100000;
    ExecutionContext execution({2});RandomContext random(991,execution);ScalarMathematicsRun run(problem,config,random);check(run.step()==GenerationOutcome::Committed,"two-worker LM run commits generation before checkpoint");auto checkpoint=decodeCheckpoint(encodeCheckpoint(run.checkpoint()));check(checkpoint.codec=="genetik.scalar-mathematics-evolution/2","LM run uses extended accounting checkpoint codec");while(!run.stoppingReason())check(run.step()==GenerationOutcome::Committed,"continuous two-worker LM generation committed");
    RandomContext resumedRandom(991,execution);auto resumed=ScalarMathematicsRun::restore(checkpoint,problem,config,resumedRandom);while(!resumed->stoppingReason())check(resumed->step()==GenerationOutcome::Committed,"resumed two-worker LM generation committed");auto left=run.population().creatures(),right=resumed->population().creatures();check(left->size()==right->size(),"continuous/resumed LM population size");for(std::size_t i=0;i<left->size();++i)check((*left)[i].text()==(*right)[i].text()&&(*left)[i].evaluation()==(*right)[i].evaluation(),"continuous/resumed LM candidate equality");check(run.work()==resumed->work()&&run.supplementalWork()==resumed->supplementalWork()&&run.preparationWork()==resumed->preparationWork(),"continuous/resumed LM work equality");const auto& lw=run.optimizerWork();const auto& rw=resumed->optimizerWork();check(lw.objectiveEvaluations==rw.objectiveEvaluations&&lw.jacobianEvaluations==rw.jacobianEvaluations&&lw.forwardEvaluations==rw.forwardEvaluations&&lw.derivativeEvaluations==rw.derivativeEvaluations&&lw.rowsProcessed==rw.rowsProcessed&&lw.parameterUpdates==rw.parameterUpdates&&lw.linearSolves==rw.linearSolves&&lw.acceptedSteps==rw.acceptedSteps&&lw.rejectedSteps==rw.rejectedSteps&&lw.mathematicalOperations==rw.mathematicalOperations&&lw.linearAlgebraOperations==rw.linearAlgebraOperations&&lw.peakWorkspaceBytes==rw.peakWorkspaceBytes,"continuous/resumed LM optimizer counters");
    auto wrong=config;wrong.fitting->method=ConstantFitMethod::Coordinate;RandomContext wrongRandom(991,execution);bool rejected=false;try{(void)ScalarMathematicsRun::restore(checkpoint,problem,wrong,wrongRandom);}catch(const CheckpointError&){rejected=true;}check(rejected,"checkpoint rejects optimizer-policy change");
    auto projectedConfig = config;
    projectedConfig.fitting->levenbergMarquardt.projectStepsToBounds = true;
    RandomContext projectedRandom(991, execution);
    ScalarMathematicsRun projectedRun(problem, projectedConfig, projectedRandom);
    check(projectedRun.step() == GenerationOutcome::Committed,
          "projected LM run commits before checkpoint");
    auto projectedCheckpoint = decodeCheckpoint(encodeCheckpoint(projectedRun.checkpoint()));
    RandomContext projectedResumeRandom(991, execution);
    auto projectedResume = ScalarMathematicsRun::restore(projectedCheckpoint, problem,
                                                         projectedConfig, projectedResumeRandom);
    check(projectedResume->checkpoint().identity == projectedCheckpoint.identity,
          "projected LM checkpoint resumes with the same policy");
    RandomContext changedToDefaultRandom(991, execution);
    rejected = false;
    try {
        (void)ScalarMathematicsRun::restore(projectedCheckpoint, problem, config,
                                            changedToDefaultRandom);
    } catch (const CheckpointError&) {
        rejected = true;
    }
    check(rejected, "checkpoint rejects projected-to-default policy change");
    RandomContext changedToProjectedRandom(991, execution);
    rejected = false;
    try {
        (void)ScalarMathematicsRun::restore(checkpoint, problem, projectedConfig,
                                            changedToProjectedRandom);
    } catch (const CheckpointError&) {
        rejected = true;
    }
    check(rejected, "checkpoint rejects default-to-projected policy change");
}
}
int main(){try{derivatives();scalarFit();weightedScalarFit();coupled();continuation();std::cout<<"S1 parameter derivatives and LM contracts passed\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
