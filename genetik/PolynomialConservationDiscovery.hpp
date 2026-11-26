#pragma once

#include "ConservationDiscovery.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

// Deterministic polynomial first integrals from grouped observations. The
// method searches the nullspace of within-trajectory centered monomials.
struct PolynomialConservationConfig {
    unsigned maximumDegree = 4;
    double rankTolerance = 1e-8;
    double jacobiTolerance = 1e-14;
    double maximumWithinFraction = 1e-6;
    double minimumBetweenFraction = 0.01;
    std::uint64_t maximumJacobiRotationsPerDegree = 10000;
    std::uint64_t maximumFeatureValues = 10000000;
    std::int64_t maximumCoefficientDenominator = 1024;
};

struct PolynomialConservationResult {
    bool found = false;
    Expression expression = Expression::constant(0.0, 2);
    unsigned degree = 0;
    std::size_t monomials = 0;
    // Sum of the sizes of every degree matrix constructed, including repeated
    // lower-degree columns as the cumulative basis grows.
    std::uint64_t cumulativeFeatureValues = 0;
    bool exactRepresentationAvailable = true;
    std::uint64_t jacobiRotations = 0;
    double eigenvalueRatio = std::numeric_limits<double>::infinity();
    ConservationObjectiveMetrics trainingAssessment;
};

namespace polynomial_conservation_detail {
struct Monomial {
    unsigned y, v;
};

inline double power(double x, unsigned n) {
    double r = 1.0;
    for (unsigned i = 0; i < n; ++i)
        r *= x;
    return r;
}

inline std::vector<Monomial> monomials(unsigned degree) {
    std::vector<Monomial> out;
    for (unsigned d = 1; d <= degree; ++d)
        for (unsigned y = d + 1; y-- > 0;)
            out.push_back({y, d - y});
    return out;
}

struct Eigen {
    std::vector<double> values;
    std::vector<std::vector<double>> vectors;
    std::uint64_t rotations = 0;
};
inline Eigen symmetricEigen(std::vector<std::vector<double>> a, double tolerance,
                            std::uint64_t maxRotations) {
    const std::size_t n = a.size();
    std::vector<std::vector<double>> q(n, std::vector<double>(n, 0.0));
    for (std::size_t i = 0; i < n; ++i)
        q[i][i] = 1.0;
    std::uint64_t rotations = 0;
    for (;;) {
        std::size_t p = 0, r = 0;
        double largest = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j)
                if (std::abs(a[i][j]) > largest) {
                    largest = std::abs(a[i][j]);
                    p = i;
                    r = j;
                }
        if (largest <= tolerance)
            break;
        if (rotations == maxRotations)
            return {};
        ++rotations;
        const double tau = (a[r][r] - a[p][p]) / (2.0 * a[p][r]);
        const double t = std::copysign(1.0, tau) / (std::abs(tau) + std::sqrt(1.0 + tau * tau));
        const double c = 1.0 / std::sqrt(1.0 + t * t), s = t * c;
        const double app = a[p][p], arr = a[r][r], apr = a[p][r];
        a[p][p] = app - t * apr;
        a[r][r] = arr + t * apr;
        a[p][r] = a[r][p] = 0.0;
        for (std::size_t k = 0; k < n; ++k)
            if (k != p && k != r) {
                const double akp = a[k][p], akr = a[k][r];
                a[k][p] = a[p][k] = c * akp - s * akr;
                a[k][r] = a[r][k] = s * akp + c * akr;
            }
        for (std::size_t k = 0; k < n; ++k) {
            const double qkp = q[k][p], qkr = q[k][r];
            q[k][p] = c * qkp - s * qkr;
            q[k][r] = s * qkp + c * qkr;
        }
    }
    Eigen out;
    out.rotations = rotations;
    out.values.reserve(n);
    out.vectors = std::move(q);
    for (std::size_t i = 0; i < n; ++i)
        out.values.push_back(a[i][i]);
    return out;
}

inline std::pair<std::int64_t, std::int64_t> rational(double x, std::int64_t maxDenom) {
    if (!std::isfinite(x))
        throw std::runtime_error("nonfinite polynomial coefficient");
    const bool negative = x < 0.0;
    x = std::abs(x);
    std::int64_t bestN = 0, bestD = 1;
    double bestError = std::numeric_limits<double>::infinity();
    for (std::int64_t d = 1; d <= maxDenom; ++d) {
        const auto n = static_cast<std::int64_t>(std::llround(x * static_cast<double>(d)));
        const double error = std::abs(x - static_cast<double>(n) / d);
        if (error < bestError) {
            bestError = error;
            bestN = n;
            bestD = d;
        }
    }
    const auto g = std::gcd(bestN, bestD);
    return {negative ? -bestN / g : bestN / g, bestD / g};
}

inline bool
clearDenominators(const std::vector<std::pair<std::int64_t, std::int64_t>>& rationalCoefficients,
                  double maximumMagnitude, std::vector<std::int64_t>& integerCoefficients) {
    std::int64_t commonDenominator = 1;
    for (const auto& [numerator, denominator] : rationalCoefficients) {
        (void)numerator;
        if (denominator <= 0)
            return false;
        const auto divisor = std::gcd(commonDenominator, denominator);
        const auto factor = denominator / divisor;
        if (commonDenominator > std::numeric_limits<std::int64_t>::max() / factor)
            return false;
        commonDenominator *= factor;
    }
    integerCoefficients.clear();
    integerCoefficients.reserve(rationalCoefficients.size());
    std::int64_t commonDivisor = 0;
    for (const auto& [numerator, denominator] : rationalCoefficients) {
        const auto factor = commonDenominator / denominator;
        if (numerator != 0 &&
            (numerator > 0 ? numerator > std::numeric_limits<std::int64_t>::max() / factor
                           : numerator < std::numeric_limits<std::int64_t>::min() / factor))
            return false;
        const auto value = numerator * factor;
        integerCoefficients.push_back(value);
        commonDivisor = std::gcd(commonDivisor, value);
    }
    if (commonDivisor == 0)
        return false;
    for (auto& value : integerCoefficients)
        value /= commonDivisor;
    for (const auto value : integerCoefficients)
        if (std::abs(static_cast<double>(value)) > maximumMagnitude ||
            value < -9007199254740991LL || value > 9007199254740991LL)
            return false;
    return true;
}

inline Expression makeExpression(const std::vector<Monomial>& ms,
                                 const std::vector<std::int64_t>& cs, ExpressionLimits limits) {
    const auto y = Expression::variable(0, 2, limits), v = Expression::variable(1, 2, limits);
    std::optional<Expression> sum;
    for (std::size_t i = 0; i < ms.size(); ++i) {
        if (cs[i] == 0)
            continue;
        std::optional<Expression> term;
        if (ms[i].y > 0)
            term.emplace(y);
        else
            term.emplace(v);
        unsigned remainingY = ms[i].y > 0 ? ms[i].y - 1 : 0;
        unsigned remainingV = ms[i].y > 0 ? ms[i].v : ms[i].v - 1;
        while (remainingY > 0) {
            --remainingY;
            term = Expression::binary(ExpressionKind::Multiply, *term, y, limits);
        }
        while (remainingV > 0) {
            --remainingV;
            term = Expression::binary(ExpressionKind::Multiply, *term, v, limits);
        }
        if (cs[i] != 1) {
            const auto coefficient = Expression::constant(static_cast<double>(cs[i]), 2, limits);
            term = Expression::binary(ExpressionKind::Multiply, coefficient, *term, limits);
        }
        sum = sum ? std::optional<Expression>(Expression::binary(ExpressionKind::Add, *sum, *term, limits))
                  : std::move(term);
    }
    if (!sum)
        throw std::invalid_argument("zero polynomial has no nonconstant expression");
    return std::move(*sum);
}

inline Expression makeMonomialExpression(Monomial monomial, ExpressionLimits limits) {
    Expression term =
        monomial.y ? Expression::variable(0, 2, limits) : Expression::variable(1, 2, limits);
    unsigned remainingY = monomial.y ? monomial.y - 1 : 0;
    unsigned remainingV = monomial.y ? monomial.v : monomial.v - 1;
    const auto y = Expression::variable(0, 2, limits);
    const auto v = Expression::variable(1, 2, limits);
    while (remainingY > 0) {
        --remainingY;
        term = Expression::binary(ExpressionKind::Multiply, term, y, limits);
    }
    while (remainingV > 0) {
        --remainingV;
        term = Expression::binary(ExpressionKind::Multiply, term, v, limits);
    }
    return term;
}
} // namespace polynomial_conservation_detail

inline PolynomialConservationResult
discoverPolynomialConservation(const ConservationDiscoveryProblem& problem,
                               PolynomialConservationConfig config = {}) {
    using namespace polynomial_conservation_detail;
    if (config.maximumDegree == 0 || config.maximumDegree > 4 ||
        !std::isfinite(config.rankTolerance) || config.rankTolerance <= 0.0 ||
        !std::isfinite(config.jacobiTolerance) || config.jacobiTolerance <= 0.0 ||
        !std::isfinite(config.maximumWithinFraction) || config.maximumWithinFraction <= 0.0 ||
        !std::isfinite(config.minimumBetweenFraction) || config.minimumBetweenFraction <= 0.0 ||
        config.minimumBetweenFraction >= 1.0 || config.maximumJacobiRotationsPerDegree == 0 ||
        config.maximumFeatureValues == 0 || config.maximumCoefficientDenominator <= 0)
        throw std::invalid_argument("invalid polynomial conservation configuration");

    const auto& groups = problem.trajectories();
    PolynomialConservationResult result;
    std::uint64_t featureValues = 0;
    for (unsigned degree = 1; degree <= config.maximumDegree; ++degree) {
        const auto ms = monomials(degree);
        const auto columns = ms.size();
        if (problem.sampleCount() > std::numeric_limits<std::uint64_t>::max() / columns ||
            columns * problem.sampleCount() > config.maximumFeatureValues - featureValues)
            throw std::length_error("polynomial conservation feature work limit");
        featureValues += static_cast<std::uint64_t>(columns * problem.sampleCount());
        result.cumulativeFeatureValues = featureValues;
        std::vector<std::vector<double>> normalized(columns);
        std::vector<double> scales(columns, 0.0), norms(columns, 0.0);
        for (std::size_t j = 0; j < columns; ++j) {
            for (const auto& g : groups)
                for (const auto& s : g.states)
                    scales[j] =
                        std::max(scales[j], std::abs(power(s[0], ms[j].y) * power(s[1], ms[j].v)));
            if (!(scales[j] > 0.0) || !std::isfinite(scales[j]))
                continue;
            normalized[j].reserve(problem.sampleCount());
            for (const auto& g : groups) {
                double mean = 0.0;
                for (const auto& s : g.states)
                    mean += power(s[0], ms[j].y) * power(s[1], ms[j].v) / scales[j];
                mean /= static_cast<double>(g.states.size());
                for (const auto& s : g.states)
                    normalized[j].push_back(
                        power(s[0], ms[j].y) * power(s[1], ms[j].v) / scales[j] - mean);
            }
            for (double x : normalized[j])
                norms[j] += x * x;
            norms[j] = std::sqrt(norms[j]);
            if (norms[j] > 0.0)
                for (double& x : normalized[j])
                    x /= norms[j];
        }
        std::vector<std::vector<double>> gram(columns, std::vector<double>(columns));
        for (std::size_t i = 0; i < columns; ++i)
            for (std::size_t j = i; j < columns; ++j) {
                double dot = 0.0;
                if (norms[i] > 0.0 && norms[j] > 0.0)
                    for (std::size_t k = 0; k < normalized[i].size(); ++k)
                        dot += normalized[i][k] * normalized[j][k];
                gram[i][j] = gram[j][i] = dot;
            }
        auto eig = symmetricEigen(std::move(gram), config.jacobiTolerance,
                                  config.maximumJacobiRotationsPerDegree);
        if (eig.vectors.empty())
            throw std::runtime_error("polynomial conservation Jacobi work limit");
        result.jacobiRotations += eig.rotations;
        const double largest = *std::max_element(eig.values.begin(), eig.values.end());
        const double smallest =
            std::max(0.0, *std::min_element(eig.values.begin(), eig.values.end()));
        result.eigenvalueRatio =
            largest > 0.0 ? smallest / largest : std::numeric_limits<double>::infinity();
        result.degree = degree;
        result.monomials = columns;

        // A feature that is constant inside each group has a zero centered
        // column, so it cannot appear in a covariance null vector. Test each
        // monomial directly; grouped scoring still rejects globally constant
        // or otherwise uninformative coordinates.
        std::optional<Expression> bestExpression;
        ConservationObjectiveMetrics bestAssessment;
        for (const auto& monomial : ms) {
            std::optional<Expression> expression;
            try {
                expression.emplace(makeMonomialExpression(monomial, problem.limits()));
            } catch (const std::exception&) {
                result.exactRepresentationAvailable = false;
                continue;
            }
            auto assessed = problem.assess(*expression);
            if (assessed.score.state() != FitnessState::Valid || !assessed.withinFraction ||
                !assessed.betweenFraction ||
                *assessed.withinFraction > config.maximumWithinFraction ||
                *assessed.betweenFraction < config.minimumBetweenFraction)
                continue;
            if (!bestExpression || expression->nodeCount() < bestExpression->nodeCount() ||
                (expression->nodeCount() == bestExpression->nodeCount() &&
                 *assessed.withinFraction < *bestAssessment.withinFraction)) {
                bestExpression = std::move(*expression);
                bestAssessment = std::move(assessed);
            }
        }
        const bool hasNullspace = largest > 0.0 && result.eigenvalueRatio <= config.rankTolerance;
        if (hasNullspace)
            for (std::size_t eigenIndex = 0; eigenIndex < eig.values.size(); ++eigenIndex) {
                const double ratio = std::max(0.0, eig.values[eigenIndex]) / largest;
                if (ratio > config.rankTolerance)
                    continue;
                std::vector<double> raw(columns, 0.0);
                for (std::size_t j = 0; j < columns; ++j)
                    if (norms[j] > 0.0)
                        raw[j] = eig.vectors[j][eigenIndex] / (scales[j] * norms[j]);
                double scale = 0.0;
                for (double c : raw)
                    scale = std::max(scale, std::abs(c));
                if (!(scale > 0.0) || !std::isfinite(scale))
                    continue;
                for (double& c : raw)
                    c /= scale;
                std::vector<std::pair<std::int64_t, std::int64_t>> coefficients;
                coefficients.reserve(columns);
                for (double c : raw)
                    coefficients.push_back(rational(c, config.maximumCoefficientDenominator));
                std::vector<std::int64_t> integerCoefficients;
                if (!clearDenominators(coefficients, problem.limits().maxAbsConstant,
                                       integerCoefficients)) {
                    result.exactRepresentationAvailable = false;
                    continue;
                }
                std::optional<Expression> expression;
                try {
                    expression.emplace(makeExpression(ms, integerCoefficients, problem.limits()));
                } catch (const std::exception&) {
                    result.exactRepresentationAvailable = false;
                    continue;
                }
                auto assessed = problem.assess(*expression);
                if (assessed.score.state() != FitnessState::Valid || !assessed.withinFraction ||
                    !assessed.betweenFraction ||
                    *assessed.withinFraction > config.maximumWithinFraction ||
                    *assessed.betweenFraction < config.minimumBetweenFraction)
                    continue;
                const bool better = !bestExpression ||
                                    expression->nodeCount() < bestExpression->nodeCount() ||
                                    (expression->nodeCount() == bestExpression->nodeCount() &&
                                     (*assessed.withinFraction < *bestAssessment.withinFraction ||
                                      (*assessed.withinFraction == *bestAssessment.withinFraction &&
                                       expression->toString() < bestExpression->toString())));
                if (better) {
                    bestExpression = std::move(*expression);
                    bestAssessment = std::move(assessed);
                }
            }
        if (bestExpression) {
            result.found = true;
            result.exactRepresentationAvailable = true;
            result.expression = std::move(*bestExpression);
            result.degree = degree;
            result.monomials = columns;
            result.trainingAssessment = std::move(bestAssessment);
            return result; // lowest accepted degree, then fewest nodes and lowest drift.
        }
    }
    return result;
}
