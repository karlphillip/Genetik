#pragma once

// Optional exact integer backend. This header is the only Genetik header that
// includes Boost.Multiprecision; ordinary builds and consumers do not require it.
#include <boost/multiprecision/cpp_int.hpp>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

struct BigIntegerLimits {
    std::size_t maxBitLength = 1'000'000;
    std::size_t maxAllocatedBytes = 16 * 1024 * 1024;
    std::size_t maxIntermediateBits = 1'000'000;
    std::uint64_t maxArithmeticWork = 100'000'000;
    std::uint64_t maxExponent = 1'000'000;
};
enum class BigIntegerStatus { Valid, MathematicalInvalid, ResourceExhausted };
struct BigIntegerResult {
    BigIntegerStatus status = BigIntegerStatus::MathematicalInvalid;
    std::optional<boost::multiprecision::cpp_int> value;
    std::uint64_t arithmeticWork = 0;
    std::size_t resultBits = 0;
    const char *reason = "";
    bool valid() const noexcept { return status == BigIntegerStatus::Valid && value.has_value(); }
};

class BoundedBigInteger {
public:
    using value_type = boost::multiprecision::cpp_int;
    explicit BoundedBigInteger(value_type value = 0) : value_(std::move(value)) {}
    const value_type &value() const noexcept { return value_; }
    std::string toString() const { return value_.convert_to<std::string>(); }

    static std::size_t bitLength(const value_type &value) {
        if (value == 0) return 0;
        const auto magnitude = value < 0 ? -value : value;
        return static_cast<std::size_t>(boost::multiprecision::msb(magnitude)) + 1;
    }
    static std::size_t payloadBytes(std::size_t bits) noexcept { return (bits + 7) / 8; }
    static BigIntegerResult parse(std::string_view text, BigIntegerLimits limits = {}) {
        BigIntegerResult out;
        if (text.empty()) return invalid("empty BigInt text");
        std::size_t offset = text.front() == '-' ? 1 : 0;
        if (offset == text.size() || (text[offset] == '0' && text.size() - offset != 1) ||
            (offset && text.substr(offset) == "0")) return invalid("noncanonical BigInt text");
        for (std::size_t i = offset; i < text.size(); ++i)
            if (text[i] < '0' || text[i] > '9') return invalid("invalid BigInt decimal digit");
        if (text.size() - offset > maxDecimalDigits(limits.maxBitLength) ||
            payloadBytes(limits.maxBitLength) > limits.maxAllocatedBytes)
            return resource("BigInt parse limit");
        const auto parseLimbs = std::max<std::uint64_t>(1, (limits.maxBitLength + 63) / 64);
        if (parseLimbs && text.size() > limits.maxArithmeticWork / parseLimbs)
            return resource("BigInt parse work limit");
        const auto decimalWork = static_cast<std::uint64_t>(text.size()) * parseLimbs;
        value_type parsed = 0;
        for (std::size_t i = offset; i < text.size(); ++i) {
            parsed *= 10;
            parsed += text[i] - '0';
        }
        if (offset) parsed = -parsed;
        const auto bits = bitLength(parsed);
        if (!fits(bits, limits)) return resource("BigInt bit or byte limit");
        out.status = BigIntegerStatus::Valid; out.value = std::move(parsed); out.resultBits = bits;
        out.arithmeticWork = decimalWork;
        return out;
    }
    static BigIntegerResult fromInt64(std::int64_t value, BigIntegerLimits limits = {}) {
        BigIntegerResult out;
        value_type converted = value;
        const auto bits = bitLength(converted);
        if (!fits(bits, limits)) return resource("BigInt bit or byte limit");
        out.status = BigIntegerStatus::Valid; out.value = std::move(converted); out.resultBits = bits;
        return out;
    }
    static std::optional<std::int64_t> toInt64(const value_type &value) {
        static const value_type min = std::numeric_limits<std::int64_t>::min();
        static const value_type max = std::numeric_limits<std::int64_t>::max();
        if (value < min || value > max) return {};
        return value.convert_to<std::int64_t>();
    }
    static int compare(const value_type &a,const value_type &b) noexcept {
        return a<b?-1:a>b?1:0;
    }

    static BigIntegerResult add(const value_type &a, const value_type &b, BigIntegerLimits limits = {}) {
        return binary(a,b,limits,std::max(bitLength(a),bitLength(b))+1,1,[&]{return value_type(a+b);});
    }
    static BigIntegerResult subtract(const value_type &a, const value_type &b, BigIntegerLimits limits = {}) {
        return binary(a,b,limits,std::max(bitLength(a),bitLength(b))+1,1,[&]{return value_type(a-b);});
    }
    static BigIntegerResult multiply(const value_type &a, const value_type &b, BigIntegerLimits limits = {}) {
        const auto work = limbCost(bitLength(a), bitLength(b));
        return binary(a,b,limits,bitLength(a)+bitLength(b),work,[&]{return value_type(a*b);});
    }
    static BigIntegerResult divide(const value_type &a, const value_type &b, BigIntegerLimits limits = {}) {
        if (b == 0) return invalid("BigInt division by zero");
        return binary(a,b,limits,bitLength(a),limbCost(bitLength(a),bitLength(b)),[&]{return value_type(a/b);});
    }
    // Truncating remainder paired with divide(); unlike modulo(), its sign may
    // follow the numerator and it permits a negative nonzero divisor.
    static BigIntegerResult remainder(const value_type &a,const value_type &b,BigIntegerLimits limits = {}) {
        if (b==0) return invalid("BigInt remainder by zero");
        return binary(a,b,limits,bitLength(b),limbCost(bitLength(a),bitLength(b)),
                      [&]{return value_type(a%b);});
    }
    // Euclidean modulo: divisor must be positive and the answer is [0, divisor).
    static BigIntegerResult modulo(const value_type &a, const value_type &b, BigIntegerLimits limits = {}) {
        if (b <= 0) return invalid("BigInt modulo requires positive divisor");
        return binary(a,b,limits,bitLength(b),limbCost(bitLength(a),bitLength(b)),[&]{value_type r=a%b;if(r<0)r+=b;return r;});
    }
    static BigIntegerResult gcd(value_type a, value_type b, BigIntegerLimits limits = {}) {
        a = absolute(std::move(a)); b = absolute(std::move(b));
        if (bitLength(a)>limits.maxBitLength||bitLength(b)>limits.maxBitLength||
            bitLength(a)>limits.maxIntermediateBits||bitLength(b)>limits.maxIntermediateBits||
            payloadBytes(bitLength(a))+payloadBytes(bitLength(b))>limits.maxAllocatedBytes)
            return resource("BigInt GCD operand limit");
        std::uint64_t work = 0;
        while (b != 0) {
            const auto cost=limbCost(bitLength(a),bitLength(b));
            if (!admitWork(work,cost,limits)) return resource("BigInt GCD work limit", work);
            work+=cost;
            value_type r = a % b; a = std::move(b); b = std::move(r);
        }
        if (!fits(bitLength(a),limits)) return resource("BigInt GCD result limit",work);
        return valid(std::move(a),work);
    }
    static BigIntegerResult lcm(const value_type &a, const value_type &b, BigIntegerLimits limits = {}) {
        if (a == 0 || b == 0) return valid(0,0);
        auto common = gcd(a,b,limits);
        if (!common.valid()) return common;
        const auto reduced = a / *common.value;
        auto result = multiply(reduced,b,limits);
        if (result.valid() && *result.value < 0) *result.value = -*result.value;
        result.arithmeticWork += common.arithmeticWork;
        return result;
    }
    static BigIntegerResult power(value_type base, std::uint64_t exponent, BigIntegerLimits limits = {}) {
        if (exponent > limits.maxExponent) return resource("BigInt exponent limit");
        const auto estimated = estimateProductBits(bitLength(base), exponent);
        if (estimated > limits.maxIntermediateBits || payloadBytes(estimated) > limits.maxAllocatedBytes)
            return resource("BigInt power intermediate limit");
        value_type result = 1;
        std::uint64_t work = 0;
        while (exponent) {
            if (exponent & 1) {
                const auto cost = limbCost(bitLength(result),bitLength(base));
                if (!admitWork(work,cost,limits)) return resource("BigInt power work limit",work);
                const auto estimate=bitLength(result)+bitLength(base);
                const auto live=payloadBytes(bitLength(result))+payloadBytes(bitLength(base));
                if (estimate>limits.maxIntermediateBits||live>limits.maxAllocatedBytes||
                    payloadBytes(estimate)>limits.maxAllocatedBytes-live)
                    return resource("BigInt power allocation limit",work);
                result *= base; work += cost;
                if (!fits(bitLength(result),limits)) return resource("BigInt power result limit",work);
            }
            exponent >>= 1;
            if (exponent) {
                const auto cost = limbCost(bitLength(base),bitLength(base));
                if (!admitWork(work,cost,limits)) return resource("BigInt power work limit",work);
                const auto estimate=bitLength(base)*2;
                const auto live=payloadBytes(bitLength(result))+payloadBytes(bitLength(base));
                if (estimate>limits.maxIntermediateBits||live>limits.maxAllocatedBytes||
                    payloadBytes(estimate)>limits.maxAllocatedBytes-live)
                    return resource("BigInt power allocation limit",work);
                base *= base; work += cost;
                if (!fits(bitLength(base),limits)) return resource("BigInt power intermediate limit",work);
            }
        }
        return valid(std::move(result),work);
    }
    static BigIntegerResult factorial(std::uint64_t n, BigIntegerLimits limits = {}) {
        if (n > limits.maxExponent) return resource("BigInt factorial step limit");
        value_type result=1; std::uint64_t work=0;
        for (std::uint64_t i=2;i<=n;++i) {
            const auto cost=limbCost(bitLength(result),bitLength(value_type(i)));
            if (!admitWork(work,cost,limits)) return resource("BigInt factorial work limit",work);
            const auto estimate=bitLength(result)+bitLength(value_type(i));
            const auto live=payloadBytes(bitLength(result))+payloadBytes(bitLength(value_type(i)));
            if (estimate>limits.maxIntermediateBits||live>limits.maxAllocatedBytes||
                payloadBytes(estimate)>limits.maxAllocatedBytes-live)
                return resource("BigInt factorial intermediate limit",work);
            result*=i; work+=cost;
            if (!fits(bitLength(result),limits)) return resource("BigInt factorial result limit",work);
        }
        return valid(std::move(result),work);
    }
    static BigIntegerResult binomial(std::uint64_t n, std::uint64_t k, BigIntegerLimits limits = {}) {
        if (k>n) return invalid("BigInt binomial requires 0<=k<=n");
        k=std::min(k,n-k); if (k>limits.maxExponent) return resource("BigInt binomial step limit");
        value_type result=1; std::uint64_t work=0;
        for (std::uint64_t i=1;i<=k;++i) {
            const value_type numerator=n-k+i;
            const auto cost=limbCost(bitLength(result),bitLength(numerator));
            if (!admitWork(work,cost+1,limits)) return resource("BigInt binomial work limit",work);
            const auto estimate=bitLength(result)+bitLength(numerator);
            const auto live=payloadBytes(bitLength(result))+payloadBytes(bitLength(numerator));
            if (estimate>limits.maxIntermediateBits||live>limits.maxAllocatedBytes||
                payloadBytes(estimate)>limits.maxAllocatedBytes-live)
                return resource("BigInt binomial intermediate limit",work);
            const value_type product=result*numerator;
            result=product/i; work+=cost+1;
            if (!fits(bitLength(result),limits)) return resource("BigInt binomial result limit",work);
        }
        return valid(std::move(result),work);
    }

private:
    value_type value_;
    static std::size_t maxDecimalDigits(std::size_t bits) noexcept {
        // Conservative floor(log10(2)*bits), with overflow-safe arithmetic.
        return bits > std::numeric_limits<std::size_t>::max()/30103 ?
            std::numeric_limits<std::size_t>::max() : bits*30103/100000 + 1;
    }
    static std::size_t estimateProductBits(std::size_t bits,std::uint64_t exponent) noexcept {
        if (!bits || exponent==0) return 1;
        if (exponent > std::numeric_limits<std::size_t>::max()/bits) return std::numeric_limits<std::size_t>::max();
        return bits*static_cast<std::size_t>(exponent);
    }
    static std::uint64_t limbCost(std::size_t a,std::size_t b) noexcept {
        const auto x=static_cast<std::uint64_t>((a+63)/64),y=static_cast<std::uint64_t>((b+63)/64);
        if (y && x>std::numeric_limits<std::uint64_t>::max()/y) return std::numeric_limits<std::uint64_t>::max();
        return std::max<std::uint64_t>(1,x*y);
    }
    static bool admitWork(std::uint64_t used,std::uint64_t cost,const BigIntegerLimits &limits) noexcept {
        return cost<=limits.maxArithmeticWork && used<=limits.maxArithmeticWork-cost;
    }
    static bool fits(std::size_t bits,const BigIntegerLimits &limits) noexcept {
        return bits<=limits.maxBitLength && bits<=limits.maxIntermediateBits &&
               payloadBytes(bits)<=limits.maxAllocatedBytes;
    }
    template<class F>
    static BigIntegerResult binary(const value_type &a,const value_type &b,const BigIntegerLimits &limits,
                                   std::size_t estimate,std::uint64_t work,F &&operation) {
        const auto aBits=bitLength(a),bBits=bitLength(b);
        const auto liveBytes=payloadBytes(aBits)+payloadBytes(bBits);
        if (aBits>limits.maxBitLength||bBits>limits.maxBitLength||liveBytes>limits.maxAllocatedBytes)
            return resource("BigInt operand limit");
        const auto cost=std::max<std::uint64_t>(1,work);
        if (cost>limits.maxArithmeticWork) return resource("BigInt arithmetic work limit");
        // Bound the largest possible intermediate before asking Boost to allocate.
        if (estimate>limits.maxIntermediateBits || payloadBytes(estimate)>limits.maxAllocatedBytes-liveBytes)
            return resource("BigInt intermediate allocation limit");
        auto value=operation();
        if (!fits(bitLength(value),limits)) return resource("BigInt result limit",cost);
        return valid(std::move(value),cost);
    }
    static value_type absolute(value_type value) { return value<0?-value:value; }
    static BigIntegerResult valid(value_type value,std::uint64_t work) {
        BigIntegerResult out; out.status=BigIntegerStatus::Valid; out.resultBits=bitLength(value);
        out.arithmeticWork=work; out.value=std::move(value); return out;
    }
    static BigIntegerResult invalid(const char *why) { BigIntegerResult out;out.reason=why;out.status=BigIntegerStatus::MathematicalInvalid;return out; }
    static BigIntegerResult resource(const char *why,std::uint64_t work=0) { BigIntegerResult out;out.reason=why;out.status=BigIntegerStatus::ResourceExhausted;out.arithmeticWork=work;return out; }
};
