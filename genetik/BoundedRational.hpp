#pragma once

// Optional exact rational arithmetic. This header is intentionally separate
// from the standard-library-only public build and requires Boost headers.
#include "BoundedBigInteger.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

// Exact normalized fraction backed by cpp_int. Construction is bounded by the
// supplied limits; binary64 import captures the exact dyadic value, not its
// intended decimal spelling.
class BoundedRational {
  public:
    using integer_type = BoundedBigInteger::value_type;
    BoundedRational(integer_type numerator = 0, integer_type denominator = 1,
                    BigIntegerLimits limits = {}) : limits_(limits) {
        if (denominator == 0) throw std::invalid_argument("zero rational denominator");
        if (denominator < 0) { numerator=-numerator; denominator=-denominator; }
        const auto maxBits=std::max(BoundedBigInteger::bitLength(numerator),BoundedBigInteger::bitLength(denominator));
        const auto limbs=static_cast<std::uint64_t>((maxBits+63)/64);
        if(maxBits>limits.maxBitLength||maxBits>limits.maxIntermediateBits||payload(maxBits)>limits.maxAllocatedBytes)throw std::length_error("rational input limit");
        if(limbs&&limbs>std::numeric_limits<std::uint64_t>::max()/limbs/4/std::max<std::uint64_t>(1,maxBits))throw std::length_error("rational normalization work estimate overflow");
        if(std::max<std::uint64_t>(1,static_cast<std::uint64_t>(maxBits)*limbs*limbs*4)>limits.maxArithmeticWork)throw std::length_error("rational normalization work limit");
        auto g=gcd(abs(numerator),denominator);
        numerator/=g; denominator/=g;
        enforce(numerator,denominator);
        n_=std::move(numerator); d_=std::move(denominator);
    }
    const integer_type& numerator() const noexcept { return n_; }
    const integer_type& denominator() const noexcept { return d_; }
    const BigIntegerLimits& limits() const noexcept { return limits_; }
    bool zero() const noexcept { return n_==0; }
    int compare(const BoundedRational& rhs) const {
        preflight(*this,rhs);
        auto a=n_*rhs.d_, b=rhs.n_*d_; return a<b?-1:a>b?1:0;
    }
    friend bool operator==(const BoundedRational&a,const BoundedRational&b) { return a.n_==b.n_&&a.d_==b.d_; }
    friend bool operator<(const BoundedRational&a,const BoundedRational&b) { return a.compare(b)<0; }
    friend BoundedRational operator+(const BoundedRational&a,const BoundedRational&b) {
        preflight(a,b);
        return {a.n_*b.d_+b.n_*a.d_,a.d_*b.d_,stricter(a.limits_,b.limits_)};
    }
    friend BoundedRational operator-(const BoundedRational&a,const BoundedRational&b) {
        preflight(a,b);
        return {a.n_*b.d_-b.n_*a.d_,a.d_*b.d_,stricter(a.limits_,b.limits_)};
    }
    friend BoundedRational operator*(const BoundedRational&a,const BoundedRational&b) {
        preflight(a,b);
        return {a.n_*b.n_,a.d_*b.d_,stricter(a.limits_,b.limits_)};
    }
    friend BoundedRational operator/(const BoundedRational&a,const BoundedRational&b) {
        if(b.n_==0) throw std::domain_error("rational division by zero");
        preflight(a,b);
        return {a.n_*b.d_,a.d_*b.n_,stricter(a.limits_,b.limits_)};
    }
    BoundedRational operator-() const { return {-n_,d_,limits_}; }
    integer_type floor() const { integer_type q=n_/d_; integer_type r=n_%d_; if(n_<0&&r!=0)--q; check(q,limits_); return q; }
    integer_type ceil() const { integer_type q=n_/d_; integer_type r=n_%d_; if(n_>0&&r!=0)++q; check(q,limits_); return q; }
    std::string toString() const { return n_.convert_to<std::string>()+"/"+d_.convert_to<std::string>(); }
    static BoundedRational fromInt64(std::int64_t x,BigIntegerLimits l={}) { return {integer_type(x),1,l}; }
    static BoundedRational fromBinary64(double x,BigIntegerLimits l={}) {
        if(!std::isfinite(x)) throw std::invalid_argument("finite binary64 required");
        if(x==0) return {0,1,l};
        const auto bits=std::bit_cast<std::uint64_t>(x); std::uint64_t m=bits&((std::uint64_t{1}<<52)-1);
        const int raw=static_cast<int>((bits>>52)&2047); int shift=raw?raw-1023-52:-1074;
        if(raw)m|=std::uint64_t{1}<<52;
        integer_type n=m,d=1;
        if(shift>=0)n<<=shift;else d<<=-shift;
        if(bits>>63)n=-n;
        return {std::move(n),std::move(d),l};
    }
    static BoundedRational parse(std::string_view text,BigIntegerLimits l={}) {
        auto slash=text.find('/'); if(slash==std::string_view::npos||text.find('/',slash+1)!=std::string_view::npos)throw std::invalid_argument("rational requires n/d");
        auto a=BoundedBigInteger::parse(text.substr(0,slash),l),b=BoundedBigInteger::parse(text.substr(slash+1),l);
        if(!a.valid()||!b.valid())throw std::length_error("rational parse limits");
        return {*a.value,*b.value,l};
    }
  private:
    integer_type n_,d_; BigIntegerLimits limits_;
    static integer_type abs(integer_type x){return x<0?-x:x;}
    static integer_type gcd(integer_type a,integer_type b){while(b!=0){integer_type r=a%b;a=std::move(b);b=std::move(r);}return a==0?integer_type(1):a;}
    static BigIntegerLimits stricter(const BigIntegerLimits&a,const BigIntegerLimits&b){BigIntegerLimits r;r.maxBitLength=std::min(a.maxBitLength,b.maxBitLength);r.maxAllocatedBytes=std::min(a.maxAllocatedBytes,b.maxAllocatedBytes);r.maxIntermediateBits=std::min(a.maxIntermediateBits,b.maxIntermediateBits);r.maxArithmeticWork=std::min(a.maxArithmeticWork,b.maxArithmeticWork);return r;}
    static std::size_t payload(std::size_t bits)noexcept{return (bits+7)/8;}
    static void preflight(const BoundedRational&a,const BoundedRational&b){const auto l=stricter(a.limits_,b.limits_);const auto bits=std::max({BoundedBigInteger::bitLength(a.n_),BoundedBigInteger::bitLength(a.d_),BoundedBigInteger::bitLength(b.n_),BoundedBigInteger::bitLength(b.d_)});const auto limbs=static_cast<std::uint64_t>((bits+63)/64);if(bits>l.maxBitLength||bits>l.maxIntermediateBits)throw std::length_error("rational operand limit");if(limbs&&limbs>std::numeric_limits<std::uint64_t>::max()/limbs/4/std::max<std::uint64_t>(1,bits))throw std::length_error("rational work estimate overflow");const auto work=std::max<std::uint64_t>(1,static_cast<std::uint64_t>(bits)*limbs*limbs*4);if(work>l.maxArithmeticWork)throw std::length_error("rational arithmetic work limit");const auto live=payload(BoundedBigInteger::bitLength(a.n_))+payload(BoundedBigInteger::bitLength(a.d_))+payload(BoundedBigInteger::bitLength(b.n_))+payload(BoundedBigInteger::bitLength(b.d_));const auto estimate=bits>(std::numeric_limits<std::size_t>::max()-2)/2?std::numeric_limits<std::size_t>::max():bits*2+2;if(live>l.maxAllocatedBytes||estimate>l.maxIntermediateBits||payload(estimate)>l.maxAllocatedBytes-live)throw std::length_error("rational intermediate storage limit");}
    static void check(const integer_type&x,const BigIntegerLimits&l){auto bits=BoundedBigInteger::bitLength(x);if(bits>l.maxBitLength||bits>l.maxIntermediateBits||BoundedBigInteger::payloadBytes(bits)>l.maxAllocatedBytes)throw std::length_error("rational integer limit");}
    void enforce(const integer_type&n,const integer_type&d)const { check(n,limits_);check(d,limits_);if(BoundedBigInteger::payloadBytes(BoundedBigInteger::bitLength(n))+BoundedBigInteger::payloadBytes(BoundedBigInteger::bitLength(d))>limits_.maxAllocatedBytes)throw std::length_error("rational allocation limit"); }
};
