#include <ExpressionDerivative.hpp>
#include <ExpressionParsing.hpp>
#include <ScalarExpressionTemplate.hpp>
#include <array>
#include <iostream>

int main() {
    const auto f = parseExpression("x0*x0+x0*x1+sin(x1)", 2);
    const std::array inputs{2., 0.};
    const auto dx = evaluatePartial(f, inputs, 0);
    const auto dy = evaluatePartial(f, inputs, 1);
    if (dx.value()!=4 || dx.derivative()!=4 || dy.derivative()!=3) return 1;
    std::cout << f.toString() << " value=" << dx.value()
              << " dx0=" << dx.derivative() << " dx1=" << dy.derivative() << '\n';

    const auto decay = parseExpression("exp(((-1.0) * x0))", 1);
    const auto d = evaluatePartial(decay, std::array{0.}, 0);
    if (d.value()!=1 || d.derivative()!=-1) return 1;
    std::cout << decay.toString() << " value=" << d.value() << " derivative=" << d.derivative() << '\n';

    // Select the x1 occurrence structurally, without placeholder variables/text rewriting.
    const ScalarExpressionTemplate scaffold(parseExpression("((-1.0)*x1)*x0",2), 1, {9,4,8}, {13,6,8});
    const auto model = scaffold.materialize(parseExpression("1.0+x1*x1",2));
    for (double velocity : {3.,0.}) {
        const std::array point{velocity,2.};
        const auto dv=evaluatePartial(model,point,0), dt=evaluatePartial(model,point,1);
        if(dv.value()!=-5*velocity || dv.derivative()!=-5 || dt.derivative()!=-4*velocity)return 1;
        std::cout << "x0=v="<<velocity<<" x1=T=2 model="<<model.toString()<<" value="<<dv.value()
                  <<" d/dv="<<dv.derivative()<<" d/dT="<<dt.derivative()<<'\n';
    }
    const auto singular=evaluatePartial(parseExpression("sqrt(x0*x0)",1),std::array{0.},0);
    if(singular.status()!=DerivativeStatus::Unavailable || singular.value()!=0 ||
       singular.derivativeFailure()!=DerivativeFailure::SingularRule)return 1;
    std::cout<<"sqrt(x0*x0) at zero: value="<<singular.value()
             <<"; derivative unavailable: active local sqrt rule is singular (not a global differentiability proof).\n";
}
