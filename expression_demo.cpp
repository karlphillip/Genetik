#include <Expression.hpp>
#include <array>
#include <iostream>
#include <limits>

int main() {
    // Manually constructed expression evaluation; this example performs no evolution/fitting.
    const auto x = Expression::variable(0);
    const auto square = Expression::binary(ExpressionKind::Multiply, x, x);
    const auto linear = Expression::binary(ExpressionKind::Add, square, x);
    const auto polynomial = Expression::binary(ExpressionKind::Add, linear, Expression::constant(1, 1));
    std::cout << "Manually constructed f(x0) = " << polynomial.toString()
              << "\nnodes=" << polynomial.nodeCount() << " depth=" << polynomial.depth() << '\n';
    for (double input : {-2., -1., 0., 1., 2.}) {
        const auto result = polynomial.evaluate(std::array{input});
        if (!result.valid()) return 1;
        std::cout << "f(" << input << ")=" << result.value() << '\n';
    }
    const auto large = Expression::constant(std::numeric_limits<double>::max());
    const auto overflow = Expression::binary(ExpressionKind::Multiply, large, large);
    const auto masked = Expression::binary(ExpressionKind::Multiply, Expression::constant(0), overflow);
    const auto result = masked.evaluate({});
    if (result.valid()) return 1;
    std::cout << "Arithmetic invalidity at postfix node " << result.failedNode().value()
              << "; no numerical prediction is available\n";
}
