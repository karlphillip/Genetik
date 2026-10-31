#include <QuantizedConstraints.hpp>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <bit>

namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
using R=BoundedRational;
std::int64_t integer(const BoundedBigInteger::value_type&x){auto v=BoundedBigInteger::toInt64(x);if(!v)throw std::runtime_error("expected Int64");return *v;}
void rationalAndCells(){
    check(R(16,10)==R(8,5),"canonical normalization");
    check(R(1,3)+R(1,6)==R(1,2)&&R(3,7)*R(14,9)==R(2,3)&&R(5,6)-R(1,3)==R(1,2)&&R(2,3)/R(4,9)==R(3,2),"exact rational arithmetic");
    check(R::fromBinary64(0.5)==R(1,2),"binary64 exact dyadic import");
    check(R::fromBinary64(-0.0)==R(0),"negative zero imports as exact zero");
    bool rejected=false;try{(void)R::fromBinary64(std::numeric_limits<double>::infinity());}catch(const std::invalid_argument&){rejected=true;}check(rejected,"nonfinite binary64 rejected");
    rejected=false;try{(void)R(1,0);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"zero rational denominator rejected");
    auto large=R(BoundedRational::integer_type(1)<<200,BoundedRational::integer_type(3));
    check(BoundedBigInteger::bitLength(large.numerator())==201,"arbitrary precision numerator");
    check(R::parse(large.toString())==large,"bounded canonical serialization roundtrip");
    check(integer(R(-17,10).floor())==-2&&integer(R(-17,10).ceil())==-1,"exact negative floor and ceiling");
    auto f1=constrainAffine(R(10),R(0),R(16),ExactRounding::Floor);
    auto f2=constrainAffine(R(20),R(0),R(32),ExactRounding::Floor);
    auto both=RationalInterval::intersect(f1.feasible,f2.feasible);
    check(f1.status==ExactConstraintStatus::Feasible&&both.lower&&both.upper&&
          both.lower->value==R(8,5)&&both.lower->closed&&both.upper->value==R(33,20)&&!both.upper->closed,
          "floor constraints intersect to [1.6,1.65)");
    const std::array<AffineQuantizedObservation,3> affineRows{{{R(1),R(3,2),R(1),ExactRounding::Floor},{R(1),R(3),R(3),ExactRounding::Floor},{R(1),R(9,2),R(4),ExactRounding::Floor}}};
    auto beta=constrainAffineObservations(affineRows);
    check(beta.status==ExactConstraintStatus::Feasible&&beta.feasible.lower&&beta.feasible.upper&&
          beta.feasible.lower->value==R(0)&&beta.feasible.upper->value==R(1,2)&&!beta.feasible.upper->closed&&beta.feasible.contains(R(1,4)),
          "fixed-alpha affine quantization solves exact beta interval");
    auto neg=constrainAffine(R(-2),R(0),R(-3),ExactRounding::Floor);
    check(neg.feasible.lower&&neg.feasible.upper&&neg.feasible.lower->value==R(1)&&!neg.feasible.lower->closed&&neg.feasible.upper->value==R(3,2)&&neg.feasible.upper->closed,"negative affine coefficient reverses exact bounds");
    check(constrainAffine(R(0),R(2),R(2),ExactRounding::Floor).status==ExactConstraintStatus::Feasible,"zero coefficient constant feasible");
    check(constrainAffine(R(0),R(2),R(1),ExactRounding::Floor).status==ExactConstraintStatus::ProvenInfeasible,"zero coefficient contradiction");
    auto ceil=quantizedCell(R(-2),ExactRounding::Ceiling);check(ceil.lower&&!ceil.lower->closed&&ceil.upper&&ceil.upper->closed&&ceil.lower->value==R(-3)&&ceil.upper->value==R(-2),"negative ceiling cell");
    auto trunc=quantizedCell(R(-2),ExactRounding::Truncate);check(trunc.lower&&!trunc.lower->closed&&trunc.upper&&trunc.upper->closed,"negative truncation cell");
    auto nearPos=quantizedCell(R(2),ExactRounding::NearestHalfAwayFromZero),nearNeg=quantizedCell(R(-2),ExactRounding::NearestHalfAwayFromZero);
    check(nearPos.lower->closed&&!nearPos.upper->closed&&!nearNeg.lower->closed&&nearNeg.upper->closed,"nearest half ties away from zero");
    check(!quantizedCell(R(0),ExactRounding::NearestHalfAwayFromZero).lower->closed,"zero excludes both half ties");
    ExactConstraintLimits low;low.maxBranches=1;
    auto domain=RationalInterval{RationalBound{R(0),true},RationalBound{R(10),true},false};
    auto exhausted=constrainFloorDifference(R(2),R(0),R(1),R(0),0,domain,low);
    check(exhausted.status==ExactConstraintStatus::ResourceExhausted,"bounded shared-floor branching resource outcome");
}
void executableAndShared(){
    constexpr double gamma=(std::sqrt(5.0)-1.0)/2.0;
    auto exact=R::fromBinary64(gamma);check(exact.toString().size()>10,"binary64 parameter retained as exact dyadic rational");
    auto shared=constrainFloorDifference(R(7),R(2),R(6),R(1),1,
       {RationalBound{R(0),true},RationalBound{R(1),false},false});
    check(shared.status==ExactConstraintStatus::Feasible&&!shared.regions.empty(),"shared parameter floor difference produces feasible union branches");
    const auto gammaRegion=constrainFloorDifference(R(3),R(0),R(2),R(0),0,
       {RationalBound{R(0),true},RationalBound{R(1),true},false});
    check(std::any_of(gammaRegion.regions.begin(),gammaRegion.regions.end(),[&](const auto&i){return i.contains(exact);}),"single Wythoff gap observation admits binary64 gamma as an exact rational point in its mathematical cell");
    auto isqrt=[](std::int64_t x){std::int64_t lo=0,hi=3037000501LL;while(lo+1<hi){auto m=lo+(hi-lo)/2;if(m<=x/m)lo=m;else hi=m;}return lo;};
    auto a=[&](std::int64_t n){return (n+isqrt(5*n*n))/2;};
    std::array<MixedNode,11> wNodes{MixedNode::input(0),MixedNode::operation(MixedKind::IntToReal),MixedNode::parameter(0),MixedNode::operation(MixedKind::RDivide),MixedNode::operation(MixedKind::CeilToInt),MixedNode::integer(1),MixedNode::operation(MixedKind::IAdd),MixedNode::operation(MixedKind::IntToReal),MixedNode::parameter(0),MixedNode::operation(MixedKind::RMultiply),MixedNode::operation(MixedKind::FloorToInt)};
    MixedExpression wythoff(wNodes,1,{},MixedType::Int64,1);const double phi=(1.0+std::sqrt(5.0))/2.0;
    for(std::int64_t n=1;n<=1200;++n){const auto x=a(n);std::array<std::int64_t,1>input{x};auto result=wythoff.evaluate(input,100,std::array<double,1>{phi});check(result.valid()&&result.value->integer()==a(n+1),"fixed-topology Wythoff formula independent integer sequence agreement");}
    std::vector<MixedNode> n{MixedNode::input(0),MixedNode::integer(1),MixedNode::operation(MixedKind::IAdd),MixedNode::operation(MixedKind::IntToReal),MixedNode::parameter(0),MixedNode::operation(MixedKind::RMultiply),MixedNode::operation(MixedKind::FloorToInt)};
    MixedExpression e(n,1,{},MixedType::Int64,1);
    auto evaluator=[&](std::int64_t x,double p)->std::optional<std::int64_t>{std::array<std::int64_t,1>a{x};auto r=e.evaluate(a,100,std::array<double,1>{p});if(!r.valid())return {};return r.value->integer();};
    std::array<ExactIntegerObservation,2> rows{{{1,1},{2,1}}};
    auto witness=verifyBinary64Witness(R(1,2),rows,evaluator);check(witness.binary64Representable&&witness.verified&&witness.checkedObservations==2,"exact rational witness separately verified in executable mixed evaluator");
    RationalInterval executableInterval{RationalBound{R(1,2),true},RationalBound{R(3,5),false},false};
    auto selected=selectAndVerifyBinary64(executableInterval,rows,evaluator);check(selected.status==Binary64WitnessStatus::Verified&&executableInterval.contains(R::fromBinary64(selected.value)),"deterministic binary64 coefficient selected inside exact interval and executed against all observations");
    const auto one=R::fromBinary64(1.0),next=R::fromBinary64(std::nextafter(1.0,2.0));
    RationalInterval noDouble{RationalBound{one,false},RationalBound{next,false},false};
    auto absent=selectAndVerifyBinary64(noDouble,std::span<const ExactIntegerObservation>{},evaluator);
    check(absent.status==Binary64WitnessStatus::NoRepresentableDouble,"open interval between adjacent binary64 values proves no executable coefficient exists");
    std::array<ExactIntegerObservation,1> wrong{{{2,2}}};check(!verifyBinary64Witness(R(1,2),wrong,evaluator).verified,"executable witness mismatches are rejected");
}
}
int main(){try{rationalAndCells();executableAndShared();std::cout<<"S2 exact rational and quantized constraint contracts passed\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
