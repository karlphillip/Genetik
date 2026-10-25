#pragma once
#include "DiscreteExpression.hpp"
#include <charconv>
#include <string_view>

enum class DiscreteParseCategory { Syntax, UnknownSymbol, InvalidLiteral, Type, Resource };
class DiscreteParseError : public std::runtime_error {
public:
    DiscreteParseError(DiscreteParseCategory category,std::size_t offset,const char* message):std::runtime_error(std::string(message)+" at byte "+std::to_string(offset)),category_(category),offset_(offset){}
    DiscreteParseCategory category() const noexcept{return category_;}
    std::size_t offset() const noexcept{return offset_;}
private:DiscreteParseCategory category_;std::size_t offset_;
};
struct DiscreteParsingLimits {std::size_t maxSourceBytes=65536,maxTokenBytes=64,maxNesting=64,maxWorkspaceBytes=1024*1024;};
namespace genetik_discrete_detail {
class Parser {
public:
    Parser(std::string_view text,std::size_t dimensions,DiscreteExpressionLimits e,DiscreteParsingLimits p):text_(text),dimensions_(dimensions),e_(e),p_(p){
        validateLimits(e);if(!p.maxSourceBytes||!p.maxTokenBytes||!p.maxNesting||p.maxNesting>128||!p.maxWorkspaceBytes)throw std::invalid_argument("invalid discrete parsing limits (nesting 1..128)");
        if(text.size()>p.maxSourceBytes)error(DiscreteParseCategory::Resource,"source byte limit");
        if(dimensions>e.maxVariables)throw std::invalid_argument("discrete schema limit");
    }
    DiscreteExpression parse(){expression(0,0);space();if(pos_!=text_.size())error(DiscreteParseCategory::Syntax,"trailing input");
        try{return {nodes_,dimensions_,e_};}catch(const std::length_error&){error(DiscreteParseCategory::Resource,"expression resource limit");}catch(const std::invalid_argument&){error(DiscreteParseCategory::Type,"invalid discrete types, bindings or literal bounds");}}
private:
    [[noreturn]] void error(DiscreteParseCategory c,const char* what) const {throw DiscreteParseError(c,pos_,what);}
    void space(){while(pos_<text_.size()&&(text_[pos_]==' '||text_[pos_]=='\t'||text_[pos_]=='\n'||text_[pos_]=='\r'))++pos_;}
    bool take(std::string_view s){space();if(text_.substr(pos_,s.size())!=s)return false;pos_+=s.size();return true;}
    void need(std::string_view s){if(!take(s))error(DiscreteParseCategory::Syntax,"missing delimiter");}
    void push(DiscreteNode n){if(nodes_.size()==e_.maxNodes||nodes_.size()>=p_.maxWorkspaceBytes/(2*sizeof(DiscreteNode)))error(DiscreteParseCategory::Resource,"parser node/workspace limit");nodes_.push_back(n);}
    static bool digit(char c){return c>='0'&&c<='9';}
    void primary(std::size_t nesting){space();if(nesting>=p_.maxNesting)error(DiscreteParseCategory::Resource,"syntactic nesting limit");
        if(pos_==text_.size())error(DiscreteParseCategory::Syntax,"expected expression");
        if(take("(")){expression(0,nesting+1);need(")");return;}
        space();const auto start=pos_;
        if(digit(text_[pos_])||(text_[pos_]=='-'&&pos_+1<text_.size()&&digit(text_[pos_+1]))){
            if(text_[pos_]=='-')++pos_;
            while(pos_<text_.size()&&digit(text_[pos_]))++pos_;
            if(pos_-start>p_.maxTokenBytes)error(DiscreteParseCategory::Resource,"integer token limit");
            std::int64_t n=0;auto result=std::from_chars(text_.data()+start,text_.data()+pos_,n);
            if(result.ec!=std::errc{}||result.ptr!=text_.data()+pos_)error(DiscreteParseCategory::InvalidLiteral,"integer literal outside Int64");
            if(pos_<text_.size()&&(text_[pos_]=='.'||text_[pos_]=='e'||text_[pos_]=='E'))error(DiscreteParseCategory::InvalidLiteral,"integer decimal syntax only");
            push(DiscreteNode::integer(n));return;
        }
        if(take("-")){primary(nesting+1);push(DiscreteNode::operation(DiscreteKind::Negate));return;}
        space();while(pos_<text_.size()&&((text_[pos_]>='a'&&text_[pos_]<='z')||text_[pos_]=='_'||digit(text_[pos_])))++pos_;
        if(pos_-start>p_.maxTokenBytes)error(DiscreteParseCategory::Resource,"symbol token limit");
        auto name=text_.substr(start,pos_-start);
        if(name=="true"||name=="false"){push(DiscreteNode::boolean(name=="true"));return;}
        if(name.size()>1&&name[0]=='x'){
            std::size_t index=0;const auto result=std::from_chars(name.data()+1,name.data()+name.size(),index);
            if(result.ec!=std::errc{}||result.ptr!=name.data()+name.size()||index>=dimensions_)error(DiscreteParseCategory::UnknownSymbol,"unknown input variable");
            push(DiscreteNode::variable(index));return;
        }
        if(name.size()>1&&name[0]=='k'){
            std::size_t id=0;auto parsed=std::from_chars(name.data()+1,name.data()+name.size(),id);
            if(parsed.ec!=std::errc{}||parsed.ptr!=name.data()+name.size()||std::find(active_.begin(),active_.end(),id)==active_.end())error(DiscreteParseCategory::UnknownSymbol,"unbound iterator");
            push(DiscreteNode::iterator(id));return;
        }
        if(name=="sum"||name=="product"||name=="first"){
            need("(");space();const auto begin=pos_;while(pos_<text_.size()&&((text_[pos_]>='a'&&text_[pos_]<='z')||text_[pos_]=='_'||digit(text_[pos_])))++pos_;
            const auto binder=text_.substr(begin,pos_-begin);std::size_t id=0;
            if(binder.size()>p_.maxTokenBytes)error(DiscreteParseCategory::Resource,"binder token limit");
            if(binder.size()<2||binder[0]!='k')error(DiscreteParseCategory::Syntax,"binder must be k followed by an integer identity");
            auto parsed=std::from_chars(binder.data()+1,binder.data()+binder.size(),id);
            if(parsed.ec!=std::errc{}||parsed.ptr!=binder.data()+binder.size()||std::find(declared_.begin(),declared_.end(),id)!=declared_.end())error(DiscreteParseCategory::Syntax,"invalid or duplicate binder identity");
            if(declared_.size()==e_.maxBinders)error(DiscreteParseCategory::Resource,"binder count limit");
            declared_.push_back(id);
            need(",");expression(0,nesting+1);need(",");expression(0,nesting+1);need(",");active_.push_back(id);expression(0,nesting+1);active_.pop_back();need(")");
            push(DiscreteNode::iteration(name=="sum"?DiscreteKind::Sum:name=="product"?DiscreteKind::Product:DiscreteKind::First,id));return;
        }
        for(auto d:integerOperators)if(name==d.name){need("(");for(unsigned j=0;j<d.arity;++j){if(j)need(",");expression(0,nesting+1);}need(")");for(unsigned raw=18;raw<=24;++raw){auto k=static_cast<DiscreteKind>(raw);if(integerOperation(k)==d.operation){push(DiscreteNode::operation(k));return;}}}
        if(name=="if"||name=="neg"){
            need("(");expression(0,nesting+1);
            if(name=="if"){need(",");expression(0,nesting+1);need(",");expression(0,nesting+1);}
            need(")");push(DiscreteNode::operation(name=="if"?DiscreteKind::IfThenElse:DiscreteKind::Negate));return;
        }
        error(name.empty()?DiscreteParseCategory::Syntax:DiscreteParseCategory::UnknownSymbol,"expected integer, variable or supported function");
    }
    void expression(unsigned minimum,std::size_t nesting){
        if(nesting>=p_.maxNesting)error(DiscreteParseCategory::Resource,"syntactic nesting limit");
        primary(nesting);for(;;){space();DiscreteKind k=DiscreteKind::Add;unsigned precedence=0;std::size_t length=0;
            const auto tail=text_.substr(pos_);
            if(tail.starts_with("==")){k=DiscreteKind::Equal;precedence=1;length=2;}
            else if(tail.starts_with("!=")){k=DiscreteKind::NotEqual;precedence=1;length=2;}
            else if(tail.starts_with("<=")){k=DiscreteKind::LessEqual;precedence=1;length=2;}
            else if(tail.starts_with(">=")){k=DiscreteKind::GreaterEqual;precedence=1;length=2;}
            else if(!tail.empty())switch(tail[0]){
                case '<':k=DiscreteKind::LessThan;precedence=1;length=1;break;case '>':k=DiscreteKind::GreaterThan;precedence=1;length=1;break;
                case '+':k=DiscreteKind::Add;precedence=2;length=1;break;case '-':k=DiscreteKind::Subtract;precedence=2;length=1;break;
                case '*':k=DiscreteKind::Multiply;precedence=3;length=1;break;case '%':k=DiscreteKind::Modulo;precedence=3;length=1;break;
            }
            if(!length||precedence<minimum)return;
            pos_+=length;expression(precedence+1,nesting+1);push(DiscreteNode::operation(k));
        }
    }
    std::string_view text_;std::size_t dimensions_,pos_=0;DiscreteExpressionLimits e_;DiscreteParsingLimits p_;std::vector<DiscreteNode> nodes_;std::vector<std::size_t> active_,declared_;
};
}
inline DiscreteExpression parseDiscreteExpression(std::string_view text,std::size_t dimensions,DiscreteExpressionLimits e={},DiscreteParsingLimits p={}) {
    return genetik_discrete_detail::Parser(text,dimensions,e,p).parse();
}
