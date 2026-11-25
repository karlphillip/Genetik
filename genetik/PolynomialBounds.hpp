#pragma once

#include "ExpressionDerivative.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

struct FiniteBox {
    std::vector<double> lower;
    std::vector<double> upper;
};

struct PolynomialBoundRequest {
    std::optional<std::pair<double, double>> valueRange;
    std::optional<std::size_t> derivativeInput;
    std::optional<std::pair<double, double>> derivativeRange;
    std::size_t maximumSubdivisions = 4096;
};

enum class PolynomialCertificateStatus { Certified, Violated, Inconclusive };

struct PolynomialBoundCertificate {
    PolynomialCertificateStatus status = PolynomialCertificateStatus::Inconclusive;
    std::optional<std::pair<double, double>> valueEnclosure;
    std::optional<std::pair<double, double>> derivativeEnclosure;
    std::optional<std::vector<double>> verifiedWitness;
    std::optional<double> witnessValue;
    std::optional<double> witnessDerivative;
    std::optional<std::pair<double, double>> witnessValueEnclosure;
    std::optional<std::pair<double, double>> witnessDerivativeEnclosure;
    std::size_t subdivisions = 0;
    std::string reason;
};

namespace genetik_bounds_detail {
struct Interval {
    double lo, hi;
};
struct DualInterval {
    Interval value;
    Interval derivative;
};
inline double down(double x) {
    return std::isfinite(x) ? std::nextafter(x, -std::numeric_limits<double>::infinity()) : x;
}
inline double up(double x) {
    return std::isfinite(x) ? std::nextafter(x, std::numeric_limits<double>::infinity()) : x;
}
inline Interval add(Interval a, Interval b) {
    const double lo = a.lo + b.lo, hi = a.hi + b.hi;
    return std::isnan(lo) || std::isnan(hi)
               ? Interval{-std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::infinity()}
               : Interval{down(lo), up(hi)};
}
inline Interval subtract(Interval a, Interval b) {
    const double lo = a.lo - b.hi, hi = a.hi - b.lo;
    return std::isnan(lo) || std::isnan(hi)
               ? Interval{-std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::infinity()}
               : Interval{down(lo), up(hi)};
}
inline Interval multiply(Interval a, Interval b) {
    const std::array<double, 4> p{a.lo * b.lo, a.lo * b.hi, a.hi * b.lo, a.hi * b.hi};
    if (std::any_of(p.begin(), p.end(), [](double x) { return std::isnan(x); }))
        return {-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
    const auto [mn, mx] = std::minmax_element(p.begin(), p.end());
    return {down(*mn), up(*mx)};
}
inline bool within(Interval x, std::pair<double, double> requested) {
    return x.lo >= requested.first && x.hi <= requested.second;
}
inline bool outside(double x, std::pair<double, double> requested) {
    return x < requested.first || x > requested.second;
}

inline std::optional<DualInterval> enclose(const Expression& expression,
                                            const FiniteBox& box,
                                            std::optional<std::size_t> derivativeInput) {
    std::vector<DualInterval> stack;
    stack.reserve(expression.nodeCount());
    for (const auto& node : expression.nodes()) {
        if (node.kind() == ExpressionKind::Variable) {
            const auto i = node.variableIndex();
            stack.push_back({{box.lower[i], box.upper[i]},
                             derivativeInput && *derivativeInput == i ? Interval{1, 1}
                                                                       : Interval{0, 0}});
            continue;
        }
        if (node.kind() == ExpressionKind::Constant) {
            stack.push_back({{node.constantValue(), node.constantValue()}, {0, 0}});
            continue;
        }
        if (node.kind() != ExpressionKind::Add && node.kind() != ExpressionKind::Subtract &&
            node.kind() != ExpressionKind::Multiply)
            return std::nullopt;
        if (stack.size() < 2)
            return std::nullopt;
        const auto right = stack.back(); stack.pop_back();
        const auto left = stack.back(); stack.pop_back();
        if (node.kind() == ExpressionKind::Add) {
            stack.push_back({add(left.value, right.value), add(left.derivative, right.derivative)});
        } else if (node.kind() == ExpressionKind::Subtract) {
            stack.push_back({subtract(left.value, right.value),
                             subtract(left.derivative, right.derivative)});
        } else {
            const auto derivative = add(multiply(left.derivative, right.value),
                                        multiply(left.value, right.derivative));
            stack.push_back({multiply(left.value, right.value), derivative});
        }
    }
    if (stack.size() != 1)
        return std::nullopt;
    return stack.back();
}
} // namespace genetik_bounds_detail

inline PolynomialBoundCertificate assessPolynomialBounds(
    const Expression& expression, const FiniteBox& box,
    const PolynomialBoundRequest& request, DerivativeLimits derivativeLimits = {}) {
    PolynomialBoundCertificate result;
    if (request.maximumSubdivisions > 1'000'000)
        throw std::invalid_argument("polynomial certificate subdivision ceiling exceeded");
    if (box.lower.size() != expression.variableCount() || box.upper.size() != expression.variableCount() ||
        box.lower.size() > 16 ||
        (request.derivativeInput && *request.derivativeInput >= expression.variableCount()) ||
        (!request.valueRange && !request.derivativeRange) || !request.maximumSubdivisions ||
        (request.derivativeRange && !request.derivativeInput) ||
        (request.valueRange && (!std::isfinite(request.valueRange->first) ||
                                !std::isfinite(request.valueRange->second) ||
                                request.valueRange->first > request.valueRange->second)) ||
        (request.derivativeRange && (!std::isfinite(request.derivativeRange->first) ||
                                     !std::isfinite(request.derivativeRange->second) ||
                                     request.derivativeRange->first > request.derivativeRange->second))) {
        result.reason = "invalid_box_or_bound_request";
        return result;
    }
    for (std::size_t i = 0; i < box.lower.size(); ++i) {
        if (!std::isfinite(box.lower[i]) || !std::isfinite(box.upper[i]) ||
            box.lower[i] > box.upper[i]) {
            result.reason = "invalid_finite_box";
            return result;
        }
    }
    const auto rootEnclosure =
        genetik_bounds_detail::enclose(expression, box, request.derivativeInput);
    if (!rootEnclosure) {
        result.reason = "unsupported_operator_or_expression";
        return result;
    }
    std::vector<FiniteBox> pending{box};
    bool allCertified = true;
    genetik_bounds_detail::Interval totalValue{
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    genetik_bounds_detail::Interval totalDerivative = totalValue;
    while (!pending.empty()) {
        auto cell = std::move(pending.back());
        pending.pop_back();
        const auto enclosure = genetik_bounds_detail::enclose(expression, cell,
                                                               request.derivativeInput);
        if (!enclosure) {
            result.reason = "unsupported_operator_or_expression";
            return result;
        }
        std::vector<double> midpoint(cell.lower.size());
        for (std::size_t i = 0; i < midpoint.size(); ++i)
            midpoint[i] = std::midpoint(cell.lower[i], cell.upper[i]);
        auto pointBox = FiniteBox{midpoint, midpoint};
        const auto point = genetik_bounds_detail::enclose(expression, pointBox,
                                                          request.derivativeInput);
        const bool valueViolation = point && request.valueRange &&
            (point->value.lo > request.valueRange->second ||
             point->value.hi < request.valueRange->first);
        const bool derivativeViolation = point && request.derivativeRange &&
            (point->derivative.lo > request.derivativeRange->second ||
             point->derivative.hi < request.derivativeRange->first);
        if (valueViolation || derivativeViolation) {
            const auto value = expression.evaluate(midpoint);
            result.status = PolynomialCertificateStatus::Violated;
            result.verifiedWitness = std::move(midpoint);
            if (value.valid()) result.witnessValue = value.value();
            result.witnessValueEnclosure = {{point->value.lo, point->value.hi}};
            if (request.derivativeInput) {
                const auto partial = evaluatePartial(expression, *result.verifiedWitness,
                                                     *request.derivativeInput, derivativeLimits);
                if (partial.derivativeAvailable()) result.witnessDerivative = partial.derivative();
                result.witnessDerivativeEnclosure = {{point->derivative.lo,
                                                       point->derivative.hi}};
            }
            result.valueEnclosure = {{rootEnclosure->value.lo, rootEnclosure->value.hi}};
            if (request.derivativeInput)
                result.derivativeEnclosure = {{rootEnclosure->derivative.lo,
                                               rootEnclosure->derivative.hi}};
            result.reason = "outward_rounded_point_witness";
            return result;
        }
        const bool valueCertified = !request.valueRange ||
            genetik_bounds_detail::within(enclosure->value, *request.valueRange);
        const bool derivativeCertified = !request.derivativeRange ||
            genetik_bounds_detail::within(enclosure->derivative, *request.derivativeRange);
        if (valueCertified && derivativeCertified) {
            totalValue.lo = std::min(totalValue.lo, enclosure->value.lo);
            totalValue.hi = std::max(totalValue.hi, enclosure->value.hi);
            totalDerivative.lo = std::min(totalDerivative.lo, enclosure->derivative.lo);
            totalDerivative.hi = std::max(totalDerivative.hi, enclosure->derivative.hi);
            continue;
        }
        std::size_t split = cell.lower.size();
        double widest = 0.0;
        for (std::size_t i = 0; i < cell.lower.size(); ++i) {
            const double width = cell.upper[i] - cell.lower[i];
            if (width > widest && std::midpoint(cell.lower[i], cell.upper[i]) != cell.lower[i] &&
                std::midpoint(cell.lower[i], cell.upper[i]) != cell.upper[i]) {
                widest = width;
                split = i;
            }
        }
        if (split == cell.lower.size() || result.subdivisions >= request.maximumSubdivisions) {
            allCertified = false;
            totalValue.lo = std::min(totalValue.lo, enclosure->value.lo);
            totalValue.hi = std::max(totalValue.hi, enclosure->value.hi);
            totalDerivative.lo = std::min(totalDerivative.lo, enclosure->derivative.lo);
            totalDerivative.hi = std::max(totalDerivative.hi, enclosure->derivative.hi);
            continue;
        }
        const double middle = std::midpoint(cell.lower[split], cell.upper[split]);
        auto left = cell, right = std::move(cell);
        left.upper[split] = middle;
        right.lower[split] = middle;
        pending.push_back(std::move(left));
        pending.push_back(std::move(right));
        ++result.subdivisions;
    }
    result.valueEnclosure = {{totalValue.lo, totalValue.hi}};
    if (request.derivativeInput)
        result.derivativeEnclosure = {{totalDerivative.lo, totalDerivative.hi}};
    if (pending.empty() && allCertified) {
        result.status = PolynomialCertificateStatus::Certified;
        result.reason = "outward_rounded_interval_enclosure_within_bounds";
    } else {
        result.status = PolynomialCertificateStatus::Inconclusive;
        result.reason = "interval_enclosure_overlaps_requested_boundary";
    }
    return result;
}
