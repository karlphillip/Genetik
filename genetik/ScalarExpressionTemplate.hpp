#pragma once
#include "ExpressionVariation.hpp"

// One explicitly selected subtree OCCURRENCE. No unresolved node enters Expression.
class ScalarExpressionTemplate final {
public:
    ScalarExpressionTemplate(const Expression& source,std::size_t subtreeRoot,
             ExpressionLimits substitutionLimits={9,4,2},ExpressionLimits modelLimits={13,6,2},
             std::size_t maxMaterializationBytes=1048576)
        : variables_(source.variableCount()),local_(substitutionLimits),model_(modelLimits),workspace_(maxMaterializationBytes) {
        (void)Expression::constant(0,variables_,local_);(void)Expression::constant(0,variables_,model_);
        if(!workspace_)throw std::invalid_argument("template materialization allowance must be positive");
        checkCapacity(source.nodeCount());
        if(source.nodeCount()>model_.maxNodes)throw std::length_error("template source model node limit");
        const auto checked=Expression(source.nodes(),variables_,model_);
        const auto [begin,end]=expressionSubtree(checked,subtreeRoot);
        const auto nodes=checked.nodes();
        prefix_.assign(nodes.begin(),nodes.begin()+begin);suffix_.assign(nodes.begin()+end,nodes.end());
        frozen_=prefix_.size()+suffix_.size();
        // Starts determine exactly how many ancestors surround the selected site.
        std::vector<std::size_t> starts;
        for(std::size_t i=0;i<nodes.size();++i) {
            const auto arity=expressionArity(nodes[i].kind());
            if(!arity)starts.push_back(i);
            else {
                for(std::size_t j=1;j<arity;++j)starts.pop_back();
                if(i>=end && starts.back()<=begin)++ancestors_;
            }
            if(i<begin || i>=end) {
                const auto cost=operationCost(nodes[i]);
                if(cost>model_.maxArithmeticOperations-frozenWork_)throw std::length_error("frozen work overflow");
                frozenWork_+=cost;
            }
        }
        if(frozen_>=model_.maxNodes || ancestors_>=model_.maxDepth)throw std::invalid_argument("no room for template substitution");
        effective_=local_;effective_.maxNodes=std::min(local_.maxNodes,model_.maxNodes-frozen_);
        effective_.maxDepth=std::min(local_.maxDepth,model_.maxDepth-ancestors_);
        effective_.maxAbsConstant=std::min(local_.maxAbsConstant,model_.maxAbsConstant);
        effective_.maxArithmeticOperations=std::min(local_.maxArithmeticOperations,model_.maxArithmeticOperations-frozenWork_);
        const auto slots=workspace_/slotBytes;
        if(slots<=frozen_)throw std::invalid_argument("no materialization workspace for substitution");
        effective_.maxNodes=std::min(effective_.maxNodes,slots-frozen_);
    }
    ScalarExpressionTemplate(const ScalarExpressionTemplate&)=default;
    ScalarExpressionTemplate& operator=(const ScalarExpressionTemplate&)=delete; // immutable scaffold value
    std::size_t variableCount() const noexcept{return variables_;}
    std::size_t frozenNodes() const noexcept{return frozen_;}
    std::size_t ancestorDepth() const noexcept{return ancestors_;}
    ExpressionLimits substitutionLimits() const noexcept{return local_;}
    ExpressionLimits modelLimits() const noexcept{return model_;}
    ExpressionLimits effectiveSubstitutionLimits() const noexcept{return effective_;}
    std::size_t maxMaterializationBytes() const noexcept{return workspace_;}
    Expression materialize(const Expression& substitution) const {
        if(substitution.variableCount()!=variables_)throw std::invalid_argument("template substitution schema differs");
        if(substitution.nodeCount()>local_.maxNodes)throw std::length_error("local substitution node limit");
        if(substitution.nodeCount()>model_.maxNodes-frozen_)throw std::length_error("complete model node limit");
        checkCapacity(frozen_+substitution.nodeCount());
        if(substitution.depth()>model_.maxDepth-ancestors_)throw std::length_error("complete model destination depth limit");
        const auto local=Expression(substitution.nodes(),variables_,local_);
        const auto inserted=local.nodes();std::vector<ExpressionNode> nodes;nodes.reserve(frozen_+inserted.size());
        nodes.insert(nodes.end(),prefix_.begin(),prefix_.end());nodes.insert(nodes.end(),inserted.begin(),inserted.end());nodes.insert(nodes.end(),suffix_.begin(),suffix_.end());
        return Expression(nodes,variables_,model_);
    }
    // Verifies every frozen field, and returns a detached, validated substitution.
    Expression substitution(const Expression& model) const {
        if(model.variableCount()!=variables_ || model.nodeCount()<=frozen_ || model.nodeCount()>model_.maxNodes)
            throw std::invalid_argument("model does not match template");
        checkCapacity(model.nodeCount());
        const auto nodes=model.nodes();const auto count=nodes.size()-frozen_;
        for(std::size_t i=0;i<prefix_.size();++i)if(!sameNode(nodes[i],prefix_[i]))throw std::invalid_argument("frozen template prefix changed");
        for(std::size_t i=0;i<suffix_.size();++i)if(!sameNode(nodes[prefix_.size()+count+i],suffix_[i]))throw std::invalid_argument("frozen template suffix changed");
        const auto result=Expression(std::span(nodes.data()+prefix_.size(),count),variables_,local_);
        // Ordinary full-model validation also enforces arithmetic and constant limits.
        (void)Expression(nodes,variables_,model_);
        return result;
    }
    std::vector<std::size_t> adjustableConstants(const Expression& model) const {
        const auto hole=substitution(model);const auto nodes=hole.nodes();std::vector<std::size_t> indices;
        for(std::size_t i=0;i<nodes.size();++i)if(nodes[i].kind()==ExpressionKind::Constant)indices.push_back(prefix_.size()+i);
        return indices;
    }
private:
    static constexpr std::size_t slotBytes=4*sizeof(ExpressionNode)+4*sizeof(std::size_t);
    void checkCapacity(std::size_t count) const {
        if(count>workspace_/slotBytes)throw std::length_error("template materialization workspace limit");
    }
    static std::size_t operationCost(const ExpressionNode& n) {
        if(!expressionArity(n.kind()))return 0;
        return expressionOperationCost(n.kind(),expressionPowerMetadata(n.kind())?n.exponent():0);
    }
    static bool sameNode(const ExpressionNode& a,const ExpressionNode& b) {
        if(a.kind()!=b.kind())return false;
        if(a.kind()==ExpressionKind::Variable)return a.variableIndex()==b.variableIndex();
        if(a.kind()==ExpressionKind::Constant)return std::bit_cast<std::uint64_t>(a.constantValue())==std::bit_cast<std::uint64_t>(b.constantValue());
        return !expressionPowerMetadata(a.kind()) || a.exponent()==b.exponent();
    }
    std::size_t variables_,frozen_=0,ancestors_=0,frozenWork_=0;
    ExpressionLimits local_,model_,effective_;
    std::size_t workspace_;
    std::vector<ExpressionNode> prefix_,suffix_;
};
