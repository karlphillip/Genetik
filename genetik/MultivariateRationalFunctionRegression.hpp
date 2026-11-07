#pragma once

// Optional bounded two-variable rational regression. Domain certification is
// available only when the strict runtime IEEE-754 contract is satisfied.
#include "RationalFunctionRegression.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfenv>
#include <cstddef>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace genetik::multivariate_rational {

struct Observation { double x = 0, y = 0, target = 0; };
struct Term { unsigned x = 0, y = 0; friend bool operator==(Term,Term)=default; };
// Graded total-degree order; within a degree, descending x exponent.
inline std::vector<Term> basis(unsigned degree) {
    if (degree > 2) throw std::invalid_argument("multivariate rational total degree must be at most two");
    std::vector<Term> out;
    for (unsigned total = 0; total <= degree; ++total)
        for (unsigned x = total + 1; x-- > 0;)
            out.push_back({x, total - x});
    return out;
}

enum class Status { Success, InvalidInput, InvalidConfiguration, RankDeficient,
                    Underdetermined, IllConditioned, CoefficientBound, ResourceLimit, InvalidModel };
inline const char* statusName(Status s) {
    switch (s) {
    case Status::Success: return "success";
    case Status::InvalidInput: return "invalid_input";
    case Status::InvalidConfiguration: return "invalid_configuration";
    case Status::RankDeficient: return "rank_deficient";
    case Status::Underdetermined: return "underdetermined";
    case Status::IllConditioned: return "ill_conditioned";
    case Status::CoefficientBound: return "coefficient_bound";
    case Status::ResourceLimit: return "resource_limit";
    case Status::InvalidModel: return "invalid_model";
    }
    return "unknown";
}
enum class DomainAssessmentStatus { Valid, Invalid, Uncertain, InvalidModel, InvalidConfiguration };
enum class DomainFinding { SeparatedFromZero, DemonstratedZero, Inconclusive };
enum class PredictionNumericalStatus { NotAssessed, FiniteBounded, OverflowPossible, Uncertain };
struct Rectangle { double xLower=0, xUpper=0, yLower=0, yUpper=0; };
struct Config {
    unsigned maximumNumeratorDegree=2, maximumDenominatorDegree=2;
    std::size_t maximumRows=10000, maximumParameters=11;
    std::size_t maximumWorkspaceBytes=32*1024*1024;
    std::size_t maximumArithmeticOperations=10000000;
    std::size_t maximumLinearAlgebraOperations=100000000;
    double maximumAbsoluteCoefficient=1e6;
    double minimumAbsoluteDenominator=1e-10;
    double minimumPivotRatio=1e-12;
    double complexityWeight=1.0;
    bool refineWithLM=false;
    LMConfig lm{};
    ExpressionLimits expressionLimits{255,16,1e6,2048};
    std::vector<ExpressionKind> operators{ExpressionKind::Add,ExpressionKind::Multiply,
                                         ExpressionKind::Divide,ExpressionKind::Powi};
};
struct Model {
    unsigned numeratorDegree=0, denominatorDegree=0;
    std::vector<Term> numeratorBasis, denominatorBasis;
    std::vector<double> numerator, denominator; // denominator[0] fixed to 1
    Expression expression{std::array{ExpressionNode::constant(0)},2};
    Expression denominatorExpression{std::array{ExpressionNode::constant(1)},2};
    double trainingSSE=std::numeric_limits<double>::infinity();
    double trainingScore=std::numeric_limits<double>::infinity();
    double pivotRatio=0;
    std::size_t parameterCount=0;
    bool refined=false;
    LMWork lmWork{};
    LMStop lmStop=LMStop::InvalidStart;
    std::optional<double> predict(double x, double y,
                                  double minimumDenominator=1e-10) const;
};
struct FitResult {
    Status status=Status::InvalidInput;
    std::string detail;
    std::vector<Model> models;
    std::optional<std::size_t> selected;
    std::size_t attemptedFamilies=0, rankFailures=0, underdeterminedFamilies=0;
    std::size_t illConditionedFamilies=0, coefficientBoundFailures=0, invalidModels=0;
    std::size_t trainingLossEvaluations=0, arithmeticOperations=0;
    std::size_t linearAlgebraOperations=0, peakWorkspaceEstimateBytes=0;
    LMWork lmWork{};
};
struct DomainResult {
    DomainAssessmentStatus status=DomainAssessmentStatus::Uncertain;
    DomainFinding finding=DomainFinding::Inconclusive;
    Rectangle region{};
    std::size_t cellsVisited=0, observedZeros=0;
    double minimumCertifiedSeparation=0;
    PredictionNumericalStatus predictionNumericalStatus=PredictionNumericalStatus::NotAssessed;
    double maximumAbsolutePredictionBound=0;
    std::size_t intervalOperations=0;
    std::string reason="inconclusive";
};

namespace detail {
inline bool finite(const Observation& r) {
    return std::isfinite(r.x)&&std::isfinite(r.y)&&std::isfinite(r.target);
}
inline std::size_t addChecked(std::size_t a, std::size_t b, const char* message) {
    if (b > std::numeric_limits<std::size_t>::max()-a) throw std::overflow_error(message);
    return a+b;
}
inline void validate(const Config& c) {
    if(c.maximumNumeratorDegree>2||c.maximumDenominatorDegree>2||!c.maximumRows||
       !c.maximumParameters||c.maximumParameters>11||!c.maximumWorkspaceBytes||
       !c.maximumArithmeticOperations||!c.maximumLinearAlgebraOperations||
       !std::isfinite(c.maximumAbsoluteCoefficient)||c.maximumAbsoluteCoefficient<=0||
       !std::isfinite(c.minimumAbsoluteDenominator)||c.minimumAbsoluteDenominator<=0||
       !std::isfinite(c.minimumPivotRatio)||c.minimumPivotRatio<=0||c.minimumPivotRatio>=1||
       !std::isfinite(c.complexityWeight)||c.complexityWeight<0)
        throw std::invalid_argument("invalid multivariate rational configuration");
    if(c.maximumAbsoluteCoefficient>c.expressionLimits.maxAbsConstant||
       c.expressionLimits.maxAbsConstant<1.0)
        throw std::invalid_argument("expression limits exclude a bounded coefficient or fixed Q(0,0)=1");
    (void)Expression::constant(0.0,2,c.expressionLimits);
    for(auto op:{ExpressionKind::Add,ExpressionKind::Multiply,ExpressionKind::Divide,ExpressionKind::Powi})
        if(std::find(c.operators.begin(),c.operators.end(),op)==c.operators.end())
            throw std::invalid_argument("rational expression grammar omits required operator");
    for(std::size_t i=0;i<c.operators.size();++i) {
        const auto op=c.operators[i];
        if(op!=ExpressionKind::Add&&op!=ExpressionKind::Multiply&&op!=ExpressionKind::Divide&&op!=ExpressionKind::Powi)
            throw std::invalid_argument("unsupported operator in rational reconstruction grammar");
        if(std::find(c.operators.begin(),c.operators.begin()+static_cast<std::ptrdiff_t>(i),op)!=
           c.operators.begin()+static_cast<std::ptrdiff_t>(i))
            throw std::invalid_argument("duplicate rational reconstruction grammar operator");
    }
}
inline double monomial(double x,double y,Term t,std::size_t& operations) {
    volatile double xp=1.0, yp=1.0;
    for(unsigned i=0;i<t.x;++i){xp=xp*x;operations=addChecked(operations,1,"multivariate rational operation counter overflow");}
    for(unsigned i=0;i<t.y;++i){yp=yp*y;operations=addChecked(operations,1,"multivariate rational operation counter overflow");}
    volatile double value=xp*yp;
    operations=addChecked(operations,1,"multivariate rational operation counter overflow");
    if(!std::isfinite(value)) throw std::overflow_error("multivariate rational polynomial basis overflow");
    return value;
}
inline std::vector<ExpressionNode> polynomialNodes(std::span<const Term> terms,
        std::span<const double> coefficients, bool parameters, std::size_t firstParameter) {
    using K=ExpressionKind;
    if(terms.empty()||terms.size()!=coefficients.size()) throw std::invalid_argument("multivariate rational polynomial shape");
    std::vector<ExpressionNode> out;
    auto appendTerm=[&](std::size_t i) {
        if(parameters) out.push_back(ExpressionNode::variable(firstParameter+i));
        else out.push_back(ExpressionNode::constant(coefficients[i]));
        const auto t=terms[i];
        if(t.x){out.push_back(ExpressionNode::variable(0));if(t.x>1)out.push_back(ExpressionNode::power(t.x));out.push_back(ExpressionNode::binary(K::Multiply));}
        if(t.y){out.push_back(ExpressionNode::variable(1));if(t.y>1)out.push_back(ExpressionNode::power(t.y));out.push_back(ExpressionNode::binary(K::Multiply));}
    };
    appendTerm(0);
    for(std::size_t i=1;i<terms.size();++i){appendTerm(i);out.push_back(ExpressionNode::binary(K::Add));}
    return out;
}
inline Expression buildExpression(std::span<const Term> pTerms,std::span<const double> p,
        std::span<const Term> qTerms,std::span<const double> q,bool parameterized,
        std::size_t denominatorParameterCount,const ExpressionLimits& limits) {
    using K=ExpressionKind;
    auto nodes=polynomialNodes(pTerms,p,parameterized,2+denominatorParameterCount);
    std::vector<ExpressionNode> qNodes;
    if(qTerms.size()!=q.size()+1||qTerms.empty()||qTerms[0].x||qTerms[0].y)
        throw std::invalid_argument("multivariate rational normalized denominator shape");
    qNodes.push_back(ExpressionNode::constant(1.0));
    for(std::size_t i=1;i<qTerms.size();++i){
        if(parameterized)qNodes.push_back(ExpressionNode::variable(2+i-1));
        else qNodes.push_back(ExpressionNode::constant(q[i-1]));
        const auto t=qTerms[i];
        if(t.x){qNodes.push_back(ExpressionNode::variable(0));if(t.x>1)qNodes.push_back(ExpressionNode::power(t.x));qNodes.push_back(ExpressionNode::binary(K::Multiply));}
        if(t.y){qNodes.push_back(ExpressionNode::variable(1));if(t.y>1)qNodes.push_back(ExpressionNode::power(t.y));qNodes.push_back(ExpressionNode::binary(K::Multiply));}
        qNodes.push_back(ExpressionNode::binary(K::Add));
    }
    nodes.insert(nodes.end(),qNodes.begin(),qNodes.end());nodes.push_back(ExpressionNode::binary(K::Divide));
    const auto variableCount=parameterized?2+denominatorParameterCount+p.size():2;
    return Expression(nodes,variableCount,limits);
}
inline Expression denominatorExpression(std::span<const Term> terms,std::span<const double> q,
                                         const ExpressionLimits& limits) {
    using K=ExpressionKind;
    if(terms.size()!=q.size()+1)throw std::invalid_argument("multivariate rational denominator shape");
    std::vector<ExpressionNode> nodes{ExpressionNode::constant(1)};
    for(std::size_t i=1;i<terms.size();++i){
        nodes.push_back(ExpressionNode::constant(q[i-1]));auto t=terms[i];
        if(t.x){nodes.push_back(ExpressionNode::variable(0));if(t.x>1)nodes.push_back(ExpressionNode::power(t.x));nodes.push_back(ExpressionNode::binary(K::Multiply));}
        if(t.y){nodes.push_back(ExpressionNode::variable(1));if(t.y>1)nodes.push_back(ExpressionNode::power(t.y));nodes.push_back(ExpressionNode::binary(K::Multiply));}
        nodes.push_back(ExpressionNode::binary(K::Add));
    }
    return Expression(nodes,2,limits);
}
inline bool consistent(const Model& model) {
    try {
        if(model.numeratorDegree>2||model.denominatorDegree>2||
           model.numeratorBasis!=basis(model.numeratorDegree)||
           model.denominatorBasis!=basis(model.denominatorDegree)||
           model.denominator.size()!=model.denominatorBasis.size()||
           model.denominator.empty()||model.denominator.front()!=1.0||
           model.numerator.size()!=model.numeratorBasis.size()||
           model.parameterCount!=model.numerator.size()+model.denominator.size()-1||
           !std::all_of(model.denominator.begin(),model.denominator.end(),[](double v){return std::isfinite(v);})||
           !std::all_of(model.numerator.begin(),model.numerator.end(),[](double v){return std::isfinite(v);}))return false;
        const auto expectedQ=denominatorExpression(model.denominatorBasis,
            std::span<const double>(model.denominator).subspan(1),model.denominatorExpression.limits());
        if(!genetik_rational_detail::sameNodes(expectedQ,model.denominatorExpression))return false;
        const auto expected=buildExpression(model.numeratorBasis,model.numerator,model.denominatorBasis,
            std::span<const double>(model.denominator).subspan(1),false,model.denominator.size()-1,model.expression.limits());
        return genetik_rational_detail::sameNodes(expected,model.expression);
    } catch(const std::exception&) { return false; }
}
inline std::optional<double> predictKnownConsistent(const Model& model,double x,double y,double minimumDenominator) {
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(minimumDenominator)||minimumDenominator<0)return {};
    const auto q=model.denominatorExpression.evaluate(std::array{x,y});if(!q.valid())return {};
    long double scale=0;
    try{for(std::size_t i=0;i<model.denominator.size();++i){std::size_t ignored=0;scale+=std::abs(static_cast<long double>(model.denominator[i])*monomial(x,y,model.denominatorBasis[i],ignored));}}
    catch(const std::exception&){return {};}
    const double threshold=std::max(minimumDenominator,static_cast<double>(128*std::numeric_limits<double>::epsilon()*
                                      scale*std::max<std::size_t>(1,model.denominatorExpression.nodeCount())));
    if(std::abs(q.value())<=threshold)return {};
    const auto p=model.expression.evaluate(std::array{x,y});if(!p.valid())return {};return p.value();
}
inline std::size_t lmPerIteration(std::size_t rows,std::size_t p) {
    const long double r=rows,n=p;
    const long double v=2*r*n*(n+1)+4*n*n*n+4*n*n+16*(2*n+n*n*n)+1;
    if(!std::isfinite(v)||v>std::numeric_limits<std::size_t>::max())return std::numeric_limits<std::size_t>::max();
    return static_cast<std::size_t>(std::ceil(v));
}
inline void addLm(LMWork& a,const LMWork& b) {
#define MVR_ADD(field) a.field=addChecked(a.field,b.field,"multivariate rational LM counter overflow")
    MVR_ADD(objectiveEvaluations);MVR_ADD(jacobianEvaluations);MVR_ADD(forwardEvaluations);MVR_ADD(derivativeEvaluations);
    MVR_ADD(rowsProcessed);MVR_ADD(parameterUpdates);MVR_ADD(linearSolves);MVR_ADD(acceptedSteps);MVR_ADD(rejectedSteps);
    MVR_ADD(mathematicalOperations);MVR_ADD(linearAlgebraOperations);
#undef MVR_ADD
    a.peakWorkspaceBytes=std::max(a.peakWorkspaceBytes,b.peakWorkspaceBytes);a.seconds+=b.seconds;
}

// Binary64 interval operations assume the documented strict IEEE-754
// round-to-nearest environment. Each primitive result is widened by one
// representable value. Any overflow, NaN, or unsupported rounding mode fails
// closed; no such result may be used to certify separation.
struct Interval { double lo=0,hi=0; bool valid=true; };
inline double down(double x){return std::nextafter(x,-std::numeric_limits<double>::infinity());}
inline double up(double x){return std::nextafter(x,std::numeric_limits<double>::infinity());}
inline Interval add(Interval a,Interval b){
    if(!a.valid||!b.valid)return {0,0,false};
    volatile double l=a.lo+b.lo,h=a.hi+b.hi;
    if(!std::isfinite(l)||!std::isfinite(h))return {0,0,false};
    return {down(l),up(h),true};
}
inline Interval mul(Interval a,Interval b){
    if(!a.valid||!b.valid)return {0,0,false};
    const std::array<double,4> v{a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};
    if(!std::all_of(v.begin(),v.end(),[](double x){return std::isfinite(x);}))return {0,0,false};
    const auto [mn,mx]=std::minmax_element(v.begin(),v.end());return {down(*mn),up(*mx),true};
}
inline Interval square(Interval a){
    if(!a.valid)return {0,0,false};
    volatile double l=a.lo*a.lo,h=a.hi*a.hi;
    if(!std::isfinite(l)||!std::isfinite(h))return {0,0,false};
    const double hi=std::max(l,h),lo=a.lo<=0&&a.hi>=0?0.0:std::min(l,h);
    return {lo==0?0:down(lo),up(hi),true};
}
inline Interval monomialRange(Rectangle r,Term t){
    if(t.x>2||t.y>2||t.x+t.y>2)return {0,0,false};
    Interval x{r.xLower,r.xUpper},y{r.yLower,r.yUpper},v{1,1};
    if(t.x==1)v=mul(v,x);else if(t.x==2)v=mul(v,square(x));
    if(t.y==1)v=mul(v,y);else if(t.y==2)v=mul(v,square(y));
    return v;
}
inline Interval polynomialRange(std::span<const double> coeffs,std::span<const Term> terms,Rectangle r){
    if(coeffs.empty()||coeffs.size()!=terms.size())return {0,0,false};
    const Interval x{r.xLower,r.xUpper},y{r.yLower,r.yUpper};
    auto termRange=[&](std::size_t i){
        const auto t=terms[i];
        if(t.x>2||t.y>2||t.x+t.y>2)return Interval{0,0,false};
        Interval value{coeffs[i],coeffs[i]};
        if(t.x){const auto power=t.x==1?x:square(x);value=mul(value,power);}
        if(t.y){const auto power=t.y==1?y:square(y);value=mul(value,power);}
        return value;
    };
    // This is the operation order used by polynomialNodes(): first term,
    // followed by increasing-basis-order additions.
    Interval sum=termRange(0);
    for(std::size_t i=1;i<coeffs.size();++i)sum=add(sum,termRange(i));
    return sum;
}
inline std::size_t polynomialRangeOperations(std::span<const Term> terms){
    std::size_t operations=0;
    for(std::size_t i=0;i<terms.size();++i){
        const auto t=terms[i];
        if(t.x)operations=addChecked(operations,1,"multivariate rational interval operation count overflow");
        if(t.x==2)operations=addChecked(operations,1,"multivariate rational interval operation count overflow");
        if(t.y)operations=addChecked(operations,1,"multivariate rational interval operation count overflow");
        if(t.y==2)operations=addChecked(operations,1,"multivariate rational interval operation count overflow");
        if(i)operations=addChecked(operations,1,"multivariate rational interval operation count overflow");
    }
    return operations;
}
inline bool supportedIntervalEnvironment(){
#if (!defined(__GNUC__) && !defined(__clang__)) || defined(__FAST_MATH__) || \
    (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0)
    return false;
#else
    if(std::fegetround()!=FE_TONEAREST||!std::numeric_limits<double>::is_iec559||
       std::numeric_limits<double>::radix!=2||
       std::numeric_limits<double>::round_style!=std::round_to_nearest||
       std::numeric_limits<double>::has_denorm==std::denorm_absent)return false;
    volatile double smallest=std::numeric_limits<double>::denorm_min();
    volatile double one=1.0;
    volatile double product=smallest*one;
    return product==std::numeric_limits<double>::denorm_min()&&
           std::nextafter(0.0,1.0)==std::numeric_limits<double>::denorm_min();
#endif
}
inline Interval divide(Interval a,Interval b){
    if(!a.valid||!b.valid||(b.lo<=0&&b.hi>=0))return {0,0,false};
    const std::array<double,4> v{a.lo/b.lo,a.lo/b.hi,a.hi/b.lo,a.hi/b.hi};
    if(!std::all_of(v.begin(),v.end(),[](double x){return std::isfinite(x);}))return {0,0,false};
    const auto [mn,mx]=std::minmax_element(v.begin(),v.end());return {down(*mn),up(*mx)};
}
inline double directDenominator(const Model& model,double x,double y){
    auto v=model.denominatorExpression.evaluate(std::array{x,y});if(!v.valid())return std::numeric_limits<double>::quiet_NaN();return v.value();
}
} // namespace detail

inline bool modelIsConsistent(const Model& model) { return detail::consistent(model); }

inline std::optional<double> Model::predict(double x,double y,double minimumDenominator) const {
    if(!detail::consistent(*this))return {};
    return detail::predictKnownConsistent(*this,x,y,minimumDenominator);
}

inline FitResult fit(std::span<const Observation> rows,const Config& c={}) {
    FitResult out;
    try{detail::validate(c);}catch(const std::exception& e){out.status=Status::InvalidConfiguration;out.detail=e.what();return out;}
    if(rows.empty()||rows.size()>c.maximumRows){out.status=rows.empty()?Status::InvalidInput:Status::ResourceLimit;out.detail="training row count outside configured bound";return out;}
    for(const auto& r:rows)if(!detail::finite(r)){out.status=Status::InvalidInput;out.detail="nonfinite observation";return out;}
    const auto epsilon=std::numeric_limits<double>::epsilon();double maxTarget=0;for(const auto& r:rows)maxTarget=std::max(maxTarget,std::abs(r.target));
    for(unsigned nd=0;nd<=c.maximumDenominatorDegree;++nd)for(unsigned np=0;np<=c.maximumNumeratorDegree;++np){
        const auto pt= basis(np), qt=basis(nd);const std::size_t qfree=qt.size()-1,pcount=qfree+pt.size();
        if(pcount>c.maximumParameters)continue;
        if(rows.size()<pcount){++out.underdeterminedFamilies;continue;}
        ++out.attemptedFamilies;
        const long double bytes=static_cast<long double>(rows.size())*pcount*(sizeof(double)+sizeof(long double))+
            static_cast<long double>(rows.size())*(sizeof(std::vector<double>)+2*sizeof(long double))+pcount*8*sizeof(long double);
        if(bytes>c.maximumWorkspaceBytes){out.status=Status::ResourceLimit;out.detail="bounded QR workspace limit";return out;}
        std::vector<std::vector<double>> design;std::vector<double> rhs;design.reserve(rows.size());rhs.reserve(rows.size());
        std::vector<double> columnScale(pcount,0);std::size_t buildOps=0;
        try{
            for(const auto& r:rows){std::vector<double> row;row.reserve(pcount);
                for(std::size_t j=1;j<qt.size();++j)row.push_back(-r.target*detail::monomial(r.x,r.y,qt[j],buildOps));
                for(auto t:pt)row.push_back(detail::monomial(r.x,r.y,t,buildOps));
                for(std::size_t j=0;j<pcount;++j){if(!std::isfinite(row[j]))throw std::overflow_error("multivariate rational linearized design overflow");columnScale[j]=std::max(columnScale[j],std::abs(row[j]));}
                rhs.push_back(r.target);design.push_back(std::move(row));
                buildOps=detail::addChecked(buildOps,pcount,"multivariate rational operation counter overflow");
            }
        }catch(const std::overflow_error& e){++out.invalidModels;out.detail=e.what();continue;}
        if(buildOps>c.maximumArithmeticOperations-out.arithmeticOperations){out.status=Status::ResourceLimit;out.detail="basis construction operation limit";return out;}
        out.arithmeticOperations+=buildOps;
        for(std::size_t j=0;j<pcount;++j){if(columnScale[j]==0){columnScale[j]=1;continue;}for(auto& row:design)row[j]/=columnScale[j];}
        const long double qrEstimate=2.0L*rows.size()*pcount*pcount-(2.0L/3.0L)*pcount*pcount*pcount+2.0L*pcount*pcount;
        if(out.linearAlgebraOperations>c.maximumLinearAlgebraOperations||!std::isfinite(qrEstimate)||qrEstimate>static_cast<long double>(c.maximumLinearAlgebraOperations-out.linearAlgebraOperations)){
            out.status=Status::ResourceLimit;out.detail="QR linear algebra allowance";return out;}
        out.linearAlgebraOperations+=static_cast<std::size_t>(qrEstimate);
        genetik_rational_detail::LeastSquaresResult solved;
        try{solved=genetik_rational_detail::householderLeastSquaresDetailed(design,rhs,c.maximumRows,11,c.maximumWorkspaceBytes);}
        catch(const std::invalid_argument& e){++out.rankFailures;out.detail=e.what();continue;}
        catch(const std::length_error& e){out.status=Status::ResourceLimit;out.detail=e.what();return out;}
        out.peakWorkspaceEstimateBytes=std::max(out.peakWorkspaceEstimateBytes,static_cast<std::size_t>(bytes));
        if(solved.pivotRatio<c.minimumPivotRatio){++out.illConditionedFamilies;out.detail="column-scaled QR pivot ratio below configured minimum for degree pair "+std::to_string(np)+"/"+std::to_string(nd);continue;}
        std::vector<double> theta=solved.coefficients;for(std::size_t j=0;j<pcount;++j)theta[j]/=columnScale[j];
        bool bounded=true;for(double v:theta)if(!std::isfinite(v)||std::abs(v)>c.maximumAbsoluteCoefficient){bounded=false;break;}
        if(!bounded){++out.coefficientBoundFailures;continue;}
        Model model;model.numeratorDegree=np;model.denominatorDegree=nd;model.numeratorBasis=pt;model.denominatorBasis=qt;
        model.parameterCount=pcount;model.pivotRatio=solved.pivotRatio;
        auto unpack=[&]{model.denominator.assign(1,1.0);for(std::size_t i=0;i<qfree;++i)model.denominator.push_back(theta[i]);model.numerator.assign(theta.begin()+static_cast<std::ptrdiff_t>(qfree),theta.end());};
        unpack();
        if(c.refineWithLM){
            try{
                const auto parameterized=detail::buildExpression(pt,model.numerator,qt,std::span<const double>(model.denominator).subspan(1),true,qfree,c.expressionLimits);
                std::vector<LMExpressionObservation> lmRows;lmRows.reserve(rows.size());for(const auto& r:rows){std::vector<double> input(parameterized.variableCount(),0);input[0]=r.x;input[1]=r.y;lmRows.push_back({std::move(input),r.target});}
                std::vector<std::size_t> bindings(pcount);for(std::size_t i=0;i<pcount;++i)bindings[i]=i+2;
                auto lmConfig=c.lm;lmConfig.maxWorkspaceBytes=std::min(lmConfig.maxWorkspaceBytes,c.maximumWorkspaceBytes);
                const auto remainingLa=c.maximumLinearAlgebraOperations-out.linearAlgebraOperations;
                const auto per=detail::lmPerIteration(rows.size(),pcount);const auto allowed=per?remainingLa/per:0;
                if(!allowed){out.status=Status::ResourceLimit;out.detail="no LM iteration fits combined linear algebra budget";return out;}
                lmConfig.maxIterations=std::min(lmConfig.maxIterations,allowed);
                const auto remainingOps=c.maximumArithmeticOperations-out.arithmeticOperations;
                if(out.lmWork.mathematicalOperations>remainingOps){out.status=Status::ResourceLimit;out.detail="LM arithmetic allowance";return out;}
                lmConfig.maxMathematicalOperations=std::min(lmConfig.maxMathematicalOperations,remainingOps-out.lmWork.mathematicalOperations);
                auto objective=expressionLeastSquares(parameterized,std::move(lmRows),bindings,c.maximumArithmeticOperations);
                std::vector<double> lower(pcount,-c.maximumAbsoluteCoefficient),upper(pcount,c.maximumAbsoluteCoefficient);
                const auto lm=levenbergMarquardt(theta,lower,upper,objective,lmConfig);
                theta=lm.parameters;model.refined=true;model.lmWork=lm.work;model.lmStop=lm.stop;detail::addLm(out.lmWork,lm.work);
                out.arithmeticOperations=detail::addChecked(out.arithmeticOperations,lm.work.mathematicalOperations,"multivariate rational LM operations overflow");
                out.linearAlgebraOperations=detail::addChecked(out.linearAlgebraOperations,lm.work.linearAlgebraOperations,"multivariate rational LM linear algebra overflow");
                if(out.arithmeticOperations>c.maximumArithmeticOperations||out.linearAlgebraOperations>c.maximumLinearAlgebraOperations){out.status=Status::ResourceLimit;out.detail="combined QR and LM allowance exceeded";return out;}
                unpack();
            }catch(const std::length_error& e){out.status=Status::ResourceLimit;out.detail=e.what();return out;}
             catch(const std::exception& e){++out.invalidModels;out.detail=e.what();continue;}
        }
        try{model.expression=detail::buildExpression(pt,model.numerator,qt,std::span<const double>(model.denominator).subspan(1),false,qfree,c.expressionLimits);
            model.denominatorExpression=detail::denominatorExpression(qt,std::span<const double>(model.denominator).subspan(1),c.expressionLimits);}
        catch(const std::length_error& e){out.status=Status::ResourceLimit;out.detail=e.what();return out;}
        catch(const std::exception& e){++out.invalidModels;out.detail=e.what();continue;}
        const auto constructionOps=detail::addChecked(model.expression.nodeCount(),model.denominatorExpression.nodeCount(),"multivariate rational construction operation overflow");
        if(constructionOps>c.maximumArithmeticOperations-out.arithmeticOperations){out.status=Status::ResourceLimit;out.detail="expression materialization operation allowance";return out;}
        out.arithmeticOperations+=constructionOps;
        double sse=0;bool valid=true;
        std::size_t scaleOps=0;for(const auto t:model.denominatorBasis)scaleOps=detail::addChecked(scaleOps,t.x+t.y+2,"multivariate rational operation counter overflow");
        const std::size_t rowOps=model.expression.arithmeticCost()+model.denominatorExpression.arithmeticCost()+3+scaleOps;
        if(rows.size()&&rowOps>(c.maximumArithmeticOperations-out.arithmeticOperations)/rows.size()){out.status=Status::ResourceLimit;out.detail="prediction loss allowance";return out;}
        for(const auto& r:rows){auto pred=detail::predictKnownConsistent(model,r.x,r.y,c.minimumAbsoluteDenominator);if(!pred){valid=false;break;}volatile double residual=*pred-r.target;volatile double sq=residual*residual;sse+=sq;if(!std::isfinite(sse)){valid=false;break;}}
        out.arithmeticOperations=detail::addChecked(out.arithmeticOperations,rowOps*rows.size(),"multivariate rational prediction operations overflow");
        ++out.trainingLossEvaluations;if(!valid){++out.invalidModels;continue;}
        model.trainingSSE=sse;const double floor=epsilon*(maxTarget+1.0);const double mean=std::max(sse/rows.size(),floor*floor);
        model.trainingScore=rows.size()*std::log(mean)+c.complexityWeight*pcount*std::log(static_cast<double>(rows.size()));
        const auto rankingOps=detail::addChecked(pcount,2,"multivariate rational ranking operation overflow");
        if(rankingOps>c.maximumArithmeticOperations-out.arithmeticOperations){out.status=Status::ResourceLimit;out.detail="model ranking operation allowance";return out;}
        out.arithmeticOperations+=rankingOps;
        out.models.push_back(std::move(model));
    }
    if(out.models.empty()){
        out.status=out.illConditionedFamilies?Status::IllConditioned:
                   out.rankFailures?Status::RankDeficient:
                   out.coefficientBoundFailures?Status::CoefficientBound:
                   out.underdeterminedFamilies?Status::Underdetermined:Status::InvalidModel;
        out.detail=out.detail.empty()?"no valid bounded model":out.detail;return out;
    }
    out.selected=0;for(std::size_t i=1;i<out.models.size();++i)if(out.models[i].trainingScore<out.models[*out.selected].trainingScore||
        (out.models[i].trainingScore==out.models[*out.selected].trainingScore&&out.models[i].expression.nodeCount()<out.models[*out.selected].expression.nodeCount()))out.selected=i;
    out.status=Status::Success;out.detail="bounded two-variable normalized rational family fitted";return out;
}

inline DomainResult assessDomain(const Model& model,Rectangle region,std::size_t maximumCells=1024,
                                 unsigned maximumDepth=10,double minimumSeparation=1e-10) {
    DomainResult out;out.region=region;
    if(!detail::consistent(model)){out.status=DomainAssessmentStatus::InvalidModel;out.reason="invalid_or_unsupported_model";return out;}
    if(!maximumCells||maximumDepth>20||!std::isfinite(minimumSeparation)||minimumSeparation<0||
       !std::isfinite(region.xLower)||!std::isfinite(region.xUpper)||!std::isfinite(region.yLower)||!std::isfinite(region.yUpper)||
       region.xLower>region.xUpper||region.yLower>region.yUpper){out.status=DomainAssessmentStatus::InvalidConfiguration;out.reason="invalid_domain_configuration";return out;}
    if(!detail::supportedIntervalEnvironment()){out.status=DomainAssessmentStatus::Uncertain;out.reason="unsupported_floating_point_environment";return out;}
    struct Cell{Rectangle r;unsigned depth;};std::vector<Cell> pending{{region,0}};bool unresolved=false,resourceLimited=false;double minSep=std::numeric_limits<double>::infinity();
    double maxPrediction=0;bool predictionBoundAvailable=true;bool predictionOverflow=false;
    while(!pending.empty()){
        auto cell=pending.back();pending.pop_back();++out.cellsVisited;
        bool positive=false,negative=false;
        for(const auto& p:std::array<std::array<double,2>,4>{{{{cell.r.xLower,cell.r.yLower}},{{cell.r.xLower,cell.r.yUpper}},{{cell.r.xUpper,cell.r.yLower}},{{cell.r.xUpper,cell.r.yUpper}}}}){
            const auto point=detail::polynomialRange(model.denominator,model.denominatorBasis,{p[0],p[0],p[1],p[1]});
            out.intervalOperations=detail::addChecked(out.intervalOperations,
                detail::polynomialRangeOperations(model.denominatorBasis),"multivariate rational domain counter overflow");
            if(!point.valid){unresolved=true;continue;}
            positive=positive||point.lo>0;negative=negative||point.hi<0;
        }
        if(positive&&negative){out.status=DomainAssessmentStatus::Invalid;out.finding=DomainFinding::DemonstratedZero;out.reason="opposite_certified_denominator_signs";return out;}
        auto range=detail::polynomialRange(model.denominator,model.denominatorBasis,cell.r);
        out.intervalOperations=detail::addChecked(out.intervalOperations,
            detail::polynomialRangeOperations(model.denominatorBasis),"multivariate rational domain counter overflow");
        const bool separated=range.valid&&(range.lo>minimumSeparation||range.hi< -minimumSeparation);
        if(separated){
            const auto separation=range.lo>minimumSeparation?range.lo:-range.hi;
            minSep=std::min(minSep,static_cast<double>(separation));
            if(predictionBoundAvailable){
                auto nr=detail::polynomialRange(model.numerator,model.numeratorBasis,cell.r);
                out.intervalOperations=detail::addChecked(out.intervalOperations,
                    detail::polynomialRangeOperations(model.numeratorBasis),"multivariate rational domain counter overflow");
                const auto quotient=detail::divide(nr,range);
                out.intervalOperations=detail::addChecked(out.intervalOperations,1,"multivariate rational domain counter overflow");
                if(!quotient.valid)predictionOverflow=true;
                const double bound=quotient.valid?std::max(std::abs(quotient.lo),std::abs(quotient.hi)):std::numeric_limits<double>::infinity();
                if(!std::isfinite(bound))predictionOverflow=true;else maxPrediction=std::max(maxPrediction,bound);
                if(bound>std::numeric_limits<double>::max())predictionOverflow=true;
            }
            continue;
        }
        unresolved=unresolved||!range.valid;
        const bool enoughCellBudget=out.cellsVisited<=maximumCells&&
            pending.size()<=maximumCells-out.cellsVisited&&
            maximumCells-out.cellsVisited-pending.size()>=4;
        if(cell.depth>=maximumDepth||!enoughCellBudget){unresolved=true;resourceLimited=true;continue;}
        const double xm=std::midpoint(cell.r.xLower,cell.r.xUpper),ym=std::midpoint(cell.r.yLower,cell.r.yUpper);
        pending.push_back({{cell.r.xLower,xm,cell.r.yLower,ym},cell.depth+1});
        pending.push_back({{xm,cell.r.xUpper,cell.r.yLower,ym},cell.depth+1});
        pending.push_back({{cell.r.xLower,xm,ym,cell.r.yUpper},cell.depth+1});
        pending.push_back({{xm,cell.r.xUpper,ym,cell.r.yUpper},cell.depth+1});
    }
    out.minimumCertifiedSeparation=std::isfinite(minSep)?minSep:0;
    if(!unresolved){
        out.status=DomainAssessmentStatus::Valid;out.finding=DomainFinding::SeparatedFromZero;out.reason="denominator_interval_separated_from_zero";
        out.maximumAbsolutePredictionBound=predictionOverflow?std::numeric_limits<double>::infinity():static_cast<double>(maxPrediction);
        out.predictionNumericalStatus=!predictionBoundAvailable?PredictionNumericalStatus::Uncertain:
            predictionOverflow?PredictionNumericalStatus::OverflowPossible:PredictionNumericalStatus::FiniteBounded;
    } else {out.status=DomainAssessmentStatus::Uncertain;out.finding=DomainFinding::Inconclusive;out.reason=resourceLimited?"domain_subdivision_resource_limit":"numerical_interval_enclosure_inconclusive";out.predictionNumericalStatus=PredictionNumericalStatus::Uncertain;}
    return out;
}

} // namespace genetik::multivariate_rational
