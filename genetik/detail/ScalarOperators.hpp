#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>
// Append-only IDs. Ordinary scalar operators all map Scalar^arity -> Scalar.
// Powi exponents are discrete metadata, never fitted coefficients.
enum class ExpressionKind {
    Variable,
    Constant,
    Add,
    Subtract,
    Multiply,
    Divide,
    Log,
    Sin,
    Cos,
    Exp,
    Sqrt,
    Powi,
    NegativePowi,
    Pow,
    Cbrt,
    Log1p,
    Log2,
    Log10,
    Expm1,
    Tan,
    Asin,
    Acos,
    Atan,
    Atan2,
    Sinh,
    Cosh,
    Tanh,
    Abs,
    Min,
    Max,
    Clamp, Erf, Erfc, Gamma, LogGamma, BesselJ0, NormalPdf, NormalCdf, PoissonPmf
};
struct ScalarOperatorDescription {
    ExpressionKind kind;
    std::string_view name;
    unsigned arity;
};
inline constexpr std::array scalarOperators{
    ScalarOperatorDescription{ExpressionKind::Variable, "variable", 0},
    ScalarOperatorDescription{ExpressionKind::Constant, "constant", 0},
    ScalarOperatorDescription{ExpressionKind::Add, "+", 2},
    ScalarOperatorDescription{ExpressionKind::Subtract, "-", 2},
    ScalarOperatorDescription{ExpressionKind::Multiply, "*", 2},
    ScalarOperatorDescription{ExpressionKind::Divide, "/", 2},
    ScalarOperatorDescription{ExpressionKind::Log, "log", 1},
    ScalarOperatorDescription{ExpressionKind::Sin, "sin", 1},
    ScalarOperatorDescription{ExpressionKind::Cos, "cos", 1},
    ScalarOperatorDescription{ExpressionKind::Exp, "exp", 1},
    ScalarOperatorDescription{ExpressionKind::Sqrt, "sqrt", 1},
    ScalarOperatorDescription{ExpressionKind::Powi, "powi", 1},
    ScalarOperatorDescription{ExpressionKind::NegativePowi, "powi", 1},
    ScalarOperatorDescription{ExpressionKind::Pow, "pow", 2},
    ScalarOperatorDescription{ExpressionKind::Cbrt, "cbrt", 1},
    ScalarOperatorDescription{ExpressionKind::Log1p, "log1p", 1},
    ScalarOperatorDescription{ExpressionKind::Log2, "log2", 1},
    ScalarOperatorDescription{ExpressionKind::Log10, "log10", 1},
    ScalarOperatorDescription{ExpressionKind::Expm1, "expm1", 1},
    ScalarOperatorDescription{ExpressionKind::Tan, "tan", 1},
    ScalarOperatorDescription{ExpressionKind::Asin, "asin", 1},
    ScalarOperatorDescription{ExpressionKind::Acos, "acos", 1},
    ScalarOperatorDescription{ExpressionKind::Atan, "atan", 1},
    ScalarOperatorDescription{ExpressionKind::Atan2, "atan2", 2},
    ScalarOperatorDescription{ExpressionKind::Sinh, "sinh", 1},
    ScalarOperatorDescription{ExpressionKind::Cosh, "cosh", 1},
    ScalarOperatorDescription{ExpressionKind::Tanh, "tanh", 1},
    ScalarOperatorDescription{ExpressionKind::Abs, "abs", 1},
    ScalarOperatorDescription{ExpressionKind::Min, "min", 2},
    ScalarOperatorDescription{ExpressionKind::Max, "max", 2},
    ScalarOperatorDescription{ExpressionKind::Clamp, "clamp", 3},
    ScalarOperatorDescription{ExpressionKind::Erf,"erf",1},
    ScalarOperatorDescription{ExpressionKind::Erfc,"erfc",1},
    ScalarOperatorDescription{ExpressionKind::Gamma,"gamma",1},
    ScalarOperatorDescription{ExpressionKind::LogGamma,"log_gamma",1},
    ScalarOperatorDescription{ExpressionKind::BesselJ0,"bessel_j0",1},
    ScalarOperatorDescription{ExpressionKind::NormalPdf,"normal_pdf",3},
    ScalarOperatorDescription{ExpressionKind::NormalCdf,"normal_cdf",3},
    ScalarOperatorDescription{ExpressionKind::PoissonPmf,"poisson_pmf",2}};
inline const ScalarOperatorDescription &scalarOperatorDescription(ExpressionKind kind) {
    const auto i = static_cast<std::size_t>(kind);
    if (i >= scalarOperators.size() || scalarOperators[i].kind != kind)
        throw std::invalid_argument("unsupported scalar operator ID");
    return scalarOperators[i];
}
inline std::size_t expressionArity(ExpressionKind kind) { return scalarOperatorDescription(kind).arity; }
inline std::optional<ExpressionKind> scalarOperatorFromName(std::string_view name) {
    for (auto d : scalarOperators)
        if (d.arity && d.name == name)
            return d.kind;
    return {};
}
inline bool expressionPowerMetadata(ExpressionKind kind) noexcept {
    return kind == ExpressionKind::Powi || kind == ExpressionKind::NegativePowi;
}
inline std::size_t expressionOperationCost(ExpressionKind kind, unsigned exponent = 0) {
    return kind == ExpressionKind::Powi           ? (exponent ? exponent - 1 : 0)
           : kind == ExpressionKind::NegativePowi ? exponent
                                                  : 1;
}
