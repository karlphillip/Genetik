#include "../research/s14/MultivariateRationalRegression.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace genetik_s14;
static void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
static void near(double a,double b,double tol,const char* m){if(!(std::abs(a-b)<=tol))throw std::runtime_error(m);}
static std::vector<Observation> grid(auto f,double scale=1.0){
    std::vector<Observation> rows;
    for(double x:{-0.8,-0.45,-0.1,0.2,0.55,0.9})for(double y:{-0.75,-0.3,0.15,0.5,0.85}){
        const double sx=x*scale,sy=y*scale;rows.push_back({sx,sy,f(sx,sy)});
    }return rows;
}
static const Model& selected(const FitResult& r){if(r.status!=Status::Success||!r.selected)throw std::runtime_error(std::string("fit status ")+statusName(r.status)+": "+r.detail);return r.models[*r.selected];}

int main(){
 try{
    const auto b0=basis(0),b1=basis(1),b2=basis(2);
    require(b0.size()==1&&b1.size()==3&&b2.size()==6,"graded total-degree basis counts");
    require(b2[0].x==0&&b2[0].y==0&&b2[1].x==1&&b2[2].y==1&&b2[3].x==2&&b2[4].x==1&&b2[4].y==1&&b2[5].y==2,
            "deterministic basis order includes mixed xy term");

    const auto linear=grid([](double x,double y){return 1.0+2*x-3*y;});
    const auto linearFit=fit(linear);const auto& lm=selected(linearFit);
    require(lm.numeratorDegree==1&&lm.denominatorDegree==0,"linear degrees selected from training only");
    require(lm.expression.variableCount()==2,"materialized model binds exactly x0 and x1");
    for(const auto& r:linear)near(*lm.predict(r.x,r.y),r.target,1e-11,"linear expression prediction");

    const auto quadratic=grid([](double x,double y){return 1+2*x-.5*y+.3*x*x+.4*x*y-.2*y*y;});
    const auto quadraticFit=fit(quadratic);const auto& qm=selected(quadraticFit);
    require(qm.numeratorDegree==2&&qm.denominatorDegree==0,"complete quadratic basis is recovered");
    for(const auto& r:quadratic)near(*qm.predict(r.x,r.y),r.target,1e-10,"quadratic independent prediction");

    const auto proper=grid([](double x,double y){return (.5+1.2*x-.7*y+.3*x*y)/(1+.2*x-.1*y+.08*x*x);});
    const auto properFit=fit(proper);const auto& pm=selected(properFit);
    require(pm.numeratorDegree==2&&pm.denominatorDegree==2,"proper rational total degrees selected");
    require(pm.numerator.size()==6&&pm.denominator.size()==6&&pm.parameterCount==11,"full two-variable degree-two model has at most eleven parameters");
    near(pm.denominator[0],1,0,"normalization Q(0,0)=1 retained");
    const std::array<double,6> expectedP{.5,1.2,-.7,0,.3,0};
    const std::array<double,6> expectedQ{1,.2,-.1,.08,0,0};
    for(std::size_t i=0;i<6;++i){near(pm.numerator[i],expectedP[i],1e-9,"identifiable numerator coefficient recovered");near(pm.denominator[i],expectedQ[i],1e-9,"identifiable denominator coefficient recovered");}
    for(const auto& r:proper)near(*pm.predict(r.x,r.y),r.target,1e-9,"proper rational expression equals independent reference");
    for(const auto& r:proper){auto ev=pm.expression.evaluate(std::array{r.x,r.y});require(ev.valid(),"ordinary Genetik Expression executes");near(ev.value(),r.target,1e-9,"Genetik expression agrees with fitted quotient");}
    auto inconsistent=pm;inconsistent.denominator[1]+=.1;
    require(!inconsistent.predict(.2,.15)&&assessDomain(inconsistent,{-.2,.2,-.2,.2}).status==S14DomainStatus::Invalid,
            "coefficient mutation cannot make prediction and domain assessment disagree");

    const auto improper=grid([](double x,double y){return (1.2+x*x+.2*x*y)/(1+.1*y);});
    const auto improperFit=fit(improper);const auto& im=selected(improperFit);
    require(im.numeratorDegree==2&&im.denominatorDegree==1,"improper bivariate rational degrees selected");
    for(const auto& r:improper)near(*im.predict(r.x,r.y),r.target,1e-9,"improper rational predictions");

    auto repeat=fit(proper);const auto& rm=selected(repeat);
    require(rm.expression.toString()==pm.expression.toString()&&rm.numerator==pm.numerator&&rm.denominator==pm.denominator,
            "fixed data gives deterministic coefficients and serialization");
    require(pm.expression.nodeCount()<=255&&pm.expression.depth()<=16&&pm.expression.arithmeticCost()<=2048,
            "default expression structural limits respected");

    Config lmConfig;lmConfig.refineWithLM=true;lmConfig.lm.maxIterations=8;lmConfig.lm.maxObjectiveEvaluations=32;
    auto noisy=proper;for(std::size_t i=0;i<noisy.size();++i)noisy[i].target+=(i%2?1:-1)*1e-4;
    auto qrNoisy=fit(noisy);auto lmNoisy=fit(noisy,lmConfig);
    require(qrNoisy.status==Status::Success&&lmNoisy.status==Status::Success,"QR-only and QR+existing LM both finish noisy fit");
    require(selected(lmNoisy).refined&&selected(lmNoisy).lmWork.objectiveEvaluations>0,"optional LM reports actual work");
    require(std::isfinite(selected(qrNoisy).trainingSSE)&&std::isfinite(selected(lmNoisy).trainingSSE),"noisy losses are finite");
    require(lmNoisy.linearAlgebraOperations<=lmConfig.maximumLinearAlgebraOperations&&
            lmNoisy.arithmeticOperations<=lmConfig.maximumArithmeticOperations,"combined configured work limits respected");

    auto under=Config{};under.maximumParameters=11;under.maximumRows=4;
    auto underData=grid([](double x,double y){return 1+x+y;});
    auto underFit=fit(std::span<const Observation>(underData).first(3),under);
    if(underFit.status!=Status::Success||underFit.underdeterminedFamilies==0)throw std::runtime_error(std::string("underdetermined fixture ")+statusName(underFit.status)+" count="+std::to_string(underFit.underdeterminedFamilies)+" detail="+underFit.detail);
    auto deficientConfig=Config{};deficientConfig.maximumNumeratorDegree=1;deficientConfig.maximumDenominatorDegree=0;
    std::vector<Observation> deficient{{-1,-2,-5},{0,0,1},{1,2,-3},{2,4,-7},{3,6,-11}};
    auto deficientFit=fit(deficient,deficientConfig);
    require(deficientFit.status==Status::Success&&deficientFit.rankFailures>0,
            "collinear input causes explicit rank-deficient family failures while smaller models remain eligible");
    auto duplicateRows=linear;duplicateRows.push_back(linear.front());duplicateRows.push_back(linear.front());
    require(fit(duplicateRows).status==Status::Success,"duplicate observations remain supported when other rows identify a valid fit");
    std::vector<Observation> almostCollinear;
    for(double x:{-1.,-.5,0.,.5,1.}){const double y=2*x+1e-13*(x*x+.1);almostCollinear.push_back({x,y,1+x-y});}
    Config nearlyConfig;nearlyConfig.maximumNumeratorDegree=1;nearlyConfig.maximumDenominatorDegree=0;
    const auto almostFit=fit(almostCollinear,nearlyConfig);
    require(almostFit.status==Status::Success&&(almostFit.illConditionedFamilies||almostFit.rankFailures),
            "nearly dependent basis columns are rejected or retained as explicit ill-conditioning diagnostics");
    Config illConfig;illConfig.minimumPivotRatio=.99;
    const auto illFit=fit(linear,illConfig);
    require(illFit.status==Status::Success&&illFit.illConditionedFamilies>0,
            "configured pivot-ratio rejection is counted while simpler models remain eligible");
    Config laLimit;laLimit.maximumLinearAlgebraOperations=1;
    require(fit(linear,laLimit).status==Status::ResourceLimit,"linear algebra budget is enforced");
    Config arithmeticLimit;arithmeticLimit.maximumArithmeticOperations=1;
    require(fit(linear,arithmeticLimit).status==Status::ResourceLimit,"polynomial basis operation budget is enforced");
    auto nonfinite=linear;nonfinite[0].x=std::numeric_limits<double>::infinity();
    require(fit(nonfinite).status==Status::InvalidInput,"nonfinite observations rejected");

    auto scaledSmall=grid([](double x,double y){return 2+.2*x-.3*y;},1e-5);
    const auto smallFit=fit(scaledSmall);require(smallFit.status==Status::Success,"column-scaled QR handles small input scales");
    for(const auto& r:scaledSmall)near(*selected(smallFit).predict(r.x,r.y),r.target,1e-10,"small-scale predictions");
    auto scaledLarge=grid([](double x,double y){return 2+2e-6*x-3e-6*y;},1e5);
    const auto largeFit=fit(scaledLarge);require(largeFit.status==Status::Success,"column-scaled QR handles large input scales");
    for(const auto& r:scaledLarge)near(*selected(largeFit).predict(r.x,r.y),r.target,1e-8,"large-scale predictions");

    std::vector<Observation> poleTraining;
    for(double x:{-.6,-.4,-.2,0.,.2,.4})for(double y:{-.5,-.25,0.,.2,.4})if(x+y<.5)
        poleTraining.push_back({x,y,1.0/(1.0-x-y)});
    Config degreeOne;degreeOne.maximumNumeratorDegree=0;degreeOne.maximumDenominatorDegree=1;
    const auto poleFit=fit(poleTraining,degreeOne);const auto& poleModel=selected(poleFit);
    near(poleModel.trainingSSE,0,1e-18,"near-pole rational topology is correctly fitted on safe training rows");
    const auto nearPoleDomain=assessDomain(poleModel,{.8,.99,0.,.2},512,8,1e-10);
    require(nearPoleDomain.status!=S14DomainStatus::Valid||
            nearPoleDomain.minimumCertifiedSeparation<.1,
            "near-pole region is not mislabeled as comfortably separated");

    Config tiny; tiny.maximumWorkspaceBytes=64;
    require(fit(proper,tiny).status==Status::ResourceLimit,"workspace exhaustion is explicit");
    Config bad;bad.maximumNumeratorDegree=3;
    require(fit(linear,bad).status==Status::InvalidConfiguration,"unsupported degree configuration rejected");
    bad=Config{};bad.operators.push_back(ExpressionKind::Sin);
    require(fit(linear,bad).status==Status::InvalidConfiguration,"unsupported grammar operator rejected");
    bad=Config{};bad.expressionLimits.maxAbsConstant=.5;
    require(fit(linear,bad).status==Status::InvalidConfiguration,"expression limits incompatible with Q normalization rejected");
    bad=Config{};bad.expressionLimits.maxNodes=2;bad.expressionLimits.maxDepth=1;
    const auto tooTightFit=fit(linear,bad);
    require(tooTightFit.status==Status::ResourceLimit||tooTightFit.status==Status::InvalidModel,
            "tight expression node/depth limits reject nonrepresentable quotient models explicitly");
    std::vector<Observation> overflowRows{{1e200,0,1},{1e200,1,2},{1e200,2,3}};
    const auto overflowFit=fit(overflowRows);
    require(overflowFit.status==Status::Success&&overflowFit.invalidModels>0,
            "overflowing high-degree families are rejected while bounded lower-degree fits remain available");

    // Q=1-x-y has a zero curve. A sampled exact zero is reported as Invalid;
    // a separated compact rectangle is certified, while bounded non-point
    // enclosure that cannot be resolved remains Uncertain.
    Model lineModel;lineModel.denominatorBasis=basis(1);lineModel.denominator={1,-1,-1};
    lineModel.denominatorExpression=detail::denominatorExpression(lineModel.denominatorBasis,
        std::span<const double>(lineModel.denominator).subspan(1),ExpressionLimits{});
    lineModel.numeratorBasis=basis(0);lineModel.numerator={1};
    lineModel.expression=detail::buildExpression(lineModel.numeratorBasis,lineModel.numerator,
        lineModel.denominatorBasis,std::span<const double>(lineModel.denominator).subspan(1),false,2,ExpressionLimits{});
    require(!lineModel.predict(.5,.5)&&!lineModel.predict(.5,.5-1e-12),
            "demonstrated and scale-near denominator zeros withhold point predictions");
    auto crosses=assessDomain(lineModel,{-1,1,-1,1});
    require(crosses.status==S14DomainStatus::Invalid&&crosses.finding==DomainFinding::DemonstratedZero,
            "zero curve is reported when a zero is numerically demonstrated");
    Model safe;safe.denominatorBasis=basis(1);safe.denominator={1,.2,-.1};
    safe.denominatorExpression=detail::denominatorExpression(safe.denominatorBasis,std::span<const double>(safe.denominator).subspan(1),ExpressionLimits{});
    auto cert=assessDomain(safe,{-0.1,0.1,-0.1,0.1});
    require(cert.status==S14DomainStatus::Valid&&cert.finding==DomainFinding::SeparatedFromZero&&cert.minimumCertifiedSeparation>0,
            "interval enclosure certifies denominator separation on safe rectangle");
    auto uncertain=assessDomain(safe,{-10,8,-9,10},1,0);
    require(uncertain.status==S14DomainStatus::Uncertain,"bounded subdivision does not overclaim large-region validity");

    // Existing one-variable public API regression is exercised without change.
    std::vector<RationalObservation> old;for(double x:{-2.,-1.,-.2,.3,1.,2.})old.push_back({x,(2.3*x+.7)/(x*x+1.5)});
    const auto oldFit=fitRationalFunction(old);require(oldFit.status==RationalStatus::Success&&oldFit.selected.has_value(),"existing scalar rational API remains usable");
    std::cout<<"S14 deterministic contracts passed; proper degrees="<<pm.numeratorDegree<<"/"<<pm.denominatorDegree
             <<" pivot_ratio="<<pm.pivotRatio<<" qr_ops="<<properFit.linearAlgebraOperations<<"\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<"S14 test failure: "<<e.what()<<"\n";return 1;}
}
