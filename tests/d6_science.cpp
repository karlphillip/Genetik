#include <AdvancedLinearAlgebra.hpp>
#include <ComplexExpressionParsing.hpp>
#include <ExpressionParsing.hpp>
#include <ExpressionSecondDerivative.hpp>
#include <FiniteStatistics.hpp>
#include <ObservedDifferentialSystem.hpp>
#include <ScientificRegression.hpp>
#include <barrier>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
void check(bool x, const char *why) {
  if (!x)
    throw std::runtime_error(why);
}
void near(double a, double b, double tolerance = 1e-11) {
  check(std::abs(a - b) <= tolerance * std::max(1., std::abs(b)),
        "independent numeric reference");
}
Expression parse(std::string_view s, std::size_t inputs = 1) {
  return parseExpression(s, inputs, {}, ExpressionLimits{63, 16, 16, 10000});
}
ScientificEquationSystem oscillator() {
  auto a = parse("x2", 5), c = parse("((-1)*x3*x2)-x4*x1", 5);
  std::vector<ScientificObservation> dx, dv;
  for (double x : {-2., -.5, 1., 2.})
    for (double v : {-1., 0., 1.}) {
      dx.push_back({{0, x, v}, v});
      dv.push_back({{0, x, v}, -.25 * v - 2 * x});
    }
  return {3, {a, c}, {{0, 1, 1, dx}, {1, 2, 1, dv}}, {{1, 0, 4}, {1, 0, 4}}};
}
void differential() {
  auto inactive =
      evaluateSecondPartial(parse("pow(x1,1.7)+normal_pdf(x1,0,1)+x0*x0", 2),
                            std::array{2., 1.}, 0, 0);
  check(inactive.derivatives.has_value(),
        "inactive unsupported rules do not cause unavailability");
  near(inactive.derivatives->second, 2);
  auto inactiveDivision =
      evaluateSecondPartial(parse("x0/x1", 2), std::array{0., 1e-310}, 0, 0);
  check(inactiveDivision.status == ScientificStatus::Unavailable,
        "required first derivative overflow remains unavailable");

  for (double t : {0., .5, 2.})
    for (double x : {-.75, .25, 1.5}) {
      auto e = parse("exp((-0.25)*x0)*sin(x1)", 2);
      auto xx = evaluateSecondPartial(e, std::array{t, x}, 1, 1);
      auto tx = evaluateSecondPartial(e, std::array{t, x}, 0, 1);
      check(xx.status == ScientificStatus::Success &&
                tx.status == ScientificStatus::Success,
            "second partial qualified heat field");
      near(xx.derivatives->second, -std::exp(-.25 * t) * std::sin(x));
      near(tx.derivatives->second, -.25 * std::exp(-.25 * t) * std::cos(x));
      auto reverse = evaluateSecondPartial(e, std::array{t, x}, 1, 0);
      near(reverse.derivatives->second, tx.derivatives->second);
      check(std::bit_cast<std::uint64_t>(*xx.primal) ==
                std::bit_cast<std::uint64_t>(
                    e.evaluate(std::array{t, x}).value()),
            "primal bits");
    }
  near(evaluateSecondPartial(parse("x0*x0*x0"), std::array{2.}, 0, 0)
           .derivatives->second,
       12);
  check(evaluateSecondPartial(parse("sqrt(x0*x0)"), std::array{0.}, 0, 0)
                .status == ScientificStatus::Unavailable,
        "conservative second singularity");
  auto invalid =
      evaluateSecondPartial(parse("sqrt(x0*x0)+log(x0)"), std::array{0.}, 0, 0);
  check(invalid.status == ScientificStatus::Invalid && !invalid.primal,
        "later primal error wins");
  auto exhausted = evaluateSecondPartial(parse("exp(x0)"), std::array{1.}, 0, 0,
                                         {1, 100, 10000, 10, 1e-12});
  check(exhausted.status == ScientificStatus::ResourceLimit,
        "bounded derivative");
  auto system = oscillator();
  ObservedOdeSystem ode(system, 0, {1, 2}, {0, 1});
  check(!ode.identity().empty(), "explicit ODE field identities");
  auto exact = system.assess(std::array{.25, 2.});
  check(exact.loss.has_value(), "whole oscillator valid");
  near(*exact.loss, 0);
  ConstantFitConfig f;
  f.maxLossAttempts = 512;
  f.lower = 0;
  f.upper = 4;
  auto fitted = fitScientificParameters(system, f);
  near(fitted.parameters[0], .25);
  near(fitted.parameters[1], 2);
  near(fitted.loss, 0);
  check(fitted.work.lossAttempts <= 512, "joint parameter budget");
  std::vector<ScientificObservation> heatRows;
  for (double t : {0., .5, 1.})
    for (double x : {-.5, .5, 1.5}) {
      double u = std::exp(-.25 * t) * std::sin(x);
      heatRows.push_back({{-.25 * u, -u, u}, 0});
    }
  ScientificEquationSystem heat(
      3, {parse("x0-x3*x1", 4), parse("x2", 4)},
      {{0, 1, 1, heatRows}, {1, 1, 1, {{{0, 0, 1}, 1}}}}, {{1, 0, 2}});
  ObservedPdeResidualSystem pde(heat, 2, {{0, {0}}, {0, {1, 1}}, {0, {}}});
  check(!pde.identity().empty(), "PDE field and derivative identities");
  auto heatFit = fitScientificParameters(heat, f);
  near(heatFit.parameters[0], .25);
  near(heatFit.loss, 0);
  check(heatFit.supplemental.fieldEvaluations > 0, "all equation work counted");
  std::vector<ScientificObservation> densities;
  for (double x : {-2., -1., 0., 1., 2., 3.}) {
    double z = (x - .5) / 1.25;
    densities.push_back(
        {{x}, std::exp(-.5 * z * z) / (1.25 * std::sqrt(2 * std::acos(-1.)))});
  }
  ScientificEquationSystem gaussian(1, {parse("normal_pdf(x0,x1,x2)", 3)},
                                    {{0, 1, 1, densities}},
                                    {{0, -4, 4}, {1, .125, 4}});
  auto gaussianConfig = f;
  gaussianConfig.lower = -4;
  auto gaussianFit = fitScientificParameters(gaussian, gaussianConfig);
  near(gaussianFit.parameters[0], .5, 1e-6);
  near(gaussianFit.parameters[1], 1.25, 1e-6);
  check(gaussianFit.work.invalid > 0,
        "invalid sigma probes counted; accepted improvements retained");
  // Independent normalized-channel fixture, counts must not change weighting.
  ScientificEquationSystem weighted(
      1, {parse("x0")},
      {{0, 1, 1, {{{0}, 1}, {{1}, 3}}}, {0, 2, 3, {{{0}, 2}, {{2}, 2}}}});
  near(*weighted.assess({}).loss, 1.0);
  auto rows = weighted.channels();
  for (auto &c : rows) {
    auto original = c.observations;
    c.observations.insert(c.observations.end(), original.begin(),
                          original.end());
  }
  ScientificEquationSystem doubled(1, weighted.equations(), rows);
  near(*doubled.assess({}).loss, 1.);
  std::atomic<bool> cancel{true};
  check(system.assess(std::array{.25, 2.}, &cancel).status ==
            ScientificStatus::Cancelled,
        "system cooperative cancellation");
  auto L = PhysicalDimension::base(SIBaseDimension::Length),
       T = PhysicalDimension::base(SIBaseDimension::Time);
  ScientificEquationSystem units(2, {parse("x0/x1", 2)},
                                 {{0, 1, 1, {{{1, 2}, .5}}}}, {}, {}, {L, T},
                                 {L / T});
  bool rejected = false;
  try {
    ScientificEquationSystem bad(2, {parse("x0+x1", 2)}, units.channels(), {},
                                 {}, {L, T}, {L / T});
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  check(rejected, "dimensionally invalid derivative RHS rejected");
}
void tensors() {
  auto a = BoundedTensor(
      TensorShape(std::array<std::size_t, 3>{2, 2, 3}),
      std::array{1., 2., 3., 4., 5., 6., 7., 8., 9., 10., 11., 12.});
  auto b = BoundedTensor(TensorShape(std::array<std::size_t, 2>{3, 2}),
                         std::array{1., 2., 3., 4., 5., 6.});
  auto c = tensorContract(a, b, 2, 0);
  check(c.success(), "rank3 contraction");
  check(c.value->shape().dimensions() == std::vector<std::size_t>{2, 2, 2},
        "contraction outputshape");
  // References independently expanded three-term dot products.
  for (std::size_t i = 0; i < 2; ++i)
    for (std::size_t j = 0; j < 2; ++j)
      for (std::size_t k = 0; k < 2; ++k) {
        double expected = 0;
        for (std::size_t l = 0; l < 3; ++l)
          expected += a.at(std::array{i, j, l}) * b.at(std::array{l, k});
        near(c.value->at(std::array{i, j, k}), expected);
      }
  auto saved =
      restoreTensor(decodeCheckpoint(encodeCheckpoint(checkpointTensor(a))));
  check(
      std::equal(a.values().begin(), a.values().end(), saved.values().begin()),
      "tensor exact codec");
  near(tensorReduce(a, 2).value->at(std::array<std::size_t, 2>{0, 0}), 6);
  auto requirement = tensorScale(a, 2).work.operations;
  check(tensorScale(a, 2, {requirement - 1, 1000, 100000, 100, 1e-12}).status ==
            ScientificStatus::ResourceLimit,
        "below tensor work boundary");
  check(tensorScale(a, 2, {requirement, 1000, 100000, 100, 1e-12}).success(),
        "at tensor work boundary");
  check(
      tensorScale(a, 2, {requirement + 1, 1000, 100000, 100, 1e-12}).success(),
      "above tensor work boundary");
  check(tensorScale(a, 2, {1, 1000, 100000, 100, 1e-12}).status ==
            ScientificStatus::ResourceLimit,
        "tensor operation limit");
  bool rejected = false;
  try {
    TensorShape shape(std::array<std::size_t, 2>{SIZE_MAX, 2});
  } catch (const ScientificResourceExhausted &) {
    rejected = true;
  }
  check(rejected, "checked shape products");
  auto small = BoundedTensor(TensorShape(std::array<std::size_t, 1>{2}),
                             std::array{1., 2.});
  near(tensorContract(small, small, {}, {})
           .value->at(std::array<std::size_t, 2>{1, 1}),
       4);
  auto sum = tensorAdd(small, small);
  near(sum.value->values()[1], 4);
}
void complexMath() {
  auto binary = [&](ComplexKind k) {
    std::array<ComplexNode, 3> nodes{ComplexNode::constant({2, 3}),
                                     ComplexNode::constant({4, -5}),
                                     ComplexNode::operation(k)};
    return ComplexExpression(nodes, 0).evaluate({});
  };
  near(binary(ComplexKind::Add).value->real(), 6);
  near(binary(ComplexKind::Subtract).value->imag(), 8);
  near(binary(ComplexKind::Multiply).value->real(), 23);
  near(binary(ComplexKind::Multiply).value->imag(), 2);
  near(binary(ComplexKind::Divide).value->real(), -7. / 41);
  near(binary(ComplexKind::Divide).value->imag(), 22. / 41);
  auto unary = [&](ComplexKind k, std::complex<double> z,
                   unsigned exponent = 0) {
    std::array<ComplexNode, 2> nodes{ComplexNode::constant(z),
                                     ComplexNode::operation(k, exponent)};
    return ComplexExpression(nodes, 0).evaluate({});
  };
  near(unary(ComplexKind::Conjugate, {2, 3}).value->imag(), -3);
  near(unary(ComplexKind::Magnitude, {2, 3}).value->real(), std::sqrt(13.));
  near(unary(ComplexKind::Sqrt, {-1, -0.}).value->imag(), -1);
  near(unary(ComplexKind::Sin, {0, 1}).value->imag(), std::sinh(1.));
  near(unary(ComplexKind::Cos, {0, 1}).value->real(), std::cosh(1.));
  near(unary(ComplexKind::Powi, {1, 1}, 4).value->real(), -4);
  check(unary(ComplexKind::Exp, {1000, 0}).status ==
            ScientificStatus::NumericalFailure,
        "complex nonfinite arithmetic failure");

  auto e = ComplexExpression::fromScalarGenome(parse("exp(x0*x1)", 2));
  for (double x : {0., .25, 1., std::acos(-1.), 2 * std::acos(-1.)}) {
    auto y = e.evaluate(std::array<std::complex<double>, 2>{{{x, 0}, {0, 1}}});
    check(y.success(), "complex exp");
    near(y.value->real(), std::cos(x));
    near(y.value->imag(), std::sin(x));
  }
  std::array<ComplexNode, 2> logarithm{
      ComplexNode::variable(0), ComplexNode::operation(ComplexKind::Log)};
  ComplexExpression log(logarithm, 1);
  auto plus = log.evaluate(std::array<std::complex<double>, 1>{{{-1., 0.}}}),
       minus = log.evaluate(std::array<std::complex<double>, 1>{{{-1., -0.}}});
  near(plus.value->imag(), std::acos(-1.));
  near(minus.value->imag(), -std::acos(-1.));
  check(log.evaluate(std::array<std::complex<double>, 1>{{{0, 0}}}).status ==
            ScientificStatus::Invalid,
        "logzero domain");
  std::array<ComplexNode, 1> bits{ComplexNode::constant({-0., -1.125})};
  ComplexExpression original(bits, 0);
  auto restored = restoreComplexExpression(decodeCheckpoint(
      encodeCheckpoint(checkpointComplexExpression(original))));
  check(std::bit_cast<std::uint64_t>(restored.nodes()[0].value.real()) ==
            std::bit_cast<std::uint64_t>(-0.),
        "complex signedzero codec");
  near(restored.nodes()[0].value.imag(), -1.125);
  check(parseComplexExpression(e.format(), 2).format() == e.format(),
        "complex canonical parsing round trip");
  check(!e.format(ExpressionTextFormat::LaTeX).empty(), "complex presentation");
  auto wrong = checkpointComplexExpression(e);
  wrong.identity += "different";
  bool rejected = false;
  try {
    restoreComplexExpression(wrong);
  } catch (const CheckpointError &) {
    rejected = true;
  }
  check(rejected, "branch identity mismatch");
}
void statistics() {
  auto x = std::array{1., 2., 3., 4.};
  auto population = finiteStatistics(x), sample = finiteStatistics(x, 1);
  near(population.value->mean, 2.5);
  near(population.value->variance, 1.25);
  near(sample.value->variance, 5. / 3);
  near(*finiteCovariance(x, x, 0, true).value, 1);
  near(*finiteCovariance(x, x).value, 1.25);
  near(
      *finiteWeightedMean(std::array{1., 3.}, std::array{.25, .75}, true).value,
      2.5);
  check(finiteCovariance(std::array{1., 1.}, std::array{1., 1.}, 0, true)
                .status == ScientificStatus::Invalid,
        "zero variance correlation");
  bool reject = false;
  try {
    finiteStatistics(std::array{1.}, 1);
  } catch (const std::invalid_argument &) {
    reject = true;
  }
  check(reject, "dof validation");
  check(finiteWeightedMean(x, std::array{0., 0., 0., 0.}).status ==
            ScientificStatus::Invalid,
        "zero total weight");
  auto density = parse("normal_pdf(x0,0,1)");
  near(density.evaluate(std::array{0.}).value(),
       1 / std::sqrt(2 * std::acos(-1.)));
  near(parse("normal_cdf(x0,0,1)").evaluate(std::array{0.}).value(), .5);
  check(!parse("normal_pdf(x0,0,0)").evaluate(std::array{0.}).valid(),
        "positive sigma");
  near(parse("poisson_pmf(x0,2)").evaluate(std::array{0.}).value(),
       std::exp(-2.));
  check(!parse("poisson_pmf(x0,2)").evaluate(std::array{.5}).valid(),
        "integer poisson argument");
}
void linear() {
  auto vector = TypedValue::vector(std::array{3., 4., 0.});
  near(*vectorNorm(vector).value, 5);
  near(*vectorNorm(vector, true).value, 25);
  near(vectorNormalize(vector).value->at(0), .6);
  auto cross = vectorCross(TypedValue::vector(std::array{1., 0., 0.}),
                           TypedValue::vector(std::array{0., 1., 0.}));
  near(cross.value->at(2), 1);
  auto a = TypedValue::matrix(2, 2, std::array{2., 1., 1., 2.});
  near(*matrixTrace(a).value, 4);
  near(*matrixDeterminant(a).value, 3);
  near(matrixPower(a, 2).value->at(0, 0), 5);
  auto solve = solveLinearSystem(a, TypedValue::vector(std::array{3., 3.}));
  auto eigen = symmetricEigen(a);
#ifdef GENETIK_HAS_LAPACK
  check(solve.success() && eigen.success(), "LAPACK results");
  for (std::size_t n : {std::size_t{1}, std::size_t{3}, std::size_t{16}}) {
    std::vector<double> diagonal(n * n), rhs(n);
    for (std::size_t i = 0; i < n; ++i) {
      diagonal[i * n + i] = static_cast<double>(i + 1);
      rhs[i] = 2 * static_cast<double>(i + 1);
    }
    auto matrix = TypedValue::matrix(n, n, diagonal);
    auto solution = solveLinearSystem(matrix, TypedValue::vector(rhs));
    auto pairs = symmetricEigen(matrix);
    check(solution.success() && pairs.success(), "matrix dimensions1,3,16");
    for (std::size_t i = 0; i < n; ++i) {
      near(solution.value->solution.at(i), 2);
      near(pairs.value->eigenvalues.at(i), static_cast<double>(i + 1));
    }
  }
  auto repeated = symmetricEigen(
      TypedValue::matrix(3, 3, std::array{2., 0., 0., 0., 2., 0., 0., 0., 2.}));
  check(repeated.success(), "repeated eigenvalues accepted");
  near(repeated.value->maximumResidual, 0);

  near(solve.value->solution.at(0), 1);
  near(solve.value->residualNorm, 0);
  near(eigen.value->eigenvalues.at(0), 1);
  near(eigen.value->eigenvalues.at(1), 3);
  near(eigen.value->maximumResidual, 0);
  auto diagonal = TypedValue::matrix(2, 2, std::array{-1., 0., 0., -2.});
  auto exp = symmetricMatrixExponential(diagonal, .5);
  check(exp.success(), "symmetric matrix exponential");
  near(exp.value->at(0, 0), std::exp(-.5));
  near(exp.value->at(1, 1), std::exp(-1.));
  near(exp.value->at(0, 1), 0);
  check(solveLinearSystem(TypedValue::matrix(2, 2, std::array{1., 1., 1., 1.}),
                          TypedValue::vector(std::array{1., 1.}))
                .status == ScientificStatus::Invalid,
        "singular solve");
#else
  check(solve.status == ScientificStatus::Unavailable &&
            eigen.status == ScientificStatus::Unavailable,
        "optional backend unavailable explicit");
#endif
  check(vectorNormalize(TypedValue::vector(std::array{0., 0.})).status ==
            ScientificStatus::Invalid,
        "zero norm invalid");
}
void references(const char *path) {
  std::ifstream f(path);
  check(bool(f), "reference input");
  std::string line;
  unsigned count = 0;
  double maxError = 0, maxScaledError = 0;
  while (std::getline(f, line)) {
    std::istringstream row(line);
    std::string expression;
    double x, y;
    row >> expression >> x >> y;
    check(bool(row), "reference row");
    auto p = parse(expression).evaluate(std::array{x});
    check(p.valid(), "reference finite");
    maxError = std::max(maxError, std::abs(p.value() - y));
    maxScaledError = std::max(maxScaledError, std::abs(p.value() - y) /
                                                  std::max(1., std::abs(y)));
    near(p.value(), y, 2e-12);
    ++count;
  }
  check(count >= 30, "reference count");
  std::cout << "Special references=" << count
            << " max absolute discrepancy=" << maxError
            << " max scaled discrepancy=" << maxScaledError << "\n";
  for (auto name : {"gamma", "log_gamma", "bessel_j0"})
    check(
        evaluatePartial(parse(std::string(name) + "(x0)"), std::array{1.25}, 0)
                .status() == DerivativeStatus::Unavailable,
        "unsupported special derivative explicit");
  for (auto name : {"gamma", "log_gamma"})
    check(!parse(std::string(name) + "(x0)").evaluate(std::array{0.}).valid(),
          "special domains");
  near(evaluatePartial(parse("erf(x0)"), std::array{.5}, 0).derivative(),
       2 / std::sqrt(std::acos(-1.)) * std::exp(-.25));
}
ScalarMathematicsProblem problem(unsigned type) {
  ExpressionLimits limits{15, 5, 8, 1000};
  if (type == 0)
    return scientificSystemProblem(oscillator(), 1, limits);
  if (type == 1)
    return complexRegressionProblem(
        {{{{.25, 0}, {0, 1}}, {std::cos(.25), std::sin(.25)}},
         {{{1., 0}, {0, 1}}, {std::cos(1.), std::sin(1.)}}},
        limits);
  auto shape = TensorShape(std::array<std::size_t, 2>{2, 2});
  auto a = BoundedTensor(shape, std::array{1., 2., 3., 4.}),
       target = BoundedTensor(shape, std::array{2.1, 4.2, 6.3, 8.4});
  return tensorScaleRegressionProblem({{{1}, a, target}, {{2}, a, target}},
                                      limits);
}
void continuation(unsigned type, unsigned workers) {
  auto p = problem(type);
  SymbolicRegressionConfig c;
  c.population = 8;
  c.maxGenerationAttempts = 8;
  c.maxEvaluations = 72;
  c.maxTrainingLossAttempts = 2000;
  c.grammar.initialMaxDepth = 3;
  c.grammar.operators =
      type == 1 ? std::vector{ExpressionKind::Add, ExpressionKind::Multiply,
                              ExpressionKind::Exp}
                : std::vector{ExpressionKind::Add, ExpressionKind::Multiply};
  ConstantFitConfig fit;
  fit.maxLossAttempts = 8;
  c.fitting = fit;
  RandomContext ra(19, {workers});
  ScalarMathematicsRun run(p, c, ra);
  while (!run.stoppingReason())
    run.step();
  CheckpointDocument document;
  {
    RandomContext rb(19, {workers});
    ScalarMathematicsRun split(p, c, rb);
    while (split.algorithm().attempts() < 3 && !split.stoppingReason())
      split.step();
    document = decodeCheckpoint(encodeCheckpoint(split.checkpoint()));
  }
  RandomContext rc(19, {workers});
  auto restored = ScalarMathematicsRun::restore(document, p, c, rc);
  while (!restored->stoppingReason())
    restored->step();
  check(run.checkpoint().payload == restored->checkpoint().payload,
        "D6 full-state continuation");
  check(run.supplementalWork() == restored->supplementalWork(),
        "restored mathematical work");
  RandomContext invalid(19, {workers});
  bool rejected = false;
  try {
    ScalarMathematicsRun::restore(document, problem((type + 1) % 3), c,
                                  invalid);
  } catch (const CheckpointError &) {
    rejected = true;
  }
  check(rejected, "cross-domain identity rejection");
}
class FaultProblem final : public RegressionProblem {
public:
  std::atomic<bool> fail{false};
  FaultProblem()
      : RegressionProblem({{{1}, 2.1}, {{2}, 3.9}},
                          ExpressionLimits{15, 5, 8, 1000}) {}
  bool parallelEvaluationSafe() const noexcept override { return true; }
  RegressionMetrics
  measureWithWork(const Expression &e, std::uint64_t *rows,
                  RegressionSupplementalWork &work) const override {
    if (fail.load()) {
      work.mathematicalOperations += 7;
      if (rows)
        ++*rows;
      throw std::runtime_error("injected scientific operational cause");
    }
    return RegressionProblem::measure(e, rows);
  }
};
void publicationFailure() {
  auto initialFault = std::make_shared<FaultProblem>();
  initialFault->fail.store(true);
  auto initialProblem = ScalarMathematicsProblem::measured(
      initialFault, "failed-initial-commit/1");
  SymbolicRegressionConfig initialConfig;
  initialConfig.population = 4;
  RandomContext initialRandom(123, {2});
  bool initCounted = false;
  try {
    ScalarMathematicsRun failed(initialProblem, initialConfig, initialRandom);
  } catch (const ScientificInitializationFailure &failure) {
    initCounted = failure.work().lossAttempts > 0 &&
                  failure.supplementalWork().mathematicalOperations > 0 &&
                  failure.candidateAttempts() > 0;
  }
  check(initCounted, "initialization failure retains drained actual work "
                     "without a committed population");
  auto fault = std::make_shared<FaultProblem>();
  auto problem =
      ScalarMathematicsProblem::measured(fault, "fixed-fault-fixture/1");
  SymbolicRegressionConfig config;
  config.population = 8;
  config.maxGenerationAttempts = 3;
  config.maxEvaluations = 32;
  config.maxTrainingLossAttempts = 100;
  config.grammar.operators = {ExpressionKind::Add, ExpressionKind::Multiply};
  config.grammar.initialMaxDepth = 3;
  RandomContext random(123, {4});
  ScalarMathematicsRun run(problem, config, random);
  auto state = RunCheckpointAccess::population(run.population());
  auto before = run.work();
  fault->fail.store(true);
  bool failed = false;
  try {
    run.step();
  } catch (const std::exception &e) {
    failed = true;
    check(std::string(e.what()).size() > 0,
          "original nested operation context retained");
  }
  check(failed, "operational preparation failure propagated");
  auto after = RunCheckpointAccess::population(run.population());
  check(state.generation == after.generation &&
            state.best.text() == after.best.text(),
        "generation/archive atomic after failure");
  for (std::size_t i = 0; i < state.members.size(); ++i)
    check(state.members[i].text() == after.members[i].text() &&
              state.members[i].fitnessState() ==
                  after.members[i].fitnessState(),
          "committed population retained");
  check(run.work().lossAttempts > before.lossAttempts &&
            run.supplementalWork().mathematicalOperations > 0,
        "failed unpublished work consumed");
}
void overlap() {
  auto p = problem(1);
  auto e = parse("exp(x0*x1)", 2);
  std::barrier gate(4);
  ExecutionContext execution({4});
  execution.run(4, [&](std::size_t index) {
    gate.arrive_and_wait();
    TrainingObjective objective(p.problem(), 16);
    ConstantFitConfig fit;
    fit.maxLossAttempts = 16;
    auto r = fitConstants(e, objective, fit);
    check(r.loss.state() == FitnessState::Valid, "private complex fitting");
    auto system = oscillator();
    auto s = system.assess(std::array{.25, 2.});
    check(s.loss.has_value(), "parallel immutable system");
    auto d = evaluateSecondPartial(parse("sqrt(x0*x0)"),
                                   std::array{index % 2 ? 1. : 0.}, 0, 0);
    check(d.status == (index % 2 ? ScientificStatus::Success
                                 : ScientificStatus::Unavailable),
          "simultaneous second success/unavailable");
    tensors();
    linear();
  });
  check(execution.peakTasks() == 4, "forced D6 overlap");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc > 1 && std::string_view(argv[1]) == "overlap")
      overlap();
    else if (argc > 1 && std::string_view(argv[1]) == "continuation") {
      for (unsigned w : {1, 2, 4})
        for (unsigned t = 0; t < 3; ++t)
          continuation(t, w);
    } else {
      publicationFailure();
      differential();
      tensors();
      complexMath();
      statistics();
      linear();
      if (argc > 1)
        references(argv[1]);
    }
    std::cout << "D6 contracts passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
