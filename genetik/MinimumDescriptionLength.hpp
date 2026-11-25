#pragma once

#include "Regression.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Opt-in deterministic two-part code. This is a fixed-precision integer code,
// not a Gaussian maximum-likelihood criterion.
struct MinimumDescriptionLengthConfig {
    double coefficientPrecision = 1e-6;
    double residualPrecision = 1e-6;
    std::size_t maximumCandidates = 256;
    std::uint64_t maximumRowEvaluations = 1'000'000;
    std::uint64_t maximumExpressionOperations = 100'000'000;
    std::uint64_t maximumBits = 10'000'000;
    std::size_t maximumEstimatedMemoryBytes = 64 * 1024 * 1024;
};

enum class MinimumDescriptionLengthStatus {
    Success,
    NoCandidates,
    UnsupportedDataModel,
    InvalidModel,
    InvalidPrediction,
    ResourceExhausted,
    NumericalFailure
};

struct MinimumDescriptionLengthCandidate {
    Expression expression;
};

struct MinimumDescriptionLengthCandidateResult {
    MinimumDescriptionLengthStatus status = MinimumDescriptionLengthStatus::InvalidModel;
    std::optional<std::uint64_t> modelBits;
    std::optional<std::uint64_t> residualBits;
    std::optional<std::uint64_t> totalBits;
    std::size_t structuralNodes = 0;
};

struct MinimumDescriptionLengthResult {
    static constexpr std::string_view formulation =
        "genetik-mdl-fixed-precision-signed-elias-gamma/1";
    MinimumDescriptionLengthStatus status = MinimumDescriptionLengthStatus::NoCandidates;
    std::optional<std::size_t> selectedIndex;
    std::optional<Expression> selectedModel;
    std::vector<MinimumDescriptionLengthCandidateResult> candidates;
    std::uint64_t rowEvaluations = 0;
    std::uint64_t expressionOperations = 0;
};

namespace genetik_mdl_detail {
inline std::uint64_t gammaBits(std::uint64_t positive) {
    if (!positive)
        throw std::invalid_argument("Elias gamma code requires a positive integer");
    std::uint64_t width = 0;
    for (; positive; positive >>= 1)
        ++width;
    return 2 * width - 1;
}

inline std::uint64_t signedCode(double value, double precision) {
    const double scaled = value / precision;
    if (!std::isfinite(scaled) || std::abs(scaled) >= std::ldexp(1.0, 63))
        throw std::overflow_error("fixed-precision value exceeds signed code range");
    const auto quantized = static_cast<std::int64_t>(std::round(scaled));
    const auto magnitude = static_cast<std::uint64_t>(quantized < 0 ? -quantized : quantized);
    return quantized < 0 ? 2 * magnitude : 2 * magnitude + 1;
}

inline std::uint64_t checkedAdd(std::uint64_t a, std::uint64_t b) {
    if (b > UINT64_MAX - a)
        throw std::overflow_error("MDL bit count overflow");
    return a + b;
}
} // namespace genetik_mdl_detail

inline MinimumDescriptionLengthResult
selectMinimumDescriptionLength(const RegressionProblem& problem,
                               std::span<const MinimumDescriptionLengthCandidate> candidates,
                               const MinimumDescriptionLengthConfig& config = {}) {
    if (!std::isfinite(config.coefficientPrecision) || config.coefficientPrecision <= 0.0 ||
        !std::isfinite(config.residualPrecision) || config.residualPrecision <= 0.0 ||
        !config.maximumCandidates || !config.maximumRowEvaluations ||
        !config.maximumExpressionOperations || !config.maximumBits ||
        !config.maximumEstimatedMemoryBytes)
        throw std::invalid_argument("invalid minimum-description-length configuration");

    MinimumDescriptionLengthResult result;
    if (candidates.empty()) {
        result.status = MinimumDescriptionLengthStatus::NoCandidates;
        return result;
    }
    if (typeid(problem) != typeid(RegressionProblem) ||
        problem.objective().loss != RegressionLoss::MeanSquaredError ||
        problem.trainingRows().empty()) {
        result.status = MinimumDescriptionLengthStatus::UnsupportedDataModel;
        return result;
    }
    if (candidates.size() > config.maximumCandidates ||
        candidates.size() >
            config.maximumEstimatedMemoryBytes / sizeof(MinimumDescriptionLengthCandidateResult)) {
        result.status = MinimumDescriptionLengthStatus::ResourceExhausted;
        return result;
    }
    result.candidates.reserve(candidates.size());
    bool anyValid = false;
    std::optional<std::size_t> winner;
    for (std::size_t ci = 0; ci < candidates.size(); ++ci) {
        auto& scored = result.candidates.emplace_back();
        const auto& input = candidates[ci].expression;
        scored.structuralNodes = input.nodeCount();
        constexpr std::size_t bytesPerNode = sizeof(ExpressionNode) + 4 * sizeof(double);
        const auto resultBytes =
            candidates.size() * sizeof(MinimumDescriptionLengthCandidateResult);
        if (input.nodeCount() > (config.maximumEstimatedMemoryBytes - resultBytes) / bytesPerNode) {
            result.status = MinimumDescriptionLengthStatus::ResourceExhausted;
            result.selectedIndex.reset();
            result.selectedModel.reset();
            return result;
        }
        std::optional<Expression> checkedStorage;
        try {
            checkedStorage.emplace(problem.checkedExpression(input));
        } catch (const std::invalid_argument&) {
            scored.status = MinimumDescriptionLengthStatus::InvalidModel;
            continue;
        } catch (const std::length_error&) {
            scored.status = MinimumDescriptionLengthStatus::InvalidModel;
            continue;
        }
        const auto& checked = *checkedStorage;
        auto nodes = checked.nodes();
        std::uint64_t modelBits = 8; // versioned model-code header
        bool supported = true;
        try {
            for (std::size_t n = 0; n < nodes.size(); ++n) {
                const auto kind = nodes[n].kind();
                modelBits = genetik_mdl_detail::checkedAdd(modelBits, 3); // node tag
                if (kind == ExpressionKind::Variable) {
                    modelBits = genetik_mdl_detail::checkedAdd(
                        modelBits, genetik_mdl_detail::gammaBits(nodes[n].variableIndex() + 1));
                } else if (kind == ExpressionKind::Constant) {
                    const auto code = genetik_mdl_detail::signedCode(nodes[n].constantValue(),
                                                                     config.coefficientPrecision);
                    const auto q =
                        static_cast<std::int64_t>((code & 1) ? (code - 1) / 2 : -(code / 2));
                    const auto reconstructed = static_cast<double>(q) * config.coefficientPrecision;
                    if (!std::isfinite(reconstructed) ||
                        std::abs(reconstructed) > checked.limits().maxAbsConstant)
                        throw std::overflow_error(
                            "quantized coefficient exceeds expression limits");
                    nodes[n] = ExpressionNode::constant(reconstructed);
                    modelBits = genetik_mdl_detail::checkedAdd(modelBits,
                                                               genetik_mdl_detail::gammaBits(code));
                } else if (kind != ExpressionKind::Add && kind != ExpressionKind::Subtract &&
                           kind != ExpressionKind::Multiply) {
                    supported = false;
                    break;
                }
            }
            if (!supported) {
                scored.status = MinimumDescriptionLengthStatus::InvalidModel;
                continue;
            }
            const Expression quantized(nodes, checked.variableCount(), checked.limits());
            if (modelBits > config.maximumBits) {
                result.status = MinimumDescriptionLengthStatus::ResourceExhausted;
                result.selectedIndex.reset();
                result.selectedModel.reset();
                return result;
            }
            const auto rows = problem.trainingRows().size();
            if (rows > config.maximumRowEvaluations - result.rowEvaluations ||
                quantized.arithmeticCost() > UINT64_MAX / rows ||
                quantized.arithmeticCost() * rows >
                    config.maximumExpressionOperations - result.expressionOperations) {
                result.status = MinimumDescriptionLengthStatus::ResourceExhausted;
                result.selectedIndex.reset();
                result.selectedModel.reset();
                return result;
            }
            std::uint64_t residualBits = 0;
            genetik_detail::EvaluationWorkspace workspace;
            bool valid = true;
            for (const auto& row : problem.trainingRows()) {
                ++result.rowEvaluations;
                const auto prediction = workspace.evaluate(quantized, row.inputs);
                result.expressionOperations = genetik_mdl_detail::checkedAdd(
                    result.expressionOperations, prediction.arithmeticOperations());
                if (!prediction.valid()) {
                    valid = false;
                    break;
                }
                volatile double residual = row.target - prediction.value();
                if (!std::isfinite(residual)) {
                    valid = false;
                    break;
                }
                const auto code =
                    genetik_mdl_detail::signedCode(residual, config.residualPrecision);
                residualBits = genetik_mdl_detail::checkedAdd(residualBits,
                                                              genetik_mdl_detail::gammaBits(code));
                if (residualBits > config.maximumBits) {
                    result.status = MinimumDescriptionLengthStatus::ResourceExhausted;
                    result.selectedIndex.reset();
                    result.selectedModel.reset();
                    return result;
                }
            }
            if (!valid) {
                scored.status = MinimumDescriptionLengthStatus::InvalidPrediction;
                continue;
            }
            const auto total = genetik_mdl_detail::checkedAdd(modelBits, residualBits);
            if (total > config.maximumBits) {
                result.status = MinimumDescriptionLengthStatus::ResourceExhausted;
                result.selectedIndex.reset();
                result.selectedModel.reset();
                return result;
            }
            scored.status = MinimumDescriptionLengthStatus::Success;
            scored.modelBits = modelBits;
            scored.residualBits = residualBits;
            scored.totalBits = total;
            anyValid = true;
            if (!winner || total < *result.candidates[*winner].totalBits ||
                (total == *result.candidates[*winner].totalBits &&
                 (scored.structuralNodes < result.candidates[*winner].structuralNodes ||
                  (scored.structuralNodes == result.candidates[*winner].structuralNodes &&
                   input.toString() < candidates[*winner].expression.toString())))) {
                winner = ci;
                result.selectedModel = quantized;
            }
        } catch (const std::invalid_argument&) {
            scored.status = MinimumDescriptionLengthStatus::InvalidModel;
        } catch (const std::length_error&) {
            scored.status = MinimumDescriptionLengthStatus::InvalidModel;
        } catch (const std::overflow_error&) {
            scored.status = MinimumDescriptionLengthStatus::NumericalFailure;
        }
    }
    result.status = anyValid ? MinimumDescriptionLengthStatus::Success
                             : MinimumDescriptionLengthStatus::InvalidModel;
    result.selectedIndex = winner;
    return result;
}
