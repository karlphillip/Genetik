#pragma once
#include <TypedSymbolicRegression.hpp>
#include <array>
namespace typed_example {
using K=TypedExpressionKind;
inline const std::array schema{TypedShape::matrix(2,3),TypedShape::vector(3),TypedShape::vector(2)};
inline TypedExpressionLimits limits(){TypedExpressionLimits l;l.maxNodes=15;l.maxDepth=5;l.maxAbsConstant=8;l.maxConstantNodes=4;l.maxConstantCoefficients=16;l.maxArithmeticOperations=512;l.maxTemporaryCoefficients=128;return l;}
inline std::vector<TypedRegressionRow> data(unsigned split) {
    // Separate data RNG. A coefficients, then x, then b, in row-major order.
    std::mt19937 random(73001+split);std::vector<TypedRegressionRow> rows;
    for(unsigned r=0;r<64;++r) {
        std::array<double,11> inputs{};
        for(auto& v:inputs) {
            if(split==0)v=static_cast<int>(std::uniform_int_distribution<unsigned>(0,8)(random))*.5-2.;
            else if(split==1)v=(2*static_cast<int>(std::uniform_int_distribution<unsigned>(0,15)(random))-15)/8.;
            else {v=std::uniform_int_distribution<unsigned>(0,3)(random)*.5+2.5;if(std::bernoulli_distribution(.5)(random))v=-v;}
        }
        std::array<double,2> targets{};
        // Independent reference, never uses TypedExpression or generated tree features.
        for(std::size_t i=0;i<2;++i){double v=inputs[3*i]*inputs[6];for(std::size_t k=1;k<3;++k)v+=inputs[3*i+k]*inputs[6+k];targets[i]=v+inputs[9+i];}
        rows.push_back({{TypedValue::matrix(2,3,std::span(inputs.data(),6)),TypedValue::vector(std::span(inputs.data()+6,3)),TypedValue::vector(std::span(inputs.data()+9,2))},TypedValue::vector(targets)});
    }
    return rows;
}
inline TypedSymbolicRegressionConfig config() {
    TypedSymbolicRegressionConfig c;c.population=128;c.maxEvaluations=10000;c.maxTrainingLossAttempts=10000;c.maxGenerationAttempts=100;
    c.grammar.initialMaxDepth=4;c.grammar.mutationMaxDepth=3;c.grammar.proposalLimit=8;
    c.grammar.shapes={TypedShape::matrix(2,3),TypedShape::vector(3),TypedShape::vector(2),TypedShape::scalar()};
    return c;
}
}
