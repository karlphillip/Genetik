#include <ExpressionDerivative.hpp>
#include <ExpressionParsing.hpp>
#include <ExpressionSecondDerivative.hpp>
#include <ExpressionVariation.hpp>
#include <PhysicalDimensions.hpp>
#include <ScalarExpressionCheckpoint.hpp>
#include <SymbolicAlgebra.hpp>
#include <TypedExpression.hpp>
#include <TypedExpressionParsing.hpp>
#include <TypedVariation.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

void scalarValuesAndDomains() {
    struct Case {
        const char* source;
        double input;
        double expected;
    };
    const std::array cases{
        Case{"asinh(x0)", 1.0, std::log(1.0 + std::sqrt(2.0))},
        Case{"acosh(x0)", 2.0, std::log(2.0 + std::sqrt(3.0))},
        Case{"atanh(x0)", 0.5, 0.5 * std::log(3.0)},
        Case{"sigmoid(x0)", 0.0, 0.5},
        Case{"softplus(x0)", 0.0, std::log(2.0)},
        Case{"sinc(x0)", 0.0, 1.0},
        Case{"sinc(x0)", -0.0, 1.0},
    };
    for (const auto& item : cases) {
        const auto expression = parseExpression(item.source, 1);
        const auto result = expression.evaluate(std::array{item.input});
        check(result.valid(), "supported smooth input evaluates");
        check(std::abs(result.value() - item.expected) <= 2e-15, "independent scalar reference");
        check(result.arithmeticOperations() == 1, "one charged scalar unary operation");
        check(parseExpression(expression.toString(), 1).evaluate(std::array{item.input}).value() ==
                  result.value(),
              "operator export reparses identically");
    }
    for (const auto& [source, input] :
         std::array<std::pair<const char*, double>, 4>{{{"acosh(x0)", std::nextafter(1.0, 0.0)},
                                                        {"atanh(x0)", 1.0},
                                                        {"atanh(x0)", -1.0},
                                                        {"atanh(x0)", std::nextafter(1.0, 2.0)}}})
        check(!parseExpression(source, 1).evaluate(std::array{input}).valid(),
              "strict mathematical domain");

    const auto positive =
        parseExpression("sigmoid(x0)+softplus(x1)", 2).evaluate(std::array{1000.0, 1000.0});
    check(positive.valid() && positive.value() == 1001.0, "positive-tail stable evaluation");
    const auto negative =
        parseExpression("sigmoid(x0)+softplus(x1)", 2).evaluate(std::array{-1000.0, -1000.0});
    check(negative.valid() && negative.value() == 0.0, "negative-tail stable evaluation");
    const auto large = parseExpression("asinh(x0)", 1).evaluate(std::array{1e300});
    check(large.valid() && std::isfinite(large.value()), "asinh avoids square overflow");
    const auto magnitude = parseExpression("hypot(x0,x1)", 2);
    check(magnitude.evaluate(std::array{3.0, 4.0}).value() == 5.0,
          "hypot binary scalar evaluation");
    check(magnitude.evaluate(std::array{1e200, 1e200}).value() == std::hypot(1e200, 1e200),
          "hypot avoids square overflow");
    check(magnitude.evaluate(std::array{1e-200, 1e-200}).value() == std::hypot(1e-200, 1e-200),
          "hypot avoids square underflow");
    check(magnitude.evaluate(std::array{0.0, -0.0}).value() == 0.0, "hypot handles signed zeros");
    check(!magnitude
               .evaluate(std::array{std::numeric_limits<double>::max(),
                                    std::numeric_limits<double>::max()})
               .valid(),
          "hypot still rejects an unrepresentable result");
    check(parseExpression(magnitude.toString(), 2).sameRepresentation(magnitude),
          "hypot export and parsing round trip");
}

void derivatives() {
    struct Case {
        const char* source;
        double x;
        double expected;
    };
    const std::array cases{
        Case{"asinh(x0)", 2.0, 1.0 / std::sqrt(5.0)},
        Case{"acosh(x0)", 2.0, 1.0 / std::sqrt(3.0)},
        Case{"atanh(x0)", 0.5, 4.0 / 3.0},
        Case{"sigmoid(x0)", 0.0, 0.25},
        Case{"softplus(x0)", 0.0, 0.5},
        Case{"sinc(x0)", 0.0, 0.0},
        Case{"sinc(x0)", 1e-4, -1e-4 / 3.0},
        Case{"sinc(x0)", 1.0, std::cos(1.0) - std::sin(1.0)},
    };
    for (const auto& item : cases) {
        const auto result = evaluatePartial(parseExpression(item.source, 1), std::array{item.x}, 0);
        check(result.derivativeAvailable(), "smooth derivative available");
        check(std::abs(result.derivative() - item.expected) <= 2e-12,
              "independent derivative reference");
        check(result.work().operations() <= 1000000, "derivative work stays within default limit");
    }
    const auto endpoint = evaluatePartial(parseExpression("acosh(x0)", 1), std::array{1.0}, 0);
    check(!endpoint.derivativeAvailable() &&
              endpoint.derivativeFailure() == DerivativeFailure::SingularRule,
          "acosh boundary derivative is singular");
    const auto edge =
        evaluatePartial(parseExpression("atanh(x0)", 1), std::array{std::nextafter(1.0, 0.0)}, 0);
    check(edge.derivativeAvailable() && std::isfinite(edge.derivative()),
          "atanh interior derivative remains finite");
    const auto hypot = parseExpression("hypot(x0,x1)", 2);
    const auto hx = evaluatePartial(hypot, std::array{3.0, 4.0}, 0);
    const auto hy = evaluatePartial(hypot, std::array{3.0, 4.0}, 1);
    check(hx.derivativeAvailable() && hy.derivativeAvailable() && hx.derivative() == 0.6 &&
              hy.derivative() == 0.8,
          "hypot analytic first partials");
    const double denorm = std::numeric_limits<double>::denorm_min();
    const auto tinyX = evaluatePartial(hypot, std::array{denorm, denorm}, 0);
    const auto tinyY = evaluatePartial(hypot, std::array{denorm, denorm}, 1);
    const double diagonalPartial = 1.0 / std::sqrt(2.0);
    check(tinyX.derivativeAvailable() && std::abs(tinyX.derivative() - diagonalPartial) <= 2e-15,
          "hypot x partial remains accurate at the subnormal diagonal");
    check(tinyY.derivativeAvailable() && std::abs(tinyY.derivative() - diagonalPartial) <= 2e-15,
          "hypot y partial remains accurate at the subnormal diagonal");
    for (std::size_t target = 0; target < 2; ++target) {
        const auto symbolic = SymbolicExpression::fromExpression(hypot).differentiate(target);
        check(symbolic.status == SymbolicStatus::Success && symbolic.expression,
              "subnormal hypot symbolic partial is constructed");
        const auto executable = symbolic.expression->toExecutable().expression;
        const auto value = executable.evaluate(std::array{denorm, denorm});
        check(value.valid() && std::abs(value.value() - diagonalPartial) <= 2e-15,
              "exported subnormal hypot partial preserves direction");
        const auto direct = evaluatePartial(hypot, std::array{denorm, denorm}, target);
        check(direct.derivativeAvailable() && value.valid() && value.value() == direct.derivative(),
              "exported and direct subnormal hypot partials agree");
    }
    for (int exponent : {-1074, -500, 0, 500, 1023}) {
        const double coordinate = std::scalbn(1.0, exponent);
        const double expected = 1.0 / std::sqrt(2.0);
        for (std::size_t target = 0; target < 2; ++target) {
            const auto symbolic = SymbolicExpression::fromExpression(hypot).differentiate(target);
            check(symbolic.status == SymbolicStatus::Success && symbolic.expression,
                  "scaled hypot symbolic partial is constructed");
            const auto executable = symbolic.expression->toExecutable().expression;
            const auto value = executable.evaluate(std::array{coordinate, coordinate});
            check(value.valid() && std::abs(value.value() - expected) <= 2e-15,
                  "exported hypot first partial is invariant under power-of-two scaling");
        }
    }
    const auto singular = evaluatePartial(hypot, std::array{0.0, -0.0}, 0);
    check(singular.valueAvailable() && !singular.derivativeAvailable() &&
              singular.derivativeFailure() == DerivativeFailure::SingularRule,
          "hypot origin primal is valid and derivative is singular");

    struct CurvatureCase {
        const char* source;
        double x;
        double expected;
    };
    const std::array curvature{
        CurvatureCase{"asinh(x0)", 0.25, -0.25 / std::pow(1.0 + 0.25 * 0.25, 1.5)},
        CurvatureCase{"acosh(x0)", 2.0, -2.0 / std::pow(3.0, 1.5)},
        CurvatureCase{"atanh(x0)", 0.25, 0.5 / std::pow(1.0 - 0.25 * 0.25, 2.0)},
        CurvatureCase{"sigmoid(x0)", 0.0, 0.0},
        CurvatureCase{"softplus(x0)", 0.0, 0.25},
        CurvatureCase{"sinc(x0)", 0.0, -1.0 / 3.0},
        CurvatureCase{"sinc(x0)", 0.25,
                      -std::sin(0.25) / 0.25 - 2.0 * std::cos(0.25) / (0.25 * 0.25) +
                          2.0 * std::sin(0.25) / (0.25 * 0.25 * 0.25)},
    };
    for (const auto& item : curvature) {
        const auto result =
            evaluateSecondPartial(parseExpression(item.source, 1), std::array{item.x}, 0, 0);
        check(result.status == ScientificStatus::Success && result.derivatives.has_value(),
              "smooth second derivative available");
        check(std::abs(result.derivatives->second - item.expected) < 2e-11,
              "independent curvature reference");
    }
    const auto hxx = evaluateSecondPartial(hypot, std::array{3.0, 4.0}, 0, 0);
    const auto hxy = evaluateSecondPartial(hypot, std::array{3.0, 4.0}, 0, 1);
    check(hxx.status == ScientificStatus::Success && hxx.derivatives &&
              std::abs(hxx.derivatives->second - 16.0 / 125.0) < 1e-15,
          "hypot second x partial");
    check(hxy.status == ScientificStatus::Success && hxy.derivatives &&
              std::abs(hxy.derivatives->second + 12.0 / 125.0) < 1e-15,
          "hypot mixed second partial");
    const auto thinCurvature = evaluateSecondPartial(hypot, std::array{1e-300, 1e-100}, 1, 1);
    check(thinCurvature.status == ScientificStatus::Success && thinCurvature.derivatives &&
              thinCurvature.derivatives->second > 0.0 &&
              std::abs(thinCurvature.derivatives->second / 1e-300 - 1.0) < 2e-15,
          "hypot small positive second partial survives intermediate underflow");
    const auto denormXX = evaluateSecondPartial(hypot, std::array{denorm, 0.0}, 0, 0);
    const auto denormXY = evaluateSecondPartial(hypot, std::array{denorm, 0.0}, 0, 1);
    const auto swappedXX = evaluateSecondPartial(hypot, std::array{0.0, denorm}, 1, 1);
    const auto swappedXY = evaluateSecondPartial(hypot, std::array{0.0, denorm}, 1, 0);
    check(denormXX.status == ScientificStatus::Success && denormXX.derivatives &&
              denormXX.derivatives->second == 0.0 && denormXY.status == ScientificStatus::Success &&
              denormXY.derivatives && denormXY.derivatives->second == 0.0,
          "requested zero hypot curvatures do not evaluate unrelated entries");
    check(swappedXX.status == ScientificStatus::Success && swappedXX.derivatives &&
              swappedXX.derivatives->second == 0.0 &&
              swappedXY.status == ScientificStatus::Success && swappedXY.derivatives &&
              swappedXY.derivatives->second == 0.0,
          "hypot zero curvature results are symmetric under axis swap");
    const auto nearOverflowXX = evaluateSecondPartial(hypot, std::array{2e-309, 2e-309}, 0, 0);
    const auto nearOverflowYY = evaluateSecondPartial(hypot, std::array{2e-309, 2e-309}, 1, 1);
    const double nearOverflowExpected = 1.0 / (2.0 * std::sqrt(2.0) * 2e-309);
    check(nearOverflowXX.status == ScientificStatus::Success && nearOverflowXX.derivatives &&
              std::abs(nearOverflowXX.derivatives->second / nearOverflowExpected - 1.0) < 2e-15 &&
              nearOverflowYY.status == ScientificStatus::Success && nearOverflowYY.derivatives &&
              std::abs(nearOverflowYY.derivatives->second / nearOverflowExpected - 1.0) < 2e-15,
          "hypot near-overflow curvature uses scaled intermediates on both axes");
    for (int exponent : {-500, 0, 500}) {
        const double coordinate = std::scalbn(1.0, exponent);
        const double expected = 1.0 / (2.0 * std::sqrt(2.0) * coordinate);
        const auto xx = evaluateSecondPartial(hypot, std::array{coordinate, coordinate}, 0, 0);
        const auto yy = evaluateSecondPartial(hypot, std::array{coordinate, coordinate}, 1, 1);
        check(xx.status == ScientificStatus::Success && xx.derivatives &&
                  std::abs(xx.derivatives->second / expected - 1.0) < 3e-15 &&
                  yy.status == ScientificStatus::Success && yy.derivatives &&
                  std::abs(yy.derivatives->second / expected - 1.0) < 3e-15,
              "hypot curvature scales consistently by powers of two");
    }
    const auto hOrigin = evaluateSecondPartial(hypot, std::array{0.0, 0.0}, 0, 0);
    check(hOrigin.status == ScientificStatus::Unavailable,
          "hypot origin second partial is unavailable");

    const auto sameInputHypot = parseExpression("hypot(x0,x0)", 1);
    const auto symbolicHypot = SymbolicExpression::fromExpression(sameInputHypot).differentiate(0);
    check(symbolicHypot.status == SymbolicStatus::Success && symbolicHypot.expression,
          "hypot symbolic derivative is available away from the origin");
    const auto exportedDerivative = symbolicHypot.expression->toExecutable().expression;
    const double large = 1e308;
    const auto exportedValue = exportedDerivative.evaluate(std::array{large});
    const auto directValue = evaluatePartial(sameInputHypot, std::array{large}, 0);
    check(exportedValue.valid() && std::abs(exportedValue.value() / std::sqrt(2.0) - 1.0) < 2e-15 &&
              directValue.derivativeAvailable() &&
              std::abs(exportedValue.value() - directValue.derivative()) < 2e-15,
          "exported hypot derivative avoids overflow at large equal inputs");
}

void dimensionsAndSymbolics() {
    const std::array dimensionful{PhysicalDimension::base(SIBaseDimension::Length)};
    const std::array dimensionless{PhysicalDimension{}};
    for (const char* name : {"asinh", "acosh", "atanh", "sigmoid", "softplus", "sinc"}) {
        const auto expression = parseExpression(std::string(name) + "(x0)", 1);
        check(inferDimensions(expression, dimensionless).back().dimensionless(),
              "dimensionless smooth output");
        bool rejected = false;
        try {
            (void)inferDimensions(expression, dimensionful);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "dimensionful smooth input rejected");
    }
    const std::array lengths{PhysicalDimension::base(SIBaseDimension::Length),
                             PhysicalDimension::base(SIBaseDimension::Length)};
    const auto lengthNorm = parseExpression("hypot(x0,x1)", 2);
    check(inferDimensions(lengthNorm, lengths).back() == lengths[0],
          "hypot preserves equal operand dimension");
    const std::array mismatched{lengths[0], PhysicalDimension::base(SIBaseDimension::Time)};
    bool mismatchRejected = false;
    try {
        (void)inferDimensions(lengthNorm, mismatched);
    } catch (const std::invalid_argument&) {
        mismatchRejected = true;
    }
    check(mismatchRejected, "hypot rejects unlike dimensions");

    const auto sigmoid = parseExpression("sigmoid(x0)", 1);
    const auto symbolic = SymbolicExpression::fromExpression(sigmoid).differentiate(0);
    check(symbolic.status == SymbolicStatus::Success, "symbolic derivative construction");
    check(symbolic.expression.has_value(), "symbolic derivative expression");
    const auto executable = symbolic.expression->toExecutable();
    const auto numeric = executable.expression.evaluate(std::array{0.25});
    const auto direct = evaluatePartial(sigmoid, std::array{0.25}, 0);
    check(numeric.valid() && std::abs(numeric.value() - direct.derivative()) < 1e-15,
          "symbolic derivative agrees with direct evaluation");

    const auto hypotSource = parseExpression("hypot(x0,x1)", 2);
    const auto hypotSymbolic = SymbolicExpression::fromExpression(hypotSource).differentiate(0);
    check(hypotSymbolic.status == SymbolicStatus::Success && hypotSymbolic.expression,
          "symbolic hypot partial built away from origin");
    const auto hypotExecutable = hypotSymbolic.expression->toExecutable();
    const auto hypotValue = hypotExecutable.expression.evaluate(std::array{3.0, 4.0});
    check(hypotValue.valid() && std::abs(hypotValue.value() - 0.6) < 1e-15,
          "symbolic and numeric hypot partial agree");
    for (const auto& [name, point] :
         std::array<std::pair<const char*, double>, 6>{{{"asinh", 0.25},
                                                        {"acosh", 2.0},
                                                        {"atanh", 0.25},
                                                        {"sigmoid", 0.25},
                                                        {"softplus", 0.25},
                                                        {"sinc", 0.25}}}) {
        const auto source = parseExpression(std::string(name) + "(x0)", 1);
        const auto transformed = SymbolicExpression::fromExpression(source).differentiate(0);
        check(transformed.status == SymbolicStatus::Success && transformed.expression.has_value(),
              "symbolic smooth derivative is constructed");
        const auto materialized = transformed.expression->toExecutable();
        const auto symbolicValue = materialized.expression.evaluate(std::array{point});
        const auto directValue = evaluatePartial(source, std::array{point}, 0);
        check(symbolicValue.valid() && directValue.derivativeAvailable() &&
                  std::abs(symbolicValue.value() - directValue.derivative()) < 2e-12,
              "symbolic and numeric smooth derivatives agree away from singularities");
    }
}

void optInAndTypedParsing() {
    const ExpressionGrammar scalarGrammar;
    check(std::find(scalarGrammar.operators.begin(), scalarGrammar.operators.end(),
                    ExpressionKind::Sinc) == scalarGrammar.operators.end(),
          "smooth operators absent from default scalar grammar");
    check(std::find(scalarGrammar.operators.begin(), scalarGrammar.operators.end(),
                    ExpressionKind::Hypot) == scalarGrammar.operators.end(),
          "hypot absent from default scalar grammar");
    TypedGrammar grammar;
    check(std::find(grammar.operators.begin(), grammar.operators.end(),
                    TypedExpressionKind::Asinh) == grammar.operators.end(),
          "smooth operators absent from default typed grammar");
    grammar.operators.push_back(TypedExpressionKind::Softplus);
    const auto schema = std::array{TypedShape::scalar()};
    const auto expression = parseTypedExpression("softplus(x0)", schema, TypedShape::scalar());
    const auto result = expression.evaluate(std::array{TypedValue::scalar(0.0)});
    check(result.valid() && std::abs(result.value().coefficients()[0] - std::log(2.0)) < 1e-15,
          "explicit typed grammar enables smooth operation");
}

void checkpointRoundTrip() {
    const auto problem = RegressionProblem::create({{{2.0}, 0.0}}, {32, 8, 1024.0});
    for (const char* source :
         {"asinh(x0)", "acosh(x0)", "atanh(x0)", "sigmoid(x0)", "softplus(x0)", "sinc(x0)"}) {
        ExpressionCandidate original(parseExpression(source, 1, {}, problem->limits()), problem);
        original.calculateFitness();
        CheckpointWriter writer;
        CandidateCheckpointCodec<ExpressionCandidate>::encode(writer, original);
        CheckpointReader reader(writer.data());
        const auto restored = CandidateCheckpointCodec<ExpressionCandidate>::decode(
            reader, ScalarCheckpointContext{problem, {}});
        reader.finish();
        check(restored.expression().sameRepresentation(original.expression()),
              "smooth operator checkpoint round trip preserves representation");
    }
}
} // namespace

int main() {
    try {
        scalarValuesAndDomains();
        derivatives();
        dimensionsAndSymbolics();
        optInAndTypedParsing();
        checkpointRoundTrip();
        std::cout << "smooth operator contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
