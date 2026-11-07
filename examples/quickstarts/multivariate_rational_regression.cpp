#include <MultivariateRationalFunctionRegression.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace mr = genetik::multivariate_rational;

int main() {
    // The fitter receives only observations; it is not given the equation or
    // any coefficient vector.
    std::vector<mr::Observation> training;
    for (double x : std::array{-0.6, -0.4, -0.2, 0.0, 0.2, 0.4, 0.6}) {
        for (double y : std::array{-0.6, -0.4, -0.2, 0.0, 0.2, 0.4, 0.6}) {
            const double target = (0.7 + 0.4*x - 0.3*y + 0.2*x*y) /
                                  (1.0 + 0.12*x - 0.08*y + 0.06*x*y);
            training.push_back({x, y, target});
        }
    }

    mr::Config config; // QR only is the default; LM requires explicit opt-in.
    config.maximumNumeratorDegree = 2;
    config.maximumDenominatorDegree = 2;
    const auto result = mr::fit(training, config);
    if (result.status != mr::Status::Success || !result.selected) {
        std::cerr << "fit failed: " << mr::statusName(result.status)
                  << " (" << result.detail << ")\n";
        return 1;
    }
    const auto& model = result.models[*result.selected];
    if (!mr::modelIsConsistent(model)) {
        std::cerr << "selected model failed its representation consistency check\n";
        return 1;
    }

    const std::array<double, 2> heldOutPoint{0.15, -0.25};
    const double reference = (0.7 + 0.4*heldOutPoint[0] - 0.3*heldOutPoint[1] +
                              0.2*heldOutPoint[0]*heldOutPoint[1]) /
                             (1.0 + 0.12*heldOutPoint[0] - 0.08*heldOutPoint[1] +
                              0.06*heldOutPoint[0]*heldOutPoint[1]);
    const auto expressionValue = model.expression.evaluate(heldOutPoint);
    const auto prediction = model.predict(heldOutPoint[0], heldOutPoint[1],
                                          config.minimumAbsoluteDenominator);
    const bool heldOutAgrees = expressionValue.valid() && prediction &&
        std::abs(expressionValue.value() - reference) < 1e-9 &&
        std::abs(*prediction - reference) < 1e-9;

    const auto domain = mr::assessDomain(model, {-0.5, 0.5, -0.5, 0.5});
    std::cout << "selected numerator/denominator degrees: " << model.numeratorDegree
              << '/' << model.denominatorDegree << '\n'
              << "expression: " << model.expression.toString() << '\n'
              << "training SSE: " << model.trainingSSE << '\n'
              << "held-out prediction: " << (heldOutAgrees ? "passed" : "failed")
              << " (reference=" << reference << ")\n"
              << "domain: " << static_cast<int>(domain.status) << " / "
              << static_cast<int>(domain.finding) << " (" << domain.reason << ")\n"
              << "QR arithmetic work: " << result.arithmeticOperations
              << ", linear-algebra work: " << result.linearAlgebraOperations
              << ", estimated peak workspace: " << result.peakWorkspaceEstimateBytes
              << " bytes\n";
    if (!heldOutAgrees) return 2;
    if (domain.status != mr::DomainAssessmentStatus::Valid) {
        std::cout << "The requested box is not certified pole-free; do not infer safety.\n";
    }
    return 0;
}
