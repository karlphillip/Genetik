#pragma once
#include "DiscreteRegression.hpp"
#include <random>
#include <cmath>

struct DiscreteGrammar {
    std::vector<DiscreteKind> operators{DiscreteKind::Add,DiscreteKind::Subtract,DiscreteKind::Multiply,DiscreteKind::Negate,DiscreteKind::Modulo,
        DiscreteKind::Equal,DiscreteKind::NotEqual,DiscreteKind::LessThan,DiscreteKind::LessEqual,DiscreteKind::GreaterThan,DiscreteKind::GreaterEqual,DiscreteKind::IfThenElse};
    std::vector<std::int64_t> constants{-2,-1,0,1,2};
    std::vector<bool> booleanConstants{false,true};
    // nullopt means all original input slots. An empty vector means none.
    std::optional<std::vector<std::size_t>> allowedVariables;
    bool positiveConstantDivisorsOnly=true;
    std::vector<std::int64_t> positiveDivisors{2,3,5};
    std::size_t initialMaxDepth=4,mutationMaxDepth=3,proposalLimit=8;
    std::size_t maxIterationNesting=2;
    bool iterationBoundsAreTerminals=false;
};
struct DiscreteVariationStatistics {std::uint64_t initializations=0,mutations=0,crossovers=0,rejections=0,fallbacks=0;bool operator==(const DiscreteVariationStatistics&) const = default;};
inline DiscreteExpression discreteSubtree(const DiscreteExpression& expression,std::size_t root) {
    const auto nodes=expression.nodes().subspan(expression.subtreeStart(root),root+1-expression.subtreeStart(root));
    std::vector<std::size_t> free;
    for(auto id:expression.scopeAt(root))if(std::any_of(nodes.begin(),nodes.end(),[&](const auto& n){return n.kind()==DiscreteKind::Iterator&&n.binder()==id;}))free.push_back(id);
    return {nodes,expression.dimensions(),expression.limits(),free};
}
inline std::optional<DiscreteExpression> replaceDiscreteSubtree(const DiscreteExpression& parent,std::size_t root,const DiscreteExpression& replacement,DiscreteExpressionLimits limits) {
    if(parent.dimensions()!=replacement.dimensions()||parent.subtreeType(root)!=replacement.type())return {};
    const auto scope=parent.scopeAt(root);for(auto id:replacement.outerBindings())if(std::find(scope.begin(),scope.end(),id)==scope.end())return {};
    const auto start=parent.subtreeStart(root);const auto kept=parent.nodeCount()-(root+1-start);
    if(kept>limits.maxNodes||replacement.nodeCount()>limits.maxNodes-kept)return {};
    if(kept+replacement.nodeCount()>limits.maxStorageBytes/DiscreteExpression::storageBytesPerNode())return {};
    std::vector<std::size_t> used(parent.outerBindings().begin(),parent.outerBindings().end());
    for(std::size_t i=0;i<parent.nodeCount();++i)if((i<start||i>root)&&discreteIteration(parent.nodes()[i].kind()))used.push_back(parent.nodes()[i].binder());
    for(auto id:replacement.outerBindings())if(std::find(used.begin(),used.end(),id)==used.end())used.push_back(id);
    std::vector<std::pair<std::size_t,std::size_t>> remap;
    // Reserve every original incoming identity before choosing fresh replacements.
    auto reserved=used;for(auto id:replacement.binders())reserved.push_back(id);
    for(auto id:replacement.binders()){
        auto fresh=id;if(std::find(used.begin(),used.end(),id)!=used.end()){fresh=0;while(std::find(reserved.begin(),reserved.end(),fresh)!=reserved.end())++fresh;reserved.push_back(fresh);}
        remap.push_back({id,fresh});used.push_back(fresh);
    }
    std::vector<DiscreteNode> nodes;nodes.reserve(kept+replacement.nodeCount());
    nodes.insert(nodes.end(),parent.nodes().begin(),parent.nodes().begin()+start);
    for(auto n:replacement.nodes()){
        if(n.kind()==DiscreteKind::Iterator||discreteIteration(n.kind()))for(auto [before,after]:remap)if(n.binder()==before){n=n.kind()==DiscreteKind::Iterator?DiscreteNode::iterator(after):DiscreteNode::iteration(n.kind(),after);break;}
        nodes.push_back(n);
    }
    nodes.insert(nodes.end(),parent.nodes().begin()+root+1,parent.nodes().end());
    try{return DiscreteExpression(nodes,parent.dimensions(),limits,parent.outerBindings());}catch(const std::length_error&){return {};}catch(const std::invalid_argument&){return {};}
}
class DiscreteVariation {
    using K=DiscreteKind;using T=DiscreteType;
public:
    DiscreteVariation(std::shared_ptr<const DiscreteRegressionProblem> p,DiscreteGrammar grammar={},std::shared_ptr<DiscreteTrainingObjective> objective={})
        :problem_(std::move(p)),grammar_(std::move(grammar)),objective_(std::move(objective)) {
        if(!problem_||(objective_&&objective_->problem()!=problem_))throw std::invalid_argument("discrete variation context mismatch");
        if(!grammar_.initialMaxDepth||!grammar_.mutationMaxDepth||grammar_.initialMaxDepth>32||grammar_.mutationMaxDepth>32||!grammar_.proposalLimit||grammar_.proposalLimit>256)throw std::invalid_argument("discrete planner depth/proposal limits");
        if(grammar_.operators.size()>14||grammar_.constants.size()>256||grammar_.positiveDivisors.size()>256||grammar_.booleanConstants.size()>2)throw std::invalid_argument("discrete grammar source limit");
        for(std::size_t i=0;i<grammar_.operators.size();++i){if(!discreteArity(grammar_.operators[i])||std::find(grammar_.operators.begin(),grammar_.operators.begin()+i,grammar_.operators[i])!=grammar_.operators.begin()+i)throw std::invalid_argument("invalid/duplicate discrete grammar operation");}
        if((grammar_.allowedVariables&&grammar_.allowedVariables->size()>1024)||(!grammar_.allowedVariables&&problem_->dimensions()>1024))throw std::invalid_argument("discrete planner variable source exceeds 1024; supply a restricted original-index set");
        if(grammar_.allowedVariables)variables_=*grammar_.allowedVariables;else for(std::size_t i=0;i<problem_->dimensions();++i)variables_.push_back(i);
        for(std::size_t i=0;i<variables_.size();++i)if(variables_[i]>=problem_->dimensions()||std::find(variables_.begin(),variables_.begin()+i,variables_[i])!=variables_.begin()+i)throw std::invalid_argument("invalid discrete grammar variable");
        for(auto c:grammar_.constants)if(genetik_discrete_detail::magnitude(c)>problem_->limits().maxConstantMagnitude)throw std::invalid_argument("discrete terminal outside bounds");
        for(auto c:grammar_.positiveDivisors)if(c<=0||genetik_discrete_detail::magnitude(c)>problem_->limits().maxConstantMagnitude)throw std::invalid_argument("positive divisor source outside bounds");
        iterationEnabled_=std::any_of(grammar_.operators.begin(),grammar_.operators.end(),discreteIteration);
        if(grammar_.maxIterationNesting>32||(iterationEnabled_&&!grammar_.maxIterationNesting))throw std::invalid_argument("invalid grammar iteration nesting");
        const auto depth=std::min(problem_->limits().maxDepth,std::max(grammar_.initialMaxDepth,grammar_.mutationMaxDepth));witnesses_.resize(depth+1);
        for(std::size_t d=1;d<=depth;++d){
            if(d>1)witnesses_[d]=witnesses_[d-1];
            const auto accept=[&](T type,const std::vector<DiscreteNode>& nodes){try{DiscreteExpression e(nodes,problem_->dimensions(),problem_->limits());auto& w=witnesses_[d][index(type)];if(!w||e.nodeCount()<w->nodeCount()||(e.nodeCount()==w->nodeCount()&&e.constantCount()<w->constantCount()))w=std::move(e);}catch(const std::length_error&){} };
            for(auto v:variables_)accept(T::Int64,{DiscreteNode::variable(v)});
            for(auto c:grammar_.constants)accept(T::Int64,{DiscreteNode::integer(c)});
            for(bool b:grammar_.booleanConstants)accept(T::Bool,{DiscreteNode::boolean(b)});
            if(d==1)continue;
            for(auto k:grammar_.operators)if(!discreteIteration(k))for(auto t:{T::Int64,T::Bool}){
                if(output(k,t)!=t)continue;
                const auto operands=types(k,t);std::vector<DiscreteNode> nodes;bool feasible=true;
                for(std::size_t j=0;j<operands.size();++j){
                    if(k==K::Modulo&&j==1&&grammar_.positiveConstantDivisorsOnly){if(grammar_.positiveDivisors.empty()){feasible=false;break;}nodes.push_back(DiscreteNode::integer(grammar_.positiveDivisors.front()));}
                    else {const auto& w=witnesses_[d-1][index(operands[j])];if(!w){feasible=false;break;}nodes.insert(nodes.end(),w->nodes().begin(),w->nodes().end());}
                }
                if(feasible){nodes.push_back(DiscreteNode::operation(k));accept(t,nodes);}
            }
        }
        const auto d=std::min(grammar_.initialMaxDepth,depth);if(!witnesses_[d][0])throw std::invalid_argument("discrete grammar cannot construct Int64 output within limits");
    }
    DiscreteExpression generate(DiscreteType type,std::mt19937& rng,std::size_t maximumDepth=0) const {
        const auto depth=std::min(maximumDepth?maximumDepth:grammar_.initialMaxDepth,witnesses_.size()-1);
        if(!depth||!witnesses_[depth][index(type)])throw std::invalid_argument("discrete result infeasible under grammar");
        for(std::size_t attempt=0;attempt<grammar_.proposalLimit;++attempt){
            auto nodes=iterationEnabled_?buildScoped(type,depth,std::min(problem_->limits().maxNodes,std::size_t{255}),rng,{},0,0):build(type,depth,std::min(problem_->limits().maxNodes,std::size_t{255}),rng);
            try{DiscreteExpression e(nodes,problem_->dimensions(),problem_->limits());validate(e);return e;}catch(const std::length_error&){}catch(const std::invalid_argument&){}
        }
        return *witnesses_[depth][index(type)]; // finite owned constructive fallback, never a target injection
    }
    void validate(const DiscreteExpression& e) const {
        if(e.dimensions()!=problem_->dimensions())throw std::invalid_argument("discrete variation input schema differs");
        (void)DiscreteExpression(e.nodes(),e.dimensions(),problem_->limits(),e.outerBindings());
        if(e.iterationDepth()>grammar_.maxIterationNesting)throw std::invalid_argument("grammar iteration nesting exceeded");
        for(std::size_t i=0;i<e.nodeCount();++i){const auto& n=e.nodes()[i];
            if(n.kind()==K::Variable){if(std::find(variables_.begin(),variables_.end(),n.variable())==variables_.end())throw std::invalid_argument("forbidden discrete variable");}
            else if(n.kind()==K::Iterator){if(!iterationEnabled_)throw std::invalid_argument("iterator without iteration grammar");}
            else if(n.kind()==K::Integer){if(std::find(grammar_.constants.begin(),grammar_.constants.end(),n.integer())==grammar_.constants.end()){
                // A dedicated positive divisor terminal is permitted only as the right child of modulo.
                bool divisor=false;for(std::size_t j=i+1;j<e.nodeCount();++j)if(e.nodes()[j].kind()==K::Modulo&&e.children(j)[1]==i)divisor=true;
                if(!divisor||std::find(grammar_.positiveDivisors.begin(),grammar_.positiveDivisors.end(),n.integer())==grammar_.positiveDivisors.end())throw std::invalid_argument("forbidden discrete constant");}}
            else if(n.kind()==K::Boolean){if(std::find(grammar_.booleanConstants.begin(),grammar_.booleanConstants.end(),n.boolean())==grammar_.booleanConstants.end())throw std::invalid_argument("forbidden boolean terminal");}
            else if(std::find(grammar_.operators.begin(),grammar_.operators.end(),n.kind())==grammar_.operators.end())throw std::invalid_argument("forbidden discrete operation");
            if(discreteIteration(n.kind())&&grammar_.iterationBoundsAreTerminals)for(std::size_t j=0;j<2;++j)if(discreteArity(e.nodes()[e.children(i)[j]].kind()))throw std::invalid_argument("grammar iteration bounds must be terminals");
            if(n.kind()==K::Modulo&&grammar_.positiveConstantDivisorsOnly){const auto& divisor=e.nodes()[e.children(i)[1]];if(divisor.kind()!=K::Integer||divisor.integer()<=0||std::find(grammar_.positiveDivisors.begin(),grammar_.positiveDivisors.end(),divisor.integer())==grammar_.positiveDivisors.end())throw std::invalid_argument("modulo divisor requires configured positive literal");}
        }
    }
    DiscreteExpressionCandidate initialize(std::mt19937& rng){++stats_.initializations;return {generate(T::Int64,rng),problem_,objective_};}
    DiscreteExpressionCandidate mutation(const DiscreteExpressionCandidate& parent,float rate,std::mt19937& rng){
        if(!std::isfinite(rate)||rate<0||rate>1)throw std::invalid_argument("invalid discrete mutation rate");
        require(parent);auto child=parent;if(std::uniform_real_distribution<float>(0,1)(rng)>=rate)return child;
        ++stats_.mutations;
        for(std::size_t p=0;p<grammar_.proposalLimit;++p){const auto root=choose(parent.expression().nodeCount(),rng);const auto type=parent.expression().subtreeType(root);const auto d=std::min(grammar_.mutationMaxDepth,witnesses_.size()-1);
            if(!witnesses_[d][index(type)]){++stats_.rejections;continue;}
            auto replacement=generate(type,rng,d);
            if(iterationEnabled_){const auto scope=parent.expression().scopeAt(root);auto used=std::vector<std::size_t>(parent.expression().binders().begin(),parent.expression().binders().end());used.insert(used.end(),scope.begin(),scope.end());std::size_t fresh=0;while(std::find(used.begin(),used.end(),fresh)!=used.end())++fresh;
                for(std::size_t attempt=0;attempt<grammar_.proposalLimit;++attempt){try{auto nodes=buildScoped(type,d,std::min(problem_->limits().maxNodes,std::size_t{255}),rng,scope,parent.expression().iterationLevelAt(root),fresh);replacement=DiscreteExpression(nodes,problem_->dimensions(),problem_->limits(),scope);break;}catch(const std::length_error&){}catch(const std::invalid_argument&){}}}
            auto next=replaceDiscreteSubtree(parent.expression(),root,replacement,problem_->limits());
            if(next){try{validate(*next);if(!next->sameRepresentation(parent.expression())){child.replaceExpression(*next);return child;}}catch(const std::invalid_argument&){}}
            ++stats_.rejections;
        }
        ++stats_.fallbacks;return child;
    }
    void crossover(const DiscreteExpressionCandidate& a,const DiscreteExpressionCandidate& b,DiscreteExpressionCandidate& x,DiscreteExpressionCandidate& y,float rate,std::mt19937& rng){
        if(!std::isfinite(rate)||rate<0||rate>1)throw std::invalid_argument("invalid discrete crossover rate");
        require(a);require(b);x=a;y=b;if(std::uniform_real_distribution<float>(0,1)(rng)>=rate)return;++stats_.crossovers;
        for(std::size_t p=0;p<grammar_.proposalLimit;++p){const auto i=choose(a.expression().nodeCount(),rng),j=choose(b.expression().nodeCount(),rng);
            if(a.expression().subtreeType(i)!=b.expression().subtreeType(j)){++stats_.rejections;continue;}
            const auto left=discreteSubtree(a.expression(),i),right=discreteSubtree(b.expression(),j);
            if(!left.outerBindings().empty()||!right.outerBindings().empty()){++stats_.rejections;continue;} // no cross-individual capture
            auto one=replaceDiscreteSubtree(a.expression(),i,right,problem_->limits()),two=replaceDiscreteSubtree(b.expression(),j,left,problem_->limits());
            if(one&&two){try{validate(*one);validate(*two);auto readyX=a,readyY=b;readyX.replaceExpression(*one);readyY.replaceExpression(*two);x=std::move(readyX);y=std::move(readyY);return;}catch(const std::invalid_argument&){}}
            ++stats_.rejections;
        }
        ++stats_.fallbacks;
    }
    DiscreteVariationStatistics statistics() const noexcept{return stats_;}
    void restoreStatistics(DiscreteVariationStatistics s){if(stats_!=DiscreteVariationStatistics{})throw std::invalid_argument("variation restore requires fresh state");stats_=s;}
    const DiscreteGrammar& grammar() const noexcept{return grammar_;}
private:
    static std::size_t index(T t){if(t==T::Int64)return 0;if(t==T::Bool)return 1;throw std::invalid_argument("invalid discrete type");}
    static std::size_t choose(std::size_t n,std::mt19937& r){return std::uniform_int_distribution<std::size_t>(0,n-1)(r);}
    static T output(K k,T requested){if(k==K::IfThenElse)return requested;return k>=K::Equal&&k<=K::GreaterEqual?T::Bool:T::Int64;}
    static std::vector<T> types(K k,T result){if(k==K::IfThenElse)return {T::Bool,result,result};return std::vector<T>(discreteArity(k),T::Int64);}
    std::vector<DiscreteNode> build(T type,std::size_t depth,std::size_t budget,std::mt19937& rng) const {
        std::vector<std::vector<DiscreteNode>> terminals;
        if(type==T::Int64){for(auto v:variables_)terminals.push_back({DiscreteNode::variable(v)});for(auto c:grammar_.constants)if(problem_->limits().maxConstants)terminals.push_back({DiscreteNode::integer(c)});}
        else if(problem_->limits().maxConstants)for(bool b:grammar_.booleanConstants)terminals.push_back({DiscreteNode::boolean(b)});
        std::vector<K> operations;
        if(depth>1)for(auto k:grammar_.operators){if(output(k,type)!=type)continue;auto ts=types(k,type);std::size_t cost=1;bool feasible=true;
            for(std::size_t j=0;j<ts.size();++j){if(k==K::Modulo&&j==1&&grammar_.positiveConstantDivisorsOnly){if(grammar_.positiveDivisors.empty()||!problem_->limits().maxConstants){feasible=false;break;}++cost;}else{const auto& w=witnesses_[depth-1][index(ts[j])];if(!w){feasible=false;break;}cost+=w->nodeCount();}}
            if(feasible&&cost<=budget)operations.push_back(k);
        }
        if(operations.empty()||(!terminals.empty()&&std::bernoulli_distribution(.35)(rng)))return terminals.empty()?std::vector<DiscreteNode>(witnesses_[depth][index(type)]->nodes().begin(),witnesses_[depth][index(type)]->nodes().end()):terminals[choose(terminals.size(),rng)];
        const auto k=operations[choose(operations.size(),rng)];const auto ts=types(k,type);std::vector<DiscreteNode> nodes;
        for(std::size_t j=0;j<ts.size();++j){std::size_t reserve=1;for(std::size_t z=j+1;z<ts.size();++z)reserve+=k==K::Modulo&&z==1&&grammar_.positiveConstantDivisorsOnly?1:witnesses_[depth-1][index(ts[z])]->nodeCount();
            if(k==K::Modulo&&j==1&&grammar_.positiveConstantDivisorsOnly)nodes.push_back(DiscreteNode::integer(grammar_.positiveDivisors[choose(grammar_.positiveDivisors.size(),rng)]));
            else {auto child=build(ts[j],depth-1,budget-nodes.size()-reserve,rng);nodes.insert(nodes.end(),child.begin(),child.end());}}
        nodes.push_back(DiscreteNode::operation(k));return nodes;
    }
    std::vector<DiscreteNode> buildScoped(T type,std::size_t depth,std::size_t budget,std::mt19937& rng,std::vector<std::size_t> scope,std::size_t level,std::size_t nextId) const {
        std::vector<DiscreteNode> terminals;
        if(type==T::Int64){for(auto v:variables_)terminals.push_back(DiscreteNode::variable(v));for(auto id:scope)terminals.push_back(DiscreteNode::iterator(id));if(problem_->limits().maxConstants)for(auto c:grammar_.constants)terminals.push_back(DiscreteNode::integer(c));}
        else if(problem_->limits().maxConstants)for(bool b:grammar_.booleanConstants)terminals.push_back(DiscreteNode::boolean(b));
        std::vector<K> operations;
        const auto minimum=[&](T t){const auto& w=witnesses_[depth-1][index(t)];return t==T::Int64&&!scope.empty()?std::size_t{1}:w?w->nodeCount():std::numeric_limits<std::size_t>::max()/8;};
        if(depth>1)for(auto k:grammar_.operators){if(output(k,type)!=type)continue;if(discreteIteration(k)&&level>=std::min(grammar_.maxIterationNesting,problem_->limits().maxIterationDepth))continue;std::size_t cost=1;for(auto t:types(k,type))cost+=minimum(t);if(cost<=budget)operations.push_back(k);}
        if(operations.empty()||(!terminals.empty()&&std::bernoulli_distribution(.35)(rng))){if(!terminals.empty())return {terminals[choose(terminals.size(),rng)]};const auto& w=witnesses_[depth][index(type)];if(!w)throw std::invalid_argument("infeasible scoped production");return {w->nodes().begin(),w->nodes().end()};}
        const auto k=operations[choose(operations.size(),rng)];const auto ts=types(k,type);std::vector<DiscreteNode> nodes;
        // Unique IDs are assigned by construction order; nested children return their declarations in the node sequence.
        const auto id=nextId;
        if(discreteIteration(k))++nextId;
        for(std::size_t j=0;j<ts.size();++j){std::size_t reserve=1;for(std::size_t z=j+1;z<ts.size();++z)reserve+=minimum(ts[z]);
            if(k==K::Modulo&&j==1&&grammar_.positiveConstantDivisorsOnly){if(grammar_.positiveDivisors.empty())throw std::invalid_argument("empty divisor source");nodes.push_back(DiscreteNode::integer(grammar_.positiveDivisors[choose(grammar_.positiveDivisors.size(),rng)]));}
            else {
                auto inner=scope;if(discreteIteration(k)&&j==2)inner.push_back(id);
                auto child=buildScoped(ts[j],discreteIteration(k)&&j<2&&grammar_.iterationBoundsAreTerminals?1:depth-1,budget-nodes.size()-reserve,rng,std::move(inner),level+(discreteIteration(k)?1:0),nextId);
                for(const auto& n:child)if(discreteIteration(n.kind()))nextId=std::max(nextId,n.binder()+1);
                nodes.insert(nodes.end(),child.begin(),child.end());
            }
        }
        nodes.push_back(discreteIteration(k)?DiscreteNode::iteration(k,id):DiscreteNode::operation(k));return nodes;
    }
    void require(const DiscreteExpressionCandidate& c) const {if(c.problem()!=problem_||c.problemIdentity()!=(objective_?std::shared_ptr<const void>(objective_):std::shared_ptr<const void>(problem_)))throw std::invalid_argument("discrete parent context differs");validate(c.expression());}
    std::shared_ptr<const DiscreteRegressionProblem> problem_;DiscreteGrammar grammar_;std::shared_ptr<DiscreteTrainingObjective> objective_;std::vector<std::size_t> variables_;std::vector<std::array<std::optional<DiscreteExpression>,2>> witnesses_;DiscreteVariationStatistics stats_;bool iterationEnabled_=false;
};
