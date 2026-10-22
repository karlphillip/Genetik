#pragma once
#include "Expression.hpp"
#include "Execution.hpp"
#include <array>
#include <numeric>
#include <locale>
#include <iomanip>
#include <sstream>

struct IntegralPolicy {
    double absTolerance=1e-10, relTolerance=1e-10;
    std::size_t maxIntegrandEvaluations=4096, maxExpressionOperations=1000000, maxNodeVisits=1000000, maxBindingsValidated=1000000;
    std::size_t maxQuadratureOperations=1000000, maxSubintervals=256, maxDepth=32;
    std::size_t maxWorkspaceBytes=8*1024*1024, maxModelNodes=65536, maxExternalInputs=65536;
};
enum class IntegralStatus { EstimatedConvergence, MathematicalInvalidity, NumericalFailure, ResourceLimit, Nonconvergence, Resolution, Cancelled };
enum class IntegralReason { None, LowerBound, UpperBound, Integrand, Arithmetic, Samples, ExpressionWork, NodeVisits, Bindings, QuadratureWork, Panels, Depth, Workspace, Roundoff, Collapsed, Cancellation };
inline const char* integralStatusName(IntegralStatus s) {
    switch(s) {
    case IntegralStatus::EstimatedConvergence:return "estimated_convergence";
    case IntegralStatus::MathematicalInvalidity:return "mathematical_invalidity";
    case IntegralStatus::NumericalFailure:return "numerical_failure";
    case IntegralStatus::ResourceLimit:return "resource_limit";
    case IntegralStatus::Nonconvergence:return "nonconvergence";
    case IntegralStatus::Resolution:return "resolution";
    case IntegralStatus::Cancelled:return "cancelled";
    }
    throw std::logic_error("unknown integral status");
}
struct IntegralWork {
    std::uint64_t integrals=0,boundEvaluations=0,integrandEvaluations=0,expressionOperations=0,nodeVisits=0,bindingsValidated=0,quadratureOperations=0;
    std::uint64_t lossArithmeticOperations=0,completedPanels=0,subdivisions=0,successes=0,mathematicalFailures=0,numericalFailures=0,resources=0,nonconvergence=0,resolution=0,cancellations=0;
    std::uint64_t workspaceBytes=0; // peak planned payload, not an additive work category
    bool operator==(const IntegralWork&) const=default;
};
inline void mergeIntegralWork(IntegralWork& a,const IntegralWork& b,bool& overflow) noexcept {
    const auto add=[&](std::uint64_t& x,std::uint64_t y){if(y>UINT64_MAX-x){x=UINT64_MAX;overflow=true;}else x+=y;};
#define GENETIK_INTEGRAL_ADD(f) add(a.f,b.f);
    GENETIK_INTEGRAL_ADD(integrals) GENETIK_INTEGRAL_ADD(boundEvaluations) GENETIK_INTEGRAL_ADD(integrandEvaluations)
    GENETIK_INTEGRAL_ADD(expressionOperations) GENETIK_INTEGRAL_ADD(nodeVisits) GENETIK_INTEGRAL_ADD(bindingsValidated) GENETIK_INTEGRAL_ADD(quadratureOperations)
    GENETIK_INTEGRAL_ADD(lossArithmeticOperations) GENETIK_INTEGRAL_ADD(completedPanels) GENETIK_INTEGRAL_ADD(subdivisions) GENETIK_INTEGRAL_ADD(successes)
    GENETIK_INTEGRAL_ADD(mathematicalFailures) GENETIK_INTEGRAL_ADD(numericalFailures) GENETIK_INTEGRAL_ADD(resources)
    GENETIK_INTEGRAL_ADD(nonconvergence) GENETIK_INTEGRAL_ADD(resolution) GENETIK_INTEGRAL_ADD(cancellations)
#undef GENETIK_INTEGRAL_ADD
    a.workspaceBytes=std::max(a.workspaceBytes,b.workspaceBytes);
}
struct IntegralResult {
    IntegralStatus status=IntegralStatus::ResourceLimit;
    IntegralReason reason=IntegralReason::None;
    std::optional<double> estimate, estimatedAbsoluteError; // last COMPLETE partition, diagnostic on failure
    std::size_t activeSubintervals=0;
    IntegralWork work;
    bool accepted() const noexcept{return status==IntegralStatus::EstimatedConvergence;}
};
namespace genetik_detail {class IntegralWorkspace;}
class IntegralExpression final {
public:
    IntegralExpression(std::size_t externalInputs,const Expression& lower,const Expression& upper,
                       const Expression& integrand,std::size_t localSlot,IntegralPolicy policy={})
        :policy_(validate(externalInputs,lower,upper,integrand,localSlot,policy)),external_(externalInputs),lower_(lower),upper_(upper),integrand_(integrand) {}
    std::size_t externalDimensions() const noexcept{return external_;}
    std::size_t localIndex() const noexcept{return external_;}
    const Expression& lower() const noexcept{return lower_;}
    const Expression& upper() const noexcept{return upper_;}
    const Expression& integrand() const noexcept{return integrand_;}
    IntegralPolicy policy() const noexcept{return policy_;}
    IntegralResult evaluate(std::span<const double> inputs,const ExecutionContext* execution=nullptr) const;
    std::string toString() const {
        std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17);
        s<<"integral[external="<<external_<<",local=x"<<external_<<",lower="<<lower_.toString()<<",upper="<<upper_.toString()
         <<",integrand="<<integrand_.toString()<<"] policy[gk7/15,abs="<<policy_.absTolerance<<",rel="<<policy_.relTolerance
         <<",samples="<<policy_.maxIntegrandEvaluations<<",panels="<<policy_.maxSubintervals<<",depth="<<policy_.maxDepth
         <<",expression_ops="<<policy_.maxExpressionOperations<<",node_visits="<<policy_.maxNodeVisits<<",bindings="<<policy_.maxBindingsValidated<<",quadrature_ops="<<policy_.maxQuadratureOperations
         <<",workspace="<<policy_.maxWorkspaceBytes<<",model_nodes="<<policy_.maxModelNodes<<",external_inputs="<<policy_.maxExternalInputs<<"]";
        return s.str();
    }
private:
    static IntegralPolicy validate(std::size_t n,const Expression& a,const Expression& b,const Expression& f,std::size_t slot,IntegralPolicy p) {
        if(n==std::numeric_limits<std::size_t>::max() || n>p.maxExternalInputs || slot!=n || a.variableCount()!=n || b.variableCount()!=n || f.variableCount()!=n+1)
            throw std::invalid_argument("integral schema requires bounds[N], integrand[N+1], local=N");
        if(!std::isfinite(p.absTolerance) || p.absTolerance<0 || !std::isfinite(p.relTolerance) || p.relTolerance<0 || p.relTolerance>1 || (p.absTolerance==0 && p.relTolerance==0))
            throw std::invalid_argument("integral tolerances require abs>=0, 0<=rel<=1, at least one positive");
        if(n>=std::vector<double>().max_size())throw std::length_error("integral binding storage size");
        auto remaining=p.maxModelNodes;
        for(auto count:{a.nodeCount(),b.nodeCount(),f.nodeCount()}){if(count>remaining)throw std::length_error("integral model node allowance");remaining-=count;}
        return p;
    }
    const IntegralPolicy policy_;
    const std::size_t external_;
    const Expression lower_,upper_,integrand_;
};

namespace genetik_detail {
struct IntegralStop {};
class IntegralWorkspace final {
    struct Panel {double a,b,value,error,floor;std::size_t depth;};
    EvaluationWorkspace operands_;
    std::vector<double> bindings_;
    std::vector<Panel> panels_;
    // QUADPACK dqk15 positive abscissae/weights, published by Piessens/de Doncker.
    static constexpr std::array<double,7> xgk{.991455371120812639206854697526329,.949107912342758524526189684047851,.864864423359769072789712788640926,.741531185599394439863864773280788,.586087235467691130294144838258730,.405845151377397166906606412076961,.207784955007898467600689403773245};
    static constexpr std::array<double,8> wk{.022935322010529224963732008058970,.063092092629978553290700663189204,.104790010322250183839876322541518,.140653259715525918745189590510238,.169004726639267902826583426598550,.190350578064785409913256402421014,.204432940075298892414161999234649,.209482141084727828012999174891714};
    static constexpr std::array<double,4> wg{.129484966168869693270611432679082,.279705391489276667901467771423780,.381830050505118944950369775488975,.417959183673469387755102040816327};
public:
    // w is caller-owned so attempted work survives operational exceptions as well.
    IntegralResult evaluate(const IntegralExpression& model,std::span<const double> inputs,IntegralWork& w,const ExecutionContext* execution=nullptr) {
        if(inputs.size()!=model.externalDimensions())throw std::invalid_argument("integral external binding count differs");
        for(double x:inputs)if(!std::isfinite(x))throw std::invalid_argument("integral bindings must be finite");
        const auto p=model.policy();IntegralResult result;panels_.clear();
        const auto inc=[](std::uint64_t& x){if(x==UINT64_MAX)throw std::overflow_error("integral counter exhausted");++x;};
        inc(w.integrals);
        auto stop=[&](IntegralStatus status,IntegralReason reason) {
            result.status=status;result.reason=reason;
            switch(status){
            case IntegralStatus::MathematicalInvalidity:inc(w.mathematicalFailures);break;
            case IntegralStatus::NumericalFailure:inc(w.numericalFailures);break;
            case IntegralStatus::ResourceLimit:inc(w.resources);break;
            case IntegralStatus::Nonconvergence:inc(w.nonconvergence);break;
            case IntegralStatus::Resolution:inc(w.resolution);break;
            case IntegralStatus::Cancelled:inc(w.cancellations);break;
            default:break;
            }
            throw IntegralStop{};
        };
        auto cancel=[&]{if(execution && execution->stopRequested())stop(IntegralStatus::Cancelled,IntegralReason::Cancellation);};
        auto resource=[&](IntegralReason r){stop(IntegralStatus::ResourceLimit,r);};
        auto charge=[&]{if(w.quadratureOperations==p.maxQuadratureOperations)resource(IntegralReason::QuadratureWork);inc(w.quadratureOperations);};
        auto arithmetic=[&](ExpressionKind k,double a,double b) {
            auto x=scalarBinary(k,a,b,charge);if(!x)stop(IntegralStatus::NumericalFailure,IntegralReason::Arithmetic);return *x;
        };
        const auto add=[&](double a,double b){return arithmetic(ExpressionKind::Add,a,b);};
        const auto sub=[&](double a,double b){return arithmetic(ExpressionKind::Subtract,a,b);};
        const auto mul=[&](double a,double b){return arithmetic(ExpressionKind::Multiply,a,b);};
        const auto div=[&](double a,double b){return arithmetic(ExpressionKind::Divide,a,b);};
        auto expression=[&](const Expression& e,std::span<const double> values,IntegralReason invalid) {
            cancel();
            if(e.arithmeticCost()>p.maxExpressionOperations-w.expressionOperations)resource(IntegralReason::ExpressionWork);
            if(e.nodeCount()>p.maxNodeVisits-w.nodeVisits)resource(IntegralReason::NodeVisits);
            if(values.size()>p.maxBindingsValidated-w.bindingsValidated)resource(IntegralReason::Bindings);
            w.bindingsValidated+=values.size();
            auto r=operands_.evaluate(e,values);
            w.expressionOperations+=r.arithmeticOperations();w.nodeVisits+=r.failedNode()?*r.failedNode()+1:e.nodeCount();
            if(!r.valid())stop(IntegralStatus::MathematicalInvalidity,invalid);
            return r.value();
        };
        bool reversed=false;
        try {
            // Checked planned payload: capacity for panels, two private children,
            // fifteen node/value pairs, bindings and the largest operand plan.
            if(p.maxSubintervals>panels_.max_size() || model.externalDimensions()+1>bindings_.max_size())resource(IntegralReason::Workspace);
            std::size_t payload=0;
            auto bytes=[&](std::size_t count,std::size_t element){if(count>(p.maxWorkspaceBytes-payload)/element)resource(IntegralReason::Workspace);payload+=count*element;};
            bytes(p.maxSubintervals,sizeof(Panel));bytes(3,sizeof(Panel));bytes(30,sizeof(double));
            bytes(model.externalDimensions()+1,sizeof(double));
            bytes(std::max({model.lower().nodeCount(),model.upper().nodeCount(),model.integrand().nodeCount()}),sizeof(double));
            w.workspaceBytes=payload;
            cancel();inc(w.boundEvaluations);const double originalA=expression(model.lower(),inputs,IntegralReason::LowerBound);
            cancel();inc(w.boundEvaluations);const double originalB=expression(model.upper(),inputs,IntegralReason::UpperBound);
            if(originalA==originalB){result.estimate=0.0;result.estimatedAbsoluteError=0.0;result.status=IntegralStatus::EstimatedConvergence;inc(w.successes);result.work=w;return result;}
            reversed=originalA>originalB;
            const double a=std::min(originalA,originalB),b=std::max(originalA,originalB);
            bindings_.assign(inputs.begin(),inputs.end());bindings_.push_back(0);
            auto sample=[&](double t) {
                cancel();if(w.integrandEvaluations==p.maxIntegrandEvaluations)resource(IntegralReason::Samples);
                inc(w.integrandEvaluations);bindings_.back()=t;return expression(model.integrand(),bindings_,IntegralReason::Integrand);
            };
            (void)sample(originalA);(void)sample(originalB);
            if(!p.maxSubintervals)resource(IntegralReason::Panels);
            panels_.reserve(p.maxSubintervals);
            auto panel=[&](double left,double right,std::size_t depth) {
                cancel();
                charge();const double center=std::midpoint(left,right);
                // Direct subtraction is executed only if representable; halves handle opposite extremes.
                double half;
                if(left<0 && right>0 && right>add(std::numeric_limits<double>::max(),left))half=sub(mul(right,.5),mul(left,.5));
                else half=mul(sub(right,left),.5);
                if(!(half>0) || center==left || center==right)stop(IntegralStatus::Resolution,IntegralReason::Collapsed);
                std::array<double,15> coords{},values{};coords[0]=center;
                // Validate ALL coordinates before using this panel's samples.
                for(std::size_t j=0;j<7;++j) {
                    const double offset=mul(half,xgk[j]);coords[1+2*j]=sub(center,offset);coords[2+2*j]=add(center,offset);
                }
                for(std::size_t i=0;i<15;++i){if(!(coords[i]>left && coords[i]<right))stop(IntegralStatus::Resolution,IntegralReason::Collapsed);for(std::size_t j=0;j<i;++j)if(coords[j]==coords[i])stop(IntegralStatus::Resolution,IntegralReason::Collapsed);}
                for(std::size_t i=0;i<15;++i)values[i]=sample(coords[i]);
                double gauss=mul(values[0],wg[3]),kronrod=mul(values[0],wk[7]),absolute=std::abs(kronrod);
                for(std::size_t j=0;j<7;++j) {
                    const double pair=add(values[1+2*j],values[2+2*j]);
                    kronrod=add(kronrod,mul(wk[j],pair));
                    absolute=add(absolute,mul(wk[j],add(std::abs(values[1+2*j]),std::abs(values[2+2*j]))));
                    if(j%2==1)gauss=add(gauss,mul(wg[j/2],pair));
                }
                const double mean=mul(kronrod,.5);
                double variation=mul(wk[7],std::abs(sub(values[0],mean)));
                for(std::size_t j=0;j<7;++j)variation=add(variation,mul(wk[j],add(std::abs(sub(values[1+2*j],mean)),std::abs(sub(values[2+2*j],mean)))));
                const double value=mul(kronrod,half),magnitude=mul(absolute,half),asc=mul(variation,half);
                double error=std::abs(mul(sub(kronrod,gauss),half));
                if(asc>0 && error>0) {
                    double factor=1;
                    if(error<div(asc,200)) {const double ratio=mul(200,div(error,asc));charge();const double root=std::sqrt(ratio);if(!std::isfinite(root))stop(IntegralStatus::NumericalFailure,IntegralReason::Arithmetic);factor=mul(ratio,root);}
                    error=mul(asc,std::min(1.,factor));
                }
                const double floor=mul(50*std::numeric_limits<double>::epsilon(),magnitude);
                error=std::max(error,floor);
                inc(w.completedPanels);return Panel{left,right,value,error,floor,depth};
            };
            panels_.push_back(panel(a,b,0));
            auto total=[&](std::optional<std::size_t> replace,const Panel* l,const Panel* r) {
                double value=0,error=0,floor=0;
                auto append=[&](const Panel& x){value=add(value,x.value);error=add(error,x.error);floor=add(floor,x.floor);};
                for(std::size_t i=0;i<panels_.size();++i){if(replace && *replace==i){append(*l);append(*r);}else append(panels_[i]);}
                return std::array<double,3>{value,error,floor};
            };
            auto totals=total({},nullptr,nullptr);
            auto retain=[&]{result.estimate=reversed?-totals[0]:totals[0];result.estimatedAbsoluteError=totals[1];result.activeSubintervals=panels_.size();};
            retain();
            for(;;) {
                cancel();const double tolerance=std::max(p.absTolerance,mul(p.relTolerance,std::abs(totals[0])));
                if(totals[1]<=tolerance){result.status=IntegralStatus::EstimatedConvergence;inc(w.successes);break;}
                if(totals[1]<=totals[2])stop(IntegralStatus::Nonconvergence,IntegralReason::Roundoff);
                if(panels_.size()==p.maxSubintervals)resource(IntegralReason::Panels);
                std::size_t selected=0;
                for(std::size_t i=1;i<panels_.size();++i)if(panels_[i].error>panels_[selected].error)selected=i;
                const auto parent=panels_[selected];
                if(parent.depth>=p.maxDepth)resource(IntegralReason::Depth);
                charge();const double middle=std::midpoint(parent.a,parent.b);
                if(middle==parent.a || middle==parent.b)stop(IntegralStatus::Resolution,IntegralReason::Collapsed);
                const auto l=panel(parent.a,middle,parent.depth+1);
                const auto r=panel(middle,parent.b,parent.depth+1);
                const auto next=total(selected,&l,&r); // can fail; old partition is still intact
                panels_[selected]=l;panels_.insert(panels_.begin()+selected+1,r);totals=next;
                inc(w.subdivisions);retain();
            }
        }catch(const IntegralStop&){}
        result.work=w;return result;
    }
};
}
inline IntegralResult IntegralExpression::evaluate(std::span<const double> inputs,const ExecutionContext* execution) const {
    genetik_detail::IntegralWorkspace workspace;IntegralWork work;return workspace.evaluate(*this,inputs,work,execution);
}
