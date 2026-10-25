#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <stdexcept>
namespace genetik_discrete_detail {
enum class IntegerOperation { Divide, Gcd, Lcm, Power, ModularPower, Factorial, Binomial };
struct IntegerOperatorDescription {
    IntegerOperation operation;
    std::string_view name;
    unsigned arity;
};
inline constexpr std::array integerOperators{
    IntegerOperatorDescription{IntegerOperation::Divide, "integer_divide", 2},
    IntegerOperatorDescription{IntegerOperation::Gcd, "gcd", 2},
    IntegerOperatorDescription{IntegerOperation::Lcm, "lcm", 2},
    IntegerOperatorDescription{IntegerOperation::Power, "integer_power", 2},
    IntegerOperatorDescription{IntegerOperation::ModularPower, "modular_power", 3},
    IntegerOperatorDescription{IntegerOperation::Factorial, "factorial", 1},
    IntegerOperatorDescription{IntegerOperation::Binomial, "binomial", 2}};
inline std::uint64_t integerMagnitude(std::int64_t a) {
    return a < 0 ? std::uint64_t(-(a + 1)) + 1 : static_cast<std::uint64_t>(a);
}
inline std::optional<std::int64_t> integerMultiply(std::int64_t a, std::int64_t b) {
    const bool negative = (a < 0) != (b < 0);
    const auto x = integerMagnitude(a), y = integerMagnitude(b);
    const auto cap = negative ? std::uint64_t{1} << 63 : static_cast<std::uint64_t>(INT64_MAX);
    if (y && x > cap / y)
        return {};
    const auto v = x * y;
    return negative ? (v == (std::uint64_t{1} << 63) ? INT64_MIN : -static_cast<std::int64_t>(v))
                    : static_cast<std::int64_t>(v);
}
// Charge BEFORE every arithmetic step. Caller owns work admission and cancellation.
// At most64 Euclid steps / power bits; modular products use bounded add-and-double.
template <class Charge>
std::optional<std::int64_t> integerMath(IntegerOperation op, std::int64_t a, std::int64_t b, std::int64_t c,
                                        Charge charge) {
    constexpr auto hi = static_cast<std::uint64_t>(INT64_MAX);
    const auto gcd = [&](std::uint64_t x, std::uint64_t y) {
        while (y) {
            charge();
            auto r = x % y;
            x = y;
            y = r;
        }
        return x;
    };
    switch (op) {
    case IntegerOperation::Divide: {
        if (!b || (a == INT64_MIN && b == -1))
            return {};
        charge();
        auto q = a / b;
        auto r = a % b;
        if (r < 0) {
            charge();
            q += b > 0 ? -1 : 1;
        }
        return q;
    }
    case IntegerOperation::Gcd: {
        auto v = gcd(integerMagnitude(a), integerMagnitude(b));
        if (v > hi)
            return {};
        return static_cast<std::int64_t>(v);
    }
    case IntegerOperation::Lcm: {
        if (!a || !b)
            return 0;
        auto x = integerMagnitude(a), y = integerMagnitude(b);
        auto g = gcd(x, y);
        charge();
        x /= g;
        if (x > hi / y)
            return {};
        charge();
        return static_cast<std::int64_t>(x * y);
    }
    case IntegerOperation::Power: {
        if (b < 0)
            return {};
        std::int64_t result = 1, base = a;
        auto exponent = static_cast<std::uint64_t>(b);
        while (exponent) {
            if (exponent & 1) {
                charge();
                auto v = integerMultiply(result, base);
                if (!v)
                    return {};
                result = *v;
            }
            exponent >>= 1;
            if (exponent) {
                charge();
                auto v = integerMultiply(base, base);
                if (!v)
                    return {};
                base = *v;
            }
        }
        return result;
    }
    case IntegerOperation::ModularPower: {
        if (b < 0 || c <= 0)
            return {};
        charge();
        auto signedBase = a % c;
        if (signedBase < 0)
            signedBase += c;
        const auto m = static_cast<std::uint64_t>(c);
        auto base = static_cast<std::uint64_t>(signedBase), result = std::uint64_t{1} % m,
             exponent = static_cast<std::uint64_t>(b);
        const auto add = [&](std::uint64_t x, std::uint64_t y) {
            charge();
            return x >= m - y ? x - (m - y) : x + y;
        };
        const auto multiply = [&](std::uint64_t x, std::uint64_t y) {
            std::uint64_t value = 0;
            while (y) {
                if (y & 1)
                    value = add(value, x);
                y >>= 1;
                if (y)
                    x = add(x, x);
            }
            return value;
        };
        while (exponent) {
            if (exponent & 1)
                result = multiply(result, base);
            exponent >>= 1;
            if (exponent)
                base = multiply(base, base);
        }
        return static_cast<std::int64_t>(result);
    }
    case IntegerOperation::Factorial: {
        if (a < 0 || a > 20)
            return {};
        std::int64_t result = 1;
        for (std::int64_t i = 2; i <= a; ++i) {
            charge();
            auto v = integerMultiply(result, i);
            if (!v)
                return {};
            result = *v;
        }
        return result;
    }
    case IntegerOperation::Binomial: {
        if (a < 0 || b < 0 || b > a)
            return {};
        b = std::min(b, a - b);
        if (b > 63)
            return {};
        std::uint64_t result = 1;
        for (std::int64_t i = 1; i <= b; ++i) {
            auto x = static_cast<std::uint64_t>(a - b + i), y = static_cast<std::uint64_t>(i);
            auto g = gcd(x, y);
            charge();
            x /= g;
            y /= g;
            g = gcd(result, y);
            charge();
            result /= g;
            y /= g;
            if (y != 1)
                throw std::logic_error("binomial cancellation invariant");
            if (x && result > hi / x)
                return {};
            charge();
            result *= x;
        }
        return static_cast<std::int64_t>(result);
    }
    }
    throw std::invalid_argument("unknown integer operation");
}
} // namespace genetik_discrete_detail
