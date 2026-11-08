#include <AutomaticSeparableRegression.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}

std::vector<RegressionRow> grid(const auto& function, bool noisy = false) {
    std::vector<RegressionRow> rows;
    std::size_t xi = 0;
    for (const double x : {-1.0, -0.25, 0.5, 1.0}) {
        std::size_t yi = 0;
        for (const double y : {-0.75, 0.0, 0.5, 1.25}) {
            double value = function(x, y);
            if (noisy)
                value += ((xi + yi) % 2 ? 1.0 : -1.0) * 1e-4;
            rows.push_back({{x, y}, value});
            ++yi;
        }
        ++xi;
    }
    return rows;
}

void hypotheses() {
    const auto additive =
        detectSeparableGrid(grid([](double x, double y) { return std::sin(x) + y * y; }));
    check(additive.selectedForm == SeparableForm::Additive, "additive grid detection");
    check(additive.additive == SeparabilityEvidence::Supported &&
              additive.multiplicative == SeparabilityEvidence::Rejected,
          "additive and multiplicative evidence remain distinct");
    check(additive.work.checkedGridEntries == 16 && additive.work.mathematicalOperations > 0,
          "detector reports grid and arithmetic work");

    const auto product =
        detectSeparableGrid(grid([](double x, double y) { return (1.0 + x) * (1.0 + y * y); }));
    check(product.selectedForm == SeparableForm::Multiplicative, "multiplicative grid detection");
    check(std::abs(product.normalizationPivot) >= 1.0,
          "normalization uses deterministic maximum-magnitude pivot");

    const auto sumOfInputs =
        detectSeparableGrid(grid([](double x, double y) { return x + y + x * y; }));
    check(!sumOfInputs.selectedForm && sumOfInputs.additive == SeparabilityEvidence::Rejected &&
              sumOfInputs.multiplicative == SeparabilityEvidence::Rejected,
          "x+y+xy is outside both supported hypotheses");
    const auto composed =
        detectSeparableGrid(grid([](double x, double y) { return std::sin(x + y); }));
    check(!composed.selectedForm && composed.additive == SeparabilityEvidence::Rejected &&
              composed.multiplicative == SeparabilityEvidence::Rejected,
          "sin(x+y) is outside both supported hypotheses");
    const auto constant = detectSeparableGrid(grid([](double, double) { return 2.0; }));
    check(constant.stoppingReason == SeparabilityStop::Ambiguous && !constant.selectedForm &&
              constant.additive == SeparabilityEvidence::Supported &&
              constant.multiplicative == SeparabilityEvidence::Supported,
          "constant grid does not select an unidentified decomposition");
}

void uncertaintyAndInputContracts() {
    const auto noisy =
        detectSeparableGrid(grid([](double x, double y) { return std::sin(x) + y * y; }, true));
    check(!noisy.selectedForm, "noisy cross differences do not select a construction");

    const std::vector<RegressionRow> duplicate{
        {{-1.0, -1.0}, 0.0}, {{-1.0, 1.0}, 0.0}, {{1.0, -1.0}, 0.0}, {{-1.0, -1.0}, 0.0}};
    check(detectSeparableGrid(duplicate).stoppingReason == SeparabilityStop::InvalidInput,
          "duplicate grid coordinates are malformed");
    const auto zeroGrid = detectSeparableGrid(grid([](double, double) { return 0.0; }));
    check(zeroGrid.stoppingReason == SeparabilityStop::Degenerate && !zeroGrid.selectedForm,
          "zero-valued grid is non-identifiable");
    const auto oneDimensional =
        detectSeparableGrid(std::vector<RegressionRow>{{{1.0, 0.0}, 1.0}, {{1.0, 2.0}, 3.0}});
    check(oneDimensional.stoppingReason == SeparabilityStop::Degenerate &&
              !oneDimensional.selectedForm,
          "one observed level in one variable is non-identifiable");

    auto smallPivotLimits = SeparabilityLimits{};
    smallPivotLimits.minimumPivotMagnitude = 100.0;
    const auto smallPivot = detectSeparableGrid(
        grid([](double x, double y) { return (1.0 + x) * (1.0 + y); }), smallPivotLimits);
    check(!smallPivot.selectedForm && smallPivot.multiplicative == SeparabilityEvidence::Uncertain,
          "near-zero normalization threshold prevents multiplicative classification");

    auto incomplete = grid([](double x, double y) { return x + y; });
    incomplete.pop_back();
    check(detectSeparableGrid(incomplete).stoppingReason == SeparabilityStop::InvalidInput,
          "incomplete grid rejected");
    check(detectSeparableGrid(std::vector<RegressionRow>{{{0.0}, 1.0}}).stoppingReason ==
              SeparabilityStop::InvalidInput,
          "incorrect observation dimension rejected");
    check(detectSeparableGrid(
              std::vector<RegressionRow>{{{0.0, 0.0}, std::numeric_limits<double>::infinity()}})
                  .stoppingReason == SeparabilityStop::InvalidInput,
          "nonfinite target rejected");
    auto budget = SeparabilityLimits{};
    budget.maxMathematicalOperations = 4;
    const auto exhausted =
        detectSeparableGrid(grid([](double x, double y) { return x + y; }), budget);
    check(exhausted.stoppingReason == SeparabilityStop::ResourceLimit && !exhausted.selectedForm &&
              exhausted.work.mathematicalOperations == 4,
          "detection work limit is explicit and exact");
    auto memory = SeparabilityLimits{};
    memory.maxEstimatedMemoryBytes = 1;
    const auto memoryLimited =
        detectSeparableGrid(grid([](double x, double y) { return x + y; }), memory);
    check(memoryLimited.stoppingReason == SeparabilityStop::ResourceLimit &&
              !memoryLimited.selectedForm && memoryLimited.work.estimatedMemoryBytes > 1,
          "estimated memory limit is enforced before grid allocation");
}

void boundedNoiseDetection() {
    auto limits = SeparabilityLimits{};
    limits.measurementNoiseBound = 1e-4;
    const auto noisyAdditive = detectSeparableGrid(
        grid([](double x, double y) { return std::sin(x) + y * y; }, true), limits);
    check(noisyAdditive.selectedForm == SeparableForm::Additive &&
              noisyAdditive.additive == SeparabilityEvidence::Supported &&
              noisyAdditive.multiplicative == SeparabilityEvidence::Rejected &&
              noisyAdditive.assumedMeasurementNoiseBound == 1e-4 &&
              noisyAdditive.normalizedAdditiveAcceptanceBound >
                  noisyAdditive.maximumAdditiveResidual,
          "declared bounded noise supports additive cross-difference consistency");

    const auto noisyProduct = detectSeparableGrid(
        grid([](double x, double y) { return (1.0 + x) * (1.0 + y * y); }, true), limits);
    check(noisyProduct.selectedForm == SeparableForm::Multiplicative &&
              noisyProduct.multiplicative == SeparabilityEvidence::Supported &&
              noisyProduct.additive == SeparabilityEvidence::Rejected &&
              noisyProduct.normalizedMultiplicativeAcceptanceBound >
                  noisyProduct.maximumMultiplicativeResidual,
          "declared bounded noise supports multiplicative rank-one consistency");

    auto strict = limits;
    strict.measurementNoiseBound.reset();
    check(!detectSeparableGrid(grid([](double x, double y) { return std::sin(x) + y * y; }, true),
                               strict)
               .selectedForm,
          "strict default remains unchanged for noisy observations");

    const auto weaklyNonseparable = detectSeparableGrid(
        grid([](double x, double y) { return x + y + 1e-4 * x * y; }, true), limits);
    check(weaklyNonseparable.selectedForm == SeparableForm::Additive &&
              weaklyNonseparable.additive == SeparabilityEvidence::Supported,
          "bounded noise reports compatibility, not proof, when a weak interaction is below its "
          "envelope");

    const auto ambiguous =
        detectSeparableGrid(grid([](double, double) { return 2.0; }, true), limits);
    check(!ambiguous.selectedForm && ambiguous.stoppingReason == SeparabilityStop::Ambiguous,
          "noise-consistent additive and product forms remain ambiguous");

    auto invalidBound = limits;
    invalidBound.measurementNoiseBound = 0.0;
    check(detectSeparableGrid(grid([](double x, double y) { return x + y; }), invalidBound)
                  .stoppingReason == SeparabilityStop::InvalidInput,
          "noise-aware mode requires a positive explicit error bound");
    invalidBound.measurementNoiseBound = std::numeric_limits<double>::infinity();
    check(detectSeparableGrid(grid([](double x, double y) { return x + y; }), invalidBound)
                  .stoppingReason == SeparabilityStop::InvalidInput,
          "nonfinite measurement error bounds are rejected");

    auto uninformativeBound = limits;
    uninformativeBound.measurementNoiseBound = 10.0;
    const auto uninformative =
        detectSeparableGrid(grid([](double x, double y) { return x + y; }), uninformativeBound);
    check(!uninformative.selectedForm &&
              uninformative.additive == SeparabilityEvidence::Uncertain &&
              uninformative.multiplicative == SeparabilityEvidence::Uncertain,
          "uninformative error bounds cannot create a decomposition selection");
}

void deterministicAndAutomaticConstruction() {
    const auto rows = grid([](double x, double y) { return std::sin(x) + y * y; });
    const auto first = detectSeparableGrid(rows);
    const auto second = detectSeparableGrid(rows);
    auto reorderedRows = rows;
    std::reverse(reorderedRows.begin(), reorderedRows.end());
    const auto reordered = detectSeparableGrid(reorderedRows);
    check(first.selectedForm == second.selectedForm && first.work == second.work &&
              first.maximumAdditiveResidual == second.maximumAdditiveResidual &&
              first.maximumMultiplicativeResidual == second.maximumMultiplicativeResidual &&
              first.selectedForm == reordered.selectedForm && first.work == reordered.work &&
              first.maximumAdditiveResidual == reordered.maximumAdditiveResidual &&
              first.maximumMultiplicativeResidual == reordered.maximumMultiplicativeResidual,
          "detection is deterministic and independent of row order");

    AutomaticSeparableConfig config;
    config.grammars[0].operators = {ExpressionKind::Sin, ExpressionKind::Add,
                                    ExpressionKind::Multiply};
    config.grammars[0].constants = {-1.0, 0.0, 1.0};
    config.grammars[1].operators = {ExpressionKind::Add, ExpressionKind::Multiply,
                                    ExpressionKind::Powi};
    config.grammars[1].constants = {-1.0, 0.0, 1.0};
    config.grammars[1].exponents = {2};
    config.grammars[0].initialMaxDepth = config.grammars[1].initialMaxDepth = 2;
    config.grammars[0].mutationMaxDepth = config.grammars[1].mutationMaxDepth = 2;
    config.search.population = 8;
    config.search.elites = 1;
    config.search.maxEvaluations = 8;
    config.search.maxTrainingLossAttempts = 8 * rows.size();
    RandomContext random(31);
    const auto result = automaticSeparableRegression(rows, config, random);
    check(result.detection.selectedForm == SeparableForm::Additive && result.domain &&
              result.search,
          "supported data triggers existing two-site template construction and search");
    const auto parts = result.domain->components(result.search->best->expression());
    check(parts[0].variableCount() == 2 && parts[1].variableCount() == 2,
          "both unknown components retain the caller's variable schema");

    const auto productRows = grid([](double x, double y) { return (1.0 + x) * (1.0 + y * y); });
    RandomContext productRandom(32);
    const auto productResult = automaticSeparableRegression(productRows, config, productRandom);
    check(productResult.detection.selectedForm == SeparableForm::Multiplicative &&
              productResult.domain && productResult.search,
          "multiplicative evidence constructs the product scaffold");
    const auto modelLimits = productResult.domain->expressionTemplate().modelLimits();
    const auto scaffoldModel = productResult.domain->materialize(
        {parseExpression("x0", 2, {}, modelLimits), parseExpression("x1", 2, {}, modelLimits)});
    check(scaffoldModel.evaluate(std::array<double, 2>{0.5, 0.5}).value() == 0.25,
          "automatic multiplicative template preserves its fixed product scaffold");
}
} // namespace

int main() {
    try {
        hypotheses();
        uncertaintyAndInputContracts();
        boundedNoiseDetection();
        deterministicAndAutomaticConstruction();
        std::cout << "automatic separability contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
