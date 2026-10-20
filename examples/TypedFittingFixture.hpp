#pragma once
#include "TypedRegressionData.hpp"
namespace typed_example {
inline const std::array fittingSchema{TypedShape::vector(3)};
inline std::vector<TypedRegressionRow> fittingData(unsigned split) {
    const std::vector<double> points=split==0?std::vector<double>{-1,0,1}:split==1?std::vector<double>{-.75,-.25,.25,.75}:std::vector<double>{-3,-2,2,3};
    const std::array<double,6> W{1.5,-.75,.25,-.5,2.,1.25};const std::array c{.625,-1.375};
    std::vector<TypedRegressionRow> rows;
    for(double a:points)for(double b:points)for(double d:points) {
        const std::array x{a,b,d};std::array<double,2> y{};
        for(std::size_t i=0;i<2;++i){double value=W[3*i]*x[0];for(std::size_t k=1;k<3;++k)value+=W[3*i+k]*x[k];y[i]=value+c[i];}
        rows.push_back({{TypedValue::vector(x)},TypedValue::vector(y)});
    }
    return rows;
}
inline TypedExpression unfitted() {
    const auto W=TypedExpression::constant(TypedValue::matrix(2,3,std::array{1.,1.,1.,1.,1.,1.}),fittingSchema,limits());
    const auto c=TypedExpression::constant(TypedValue::vector(std::array{1.,1.}),fittingSchema,limits());
    return TypedExpression::binary(K::Add,TypedExpression::binary(K::MatVec,W,TypedExpression::variable(0,fittingSchema,limits()),limits()),c,limits());
}
inline ConstantFitConfig fittingConfig(){ConstantFitConfig c;c.maxLossAttempts=5000;c.maxSweeps=512;c.maxProposals=20000;c.minimumStep=1e-10;return c;}
}
