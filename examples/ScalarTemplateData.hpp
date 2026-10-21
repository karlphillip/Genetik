#pragma once
#include <ScalarTemplateRegression.hpp>
#include <array>

namespace scalar_template_example {
inline Expression scaffoldSource() {
    using N=ExpressionNode;using K=ExpressionKind;
    const std::array nodes{N::constant(-1),N::variable(1),N::binary(K::Multiply),N::variable(0),N::binary(K::Multiply)};
    return Expression(nodes,2,{13,6,8}); // selected occurrence is postfix root1, not a magic variable
}
inline std::vector<RegressionRow> data(int stage,bool affine=false) {
    const std::vector<double> v=stage==0?std::vector<double>{-2,-1,0,1,2}:stage==1?std::vector<double>{-1.5,-.5,0,.5,1.5}:std::vector<double>{-3,0,3};
    const std::vector<double> t=stage==0?std::vector<double>{-2,-1,0,1,2}:stage==1?std::vector<double>{-1.5,-.5,.5,1.5}:std::vector<double>{-3,3};
    std::vector<RegressionRow> rows;
    // Independent ordinary arithmetic target oracle; v outer/T inner, deterministic.
    for(double velocity:v)for(double temperature:t)rows.push_back({{velocity,temperature},-(affine ? .5*temperature+1.5:temperature*temperature+1)*velocity});
    return rows;
}
inline std::shared_ptr<const ScalarTemplateProblem> problem(const std::vector<RegressionRow>& rows,bool fitting=false) {
    const ExpressionLimits local{9,4,fitting?8.:2.},full{13,6,fitting?8.:2.};
    const auto source=scaffoldSource();const ScalarExpressionTemplate scaffold(source,1,local,full);
    return ScalarTemplateProblem::create(scaffold,RegressionProblem::create(rows,full),{1},{ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply});
}
inline SymbolicRegressionConfig config() {
    SymbolicRegressionConfig c;c.population=128;c.elites=2;c.maxEvaluations=10000;c.maxTrainingLossAttempts=10000;
    c.maxGenerationAttempts=2000;c.trainingLossThreshold=0;c.fitting.reset();return c;
}
}
