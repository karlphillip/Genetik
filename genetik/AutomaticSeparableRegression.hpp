#pragma once

#include "ExpressionParsing.hpp"
#include "MultiComponentTemplateRegression.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Detects only finite-grid evidence for additive or rank-one multiplicative
// separability. A supported result is a search hypothesis, not a global proof.
enum class SeparabilityEvidence { Supported, Rejected, Uncertain };
enum class SeparableForm { Additive, Multiplicative };
enum class SeparabilityStop { CompleteGrid, InvalidInput, ResourceLimit, Degenerate, Ambiguous };

struct SeparabilityLimits {
    std::size_t maxObservations = 4096;
    std::uint64_t maxMathematicalOperations = 1'000'000;
    std::size_t maxEstimatedMemoryBytes = 1'048'576;
    double absoluteTolerance = 1e-10;
    double relativeTolerance = 1e-8;
    double minimumPivotMagnitude = 1e-12;
    // Empty preserves the historical strict detector. When set, this is an
    // independently justified absolute bound on each target measurement error.
    std::optional<double> measurementNoiseBound;
};

struct SeparabilityWork {
    std::uint64_t checkedGridEntries = 0;
    std::uint64_t mathematicalOperations = 0;
    std::size_t estimatedMemoryBytes = 0;
    bool operator==(const SeparabilityWork&) const = default;
};

struct SeparabilityDetection {
    SeparabilityStop stoppingReason = SeparabilityStop::InvalidInput;
    SeparabilityEvidence additive = SeparabilityEvidence::Uncertain;
    SeparabilityEvidence multiplicative = SeparabilityEvidence::Uncertain;
    std::optional<SeparableForm> selectedForm;
    double normalizationPivot = 0.0;
    std::size_t xLevels = 0;
    std::size_t yLevels = 0;
    double maximumAdditiveResidual = 0.0;
    double maximumMultiplicativeResidual = 0.0;
    SeparabilityWork work;
    std::string diagnostic;
    std::optional<double> assumedMeasurementNoiseBound;
    double normalizedAdditiveAcceptanceBound = 0.0;
    double normalizedMultiplicativeAcceptanceBound = 0.0;
};

namespace genetik_separability_detail {
inline void charge(SeparabilityDetection& result, const SeparabilityLimits& limits) {
    if (result.work.mathematicalOperations == limits.maxMathematicalOperations)
        throw std::length_error("separability mathematical-work limit");
    ++result.work.mathematicalOperations;
}

} // namespace genetik_separability_detail

inline SeparabilityDetection detectSeparableGrid(const std::vector<RegressionRow>& rows,
                                                 SeparabilityLimits limits = {}) {
    using namespace genetik_separability_detail;
    SeparabilityDetection result;
    const auto uncertain = [&](SeparabilityStop stop, const char* message) {
        result.stoppingReason = stop;
        result.diagnostic = message;
        return result;
    };
    if (!std::isfinite(limits.absoluteTolerance) || limits.absoluteTolerance < 0.0 ||
        !std::isfinite(limits.relativeTolerance) || limits.relativeTolerance < 0.0 ||
        limits.relativeTolerance > 1.0 || !std::isfinite(limits.minimumPivotMagnitude) ||
        limits.minimumPivotMagnitude < 0.0 || limits.maxObservations == 0 ||
        limits.maxMathematicalOperations == 0 || limits.maxEstimatedMemoryBytes == 0)
        return uncertain(SeparabilityStop::InvalidInput, "invalid detection limits");
    if (limits.measurementNoiseBound &&
        (!std::isfinite(*limits.measurementNoiseBound) || *limits.measurementNoiseBound <= 0.0))
        return uncertain(SeparabilityStop::InvalidInput,
                         "measurement noise bound must be finite and positive");
    result.assumedMeasurementNoiseBound = limits.measurementNoiseBound;
    if (rows.empty() || rows.size() > limits.maxObservations)
        return uncertain(rows.empty() ? SeparabilityStop::InvalidInput
                                      : SeparabilityStop::ResourceLimit,
                         "observation count is empty or exceeds its limit");
    constexpr std::size_t bytesPerObservation = 3 * sizeof(double) + sizeof(std::uint8_t);
    constexpr std::size_t vectorOverhead = 4 * sizeof(std::vector<double>);
    if (rows.size() >
        (std::numeric_limits<std::size_t>::max() - vectorOverhead) / bytesPerObservation)
        return uncertain(SeparabilityStop::ResourceLimit, "estimated grid memory size overflow");
    result.work.estimatedMemoryBytes = rows.size() * bytesPerObservation + vectorOverhead;
    if (result.work.estimatedMemoryBytes > limits.maxEstimatedMemoryBytes)
        return uncertain(SeparabilityStop::ResourceLimit, "estimated grid memory limit exceeded");

    std::vector<double> xs, ys;
    xs.reserve(rows.size());
    ys.reserve(rows.size());
    for (const auto& row : rows) {
        if (row.inputs.size() != 2 || !std::isfinite(row.inputs[0]) ||
            !std::isfinite(row.inputs[1]) || !std::isfinite(row.target))
            return uncertain(SeparabilityStop::InvalidInput,
                             "observations require finite x, y, and target values");
        xs.push_back(row.inputs[0]);
        ys.push_back(row.inputs[1]);
    }
    std::sort(xs.begin(), xs.end());
    std::sort(ys.begin(), ys.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
    result.xLevels = xs.size();
    result.yLevels = ys.size();
    if (xs.size() < 2 || ys.size() < 2)
        return uncertain(SeparabilityStop::Degenerate,
                         "at least two observed levels per variable are required");
    if (xs.empty() || ys.empty() || xs.size() > limits.maxObservations / ys.size() ||
        xs.size() * ys.size() != rows.size())
        return uncertain(SeparabilityStop::InvalidInput,
                         "observations are not a complete rectangular grid");

    std::vector<double> values(xs.size() * ys.size(), 0.0);
    std::vector<std::uint8_t> present(values.size(), 0);
    double scale = 0.0;
    if (rows.size() > limits.maxMathematicalOperations) {
        result.work.mathematicalOperations = limits.maxMathematicalOperations;
        result.stoppingReason = SeparabilityStop::ResourceLimit;
        result.diagnostic = "separability mathematical-work limit exhausted during grid indexing";
        return result;
    }
    result.work.mathematicalOperations =
        rows.size(); // one charged coordinate lookup per observation
    for (const auto& row : rows) {
        const auto xi = static_cast<std::size_t>(
            std::lower_bound(xs.begin(), xs.end(), row.inputs[0]) - xs.begin());
        const auto yi = static_cast<std::size_t>(
            std::lower_bound(ys.begin(), ys.end(), row.inputs[1]) - ys.begin());
        const auto index = xi * ys.size() + yi;
        if (present[index])
            return uncertain(SeparabilityStop::InvalidInput, "duplicate grid coordinate");
        present[index] = 1;
        values[index] = row.target;
        scale = std::max(scale, std::abs(row.target));
    }
    if (std::find(present.begin(), present.end(), 0) != present.end())
        return uncertain(SeparabilityStop::InvalidInput, "incomplete rectangular grid");

    try {
        const auto at = [&](std::size_t i, std::size_t j) { return values[i * ys.size() + j]; };
        result.work.checkedGridEntries = values.size();
        if (scale == 0.0) {
            result.stoppingReason = SeparabilityStop::Degenerate;
            result.additive = result.multiplicative = SeparabilityEvidence::Uncertain;
            result.diagnostic =
                "zero-valued grid has non-identifiable additive and multiplicative factors";
            return result;
        }

        // Scale before differences/products to prevent avoidable overflow.
        for (std::size_t i = 1; i < xs.size(); ++i) {
            for (std::size_t j = 1; j < ys.size(); ++j) {
                charge(result, limits);
                const double cross =
                    (at(i, j) / scale - at(i, 0) / scale) - (at(0, j) / scale - at(0, 0) / scale);
                result.maximumAdditiveResidual =
                    std::max(result.maximumAdditiveResidual, std::abs(cross));
            }
        }
        const double numericalTolerance =
            limits.absoluteTolerance / scale + limits.relativeTolerance;
        const double roundoffAllowance =
            limits.measurementNoiseBound ? 64.0 * std::numeric_limits<double>::epsilon() : 0.0;
        const double normalizedNoise =
            limits.measurementNoiseBound ? *limits.measurementNoiseBound / scale : 0.0;
        const double additiveTolerance =
            numericalTolerance + roundoffAllowance + 4.0 * normalizedNoise;
        result.normalizedAdditiveAcceptanceBound = additiveTolerance;
        const double additiveUncertaintyLimit =
            limits.measurementNoiseBound ? 10.0 * additiveTolerance : 10.0 * numericalTolerance;
        if (!std::isfinite(additiveTolerance) || additiveTolerance > 0.1)
            result.additive = SeparabilityEvidence::Uncertain;
        else if (result.maximumAdditiveResidual <= additiveTolerance)
            result.additive = SeparabilityEvidence::Supported;
        else if (result.maximumAdditiveResidual <= additiveUncertaintyLimit)
            result.additive = SeparabilityEvidence::Uncertain;
        else
            result.additive = SeparabilityEvidence::Rejected;

        // The largest-magnitude entry is a deterministic, well-conditioned
        // normalization pivot. Ties retain the lexicographically first point.
        std::size_t pivotI = 0, pivotJ = 0;
        double pivotAbs = -1.0;
        for (std::size_t i = 0; i < xs.size(); ++i) {
            for (std::size_t j = 0; j < ys.size(); ++j) {
                charge(result, limits);
                if (std::abs(at(i, j)) > pivotAbs) {
                    pivotI = i;
                    pivotJ = j;
                    pivotAbs = std::abs(at(i, j));
                }
            }
        }
        result.normalizationPivot = at(pivotI, pivotJ);
        if (pivotAbs <= limits.minimumPivotMagnitude) {
            result.multiplicative = SeparabilityEvidence::Uncertain;
            result.stoppingReason = SeparabilityStop::Degenerate;
            result.diagnostic = "multiplicative normalization pivot is zero or too small";
        } else {
            for (std::size_t i = 0; i < xs.size(); ++i) {
                for (std::size_t j = 0; j < ys.size(); ++j) {
                    charge(result, limits);
                    const double a = at(i, j) / scale;
                    const double b = at(i, pivotJ) / scale;
                    const double c = at(pivotI, j) / scale;
                    const double d = at(pivotI, pivotJ) / scale;
                    const double residual = std::abs(a * d - b * c);
                    result.maximumMultiplicativeResidual =
                        std::max(result.maximumMultiplicativeResidual, residual);
                }
            }
            const double multiplicativeNumericalTolerance = numericalTolerance + roundoffAllowance;
            // If every observed entry differs from an unknown rank-one entry by
            // at most epsilon, each normalized 2x2 minor differs by no more
            // than 4*epsilon*(M_observed+epsilon)+2*epsilon^2. The normalized
            // observed maximum is one, so M_observed <= 1.
            const double multiplicativeNoiseTolerance =
                4.0 * normalizedNoise * (1.0 + normalizedNoise) +
                2.0 * normalizedNoise * normalizedNoise;
            const double multiplicativeTolerance =
                multiplicativeNumericalTolerance + multiplicativeNoiseTolerance;
            result.normalizedMultiplicativeAcceptanceBound = multiplicativeTolerance;
            const double multiplicativeUncertaintyLimit =
                limits.measurementNoiseBound ? 10.0 * multiplicativeTolerance
                                             : 10.0 * multiplicativeNumericalTolerance;
            if (!std::isfinite(multiplicativeTolerance) || multiplicativeTolerance > 0.1)
                result.multiplicative = SeparabilityEvidence::Uncertain;
            else if (result.maximumMultiplicativeResidual <= multiplicativeTolerance)
                result.multiplicative = SeparabilityEvidence::Supported;
            else if (result.maximumMultiplicativeResidual <= multiplicativeUncertaintyLimit)
                result.multiplicative = SeparabilityEvidence::Uncertain;
            else
                result.multiplicative = SeparabilityEvidence::Rejected;
        }
    } catch (const std::length_error&) {
        result.stoppingReason = SeparabilityStop::ResourceLimit;
        result.additive = result.multiplicative = SeparabilityEvidence::Uncertain;
        result.selectedForm.reset();
        result.diagnostic = "separability mathematical-work limit exhausted";
        return result;
    }

    if (result.stoppingReason != SeparabilityStop::Degenerate)
        result.stoppingReason = SeparabilityStop::CompleteGrid;
    if (result.additive == SeparabilityEvidence::Supported &&
        result.multiplicative == SeparabilityEvidence::Rejected)
        result.selectedForm = SeparableForm::Additive;
    else if (result.multiplicative == SeparabilityEvidence::Supported &&
             result.additive == SeparabilityEvidence::Rejected)
        result.selectedForm = SeparableForm::Multiplicative;
    else if (result.additive == SeparabilityEvidence::Supported &&
             result.multiplicative == SeparabilityEvidence::Supported) {
        result.stoppingReason = SeparabilityStop::Ambiguous;
        result.diagnostic = "both hypotheses fit finite observations; decomposition is ambiguous";
    } else if (result.diagnostic.empty()) {
        result.diagnostic = result.additive == SeparabilityEvidence::Uncertain ||
                                    result.multiplicative == SeparabilityEvidence::Uncertain
                                ? "one or more hypotheses remain numerically uncertain"
                                : "neither supported separability hypothesis fits the grid";
    }
    return result;
}

struct AutomaticSeparableConfig {
    SeparabilityLimits detection;
    ExpressionLimits modelLimits{31, 10, 8.0, 40};
    std::array<ExpressionLimits, 2> componentLimits{ExpressionLimits{15, 5, 8.0, 20},
                                                    ExpressionLimits{15, 5, 8.0, 20}};
    SymbolicRegressionConfig search;
    std::array<ExpressionGrammar, 2> grammars;
};

struct AutomaticSeparableResult {
    SeparabilityDetection detection;
    std::optional<SymbolicRegressionResult> search;
    std::shared_ptr<const MultiComponentTemplateProblem> domain;
};

// Detection always precedes template creation. The held-out rows are not an
// argument: callers assess those only after this function returns.
inline AutomaticSeparableResult
automaticSeparableRegression(const std::vector<RegressionRow>& training,
                             const AutomaticSeparableConfig& config, RandomContext& random) {
    AutomaticSeparableResult result;
    result.detection = detectSeparableGrid(training, config.detection);
    if (!result.detection.selectedForm)
        return result;
    const bool additive = *result.detection.selectedForm == SeparableForm::Additive;
    const auto source = parseExpression(additive ? "x0+x1" : "x0*x1", 2, {}, config.modelLimits);
    MultiScalarExpressionTemplate scaffold(source, 0, 1, config.componentLimits,
                                           config.modelLimits);
    auto problem = RegressionProblem::create(training, config.modelLimits);
    const std::array<std::vector<ExpressionKind>, 2> operators{config.grammars[0].operators,
                                                               config.grammars[1].operators};
    result.domain = MultiComponentTemplateProblem::create(scaffold, std::move(problem),
                                                          {{{0}, {1}}}, operators);
    result.search =
        multiComponentTemplateRegression(result.domain, config.grammars, config.search, random);
    return result;
}
