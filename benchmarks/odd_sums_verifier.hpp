#pragma once

#include <ConstructiveRecurrenceSynthesis.hpp>

#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <vector>

namespace odd_sums_verifier {
using namespace genetik::constructive;
using Monomial = std::array<unsigned, 3>;
using Polynomial = std::map<Monomial, std::int64_t>;

enum class Proof { Proved, Counterexample, Inconclusive };

inline std::optional<std::int64_t> checkedAdd(std::int64_t a, std::int64_t b) {
    if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
        (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b))
        return std::nullopt;
    return a + b;
}

inline std::optional<std::int64_t> checkedMultiply(std::int64_t a, std::int64_t b) {
    if (a == 0 || b == 0)
        return 0;
    constexpr auto min = std::numeric_limits<std::int64_t>::min();
    constexpr auto max = std::numeric_limits<std::int64_t>::max();
    if ((a > 0 && b > 0 && a > max / b) || (a > 0 && b < 0 && b < min / a) ||
        (a < 0 && b > 0 && a < min / b) || (a < 0 && b < 0 && a < max / b))
        return std::nullopt;
    return a * b;
}

using Univariate = std::map<unsigned, std::int64_t>;
inline bool accumulate(Univariate& p, unsigned degree, std::int64_t value) {
    const auto sum = checkedAdd(p[degree], value);
    if (!sum)
        return false;
    if (*sum)
        p[degree] = *sum;
    else
        p.erase(degree);
    return true;
}
inline std::optional<Univariate> multiply(const Univariate& a, const Univariate& b) {
    Univariate result;
    for (const auto& [da, ca] : a)
        for (const auto& [db, cb] : b) {
            const auto product = checkedMultiply(ca, cb);
            if (!product || !accumulate(result, da + db, *product))
                return std::nullopt;
        }
    return result;
}

inline std::optional<Polynomial> integerPolynomial(const DiscreteExpression& e) {
    std::vector<Polynomial> stack;
    for (const auto& node : e.nodes()) {
        if (node.kind() == DiscreteKind::Integer) {
            Polynomial p;
            if (node.integer())
                p[{}] = node.integer();
            stack.push_back(std::move(p));
        } else if (node.kind() == DiscreteKind::Variable) {
            if (node.variable() >= 3)
                return std::nullopt;
            Polynomial p;
            Monomial m{};
            m[node.variable()] = 1;
            p[m] = 1;
            stack.push_back(std::move(p));
        } else if (node.kind() == DiscreteKind::Add || node.kind() == DiscreteKind::Subtract ||
                   node.kind() == DiscreteKind::Multiply) {
            if (stack.size() < 2)
                return std::nullopt;
            auto rhs = std::move(stack.back());
            stack.pop_back();
            auto lhs = std::move(stack.back());
            stack.pop_back();
            Polynomial result;
            if (node.kind() == DiscreteKind::Multiply) {
                for (const auto& [a, ca] : lhs)
                    for (const auto& [b, cb] : rhs) {
                        Monomial m{};
                        for (std::size_t i = 0; i < 3; ++i) {
                            if (a[i] > std::numeric_limits<unsigned>::max() - b[i])
                                return std::nullopt;
                            m[i] = a[i] + b[i];
                        }
                        const auto product = checkedMultiply(ca, cb);
                        if (!product)
                            return std::nullopt;
                        const auto sum = checkedAdd(result[m], *product);
                        if (!sum)
                            return std::nullopt;
                        if (*sum)
                            result[m] = *sum;
                        else
                            result.erase(m);
                    }
            } else {
                result = std::move(lhs);
                for (const auto& [m, c] : rhs) {
                    const auto signedValue =
                        node.kind() == DiscreteKind::Add ? c : (c == INT64_MIN ? 0 : -c);
                    if (node.kind() == DiscreteKind::Subtract && c == INT64_MIN)
                        return std::nullopt;
                    const auto sum = checkedAdd(result[m], signedValue);
                    if (!sum)
                        return std::nullopt;
                    if (*sum)
                        result[m] = *sum;
                    else
                        result.erase(m);
                }
            }
            stack.push_back(std::move(result));
        } else
            return std::nullopt;
    }
    if (stack.size() != 1)
        return std::nullopt;
    return stack.back();
}

inline std::optional<Univariate> substituteSquareInduction(const DiscreteExpression& e) {
    const auto polynomial = integerPolynomial(e);
    if (!polynomial)
        return std::nullopt;
    const Univariate i{{1, 1}};
    const Univariate i2{{2, 1}};
    const Univariate nextSquare{{0, 1}, {1, 2}, {2, 1}};
    const std::array<Univariate, 3> substitutions{i, i2, nextSquare};
    Univariate result;
    for (const auto& [monomial, coefficient] : *polynomial) {
        Univariate term{{0, coefficient}};
        for (std::size_t variable = 0; variable < 3; ++variable)
            for (unsigned power = 0; power < monomial[variable]; ++power) {
                if (power > std::numeric_limits<unsigned>::max() - 2)
                    return std::nullopt;
                auto product = multiply(term, substitutions[variable]);
                if (!product)
                    return std::nullopt;
                term = std::move(*product);
            }
        for (const auto& [degree, value] : term)
            if (!accumulate(result, degree, value))
                return std::nullopt;
    }
    return result;
}

inline Proof inductionCheck(const DiscreteExpression& transition) {
    // Check the base terms 0 and 1, then prove the symbolic induction step
    // T(i, i^2, (i+1)^2) = (i+2)^2 with checked int64 coefficient arithmetic.
    IntegerRecurrence base(0, {0, 1}, transition);
    for (std::int64_t n = 0; n <= 1; ++n) {
        const auto value = base.evaluate(n);
        if (!value.valid())
            return Proof::Inconclusive;
        if (value.value->integer() != n * n)
            return Proof::Counterexample;
    }
    const auto actual = substituteSquareInduction(transition);
    if (!actual) {
        const bool polynomialSyntax = std::all_of(
            transition.nodes().begin(), transition.nodes().end(), [](const DiscreteNode& node) {
                return node.kind() == DiscreteKind::Integer ||
                       node.kind() == DiscreteKind::Variable || node.kind() == DiscreteKind::Add ||
                       node.kind() == DiscreteKind::Subtract ||
                       node.kind() == DiscreteKind::Multiply;
            });
        if (polynomialSyntax)
            return Proof::Inconclusive;
        // A non-polynomial transition cannot be proved by this verifier, but a
        // concrete checked execution can still produce a genuine counterexample.
        IntegerRecurrence rollout(0, {0, 1}, transition);
        for (std::int64_t n = 2; n <= 128; ++n) {
            const auto value = rollout.evaluate(n);
            if (!value.valid())
                return Proof::Inconclusive;
            if (value.value->integer() != n * n)
                return Proof::Counterexample;
        }
        return Proof::Inconclusive;
    }
    const Univariate expected{{0, 4}, {1, 4}, {2, 1}};
    return *actual == expected ? Proof::Proved : Proof::Counterexample;
}
} // namespace odd_sums_verifier
