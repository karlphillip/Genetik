#pragma once
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Independent text limits; expression/evaluation limits retain their existing units.
struct ExpressionParseLimits {
    std::size_t maxSourceBytes=1048576, maxTokens=65536, maxTokenBytes=256;
    std::size_t maxNesting=64, maxConstantNodes=1024, maxConstantCoefficients=65536;
    std::size_t maxVariables=64, maxWorkspaceBytes=8388608;
};
enum class ExpressionParseCategory { Syntax, UnknownSymbol, Arity, Literal, Shape, Resource };
class ExpressionParseError : public std::invalid_argument {
public:
    ExpressionParseError(ExpressionParseCategory category, std::size_t offset, const char* message)
        : std::invalid_argument("expression byte "+std::to_string(offset)+": "+message), category_(category),offset_(offset) {}
    ExpressionParseCategory category() const noexcept { return category_; }
    std::size_t offset() const noexcept { return offset_; }
private:
    ExpressionParseCategory category_;
    std::size_t offset_;
};
namespace genetik_parse_detail {
using Category=ExpressionParseCategory;
[[noreturn]] inline void fail(Category c,std::size_t p,const char* message) { throw ExpressionParseError(c,p,message); }
inline bool digit(char c) {return c>='0' && c<='9';}
inline bool alpha(char c) {return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || c=='_';}
inline bool space(char c) {return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\v'||c=='\f';}
inline void validate(ExpressionParseLimits l,std::string_view text) {
    if(!l.maxSourceBytes || !l.maxTokens || !l.maxTokenBytes || !l.maxNesting || l.maxNesting>128 || !l.maxWorkspaceBytes)
        throw std::invalid_argument("parser counts must be positive; nesting must be in [1,128]");
    if(text.size()>l.maxSourceBytes)fail(Category::Resource,l.maxSourceBytes,"source byte limit");
}
struct Budget {
    ExpressionParseLimits limits;
    std::size_t workspace=0, constants=0, coefficients=0;
    void charge(std::size_t n,std::size_t bytes,std::size_t p) {
        if(n>(limits.maxWorkspaceBytes-workspace)/bytes)fail(Category::Resource,p,"parser workspace limit");
        workspace+=n*bytes;
    }
    void constant(std::size_t n,std::size_t p) {
        if(constants>=limits.maxConstantNodes || n>limits.maxConstantCoefficients-coefficients)
            fail(Category::Resource,p,"parser constant/coefficient limit");
        ++constants;coefficients+=n;
        // Covers coefficient input, TypedValue and detached node copies at peak.
        charge(n,4*sizeof(double),p);
    }
};
enum class TokenKind { End, Number, Name, Symbol };
struct Token {TokenKind kind; std::string_view text;std::size_t offset;};
class Lexer {
public:
    Lexer(std::string_view text,ExpressionParseLimits limits):text_(text),limits_(limits){next();}
    const Token& token() const {return token_;}
    void next() {
        while(pos_<text_.size() && space(text_[pos_]))++pos_;
        const auto start=pos_;
        if(pos_==text_.size()){token_={TokenKind::End,{},pos_};return;}
        if(++tokens_>limits_.maxTokens)fail(Category::Resource,start,"token count limit");
        const char first=text_[pos_++];auto kind=TokenKind::Symbol;
        if(digit(first)||first=='.') {
            kind=TokenKind::Number;
            while(pos_<text_.size()) {
                const char c=text_[pos_];
                if(digit(c)||alpha(c)||c=='.')++pos_;
                else if((c=='+'||c=='-') && (text_[pos_-1]=='e'||text_[pos_-1]=='E'))++pos_;
                else break;
                if(pos_-start>limits_.maxTokenBytes)fail(Category::Resource,start,"numeric token byte limit");
            }
        } else if(alpha(first)) {
            kind=TokenKind::Name;
            while(pos_<text_.size() && (alpha(text_[pos_])||digit(text_[pos_]))) {
                ++pos_;if(pos_-start>limits_.maxTokenBytes)fail(Category::Resource,start,"identifier byte limit");
            }
        }
        token_={kind,text_.substr(start,pos_-start),start};
    }
private:
    std::string_view text_; ExpressionParseLimits limits_; std::size_t pos_=0,tokens_=0;
    Token token_{};
};
// Every recursion passes a nesting guard; infix chains do not recurse with length.
// At most128 nested primary/literal frames, independent of semantic depth.
template<class Builder> class Parser {
public:
    Parser(std::string_view text,Builder& builder,Budget& budget):lex_(text,budget.limits),b_(builder),budget_(budget) {}
    void parse() {expression();if(tok().kind!=TokenKind::End)fail(Category::Syntax,tok().offset,"trailing input");}
private:
    struct Nest {
        Parser& p;
        explicit Nest(Parser& parser):p(parser) {if(p.nesting_>=p.budget_.limits.maxNesting)fail(Category::Resource,p.tok().offset,"syntactic nesting limit");++p.nesting_;}
        ~Nest(){--p.nesting_;}
    };
    const Token& tok() const {return lex_.token();}
    bool is(std::string_view s) const {return tok().text==s;}
    void expect(std::string_view s,Category c=Category::Syntax) {if(!is(s))fail(c,tok().offset,"missing delimiter or incorrect arity");lex_.next();}
    std::size_t integer() {
        const auto t=tok();std::size_t v=0;
        if(t.kind!=TokenKind::Number || t.text.empty())fail(Category::Literal,t.offset,"expected unsigned decimal integer");
        for(char c:t.text)if(!digit(c))fail(Category::Literal,t.offset,"expected unsigned decimal integer");
        const auto r=std::from_chars(t.text.data(),t.text.data()+t.text.size(),v);
        if(r.ec!=std::errc{})fail(Category::Resource,t.offset,"integer conversion range");
        lex_.next();return v;
    }
    double number() {
        if(is("(")){Nest n(*this);lex_.next();const auto v=number();expect(")");return v;}
        bool negative=false;
        if(is("-")||is("+")){negative=is("-");lex_.next();}
        const auto t=tok();
        if(t.kind!=TokenKind::Number)fail(Category::Literal,t.offset,"signs are supported only on numeric literals");
        double v=0;const auto r=std::from_chars(t.text.data(),t.text.data()+t.text.size(),v,std::chars_format::general);
        if(r.ec!=std::errc{} || r.ptr!=t.text.data()+t.text.size() || !std::isfinite(v))fail(Category::Literal,t.offset,"invalid or out-of-range finite decimal");
        lex_.next();return negative ? -v:v;
    }
    void expression() {
        term();while(is("+")||is("-")){const auto t=tok();lex_.next();term();b_.operation(t.text,0,t.offset);}
    }
    void term() {
        primary();while(is("*")||is("/")){const auto t=tok();lex_.next();primary();b_.operation(t.text,0,t.offset);}
    }
    void primary() {
        const auto t=tok();
        if(t.kind==TokenKind::Number || is("-")||is("+")){b_.literal(number(),t.offset);return;}
        if(is("(")){Nest n(*this);lex_.next();expression();expect(")");return;}
        if(t.kind!=TokenKind::Name)fail(Category::Syntax,t.offset,"expected expression");
        lex_.next();
        if(t.text=="nan"||t.text=="inf"||t.text=="infinity")fail(Category::Literal,t.offset,"nonfinite literal is unsupported");
        if(t.text.size()>1 && t.text[0]=='x' && digit(t.text[1])) {
            std::size_t index=0;const auto s=t.text.substr(1);const auto r=std::from_chars(s.data(),s.data()+s.size(),index);
            if(r.ec!=std::errc{} || r.ptr!=s.data()+s.size() || (s.size()>1&&s[0]=='0'))fail(Category::UnknownSymbol,t.offset,"invalid canonical variable");
            b_.variable(index,t.offset);return;
        }
        const bool aggregate=t.text=="scalar"||t.text=="vector"||t.text=="matrix";
        if(aggregate && Builder::typed) {
            Nest n(*this);std::size_t rows=1,cols=1;
            if(t.text!="scalar") {expect("[");rows=integer();if(t.text=="matrix"){expect(",");cols=integer();}expect("]");}
            const auto count=b_.aggregateSize(t.text,rows,cols,t.offset);
            budget_.constant(count,t.offset);expect("(");std::vector<double> values;values.reserve(count);
            for(std::size_t i=0;i<count;++i){if(i)expect(",",Category::Arity);values.push_back(number());}
            expect(")",Category::Arity);b_.aggregate(t.text,rows,cols,values,t.offset);return;
        }
        const bool binary=t.text=="dot"||t.text=="matvec"||t.text=="matmul";
        const bool unary=t.text=="log"||t.text=="sin"||t.text=="cos"||t.text=="exp"||t.text=="sqrt"||t.text=="powi";
        if(!unary && !(Builder::typed && (binary||t.text=="transpose")))fail(Category::UnknownSymbol,t.offset,"unknown function or variable");
        Nest n(*this);expect("(");expression();unsigned exponent=0;
        if(binary){expect(",",Category::Arity);expression();}
        if(t.text=="powi"){expect(",",Category::Arity);const auto e=integer();if(e>8)fail(Category::Literal,t.offset,"powi exponent must be in [0,8]");exponent=static_cast<unsigned>(e);}
        expect(")",Category::Arity);b_.operation(t.text,exponent,t.offset);
    }
    Lexer lex_;Builder& b_;Budget& budget_;std::size_t nesting_=0;
};
inline std::size_t capacity(std::string_view text,std::size_t nodes,Budget& budget,std::size_t itemBytes) {
    const auto n=std::min(text.size(),nodes);
    budget.charge(n,itemBytes,0);
    budget.charge(budget.limits.maxNesting,1024,0); // conservative bounded call-frame allowance
    return n;
}
}
