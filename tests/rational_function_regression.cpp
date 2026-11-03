#include <RationalFunctionRegression.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void near(double a, double b, double tolerance, const char* message) {
    if (!(std::abs(a-b) <= tolerance)) throw std::runtime_error(message);
}
static std::vector<RationalObservation> data(auto fn, std::initializer_list<double> xs) {
    std::vector<RationalObservation> rows;
    for (double x : xs) rows.push_back({x,fn(x)});
    return rows;
}
static Expression rationalReferenceA() {
    using K=ExpressionKind;
    const std::vector<ExpressionNode> nodes{
        ExpressionNode::variable(0),ExpressionNode::constant(1.4),ExpressionNode::binary(K::Multiply),
        ExpressionNode::constant(.2),ExpressionNode::binary(K::Add),
        ExpressionNode::variable(0),ExpressionNode::power(2),ExpressionNode::constant(.8),
        ExpressionNode::binary(K::Add),ExpressionNode::binary(K::Divide)};
    return Expression(nodes,1,{31,8,8,128});
}
static Expression rationalReferenceB() {
    using K=ExpressionKind;
    const std::vector<ExpressionNode> nodes{
        ExpressionNode::variable(0),ExpressionNode::power(2),ExpressionNode::constant(.8),ExpressionNode::binary(K::Multiply),
        ExpressionNode::constant(-1),ExpressionNode::binary(K::Add),
        ExpressionNode::variable(0),ExpressionNode::constant(2.5),ExpressionNode::binary(K::Add),ExpressionNode::binary(K::Divide)};
    return Expression(nodes,1,{31,8,8,128});
}
static Expression nonrationalReference() {
    using K=ExpressionKind;
    const std::vector<ExpressionNode> nodes{ExpressionNode::variable(0),ExpressionNode::constant(1.1),
        ExpressionNode::binary(K::Multiply),ExpressionNode::unary(K::Sin),ExpressionNode::constant(.2),ExpressionNode::binary(K::Add)};
    return Expression(nodes,1,{31,8,8,128});
}
int main() {
    try {
        
        const Expression zeroDomain(std::array{ExpressionNode::variable(0),ExpressionNode::variable(0),
            ExpressionNode::binary(ExpressionKind::Divide)},1);
        require(!zeroDomain.evaluate(std::array{0.0}).valid(),"expression division domain failure retained");
        const Expression overflow(std::array{ExpressionNode::constant(std::numeric_limits<double>::max()),
            ExpressionNode::constant(2),ExpressionNode::binary(ExpressionKind::Multiply)},0);
        require(!overflow.evaluate(std::span<const double>{}).valid(),"overflow prediction retained as invalid");
        const auto refA=rationalReferenceA(),refB=rationalReferenceB(),refControl=nonrationalReference();
        for(double x:{-1.8,-1.,-.25,.2,1.,1.8}) {
            near(refA.evaluate(std::array{x}).value(),(1.4*x+.2)/(x*x+.8),1e-14,"independent rational A reference grammar preflight");
            near(refB.evaluate(std::array{x}).value(),(.8*x*x-1)/(x+2.5),1e-14,"independent rational B reference grammar preflight");
            near(refControl.evaluate(std::array{x}).value(),std::sin(1.1*x)+.2,1e-14,"nonrational control grammar preflight");
        }
        require(refA.nodeCount()<=31&&refB.depth()<=8&&refControl.arithmeticCost()<=128,"reference expression resource preflight");
        const auto rational = data([](double x){return (2.3*x+0.7)/(x*x+1.5);}, {-2,-1.5,-1,-.5,-.2,.2,.5,1,1.5,2});
        RationalConfig c;
        const auto s8Development=data([](double x){return x/(1+x*x);},{-2,-1.5,-1,-.5,-.2,.2,.5,1,1.5,2});
        auto dev=fitRationalFunction(s8Development,c);require(dev.status==RationalStatus::Success,"S8 rational development fit");
        require(dev.models[*dev.selected].trainingSSE<1e-24,"S8 rational topology recovery");
        auto fit=fitRationalFunction(rational,c);
        require(fit.status==RationalStatus::Success && fit.selected.has_value(),"rational fit success");
        auto repeat=fitRationalFunction(rational,c);
        require(repeat.status==RationalStatus::Success && repeat.selected.has_value() &&
                repeat.models[*repeat.selected].expression.toString()==fit.models[*fit.selected].expression.toString() &&
                repeat.models[*repeat.selected].numerator==fit.models[*fit.selected].numerator &&
                repeat.models[*repeat.selected].denominator==fit.models[*fit.selected].denominator,
                "deterministic coefficients and expression serialization");
        const auto& model=fit.models[*fit.selected];
        require(model.numeratorDegree==1 && model.denominatorDegree==2,"rational degree selection");
        require(model.trainingSSE<1e-24,"rational exact training recovery");
        near(model.denominator.front(),1,0,"denominator normalization");
        for(const auto& row:rational) near(*model.predict(row.x,c.minimumAbsoluteDenominator),row.y,1e-11,"rational expression evaluator");
        require(model.expression.variableCount()==1 && model.expression.nodeCount()<=c.maximumParameters*8+8,"ordinary bounded Genetik expression");

        const auto polynomial=data([](double x){return 2-3*x+.5*x*x;},{-2,-1.2,-.4,.3,1.1,2});
        auto poly=fitRationalFunction(polynomial,c);require(poly.status==RationalStatus::Success,"polynomial success");
        require(poly.models[*poly.selected].numeratorDegree==2 && poly.models[*poly.selected].denominatorDegree==0,"polynomial degree selection");
        const auto constant=data([](double){return -2.5;},{-1,0,1,2});
        auto con=fitRationalFunction(constant,c);require(con.status==RationalStatus::Success,"constant model");
        near(*con.models[*con.selected].predict(.2),-2.5,1e-12,"negative constant");
        const auto improper=data([](double x){return (x*x*x+2*x+1)/(x+2);},{-1.5,-1,-.5,0,.5,1,2,3});
        auto imp=fitRationalFunction(improper,c);require(imp.status==RationalStatus::Success,"improper rational success");
        for(const auto& row:improper) near(*imp.models[*imp.selected].predict(row.x),row.y,1e-8,"improper rational prediction");

        auto repeated=data([](double x){return (x*x-1)/(x-1);},{-2,-1,0,.5,1.5,2});
        auto rep=fitRationalFunction(repeated,c);require(rep.status==RationalStatus::Success,"redundant/reducible rational fit");
        const auto poleData=data([](double x){return 1.0/(x+2.5);},{-1.5,-1,-.5,0,.5,1,1.5});
        auto poleFit=fitRationalFunction(poleData,c);require(poleFit.status==RationalStatus::Success,"nonzero-constant denominator fit");
        require(!poleFit.models[*poleFit.selected].predict(-2.5,c.minimumAbsoluteDenominator),"pole rejected by explicit denominator check");
        require(!poleFit.models[*poleFit.selected].predict(-2.5+1e-12,c.minimumAbsoluteDenominator),"near-pole denominator rejected");
        const auto validPoint=predictRationalChecked(poleFit.models[*poleFit.selected],0.0);
        require(validPoint.status==RationalPredictionStatus::Valid&&validPoint.value.has_value()&&validPoint.reason=="valid",
                "checked point prediction reports a finite value");
        const auto invalidPoint=predictRationalChecked(poleFit.models[*poleFit.selected],-2.5);
        require(invalidPoint.status==RationalPredictionStatus::Invalid&&!invalidPoint.value&&invalidPoint.reason=="zero_denominator",
                "zero denominator is explicitly invalid");
        const auto uncertainPoint=predictRationalChecked(poleFit.models[*poleFit.selected],-2.5+1e-12);
        require(uncertainPoint.status==RationalPredictionStatus::Uncertain&&!uncertainPoint.value&&uncertainPoint.reason=="denominator_near_zero",
                "near-pole point is withheld as numerically uncertain");
        auto overflowingModel=poleFit.models[*poleFit.selected];
        overflowingModel.expression=Expression(std::array{ExpressionNode::constant(std::numeric_limits<double>::max()),
            ExpressionNode::constant(2),ExpressionNode::binary(ExpressionKind::Multiply)},1);
        const auto invalidPrediction=predictRationalChecked(overflowingModel,0.0);
        require(invalidPrediction.status==RationalPredictionStatus::Invalid&&!invalidPrediction.value&&
                invalidPrediction.reason=="invalid_rational_expression","overflow remains an explicit invalid prediction");
        auto noisy=rational;for(std::size_t i=0;i<noisy.size();++i)noisy[i].y+=((i%2)?1:-1)*1e-4;
        auto noise=fitRationalFunction(noisy,c);require(noise.status==RationalStatus::Success && std::isfinite(noise.models[*noise.selected].trainingSSE),"noisy fit");
        const auto nonrational=data([](double x){return std::sin(x);},{-2,-1.5,-1,-.5,0,.5,1,1.5,2});
        auto nr=fitRationalFunction(nonrational,c);require(nr.status==RationalStatus::Success,"bounded approximation to nonrational control");

        auto lmConfig=c;lmConfig.refineWithLM=true;lmConfig.lm.maxIterations=12;lmConfig.lm.maxObjectiveEvaluations=32;
        auto lm=fitRationalFunction(rational,lmConfig);require(lm.status==RationalStatus::Success,"LM refinement path");
        require(lm.models[*lm.selected].refined,"LM was requested");
        require(lm.models[*lm.selected].trainingSSE<=model.trainingSSE+1e-18,"LM refinement retained no-worse prediction score");

        const std::array<double,3> safeInputs{-1.8,0,1.8};
        const auto safe=assessRationalDomain(model,safeInputs,{-1.8,1.8});
        require(safe.status==DomainStatus::Valid,"outward interval check certifies denominator away from zero");
        auto nearPoleModel=model;nearPoleModel.denominator={1,.4};nearPoleModel.denominatorDegree=1;
        const auto crossing=assessRationalDomain(nearPoleModel,safeInputs,{-3,0});
        require(crossing.status==DomainStatus::Invalid&&!crossing.possiblePoleIntervals.empty(),"denominator root in domain reported invalid");
        const auto nearPole=assessRationalDomain(nearPoleModel,safeInputs,{-2.4,-2.0});
        require(nearPole.status==DomainStatus::Valid,"near-pole interval separated from zero is certifiable");
        require(std::abs(genetik_rational_detail::polynomialValue(nearPoleModel.denominator,-2.4))>.039,"small denominator is reported as near pole rather than singular");
        auto repeatedRoot=nearPoleModel;repeatedRoot.denominator={1,2,1};repeatedRoot.denominatorDegree=2;
        const auto evenPole=assessRationalDomain(repeatedRoot,safeInputs,{-2,0});
        require(evenPole.status==DomainStatus::Invalid,"even-multiplicity denominator zero is found");
        auto positiveQuadratic=nearPoleModel;positiveQuadratic.denominator={1,0,1};positiveQuadratic.denominatorDegree=2;
        const auto boundedUncertain=assessRationalDomain(positiveQuadratic,safeInputs,{-1,1},1e-10,1);
        require(boundedUncertain.status==DomainStatus::Uncertain,"diagnostic resource exhaustion remains uncertain");
        auto nonfiniteDiagnostic=nearPoleModel;nonfiniteDiagnostic.denominator={1,std::numeric_limits<double>::infinity()};
        require(assessRationalDomain(nonfiniteDiagnostic,safeInputs,{-1,1}).status==DomainStatus::Invalid,
                "nonfinite denominator coefficients are invalid");

        auto duplicate=fitRationalFunction(data([](double){return 1.;},{0,0,0,0}),c);
        require(duplicate.status==RationalStatus::Success || duplicate.status==RationalStatus::RankDeficient,"rank-deficient status explicit");
        auto nonfinite=std::vector<RationalObservation>{{0,1},{1,std::numeric_limits<double>::infinity()}};
        require(fitRationalFunction(nonfinite,c).status==RationalStatus::InvalidInput,"nonfinite observations rejected");
        auto overflowing=std::vector<RationalObservation>{{1e200,1e200},{1.1e200,2e200}};
        require(fitRationalFunction(overflowing,c).status==RationalStatus::InvalidInput,"polynomial-basis overflow reported");
        auto lowOps=c;lowOps.maximumArithmeticOperations=1;
        require(fitRationalFunction(rational,lowOps).status==RationalStatus::ResourceLimit,"arithmetic operation cap enforced");
        auto lowQr=c;lowQr.maximumLinearAlgebraOperations=1;
        require(fitRationalFunction(rational,lowQr).status==RationalStatus::ResourceLimit,"linear algebra cap enforced");
        auto rowLimited=c;rowLimited.maximumRows=2;
        require(fitRationalFunction(rational,rowLimited).status==RationalStatus::ResourceLimit,"row bound enforced");
        auto memoryLimited=c;memoryLimited.maximumWorkspaceBytes=8;
        require(fitRationalFunction(rational,memoryLimited).status==RationalStatus::ResourceLimit,"workspace bound enforced");
        auto parameterLimited=c;parameterLimited.maximumParameters=2;
        auto limited=fitRationalFunction(rational,parameterLimited);
        require(limited.status==RationalStatus::Success && limited.models[*limited.selected].numerator.size()+limited.models[*limited.selected].denominator.size()-1<=2,"parameter bound enforced");
        bool invalidConfig=false;try{auto bad=c;bad.maximumNumeratorDegree=4;(void)fitRationalFunction(rational,bad);}catch(const std::invalid_argument&){invalidConfig=true;}
        require(invalidConfig,"degree limit rejected");
        invalidConfig=false;try{auto bad=c;bad.operators.erase(std::find(bad.operators.begin(),bad.operators.end(),ExpressionKind::Divide));(void)fitRationalFunction(rational,bad);}catch(const std::invalid_argument&){invalidConfig=true;}
        require(invalidConfig,"required grammar operator rejected");
        std::cout<<"S13 rational correctness, limits, family selection and optional LM passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
