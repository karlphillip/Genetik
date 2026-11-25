#pragma once

#include "Regression.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

struct ConservationTrajectory {
    std::vector<std::vector<double>> states;
};

struct ConservationObjectiveConfig {
    double minimumMagnitude = 1e-12;
    double minimumNormalizedSeparation = 1e-12;
    double minimumBetweenFraction = 0.01;
    double complexityPenalty = 1e-6;
};

struct ConservationObjectiveMetrics {
    Fitness score = Fitness::invalid();
    std::optional<double> withinFraction;
    std::optional<double> betweenFraction;
    std::optional<double> separation;
    std::size_t observationsAttempted = 0;
};

// Group labels are retained only for scoring. Expressions see ordered state
// coordinates, never trajectory IDs, time, initial labels, or reference laws.
class ConservationDiscoveryProblem final : public RegressionProblem {
  public:
    ConservationDiscoveryProblem(const std::vector<ConservationTrajectory>& trajectories,
                                 ExpressionLimits limits = {63, 8, 1e6},
                                 ConservationObjectiveConfig config = {})
        : RegressionProblem(MeasurementSchema{2, limits}), trajectories_(trajectories),
          config_(config) {
        if (trajectories_.size() < 2 || !std::isfinite(config_.minimumMagnitude) ||
            config_.minimumMagnitude <= 0.0 ||
            !std::isfinite(config_.minimumNormalizedSeparation) ||
            config_.minimumNormalizedSeparation <= 0.0 ||
            !std::isfinite(config_.minimumBetweenFraction) ||
            config_.minimumBetweenFraction <= 0.0 || config_.minimumBetweenFraction >= 1.0 ||
            !std::isfinite(config_.complexityPenalty) || config_.complexityPenalty < 0.0)
            throw std::invalid_argument("invalid conservation objective configuration/data");
        for (const auto& trajectory : trajectories_) {
            if (trajectory.states.size() < 2)
                throw std::invalid_argument("each conservation trajectory requires multiple states");
            if (trajectory.states.size() > std::numeric_limits<std::size_t>::max() - sampleCount_)
                throw std::length_error("conservation observation count overflow");
            sampleCount_ += trajectory.states.size();
            for (const auto& state : trajectory.states) {
                if (state.size() != 2 || !std::isfinite(state[0]) || !std::isfinite(state[1]))
                    throw std::invalid_argument("conservation states must be finite (y,v) pairs");
            }
        }
    }

    bool parallelEvaluationSafe() const noexcept override { return true; }
    std::size_t sampleCount() const noexcept override { return sampleCount_; }

    ConservationObjectiveMetrics assess(const Expression& expression,
                                        std::uint64_t* rowAttempts = nullptr,
                                        RegressionSupplementalWork* supplemental = nullptr) const {
        const auto checked = checkedExpression(expression);
        ConservationObjectiveMetrics result;
        std::vector<std::vector<double>> values;
        values.reserve(trajectories_.size());
        double magnitude = 0.0;
        genetik_detail::EvaluationWorkspace workspace;
        for (const auto& trajectory : trajectories_) {
            auto& group = values.emplace_back();
            group.reserve(trajectory.states.size());
            for (const auto& state : trajectory.states) {
                if (rowAttempts) {
                    if (*rowAttempts == UINT64_MAX)
                        throw std::overflow_error("conservation row counter overflow");
                    ++*rowAttempts;
                }
                ++result.observationsAttempted;
                const auto prediction = workspace.evaluate(checked, state);
                if (supplemental) {
                    if (prediction.arithmeticOperations() > UINT64_MAX - supplemental->mathematicalOperations ||
                        supplemental->fieldEvaluations == UINT64_MAX)
                        throw std::overflow_error("conservation work counter overflow");
                    supplemental->mathematicalOperations += prediction.arithmeticOperations();
                    ++supplemental->fieldEvaluations;
                }
                if (!prediction.valid())
                    return result;
                magnitude = std::max(magnitude, std::abs(prediction.value()));
                group.push_back(prediction.value());
            }
        }
        if (!std::isfinite(magnitude) || magnitude < config_.minimumMagnitude)
            return result;

        // Divide before moments so finite large or tiny candidate scales cannot
        // overflow or underflow the normalized objective arithmetic.
        std::vector<double> means(trajectories_.size(), 0.0);
        double grandSum = 0.0;
        std::size_t count = 0;
        for (std::size_t group = 0; group < values.size(); ++group) {
            double sum = 0.0;
            for (double& value : values[group]) {
                value /= magnitude;
                volatile double next = sum + value;
                if (!std::isfinite(next)) return result;
                sum = next;
                ++count;
            }
            means[group] = sum / static_cast<double>(values[group].size());
            volatile double nextGrand = grandSum + means[group] *
                                                   static_cast<double>(values[group].size());
            if (!std::isfinite(nextGrand)) return result;
            grandSum = nextGrand;
        }
        const double grandMean = grandSum / static_cast<double>(count);
        double within = 0.0, between = 0.0;
        for (std::size_t group = 0; group < values.size(); ++group) {
            for (const double value : values[group]) {
                const double delta = value - means[group];
                volatile double next = within + delta * delta;
                if (!std::isfinite(next)) return result;
                within = next;
            }
            const double delta = means[group] - grandMean;
            volatile double next = between + static_cast<double>(values[group].size()) * delta * delta;
            if (!std::isfinite(next)) return result;
            between = next;
        }
        const double total = within + between;
        if (!std::isfinite(total) || !(total > 0.0))
            return result;
        const double withinFraction = within / total;
        const double betweenFraction = between / total;
        if (!std::isfinite(withinFraction) || !std::isfinite(betweenFraction) ||
            between <= 0.0 || betweenFraction < config_.minimumBetweenFraction ||
            std::sqrt(between / static_cast<double>(count)) <
                config_.minimumNormalizedSeparation)
            return result;
        const double complexity = config_.complexityPenalty *
                                  static_cast<double>(checked.nodeCount());
        const double score = withinFraction + complexity;
        if (!std::isfinite(score))
            return result;
        result.score = Fitness::valid(score);
        result.withinFraction = withinFraction;
        result.betweenFraction = betweenFraction;
        result.separation = std::sqrt(between / static_cast<double>(count));
        return result;
    }

    RegressionMetrics measure(const Expression& expression,
                              std::uint64_t* rowAttempts = nullptr) const override {
        const auto metrics = assess(expression, rowAttempts);
        return {metrics.score, {}, metrics.observationsAttempted};
    }

    RegressionMetrics measureWithWork(const Expression& expression, std::uint64_t* rowAttempts,
                                      RegressionSupplementalWork& supplemental) const override {
        const auto metrics = assess(expression, rowAttempts, &supplemental);
        return {metrics.score, {}, metrics.observationsAttempted};
    }

    const std::vector<ConservationTrajectory>& trajectories() const noexcept {
        return trajectories_;
    }

  private:
    std::vector<ConservationTrajectory> trajectories_;
    ConservationObjectiveConfig config_;
    std::size_t sampleCount_ = 0;
};
