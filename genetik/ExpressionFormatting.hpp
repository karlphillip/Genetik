#pragma once
#include "Expression.hpp"
#include <functional>
#include <array>

enum class ExpressionTextFormat { Canonical,Unicode,LaTeX };
using ExpressionVariableFormatter=std::function<std::string(std::size_t,ExpressionTextFormat)>;
struct ExpressionFormatLimits {std::size_t maxOutputBytes=1024*1024,maxNodes=100000;};
// Synchronous, iterative, presentation only. No reordering, folding or tree edits.
inline std::string formatExpression(const Expression& e,ExpressionTextFormat format=ExpressionTextFormat::Unicode,
        const ExpressionVariableFormatter& variable={},ExpressionFormatLimits limits={}) {
    if(!limits.maxOutputBytes||!limits.maxNodes)throw std::invalid_argument("invalid expression format limits");
    if(e.nodeCount()>limits.maxNodes)throw std::length_error("expression format node limit");
    if(format==ExpressionTextFormat::Canonical){auto text=e.toString();if(text.size()>limits.maxOutputBytes)throw std::length_error("expression format output limit");return text;}
    if(format!=ExpressionTextFormat::Unicode && format!=ExpressionTextFormat::LaTeX)throw std::invalid_argument("unknown expression text format");
    const bool tex=format==ExpressionTextFormat::LaTeX;
    struct Item{std::string text;unsigned precedence;ExpressionKind kind;};std::vector<Item> stack;std::size_t payload=0;
    const auto wrap=[&](std::string t){return tex?"\\left("+t+"\\right)":"("+t+")";};
    const auto bounded=[&](const std::string& s){if(s.size()>limits.maxOutputBytes)throw std::length_error("expression format output limit");};
    for(const auto& n:e.nodes()) {
        std::string text;unsigned precedence=5;const auto k=n.kind();
        if(k==ExpressionKind::Variable){text=variable?wrap(variable(n.variableIndex(),format)):(tex?"x_{"+std::to_string(n.variableIndex())+"}":"x"+std::to_string(n.variableIndex()));}
        else if(k==ExpressionKind::Constant){char b[64];const auto r=std::to_chars(b,b+64,n.constantValue(),std::chars_format::general,std::numeric_limits<double>::max_digits10);if(r.ec!=std::errc{})throw std::runtime_error("constant formatting failed");text.assign(b,r.ptr);if(!tex && text.starts_with('-'))text="−"+text.substr(1);if(std::signbit(n.constantValue()))precedence=2;if(tex){const auto exp=text.find('e');if(exp!=std::string::npos)text=text.substr(0,exp)+"\\times10^{"+text.substr(exp+1)+"}";}}
        else {
            auto right=std::move(stack.back());stack.pop_back();payload-=right.text.size();
            if(expressionArity(k)==2){auto left=std::move(stack.back());stack.pop_back();payload-=left.text.size();precedence=(k==ExpressionKind::Add||k==ExpressionKind::Subtract)?1:2;
                if(tex && k==ExpressionKind::Divide){text="\\frac{"+left.text+"}{"+right.text+"}";precedence=4;}
                else {if(left.precedence<precedence || (precedence==2 && left.kind==ExpressionKind::Divide))left.text=wrap(std::move(left.text));if(right.precedence<=precedence || (precedence==1 && (right.text.starts_with("−")||right.text.starts_with("-"))))right.text=wrap(std::move(right.text));
                    const std::string op=k==ExpressionKind::Add?" + ":k==ExpressionKind::Subtract?(tex?" - ":" − "):k==ExpressionKind::Multiply?(tex?" \\cdot ":"·"):" / ";text=left.text+op+right.text;}
            }else if(k==ExpressionKind::Powi){precedence=3;auto base=right.precedence<=precedence?wrap(std::move(right.text)):std::move(right.text);if(tex)text=base+"^{"+std::to_string(n.exponent())+"}";else{constexpr std::array digits{"⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸"};text=base+digits[n.exponent()];}}
            else {precedence=4;
                if(k==ExpressionKind::Sqrt)text=tex?"\\sqrt{"+right.text+"}":"√"+wrap(std::move(right.text));
                else {const std::string name=k==ExpressionKind::Log?"ln":k==ExpressionKind::Sin?"sin":k==ExpressionKind::Cos?"cos":"exp";text=(tex?"\\":"")+name+wrap(std::move(right.text));}
            }
        }
        bounded(text);if(payload>limits.maxOutputBytes-text.size())throw std::length_error("expression format workspace limit");payload+=text.size();stack.push_back({std::move(text),precedence,k});
    }
    return std::move(stack.back().text);
}
