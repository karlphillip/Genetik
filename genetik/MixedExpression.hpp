#pragma once

// Scalar mixed integer/real/boolean expressions for direct integer formulas.
// Real operations dispatch to the same checked kernels as Expression.hpp;
// integer arithmetic dispatches to DiscreteExpression's checked kernels.
#include "DiscreteExpression.hpp"
#include "Expression.hpp"
#include <bit>
#include <charconv>
#include <functional>
#include <sstream>
#include <variant>

enum class MixedType : std::uint8_t { Int64, Real64, Bool };
enum class MixedKind : std::uint8_t {
    Integer=0, Real=1, Boolean=2, Input=3,
    IAdd=4, ISubtract=5, IMultiply=6, IModulo=7,
    RAdd=8, RSubtract=9, RMultiply=10, RDivide=11, RPow=12, RLog=13, RExp=14, RSin=15, RCos=16, RTan=17, RAsin=18, RAcos=19, RAtan=20, RSinh=21, RCosh=22, RTanh=23, RSqrt=24,
    IntToReal=25, FloorToInt=26, CeilToInt=27, TruncateToInt=28, NearestToInt=29,
    EqualInt=30, LessInt=31, LessEqualInt=32, EqualReal=33, LessReal=34, LessEqualReal=35,
    BoolNot=36, BoolAnd=37, BoolOr=38, IfThenElse=39, IntegerSqrt=40, SharedRealParameter=41
};
inline std::string_view mixedKindName(MixedKind k){switch(k){
case MixedKind::IAdd:return "iadd";case MixedKind::ISubtract:return "isub";case MixedKind::IMultiply:return "imul";case MixedKind::IModulo:return "imod_euclidean";
case MixedKind::RAdd:return "radd";case MixedKind::RSubtract:return "rsub";case MixedKind::RMultiply:return "rmul";case MixedKind::RDivide:return "rdiv";case MixedKind::RPow:return "rpow";case MixedKind::RLog:return "rlog";case MixedKind::RExp:return "rexp";case MixedKind::RSin:return "rsin";case MixedKind::RCos:return "rcos";case MixedKind::RTan:return "rtan";case MixedKind::RAsin:return "rasin";case MixedKind::RAcos:return "racos";case MixedKind::RAtan:return "ratan";case MixedKind::RSinh:return "rsinh";case MixedKind::RCosh:return "rcosh";case MixedKind::RTanh:return "rtanh";case MixedKind::RSqrt:return "rsqrt";
case MixedKind::IntToReal:return "int64_to_real64_rounded";case MixedKind::FloorToInt:return "floor_to_int64";case MixedKind::CeilToInt:return "ceil_to_int64";case MixedKind::TruncateToInt:return "truncate_to_int64";case MixedKind::NearestToInt:return "nearest_half_away_to_int64";case MixedKind::IntegerSqrt:return "isqrt_int64";
case MixedKind::EqualInt:return "equal_int64";case MixedKind::LessInt:return "less_int64";case MixedKind::LessEqualInt:return "less_equal_int64";case MixedKind::EqualReal:return "equal_real64";case MixedKind::LessReal:return "less_real64";case MixedKind::LessEqualReal:return "less_equal_real64";case MixedKind::BoolNot:return "bool_not";case MixedKind::BoolAnd:return "bool_and";case MixedKind::BoolOr:return "bool_or";case MixedKind::IfThenElse:return "if_then_else";default:throw std::invalid_argument("terminal has no operator name");}}

class MixedValue {
public:
    static MixedValue integer(std::int64_t x) { return MixedValue(x); }
    static MixedValue real(double x) { if (!std::isfinite(x)) throw std::invalid_argument("mixed real value must be finite"); return MixedValue(x); }
    static MixedValue boolean(bool x) { return MixedValue(x); }
    MixedType type() const noexcept { return static_cast<MixedType>(value_.index()); }
    std::int64_t integer() const { return std::get<std::int64_t>(value_); }
    double real() const { return std::get<double>(value_); }
    bool boolean() const { return std::get<bool>(value_); }
    bool operator==(const MixedValue&) const = default;
private:
    explicit MixedValue(std::int64_t x):value_(x){}
    explicit MixedValue(double x):value_(x){}
    explicit MixedValue(bool x):value_(x){}
    std::variant<std::int64_t,double,bool> value_;
};

class MixedNode {
public:
    static MixedNode integer(std::int64_t x) { return {MixedKind::Integer,x,0}; }
    static MixedNode real(double x) { if(!std::isfinite(x)) throw std::invalid_argument("mixed real constant must be finite"); return {MixedKind::Real,0,x}; }
    static MixedNode parameter(std::size_t id) { if(id>static_cast<std::size_t>(INT64_MAX))throw std::invalid_argument("mixed shared parameter id outside Int64");return {MixedKind::SharedRealParameter,static_cast<std::int64_t>(id),0}; }
    static MixedNode boolean(bool x) { return {MixedKind::Boolean,x?1:0,0}; }
    static MixedNode input(std::size_t i) { if(i>static_cast<std::size_t>(INT64_MAX))throw std::invalid_argument("mixed input index outside Int64");return {MixedKind::Input,static_cast<std::int64_t>(i),0}; }
    static MixedNode operation(MixedKind k) { if(arity(k)==0) throw std::invalid_argument("mixed operator required"); return {k,0,0}; }
    MixedKind kind() const noexcept { return kind_; }
    std::int64_t integer() const { if(kind_!=MixedKind::Integer&&kind_!=MixedKind::Boolean)throw std::logic_error("not integer literal");return integer_; }
    double real() const { if(kind_!=MixedKind::Real)throw std::logic_error("not real literal");return real_; }
    std::size_t inputIndex() const { if(kind_!=MixedKind::Input)throw std::logic_error("not an input");return static_cast<std::size_t>(integer_); }
    std::size_t parameterIndex() const { if(kind_!=MixedKind::SharedRealParameter)throw std::logic_error("not a shared real parameter");return static_cast<std::size_t>(integer_); }
    static std::size_t arity(MixedKind k) {
        switch(k) {
        case MixedKind::Integer:case MixedKind::Real:case MixedKind::Boolean:case MixedKind::Input:case MixedKind::SharedRealParameter:return 0;
        case MixedKind::IntToReal:case MixedKind::FloorToInt:case MixedKind::CeilToInt:case MixedKind::TruncateToInt:case MixedKind::NearestToInt:case MixedKind::IntegerSqrt:
        case MixedKind::RLog:case MixedKind::RExp:case MixedKind::RSin:case MixedKind::RCos:case MixedKind::RTan:case MixedKind::RAsin:case MixedKind::RAcos:case MixedKind::RAtan:case MixedKind::RSinh:case MixedKind::RCosh:case MixedKind::RTanh:case MixedKind::RSqrt:case MixedKind::BoolNot:return 1;
        case MixedKind::IAdd:case MixedKind::ISubtract:case MixedKind::IMultiply:case MixedKind::IModulo:
        case MixedKind::RAdd:case MixedKind::RSubtract:case MixedKind::RMultiply:case MixedKind::RDivide:case MixedKind::RPow:
        case MixedKind::EqualInt:case MixedKind::LessInt:case MixedKind::LessEqualInt:case MixedKind::EqualReal:case MixedKind::LessReal:case MixedKind::LessEqualReal:
        case MixedKind::BoolAnd:case MixedKind::BoolOr:return 2;
        case MixedKind::IfThenElse:return 3;
        }
        throw std::invalid_argument("unknown mixed operator");
    }
private:
    MixedNode(MixedKind k,std::int64_t i,double r):kind_(k),integer_(i),real_(r){}
    MixedKind kind_; std::int64_t integer_; double real_;
};

enum class MixedStatus { Success, MathematicalInvalidity, ResourceLimit };
struct MixedResult { MixedStatus status=MixedStatus::MathematicalInvalidity; std::optional<MixedValue> value; std::size_t nodeVisits=0,operations=0; bool valid()const noexcept{return status==MixedStatus::Success&&value.has_value();} };

struct MixedLimits { std::size_t maxNodes=1023,maxDepth=64,maxOperations=1000000; double maxAbsReal=1e100; };
struct MixedComplexity {std::size_t nodes=0,depth=0,operations=0,constants=0,constantBits=0,descriptionBytes=0;};

class MixedExpression {
    struct Meta { MixedType type; std::size_t depth,start; std::array<std::size_t,3> child{}; };
public:
    static constexpr std::size_t MaxSharedParameters=64;
    MixedExpression(std::span<const MixedNode> nodes,std::size_t inputCount,MixedLimits limits={},std::optional<MixedType> requiredRoot=MixedType::Int64,std::size_t parameterCount=0) : inputCount_(inputCount),parameterCount_(parameterCount),limits_(limits) {
        if(nodes.empty()||!limits.maxNodes||!limits.maxDepth||!limits.maxOperations||!std::isfinite(limits.maxAbsReal)||limits.maxAbsReal<0||parameterCount>MaxSharedParameters)throw std::invalid_argument("invalid mixed expression limits, shared parameter count, or empty tree");
        if(nodes.size()>limits.maxNodes)throw std::length_error("mixed expression node limit");
        std::vector<std::size_t> stack; stack.reserve(nodes.size()); nodes_.assign(nodes.begin(),nodes.end()); meta_.reserve(nodes.size());
        for(std::size_t i=0;i<nodes_.size();++i) {
            auto k=nodes_[i].kind(); const auto ar=MixedNode::arity(k); Meta m{};m.start=i;
            if(ar==0) {
                if(k==MixedKind::Integer||k==MixedKind::Input)m.type=MixedType::Int64;
                else if(k==MixedKind::Real||k==MixedKind::SharedRealParameter)m.type=MixedType::Real64;else m.type=MixedType::Bool;
                if(k==MixedKind::Input&&nodes_[i].inputIndex()>=inputCount_)throw std::invalid_argument("mixed input index outside schema");
                if(k==MixedKind::Real&&std::abs(nodes_[i].real())>limits.maxAbsReal)throw std::invalid_argument("mixed real terminal outside limits");
                if(k==MixedKind::SharedRealParameter&&nodes_[i].parameterIndex()>=parameterCount_)throw std::invalid_argument("mixed shared parameter reference outside candidate parameter schema");
            } else {
                if(stack.size()<ar)throw std::invalid_argument("malformed mixed postfix expression");
                for(std::size_t j=ar;j>0;--j){m.child[j-1]=stack.back();stack.pop_back();}
                m.start=meta_[m.child[0]].start;
                auto type=[&](std::size_t j){return meta_[m.child[j]].type;};
                auto req=[&](std::size_t j,MixedType t){if(type(j)!=t)throw std::invalid_argument("mixed expression operator type mismatch");};
                switch(k) {
                case MixedKind::IAdd:case MixedKind::ISubtract:case MixedKind::IMultiply:case MixedKind::IModulo:req(0,MixedType::Int64);req(1,MixedType::Int64);m.type=MixedType::Int64;break;
                case MixedKind::IntegerSqrt:req(0,MixedType::Int64);m.type=MixedType::Int64;break;
                case MixedKind::RAdd:case MixedKind::RSubtract:case MixedKind::RMultiply:case MixedKind::RDivide:case MixedKind::RPow:req(0,MixedType::Real64);req(1,MixedType::Real64);m.type=MixedType::Real64;break;
                case MixedKind::IntToReal:req(0,MixedType::Int64);m.type=MixedType::Real64;break;
                case MixedKind::FloorToInt:case MixedKind::CeilToInt:case MixedKind::TruncateToInt:case MixedKind::NearestToInt:req(0,MixedType::Real64);m.type=MixedType::Int64;break;
                case MixedKind::RLog:case MixedKind::RExp:case MixedKind::RSin:case MixedKind::RCos:case MixedKind::RTan:case MixedKind::RAsin:case MixedKind::RAcos:case MixedKind::RAtan:case MixedKind::RSinh:case MixedKind::RCosh:case MixedKind::RTanh:case MixedKind::RSqrt:req(0,MixedType::Real64);m.type=MixedType::Real64;break;
                case MixedKind::EqualInt:case MixedKind::LessInt:case MixedKind::LessEqualInt:req(0,MixedType::Int64);req(1,MixedType::Int64);m.type=MixedType::Bool;break;
                case MixedKind::EqualReal:case MixedKind::LessReal:case MixedKind::LessEqualReal:req(0,MixedType::Real64);req(1,MixedType::Real64);m.type=MixedType::Bool;break;
                case MixedKind::BoolNot:req(0,MixedType::Bool);m.type=MixedType::Bool;break;
                case MixedKind::BoolAnd:case MixedKind::BoolOr:req(0,MixedType::Bool);req(1,MixedType::Bool);m.type=MixedType::Bool;break;
                case MixedKind::IfThenElse:req(0,MixedType::Bool);if(type(1)!=type(2))throw std::invalid_argument("mixed conditional branch types differ");m.type=type(1);break;
                default:throw std::invalid_argument("invalid mixed operation");
                }
                std::size_t d=0;for(std::size_t j=0;j<ar;++j)d=std::max(d,meta_[m.child[j]].depth);m.depth=d+1;
                if(m.depth>limits.maxDepth)throw std::length_error("mixed expression depth limit");
            }
            meta_.push_back(m);stack.push_back(i);
        }
        if(stack.size()!=1)throw std::invalid_argument("mixed postfix expression has multiple roots");
        root_=stack.back();if(requiredRoot&&meta_[root_].type!=*requiredRoot)throw std::invalid_argument("mixed expression root has unexpected type");
    }
    MixedExpression(const MixedExpression& x):MixedExpression(x.nodes(),x.inputCount(),x.limits(),x.rootType(),x.parameterCount()){}
    MixedExpression(MixedExpression&&) noexcept=default;
    MixedExpression& operator=(MixedExpression x) noexcept{swap(x);return *this;}
    void swap(MixedExpression&x)noexcept{std::swap(inputCount_,x.inputCount_);std::swap(parameterCount_,x.parameterCount_);std::swap(limits_,x.limits_);nodes_.swap(x.nodes_);meta_.swap(x.meta_);std::swap(root_,x.root_);}
    std::vector<MixedNode> nodes()const{return nodes_;} std::size_t inputCount()const noexcept{return inputCount_;} std::size_t parameterCount()const noexcept{return parameterCount_;} std::size_t nodeCount()const noexcept{return nodes_.size();} std::size_t depth()const noexcept{return meta_[root_].depth;} MixedType rootType()const noexcept{return meta_[root_].type;} MixedLimits limits()const noexcept{return limits_;}
    MixedType nodeType(std::size_t i)const{return meta_.at(i).type;}
    static bool int64ExactlyRepresentableAsReal(std::int64_t n)noexcept{return n>=-(std::int64_t{1}<<53)&&n<=(std::int64_t{1}<<53);}
    static double int64ToReal64(std::int64_t value)noexcept{
        if(value==0)return 0.0;
        const bool negative=value<0;
        const std::uint64_t magnitude=negative?std::uint64_t(-(value+1))+1:std::uint64_t(value);
        const unsigned width=std::bit_width(magnitude);
        std::uint64_t significand;
        unsigned exponent=width-1;
        if(width<=53)significand=magnitude<<(53-width);
        else {
            const unsigned shift=width-53;
            significand=magnitude>>shift;
            const std::uint64_t remainder=magnitude&((std::uint64_t{1}<<shift)-1);
            const std::uint64_t halfway=std::uint64_t{1}<<(shift-1);
            if(remainder>halfway||(remainder==halfway&&(significand&1)))++significand;
            if(significand==(std::uint64_t{1}<<53)){significand>>=1;++exponent;}
        }
        const std::uint64_t bits=(negative?(std::uint64_t{1}<<63):0)|
            (std::uint64_t(exponent+1023)<<52)|(significand- (std::uint64_t{1}<<52));
        return std::bit_cast<double>(bits);
    }
    std::size_t constantCount()const noexcept{std::size_t n=0;for(const auto&x:nodes_)if(x.kind()==MixedKind::Integer||x.kind()==MixedKind::Real)++n;return n;}
    std::size_t operationCount()const noexcept{return nodes_.size()-constantCount()-static_cast<std::size_t>(std::count_if(nodes_.begin(),nodes_.end(),[](const auto&x){return x.kind()==MixedKind::Input||x.kind()==MixedKind::Boolean||x.kind()==MixedKind::SharedRealParameter;}));}
    std::size_t constantBitLength()const noexcept{std::size_t n=0;for(const auto&x:nodes_){if(x.kind()==MixedKind::Integer){const auto v=x.integer();const auto magnitude=v<0?std::uint64_t(-(v+1))+1:std::uint64_t(v);n+=std::max<unsigned>(1,std::bit_width(magnitude))+static_cast<unsigned>(v<0);}else if(x.kind()==MixedKind::Real)n+=64;}return n;}
    std::size_t constantDescriptionLength()const{std::size_t total=0;for(const auto&n:nodes_){if(n.kind()==MixedKind::Integer)total+=std::to_string(n.integer()).size();else if(n.kind()==MixedKind::Real){char b[64];const auto r=std::to_chars(b,b+sizeof b,n.real(),std::chars_format::general,std::numeric_limits<double>::max_digits10);total+=static_cast<std::size_t>(r.ptr-b);}}return total;}
    MixedComplexity complexity()const{return {nodeCount(),depth(),operationCount(),constantCount(),constantBitLength(),constantDescriptionLength()};}
    std::pair<std::size_t,std::size_t> subtree(std::size_t i)const{if(i>=nodes_.size())throw std::out_of_range("mixed subtree root outside expression");std::vector<std::size_t>starts;for(std::size_t j=0;j<=i;++j){const auto ar=MixedNode::arity(nodes_[j].kind());if(!ar)starts.push_back(j);else for(std::size_t k=1;k<ar;++k)starts.pop_back();}return {starts.back(),i+1};}
    std::optional<MixedExpression> replaceSubtree(std::size_t root,const MixedExpression& replacement)const{
        if(replacement.inputCount()!=inputCount_||replacement.rootType()!=nodeType(root))return {};
        const auto [first,last]=subtree(root);const auto inserted=replacement.nodes();const auto remaining=nodes_.size()-(last-first);
        if(remaining>limits_.maxNodes||inserted.size()>limits_.maxNodes-remaining)return {};
        std::vector<MixedNode> next;next.reserve(remaining+inserted.size());next.insert(next.end(),nodes_.begin(),nodes_.begin()+first);next.insert(next.end(),inserted.begin(),inserted.end());next.insert(next.end(),nodes_.begin()+last,nodes_.end());
        try{return MixedExpression(next,inputCount_,limits_,rootType(),parameterCount_);}catch(const std::length_error&){return {};}catch(const std::invalid_argument&){return {};}
    }
    MixedResult evaluate(std::span<const std::int64_t> inputs,std::size_t budget=std::numeric_limits<std::size_t>::max(),std::span<const double> parameters={}) const {
        if(inputs.size()!=inputCount_)throw std::invalid_argument("mixed input count mismatch");
        if(parameters.size()!=parameterCount_)throw std::invalid_argument("mixed shared parameter count mismatch");
        MixedResult out;out.status=MixedStatus::Success;const auto maximum=std::min(limits_.maxOperations,budget);
        auto charge=[&](bool operation){if(operation&&out.operations==maximum){out.status=MixedStatus::ResourceLimit;return false;}++out.nodeVisits;if(operation)++out.operations;return true;};
        struct MixedWorkLimit{};
        auto chargeIntegerStep=[&](){if(out.operations==maximum)throw MixedWorkLimit{};++out.operations;};
        auto apply=[&](MixedKind k,const std::array<std::optional<MixedValue>,3>&v)->std::optional<MixedValue>{const auto&a=*v[0];const MixedValue*b=v[1]?&*v[1]:nullptr;auto invalid=[&]()->std::optional<MixedValue>{out.status=MixedStatus::MathematicalInvalidity;return {};};auto realValue=[&](double x)->std::optional<MixedValue>{if(!std::isfinite(x)||std::abs(x)>limits_.maxAbsReal)return invalid();return MixedValue::real(x);};
            switch(k){
            case MixedKind::IAdd:case MixedKind::ISubtract:case MixedKind::IMultiply:case MixedKind::IModulo:{auto dk=k==MixedKind::IAdd?DiscreteKind::Add:k==MixedKind::ISubtract?DiscreteKind::Subtract:k==MixedKind::IMultiply?DiscreteKind::Multiply:DiscreteKind::Modulo;std::int64_t r;if(!genetik_discrete_detail::arithmetic(dk,a.integer(),b->integer(),r))return invalid();return MixedValue::integer(r);}
            case MixedKind::IntegerSqrt:{try{auto value=genetik_discrete_detail::integerMath(genetik_discrete_detail::IntegerOperation::Sqrt,a.integer(),0,0,chargeIntegerStep);if(!value)return invalid();return MixedValue::integer(*value);}catch(const MixedWorkLimit&){out.status=MixedStatus::ResourceLimit;return {};}}
            case MixedKind::IntToReal:return realValue(int64ToReal64(a.integer()));
            case MixedKind::FloorToInt:case MixedKind::CeilToInt:case MixedKind::TruncateToInt:case MixedKind::NearestToInt:{const double x=a.real();constexpr double bound=0x1p63;if(!std::isfinite(x)||!(x>=-bound&&x<bound))return invalid();const double r=k==MixedKind::FloorToInt?std::floor(x):k==MixedKind::CeilToInt?std::ceil(x):k==MixedKind::TruncateToInt?std::trunc(x):std::round(x);if(!(r>=-bound&&r<bound))return invalid();return MixedValue::integer(static_cast<std::int64_t>(r));}
            case MixedKind::RAdd:case MixedKind::RSubtract:case MixedKind::RMultiply:case MixedKind::RDivide:case MixedKind::RPow:{auto ek=k==MixedKind::RAdd?ExpressionKind::Add:k==MixedKind::RSubtract?ExpressionKind::Subtract:k==MixedKind::RMultiply?ExpressionKind::Multiply:k==MixedKind::RDivide?ExpressionKind::Divide:ExpressionKind::Pow;auto x=genetik_detail::scalarBinary(ek,a.real(),b->real(),[]{});if(!x)return invalid();return realValue(*x);}
            case MixedKind::RLog:case MixedKind::RExp:case MixedKind::RSin:case MixedKind::RCos:case MixedKind::RTan:case MixedKind::RAsin:case MixedKind::RAcos:case MixedKind::RAtan:case MixedKind::RSinh:case MixedKind::RCosh:case MixedKind::RTanh:case MixedKind::RSqrt:{auto ek=k==MixedKind::RLog?ExpressionKind::Log:k==MixedKind::RExp?ExpressionKind::Exp:k==MixedKind::RSin?ExpressionKind::Sin:k==MixedKind::RCos?ExpressionKind::Cos:k==MixedKind::RTan?ExpressionKind::Tan:k==MixedKind::RAsin?ExpressionKind::Asin:k==MixedKind::RAcos?ExpressionKind::Acos:k==MixedKind::RAtan?ExpressionKind::Atan:k==MixedKind::RSinh?ExpressionKind::Sinh:k==MixedKind::RCosh?ExpressionKind::Cosh:k==MixedKind::RTanh?ExpressionKind::Tanh:ExpressionKind::Sqrt;auto x=genetik_detail::scalarUnary(ek,a.real(),0,[]{});if(!x)return invalid();return realValue(*x);}
            case MixedKind::EqualInt:return MixedValue::boolean(a.integer()==b->integer());case MixedKind::LessInt:return MixedValue::boolean(a.integer()<b->integer());case MixedKind::LessEqualInt:return MixedValue::boolean(a.integer()<=b->integer());case MixedKind::EqualReal:return MixedValue::boolean(a.real()==b->real());case MixedKind::LessReal:return MixedValue::boolean(a.real()<b->real());case MixedKind::LessEqualReal:return MixedValue::boolean(a.real()<=b->real());case MixedKind::BoolNot:return MixedValue::boolean(!a.boolean());case MixedKind::BoolAnd:return MixedValue::boolean(a.boolean()&&b->boolean());case MixedKind::BoolOr:return MixedValue::boolean(a.boolean()||b->boolean());default:return invalid();}
        };
        struct Frame{std::size_t node=0,next=0;std::array<std::optional<MixedValue>,3>values{};bool charged=false;};std::vector<Frame> frames;frames.reserve(meta_[root_].depth);frames.push_back({root_});std::optional<MixedValue> returned;std::size_t steps=0;const auto stepLimit=nodes_.size()>(std::numeric_limits<std::size_t>::max()-4)/4?std::numeric_limits<std::size_t>::max():nodes_.size()*4+4;
        while(!frames.empty()){
            if(++steps>stepLimit){out.status=MixedStatus::ResourceLimit;break;}
            if(out.status!=MixedStatus::Success)break;
            auto&f=frames.back();const auto k=nodes_[f.node].kind();const auto ar=MixedNode::arity(k);
            if(!f.charged){if(!charge(ar!=0))break;f.charged=true;}
            if(ar==0){const auto&n=nodes_[f.node];if(k==MixedKind::Integer)returned=MixedValue::integer(n.integer());else if(k==MixedKind::Real)returned=MixedValue::real(n.real());else if(k==MixedKind::SharedRealParameter){const auto x=parameters[n.parameterIndex()];if(!std::isfinite(x)||std::abs(x)>limits_.maxAbsReal){out.status=MixedStatus::MathematicalInvalidity;break;}returned=MixedValue::real(x);}else if(k==MixedKind::Boolean)returned=MixedValue::boolean(n.integer()!=0);else returned=MixedValue::integer(inputs[n.inputIndex()]);frames.pop_back();if(frames.empty()){out.value=std::move(returned);break;}}
            else {
                if(returned){auto&parent=frames.back();const auto pk=nodes_[parent.node].kind();
                    if(pk==MixedKind::IfThenElse){if(parent.next==1){parent.values[0]=std::move(returned);returned.reset();const auto branch=parent.values[0]->boolean()?1u:2u;parent.next=3;frames.push_back({meta_[parent.node].child[branch]});continue;}auto value=std::move(returned);frames.pop_back();if(frames.empty()){out.value=std::move(value);break;}returned=std::move(value);continue;}
                    if(pk==MixedKind::BoolAnd||pk==MixedKind::BoolOr){if(parent.next==1){parent.values[0]=std::move(returned);returned.reset();const bool shortCircuit=pk==MixedKind::BoolAnd?!parent.values[0]->boolean():parent.values[0]->boolean();if(shortCircuit){auto value=MixedValue::boolean(pk==MixedKind::BoolOr);frames.pop_back();returned=std::move(value);if(frames.empty()){out.value=std::move(returned);break;}continue;}parent.next=2;frames.push_back({meta_[parent.node].child[1]});continue;}auto value=std::move(returned);frames.pop_back();if(frames.empty()){out.value=std::move(value);break;}returned=std::move(value);continue;}
                    parent.values[parent.next-1]=std::move(returned);returned.reset();
                }
                auto&current=frames.back();const auto ck=nodes_[current.node].kind();const auto&meta=meta_[current.node];const auto arity=MixedNode::arity(ck);
                if(ck==MixedKind::IfThenElse&&current.next==0){current.next=1;frames.push_back({meta.child[0]});continue;}
                if((ck==MixedKind::BoolAnd||ck==MixedKind::BoolOr)&&current.next==0){current.next=1;frames.push_back({meta.child[0]});continue;}
                if(current.next<arity){const auto child=current.next++;frames.push_back({meta.child[child]});continue;}
                auto value=apply(ck,current.values);frames.pop_back();returned=std::move(value);if(frames.empty()){out.value=std::move(returned);break;}
                if(!returned&&out.status==MixedStatus::MathematicalInvalidity)break;
            }
        }
        if(out.value)out.status=MixedStatus::Success;
        return out;
    }
    std::string canonical()const {std::vector<std::string> s;s.reserve(nodes_.size());for(const auto&n:nodes_){auto k=n.kind();if(k==MixedKind::Integer)s.push_back("i64("+std::to_string(n.integer())+")");else if(k==MixedKind::Real){char b[64];auto r=std::to_chars(b,b+sizeof b,n.real(),std::chars_format::general,std::numeric_limits<double>::max_digits10);s.push_back("f64("+std::string(b,r.ptr)+")");}else if(k==MixedKind::SharedRealParameter)s.push_back("alpha"+std::to_string(n.parameterIndex()));else if(k==MixedKind::Boolean)s.push_back(n.integer()?"true":"false");else if(k==MixedKind::Input)s.push_back("x"+std::to_string(n.inputIndex()));else{auto ar=MixedNode::arity(k);std::vector<std::string> a(ar);for(std::size_t j=ar;j>0;--j){a[j-1]=std::move(s.back());s.pop_back();}s.push_back(std::string(mixedKindName(k))+"("+([&]{std::string x;for(std::size_t j=0;j<a.size();++j){if(j)x+=", ";x+=a[j];}return x;})()+")");}}return s.back();}
    std::string serialize()const{std::ostringstream out;out<<(parameterCount_?"genetik.mixed-expression/2 ":"genetik.mixed-expression/1 ")<<inputCount_<<' '<<limits_.maxNodes<<' '<<limits_.maxDepth<<' '<<limits_.maxOperations<<' '<<std::bit_cast<std::uint64_t>(limits_.maxAbsReal)<<' '<<nodes_.size();if(parameterCount_)out<<' '<<parameterCount_;for(const auto&n:nodes_){out<<' '<<static_cast<unsigned>(n.kind());if(n.kind()==MixedKind::Integer)out<<' '<<n.integer();else if(n.kind()==MixedKind::Real)out<<' '<<std::bit_cast<std::uint64_t>(n.real());else if(n.kind()==MixedKind::Boolean)out<<' '<<n.integer();else if(n.kind()==MixedKind::Input)out<<' '<<n.inputIndex();else if(n.kind()==MixedKind::SharedRealParameter)out<<' '<<n.parameterIndex();}return out.str();}
    static MixedExpression deserialize(std::string_view encoded,MixedLimits expected={},std::optional<MixedType> requiredRoot=std::nullopt){std::istringstream in{std::string(encoded)};std::string magic;std::size_t inputs,nodes,parameters=0;MixedLimits limits;std::uint64_t maxReal;if(!(in>>magic>>inputs>>limits.maxNodes>>limits.maxDepth>>limits.maxOperations>>maxReal>>nodes)||(magic!="genetik.mixed-expression/1"&&magic!="genetik.mixed-expression/2"))throw std::invalid_argument("invalid mixed expression serialization header");if(magic=="genetik.mixed-expression/2"&&!(in>>parameters))throw std::invalid_argument("missing shared parameter count");limits.maxAbsReal=std::bit_cast<double>(maxReal);if(limits.maxNodes!=expected.maxNodes||limits.maxDepth!=expected.maxDepth||limits.maxOperations!=expected.maxOperations||std::bit_cast<std::uint64_t>(limits.maxAbsReal)!=std::bit_cast<std::uint64_t>(expected.maxAbsReal)||nodes>limits.maxNodes||parameters>MaxSharedParameters)throw std::invalid_argument("mixed expression serialization limits differ");std::vector<MixedNode> result;result.reserve(nodes);for(std::size_t i=0;i<nodes;++i){unsigned raw;if(!(in>>raw)||raw>static_cast<unsigned>(MixedKind::SharedRealParameter))throw std::invalid_argument("invalid mixed operator identity");auto k=static_cast<MixedKind>(raw);if(k==MixedKind::Integer){std::int64_t v;if(!(in>>v))throw std::invalid_argument("invalid mixed integer constant");result.push_back(MixedNode::integer(v));}else if(k==MixedKind::Real){std::uint64_t v;if(!(in>>v))throw std::invalid_argument("invalid mixed real constant");result.push_back(MixedNode::real(std::bit_cast<double>(v)));}else if(k==MixedKind::SharedRealParameter){std::int64_t v;if(!(in>>v)||v<0)throw std::invalid_argument("invalid shared parameter identity");result.push_back(MixedNode::parameter(static_cast<std::size_t>(v)));}else if(k==MixedKind::Boolean){int v;if(!(in>>v)||(v!=0&&v!=1))throw std::invalid_argument("invalid mixed bool constant");result.push_back(MixedNode::boolean(v!=0));}else if(k==MixedKind::Input){std::size_t v;if(!(in>>v))throw std::invalid_argument("invalid mixed input identity");result.push_back(MixedNode::input(v));}else result.push_back(MixedNode::operation(k));}std::string extra;if(in>>extra)throw std::invalid_argument("trailing mixed expression serialization");return MixedExpression(result,inputs,limits,requiredRoot,parameters);}
private: std::size_t inputCount_,parameterCount_;MixedLimits limits_;std::vector<MixedNode>nodes_;std::vector<Meta>meta_;std::size_t root_{};
};
