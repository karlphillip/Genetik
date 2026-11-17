#include <ConstantFitting.hpp>
#include <array>
#include <atomic>
#include <cmath>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void near(double value, double expected, double tolerance, const char* message) {
    if (!std::isfinite(value) || std::abs(value - expected) > tolerance * std::max(1.0, std::abs(expected)))
        throw std::runtime_error(message);
}

struct Family {
    const char* name;
    double lo, hi;
    std::array<double, 3> truth;
    std::array<std::array<double, 3>, 3> starts;
    std::size_t parameterCount;
    std::vector<std::size_t> slots;
};

Family family(const char* name) {
    if (std::string(name) == "polynomial")
        return {name, -2, 2, {2.3, -1.4, .7}, {{{.3,.2,-.2},{-1.5,1.1,1.2},{2.8,-2.2,-1.7}}}, 3, {0,4,8}};
    if (std::string(name) == "rational")
        return {name, -1, 1, {1.2,.3,.4}, {{{.5,-.5,-.5},{2.2,1.3,.8},{-1.7,.6,-.25}}}, 3, {0,3,6}};
    if (std::string(name) == "sinusoid")
        return {name, -3, 3, {1.4,1.3,.2}, {{{.8,.7,-1},{2.4,1.8,1},{1.1,1.2,2}}}, 3, {0,1,4}};
    if (std::string(name) == "exponential")
        return {name, -1, 2, {2.2,.65,0}, {{{.6,.2,0},{3,1.2,0},{1.6,.9,0}}}, 2, {0,2}};
    throw std::invalid_argument("unknown family");
}

double target(const Family& f, double x) {
    if (std::string(f.name) == "polynomial") return f.truth[0]*x*x + f.truth[1]*x + f.truth[2];
    if (std::string(f.name) == "rational" || std::string(f.name) == "independent_rational")
        return (f.truth[0]*x + f.truth[1])/(1 + f.truth[2]*x);
    if (std::string(f.name) == "sinusoid") return f.truth[0]*std::sin(f.truth[1]*x + f.truth[2]);
    return f.truth[0]*std::exp(-f.truth[1]*x);
}

std::vector<RegressionRow> observations(const Family& f, std::size_t count, double phase) {
    std::vector<RegressionRow> rows;
    for (std::size_t i = 0; i < count; ++i) {
        const double fraction = phase == 0 ? static_cast<double>(i)/(count-1)
                                           : (static_cast<double>(i)+phase)/count;
        const double x = f.lo + (f.hi-f.lo)*fraction;
        rows.push_back({{x}, target(f,x)});
    }
    return rows;
}

Expression model(const Family& f, const std::array<double,3>& theta) {
    constexpr ExpressionLimits limits{31,6,8,2048};
    const auto x = Expression::variable(0,1);
    const auto c = [&](double v) { return Expression::constant(v,1,limits); };
    if (std::string(f.name) == "polynomial")
        return Expression::binary(ExpressionKind::Add,
            Expression::binary(ExpressionKind::Add,
                Expression::binary(ExpressionKind::Multiply,c(theta[0]),Expression::power(x,2)),
                Expression::binary(ExpressionKind::Multiply,c(theta[1]),x)),c(theta[2]));
    if (std::string(f.name) == "rational" || std::string(f.name) == "independent_rational")
        return Expression::binary(ExpressionKind::Divide,
            Expression::binary(ExpressionKind::Add,Expression::binary(ExpressionKind::Multiply,c(theta[0]),x),c(theta[1])),
            Expression::binary(ExpressionKind::Add,c(1),Expression::binary(ExpressionKind::Multiply,c(theta[2]),x)));
    if (std::string(f.name) == "sinusoid")
        return Expression::binary(ExpressionKind::Multiply,c(theta[0]),
            Expression::unary(ExpressionKind::Sin,Expression::binary(ExpressionKind::Add,
                Expression::binary(ExpressionKind::Multiply,c(theta[1]),x),c(theta[2]))));
    return Expression::binary(ExpressionKind::Multiply,c(theta[0]),
        Expression::unary(ExpressionKind::Exp,Expression::binary(ExpressionKind::Multiply,
            c(-1),Expression::binary(ExpressionKind::Multiply,c(theta[1]),x))));
}

ConstantFitConfig config() {
    ConstantFitConfig c;
    c.method=ConstantFitMethod::LevenbergMarquardt;
    c.lower=-3; c.upper=3; c.initialStep=.4; c.minimumStep=1e-9;
    c.maxLossAttempts=512; c.maxSweeps=80; c.maxProposals=4096; c.lossThreshold=1e-24;
    c.levenbergMarquardt.maxIterations=64;
    c.levenbergMarquardt.maxObjectiveEvaluations=128;
    c.levenbergMarquardt.maxJacobianEvaluations=128;
    c.levenbergMarquardt.maxMathematicalOperations=30000000;
    c.levenbergMarquardt.maxWorkspaceBytes=67108864;
    c.levenbergMarquardt.lossTolerance=1e-24;
    return c;
}

double rmse(const Expression& e, const std::vector<RegressionRow>& rows) {
    auto p=RegressionProblem::create(rows,{31,6,8,2048});
    auto metrics=p->measure(e);
    check(metrics.loss.state()==FitnessState::Valid,"held-out expression remains domain-valid");
    return std::sqrt(metrics.loss.score());
}

void checkRationalJacobian(const std::vector<RegressionRow>& rows, const std::array<double,3>& theta) {
    constexpr std::size_t schema=4;
    const auto x=Expression::variable(0,schema), a=Expression::variable(1,schema);
    const auto b=Expression::variable(2,schema), c=Expression::variable(3,schema);
    const auto one=Expression::constant(1,schema);
    auto e=Expression::binary(ExpressionKind::Divide,
        Expression::binary(ExpressionKind::Add,Expression::binary(ExpressionKind::Multiply,a,x),b),
        Expression::binary(ExpressionKind::Add,one,Expression::binary(ExpressionKind::Multiply,c,x)));
    const std::array<std::size_t,3> parameters{1,2,3};
    for (const auto& row:rows) {
        const std::array<double,4> bindings{row.inputs[0],theta[0],theta[1],theta[2]};
        const auto analytic=evaluateParameterJacobian(e,bindings,parameters);
        check(analytic.status==ParameterJacobianStatus::Success,"rational analytic Jacobian remains available");
        for (std::size_t j=0;j<3;++j) {
            constexpr double h=1e-6;
            auto plus=bindings,minus=bindings;plus[j+1]+=h;minus[j+1]-=h;
            const auto p=e.evaluate(plus),m=e.evaluate(minus);
            check(p.valid()&&m.valid(),"independent finite-difference rational probes are valid");
            const double numeric=(p.value()-m.value())/(2*h);
            near(analytic.derivatives[j],numeric,2e-8,"analytic rational derivative matches central difference");
        }
    }
}

void controls() {
    for (const char* name : {"polynomial","rational","sinusoid","exponential"}) {
        const auto f=family(name);
        for (std::size_t start=0; start<3; ++start) {
            auto train=observations(f,25,0);
            auto assessment=observations(f,19,.719);
            auto p=RegressionProblem::create(train,{31,6,8,2048});
            auto expression=model(f,f.starts[start]);
            TrainingObjective objective(p,2048);
            const auto wallBegin=std::chrono::steady_clock::now();
            const auto cpuBegin=std::clock();
            const auto fitted=fitSelectedConstants(expression,objective,f.slots,config());
            const double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-wallBegin).count();
            const double cpu=static_cast<double>(std::clock()-cpuBegin)/CLOCKS_PER_SEC;
            const auto direct=p->measure(fitted.expression).loss;
            check(fitted.loss.state()==FitnessState::Valid&&direct.state()==FitnessState::Valid,
                  "LM fixed fit returns a valid loss");
            near(fitted.loss.score(),direct.score(),2e-12,
                 "LM result loss agrees with its returned expression");
            check(fitted.optimizerStop.has_value(),"LM result retains the exact lower-level stop");
            check(fitted.lossAttempts==fitted.optimizerWork.objectiveEvaluations,
                  "LM objective accounting remains unchanged");
            std::cout<<std::setprecision(17)<<"fit\t"<<f.name<<"\t"<<start
                <<"\t"<<fitted.startingLoss.score()<<"\t"<<fitted.loss.score()
                <<"\t"<<rmse(fitted.expression,assessment)<<"\t";
            for(std::size_t i=0;i<f.parameterCount;++i)std::cout<<(i?",":"")<<fitted.expression.nodes()[f.slots[i]].constantValue();
            std::cout<<"\t"<<static_cast<int>(*fitted.optimizerStop)<<"\t"<<fitted.optimizerWork.objectiveEvaluations
                <<"\t"<<fitted.optimizerWork.jacobianEvaluations<<"\t"<<fitted.optimizerWork.derivativeEvaluations
                <<"\t"<<fitted.optimizerWork.linearAlgebraOperations<<"\t"<<fitted.optimizerWork.acceptedSteps
                <<"\t"<<fitted.optimizerWork.rejectedSteps<<"\t"<<fitted.optimizerWork.peakWorkspaceBytes
                <<"\t"<<wall<<"\t"<<cpu<<"\n";
            if (std::string(name)=="rational"&&start==0) {
                checkRationalJacobian(train,{.5,-.5,-.5});
                checkRationalJacobian(train,{.24859005981219559,.17489604445356347,1.7851251348993684});
                near(fitted.startingLoss.score(),.67082323486258344,1e-12,
                     "archived S11 rational start is reproduced");
                check(*fitted.optimizerStop==LMStop::StepTolerance,
                      "archived S11 rational stop is the lower-level step tolerance");
                near(fitted.loss.score(),.51708303388229904,2e-8,
                     "S11 rational returned loss is associated with its returned coefficients");
                check(fitted.optimizerWork.acceptedSteps==33&&fitted.optimizerWork.rejectedSteps==24,
                      "S11 rational accepted/rejected path is preserved");
            } else {
                for (std::size_t i=0;i<f.parameterCount;++i)
                    near(fitted.expression.nodes()[f.slots[i]].constantValue(),f.truth[i],1e-6,
                         "successful S11 fixed-fit coefficient recovery is preserved");
                check(rmse(fitted.expression,assessment)<=1e-6,
                      "successful S11 fixed-fit assessment recovery is preserved");
            }
        }
    }
}

void independentRational() {
    // Frozen before this correction was exercised; this is an independent
    // rational target and a non-solution start, not a retry of the S11 case.
    Family f{"independent_rational",-1,1,{.8,1.1,-.3},{},3,{0,3,6}};
    auto customRows=[](std::size_t count,bool held) {
        std::vector<RegressionRow> rows;
        for (std::size_t i=0;i<count;++i) {
            const double x=held ? -.95+1.8*static_cast<double>(i)/(count-1)
                                : -1+2*static_cast<double>(i)/(count-1);
            rows.push_back({{x},(.8*x+1.1)/(1-.3*x)});
        }
        return rows;
    };
    auto train=customRows(25,false), assessment=customRows(19,true);
    auto p=RegressionProblem::create(train,{31,6,8,2048});
    const std::array<double,3> start{-.7,.2,.5};
    auto expression=model(f,start); TrainingObjective objective(p,2048);
    const auto wallBegin=std::chrono::steady_clock::now();
    const auto cpuBegin=std::clock();
    const auto fitted=fitSelectedConstants(expression,objective,f.slots,config());
    const double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-wallBegin).count();
    const double cpu=static_cast<double>(std::clock()-cpuBegin)/CLOCKS_PER_SEC;
    near(fitted.loss.score(),p->measure(fitted.expression).loss.score(),2e-12,
         "independent rational loss matches returned expression");
    for (std::size_t i=0;i<3;++i)
        near(fitted.expression.nodes()[f.slots[i]].constantValue(),f.truth[i],1e-6,
             "independent rational coefficient recovery");
    check(rmse(fitted.expression,assessment)<=1e-6,
          "independent rational assessment recovery");
    std::cout<<std::setprecision(17)<<"independent\t"<<f.name<<"\t0\t"<<fitted.startingLoss.score()<<"\t"<<fitted.loss.score()
        <<"\t"<<rmse(fitted.expression,assessment)<<"\t";
    for(std::size_t i=0;i<3;++i)std::cout<<(i?",":"")<<fitted.expression.nodes()[f.slots[i]].constantValue();
    std::cout<<"\t"<<static_cast<int>(*fitted.optimizerStop)<<"\t"<<fitted.optimizerWork.objectiveEvaluations
        <<"\t"<<fitted.optimizerWork.jacobianEvaluations<<"\t"<<fitted.optimizerWork.derivativeEvaluations
        <<"\t"<<fitted.optimizerWork.linearAlgebraOperations<<"\t"<<fitted.optimizerWork.acceptedSteps
        <<"\t"<<fitted.optimizerWork.rejectedSteps<<"\t"<<fitted.optimizerWork.peakWorkspaceBytes
        <<"\t"<<wall<<"\t"<<cpu<<"\n";
}

void cancellation() {
    std::atomic<bool> cancelled{true};
    LMResidualFunction objective=[](std::span<const double> p,bool jac,std::size_t) {
        LMResidualEvaluation e;e.valid=true;e.residuals={p[0]-1};if(jac)e.jacobian={1};return e;
    };
    const auto result=levenbergMarquardt(std::array{0.},std::array{-2.},std::array{2.},objective,{},&cancelled);
    check(result.stop==LMStop::Cancelled&&result.work.objectiveEvaluations==0,
          "pre-requested LM cancellation remains distinct and uncharged");
}
}

int main() {
    try { std::cout<<"kind\tfamily\tstart\ttraining_start\ttraining_final\tassessment_rmse\tcoefficients\tLMStop(enum)\tobjective\tjacobian\tderivative\tlinear_algebra\taccepted\trejected\tlogical_workspace_bytes\twall_seconds\tcpu_seconds\n"; controls(); independentRational(); cancellation(); }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
