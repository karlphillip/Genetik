#pragma once
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <span>
#include <algorithm>

// Shared internal kernels; expression headers enforce the strict binary64 build contract.
namespace genetik_detail {
enum class ScientificKind { Sin, Cos, Exp, Sqrt, Powi };
inline void validateExponent(unsigned exponent) {
    if (exponent > 8) throw std::invalid_argument("integer power exponent must be in [0,8]");
}
inline void validateExponents(std::span<const unsigned> exponents) {
    if (exponents.empty() || exponents.size() > 9) throw std::invalid_argument("power exponent subset requires 1..9 values");
    for (std::size_t i=0;i<exponents.size();++i) {
        validateExponent(exponents[i]);
        if (std::find(exponents.begin(),exponents.begin()+i,exponents[i])!=exponents.begin()+i)
            throw std::invalid_argument("duplicate power exponent");
    }
}
inline std::string scientificName(ScientificKind kind) {
    switch(kind) {
    case ScientificKind::Sin:return "sin"; case ScientificKind::Cos:return "cos";
    case ScientificKind::Exp:return "exp"; case ScientificKind::Sqrt:return "sqrt";
    case ScientificKind::Powi:return "powi";
    }
    throw std::logic_error("unsupported scientific operator");
}
template<class Charge>
std::optional<double> scientificScalar(ScientificKind kind, double base, unsigned exponent, Charge charge) {
    if (kind == ScientificKind::Powi) {
        validateExponent(exponent);
        if (!exponent) return 1.0;
        double value = base;
        for (unsigned i=1;i<exponent;++i) {
            charge(); volatile double next = value * base;
            if (!std::isfinite(next)) return {};
            value = next;
        }
        return value;
    }
    if (kind == ScientificKind::Sqrt && base < 0.0) return {};
    charge(); volatile double value = 0;
    switch(kind) {
    case ScientificKind::Sin: value=std::sin(base);break;
    case ScientificKind::Cos: value=std::cos(base);break;
    case ScientificKind::Exp: value=std::exp(base);break;
    case ScientificKind::Sqrt:value=std::sqrt(base);break;
    default:throw std::logic_error("unreachable scientific operator");
    }
    if (!std::isfinite(value)) return {};
    return value;
}
}
