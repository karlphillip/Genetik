#pragma once
#include "ScientificWork.hpp"
#include <numeric>

struct FiniteStatistics {
  double mean = 0, variance = 0;
};
// Two passes, mean accumulated as x/N to avoid a gratuitous overflowing sum.
inline ScientificResult<FiniteStatistics>
finiteStatistics(std::span<const double> x, std::size_t degreesOfFreedom = 0,
                 ScientificLimits limits = {},
                 const std::atomic<bool> *cancelled = nullptr) {
  if (x.empty() || degreesOfFreedom >= x.size())
    throw std::invalid_argument(
        "empty statistics or invalid degrees of freedom");
  return scientificCalculation<FiniteStatistics>(
      limits, cancelled, [&](auto &b) {
        b.storage(x.size(), sizeof(double));
        b.charge(x.size());
        genetik_science::inputs(x);
        double mean = 0, s = 0;
        for (double v : x)
          mean = b.add(mean, b.div(v, static_cast<double>(x.size())));
        for (double v : x) {
          auto d = b.sub(v, mean);
          s = b.add(s, b.mul(d, d));
        }
        return FiniteStatistics{
            mean, b.div(s, static_cast<double>(x.size() - degreesOfFreedom))};
      });
}
inline ScientificResult<double>
finiteCovariance(std::span<const double> x, std::span<const double> y,
                 std::size_t degreesOfFreedom = 0, bool correlation = false,
                 ScientificLimits limits = {},
                 const std::atomic<bool> *cancelled = nullptr) {
  if (x.size() != y.size() || x.empty() || degreesOfFreedom >= x.size())
    throw std::invalid_argument("covariance schema/dof");
  return scientificCalculation<double>(limits, cancelled, [&](auto &b) {
    b.storage(x.size(), 2 * sizeof(double));
    b.charge(x.size());
    genetik_science::inputs(x);
    b.charge(y.size());
    genetik_science::inputs(y);
    double a = 0, c = 0, s = 0, vx = 0, vy = 0;
    for (std::size_t i = 0; i < x.size(); ++i) {
      a = b.add(a, b.div(x[i], static_cast<double>(x.size())));
      c = b.add(c, b.div(y[i], static_cast<double>(y.size())));
    }
    for (std::size_t i = 0; i < x.size(); ++i) {
      double dx = b.sub(x[i], a), dy = b.sub(y[i], c);
      s = b.add(s, b.mul(dx, dy));
      vx = b.add(vx, b.mul(dx, dx));
      vy = b.add(vy, b.mul(dy, dy));
    }
    if (!correlation)
      return b.div(s, static_cast<double>(x.size() - degreesOfFreedom));
    if (vx == 0 || vy == 0)
      throw genetik_science::Stop{ScientificStatus::Invalid,
                                  "correlation of zero variance"};
    b.charge(2);
    return b.div(b.div(s, std::sqrt(vx)), std::sqrt(vy));
  });
}
inline ScientificResult<double>
finiteWeightedMean(std::span<const double> x, std::span<const double> weights,
                   bool probabilities = false, ScientificLimits limits = {},
                   const std::atomic<bool> *cancelled = nullptr) {
  if (x.empty() || x.size() != weights.size())
    throw std::invalid_argument("weighted schema");
  return scientificCalculation<double>(limits, cancelled, [&](auto &b) {
    b.storage(x.size(), 2 * sizeof(double));
    b.charge(x.size());
    genetik_science::inputs(x);
    b.charge(weights.size());
    genetik_science::inputs(weights);
    for (double w : weights)
      if (w < 0)
        throw std::invalid_argument("negative weight");
    double maximum = *std::max_element(weights.begin(), weights.end());
    if (maximum == 0)
      throw genetik_science::Stop{ScientificStatus::Invalid,
                                  "all zero weights"};
    double sum = 0, total = 0;
    for (double w : weights)
      total = b.add(total, b.div(w, maximum));
    if (probabilities) {
      double ordinary = b.mul(maximum, total);
      if (std::abs(b.sub(ordinary, 1)) > limits.tolerance)
        throw genetik_science::Stop{ScientificStatus::Invalid,
                                    "probabilities must sum to one"};
    }
    for (std::size_t i = 0; i < x.size(); ++i) {
      double w = b.div(b.div(weights[i], maximum), total);
      if (weights[i] > 0 && w == 0)
        throw genetik_science::Stop{ScientificStatus::NumericalFailure,
                                    "weight normalization underflow"};
      sum = b.add(sum, b.mul(w, x[i]));
    }
    return sum;
  });
}
