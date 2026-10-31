#pragma once

// Optional exact feasibility tools for affine quantized expressions.
#include "BoundedRational.hpp"
#include "MixedExpression.hpp"
#include <array>
#include <atomic>
#include <limits>
#include <optional>
#include <span>
#include <vector>

enum class ExactConstraintStatus { Feasible, ProvenInfeasible, Unsupported, ResourceExhausted, Unknown };
enum class ExactRounding { Floor, Ceiling, Truncate, NearestHalfAwayFromZero };
struct RationalBound { BoundedRational value; bool closed=false; };
struct RationalInterval {
    std::optional<RationalBound> lower, upper;
    bool empty=false;
    bool contains(const BoundedRational&x)const {
        if(empty)return false;
        if(lower&&(x<lower->value||(!lower->closed&&x==lower->value)))return false;
        if(upper&&(upper->value<x||(!upper->closed&&x==upper->value)))return false;
        return true;
    }
    static RationalInterval intersect(RationalInterval a,const RationalInterval&b) {
        if(a.empty||b.empty){a.empty=true;return a;}
        auto tighterLower=[](const std::optional<RationalBound>&x,const std::optional<RationalBound>&y)->std::optional<RationalBound>{if(!x)return y;if(!y)return x;if(x->value<y->value)return y;if(y->value<x->value)return x;return RationalBound{x->value,x->closed&&y->closed};};
        auto tighterUpper=[](const std::optional<RationalBound>&x,const std::optional<RationalBound>&y)->std::optional<RationalBound>{if(!x)return y;if(!y)return x;if(x->value<y->value)return x;if(y->value<x->value)return y;return RationalBound{x->value,x->closed&&y->closed};};
        a.lower=tighterLower(a.lower,b.lower);a.upper=tighterUpper(a.upper,b.upper);
        if(a.lower&&a.upper&&(a.upper->value<a.lower->value||(a.upper->value==a.lower->value&&(!a.lower->closed||!a.upper->closed))))a.empty=true;
        return a;
    }
};
struct ExactConstraintLimits { std::size_t maxObservations=10000,maxBranches=50000,maxWorkspaceBytes=4*1024*1024; std::uint64_t maxWork=1000000; };
struct ExactConstraintResult { ExactConstraintStatus status=ExactConstraintStatus::Unknown; RationalInterval feasible; std::vector<RationalInterval> regions; std::uint64_t work=0; std::string reason; };
struct AffineQuantizedObservation { BoundedRational coefficient,offset,target; ExactRounding rounding=ExactRounding::Floor; };
inline std::uint64_t rationalConstraintWork(std::span<const BoundedRational> values,std::uint64_t operations=16) noexcept {
    std::size_t bits=1;
    for(const auto&v:values)bits=std::max({bits,BoundedBigInteger::bitLength(v.numerator()),BoundedBigInteger::bitLength(v.denominator())});
    const auto limbs=static_cast<std::uint64_t>((bits+63)/64);
    if(limbs&&limbs>std::numeric_limits<std::uint64_t>::max()/limbs/4/std::max<std::uint64_t>(1,bits))return UINT64_MAX;
    const auto per=static_cast<std::uint64_t>(bits)*limbs*limbs*4;
    if(operations&&per>UINT64_MAX/operations)return UINT64_MAX;
    return std::max<std::uint64_t>(1,per*operations);
}
struct RationalWitness { ExactConstraintStatus status=ExactConstraintStatus::Unknown; std::optional<BoundedRational> value; std::string reason; };
inline RationalWitness selectRationalWitness(const RationalInterval&i) {
    if(i.empty)return {ExactConstraintStatus::ProvenInfeasible,{},"empty interval"};
    if(!i.lower||!i.upper)return {ExactConstraintStatus::Unsupported,{},"unbounded interval has no canonical midpoint"};
    if(i.lower->value==i.upper->value){if(i.lower->closed&&i.upper->closed)return {ExactConstraintStatus::Feasible,i.lower->value,{}};return {ExactConstraintStatus::ProvenInfeasible,{},"open singleton"};}
    auto mid=(i.lower->value+i.upper->value)/BoundedRational(2);
    if(!i.contains(mid))return {ExactConstraintStatus::Unknown,{},"midpoint failed exact interval membership"};
    return {ExactConstraintStatus::Feasible,std::move(mid),{}};
}

// The output cell for round(z)==y. Endpoint ownership follows the declared
// executable convention: truncation is toward zero; nearest ties go away.
inline RationalInterval quantizedCell(const BoundedRational& y,ExactRounding mode) {
    if(y.denominator()!=1)throw std::invalid_argument("quantized integer target must be integral");
    using R=BoundedRational; const R one(1),half(1,2); RationalInterval i;
    if(mode==ExactRounding::Floor){i.lower=RationalBound{y,true};i.upper=RationalBound{y+one,false};}
    else if(mode==ExactRounding::Ceiling){i.lower=RationalBound{y-one,false};i.upper=RationalBound{y,true};}
    else if(mode==ExactRounding::Truncate){if(y.zero()){i.lower=RationalBound{-one,false};i.upper=RationalBound{one,false};}else if(y< R(0)){i.lower=RationalBound{y-one,false};i.upper=RationalBound{y,true};}else{i.lower=RationalBound{y,true};i.upper=RationalBound{y+one,false};}}
    else {if(y<R(0)){i.lower=RationalBound{y-half,false};i.upper=RationalBound{y+half,true};}else if(y.zero()){i.lower=RationalBound{-half,false};i.upper=RationalBound{half,false};}else{i.lower=RationalBound{y-half,true};i.upper=RationalBound{y+half,false};}}
    return i;
}

// Solve round(c*alpha+d)==y exactly over a caller-supplied interval.
inline ExactConstraintResult constrainAffine(const BoundedRational&c,const BoundedRational&d,
        const BoundedRational&y,ExactRounding rounding,RationalInterval domain={},
        ExactConstraintLimits limits={},const std::atomic<bool>*cancelled=nullptr) {
    ExactConstraintResult r;
    try {
    if(cancelled&&cancelled->load(std::memory_order_relaxed)){r.status=ExactConstraintStatus::Unknown;r.reason="cancelled";return r;}
    const std::array<BoundedRational,5> values{c,d,y,domain.lower?domain.lower->value:BoundedRational(0),domain.upper?domain.upper->value:BoundedRational(0)};r.work=rationalConstraintWork(values);
    if(r.work>limits.maxWork){r.status=ExactConstraintStatus::ResourceExhausted;r.reason="rational arithmetic work limit";return r;}
    auto cell=quantizedCell(y,rounding); RationalInterval solution;
    if(c.zero()) { // Constant expression: compare its exact rounded output.
        bool match=false;
        if(rounding==ExactRounding::Floor)match=d.floor()==y.numerator()/y.denominator()&&y.denominator()==1;
        else if(rounding==ExactRounding::Ceiling)match=d.ceil()==y.numerator()/y.denominator()&&y.denominator()==1;
        else if(rounding==ExactRounding::Truncate){auto q=d.numerator()/d.denominator();match=y.denominator()==1&&q==y.numerator();}
        else {auto q=d<BoundedRational(0)?-((-d+BoundedRational(1,2)).floor()): (d+BoundedRational(1,2)).floor();match=y.denominator()==1&&q==y.numerator();}
        solution=match?domain:RationalInterval{std::nullopt,std::nullopt,true};
    } else {
        // A <= c*alpha+d <= B. Reverse endpoints for c<0 and preserve their openness.
        auto lo=(*cell.lower);auto hi=(*cell.upper);
        RationalInterval pulled;
        if(c<BoundedRational(0)){std::swap(lo,hi);}
        auto l=(lo.value-d)/c, u=(hi.value-d)/c;
        bool lc=lo.closed,uc=hi.closed;
        pulled.lower=RationalBound{l,lc};pulled.upper=RationalBound{u,uc};solution=RationalInterval::intersect(pulled,domain);
    }
    r.feasible=solution;r.status=solution.empty?ExactConstraintStatus::ProvenInfeasible:ExactConstraintStatus::Feasible;
    if(!solution.empty)r.regions.push_back(solution);
    return r;
    } catch(const std::length_error&e) { r.status=ExactConstraintStatus::ResourceExhausted;r.reason=e.what();return r; }
      catch(const std::bad_alloc&) { r.status=ExactConstraintStatus::ResourceExhausted;r.reason="allocation failure";return r; }
}

inline ExactConstraintResult constrainAffineObservations(std::span<const AffineQuantizedObservation> observations,
        RationalInterval domain={},ExactConstraintLimits limits={},const std::atomic<bool>*cancelled=nullptr) {
    ExactConstraintResult out;out.status=ExactConstraintStatus::Feasible;out.feasible=domain;out.regions.push_back(domain);
    if(domain.empty){out.status=ExactConstraintStatus::ProvenInfeasible;out.regions.clear();out.reason="empty parameter domain";return out;}
    if(observations.size()>limits.maxObservations||observations.size()>limits.maxWorkspaceBytes/sizeof(AffineQuantizedObservation)){out.status=ExactConstraintStatus::ResourceExhausted;out.reason="observation/storage limit";return out;}
    for(const auto&o:observations){if(cancelled&&cancelled->load(std::memory_order_relaxed)){out.status=ExactConstraintStatus::Unknown;out.reason="cancelled";return out;}
        auto one=constrainAffine(o.coefficient,o.offset,o.target,o.rounding,out.feasible,limits,cancelled);
        if(one.work>limits.maxWork-out.work){out.status=ExactConstraintStatus::ResourceExhausted;out.reason="work limit";return out;}out.work+=one.work;
        if(one.status!=ExactConstraintStatus::Feasible){out.status=one.status;out.reason=one.reason;out.feasible=one.feasible;out.regions.clear();return out;}
        out.feasible=one.feasible;out.regions.assign(1,out.feasible);
    }
    return out;
}

// Shared parameter in floor(c1*a+d1)-floor(c2*a+d2)==target. The parameter
// domain must be bounded; each possible second-floor value is a branch. If the
// finite branch count exceeds the declared cap the result is not called infeasible.
inline ExactConstraintResult constrainFloorDifference(const BoundedRational&c1,const BoundedRational&d1,
        const BoundedRational&c2,const BoundedRational&d2,std::int64_t target,
        const RationalInterval&domain,ExactConstraintLimits limits={},const std::atomic<bool>*cancelled=nullptr) {
    ExactConstraintResult r;
    try {
    if(domain.empty){r.status=ExactConstraintStatus::ProvenInfeasible;r.reason="empty parameter domain";return r;}
    if(!domain.lower||!domain.upper){r.status=ExactConstraintStatus::Unsupported;r.reason="shared-floor difference requires bounded parameter domain";return r;}
    auto endpointFloor=[&](const BoundedRational&c,const BoundedRational&d,const BoundedRational&a){auto z=c*a+d;return z.floor();};
    auto k0=endpointFloor(c2,d2,domain.lower->value),k1=endpointFloor(c2,d2,domain.upper->value);
    auto low=std::min(k0,k1),high=std::max(k0,k1);
    const auto branchLimit=std::min(limits.maxBranches,limits.maxWorkspaceBytes/sizeof(RationalInterval));
    if(high-low>=branchLimit){r.status=ExactConstraintStatus::ResourceExhausted;r.reason="shared-floor branch/storage limit";return r;}
    bool any=false;
    for(auto k=low;k<=high;++k){if(cancelled&&cancelled->load(std::memory_order_relaxed)){r.status=ExactConstraintStatus::Unknown;r.reason="cancelled";return r;}
        auto second=constrainAffine(c2,d2,BoundedRational(k),ExactRounding::Floor,domain,limits,cancelled);
        auto first=constrainAffine(c1,d1,BoundedRational(k+target),ExactRounding::Floor,domain,limits,cancelled);
        if(second.work>limits.maxWork-r.work||first.work>limits.maxWork-r.work-second.work){r.work=limits.maxWork;r.status=ExactConstraintStatus::ResourceExhausted;r.reason="shared-floor arithmetic work limit";return r;}r.work+=second.work+first.work;
        if(second.status==ExactConstraintStatus::ResourceExhausted||second.status==ExactConstraintStatus::Unknown||second.status==ExactConstraintStatus::Unsupported){r.status=second.status;r.reason=second.reason;return r;}
        if(first.status==ExactConstraintStatus::ResourceExhausted||first.status==ExactConstraintStatus::Unknown||first.status==ExactConstraintStatus::Unsupported){r.status=first.status;r.reason=first.reason;return r;}
        if(second.status==ExactConstraintStatus::ProvenInfeasible||first.status==ExactConstraintStatus::ProvenInfeasible)continue;
        auto branch=RationalInterval::intersect(second.feasible,first.feasible);
        if(!branch.empty){r.regions.push_back(branch);any=true;}
    }
    // `regions` is authoritative; `feasible` is populated only for a single region.
    if(!any)r.status=ExactConstraintStatus::ProvenInfeasible;
    else {r.status=ExactConstraintStatus::Feasible;if(r.regions.size()==1)r.feasible=r.regions.front();}
    return r;
    } catch(const std::length_error&e) { r.status=ExactConstraintStatus::ResourceExhausted;r.reason=e.what();return r; }
      catch(const std::bad_alloc&) { r.status=ExactConstraintStatus::ResourceExhausted;r.reason="allocation failure";return r; }
}

struct FloorDifferenceObservation { BoundedRational firstCoefficient,firstOffset,secondCoefficient,secondOffset;std::int64_t target=0; };
inline ExactConstraintResult constrainFloorDifferenceObservations(std::span<const FloorDifferenceObservation> observations,
        const RationalInterval&domain,ExactConstraintLimits limits={},const std::atomic<bool>*cancelled=nullptr) {
    ExactConstraintResult out;out.status=ExactConstraintStatus::Feasible;out.regions={domain};
    if(domain.empty){out.status=ExactConstraintStatus::ProvenInfeasible;out.regions.clear();out.reason="empty parameter domain";return out;}
    if(observations.size()>limits.maxObservations||observations.size()>limits.maxWorkspaceBytes/sizeof(FloorDifferenceObservation)){out.status=ExactConstraintStatus::ResourceExhausted;out.reason="observation/storage limit";return out;}
    const auto branchLimit=std::min(limits.maxBranches,limits.maxWorkspaceBytes/sizeof(RationalInterval));
    for(const auto&o:observations){if(cancelled&&cancelled->load(std::memory_order_relaxed)){out.status=ExactConstraintStatus::Unknown;out.reason="cancelled";out.regions.clear();return out;}
        std::vector<RationalInterval> next;
        for(const auto&region:out.regions){auto one=constrainFloorDifference(o.firstCoefficient,o.firstOffset,o.secondCoefficient,o.secondOffset,o.target,region,limits,cancelled);if(one.work>limits.maxWork-out.work){out.status=ExactConstraintStatus::ResourceExhausted;out.reason="combined arithmetic work limit";out.regions.clear();return out;}out.work+=one.work;if(one.status==ExactConstraintStatus::ResourceExhausted||one.status==ExactConstraintStatus::Unsupported||one.status==ExactConstraintStatus::Unknown){out.status=one.status;out.reason=one.reason;out.regions.clear();return out;}for(const auto&candidate:one.regions){if(next.size()>=branchLimit){out.status=ExactConstraintStatus::ResourceExhausted;out.reason="combined interval branch/storage limit";out.regions.clear();return out;}next.push_back(candidate);}}
        out.regions=std::move(next);if(out.regions.empty()){out.status=ExactConstraintStatus::ProvenInfeasible;return out;}
    }
    if(out.regions.size()==1)out.feasible=out.regions.front();
    return out;
}

struct ExecutableWitness { bool binary64Representable=false,verified=false; double value=0; std::size_t checkedObservations=0; std::string reason; };
struct ExactIntegerObservation { std::int64_t input=0,target=0; };
template<class Evaluator>
inline ExecutableWitness verifyBinary64Witness(const BoundedRational&x,
        std::span<const ExactIntegerObservation> observations,
        Evaluator&&evaluate,std::size_t maxObservations=100000,const RationalInterval*allowed=nullptr) {
    ExecutableWitness r;if(observations.size()>maxObservations){r.reason="observation limit";return r;}
    const double d=x.numerator().convert_to<double>()/x.denominator().convert_to<double>();
    if(!std::isfinite(d)){r.reason="nonfinite binary64 witness";return r;}
    r.value=d;r.binary64Representable=BoundedRational::fromBinary64(d)==x;
    if(allowed&&!allowed->contains(BoundedRational::fromBinary64(d))){r.reason="nearest binary64 value is outside the exact feasible interval";return r;}
    for(const auto&row:observations){const auto prediction=evaluate(row.input,d);++r.checkedObservations;if(!prediction||*prediction!=row.target){r.reason="executable output mismatch";return r;}}
    r.verified=true;return r;
}

enum class Binary64WitnessStatus { Verified, NoRepresentableDouble, ExecutableMismatch, Unknown, ResourceExhausted };
struct Binary64WitnessResult { Binary64WitnessStatus status=Binary64WitnessStatus::Unknown; double value=0;std::size_t steps=0;std::string reason; };
template<class Evaluator>
inline Binary64WitnessResult selectAndVerifyBinary64(const RationalInterval&interval,
        std::span<const ExactIntegerObservation> observations,Evaluator&&evaluate,
        std::size_t maxObservations=100000,std::size_t maxAdjacentValues=4096) {
    if(interval.empty)return {Binary64WitnessStatus::NoRepresentableDouble,0,0,"empty exact interval"};
    if(!interval.lower||!interval.upper)return {Binary64WitnessStatus::Unknown,0,0,"bounded interval required"};
    if(observations.size()>maxObservations)return {Binary64WitnessStatus::ResourceExhausted,0,0,"observation limit"};
    auto exact=selectRationalWitness(interval);if(!exact.value)return {Binary64WitnessStatus::Unknown,0,0,exact.reason};
    double candidate=exact.value->numerator().convert_to<double>()/exact.value->denominator().convert_to<double>();
    if(!std::isfinite(candidate))return {Binary64WitnessStatus::Unknown,0,0,"rational-to-binary64 seed outside supported conversion range"};
    const auto seed=BoundedRational::fromBinary64(candidate);
    bool upward=seed<interval.lower->value||(seed==interval.lower->value&&!interval.lower->closed);
    for(std::size_t step=0;step<=maxAdjacentValues;++step){
        const auto dyadic=BoundedRational::fromBinary64(candidate);
        if(interval.contains(dyadic)){
            for(const auto&row:observations){auto output=evaluate(row.input,candidate);if(!output||*output!=row.target)return {Binary64WitnessStatus::ExecutableMismatch,candidate,step,"binary64 expression failed exact observation verification"};}
            return {Binary64WitnessStatus::Verified,candidate,step,{}};
        }
        if(step==maxAdjacentValues)break;
        const double next=std::nextafter(candidate,upward?std::numeric_limits<double>::infinity():-std::numeric_limits<double>::infinity());
        if(!std::isfinite(next))return {Binary64WitnessStatus::NoRepresentableDouble,0,step,"adjacent binary64 values passed finite interval"};
        const auto nd=BoundedRational::fromBinary64(next);
        if((upward&&interval.upper->value<nd)||(!upward&&nd<interval.lower->value))
            return {Binary64WitnessStatus::NoRepresentableDouble,0,step+1,"adjacent binary64 values straddle exact interval"};
        candidate=next;
    }
    return {Binary64WitnessStatus::Unknown,candidate,maxAdjacentValues,"binary64 search limit; absence not proved"};
}
