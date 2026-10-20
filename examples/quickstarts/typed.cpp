#include <TypedExpression.hpp>
#include <array>
#include <iostream>
#include <stdexcept>

int main() {
    using K = TypedExpressionKind;
    const std::array schema{TypedShape::vector(3), TypedShape::vector(2)};
    const auto A = TypedExpression::constant(
        TypedValue::matrix(2, 3, std::array{1., 2., 3., -1., 0., 4.}), schema);
    const auto x = TypedExpression::variable(0, schema);
    const auto b = TypedExpression::variable(1, schema);
    const auto f = TypedExpression::binary(K::Add, TypedExpression::binary(K::MatVec, A, x), b);
    const std::array bindings{TypedValue::vector(std::array{2., -1., 3.}),
                              TypedValue::vector(std::array{5., -2.})};
    const auto result = f.evaluate(bindings);
    if (!result.valid() || result.value().at(0) != 14 || result.value().at(1) != 8)
        throw std::runtime_error("A*x+b fixture failed");
    std::cout << f.schemaString() << " -> " << f.shape().toString() << '\n'
              << f.toString() << '\n' << result.value().toString()
              << " arithmetic=" << result.arithmeticOperations()
              << " temporary_coefficients=" << f.temporaryCoefficients() << '\n';

    const auto matrix = TypedExpression::constant(TypedValue::matrix(2, 3, std::array{1., 2., 3., -1., 0., 4.}));
    const auto other = TypedExpression::constant(TypedValue::matrix(3, 2, std::array{2., 0., 1., 1., -1., 2.}));
    const auto product = TypedExpression::binary(K::MatMul, matrix, other).evaluate({});
    const auto transpose = TypedExpression::unary(K::Transpose, matrix).evaluate({});
    const auto dot = TypedExpression::binary(K::Dot,
        TypedExpression::constant(TypedValue::vector(std::array{2., -1., 3.})),
        TypedExpression::constant(TypedValue::vector(std::array{1., 2., 3.}))).evaluate({});
    if (product.value().at(0, 0) != 1 || product.value().at(0, 1) != 8 ||
        product.value().at(1, 0) != -6 || product.value().at(1, 1) != 8 ||
        transpose.value().at(2, 1) != 4 || dot.value().scalarValue() != 9)
        throw std::runtime_error("rectangular product/transpose/dot fixture failed");
    std::cout << "A*B=" << product.value().toString() << '\n'
              << "transpose(A)=" << transpose.value().toString() << '\n'
              << "dot=" << dot.value().toString() << '\n';
    const auto limited = f.evaluate(bindings, {.maxArithmeticOperations = 11});
    if (limited.status() != TypedEvaluationStatus::ResourceLimit)
        throw std::runtime_error("expected local resource outcome");
    std::cout << "11-operation allowance: resource limit, no value\n";
}
