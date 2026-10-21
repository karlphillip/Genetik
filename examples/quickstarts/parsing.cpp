#include <ExpressionParsing.hpp>
#include <TypedExpressionParsing.hpp>
#include <array>
#include <iostream>

int main() {
    const auto f=parseExpression("exp(-1.0*x0) + powi(x0,2)",1);
    if(!f.sameRepresentation(parseExpression(f.toString(),1)) || f.evaluate(std::array{0.}).value()!=1)return 1;
    std::cout<<f.toString()<<'\n';
    const std::array schema{TypedShape::matrix(2,3),TypedShape::vector(3),TypedShape::vector(2)};
    const auto g=parseTypedExpression("matvec(x0,x1) + x2",schema,TypedShape::vector(2));
    const std::array bindings{TypedValue::matrix(2,3,std::array{1.,2.,3.,-1.,0.,4.}),TypedValue::vector(std::array{2.,-1.,3.}),TypedValue::vector(std::array{5.,-2.})};
    const auto value=g.evaluate(bindings);
    if(!value.valid()||value.value().at(0)!=14||value.value().at(1)!=8)return 1;
    const auto owned=parseTypedExpression("matrix[2,3](1,2,3,-1,0,4)",{},TypedShape::matrix(2,3));
    if(parseTypedExpression(owned.toString(),{},owned.shape()).toString()!=owned.toString())return 1;
    if(parseTypedExpression(g.toString(),schema,g.shape()).toString()!=g.toString())return 1;
    std::cout<<g.schemaString()<<" -> "<<g.toString()<<" = "<<value.value().toString()<<'\n'<<owned.toString()<<'\n';
    try{(void)parseExpression("-x0",1);return 1;}
    catch(const ExpressionParseError& e){std::cout<<"rejected: "<<e.what()<<'\n';}
}
