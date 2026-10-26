#include <AdvancedLinearAlgebra.hpp>
#include <ExpressionParsing.hpp>
#include <ExpressionSecondDerivative.hpp>
#include <FiniteStatistics.hpp>
#include <ScientificRegression.hpp>
#include <iostream>
int main() {
  auto residual = parseExpression("x0-x2*x1", 3);
  ScientificEquationSystem heat(2, {residual},
                                {{0, 1, 1, {{{-.25, -1}, 0}, {{-.5, -2}, 0}}}},
                                {{1, 0, 2}});
  ConstantFitConfig fit;
  fit.lower = 0;
  fit.upper = 2;
  fit.maxLossAttempts = 128;
  auto fitted = fitScientificParameters(heat, fit);
  std::cout << "Heat residual coefficient: " << fitted.parameters[0]
            << " loss=" << fitted.loss << '\n';
  auto field = parseExpression("exp((-0.25)*x0)*sin(x1)", 2);
  auto derivative = evaluateSecondPartial(field, std::array{.5, 1.}, 1, 1);
  if (!derivative.derivatives)
    return 1;
  std::cout << "u_xx=" << derivative.derivatives->second << '\n';
  auto matrix = TypedValue::matrix(2, 2, std::array{2., 1., 1., 2.});
  auto values = symmetricEigen(matrix);
  std::cout << "Eigen backend status=" << static_cast<int>(values.status)
            << '\n';
  auto tensor = BoundedTensor(TensorShape(std::array<std::size_t, 2>{2, 2}),
                              std::array{1., 2., 3., 4.});
  auto contraction = tensorContract(tensor, tensor, 1, 0);
  if (!contraction.success())
    return 1;
  std::cout << "Tensor contraction[0,0]="
            << contraction.value->at(std::array<std::size_t, 2>{0, 0}) << '\n';
  auto complex =
      ComplexExpression::fromScalarGenome(parseExpression("exp(x0*x1)", 2));
  auto result =
      complex.evaluate(std::array<std::complex<double>, 2>{{{1, 0}, {0, 1}}});
  if (!result.success())
    return 1;
  std::cout << "exp(i)=" << *result.value << '\n';
  auto stats = finiteStatistics(std::array{1., 2., 3., 4.});
  if (!stats.success())
    return 1;
  std::cout << "Population variance=" << stats.value->variance << '\n';
  std::cout << "erf(1)="
            << parseExpression("erf(x0)", 1).evaluate(std::array{1.}).value()
            << '\n';
  return 0;
}
