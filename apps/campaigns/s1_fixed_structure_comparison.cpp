#include <ExpressionParsing.hpp>
#include <ScientificLevenbergMarquardt.hpp>
#include <ScientificRegression.hpp>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <chrono>
#include <cmath>
#include <ctime>

namespace {
using K=ExpressionKind;
Expression x(std::size_t i,std::size_t n){return Expression::variable(i,n);}
Expression c(double v,std::size_t n){return Expression::constant(v,n);}
Expression b(K k,Expression l,Expression r){return Expression::binary(k,l,r);}
Expression u(K k,Expression a){return Expression::unary(k,a);}
struct Task {std::string name;std::vector<std::size_t> fit;std::vector<std::vector<double>> starts;std::vector<double> truth;Expression model;std::function<double(double)> target;};
Task task(std::string name,std::vector<double> truth,std::vector<std::vector<double>> starts,std::function<double(double)> target){
    constexpr std::size_t n=1;auto xx=x(0,n);Expression model=Expression::constant(0,n);
    if(name=="fractional-power")model=b(K::Multiply,c(truth[0],n),b(K::Pow,xx,c(truth[1],n)));
    else if(name=="trigonometric")model=b(K::Multiply,c(truth[0],n),u(K::Sin,b(K::Add,b(K::Multiply,c(truth[1],n),xx),c(truth[2],n))));
    else if(name=="exponential")model=b(K::Multiply,c(truth[0],n),u(K::Exp,b(K::Multiply,b(K::Multiply,c(-1,n),c(truth[1],n)),xx)));
    else model=b(K::Divide,b(K::Add,xx,c(truth[0],n)),b(K::Add,Expression::power(xx,2),c(truth[1],n)));
    std::vector<std::size_t> indices;for(std::size_t i=0;i<model.nodeCount();++i)if(model.nodes()[i].kind()==K::Constant&&model.nodes()[i].constantValue()!=-1)indices.push_back(i);
    return {std::move(name),std::move(indices),std::move(starts),std::move(truth),std::move(model),std::move(target)};
}
std::vector<RegressionRow> scalarRows(const Task&t,bool held){std::vector<RegressionRow> rows;const int count=held?15:30;for(int i=0;i<count;++i){double q=held?i+.5:i;double v;if(t.name=="fractional-power")v=held?.25+.2*q:.2+.1*q;else if(t.name=="trigonometric")v=held?-1.925+.3*i:-2+.15*i;else if(t.name=="exponential")v=held?.1+.2*q:.05+.1*q;else v=held?-1.925+.3*i:-2+.15*i;rows.push_back({{v},t.target(v)});}return rows;}
double rmse(const Expression&e,const std::vector<RegressionRow>& rows){double sum=0;for(const auto&r:rows){auto y=e.evaluate(r.inputs);if(!y.valid())return std::numeric_limits<double>::infinity();double d=y.value()-r.target;sum+=d*d;}return std::sqrt(sum/rows.size());}
std::string coeffs(const Expression&e,std::span<const std::size_t> indexes){std::ostringstream o;o<<std::setprecision(17)<<'[';for(std::size_t j=0;j<indexes.size();++j){if(j)o<<',';o<<e.nodes()[indexes[j]].constantValue();}return o.str()+']';}
ScientificEquationSystem oscillator(bool held){std::vector<ScientificObservation> dx,dv;std::vector<double> xs=held?std::vector<double>{-.75,.75,1.75}:std::vector<double>{-2.,-1.,0.,1.,2.};std::vector<double> vs=held?std::vector<double>{-.5,.5}:std::vector<double>{-1.,0.,1.};for(double xx:xs)for(double vv:vs){dx.push_back({{0,xx,vv},vv});dv.push_back({{0,xx,vv},-.25*vv-2*xx});}auto eq0=parseExpression("x2",5),eq1=parseExpression("((-1)*x3*x2)-x4*x1",5);return {3,{eq0,eq1},{{0,1,1,dx},{1,2,1,dv}},{{1,0,4},{1.4,0,4}}};}
}
int main(int argc,char**argv){try{
    if(argc!=2)throw std::invalid_argument("usage: genetik_s1_fixed_structure_comparison OUTPUT.tsv");
    std::ofstream out(argv[1]);if(!out)throw std::runtime_error("cannot create output");out<<"task\tmethod\tstart\tparameters\ttraining_mse\theldout_rmse\tobjective_calls\trows\tmath_ops\tderivative_ops\tlinear_algebra_ops\taccepted\trejected\tlinear_solves\twall_seconds\tcpu_seconds\n"<<std::setprecision(17);
    std::vector<Task> tasks;
    tasks.push_back(task("fractional-power",{3.5,1.7},{{2.,1.2},{5.,2.1}},[](double v){return 3.5*std::pow(v,1.7);}));
    tasks.push_back(task("trigonometric",{2.,1.5,.3},{{1.5,1.,0.},{3.2,1.8,.6}},[](double v){return 2*std::sin(1.5*v+.3);}));
    tasks.push_back(task("exponential",{3.,.7},{{1.5,.3},{4.,1.1}},[](double v){return 3*std::exp(-.7*v);}));
    tasks.push_back(task("rational",{2.,1.},{{1.3,1.2},{3.,2.}},[](double v){return (v+2)/(v*v+1);}));
    for(const auto&t:tasks)for(std::size_t s=0;s<t.starts.size();++s)for(auto method:{ConstantFitMethod::Coordinate,ConstantFitMethod::LevenbergMarquardt}){
        auto model=t.model;for(std::size_t j=0;j<t.fit.size();++j)model=model.withConstant(t.fit[j],t.starts[s][j]);auto training=scalarRows(t,false),held=scalarRows(t,true);auto problem=RegressionProblem::create(training,{127,16,32,10000});TrainingObjective objective(problem,2048);ConstantFitConfig cfg;cfg.maxLossAttempts=1024;cfg.maxSweeps=64;cfg.maxProposals=2048;cfg.initialStep=.4;cfg.minimumStep=1e-10;cfg.lower=-8;cfg.upper=8;cfg.lossThreshold=1e-24;cfg.method=method;cfg.levenbergMarquardt.maxIterations=80;cfg.levenbergMarquardt.maxObjectiveEvaluations=128;cfg.levenbergMarquardt.maxJacobianEvaluations=128;cfg.levenbergMarquardt.maxMathematicalOperations=30000000;cfg.levenbergMarquardt.lossTolerance=1e-22;const auto begin=std::chrono::steady_clock::now();const auto cpuBegin=std::clock();auto fitted=fitSelectedConstants(model,objective,t.fit,cfg);const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();const double cpuSeconds=static_cast<double>(std::clock()-cpuBegin)/CLOCKS_PER_SEC;const auto name=method==ConstantFitMethod::Coordinate?"coordinate":"levenberg-marquardt";out<<t.name<<'\t'<<name<<'\t'<<s<<'\t'<<coeffs(fitted.expression,t.fit)<<'\t'<<(fitted.loss.state()==FitnessState::Valid?fitted.loss.score():-1)<<'\t'<<rmse(fitted.expression,held)<<'\t'<<fitted.lossAttempts<<'\t'<<objective.work().rowAttempts<<'\t'<<objective.supplementalWork().mathematicalOperations<<'\t'<<fitted.optimizerWork.derivativeEvaluations<<'\t'<<fitted.optimizerWork.linearAlgebraOperations<<'\t'<<fitted.optimizerWork.acceptedSteps<<'\t'<<fitted.optimizerWork.rejectedSteps<<'\t'<<fitted.optimizerWork.linearSolves<<'\t'<<seconds<<'\t'<<cpuSeconds<<'\n';
    }
    auto train=oscillator(false),held=oscillator(true);ConstantFitConfig coord;coord.lower=-8;coord.upper=8;coord.maxLossAttempts=4096;coord.maxSweeps=100;coord.maxProposals=8192;const auto cWall=std::chrono::steady_clock::now();auto cCpu=std::clock();auto cfit=fitScientificParameters(train,coord);const double cSeconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-cWall).count();const double cCpuSeconds=static_cast<double>(std::clock()-cCpu)/CLOCKS_PER_SEC;LMConfig lm;lm.maxIterations=100;lm.maxObjectiveEvaluations=512;lm.maxJacobianEvaluations=512;lm.maxMathematicalOperations=30000000;auto lCpu=std::clock();auto lfit=fitScientificParametersLevenbergMarquardt(train,lm);const double lCpuSeconds=static_cast<double>(std::clock()-lCpu)/CLOCKS_PER_SEC;auto ca=held.assess(cfit.parameters),la=held.assess(lfit.parameters);out<<"coupled-oscillator\tcoordinate\t0\t["<<cfit.parameters[0]<<','<<cfit.parameters[1]<<"]\t"<<*cfit.loss<<'\t'<<*ca.loss<<'\t'<<cfit.work.lossAttempts<<'\t'<<cfit.work.rowAttempts<<'\t'<<cfit.supplemental.mathematicalOperations<<"\t0\t0\t0\t0\t0\t"<<cSeconds<<"\t"<<cCpuSeconds<<'\n';
    out<<"coupled-oscillator\tlevenberg-marquardt\t0\t["<<lfit.parameters[0]<<','<<lfit.parameters[1]<<"]\t"<<*lfit.sumSquaredResiduals<<'\t'<<*la.loss<<'\t'<<lfit.work.objectiveEvaluations<<'\t'<<lfit.work.rowsProcessed<<'\t'<<lfit.work.mathematicalOperations<<'\t'<<lfit.work.derivativeEvaluations<<'\t'<<lfit.work.linearAlgebraOperations<<'\t'<<lfit.work.acceptedSteps<<'\t'<<lfit.work.rejectedSteps<<'\t'<<lfit.work.linearSolves<<'\t'<<lfit.work.seconds<<'\t'<<lCpuSeconds<<'\n';
    std::cout<<"fixed-topology results written: "<<argv[1]<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
