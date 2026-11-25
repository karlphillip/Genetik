#include <PolynomialBounds.hpp>
#include <MathematicalConstraints.hpp>

#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
Expression multiply(const Expression& a, const Expression& b) {
    return Expression::binary(ExpressionKind::Multiply, a, b);
}
}

int main() {
    try {
        const auto x = Expression::variable(0);
        const auto x3 = multiply(multiply(x, x), x);
        const auto trap = Expression::binary(
            ExpressionKind::Subtract, x3,
            multiply(Expression::constant(0.25, 1), x));
        const FiniteBox unit{{-1.0}, {1.0}};

        PolynomialBoundRequest trapValue;
        trapValue.valueRange = std::pair{-1.0, 1.0};
        const auto value = assessPolynomialBounds(trap, unit, trapValue);
        check(value.status == PolynomialCertificateStatus::Certified &&
                  value.subdivisions > 0 && value.valueEnclosure &&
                  value.valueEnclosure->first >= -1.0 &&
                  value.valueEnclosure->second <= 1.0,
              "outward interval subdivision certifies the complete value range");

        PolynomialBoundRequest monotonicity;
        monotonicity.derivativeInput = 0;
        monotonicity.derivativeRange = std::pair{0.0, 3.0};
        const auto falseMonotone = assessPolynomialBounds(trap, unit, monotonicity);
        check(falseMonotone.status == PolynomialCertificateStatus::Violated &&
                  falseMonotone.verifiedWitness && falseMonotone.witnessDerivative &&
                  *falseMonotone.witnessDerivative < 0.0 &&
                  falseMonotone.witnessDerivativeEnclosure &&
                  falseMonotone.witnessDerivativeEnclosure->second < 0.0,
              "midpoint derivative witness defeats endpoint-only monotonicity sampling");
        MathematicalConstraints candidateAssessment;
        const auto assessedCandidate =
            candidateAssessment.assessWholeBox(trap, unit, monotonicity);
        check(assessedCandidate.status == ConstraintStatus::Violated &&
                  assessedCandidate.wholeBoxCertificate &&
                  !assessedCandidate.sampledOnly,
              "candidate assessment exposes and preserves the whole-box certificate result");

        const auto control = Expression::binary(ExpressionKind::Add, x, x3);
        PolynomialBoundRequest positive;
        positive.valueRange = std::pair{-2.1, 2.1};
        positive.derivativeInput = 0;
        positive.derivativeRange = std::pair{0.5, 5.0};
        const auto certified = assessPolynomialBounds(control, unit, positive);
        check(certified.status == PolynomialCertificateStatus::Certified &&
                  certified.derivativeEnclosure && certified.derivativeEnclosure->first >= 0.5,
              "positive monotonicity control is certified across the full box");

        const auto singular = Expression::unary(ExpressionKind::Log, x);
        const auto unsupported = assessPolynomialBounds(singular, unit, monotonicity);
        check(unsupported.status == PolynomialCertificateStatus::Inconclusive &&
                  unsupported.reason == "unsupported_operator_or_expression",
              "unsupported singular expression remains explicitly inconclusive");
        std::cout << "polynomial bound certificate contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
