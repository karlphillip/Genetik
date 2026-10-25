#pragma once
#include "DiscreteExpression.hpp"
#include <functional>

enum class DiscreteFormat { Canonical, Unicode, LaTeX };
inline std::string formatDiscreteExpression(const DiscreteExpression& expression,DiscreteFormat format=DiscreteFormat::Unicode,
        const std::function<std::string(std::size_t)>& variable={},std::size_t maxBytes=1024*1024) {
    (void)expression.type();
    if(format==DiscreteFormat::Canonical)return expression.toString(maxBytes);
    if(format!=DiscreteFormat::Unicode&&format!=DiscreteFormat::LaTeX)throw std::invalid_argument("unknown discrete format");
    const bool tex=format==DiscreteFormat::LaTeX;std::vector<std::string> stack;std::size_t workspace=0;
    for(const auto& n:expression.nodes()){
        const auto arity=discreteArity(n.kind());std::string value;
        if(!arity){
            if(n.kind()==DiscreteKind::Variable)value=variable?variable(n.variable()):"x"+std::to_string(n.variable());
            else if(n.kind()==DiscreteKind::Iterator)value="k"+std::to_string(n.binder());
            else if(n.kind()==DiscreteKind::Integer)value=std::to_string(n.integer());
            else value=n.boolean()?(tex?"\\mathrm{true}":"true"):(tex?"\\mathrm{false}":"false");
        }else {
            const auto start=stack.size()-arity;
            const auto join=[&](std::initializer_list<std::string_view> parts){std::size_t size=0;for(auto part:parts){if(part.size()>maxBytes-size)throw std::length_error("discrete formatting bound");size+=part.size();}std::string text;text.reserve(size);for(auto part:parts)text.append(part);return text;};
            const auto& a=stack[start];
            if(arity==1)value=tex?join({"-\\left(",a,"\\right)"}):join({"−(",a,")"});
            else if(discreteIteration(n.kind())){const auto& b=stack[start+1];const auto& body=stack[start+2];const auto binder="k"+std::to_string(n.binder());
                value=tex?join({n.kind()==DiscreteKind::Sum?"\\sum_{":"\\prod_{",binder,"=",a,"}^{",b,"}{",body,"}"}):join({n.kind()==DiscreteKind::Sum?"Σ[":"Π[",binder,"=",a,"..",b,"] ",body});}
            else if(arity==3){const auto& b=stack[start+1];const auto& c=stack[start+2];value=tex?join({"\\begin{cases}",b," & \\text{if } ",a,"\\\\",c," & \\text{otherwise}\\end{cases}"}):join({"(if ",a," then ",b," else ",c,")"});}
            else {std::string op=discreteSymbol(n.kind());
                if(n.kind()==DiscreteKind::Multiply)op=tex?"\\cdot":"·";
                if(n.kind()==DiscreteKind::Modulo)op=tex?"\\bmod":"mod";
                if(n.kind()==DiscreteKind::Equal)op="=";
                if(n.kind()==DiscreteKind::NotEqual)op=tex?"\\ne":"≠";
                if(n.kind()==DiscreteKind::LessEqual)op=tex?"\\le":"≤";
                if(n.kind()==DiscreteKind::GreaterEqual)op=tex?"\\ge":"≥";
                value=join({tex?"\\left(":"(",a," ",op," ",stack[start+1],tex?"\\right)":")"});}
            for(std::size_t i=start;i<stack.size();++i)workspace-=stack[i].size();
            stack.resize(start);
        }
        if(value.size()>maxBytes-workspace)throw std::length_error("discrete formatting workspace limit");
        workspace+=value.size();stack.push_back(std::move(value));
    }
    return stack.back();
}
