#include <ExpressionParsing.hpp>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
struct Spec {
    std::string reference;
    std::size_t variables=0, maxNodes=0, maxDepth=0, maxOperations=0;
    double maxAbsConstant=0, fitLower=0, fitUpper=0;
    bool powerExponentsAreConstants=true;
    std::set<std::string> operators;
    std::set<unsigned> integerExponents{0,1,2,3,4,5,6,7,8}, negativeExponents{1,2,3,4,5,6,7,8};
    std::string data;
};
std::size_t countArg(const char* s) {
    std::size_t used=0; const auto v=std::stoull(s,&used);
    if(used!=std::string(s).size()) throw std::invalid_argument("invalid unsigned count");
    return static_cast<std::size_t>(v);
}
double realArg(const char* s) {
    std::size_t used=0; const auto v=std::stod(s,&used);
    if(used!=std::string(s).size()||!std::isfinite(v)) throw std::invalid_argument("expected finite number");
    return v;
}
Spec arguments(int argc,char** argv) {
    Spec s;
    for(int i=1;i<argc;++i) {
        const std::string k=argv[i];
        if(i+1>=argc) throw std::invalid_argument("missing value for "+k);
        const char* v=argv[++i];
        if(k=="--reference")s.reference=v;
        else if(k=="--variables")s.variables=countArg(v);
        else if(k=="--operators") {std::stringstream in(v);std::string op;while(std::getline(in,op,',')){if(op.empty())throw std::invalid_argument("empty operator");if(!s.operators.insert(op).second)throw std::invalid_argument("duplicate grammar operator: "+op);}}
        else if(k=="--integer-exponents"||k=="--negative-exponents") {
            std::stringstream in(v);std::string item;auto& out=k=="--integer-exponents"?s.integerExponents:s.negativeExponents;out.clear();
            while(std::getline(in,item,',')){const auto x=countArg(item.c_str());if(x>8)throw std::invalid_argument("integer power exponent must be within 0..8");out.insert(static_cast<unsigned>(x));}
        }
        else if(k=="--max-nodes")s.maxNodes=countArg(v);
        else if(k=="--max-depth")s.maxDepth=countArg(v);
        else if(k=="--max-operations")s.maxOperations=countArg(v);
        else if(k=="--max-abs-constant")s.maxAbsConstant=realArg(v);
        else if(k=="--fit-lower")s.fitLower=realArg(v);
        else if(k=="--fit-upper")s.fitUpper=realArg(v);
        else if(k=="--power-exponents-are-constants")s.powerExponentsAreConstants=std::string(v)=="true";
        else if(k=="--data")s.data=v;
        else throw std::invalid_argument("unknown option "+k);
    }
    return s;
}
struct Audit {Expression expression;std::size_t constants=0;std::set<std::string> operators;std::size_t rows=0;};
std::string topology(const Expression& e) {
    std::ostringstream o;
    for(const auto& n:e.nodes()) {
        if(n.kind()==ExpressionKind::Constant)o<<"C;";
        else if(n.kind()==ExpressionKind::Variable)o<<"V"<<n.variableIndex()<<';';
        else if(expressionPowerMetadata(n.kind()))o<<"O"<<static_cast<unsigned>(n.kind())<<':'<<n.exponent()<<';';
        else o<<"O"<<static_cast<unsigned>(n.kind())<<';';
    }
    return o.str();
}
Audit validate(const Spec& s) {
    if(!s.variables||!s.maxNodes||!s.maxDepth||!s.maxOperations)
        throw std::invalid_argument("variable and resource limits must be positive");
    if(!std::isfinite(s.maxAbsConstant)||s.maxAbsConstant<0||
       !std::isfinite(s.fitLower)||!std::isfinite(s.fitUpper)||s.fitLower>s.fitUpper)
        throw std::invalid_argument("invalid constant or parameter bounds");
    if(s.reference.empty()||s.operators.empty())throw std::invalid_argument("reference and operator vocabulary are required");
    for(const auto& name:s.operators) {
        const auto kind=name=="npowi"?std::optional<ExpressionKind>{ExpressionKind::NegativePowi}:scalarOperatorFromName(name);
        if(!kind || expressionArity(*kind)==0)
            throw std::invalid_argument("unsupported grammar operator: "+name);
    }
    ExpressionLimits limits{s.maxNodes,s.maxDepth,s.maxAbsConstant,s.maxOperations};
    auto e=parseExpression(s.reference,s.variables,{},limits);
    Audit a{e,0,{},0};
    for(const auto& n:e.nodes()) {
        if(n.kind()==ExpressionKind::Constant) {
            ++a.constants;
            const auto v=n.constantValue();
            if(v<s.fitLower||v>s.fitUpper)throw std::invalid_argument("reference constant outside fitting parameter bounds");
        } else if(n.kind()==ExpressionKind::Variable) {
            if(n.variableIndex()>=s.variables)throw std::invalid_argument("reference variable outside declared dimensions");
        } else {
            const std::string op(scalarOperatorDescription(n.kind()).name);
            a.operators.insert(op);
            if(!s.operators.count(op))throw std::invalid_argument("unsupported operator in reference: "+op);
            if(expressionPowerMetadata(n.kind())) {
                const auto required=n.kind()==ExpressionKind::Powi?"powi":"npowi";
                if(!s.operators.count(required))throw std::invalid_argument(std::string("power metadata not enabled: ")+required);
                const auto& allowed=n.kind()==ExpressionKind::Powi?s.integerExponents:s.negativeExponents;
                if(!allowed.count(static_cast<unsigned>(n.exponent())))throw std::invalid_argument(std::string("power exponent is outside declared grammar: ")+required);
            }
        }
    }
    if(s.powerExponentsAreConstants&&s.operators.count("pow")) {
        const auto nodes=e.nodes();
        for(std::size_t i=0;i<nodes.size();++i)if(nodes[i].kind()==ExpressionKind::Pow) {
            if(i==0||nodes[i-1].kind()!=ExpressionKind::Constant)
                throw std::invalid_argument("real-power exponent must be a constant slot under the declared grammar");
        }
    }
    if(!s.data.empty()) {
        std::ifstream in(s.data); if(!in)throw std::runtime_error("cannot open observation file");
        std::string line; if(!std::getline(in,line))throw std::invalid_argument("observation file is empty");
        std::stringstream header(line);std::string column;std::size_t columnIndex=0;
        while(std::getline(header,column,',')) {
            const auto expected=columnIndex<s.variables?"x"+std::to_string(columnIndex):"y";
            if(column!=expected)throw std::invalid_argument("observation header does not match declared variable schema");
            ++columnIndex;
        }
        if(columnIndex!=s.variables+1)throw std::invalid_argument("observation header dimension does not match reference schema");
        while(std::getline(in,line)) {
            if(line.empty())continue;
            std::stringstream ss(line);std::string cell;std::vector<double> fields;
            while(std::getline(ss,cell,','))fields.push_back(realArg(cell.c_str()));
            if(fields.size()!=s.variables+1)throw std::invalid_argument("observation dimension does not match reference schema");
            const auto r=e.evaluate(std::span<const double>(fields.data(),s.variables));
            if(!r.valid())throw std::invalid_argument("mathematical-domain failure on declared observation row "+std::to_string(a.rows));
            ++a.rows;
        }
        if(!a.rows)throw std::invalid_argument("observation file contains no rows");
    }
    return a;
}
std::string json(std::string_view s){std::string o="\"";for(char c:s){if(c=='"'||c=='\\'){o+='\\';o+=c;}else if(c=='\n')o+="\\n";else o+=c;}return o+'"';}
int selfTest(const std::string& domainFixture) {
    auto run=[](std::string expr,std::size_t vars,std::string ops,std::size_t nodes,std::size_t depth,std::size_t work,double lower=-8,double upper=8,std::string data={}){
        Spec s; s.reference=std::move(expr);s.variables=vars;s.maxNodes=nodes;s.maxDepth=depth;
        s.maxOperations=work;s.maxAbsConstant=8;s.fitLower=lower;s.fitUpper=upper;s.data=std::move(data);
        std::stringstream in(ops);std::string op;while(std::getline(in,op,','))s.operators.insert(op);
        return validate(s);
    };
    auto rejects=[&](auto f){try{f();return false;}catch(const std::exception&){return true;}};
    if(run("(x0*x0)+1",1,"+,*",5,3,4).expression.nodeCount()!=5)return 1;
    if(run("sin((1.5*x0))",1,"*,sin",4,3,4).expression.depth()!=3)return 2;
    if(!rejects([&]{run("sin(x0)",1,"+",2,2,1);}))return 3; // unavailable operator
    if(!rejects([&]{run("x1",1,"+",2,2,1);}))return 4; // dimension
    if(!rejects([&]{run("(x0+)",1,"+",3,3,2);}))return 5; // malformed/postfix-invalid
    if(!rejects([&]{run("(x0+1)",1,"+",2,3,2);}))return 6; // node cap
    if(!rejects([&]{run("sin(sin(x0))",1,"sin",3,2,2);}))return 7; // depth cap
    if(!rejects([&]{run("powi(x0,2)",1,"+",2,2,2);}))return 8; // power grammar
    if(!rejects([&]{run("x0+2",1,"+",3,2,2,-1,1);}))return 9; // fitted constant bounds
    if(!rejects([&]{run("x0+1",1,"+",3,2,2,2,-2);}))return 10; // invalid fit bounds
    if(!rejects([&]{run("(x0/0)",1,"/",3,2,1,-8,8,domainFixture);}))return 11;
    if(!rejects([&]{run("x0*x0",1,"*",3,2,0);}))return 12; // operation cap
    // A separately specified two-variable grammar verifies correct identity ordering.
    if(run("(x0*x1)+x0",2,"+,*",5,3,4).expression.variableCount()!=2)return 13;
    if(!rejects([&]{run("sin(((0.8*x0)+0.25))+(0.1*x0*x0)",1,"+,*,sin",10,5,9);}))return 14; // historical 12-node reference / 10-node cap
    if(!rejects([&]{run("powi(x0,9)",1,"powi",2,2,9);}))return 15; // exponent restriction
    if(!rejects([&]{run("x0",1,"+",1,1,1,8,-8);}))return 16; // reversed bounds
    if(!rejects([&]{run("(x0+1)",1,"+",3,1,2);}))return 17; // degenerate depth limit
    {std::ofstream f(domainFixture);f<<"x0,y\n-2,0\n";}
    if(!rejects([&]{run("pow(x0,0.5)",1,"pow",3,2,1,-8,8,domainFixture);}))return 18;
    if(!rejects([&]{run("pow(x0,x0)",1,"pow",3,2,1);}))return 19; // fitted exponent must match declared parameterization
    if(run("pow(x0,1.7)",1,"pow",3,2,1).constants!=1)return 20; // independent real-exponent slot
    if(!rejects([&]{run("x0",1,"+,not_an_operator",1,1,0);}))return 21; // unused grammar operator
    std::cout<<"scientific benchmark preflight self-tests passed: 21\n";return 0;
}
}
int main(int argc,char**argv) try {
    if(argc==3&&std::string(argv[1])=="--self-test") {
        const std::string fixture=argv[2];
        const int result=selfTest(fixture);
        std::remove(fixture.c_str());
        return result;
    }
    const auto a=validate(arguments(argc,argv));
    std::cout<<"{\"status\":\"representable\",\"canonical_expression\":"<<json(a.expression.toString())
        <<",\"postfix_structure_id\":"<<json(topology(a.expression))
        <<",\"variables\":"<<a.expression.variableCount()<<",\"nodes\":"<<a.expression.nodeCount()
        <<",\"depth\":"<<a.expression.depth()<<",\"arithmetic_operations\":"<<a.expression.arithmeticCost()
        <<",\"independent_constant_slots\":"<<a.constants<<",\"domain_rows_checked\":"<<a.rows<<",\"operators\":[";
    bool first=true;for(const auto&op:a.operators){if(!first)std::cout<<',';first=false;std::cout<<json(op);}std::cout<<"]}\n";return 0;
} catch(const std::exception&e){std::cerr<<"benchmark preflight rejected: "<<e.what()<<'\n';return 2;}
