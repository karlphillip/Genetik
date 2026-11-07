#include <ExpressionDerivative.hpp>
#include <ExpressionParsing.hpp>

#include <array>
#include <cmath>
#include <iostream>

int main() {
    const auto expression = parseExpression("sigmoid(x0) + sinc(x1)", 2);
    const std::array inputs{0.0, 0.0};
    const auto value = expression.evaluate(inputs);
    const auto slope = evaluatePartial(expression, inputs, 0);
    if (!value.valid() || !slope.derivativeAvailable() || value.value() != 1.5 ||
        std::abs(slope.derivative() - 0.25) > 1e-15)
        return 1;

    std::cout << expression.toString() << " = " << value.value() << '\n'
              << "d/dx0 = " << slope.derivative() << '\n';
}
