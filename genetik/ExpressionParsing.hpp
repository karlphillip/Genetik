#pragma once
#include "Expression.hpp"
#include "detail/ExpressionParser.hpp"

namespace genetik_parse_detail {
inline ExpressionKind scalarOperator(std::string_view name) {
    if(name=="+")return ExpressionKind::Add;
    if(name=="-")return ExpressionKind::Subtract;
    if(name=="*")return ExpressionKind::Multiply;
    if(name=="/")return ExpressionKind::Divide;
    if(name=="log")return ExpressionKind::Log;
    if(name=="sin")return ExpressionKind::Sin;
    if(name=="cos")return ExpressionKind::Cos;
    if(name=="exp")return ExpressionKind::Exp;
    if(name=="sqrt")return ExpressionKind::Sqrt;
    if(name=="powi")return ExpressionKind::Powi;
    throw std::logic_error("unrecognized internal parser operator");
}
class ScalarBuilder {
public:
    static constexpr bool typed=false;
    ScalarBuilder(std::string_view text,std::size_t variables,ExpressionLimits limits,Budget& budget)
        :variables_(variables),limits_(limits),budget_(budget) {
        if(!limits.maxNodes||!limits.maxDepth||!std::isfinite(limits.maxAbsConstant)||limits.maxAbsConstant<0)
            throw std::invalid_argument("invalid expression limits");
        if(variables>budget.limits.maxVariables)throw std::invalid_argument("parser variable schema limit");
        const auto n=capacity(text,limits.maxNodes,budget,sizeof(ExpressionNode)+sizeof(std::size_t));nodes_.reserve(n);depths_.reserve(n);
    }
    void literal(double v,std::size_t p) {
        budget_.constant(1,p);
        if(std::abs(v)>limits_.maxAbsConstant)fail(Category::Resource,p,"constant magnitude limit");
        append(ExpressionNode::constant(v),1,p);
    }
    void variable(std::size_t index,std::size_t p) {
        if(index>=variables_)fail(Category::UnknownSymbol,p,"variable outside declared schema");
        append(ExpressionNode::variable(index),1,p);
    }
    void operation(std::string_view name,unsigned exponent,std::size_t p) {
        const auto k=scalarOperator(name);const auto arity=expressionArity(k);
        auto depth=depths_.back();depths_.pop_back();
        if(arity==2){depth=std::max(depth,depths_.back());depths_.pop_back();}
        if(depth>=limits_.maxDepth)fail(Category::Resource,p,"expression depth limit");
        const std::size_t cost=k==ExpressionKind::Powi ? (exponent ? exponent-1:0):1;
        if(cost>limits_.maxArithmeticOperations-work_)fail(Category::Resource,p,"expression arithmetic work limit");
        work_+=cost;
        append(k==ExpressionKind::Powi ? ExpressionNode::power(exponent):arity==1 ? ExpressionNode::unary(k):ExpressionNode::binary(k),depth+1,p);
    }
    Expression finish() {return Expression(nodes_,variables_,limits_);}
    std::size_t aggregateSize(std::string_view,std::size_t,std::size_t,std::size_t p){fail(Category::UnknownSymbol,p,"aggregate literal in scalar language");}
    void aggregate(std::string_view,std::size_t,std::size_t,const std::vector<double>&,std::size_t p){fail(Category::UnknownSymbol,p,"aggregate literal in scalar language");}
private:
    void append(ExpressionNode n,std::size_t depth,std::size_t p) {
        if(nodes_.size()>=limits_.maxNodes)fail(Category::Resource,p,"expression node limit");
        nodes_.push_back(n);depths_.push_back(depth);
    }
    std::size_t variables_;ExpressionLimits limits_;Budget& budget_;
    std::vector<ExpressionNode> nodes_;std::vector<std::size_t> depths_;std::size_t work_=0;
};
}

// Return a new owned value. Text is borrowed only for this synchronous call.
// Malformed source -> ExpressionParseError; bad API limits -> invalid_argument;
// allocation failures propagate unchanged. No evaluation, RNG or loss work occurs.
inline Expression parseExpression(std::string_view text,std::size_t variableCount,
                                  ExpressionParseLimits parsing={},ExpressionLimits expression={}) {
    genetik_parse_detail::validate(parsing,text);
    genetik_parse_detail::Budget budget{parsing};
    genetik_parse_detail::ScalarBuilder builder(text,variableCount,expression,budget);
    genetik_parse_detail::Parser parser(text,builder,budget);parser.parse();return builder.finish();
}
