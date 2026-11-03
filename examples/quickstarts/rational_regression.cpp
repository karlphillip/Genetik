#include <RationalFunctionRegression.hpp>
#include <array>
#include <iostream>
#include <vector>

int main() {
    std::vector<RationalObservation> training;
    for (double x : std::array{-1.0, -0.5, 0.0, 0.5, 1.0, 1.5})
        training.push_back({x, (1.4 * x + 0.2) / (x * x + 0.8)});

    RationalConfig config;
    config.maximumNumeratorDegree = 2;
    config.maximumDenominatorDegree = 2;
    config.refineWithLM = true;
    const auto fit = fitRationalFunction(training, config);
    if (fit.status != RationalStatus::Success || !fit.selected) return 1;

    const auto& model = fit.models[*fit.selected];
    const auto point = predictRationalChecked(model, 0.25, config.minimumAbsoluteDenominator);
    const std::array<double, 2> domainSamples{-1.0, 1.5};
    const auto domain = assessRationalDomain(
        model, domainSamples, {-1.0, 1.5}, config.minimumAbsoluteDenominator);
    std::cout << "selected degree (" << model.numeratorDegree << ','
              << model.denominatorDegree << "), training SSE=" << model.trainingSSE
              << ", point status=" << rationalPredictionStatusName(point.status)
              << ", interval status=" << domainStatusName(domain.status) << '\n';
    std::cout << "A successful fit does not establish safety outside this checked interval.\n";
}
