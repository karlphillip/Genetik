#pragma once
#include "TypedExpression.hpp"
#include "ExpressionParsing.hpp"

namespace genetik_parse_detail {
inline TypedExpressionKind typedOperator(std::string_view name) {
    using K=TypedExpressionKind;
    if(name=="+")return K::Add;
    if(name=="-")return K::Subtract;
    if(name=="*")return K::Multiply;
    if(name=="/")return K::Divide;
    if(name=="log")return K::Log;
    if(name=="sin")return K::Sin;
    if(name=="cos")return K::Cos;
    if(name=="exp")return K::Exp;
    if(name=="sqrt")return K::Sqrt;
    if(name=="powi")return K::Powi;
    if(name=="dot")return K::Dot;
    if(name=="matvec")return K::MatVec;
    if(name=="matmul")return K::MatMul;
    if(name=="transpose")return K::Transpose;
    throw std::logic_error("unrecognized internal typed parser operator");
}
class TypedBuilder {
    struct Info {TypedShape shape;std::size_t depth;};
public:
    static constexpr bool typed=true;
    TypedBuilder(std::string_view text,std::span<const TypedShape> schema,TypedExpressionLimits limits,Budget& budget)
        :schema_(schema),limits_(limits),budget_(budget) {
        genetik_typed_detail::validate(limits.values);
        if(!limits.maxNodes||!limits.maxDepth||!std::isfinite(limits.maxAbsConstant)||limits.maxAbsConstant<0)
            throw std::invalid_argument("invalid typed expression limits");
        if(schema.size()>limits.maxVariables || schema.size()>budget.limits.maxVariables)throw std::invalid_argument("parser variable schema limit");
        std::size_t count=0;
        for(auto shape:schema){genetik_typed_detail::validateShape(shape,limits.values);count=genetik_typed_detail::add(count,shape.coefficients(),"schema coefficient overflow");}
        if(count>limits.maxBindingCoefficients)throw std::invalid_argument("parser binding coefficient limit");
        const auto n=capacity(text,limits.maxNodes,budget,sizeof(TypedExpressionNode)+sizeof(Info));nodes_.reserve(n);info_.reserve(n);
    }
    void literal(double v,std::size_t p){budget_.constant(1,p);addConstant(TypedValue::scalar(v),p);}
    void variable(std::size_t index,std::size_t p) {
        if(index>=schema_.size())fail(Category::UnknownSymbol,p,"variable outside declared schema");
        append(TypedExpressionNode::variable(index),{schema_[index],1},p);
    }
    std::size_t aggregateSize(std::string_view name,std::size_t rows,std::size_t cols,std::size_t p) {
        if(nodes_.size()>=limits_.maxNodes)fail(Category::Resource,p,"typed node limit");
        if(!rows||!cols)fail(Category::Shape,p,"aggregate dimensions must be positive");
        if(rows>std::numeric_limits<std::size_t>::max()/cols)fail(Category::Resource,p,"dimension product overflow");
        const auto shape=name=="scalar" ? TypedShape::scalar():name=="vector" ? TypedShape::vector(rows):TypedShape::matrix(rows,cols);
        try {genetik_typed_detail::validateShape(shape,limits_.values);}
        catch(const TypedLimitError&){fail(Category::Resource,p,"aggregate dimension/coefficient limit");}
        if(constants_>=limits_.maxConstantNodes || shape.coefficients()>limits_.maxConstantCoefficients-coefficients_)
            fail(Category::Resource,p,"typed constant/coefficient limit");
        return shape.coefficients();
    }
    void aggregate(std::string_view name,std::size_t rows,std::size_t cols,const std::vector<double>& values,std::size_t p) {
        addConstant(name=="scalar" ? TypedValue::scalar(values[0]):name=="vector" ? TypedValue::vector(values,limits_.values):TypedValue::matrix(rows,cols,values,limits_.values),p);
    }
    void operation(std::string_view name,unsigned exponent,std::size_t p) {
        const auto k=typedOperator(name);const auto arity=typedExpressionArity(k);
        auto right=info_.back();info_.pop_back();auto left=right;
        if(arity==2){left=info_.back();info_.pop_back();}
        const auto shape=genetik_typed_detail::operationShape(k,left.shape,right.shape);
        if(!shape)fail(Category::Shape,p,"incompatible operand kinds or shapes");
        try {genetik_typed_detail::validateShape(*shape,limits_.values);}
        catch(const TypedLimitError&){fail(Category::Resource,p,"result shape limit");}
        const auto depth=std::max(left.depth,right.depth);
        if(depth>=limits_.maxDepth)fail(Category::Resource,p,"typed expression depth limit");
        append(k==TypedExpressionKind::Powi ? TypedExpressionNode::power(exponent):arity==1 ? TypedExpressionNode::unary(k):TypedExpressionNode::binary(k),{*shape,depth+1},p);
    }
    TypedExpression finish(std::size_t p) {
        try {return TypedExpression(nodes_,schema_,limits_);}
        catch(const TypedLimitError&){fail(Category::Resource,p,"typed execution plan limit");}
    }
private:
    void addConstant(const TypedValue& v,std::size_t p) {
        if(constants_>=limits_.maxConstantNodes || v.shape().coefficients()>limits_.maxConstantCoefficients-coefficients_)fail(Category::Resource,p,"typed constant/coefficient limit");
        for(double d:v.coefficients())if(std::abs(d)>limits_.maxAbsConstant)fail(Category::Resource,p,"constant magnitude limit");
        ++constants_;coefficients_+=v.shape().coefficients();append(TypedExpressionNode::constant(v),{v.shape(),1},p);
    }
    void append(TypedExpressionNode node,Info info,std::size_t p) {
        if(nodes_.size()>=limits_.maxNodes)fail(Category::Resource,p,"typed node limit");
        nodes_.push_back(std::move(node));info_.push_back(info);
    }
    std::span<const TypedShape> schema_;TypedExpressionLimits limits_;Budget& budget_;
    std::vector<TypedExpressionNode> nodes_;std::vector<Info> info_;
    std::size_t constants_=0,coefficients_=0;
};
}

// Schema is borrowed only during import; result owns its validated copy.
inline TypedExpression parseTypedExpression(std::string_view text,std::span<const TypedShape> schema,
        TypedShape expectedOutput,ExpressionParseLimits parsing={},TypedExpressionLimits expression={}) {
    genetik_parse_detail::validate(parsing,text);
    genetik_typed_detail::validateShape(expectedOutput,expression.values);
    genetik_parse_detail::Budget budget{parsing};genetik_parse_detail::TypedBuilder builder(text,schema,expression,budget);
    genetik_parse_detail::Parser parser(text,builder,budget);parser.parse();auto result=builder.finish(text.size());
    if(result.shape()!=expectedOutput)genetik_parse_detail::fail(ExpressionParseCategory::Shape,text.size(),"expression output differs from expected shape");
    return result;
}
