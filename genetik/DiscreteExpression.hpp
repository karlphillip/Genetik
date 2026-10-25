#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <concepts>
#include <utility>
#include <type_traits>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace genetik_discrete_detail {
template<std::integral I> requires (!std::same_as<I,bool>)
inline std::int64_t checkedInteger(I n) {
    if constexpr(std::numeric_limits<I>::digits>std::numeric_limits<std::int64_t>::digits) {
        if(n>static_cast<I>(std::numeric_limits<std::int64_t>::max()))throw std::invalid_argument("integer terminal outside Int64");
        if constexpr(std::is_signed_v<I>)if(n<static_cast<I>(std::numeric_limits<std::int64_t>::min()))throw std::invalid_argument("integer terminal outside Int64");
    }
    return static_cast<std::int64_t>(n);
}
}
// Separate exact language: never stores integer operands in binary64.
enum class DiscreteType { Int64, Bool };
class DiscreteValue {
public:
    template<std::integral I> requires (!std::same_as<I,bool>)
    static DiscreteValue integer(I n) { return DiscreteValue(genetik_discrete_detail::checkedInteger(n)); }
    template<std::floating_point F> static DiscreteValue integer(F)=delete;
    static DiscreteValue integer(bool)=delete;
    static DiscreteValue boolean(bool b) { return DiscreteValue(b); }
    DiscreteType type() const noexcept { return value_.index()==0?DiscreteType::Int64:DiscreteType::Bool; }
    std::int64_t integer() const { return std::get<std::int64_t>(value_); }
    bool boolean() const { return std::get<bool>(value_); }
    bool operator==(const DiscreteValue&) const = default;
private:
    explicit DiscreteValue(std::int64_t n):value_(n){}
    explicit DiscreteValue(bool b):value_(b){}
    std::variant<std::int64_t,bool> value_;
};
enum class DiscreteKind { Integer, Boolean, Variable, Add, Subtract, Multiply, Negate, Modulo,
    Equal, NotEqual, LessThan, LessEqual, GreaterThan, GreaterEqual, IfThenElse, Iterator=15, Sum=16, Product=17 };
inline bool discreteIteration(DiscreteKind k) noexcept {return k==DiscreteKind::Sum||k==DiscreteKind::Product;}
inline std::size_t discreteArity(DiscreteKind k) {
    switch(k) {
    case DiscreteKind::Integer:case DiscreteKind::Boolean:case DiscreteKind::Variable:case DiscreteKind::Iterator:return 0;
    case DiscreteKind::Negate:return 1;
    case DiscreteKind::IfThenElse:case DiscreteKind::Sum:case DiscreteKind::Product:return 3;
    case DiscreteKind::Add:case DiscreteKind::Subtract:case DiscreteKind::Multiply:case DiscreteKind::Modulo:
    case DiscreteKind::Equal:case DiscreteKind::NotEqual:case DiscreteKind::LessThan:case DiscreteKind::LessEqual:
    case DiscreteKind::GreaterThan:case DiscreteKind::GreaterEqual:return 2;
    }
    throw std::invalid_argument("unknown discrete operator");
}
class DiscreteNode {
public:
    template<std::integral I> requires (!std::same_as<I,bool>)
    static DiscreteNode integer(I n){return {DiscreteKind::Integer,genetik_discrete_detail::checkedInteger(n),0};}
    template<std::floating_point F> static DiscreteNode integer(F)=delete;
    static DiscreteNode integer(bool)=delete;
    static DiscreteNode boolean(bool b){return {DiscreteKind::Boolean,b?1:0,0};}
    static DiscreteNode variable(std::size_t i){return {DiscreteKind::Variable,0,i};}
    static DiscreteNode iterator(std::size_t id){return {DiscreteKind::Iterator,0,id};}
    static DiscreteNode iteration(DiscreteKind k,std::size_t id){if(!discreteIteration(k))throw std::invalid_argument("sum/product binder required");return {k,0,id};}
    std::size_t binder() const {if(kind_!=DiscreteKind::Iterator&&!discreteIteration(kind_))throw std::logic_error("not a bound node");return variable_;}
    static DiscreteNode operation(DiscreteKind k){if(discreteIteration(k))throw std::invalid_argument("iteration requires explicit binder identity");if(!discreteArity(k))throw std::invalid_argument("operator required");return {k,0,0};}
    DiscreteKind kind() const noexcept{return kind_;}
    std::int64_t integer() const {if(kind_!=DiscreteKind::Integer)throw std::logic_error("not integer literal");return literal_;}
    bool boolean() const {if(kind_!=DiscreteKind::Boolean)throw std::logic_error("not boolean literal");return literal_!=0;}
    std::size_t variable() const {if(kind_!=DiscreteKind::Variable)throw std::logic_error("not variable");return variable_;}
    bool operator==(const DiscreteNode&) const = default;
private:
    DiscreteNode(DiscreteKind k,std::int64_t n,std::size_t i):kind_(k),literal_(n),variable_(i){}
    DiscreteKind kind_;std::int64_t literal_;std::size_t variable_;
};
struct DiscreteExpressionLimits {
    std::size_t maxNodes=4096,maxDepth=256,maxVariables=1024,maxConstants=4096;
    std::uint64_t maxConstantMagnitude=std::uint64_t{1}<<63;
    std::size_t maxStorageBytes=4*1024*1024;
    std::size_t maxIterationDepth=8,maxBinders=128;
};
struct DiscreteEvaluationLimits {
    std::size_t maxNodeVisits=4096,maxOperations=4096,maxWorkspaceBytes=1024*1024;
    std::size_t maxIterationsPerOperation=10000,maxAggregateIterations=100000,maxIterationDepth=8;
};
enum class DiscreteEvaluationStatus { Valid, MathematicalInvalid, ResourceExhausted, Cancelled };
struct DiscreteEvaluationResult {
    DiscreteEvaluationStatus status=DiscreteEvaluationStatus::MathematicalInvalid;
    std::optional<DiscreteValue> value;
    std::size_t nodeVisits=0,operations=0;
    const char* reason="";
    std::size_t iterations=0,iterationOperations=0;
    bool valid() const noexcept{return status==DiscreteEvaluationStatus::Valid;}
};
namespace genetik_discrete_detail {
inline std::uint64_t magnitude(std::int64_t n) noexcept {return n<0?std::uint64_t(-(n+1))+1:std::uint64_t(n);}
inline bool arithmetic(DiscreteKind k,std::int64_t a,std::int64_t b,std::int64_t& out) {
    constexpr auto lo=std::numeric_limits<std::int64_t>::min(),hi=std::numeric_limits<std::int64_t>::max();
    switch(k) {
    case DiscreteKind::Add:if((b>0&&a>hi-b)||(b<0&&a<lo-b))return false;out=a+b;return true;
    case DiscreteKind::Subtract:if((b>0&&a<lo+b)||(b<0&&a>hi+b))return false;out=a-b;return true;
    case DiscreteKind::Multiply:{const bool negative=(a<0)!=(b<0);const auto x=magnitude(a),y=magnitude(b),limit=negative?(std::uint64_t{1}<<63):std::uint64_t(hi);
        if(y&&x>limit/y)return false;
        const auto p=x*y;out=negative?(p==(std::uint64_t{1}<<63)?lo:-static_cast<std::int64_t>(p)):static_cast<std::int64_t>(p);return true;}
    case DiscreteKind::Negate:if(a==lo)return false;out=-a;return true;
    case DiscreteKind::Modulo:if(b<=0)return false;out=a%b;if(out<0)out+=b;return true;
    default:throw std::logic_error("not discrete arithmetic");
    }
}
inline void limit(std::size_t n,std::size_t cap,const char* message){if(n>cap)throw std::length_error(message);}
inline void validateLimits(const DiscreteExpressionLimits& l) {
    if(!l.maxNodes||!l.maxDepth||!l.maxStorageBytes||l.maxConstantMagnitude>(std::uint64_t{1}<<63)||l.maxIterationDepth>128||l.maxBinders>4096)throw std::invalid_argument("invalid discrete expression limits");
}
}
class DiscreteExpression {
    struct Metadata {DiscreteType type;std::size_t depth,start;std::array<std::size_t,3> children{};std::size_t iterationDepth=0;};
public:
    DiscreteExpression(std::span<const DiscreteNode> nodes,std::size_t dimensions,DiscreteExpressionLimits limits={},std::span<const std::size_t> outerBindings={}) :dimensions_(dimensions),limits_(limits) {
        using namespace genetik_discrete_detail;validateLimits(limits);
        if(nodes.empty())throw std::invalid_argument("empty discrete expression");
        limit(nodes.size(),limits.maxNodes,"discrete node limit");limit(dimensions,limits.maxVariables,"discrete input limit");
        const auto bytes=storageBytesPerNode();
        if(outerBindings.size()>limits.maxStorageBytes/(3*sizeof(std::size_t)))throw std::length_error("outer scope storage limit");
        limit(nodes.size(),(limits.maxStorageBytes-outerBindings.size()*3*sizeof(std::size_t))/bytes,"discrete construction storage limit");
        if(outerBindings.size()>limits.maxBinders)throw std::length_error("outer binder limit");
        outer_.assign(outerBindings.begin(),outerBindings.end());
        for(std::size_t i=0;i<outer_.size();++i)if(std::find(outer_.begin(),outer_.begin()+i,outer_[i])!=outer_.begin()+i)throw std::invalid_argument("duplicate outer binder");
        std::vector<std::size_t> stack;stack.reserve(nodes.size());metadata_.reserve(nodes.size());
        for(std::size_t i=0;i<nodes.size();++i){const auto& n=nodes[i];auto arity=discreteArity(n.kind());
            if(stack.size()<arity)throw std::invalid_argument("discrete postfix arity");
            Metadata m{DiscreteType::Int64,1,i,{}};
            for(std::size_t j=0;j<arity;++j)m.children[j]=stack[stack.size()-arity+j];
            if(!arity){
                if(n.kind()==DiscreteKind::Variable){if(n.variable()>=dimensions)throw std::invalid_argument("discrete variable outside schema");}
                else if(n.kind()!=DiscreteKind::Iterator) {++constants_;limit(constants_,limits.maxConstants,"discrete constant count limit");
                    if(n.kind()==DiscreteKind::Integer&&magnitude(n.integer())>limits.maxConstantMagnitude)throw std::invalid_argument("discrete literal outside coefficient bounds");
                    if(n.kind()==DiscreteKind::Boolean)m.type=DiscreteType::Bool;}
            }else{
                const auto type=[&](std::size_t j){return metadata_[m.children[j]].type;};
                for(std::size_t j=0;j<arity;++j){if(metadata_[m.children[j]].depth>=limits.maxDepth)throw std::length_error("discrete depth limit");m.depth=std::max(m.depth,metadata_[m.children[j]].depth+1);}
                m.start=metadata_[m.children[0]].start;
                for(std::size_t j=0;j<arity;++j)m.iterationDepth=std::max(m.iterationDepth,metadata_[m.children[j]].iterationDepth);
                if(discreteIteration(n.kind())){++m.iterationDepth;if(m.iterationDepth>limits.maxIterationDepth)throw std::length_error("iteration nesting limit");
                    if(std::find(binders_.begin(),binders_.end(),n.binder())!=binders_.end()||std::find(outer_.begin(),outer_.end(),n.binder())!=outer_.end())throw std::invalid_argument("duplicate binder identity");
                    if(binders_.size()>=limits.maxBinders-outer_.size())throw std::length_error("binder count limit");
                    binders_.push_back(n.binder());}
                if(n.kind()==DiscreteKind::IfThenElse){if(type(0)!=DiscreteType::Bool||type(1)!=type(2))throw std::invalid_argument("if requires Bool,T,T");m.type=type(1);}
                else {for(std::size_t j=0;j<arity;++j)if(type(j)!=DiscreteType::Int64)throw std::invalid_argument("integer operands required");
                    if(n.kind()>=DiscreteKind::Equal&&n.kind()<=DiscreteKind::GreaterEqual)m.type=DiscreteType::Bool;}
                stack.resize(stack.size()-arity);
            }
            metadata_.push_back(m);stack.push_back(i);
        }
        if(stack.size()!=1)throw std::invalid_argument("discrete postfix has extra roots");
        nodes_.assign(nodes.begin(),nodes.end());
        validateScope();
    }
    DiscreteExpression(const DiscreteExpression&)=default;
    DiscreteExpression(DiscreteExpression&&) noexcept=default;
    DiscreteExpression& operator=(DiscreteExpression other) noexcept {swap(other);return *this;}
    template<std::integral I> requires (!std::same_as<I,bool>)
    static DiscreteExpression integer(I n,std::size_t dimensions=0,DiscreteExpressionLimits l={}) {const std::array a{DiscreteNode::integer(n)};return {a,dimensions,l};}
    template<std::floating_point F> static DiscreteExpression integer(F,std::size_t=0,DiscreteExpressionLimits={})=delete;
    static DiscreteExpression integer(bool,std::size_t=0,DiscreteExpressionLimits={})=delete;
    static DiscreteExpression boolean(bool b,std::size_t dimensions=0,DiscreteExpressionLimits l={}) {const std::array a{DiscreteNode::boolean(b)};return {a,dimensions,l};}
    static DiscreteExpression variable(std::size_t i,std::size_t dimensions,DiscreteExpressionLimits l={}) {const std::array a{DiscreteNode::variable(i)};return {a,dimensions,l};}
    static DiscreteExpression unary(DiscreteKind k,const DiscreteExpression& a,DiscreteExpressionLimits l={}) {
        if(discreteArity(k)!=1)throw std::invalid_argument("unary discrete operator required");
        const std::array<const DiscreteExpression*,1> args{&a};return compose(k,args,l);
    }
    static DiscreteExpression binary(DiscreteKind k,const DiscreteExpression& a,const DiscreteExpression& b,DiscreteExpressionLimits l={}) {
        if(discreteArity(k)!=2)throw std::invalid_argument("binary discrete operator required");
        const std::array<const DiscreteExpression*,2> args{&a,&b};return compose(k,args,l);
    }
    static DiscreteExpression conditional(const DiscreteExpression& condition,const DiscreteExpression& yes,const DiscreteExpression& no,DiscreteExpressionLimits l={}) {
        const std::array<const DiscreteExpression*,3> args{&condition,&yes,&no};return compose(DiscreteKind::IfThenElse,args,l);
    }
    static DiscreteExpression iteration(DiscreteKind k,std::size_t id,const DiscreteExpression& lower,const DiscreteExpression& upper,const DiscreteExpression& body,DiscreteExpressionLimits l={}) {
        if(!discreteIteration(k))throw std::invalid_argument("sum/product required");
        std::vector<std::size_t> scope;
        for(const auto* e:{&lower,&upper,&body})for(auto outer:e->outerBindings())if(!(e==&body&&outer==id)&&std::find(scope.begin(),scope.end(),outer)==scope.end())scope.push_back(outer);
        if(std::find(lower.outerBindings().begin(),lower.outerBindings().end(),id)!=lower.outerBindings().end()||std::find(upper.outerBindings().begin(),upper.outerBindings().end(),id)!=upper.outerBindings().end())throw std::invalid_argument("binder not visible in own bounds");
        std::size_t count=1;for(const auto* e:{&lower,&upper,&body}){if(e->dimensions()!=lower.dimensions())throw std::invalid_argument("iteration schema mismatch");if(count>l.maxNodes||e->nodeCount()>l.maxNodes-count)throw std::length_error("iteration node limit");count+=e->nodeCount();}
        if(count>l.maxStorageBytes/storageBytesPerNode())throw std::length_error("iteration storage limit");
        std::vector<DiscreteNode> nodes;nodes.reserve(count);for(const auto* e:{&lower,&upper,&body})nodes.insert(nodes.end(),e->nodes().begin(),e->nodes().end());nodes.push_back(DiscreteNode::iteration(k,id));return {nodes,lower.dimensions(),l,scope};
    }
    std::span<const std::size_t> outerBindings() const noexcept{return outer_;}
    std::span<const std::size_t> binders() const noexcept{return binders_;}
    std::vector<std::size_t> scopeAt(std::size_t target) const {
        if(target>=nodeCount())throw std::out_of_range("scope root outside expression");
        auto scope=outer_;std::size_t root=nodeCount()-1;
        while(root!=target){bool found=false;for(std::size_t j=0;j<discreteArity(nodes_[root].kind());++j){const auto child=metadata_[root].children[j];if(target>=metadata_[child].start&&target<=child){if(discreteIteration(nodes_[root].kind())&&j==2)scope.push_back(nodes_[root].binder());root=child;found=true;break;}}if(!found)throw std::logic_error("invalid subtree scope");}return scope;
    }
    static constexpr std::size_t storageBytesPerNode() noexcept {return sizeof(DiscreteNode)+sizeof(Metadata)+8*sizeof(std::size_t);}
    std::span<const DiscreteNode> nodes() const noexcept{return nodes_;}
    std::size_t dimensions() const noexcept{return dimensions_;}
    std::size_t nodeCount() const noexcept{return nodes_.size();}
    std::size_t depth() const {if(metadata_.empty())throw std::logic_error("moved-from discrete expression");return metadata_.back().depth;}
    std::size_t iterationDepth() const {(void)type();return metadata_.back().iterationDepth;}
    std::size_t iterationLevelAt(std::size_t target) const {
        if(target>=nodeCount())throw std::out_of_range("iteration root outside expression");
        std::size_t root=nodeCount()-1,level=0;while(root!=target){if(discreteIteration(nodes_[root].kind()))++level;bool found=false;for(std::size_t j=0;j<discreteArity(nodes_[root].kind());++j){auto child=metadata_[root].children[j];if(target>=metadata_[child].start&&target<=child){root=child;found=true;break;}}if(!found)throw std::logic_error("invalid iteration root");}return level;
    }
    std::size_t constantCount() const noexcept{return constants_;}
    DiscreteType type() const {if(metadata_.empty())throw std::logic_error("moved-from discrete expression");return metadata_.back().type;}
    DiscreteType subtreeType(std::size_t i) const{return metadata_.at(i).type;}
    std::size_t subtreeStart(std::size_t i) const{return metadata_.at(i).start;}
    const auto& children(std::size_t i) const{return metadata_.at(i).children;}
    DiscreteExpressionLimits limits() const noexcept{return limits_;}
    bool sameRepresentation(const DiscreteExpression& e) const noexcept{return dimensions_==e.dimensions_&&outer_==e.outer_&&nodes_==e.nodes_;}
    void swap(DiscreteExpression& e) noexcept {nodes_.swap(e.nodes_);metadata_.swap(e.metadata_);outer_.swap(e.outer_);binders_.swap(e.binders_);std::swap(dimensions_,e.dimensions_);std::swap(limits_,e.limits_);std::swap(constants_,e.constants_);}
    DiscreteEvaluationResult evaluate(std::span<const std::int64_t> inputs,DiscreteEvaluationLimits l={},const std::atomic<bool>* stop=nullptr) const {
        return evaluateWithCancellation(inputs,l,[&]{return stop&&stop->load(std::memory_order_relaxed);});
    }
    template<class Stop> DiscreteEvaluationResult evaluateWithCancellation(std::span<const std::int64_t> inputs,DiscreteEvaluationLimits l,Stop&& stop) const {
        (void)depth();
        if(!outer_.empty())throw std::invalid_argument("open scoped fragment is not executable");
        if(inputs.size()!=dimensions_)throw std::invalid_argument("discrete input dimension mismatch");
        if(l.maxIterationDepth>128||l.maxIterationsPerOperation>1000000000||l.maxAggregateIterations>1000000000)throw std::invalid_argument("invalid iteration evaluation limits");
        struct Frame {std::size_t index,stage;DiscreteValue left=DiscreteValue::integer(0);std::int64_t cursor=0,upper=0,accumulator=0;};
        DiscreteEvaluationResult result;std::vector<Frame> frames;DiscreteValue last=DiscreteValue::integer(0);std::size_t activeIterations=0;
        const auto fail=[&](DiscreteEvaluationStatus status,const char* why){result.status=status;result.reason=why;return result;};
        const auto frameCap=l.maxWorkspaceBytes/sizeof(Frame);
        if(!frameCap)return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete frame storage exhausted");
        frames.reserve(std::min(depth(),frameCap));frames.push_back({nodes_.size()-1,0});
        const auto charge=[&]{if(result.operations==l.maxOperations)return false;++result.operations;return true;};
        while(!frames.empty()){
            if(stop())return fail(DiscreteEvaluationStatus::Cancelled,"discrete evaluation cancelled");
            auto& f=frames.back();const auto i=f.index;const auto& n=nodes_[i];const auto arity=discreteArity(n.kind());
            if(f.stage==0){
                if(result.nodeVisits==l.maxNodeVisits)return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete node visits exhausted");
                ++result.nodeVisits;
                if(!arity){
                    if(n.kind()==DiscreteKind::Iterator){bool found=false;for(auto it=frames.rbegin();it!=frames.rend();++it)if(discreteIteration(nodes_[it->index].kind())&&it->stage==3&&nodes_[it->index].binder()==n.binder()){last=DiscreteValue::integer(it->cursor);found=true;break;}if(!found)throw std::logic_error("validated iterator binding missing");}
                    else last=n.kind()==DiscreteKind::Integer?DiscreteValue::integer(n.integer()):n.kind()==DiscreteKind::Boolean?DiscreteValue::boolean(n.boolean()):DiscreteValue::integer(inputs[n.variable()]);
                    frames.pop_back();continue;
                }
                if(discreteIteration(n.kind())){if(activeIterations==l.maxIterationDepth)return fail(DiscreteEvaluationStatus::ResourceExhausted,"iteration nesting exhausted");++activeIterations;++result.iterationOperations;}
                f.stage=1;if(frames.size()==frameCap)return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete frame storage exhausted");frames.push_back({metadata_[i].children[0],0});continue;
            }
            if(discreteIteration(n.kind())){
                if(f.stage==1){f.left=last;f.stage=2;if(frames.size()==frameCap)return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete frame storage exhausted");frames.push_back({metadata_[i].children[1],0});continue;}
                if(f.stage==2){
                    if(!charge())return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete operations exhausted");
                    f.cursor=f.left.integer();f.upper=last.integer();f.accumulator=n.kind()==DiscreteKind::Sum?0:1;
                    if(f.cursor>f.upper){last=DiscreteValue::integer(f.accumulator);--activeIterations;frames.pop_back();continue;}
                    // Unsigned subtraction gives the ordered mathematical distance in [0,2^64-1].
                    const auto distance=static_cast<std::uint64_t>(f.upper)-static_cast<std::uint64_t>(f.cursor);
                    if(distance>=l.maxIterationsPerOperation)return fail(DiscreteEvaluationStatus::ResourceExhausted,"per-operation iteration allowance exhausted");
                    f.stage=3;
                }else{
                    if(!charge())return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete operations exhausted");
                    std::int64_t next=0;if(!genetik_discrete_detail::arithmetic(n.kind()==DiscreteKind::Sum?DiscreteKind::Add:DiscreteKind::Multiply,f.accumulator,last.integer(),next))return fail(DiscreteEvaluationStatus::MathematicalInvalid,"iteration accumulation overflow");f.accumulator=next;
                    if(f.cursor==f.upper){last=DiscreteValue::integer(f.accumulator);--activeIterations;frames.pop_back();continue;}
                    if(!charge())return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete operations exhausted");
                    ++f.cursor; // cursor < upper <= INT64_MAX; never increment after the last term.
                }
                if(result.iterations==l.maxAggregateIterations)return fail(DiscreteEvaluationStatus::ResourceExhausted,"aggregate iteration allowance exhausted");
                if(frames.size()==frameCap)return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete frame storage exhausted");
                ++result.iterations;frames.push_back({metadata_[i].children[2],0});continue;
            }
            if(n.kind()==DiscreteKind::IfThenElse){
                if(f.stage==1){if(!charge())return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete operations exhausted");f.stage=2;const auto branch=last.boolean()?1:2;if(frames.size()==frameCap)return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete frame storage exhausted");frames.push_back({metadata_[i].children[branch],0});continue;}
                frames.pop_back();continue;
            }
            if(arity==2&&f.stage==1){f.left=last;f.stage=2;if(frames.size()==frameCap)return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete frame storage exhausted");frames.push_back({metadata_[i].children[1],0});continue;}
            if(!charge())return fail(DiscreteEvaluationStatus::ResourceExhausted,"discrete operations exhausted");
            const auto a=arity==1?last.integer():f.left.integer(),b=arity==1?0:last.integer();std::int64_t out=0;
            switch(n.kind()){
            case DiscreteKind::Equal:last=DiscreteValue::boolean(a==b);break;
            case DiscreteKind::NotEqual:last=DiscreteValue::boolean(a!=b);break;
            case DiscreteKind::LessThan:last=DiscreteValue::boolean(a<b);break;
            case DiscreteKind::LessEqual:last=DiscreteValue::boolean(a<=b);break;
            case DiscreteKind::GreaterThan:last=DiscreteValue::boolean(a>b);break;
            case DiscreteKind::GreaterEqual:last=DiscreteValue::boolean(a>=b);break;
            default:if(!genetik_discrete_detail::arithmetic(n.kind(),a,b,out))return fail(DiscreteEvaluationStatus::MathematicalInvalid,"integer overflow or nonpositive modulo divisor");last=DiscreteValue::integer(out);
            }
            frames.pop_back();
        }
        result.status=DiscreteEvaluationStatus::Valid;result.value=last;return result;
    }
    std::string toString(std::size_t maxBytes=1024*1024) const;
private:
    void validateScope() const {
        struct Walk {std::size_t node,stage;};std::vector<Walk> walk;walk.reserve(depth());auto active=outer_;walk.push_back({nodeCount()-1,0});
        while(!walk.empty()){auto& w=walk.back();const auto& n=nodes_[w.node];const auto arity=discreteArity(n.kind());
            if(!arity){if(n.kind()==DiscreteKind::Iterator&&std::find(active.begin(),active.end(),n.binder())==active.end())throw std::invalid_argument("unbound iterator");walk.pop_back();continue;}
            if(w.stage==3&&discreteIteration(n.kind()))active.pop_back();
            if(w.stage<arity){auto j=w.stage++;if(discreteIteration(n.kind())&&j==2)active.push_back(n.binder());walk.push_back({metadata_[w.node].children[j],0});}else walk.pop_back();
        }
    }
    static DiscreteExpression compose(DiscreteKind k,std::span<const DiscreteExpression* const> args,DiscreteExpressionLimits l) {
        genetik_discrete_detail::validateLimits(l);std::size_t count=1;
        for(const auto* e:args){(void)e->type();if(e->dimensions()!=args.front()->dimensions())throw std::invalid_argument("discrete operand schema mismatch");if(count>l.maxNodes||e->nodeCount()>l.maxNodes-count)throw std::length_error("discrete composition node limit");count+=e->nodeCount();}
        if(count>l.maxStorageBytes/(sizeof(DiscreteNode)+sizeof(Metadata)+sizeof(std::size_t)))throw std::length_error("discrete composition storage limit");
        std::vector<std::size_t> scope;for(const auto* e:args)for(auto id:e->outerBindings())if(std::find(scope.begin(),scope.end(),id)==scope.end())scope.push_back(id);
        std::vector<DiscreteNode> nodes;nodes.reserve(count);for(const auto* e:args)nodes.insert(nodes.end(),e->nodes().begin(),e->nodes().end());nodes.push_back(DiscreteNode::operation(k));return {nodes,args.front()->dimensions(),l,scope};
    }
    std::vector<DiscreteNode> nodes_;std::vector<Metadata> metadata_;std::vector<std::size_t> outer_,binders_;std::size_t dimensions_;DiscreteExpressionLimits limits_;std::size_t constants_=0;
};
inline const char* discreteSymbol(DiscreteKind k) {
    switch(k){case DiscreteKind::Add:return "+";case DiscreteKind::Subtract:return "-";case DiscreteKind::Multiply:return "*";case DiscreteKind::Modulo:return "%";
    case DiscreteKind::Equal:return "==";case DiscreteKind::NotEqual:return "!=";case DiscreteKind::LessThan:return "<";case DiscreteKind::LessEqual:return "<=";case DiscreteKind::GreaterThan:return ">";case DiscreteKind::GreaterEqual:return ">=";
    default:throw std::invalid_argument("not binary discrete symbol");}
}
inline std::string DiscreteExpression::toString(std::size_t maxBytes) const {
    struct Event {std::size_t node;unsigned stage;};std::vector<Event> stack;stack.reserve(depth());stack.push_back({nodeCount()-1,0});std::string out;
    const auto emit=[&](std::string_view s){if(s.size()>maxBytes-out.size())throw std::length_error("discrete export size limit");out.append(s);};
    while(!stack.empty()){
        auto& f=stack.back();const auto& n=nodes_[f.node];const auto arity=discreteArity(n.kind());
        if(!arity){if(n.kind()==DiscreteKind::Integer)emit(std::to_string(n.integer()));else if(n.kind()==DiscreteKind::Boolean)emit(n.boolean()?"true":"false");else if(n.kind()==DiscreteKind::Iterator){emit("k");emit(std::to_string(n.binder()));}else {emit("x");emit(std::to_string(n.variable()));}stack.pop_back();continue;}
        if(!f.stage){if(discreteIteration(n.kind())){emit(n.kind()==DiscreteKind::Sum?"sum(k":"product(k");emit(std::to_string(n.binder()));emit(", ");}else emit(n.kind()==DiscreteKind::IfThenElse?"if(":arity==1?"neg(":"(");f.stage=1;stack.push_back({metadata_[f.node].children[0],0});continue;}
        if(f.stage<arity){const auto child=f.stage++;if(arity==3)emit(", ");else {emit(" ");emit(discreteSymbol(n.kind()));emit(" ");}stack.push_back({metadata_[f.node].children[child],0});continue;}
        emit(")");stack.pop_back();
    }
    return out;
}
