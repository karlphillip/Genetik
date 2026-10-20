#pragma once
#include "TypedRegression.hpp"
#include "ExpressionVariation.hpp" // common variation counters, no scalar encoding

struct TypedVariationStatistics : VariationStatistics {
    std::uint64_t constructionProposals=0,constructionFallbacks=0;
    bool operator==(const TypedVariationStatistics&) const = default;
};
struct TypedGrammar {
    std::vector<TypedExpressionKind> operators{TypedExpressionKind::Add,TypedExpressionKind::Subtract,
        TypedExpressionKind::Multiply,TypedExpressionKind::Dot,TypedExpressionKind::MatVec,
        TypedExpressionKind::MatMul,TypedExpressionKind::Transpose};
    // Empty shapes resolves to distinct schema/output/scalar shapes, in that order.
    std::vector<TypedShape> shapes;
    std::vector<TypedShape> constantShapes{TypedShape::scalar()};
    std::vector<double> constants{-1,0,1};
    std::size_t initialMaxDepth=4,mutationMaxDepth=3,proposalLimit=8;
    double operationProbability=.65;
};
inline std::pair<std::size_t,std::size_t> typedSubtree(const TypedExpression& e,std::size_t root) {
    const auto nodes=e.nodes();
    if(root>=nodes.size())throw std::out_of_range("typed subtree root outside expression");
    std::vector<std::size_t> starts;
    for(std::size_t i=0;i<=root;++i) {
        const auto arity=typedExpressionArity(nodes[i].kind());
        if(!arity)starts.push_back(i);
        else for(std::size_t j=1;j<arity;++j)starts.pop_back();
    }
    return {starts.back(),root+1};
}
inline TypedExpression typedSubexpression(const TypedExpression& e,std::size_t root) {
    const auto [begin,end]=typedSubtree(e,root);const auto nodes=e.nodes();
    return TypedExpression(std::span(nodes.data()+begin,end-begin),e.schema(),e.limits());
}
// Only expected complete-plan limit rejection returns nullopt. Bad schemas are API errors.
inline std::optional<TypedExpression> replaceTypedSubtree(const TypedExpression& parent,std::size_t root,
        const TypedExpression& replacement,TypedExpressionLimits limits) {
    if(parent.schema()!=replacement.schema())throw std::invalid_argument("typed replacement schema differs");
    if(parent.nodeShape(root)!=replacement.shape())return {};
    auto nodes=parent.nodes();const auto inserted=replacement.nodes();const auto [begin,end]=typedSubtree(parent,root);
    const auto remaining=nodes.size()-(end-begin);
    if(remaining>limits.maxNodes || inserted.size()>limits.maxNodes-remaining)return {};
    nodes.erase(nodes.begin()+begin,nodes.begin()+end);nodes.insert(nodes.begin()+begin,inserted.begin(),inserted.end());
    try{return TypedExpression(nodes,parent.schema(),limits);}catch(const TypedLimitError&){return {};}
}
class TypedVariation {
    using K=TypedExpressionKind;
    struct Production {K kind;std::size_t left,right;};
public:
    TypedVariation(std::shared_ptr<const TypedRegressionProblem> problem,TypedGrammar grammar={},
                   std::shared_ptr<TypedRegressionPreparation> preparation={})
        :problem_(std::move(problem)),grammar_(std::move(grammar)),preparation_(std::move(preparation)) {
        if(!problem_ || !grammar_.initialMaxDepth || !grammar_.mutationMaxDepth || !grammar_.proposalLimit ||
           !std::isfinite(grammar_.operationProbability) || grammar_.operationProbability<0 || grammar_.operationProbability>1)
            throw std::invalid_argument("invalid typed grammar");
        // Planning is bounded independently of user expression limits.
        if(grammar_.initialMaxDepth>64 || grammar_.mutationMaxDepth>64 || grammar_.proposalLimit>4096)
            throw std::invalid_argument("typed grammar planner limits exceeded");
        if(grammar_.operators.size()>9)throw std::invalid_argument("typed grammar has too many operator kinds");
        for(std::size_t i=0;i<grammar_.operators.size();++i) {
            if(!typedExpressionArity(grammar_.operators[i]))throw std::invalid_argument("typed grammar operator is terminal");
            if(std::find(grammar_.operators.begin(),grammar_.operators.begin()+i,grammar_.operators[i])!=grammar_.operators.begin()+i)
                throw std::invalid_argument("duplicate typed grammar operator");
        }
        if(grammar_.shapes.empty()) {
            grammar_.shapes=problem_->schema();grammar_.shapes.push_back(problem_->outputShape());grammar_.shapes.push_back(TypedShape::scalar());
            std::vector<TypedShape> distinct;for(auto s:grammar_.shapes)if(std::find(distinct.begin(),distinct.end(),s)==distinct.end())distinct.push_back(s);
            grammar_.shapes=std::move(distinct);
        }
        if(grammar_.shapes.size()>16 || grammar_.shapes.empty())throw std::invalid_argument("typed grammar requires 1..16 admissible shapes");
        for(std::size_t i=0;i<grammar_.shapes.size();++i) {
            genetik_typed_detail::validateShape(grammar_.shapes[i],problem_->limits().values);
            if(std::find(grammar_.shapes.begin(),grammar_.shapes.begin()+i,grammar_.shapes[i])!=grammar_.shapes.begin()+i)
                throw std::invalid_argument("duplicate typed grammar shape");
        }
        for(auto s:problem_->schema())(void)shapeIndex(s);
        (void)shapeIndex(problem_->outputShape());
        for(auto s:grammar_.constantShapes)(void)shapeIndex(s);
        if(!grammar_.constantShapes.empty() && grammar_.constants.empty())throw std::invalid_argument("typed constant source is empty");
        for(double c:grammar_.constants)if(!std::isfinite(c) || std::abs(c)>problem_->limits().maxAbsConstant)
            throw std::invalid_argument("typed constant outside limits");
        const auto depth=std::min(problem_->limits().maxDepth,std::max(grammar_.initialMaxDepth,grammar_.mutationMaxDepth));
        witnesses_.resize(depth+1,std::vector<std::optional<TypedExpression>>(grammar_.shapes.size()));
        for(std::size_t d=1;d<=depth;++d) {
            auto& level=witnesses_[d];if(d>1)level=witnesses_[d-1];
            for(std::size_t s=0;s<grammar_.shapes.size();++s) {
                auto accept=[&](TypedExpression e){if(!level[s] || e.nodeCount()<level[s]->nodeCount())level[s]=std::move(e);};
                for(std::size_t v=0;v<problem_->schema().size();++v)if(problem_->schema()[v]==grammar_.shapes[s])
                    try{accept(TypedExpression::variable(v,problem_->schema(),problem_->limits()));}catch(const TypedLimitError&){}
                if(isConstantShape(s))try{accept(TypedExpression::constant(constant(s,nullptr),problem_->schema(),problem_->limits()));}catch(const TypedLimitError&){}
                if(d==1)continue;
                for(auto op:grammar_.operators)for(std::size_t a=0;a<grammar_.shapes.size();++a) {
                    if(!witnesses_[d-1][a])continue;
                    const auto arity=typedExpressionArity(op);
                    const auto n=arity==1?std::size_t{1}:grammar_.shapes.size();
                    for(std::size_t b=0;b<n;++b) {
                        if(arity==2 && !witnesses_[d-1][b])continue;
                        const auto output=genetik_typed_detail::operationShape(op,grammar_.shapes[a],arity==1?grammar_.shapes[a]:grammar_.shapes[b]);
                        if(!output || *output!=grammar_.shapes[s])continue;
                        auto expression=compose(op,*witnesses_[d-1][a],arity==1?nullptr:&*witnesses_[d-1][b]);
                        if(expression && expression->shape()==grammar_.shapes[s])accept(std::move(*expression));
                    }
                }
            }
        }
        productionCache_.resize(depth+1,std::vector<std::vector<Production>>(grammar_.shapes.size()));
        for(std::size_t d=2;d<=depth;++d)for(std::size_t s=0;s<grammar_.shapes.size();++s)
            productionCache_[d][s]=makeProductions(s,d,problem_->limits().maxNodes);
        const auto output=shapeIndex(problem_->outputShape());
        const auto d=std::min(grammar_.initialMaxDepth,depth);
        if(!witnesses_[d][output])throw std::invalid_argument("typed output has no feasible grammar witness within initial limits");
        // Reject predictable dataset-work excess before random initialization.
        (void)problem_->checkedExpression(*witnesses_[d][output]);
    }
    TypedVariationStatistics statistics() const noexcept{return statistics_;}
    void markElite(TypedExpressionCandidate& c) const{c.setPreparationOrigin(PreparationOrigin::Elite);}
    void markOffspring(TypedExpressionCandidate& c,const TypedExpressionCandidate& p) const {
        c.setPreparationOrigin(c.text()==p.text()?PreparationOrigin::UnchangedOffspring:PreparationOrigin::ChangedOffspring);
    }
    TypedExpressionCandidate initialize(std::mt19937& random) {
        countTrainingWork(statistics_.initializations);
        return TypedExpressionCandidate(randomExpression(problem_->outputShape(),grammar_.initialMaxDepth,random),problem_,preparation_);
    }
    TypedExpressionCandidate mutation(const TypedExpressionCandidate& parent,float rate,std::mt19937& random) {
        requireProblem(parent);auto child=parent;
        if(std::uniform_real_distribution<float>(0,1)(random)>=rate)return child;
        for(std::size_t i=0;i<grammar_.proposalLimit;++i) {
            countTrainingWork(statistics_.mutationProposals);
            const auto root=index(parent.expression().nodeCount(),random);
            const auto shape=parent.expression().nodeShape(root);
            const auto depth=std::min(grammar_.mutationMaxDepth,witnesses_.size()-1);
            if(!witnesses_[depth][shapeIndex(shape)]){countTrainingWork(statistics_.structuralRejections);continue;}
            auto replacement=randomExpression(shape,grammar_.mutationMaxDepth,random);
            auto next=replaceTypedSubtree(parent.expression(),root,replacement,problem_->limits());
            if(next)try{*next=problem_->checkedExpression(*next);}catch(const TypedLimitError&){next.reset();}
            if(!next){countTrainingWork(statistics_.structuralRejections);continue;}
            child.replaceExpression(*next);if(child.text()!=parent.text())countTrainingWork(statistics_.mutationChanges);return child;
        }
        countTrainingWork(statistics_.mutationFallbacks);return child;
    }
    void crossover(const TypedExpressionCandidate& father,const TypedExpressionCandidate& mother,
            TypedExpressionCandidate& a,TypedExpressionCandidate& b,float rate,std::mt19937& random) {
        requireProblem(father);requireProblem(mother);auto left=father,right=mother;
        if(std::uniform_real_distribution<float>(0,1)(random)<rate) {
            bool accepted=false;
            for(std::size_t i=0;i<grammar_.proposalLimit;++i) {
                countTrainingWork(statistics_.crossoverProposals);
                const auto fr=index(father.expression().nodeCount(),random),mr=index(mother.expression().nodeCount(),random);
                if(father.expression().nodeShape(fr)!=mother.expression().nodeShape(mr)){countTrainingWork(statistics_.structuralRejections);continue;}
                auto l=replaceTypedSubtree(father.expression(),fr,typedSubexpression(mother.expression(),mr),problem_->limits());
                auto r=replaceTypedSubtree(mother.expression(),mr,typedSubexpression(father.expression(),fr),problem_->limits());
                if(l && r)try{*l=problem_->checkedExpression(*l);*r=problem_->checkedExpression(*r);}catch(const TypedLimitError&){l.reset();}
                if(!l || !r){countTrainingWork(statistics_.structuralRejections);continue;}
                left.replaceExpression(*l);right.replaceExpression(*r);
                if(left.text()!=father.text())countTrainingWork(statistics_.crossoverChanges);
                if(right.text()!=mother.text())countTrainingWork(statistics_.crossoverChanges);
                accepted=true;break;
            }
            if(!accepted)countTrainingWork(statistics_.crossoverFallbacks);
        }
        a=std::move(left);b=std::move(right);
    }
private:
    static std::size_t index(std::size_t n,std::mt19937& random){return std::uniform_int_distribution<std::size_t>(0,n-1)(random);}
    std::size_t shapeIndex(TypedShape s) const {
        const auto it=std::find(grammar_.shapes.begin(),grammar_.shapes.end(),s);
        if(it==grammar_.shapes.end())throw std::invalid_argument("shape absent from typed grammar vocabulary");
        return static_cast<std::size_t>(it-grammar_.shapes.begin());
    }
    bool isConstantShape(std::size_t s) const{return std::find(grammar_.constantShapes.begin(),grammar_.constantShapes.end(),grammar_.shapes[s])!=grammar_.constantShapes.end();}
    TypedValue constant(std::size_t s,std::mt19937* random) const {
        const auto shape=grammar_.shapes[s];std::vector<double> data(shape.coefficients());
        for(double& v:data)v=grammar_.constants[random?index(grammar_.constants.size(),*random):0];
        if(shape.kind()==TypedValueKind::Scalar)return TypedValue::scalar(data[0]);
        if(shape.kind()==TypedValueKind::Vector)return TypedValue::vector(data,problem_->limits().values);
        return TypedValue::matrix(shape.rows(),shape.columns(),data,problem_->limits().values);
    }
    std::optional<TypedExpression> compose(K op,const TypedExpression& a,const TypedExpression* b) const {
        try{return b?TypedExpression::binary(op,a,*b,problem_->limits()):TypedExpression::unary(op,a,problem_->limits());}
        catch(const std::invalid_argument&){return {};}
        catch(const TypedLimitError&){return {};}
    }
    std::vector<Production> makeProductions(std::size_t s,std::size_t depth,std::size_t budget) const {
        std::vector<Production> out;
        if(depth<=1)return out;
        for(auto op:grammar_.operators)for(std::size_t a=0;a<grammar_.shapes.size();++a) {
            const auto& left=witnesses_[depth-1][a];if(!left)continue;
            const auto arity=typedExpressionArity(op);const auto n=arity==1?std::size_t{1}:grammar_.shapes.size();
            for(std::size_t b=0;b<n;++b) {
                const auto& right=witnesses_[depth-1][b];if(arity==2 && !right)continue;
                const auto minimum=left->nodeCount()+(arity==2?right->nodeCount():0);
                if(minimum>=budget)continue;
                const auto output=genetik_typed_detail::operationShape(op,grammar_.shapes[a],arity==1?grammar_.shapes[a]:grammar_.shapes[b]);
                if(!output || *output!=grammar_.shapes[s])continue;
                const auto e=compose(op,*left,arity==1?nullptr:&*right);
                if(e && e->shape()==grammar_.shapes[s])out.push_back({op,a,b});
            }
        }
        return out;
    }
    TypedExpression randomExpression(TypedShape shape,std::size_t maximumDepth,std::mt19937& random) {
        const auto s=shapeIndex(shape),maxDepth=std::min(maximumDepth,witnesses_.size()-1);
        std::vector<std::size_t> depths;
        for(std::size_t d=1;d<=maxDepth;++d)if(witnesses_[d][s])depths.push_back(d);
        if(depths.empty())throw std::invalid_argument("typed subtree has no grammar witness");
        const auto depth=depths[index(depths.size(),random)];
        for(std::size_t attempt=0;attempt<grammar_.proposalLimit;++attempt) {
            countTrainingWork(statistics_.constructionProposals);
            struct Task{std::size_t shape,depth,budget;std::optional<K> emit;};
            std::vector<Task> tasks{{s,depth,problem_->limits().maxNodes,{}}};std::vector<TypedExpressionNode> nodes;
            while(!tasks.empty()) {
                const auto task=tasks.back();tasks.pop_back();
                if(task.emit){nodes.push_back(typedExpressionArity(*task.emit)==1?TypedExpressionNode::unary(*task.emit):TypedExpressionNode::binary(*task.emit));continue;}
                std::vector<std::size_t> variables;
                for(std::size_t v=0;v<problem_->schema().size();++v)if(problem_->schema()[v]==grammar_.shapes[task.shape])variables.push_back(v);
                const bool constants=isConstantShape(task.shape) && problem_->limits().maxConstantNodes>0 && grammar_.shapes[task.shape].coefficients()<=problem_->limits().maxConstantCoefficients;
                const bool terminal=constants || !variables.empty();
                std::vector<Production> options;
                if(task.depth>1)for(const auto& p:productionCache_[task.depth][task.shape]) {
                    const auto cost=witnesses_[task.depth-1][p.left]->nodeCount()+(typedExpressionArity(p.kind)==2?witnesses_[task.depth-1][p.right]->nodeCount():0);
                    if(cost<task.budget)options.push_back(p);
                }
                if(!options.empty() && (!terminal || std::bernoulli_distribution(grammar_.operationProbability)(random))) {
                    const auto p=options[index(options.size(),random)];const auto arity=typedExpressionArity(p.kind);
                    tasks.push_back({0,0,0,p.kind});
                    if(arity==1)tasks.push_back({p.left,task.depth-1,task.budget-1,{}});
                    else {
                        const auto minLeft=witnesses_[task.depth-1][p.left]->nodeCount(),minRight=witnesses_[task.depth-1][p.right]->nodeCount();
                        const auto leftBudget=minLeft+index(task.budget-minLeft-minRight,random);
                        tasks.push_back({p.right,task.depth-1,task.budget-1-leftBudget,{}});
                        tasks.push_back({p.left,task.depth-1,leftBudget,{}});
                    }
                } else if(!variables.empty() && (!constants || std::bernoulli_distribution(.5)(random)))nodes.push_back(TypedExpressionNode::variable(variables[index(variables.size(),random)]));
                else if(constants)nodes.push_back(TypedExpressionNode::constant(constant(task.shape,&random)));
                else throw std::logic_error("typed feasible production lost its witness");
            }
            try{
                auto e=TypedExpression(nodes,problem_->schema(),problem_->limits());
                if(shape==problem_->outputShape())e=problem_->checkedExpression(e);
                return e;
            }catch(const TypedLimitError&){countTrainingWork(statistics_.structuralRejections);}
        }
        // Explicit finite fallback; witness owns its data and was fully validated.
        countTrainingWork(statistics_.constructionFallbacks);
        return *witnesses_[depth][s];
    }
    void requireProblem(const TypedExpressionCandidate& c) const{if(c.problem()!=problem_)throw std::invalid_argument("typed variation problem differs");}
    std::shared_ptr<const TypedRegressionProblem> problem_;
    TypedGrammar grammar_;
    std::shared_ptr<TypedRegressionPreparation> preparation_;
    std::vector<std::vector<std::optional<TypedExpression>>> witnesses_;
    std::vector<std::vector<std::vector<Production>>> productionCache_;
    TypedVariationStatistics statistics_;
};
