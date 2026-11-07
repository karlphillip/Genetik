#include <MultivariateRationalFunctionRegression.hpp>

#include <cfenv>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace genetik::multivariate_rational;
static void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}

static Model model(unsigned numeratorDegree,std::vector<double> p,
                   unsigned denominatorDegree,std::vector<double> q) {
    Model m;
    m.numeratorDegree=numeratorDegree;m.denominatorDegree=denominatorDegree;
    m.numeratorBasis=basis(numeratorDegree);m.denominatorBasis=basis(denominatorDegree);
    m.numerator=std::move(p);m.denominator=std::move(q);
    m.parameterCount=m.numerator.size()+m.denominator.size()-1;
    m.denominatorExpression=detail::denominatorExpression(m.denominatorBasis,
        std::span<const double>(m.denominator).subspan(1),ExpressionLimits{});
    m.expression=detail::buildExpression(m.numeratorBasis,m.numerator,m.denominatorBasis,
        std::span<const double>(m.denominator).subspan(1),false,m.denominator.size()-1,ExpressionLimits{});
    return m;
}

static bool safelySeparated(const Model& m,Rectangle r) {
    const auto result=assessDomain(m,r,256,8,1e-10);
    return result.status==DomainAssessmentStatus::Valid&&result.finding==DomainFinding::SeparatedFromZero;
}

int main(){
 try {
    const Rectangle unit{0,1,0,1};
    const auto linearZero=model(0,{1},1,{1,-2,0}); // Q=1-2x
    const auto linearResult=assessDomain(linearZero,unit);
    check(linearResult.status==DomainAssessmentStatus::Invalid&&linearResult.finding==DomainFinding::DemonstratedZero,
          "independent linear denominator sign change proves an interior zero");

    const auto squareZero=model(0,{1},2,{1,0,0,-2,0,0}); // Q=1-2x^2
    const auto squareResult=assessDomain(squareZero,unit);
    check(squareResult.status==DomainAssessmentStatus::Invalid&&squareResult.finding==DomainFinding::DemonstratedZero,
          "independent quadratic denominator sign change proves an interior zero");

    const auto positiveQuadratic=model(0,{1},2,{1,0,0,1,0,1}); // Q=1+x^2+y^2
    const auto positiveResult=assessDomain(positiveQuadratic,unit);
    check(positiveResult.status==DomainAssessmentStatus::Valid&&positiveResult.minimumCertifiedSeparation>0.99,
          "positive quadratic is certified with a conservative lower bound");
    check(positiveResult.cellsVisited==1&&positiveResult.intervalOperations>0,
          "domain work counters include cells and interval operations");

    const auto linearSafe=model(0,{1},1,{1,.25,-.25}); // Q=1+x/4-y/4
    check(safelySeparated(linearSafe,{-1,1,-1,1}),
          "constant, linear x/y, negative coefficients, and zero-spanning intervals are enclosed safely");
    check(safelySeparated(linearSafe,{2,3,-3,-2}),
          "linear terms remain bounded on rectangles entirely above and below zero");
    const auto signedZero=model(0,{1},1,{1,-0.0,0.0});
    check(safelySeparated(signedZero,{-1,1,-1,1}),
          "signed-zero coefficients widen conservatively without changing normalization");
    const auto xSquare=model(0,{1},2,{1,0,0,1,0,0});
    check(safelySeparated(xSquare,{-3,-2,0,1})&&safelySeparated(xSquare,{2,3,-1,1}),
          "x squared bounds are conservative on negative and positive rectangles");
    const auto ySquare=model(0,{1},2,{1,0,0,0,0,1});
    check(safelySeparated(ySquare,{-1,1,-3,-2})&&safelySeparated(ySquare,{-1,1,2,3}),
          "y squared bounds are conservative on negative and positive rectangles");
    const auto subnormalCoefficient=model(0,{1},1,{1,std::numeric_limits<double>::denorm_min(),0});
    const auto subnormalResult=assessDomain(subnormalCoefficient,{0,1,0,1});
    check(detail::supportedIntervalEnvironment()?
          subnormalResult.status==DomainAssessmentStatus::Valid:
          subnormalResult.status==DomainAssessmentStatus::Uncertain&&subnormalResult.reason=="unsupported_floating_point_environment",
          "subnormal arithmetic either remains enclosed or fails closed when FTZ/DAZ is active");
    const auto overflowModel=model(0,{1},2,{1,0,0,1,0,0});
    const auto overflowResult=assessDomain(overflowModel,{1e200,1e201,0,1},16,2,1e-10);
    check(overflowResult.status==DomainAssessmentStatus::Uncertain,
          "overflowing interval intermediates fail closed as uncertain");

    const auto mixed=model(0,{1},2,{1,0,0,0,1,0}); // Q=1+xy
    check(safelySeparated(mixed,unit),"mixed xy is certified on a rectangle with xy >= 0");
    const auto mixedPossible=assessDomain(mixed,{-2,-1,1,2},256,8,1e-10);
    check(mixedPossible.status!=DomainAssessmentStatus::Valid,
          "mixed xy region with a possible zero is not certified pole-free");
    const auto exhausted=assessDomain(mixed,{-2,-1,1,2},1,0,1e-10);
    check(exhausted.status==DomainAssessmentStatus::Uncertain&&exhausted.cellsVisited<=1&&exhausted.reason=="domain_subdivision_resource_limit",
          "cell/depth exhaustion is explicit and respects its configured bound");

    // Historical C2 false certificate. Independent values are exactly 1,
    // 0.625, and -2, so continuity guarantees a zero although S14 omitted x^3.
    Model cubic;
    cubic.numeratorDegree=0;cubic.denominatorDegree=3;
    cubic.numeratorBasis=basis(0);cubic.numerator={1};
    cubic.denominatorBasis={{0,0},{3,0}};cubic.denominator={1,-3};cubic.parameterCount=2;
    cubic.denominatorExpression=detail::denominatorExpression(cubic.denominatorBasis,
        std::span<const double>(cubic.denominator).subspan(1),ExpressionLimits{});
    cubic.expression=detail::buildExpression(cubic.numeratorBasis,cubic.numerator,cubic.denominatorBasis,
        std::span<const double>(cubic.denominator).subspan(1),false,1,ExpressionLimits{});
    check(assessDomain(cubic,unit).status==DomainAssessmentStatus::InvalidModel,
          "historical unsupported cubic is rejected before domain assessment");
    check(1.0-3.0*0.0==1.0&&1.0-3.0*0.125==0.625&&1.0-3.0==-2.0,
          "independent cubic reference values retain the intermediate-value counterexample");

    for(const Term t:std::array<Term,4>{{{2,1},{1,2},{3,0},{0,3}}}) {
        auto bad=model(0,{1},2,{1,0,0,0,0,0});
        bad.denominatorBasis[1]=t;
        bad.denominatorExpression=detail::denominatorExpression(bad.denominatorBasis,
            std::span<const double>(bad.denominator).subspan(1),ExpressionLimits{});
        bad.expression=detail::buildExpression(bad.numeratorBasis,bad.numerator,bad.denominatorBasis,
            std::span<const double>(bad.denominator).subspan(1),false,5,ExpressionLimits{});
        check(assessDomain(bad,unit).status==DomainAssessmentStatus::InvalidModel,
              "unsupported mixed or higher-degree term is rejected");
    }

    auto wrongCount=positiveQuadratic;wrongCount.denominator.pop_back();
    check(assessDomain(wrongCount,unit).status==DomainAssessmentStatus::InvalidModel,"coefficient count mismatch rejected");
    auto duplicate=positiveQuadratic;duplicate.denominatorBasis[4]=duplicate.denominatorBasis[3];
    check(assessDomain(duplicate,unit).status==DomainAssessmentStatus::InvalidModel,"duplicate basis term rejected");
    auto nonfinite=positiveQuadratic;nonfinite.denominator[3]=std::numeric_limits<double>::infinity();
    check(assessDomain(nonfinite,unit).status==DomainAssessmentStatus::InvalidModel,"nonfinite coefficient rejected");
    auto badNormalization=positiveQuadratic;badNormalization.denominator[0]=2;
    check(assessDomain(badNormalization,unit).status==DomainAssessmentStatus::InvalidModel,"normalization mismatch rejected");
    auto badExpression=positiveQuadratic;badExpression.expression=Expression::constant(1,2);
    check(assessDomain(badExpression,unit).status==DomainAssessmentStatus::InvalidModel,"materialized expression inconsistency rejected");
    auto badBinding=positiveQuadratic;badBinding.denominatorExpression=Expression::constant(1,1);
    check(assessDomain(badBinding,unit).status==DomainAssessmentStatus::InvalidModel,"unsupported variable binding rejected");
    check(assessDomain(positiveQuadratic,unit,0).status==DomainAssessmentStatus::InvalidConfiguration,
          "zero cell allowance rejected as invalid configuration");
    check(assessDomain(positiveQuadratic,{0,std::numeric_limits<double>::infinity(),0,1}).status==DomainAssessmentStatus::InvalidConfiguration,
          "nonfinite rectangle rejected as invalid configuration");

    auto partial=assessDomain(positiveQuadratic,unit,1,0,1e-10);
    check(partial.status==DomainAssessmentStatus::Valid||partial.status==DomainAssessmentStatus::Uncertain,
          "bounded work never emits an invalid status for a supported finite model");
    if(std::fesetround(FE_UPWARD)==0) {
        const auto rounding=assessDomain(positiveQuadratic,unit);
        check(rounding.status==DomainAssessmentStatus::Uncertain&&rounding.reason=="unsupported_floating_point_environment",
              "non-nearest rounding mode fails closed");
        (void)std::fesetround(FE_TONEAREST);
    }
    std::cout<<"multivariate rational domain contracts passed; status model rejects unsupported and inconsistent representations\n";
    return 0;
 } catch(const std::exception& e) {std::cerr<<"multivariate rational domain failure: "<<e.what()<<"\n";return 1;}
}
