#pragma once
// Fixed diagnostic corpus; targets/fixtures are application data, never GP templates.
#include <examples/PrimeRegression.hpp>
#include <examples/StrictArithmeticData.hpp>
namespace evaluation_evidence {
using K=ExpressionKind;
inline Expression bin(K k,const Expression& a,const Expression& b){return Expression::binary(k,a,b);}
inline Expression unary(K k,const Expression& a){return Expression::unary(k,a);}
struct Case {std::string name;Expression expression;std::vector<RegressionRow> rows;std::size_t batches;};
inline std::vector<Case> corpus() {
    const auto x=Expression::variable(0); const auto c=[](double v){return Expression::constant(v,1);};
    const auto u=Expression::variable(0,2),v=Expression::variable(1,2);
    auto primes=prime_example::rows(prime_example::reference());
    const auto polynomial=bin(K::Add,bin(K::Add,bin(K::Multiply,x,x),x),c(1));
    // Accepted prime representative, with faithful grouping/constants unchanged.
    const auto prime=bin(K::Multiply,x,bin(K::Add,
        bin(K::Multiply,bin(K::Multiply,bin(K::Divide,c(-3),c(-3)),x),bin(K::Divide,c(.79185613989830017),x)),
        bin(K::Multiply,c(.1121049374658476),unary(K::Log,x))));
    const auto target=bin(K::Add,unary(K::Log,u),bin(K::Divide,v,u));
    const auto mixed=bin(K::Divide,bin(K::Subtract,bin(K::Multiply,target,bin(K::Add,u,v)),unary(K::Log,u)),bin(K::Add,u,Expression::constant(2,2)));
    // Full balanced 31-node boundary tree, like the bounded variation fixtures.
    auto near=x;for(int i=0;i<4;++i)near=bin(K::Add,near,near);
    std::vector<Case> cases;
    cases.push_back({"polynomial",polynomial,primes,2000});
    cases.push_back({"prime",prime,primes,2000});
    cases.push_back({"arithmetic",target,regression_example::arithmeticRows(0),20000});
    cases.push_back({"mixed",mixed,regression_example::arithmeticRows(0),20000});
    cases.push_back({"near_limit",near,primes,1000});
    auto late=primes;late.back().inputs[0]=0;
    auto early=late;early.front().inputs[0]=0;
    const auto log=unary(K::Log,x);
    cases.push_back({"early_invalid",log,early,100000});
    cases.push_back({"late_invalid",log,late,2000});
    const double d=std::numeric_limits<double>::denorm_min(),m=std::numeric_limits<double>::max();
    const std::vector<RegressionRow> edgeRows={{{1},0},{{0},0},{{-0.0},0},{{-1},0},{{d},0},{{2},0}};
    cases.push_back({"alternating_log",log,edgeRows,10000});
    cases.push_back({"positive_zero_division",bin(K::Divide,x,c(0)),edgeRows,10000});
    cases.push_back({"negative_zero_division",bin(K::Divide,x,c(-0.0)),edgeRows,10000});
    cases.push_back({"signed_underflow",bin(K::Multiply,c(-d),c(.5)),edgeRows,10000});
    cases.push_back({"subnormal_ratio",bin(K::Divide,c(d),c(d)),edgeRows,10000});
    cases.push_back({"masked_overflow",bin(K::Multiply,c(0),bin(K::Multiply,c(m),c(m))),edgeRows,10000});
    cases.push_back({"underflow_domain",unary(K::Log,bin(K::Multiply,c(d),c(.5))),edgeRows,10000});
    cases.push_back({"residual_overflow",c(m),{{{1},-m}},10000});
    cases.push_back({"sum_overflow",c(1e154),{{{1},0},{{2},0}},10000});
    cases.push_back({"higher_variable",bin(K::Subtract,Expression::variable(2,3),Expression::constant(.1,3)),{{{1,2,3},0},{{4,5,-0.0},1}},10000});
    return cases;
}
}
