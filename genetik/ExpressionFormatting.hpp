#pragma once
#include "Expression.hpp"
#include <functional>
#include <array>

enum class ExpressionTextFormat { Canonical,Unicode,LaTeX };
using ExpressionVariableFormatter=std::function<std::string(std::size_t,ExpressionTextFormat)>;
struct ExpressionFormatOptions {unsigned significantDigits=std::numeric_limits<double>::max_digits10;};
struct ExpressionFormatLimits {std::size_t maxOutputBytes=1024*1024,maxNodes=100000;};
// Synchronous, iterative, presentation only. No reordering, folding or tree edits.
inline std::string formatExpression(const Expression& e,ExpressionTextFormat format=ExpressionTextFormat::Unicode,
        const ExpressionVariableFormatter& variable={},ExpressionFormatLimits limits={},ExpressionFormatOptions options={}) {
    if(!options.significantDigits || options.significantDigits>std::numeric_limits<double>::max_digits10)throw std::invalid_argument("formula precision must be1..17 significant digits");
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
        else if(k==ExpressionKind::Constant){char b[64];const auto r=std::to_chars(b,b+64,n.constantValue(),std::chars_format::general,static_cast<int>(options.significantDigits));if(r.ec!=std::errc{})throw std::runtime_error("constant formatting failed");text.assign(b,r.ptr);if(!tex && text.starts_with('-'))text="−"+text.substr(1);if(std::signbit(n.constantValue()))precedence=2;if(tex){const auto exp=text.find('e');if(exp!=std::string::npos)text=text.substr(0,exp)+"\\times10^{"+text.substr(exp+1)+"}";}}
        else {
            auto right=std::move(stack.back());stack.pop_back();payload-=right.text.size();
            if(k>=ExpressionKind::NegativePowi){
                precedence=4;std::vector<Item> args;args.push_back(std::move(right));
                for(std::size_t j=1;j<expressionArity(k);++j){auto item=std::move(stack.back());stack.pop_back();payload-=item.text.size();args.push_back(std::move(item));}std::reverse(args.begin(),args.end());
                if(k==ExpressionKind::NegativePowi){precedence=3;constexpr std::array digits{"⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸"};text=wrap(args[0].text)+(tex?"^{-"+std::to_string(n.exponent())+"}":"⁻"+std::string(digits[n.exponent()]));}
                else if(k==ExpressionKind::Cbrt)text=tex?"\\sqrt[3]{"+args[0].text+"}":"∛"+wrap(args[0].text);
                else if(k==ExpressionKind::Abs)text=tex?"\\left|"+args[0].text+"\\right|":"|"+args[0].text+"|";
                else if(k==ExpressionKind::Pow){precedence=3;text=wrap(args[0].text)+(tex?"^{"+args[1].text+"}":"^"+wrap(args[1].text));}
                else{auto name=std::string(scalarOperatorDescription(k).name);text=tex?(k==ExpressionKind::Log2?"\\log_{2}":k==ExpressionKind::Log10?"\\log_{10}":"\\operatorname{"+name+"}"):name;text+=tex?"\\left(":"(";for(std::size_t j=0;j<args.size();++j){if(j)text+=", ";text+=args[j].text;}text+=tex?"\\right)":")";}
            }
            else if(expressionArity(k)==2){auto left=std::move(stack.back());stack.pop_back();payload-=left.text.size();precedence=(k==ExpressionKind::Add||k==ExpressionKind::Subtract)?1:2;
                if(tex && k==ExpressionKind::Divide){text="\\frac{"+left.text+"}{"+right.text+"}";precedence=4;}
                else {if(left.precedence<precedence || (precedence==2 && left.kind==ExpressionKind::Divide))left.text=wrap(std::move(left.text));if(right.precedence<=precedence || (precedence==1 && (right.text.starts_with("−")||right.text.starts_with("-"))))right.text=wrap(std::move(right.text));
                    const std::string op=k==ExpressionKind::Add?" + ":k==ExpressionKind::Subtract?(tex?" - ":" − "):k==ExpressionKind::Multiply?(tex?" \\cdot ":"·"):" / ";text=left.text+op+right.text;}
            }else if(k==ExpressionKind::Powi){precedence=3;auto base=right.precedence<=precedence?wrap(std::move(right.text)):std::move(right.text);if(tex)text=base+"^{"+std::to_string(n.exponent())+"}";else{constexpr std::array digits{"⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸"};text=base+digits[n.exponent()];}}
            else {precedence=4;
                if(k==ExpressionKind::Sqrt)text=tex?"\\sqrt{"+right.text+"}":"√"+wrap(std::move(right.text));
                else {const std::string name=k==ExpressionKind::Log?"ln":k==ExpressionKind::Sin?"sin":k==ExpressionKind::Cos?"cos":"exp";text=(tex?"\\":"")+name+((right.kind==ExpressionKind::Variable&&variable)?std::move(right.text):wrap(std::move(right.text)));}
            }
        }
        bounded(text);if(payload>limits.maxOutputBytes-text.size())throw std::length_error("expression format workspace limit");payload+=text.size();stack.push_back({std::move(text),precedence,k});
    }
    return std::move(stack.back().text);
}
