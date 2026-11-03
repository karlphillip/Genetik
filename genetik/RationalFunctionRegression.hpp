#pragma once

// Optional bounded rational-function regression. This facility is independent
// of symbolicRegression() and the generic genetic algorithm.
#include "Expression.hpp"
#include "LevenbergMarquardt.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

enum class RationalPredictionStatus { Valid, Invalid, Uncertain };
struct RationalPredictionResult {
    RationalPredictionStatus status = RationalPredictionStatus::Invalid;
    std::optional<double> value;
    std::string reason;
};

namespace genetik_rational_detail {
struct LeastSquaresResult { std::vector<double> coefficients; double pivotRatio = 1.0; };
inline LeastSquaresResult householderLeastSquaresDetailed(
    const std::vector<std::vector<double>>& design,
    const std::vector<double>& targetValues,
    std::size_t maximumRows = 10000,
    std::size_t maximumColumns = 16,
    std::size_t maximumBytes = 32 * 1024 * 1024) {
    if (design.empty() || design.size() != targetValues.size() ||
        design.front().empty() || design.size() > maximumRows ||
        design.front().size() > design.size() ||
        design.front().size() > maximumColumns)
        throw std::invalid_argument("least-squares shape or resource limit");
    const std::size_t n = design.size(), d = design.front().size();
    if (n > maximumBytes / sizeof(long double) / d)
        throw std::length_error("least-squares workspace limit");
    std::vector<std::vector<long double>> a(n, std::vector<long double>(d));
    std::vector<long double> b(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (design[i].size() != d || !std::isfinite(targetValues[i]))
            throw std::invalid_argument("least-squares data");
        b[i] = targetValues[i];
        for (std::size_t j = 0; j < d; ++j) {
            if (!std::isfinite(design[i][j]))
                throw std::invalid_argument("least-squares data");
            a[i][j] = design[i][j];
        }
    }
    std::vector<std::size_t> permutation(d);
    for (std::size_t j = 0; j < d; ++j) permutation[j] = j;
    long double largestDiagonal = 0, smallestDiagonal = std::numeric_limits<long double>::infinity();
    for (std::size_t k = 0; k < d; ++k) {
        std::size_t pivot = k;
        long double norm = 0;
        for (std::size_t j = k; j < d; ++j) {
            long double columnNorm = 0;
            for (std::size_t i = k; i < n; ++i) columnNorm = std::hypot(columnNorm, a[i][j]);
            if (columnNorm > norm) { norm = columnNorm; pivot = j; }
        }
        if (k == 0) largestDiagonal = norm;
        const auto threshold = largestDiagonal * 128 * std::numeric_limits<long double>::epsilon() *
                               static_cast<long double>(std::max(n,d));
        if (!std::isfinite(norm) || norm <= threshold) throw std::invalid_argument("rank-deficient or ill-conditioned design");
        if (pivot != k) {
            for (std::size_t i = 0; i < n; ++i) std::swap(a[i][k], a[i][pivot]);
            std::swap(permutation[k], permutation[pivot]);
        }
        const auto alpha = -std::copysign(norm, a[k][k]);
        std::vector<long double> v(n - k);
        for (std::size_t i = k; i < n; ++i) v[i-k] = a[i][k];
        v[0] -= alpha;
        long double vv = 0;
        for (auto value : v) vv += value * value;
        if (!(vv > 0) || !std::isfinite(vv))
            throw std::invalid_argument("rank-deficient reflector");
        for (std::size_t j = k; j < d; ++j) {
            long double dot = 0;
            for (std::size_t i = k; i < n; ++i) dot += v[i-k] * a[i][j];
            for (std::size_t i = k; i < n; ++i) a[i][j] -= 2 * v[i-k] * dot / vv;
        }
        long double dot = 0;
        for (std::size_t i = k; i < n; ++i) dot += v[i-k] * b[i];
        for (std::size_t i = k; i < n; ++i) b[i] -= 2 * v[i-k] * dot / vv;
        const auto diagonal = std::abs(a[k][k]);
        largestDiagonal = std::max(largestDiagonal, diagonal);
        smallestDiagonal = std::min(smallestDiagonal, diagonal);
    }
    std::vector<double> result(d);
    std::vector<long double> x(d);
    for (std::size_t k = d; k-- > 0;) {
        long double value = b[k];
        for (std::size_t j = k + 1; j < d; ++j) value -= a[k][j] * x[j];
        const auto rankThreshold = largestDiagonal * 128 * std::numeric_limits<long double>::epsilon() *
                                   static_cast<long double>(std::max(n, d));
        if (!std::isfinite(a[k][k]) || std::abs(a[k][k]) <= rankThreshold)
            throw std::invalid_argument("rank-deficient triangular system");
        x[k] = value / a[k][k];
        const auto original = permutation[k];
        result[original] = static_cast<double>(x[k]);
        if (!std::isfinite(result[original])) throw std::overflow_error("nonfinite least-squares coefficient");
    }
    return {std::move(result), static_cast<double>(smallestDiagonal / largestDiagonal)};
}
inline std::vector<double> householderLeastSquares(
    const std::vector<std::vector<double>>& design,
    const std::vector<double>& targetValues,
    std::size_t maximumRows = 10000,
    std::size_t maximumColumns = 16,
    std::size_t maximumBytes = 32 * 1024 * 1024) {
    return householderLeastSquaresDetailed(design, targetValues, maximumRows,
                                           maximumColumns, maximumBytes).coefficients;
}

struct Observation { double x = 0, y = 0; };
enum class Status { Success, InvalidInput, RankDeficient, ResourceLimit, InvalidModel, FitFailure };
inline const char* rationalStatusName(Status s) {
    switch (s) {
    case Status::Success: return "success";
    case Status::InvalidInput: return "invalid_input";
    case Status::RankDeficient: return "rank_deficient";
    case Status::ResourceLimit: return "resource_limit";
    case Status::InvalidModel: return "invalid_model";
    case Status::FitFailure: return "fit_failure";
    }
    return "unknown";
}
inline const char* lmStopName(LMStop s) {
    switch(s){case LMStop::GradientTolerance:return "gradient_tolerance";case LMStop::StepTolerance:return "step_tolerance";
    case LMStop::LossTolerance:return "loss_tolerance";case LMStop::IterationLimit:return "iteration_limit";
    case LMStop::EvaluationLimit:return "evaluation_limit";case LMStop::DerivativeLimit:return "derivative_limit";
    case LMStop::WorkspaceLimit:return "workspace_limit";case LMStop::SingularSystem:return "singular_system";
    case LMStop::InvalidStart:return "invalid_start";case LMStop::DampingLimit:return "damping_limit";case LMStop::Cancelled:return "cancelled";}
    return "unknown";
}
struct Config {
    unsigned maximumNumeratorDegree = 3;
    unsigned maximumDenominatorDegree = 3;
    std::size_t maximumRows = 10000;
    std::size_t maximumParameters = 8;
    std::size_t maximumWorkspaceBytes = 32 * 1024 * 1024;
    std::size_t maximumArithmeticOperations = 1000000;
    std::size_t maximumLinearAlgebraOperations = 50000000;
    double maximumAbsoluteCoefficient = 1e6;
    double minimumAbsoluteDenominator = 1e-10;
    double complexityWeight = 1.0; // BIC-style, training-only ranking.
    std::vector<ExpressionKind> operators{ExpressionKind::Add,ExpressionKind::Multiply,
                                          ExpressionKind::Divide,ExpressionKind::Powi};
    ExpressionLimits expressionLimits{127,16,1e6,1024};
    bool refineWithLM = false;
    LMConfig lm{};
};
struct Model {
    unsigned numeratorDegree = 0, denominatorDegree = 0;
    std::vector<double> numerator, denominator; // denominator[0] == 1
    Expression expression{std::array{ExpressionNode::constant(0)}, 1};
    Expression denominatorExpression{std::array{ExpressionNode::constant(1)}, 1};
    double pivotRatio = 0;
    double trainingSSE = std::numeric_limits<double>::infinity();
    double trainingScore = std::numeric_limits<double>::infinity();
    LMWork lmWork{};
    LMStop lmStop = LMStop::InvalidStart;
    bool refined = false;
    std::optional<double> predict(double x, double minimumDenominator = 1e-10) const;
};

inline Expression parameterizedExpression(unsigned m, unsigned n, std::size_t parameters, const Config& c);
inline Expression materialize(const Expression& parameterized, std::span<const double> theta,
                              double maximumAbsoluteCoefficient);
inline Expression materializeDenominator(unsigned degree, std::span<const double> theta,
                                         const ExpressionLimits& limits, double coefficientBound);

inline bool sameNode(const ExpressionNode& a, const ExpressionNode& b) {
    if (a.kind() != b.kind()) return false;
    if (a.kind() == ExpressionKind::Variable) return a.variableIndex() == b.variableIndex();
    if (a.kind() == ExpressionKind::Constant)
        return std::bit_cast<std::uint64_t>(a.constantValue()) == std::bit_cast<std::uint64_t>(b.constantValue());
    if (expressionPowerMetadata(a.kind())) return a.exponent() == b.exponent();
    return true;
}
inline bool sameNodes(const Expression& a, const Expression& b) {
    if (a.variableCount() != b.variableCount() || a.nodeCount() != b.nodeCount()) return false;
    const auto an = a.nodes(), bn = b.nodes();
    for (std::size_t i = 0; i < an.size(); ++i) if (!sameNode(an[i], bn[i])) return false;
    return true;
}
inline bool representationConsistent(const Model& model) {
    if (model.numeratorDegree > 3 || model.denominatorDegree > 3 ||
        model.numerator.size() != static_cast<std::size_t>(model.numeratorDegree) + 1 ||
        model.denominator.size() != static_cast<std::size_t>(model.denominatorDegree) + 1 ||
        model.denominator.empty() ||
        std::bit_cast<std::uint64_t>(model.denominator.front()) != std::bit_cast<std::uint64_t>(1.0))
        return false;
    for (double value : model.numerator) if (!std::isfinite(value)) return false;
    for (double value : model.denominator) if (!std::isfinite(value)) return false;
    try {
        std::vector<double> theta(model.denominatorDegree + model.numeratorDegree + 1);
        for (unsigned j = 1; j <= model.denominatorDegree; ++j) theta[j - 1] = model.denominator[j];
        for (unsigned j = 0; j <= model.numeratorDegree; ++j)
            theta[model.denominatorDegree + j] = model.numerator[j];
        double coefficientBound = 1.0;
        for (double value : theta) coefficientBound = std::max(coefficientBound, std::abs(value));
        Config config;
        config.expressionLimits = model.expression.limits();
        config.maximumAbsoluteCoefficient = coefficientBound;
        const auto parameterized = parameterizedExpression(model.numeratorDegree, model.denominatorDegree,
                                                            theta.size(), config);
        const auto expected = materialize(parameterized, theta, coefficientBound);
        const auto expectedDenominator = materializeDenominator(model.denominatorDegree, theta,
                                                                  model.denominatorExpression.limits(),
                                                                  coefficientBound);
        return sameNodes(model.expression, expected) && sameNodes(model.denominatorExpression, expectedDenominator);
    } catch (const std::exception&) { return false; }
}
inline std::optional<double> Model::predict(double x, double minimumDenominator) const {
    if (!representationConsistent(*this) || !std::isfinite(x) ||
        !std::isfinite(minimumDenominator) || minimumDenominator < 0) return {};
    const auto q = denominatorExpression.evaluate(std::array{x});
    if (!q.valid() || std::abs(q.value()) < minimumDenominator) return {};
    const auto value = expression.evaluate(std::array{x});
    if (!value.valid()) return {};
    return value.value();
}
struct FitResult {
    Status status = Status::InvalidInput;
    std::string detail;
    std::vector<Model> models; // fixed family order, retaining failure-free fits
    std::optional<std::size_t> selected;
    std::size_t attemptedFamilies = 0, rankFailures = 0, invalidModels = 0, trainingLossEvaluations = 0;
    std::size_t peakWorkspaceEstimateBytes = 0, arithmeticOperations = 0, linearAlgebraOperations = 0;
    LMWork lmWork{};
};

inline void validateConfig(const Config& c) {
    if (c.maximumNumeratorDegree > 3 || c.maximumDenominatorDegree > 3 ||
        !c.maximumRows || !c.maximumParameters || c.maximumParameters > 8 ||
        !c.maximumWorkspaceBytes || !c.maximumArithmeticOperations || !c.maximumLinearAlgebraOperations ||
        !std::isfinite(c.maximumAbsoluteCoefficient) || c.maximumAbsoluteCoefficient <= 0 ||
        !std::isfinite(c.minimumAbsoluteDenominator) || c.minimumAbsoluteDenominator <= 0 ||
        !std::isfinite(c.complexityWeight) || c.complexityWeight < 0)
        throw std::invalid_argument("invalid S13 rational discovery configuration");
    if(c.maximumAbsoluteCoefficient>c.expressionLimits.maxAbsConstant)
        throw std::invalid_argument("coefficient limit exceeds expression constant limit");
    if(c.expressionLimits.maxAbsConstant < 1.0)
        throw std::invalid_argument("expression constant limit excludes fixed denominator normalization Q(0)=1");
    (void)Expression::constant(0,1,c.expressionLimits);
    for(auto required:{ExpressionKind::Add,ExpressionKind::Multiply,ExpressionKind::Divide})
        if(std::find(c.operators.begin(),c.operators.end(),required)==c.operators.end())
            throw std::invalid_argument("rational model grammar omits a required operator");
    if((c.maximumNumeratorDegree>1||c.maximumDenominatorDegree>1) &&
       std::find(c.operators.begin(),c.operators.end(),ExpressionKind::Powi)==c.operators.end())
        throw std::invalid_argument("rational model grammar omits integer power");
    for(std::size_t i=0;i<c.operators.size();++i)
        if(std::find(c.operators.begin(),c.operators.begin()+static_cast<std::ptrdiff_t>(i),c.operators[i])!=c.operators.begin()+static_cast<std::ptrdiff_t>(i))
            throw std::invalid_argument("duplicate rational grammar operator");
    for(auto op:c.operators)if(op!=ExpressionKind::Add&&op!=ExpressionKind::Multiply&&
       op!=ExpressionKind::Divide&&op!=ExpressionKind::Powi)
        throw std::invalid_argument("unsupported operator in rational model grammar");
}
inline std::size_t maximumLmLinearAlgebraPerIteration(std::size_t rows, std::size_t parameters) {
    // Conservative upper bound for Jacobian normal-matrix construction,
    // rank inspection, and all sixteen damped-solve retries in one LM step.
    const long double p = static_cast<long double>(parameters);
    const long double m = static_cast<long double>(rows);
    const long double bound = 2.0L*m*p*(p+1.0L) + 4.0L*p*p*p + 4.0L*p*p +
                              16.0L*(2.0L*p+p*p*p) + 1.0L;
    if (!std::isfinite(bound) || bound > static_cast<long double>(std::numeric_limits<std::size_t>::max()))
        return std::numeric_limits<std::size_t>::max();
    return static_cast<std::size_t>(std::ceil(bound));
}
inline std::size_t combinedLinearAlgebraWork(const FitResult& result) {
    if (result.lmWork.linearAlgebraOperations >
        std::numeric_limits<std::size_t>::max() - result.linearAlgebraOperations)
        return std::numeric_limits<std::size_t>::max();
    return result.linearAlgebraOperations + result.lmWork.linearAlgebraOperations;
}
inline std::vector<ExpressionNode> polynomialNodes(unsigned degree, std::size_t firstParameter) {
    using K = ExpressionKind;
    std::vector<ExpressionNode> out;
    auto term = [&](unsigned j) {
        out.push_back(ExpressionNode::variable(firstParameter + j));
        if (j) {
            out.push_back(ExpressionNode::variable(0));
            if (j > 1) out.push_back(ExpressionNode::power(j));
            out.push_back(ExpressionNode::binary(K::Multiply));
        }
    };
    term(0);
    for (unsigned j = 1; j <= degree; ++j) {
        term(j); out.push_back(ExpressionNode::binary(K::Add));
    }
    return out;
}
inline std::vector<ExpressionNode> denominatorNodes(unsigned degree) {
    using K = ExpressionKind;
    std::vector<ExpressionNode> out{ExpressionNode::constant(1)};
    for (unsigned j = 1; j <= degree; ++j) {
        out.push_back(ExpressionNode::variable(j));
        out.push_back(ExpressionNode::variable(0));
        if (j > 1) out.push_back(ExpressionNode::power(j));
        out.push_back(ExpressionNode::binary(K::Multiply));
        out.push_back(ExpressionNode::binary(K::Add));
    }
    return out;
}
inline Expression parameterizedExpression(unsigned m, unsigned n, std::size_t parameters,
                                           const Config& c) {
    auto nodes = polynomialNodes(m, n + 1);
    const auto q = denominatorNodes(n);
    nodes.insert(nodes.end(), q.begin(), q.end());
    nodes.push_back(ExpressionNode::binary(ExpressionKind::Divide));
    return Expression(nodes, parameters + 1, c.expressionLimits);
}
inline Expression materialize(const Expression& parameterized, std::span<const double> theta,
                              double maximumAbsoluteCoefficient) {
    std::vector<ExpressionNode> nodes;
    nodes.reserve(parameterized.nodeCount());
    for (const auto& node : parameterized.nodes()) {
        if (node.kind() == ExpressionKind::Variable && node.variableIndex() != 0) {
            const auto index = node.variableIndex() - 1;
            if (index >= theta.size() || !std::isfinite(theta[index]) ||
                std::abs(theta[index]) > maximumAbsoluteCoefficient)
                throw std::invalid_argument("coefficient outside configured bounds");
            nodes.push_back(ExpressionNode::constant(theta[index]));
        } else nodes.push_back(node);
    }
    return Expression(nodes, 1, parameterized.limits());
}
inline Expression materializeDenominator(unsigned degree,std::span<const double> theta,
                                         const ExpressionLimits& limits,double coefficientBound) {
    auto nodes=denominatorNodes(degree);
    for(auto& node:nodes)if(node.kind()==ExpressionKind::Variable&&node.variableIndex()!=0) {
        const auto index=node.variableIndex()-1;
        if(index>=degree||!std::isfinite(theta[index])||std::abs(theta[index])>coefficientBound)
            throw std::invalid_argument("denominator coefficient outside bounds");
        node=ExpressionNode::constant(theta[index]);
    }
    return Expression(nodes,1,limits);
}
inline std::vector<double> polynomialCoefficients(std::span<const double> theta,
                                                   unsigned m, unsigned n) {
    std::vector<double> p(m + 1);
    for (unsigned j = 0; j <= m; ++j) p[j] = theta[n + j];
    return p;
}
inline std::vector<double> denominatorCoefficients(std::span<const double> theta, unsigned n) {
    std::vector<double> q(n + 1); q[0] = 1;
    for (unsigned j = 1; j <= n; ++j) q[j] = theta[j - 1];
    return q;
}
inline double sse(const Model& model, std::span<const Observation> rows,
                  double minimumDenominator, std::size_t& operations) {
    double sum = 0;
    for (const auto& row : rows) {
        auto prediction = model.predict(row.x, minimumDenominator);
        if (!prediction) return std::numeric_limits<double>::infinity();
        const auto work = model.expression.arithmeticCost()+model.denominatorExpression.arithmeticCost()+3;
        if (work > std::numeric_limits<std::size_t>::max() - operations)
            throw std::overflow_error("S13 operation counter overflow");
        operations += work;
        volatile double residual = *prediction - row.y;
        volatile double square = residual * residual;
        sum += square;
        if (!std::isfinite(sum)) return std::numeric_limits<double>::infinity();
    }
    return sum;
}
inline FitResult discover(std::span<const Observation> rows, const Config& c = {}) {
    validateConfig(c);
    FitResult out;
    if (rows.empty() || rows.size() > c.maximumRows) {
        out.status = rows.empty() ? Status::InvalidInput : Status::ResourceLimit;
        out.detail = "training row count outside configured bound"; return out;
    }
    double maxY = 0;
    for (const auto& r : rows) {
        if (!std::isfinite(r.x) || !std::isfinite(r.y)) {
            out.status = Status::InvalidInput; out.detail = "nonfinite observation"; return out;
        }
        maxY = std::max(maxY, std::abs(r.y));
    }
    const double scoreFloor = std::numeric_limits<double>::epsilon() * (maxY + 1.0);
    for (unsigned n = 0; n <= c.maximumDenominatorDegree; ++n) {
        for (unsigned m = 0; m <= c.maximumNumeratorDegree; ++m) {
            const std::size_t p = n + m + 1;
            if (p > c.maximumParameters || rows.size() < p) continue;
            ++out.attemptedFamilies;
            const long double designWorkspace=static_cast<long double>(rows.size())*p*sizeof(double)+
                static_cast<long double>(rows.size())*(sizeof(std::vector<double>)+sizeof(double));
            const long double qrWorkspace=static_cast<long double>(rows.size())*p*sizeof(long double)+
                static_cast<long double>(rows.size())*(sizeof(std::vector<long double>)+2*sizeof(long double));
            const long double peakWorkspace=designWorkspace+qrWorkspace+static_cast<long double>(p)*4*sizeof(long double);
            if (peakWorkspace > static_cast<long double>(c.maximumWorkspaceBytes) ||
                peakWorkspace > static_cast<long double>(std::numeric_limits<std::size_t>::max())) {
                out.status = Status::ResourceLimit; out.detail = "design workspace limit"; return out;
            }
            std::vector<std::vector<double>> design;
            std::vector<double> rhs;
            design.reserve(rows.size()); rhs.reserve(rows.size());
            std::size_t ops = 0;
            for (const auto& row : rows) {
                std::vector<double> powers(m + n + 1, 1.0);
                for (std::size_t j = 1; j < powers.size(); ++j) {
                    powers[j] = powers[j - 1] * row.x;
                    if (!std::isfinite(powers[j])) { out.status = Status::InvalidInput; out.detail = "power basis overflow"; return out; }
                }
                std::vector<double> values; values.reserve(p);
                for (unsigned j = 1; j <= n; ++j) values.push_back(row.y * powers[j]);
                for (unsigned j = 0; j <= m; ++j) values.push_back(-powers[j]);
                for (double v : values) if (!std::isfinite(v)) { out.status=Status::InvalidInput;out.detail="linearized design overflow";return out; }
                design.push_back(std::move(values)); rhs.push_back(-row.y);
                ops += p * 4 + m + n;
                if (ops > c.maximumArithmeticOperations-out.arithmeticOperations) { out.status=Status::ResourceLimit;out.detail="design operation allowance";return out; }
            }
            const auto estimatedWorkspace=static_cast<std::size_t>(peakWorkspace);
            out.peakWorkspaceEstimateBytes = std::max(out.peakWorkspaceEstimateBytes, estimatedWorkspace);
            out.arithmeticOperations += ops;
            const long double qrEstimate=2.0L*rows.size()*p*p-(2.0L/3.0L)*p*p*p+2.0L*p*p;
            const auto usedLinearAlgebra = combinedLinearAlgebraWork(out);
            if(usedLinearAlgebra > c.maximumLinearAlgebraOperations ||
               qrEstimate>static_cast<long double>(std::numeric_limits<std::size_t>::max()-out.linearAlgebraOperations) ||
               qrEstimate>static_cast<long double>(c.maximumLinearAlgebraOperations-usedLinearAlgebra)) {
                out.status=Status::ResourceLimit;out.detail="linear algebra work counter bound";return out;
            }
            out.linearAlgebraOperations+=static_cast<std::size_t>(std::max(0.0L,qrEstimate));
            std::vector<double> theta;
            double pivotRatio = 0;
            try {
                const auto solved = householderLeastSquaresDetailed(
                    design, rhs, c.maximumRows, c.maximumParameters,
                    c.maximumWorkspaceBytes-static_cast<std::size_t>(designWorkspace));
                theta = solved.coefficients; pivotRatio = solved.pivotRatio;
            } catch (const std::invalid_argument&) { ++out.rankFailures; continue; }
              catch (const std::length_error&) { out.status=Status::ResourceLimit;out.detail="QR workspace limit";return out; }
            std::vector<std::vector<double>>().swap(design);
            std::vector<double>().swap(rhs);
            if (std::any_of(theta.begin(), theta.end(), [&](double v) { return std::abs(v)>c.maximumAbsoluteCoefficient; })) {
                ++out.invalidModels; continue;
            }
            Model model;
            model.numeratorDegree=m;model.denominatorDegree=n;model.pivotRatio=pivotRatio;
            model.numerator=polynomialCoefficients(theta,m,n);
            model.denominator=denominatorCoefficients(theta,n);
            const auto parameterized=parameterizedExpression(m,n,p,c);
            if (c.refineWithLM) {
                std::vector<LMExpressionObservation> lmRows;
                lmRows.reserve(rows.size());
                for (const auto& row : rows) {
                    std::vector<double> inputs(p+1); inputs[0]=row.x;
                    std::copy(theta.begin(),theta.end(),inputs.begin()+1);
                    lmRows.push_back({std::move(inputs),row.y});
                }
                std::vector<std::size_t> bindings(p);
                for(std::size_t j=0;j<p;++j)bindings[j]=j+1;
                auto lmConfig=c.lm;
                lmConfig.maxWorkspaceBytes=std::min(lmConfig.maxWorkspaceBytes,c.maximumWorkspaceBytes);
                const auto usedBeforeLm = combinedLinearAlgebraWork(out);
                if (usedBeforeLm > c.maximumLinearAlgebraOperations) {
                    out.status=Status::ResourceLimit;out.detail="combined linear algebra allowance";return out;
                }
                const auto remainingLinearAlgebra=c.maximumLinearAlgebraOperations-usedBeforeLm;
                const auto perIteration=maximumLmLinearAlgebraPerIteration(rows.size(),p);
                const auto allowedIterations=perIteration ? remainingLinearAlgebra/perIteration : 0;
                if (!allowedIterations) {
                    out.status=Status::ResourceLimit;out.detail="insufficient linear algebra allowance for bounded LM iteration";return out;
                }
                lmConfig.maxIterations=std::min(lmConfig.maxIterations,allowedIterations);
                if(out.lmWork.mathematicalOperations>c.maximumArithmeticOperations-out.arithmeticOperations) {
                    out.status=Status::ResourceLimit;out.detail="LM mathematical-operation allowance";return out;
                }
                const auto remainingOps=c.maximumArithmeticOperations-out.arithmeticOperations-out.lmWork.mathematicalOperations;
                if(!remainingOps){out.status=Status::ResourceLimit;out.detail="LM mathematical-operation allowance";return out;}
                lmConfig.maxMathematicalOperations=std::min(lmConfig.maxMathematicalOperations,remainingOps);
                auto residual=expressionLeastSquares(parameterized,std::move(lmRows),bindings,
                    remainingOps);
                std::vector<double> lower(p,-c.maximumAbsoluteCoefficient), upper(p,c.maximumAbsoluteCoefficient);
                auto lm=levenbergMarquardt(theta,lower,upper,residual,lmConfig);
                theta=lm.parameters;model.lmWork=lm.work;model.lmStop=lm.stop;model.refined=true;
                auto add=[&](std::size_t& to,std::size_t value){if(value>std::numeric_limits<std::size_t>::max()-to)throw std::overflow_error("S13 LM work overflow");to+=value;};
                add(out.lmWork.objectiveEvaluations,lm.work.objectiveEvaluations);add(out.lmWork.jacobianEvaluations,lm.work.jacobianEvaluations);
                add(out.lmWork.forwardEvaluations,lm.work.forwardEvaluations);add(out.lmWork.derivativeEvaluations,lm.work.derivativeEvaluations);
                add(out.lmWork.rowsProcessed,lm.work.rowsProcessed);add(out.lmWork.parameterUpdates,lm.work.parameterUpdates);
                add(out.lmWork.linearSolves,lm.work.linearSolves);add(out.lmWork.acceptedSteps,lm.work.acceptedSteps);
                add(out.lmWork.rejectedSteps,lm.work.rejectedSteps);add(out.lmWork.mathematicalOperations,lm.work.mathematicalOperations);
                add(out.lmWork.linearAlgebraOperations,lm.work.linearAlgebraOperations);
                out.lmWork.peakWorkspaceBytes=std::max(out.lmWork.peakWorkspaceBytes,lm.work.peakWorkspaceBytes);out.lmWork.seconds+=lm.work.seconds;
                if (combinedLinearAlgebraWork(out) > c.maximumLinearAlgebraOperations) {
                    out.status=Status::ResourceLimit;out.detail="combined linear algebra allowance exceeded";return out;
                }
                model.numerator=polynomialCoefficients(theta,m,n);
                model.denominator=denominatorCoefficients(theta,n);
            }
            try { model.expression=materialize(parameterized,theta,c.maximumAbsoluteCoefficient); }
            catch (const std::exception&) { ++out.invalidModels; continue; }
            try { model.denominatorExpression=materializeDenominator(n,theta,c.expressionLimits,c.maximumAbsoluteCoefficient); }
            catch (const std::exception&) { ++out.invalidModels;continue; }
            const auto modelOperations=model.expression.arithmeticCost()+model.denominatorExpression.arithmeticCost()+3;
            if(out.lmWork.mathematicalOperations>c.maximumArithmeticOperations-out.arithmeticOperations ||
               (rows.size() && modelOperations>(c.maximumArithmeticOperations-out.arithmeticOperations-out.lmWork.mathematicalOperations)/rows.size())) {
                out.status=Status::ResourceLimit;out.detail="prediction evaluation allowance";return out;
            }
            ++out.trainingLossEvaluations;
            model.trainingSSE=sse(model,rows,c.minimumAbsoluteDenominator,out.arithmeticOperations);
            if (!std::isfinite(model.trainingSSE)) {++out.invalidModels;continue;}
            const double meanLoss=std::max(model.trainingSSE/static_cast<double>(rows.size()),scoreFloor*scoreFloor);
            model.trainingScore=static_cast<double>(rows.size())*std::log(meanLoss)+
                c.complexityWeight*static_cast<double>(p)*std::log(static_cast<double>(rows.size()));
            out.models.push_back(std::move(model));
        }
    }
    if (out.models.empty()) {
        out.status = out.rankFailures ? Status::RankDeficient : Status::InvalidModel;
        out.detail = out.rankFailures ? "all eligible model designs were rank deficient" : "no valid rational model";
        return out;
    }
    out.selected=0;
    for(std::size_t i=1;i<out.models.size();++i) {
        const auto& a=out.models[i];const auto& b=out.models[*out.selected];
        if(a.trainingScore<b.trainingScore ||
           (a.trainingScore==b.trainingScore && a.expression.nodeCount()<b.expression.nodeCount())) out.selected=i;
    }
    out.status=Status::Success;out.detail="bounded normalized rational family fitted";
    return out;
}

} // namespace genetik_rational_detail

using RationalObservation = genetik_rational_detail::Observation;
using RationalStatus = genetik_rational_detail::Status;
using RationalConfig = genetik_rational_detail::Config;
using RationalModel = genetik_rational_detail::Model;
using RationalFitResult = genetik_rational_detail::FitResult;
using genetik_rational_detail::rationalStatusName;
inline RationalFitResult fitRationalFunction(std::span<const RationalObservation> rows,
                                              const RationalConfig& config = {}) {
    return genetik_rational_detail::discover(rows, config);
}
inline const char* rationalPredictionStatusName(RationalPredictionStatus status) {
    switch (status) {
    case RationalPredictionStatus::Valid: return "valid";
    case RationalPredictionStatus::Invalid: return "invalid";
    case RationalPredictionStatus::Uncertain: return "uncertain";
    }
    return "uncertain";
}
inline RationalPredictionResult predictRationalChecked(const RationalModel& model, double x,
                                                        double minimumAbsoluteDenominator = 1e-10) {
    if (!genetik_rational_detail::representationConsistent(model))
        return {RationalPredictionStatus::Invalid, {}, "inconsistent_model_representation"};
    if (!std::isfinite(x) || !std::isfinite(minimumAbsoluteDenominator) ||
        minimumAbsoluteDenominator < 0)
        return {RationalPredictionStatus::Invalid, {}, "invalid_input"};
    const auto q = model.denominatorExpression.evaluate(std::array{x});
    if (!q.valid()) return {RationalPredictionStatus::Invalid, {}, "invalid_denominator_expression"};
    const double qValue = q.value();
    if (qValue == 0.0) return {RationalPredictionStatus::Invalid, {}, "zero_denominator"};
    long double scale = 0.0L, power = 1.0L;
    for (double coefficient : model.denominator) {
        scale += std::abs(static_cast<long double>(coefficient)) * power;
        power *= std::abs(static_cast<long double>(x));
    }
    if (!std::isfinite(scale))
        return {RationalPredictionStatus::Uncertain, {}, "denominator_scale_overflow"};
    const long double roundoff = 64.0L * std::numeric_limits<double>::epsilon() * scale;
    if (std::abs(static_cast<long double>(qValue)) <=
        std::max(static_cast<long double>(minimumAbsoluteDenominator), roundoff))
        return {RationalPredictionStatus::Uncertain, {}, "denominator_near_zero"};
    const auto prediction = model.expression.evaluate(std::array{x});
    if (!prediction.valid()) return {RationalPredictionStatus::Invalid, {}, "invalid_rational_expression"};
    return {RationalPredictionStatus::Valid, prediction.value(), "valid"};
}

enum class DomainStatus { Valid, Invalid, Uncertain };
inline const char* domainStatusName(DomainStatus status) {
    switch (status) {
    case DomainStatus::Valid: return "valid";
    case DomainStatus::Invalid: return "invalid";
    case DomainStatus::Uncertain: return "uncertain";
    }
    return "uncertain";
}

struct Interval { double lower = 0, upper = 0; };
struct PoleDiagnostic {
    DomainStatus status = DomainStatus::Uncertain;
    std::string reason = "unresolved";
    Interval assessedInterval{};
    unsigned numeratorDegree = 0, denominatorDegree = 0;
    double maximumNumeratorCoefficient = 0, maximumDenominatorCoefficient = 0;
    double minimumObservedAbsoluteDenominator = std::numeric_limits<double>::infinity();
    std::size_t invalidObservedPredictions = 0;
    std::vector<Interval> possiblePoleIntervals;
    std::size_t intervalCellsChecked = 0;
};

namespace genetik_rational_detail {
struct BoundedInterval { double lo, hi; };
inline double down(double x) { return std::nextafter(x, -std::numeric_limits<double>::infinity()); }
inline double up(double x) { return std::nextafter(x, std::numeric_limits<double>::infinity()); }
inline BoundedInterval add(BoundedInterval a, BoundedInterval b) {
    return {down(a.lo + b.lo), up(a.hi + b.hi)};
}
inline BoundedInterval multiply(BoundedInterval a, BoundedInterval b) {
    const std::array<double,4> v{a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};
    if (std::any_of(v.begin(),v.end(),[](double x){return !std::isfinite(x);}))
        return {-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity()};
    const auto [mn,mx]=std::minmax_element(v.begin(),v.end());
    return {down(*mn),up(*mx)};
}
inline BoundedInterval polynomialRange(std::span<const double> coefficients, double lo, double hi) {
    BoundedInterval x{lo,hi};
    BoundedInterval y{coefficients.back(),coefficients.back()};
    for (std::size_t i=coefficients.size()-1;i>0;--i)
        y=add(multiply(y,x),{coefficients[i-1],coefficients[i-1]});
    return y;
}
inline double polynomialValue(std::span<const double> coefficients,double x) {
    double y=coefficients.back();
    for(std::size_t i=coefficients.size()-1;i>0;--i)y=y*x+coefficients[i-1];
    return y;
}
} // namespace genetik_rational_detail

// Certifies denominator separation from zero over a closed finite interval by
// outward-rounded interval Horner evaluation and bounded subdivision. A root
// or threshold crossing is Invalid when demonstrated; unresolved cells remain
// Uncertain. It never clips a denominator or changes prediction semantics.
inline PoleDiagnostic assessRationalDomain(const RationalModel& model,
                                           std::span<const double> observedInputs,
                                           Interval predictionDomain,
                                           double minimumAbsoluteDenominator = 1e-10,
                                           std::size_t maximumCells = 65536,
                                           double maximumAbsoluteCoefficient = std::numeric_limits<double>::max()) {
    PoleDiagnostic out;
    out.assessedInterval=predictionDomain;
    out.numeratorDegree=model.numeratorDegree;
    out.denominatorDegree=model.denominatorDegree;
    if(!genetik_rational_detail::representationConsistent(model)) {
        out.status=DomainStatus::Invalid;out.reason="inconsistent_model_representation";return out;
    }
    for(double x:model.numerator)out.maximumNumeratorCoefficient=std::max(out.maximumNumeratorCoefficient,std::abs(x));
    for(double x:model.denominator)out.maximumDenominatorCoefficient=std::max(out.maximumDenominatorCoefficient,std::abs(x));
    if(model.numerator.size()!=static_cast<std::size_t>(model.numeratorDegree)+1 ||
       model.denominator.size()!=static_cast<std::size_t>(model.denominatorDegree)+1 ||
       model.denominator.empty() || model.denominator.front()!=1.0 ||
       !std::isfinite(predictionDomain.lower)||!std::isfinite(predictionDomain.upper)||
       predictionDomain.lower>predictionDomain.upper||!std::isfinite(minimumAbsoluteDenominator)||
       minimumAbsoluteDenominator<=0||maximumCells==0||!std::isfinite(maximumAbsoluteCoefficient)||
       maximumAbsoluteCoefficient<=0) {
        out.status=DomainStatus::Invalid; return out;
    }
    for(double c:model.numerator)if(!std::isfinite(c)||std::abs(c)>maximumAbsoluteCoefficient){out.status=DomainStatus::Invalid;return out;}
    for(std::size_t i=1;i<model.denominator.size();++i)
        if(!std::isfinite(model.denominator[i])||std::abs(model.denominator[i])>maximumAbsoluteCoefficient){out.status=DomainStatus::Invalid;return out;}
    for(double x:observedInputs) {
        if(!std::isfinite(x)){++out.invalidObservedPredictions;continue;}
        const double q=genetik_rational_detail::polynomialValue(model.denominator,x);
        if(!std::isfinite(q)||std::abs(q)<minimumAbsoluteDenominator)++out.invalidObservedPredictions;
        else out.minimumObservedAbsoluteDenominator=std::min(out.minimumObservedAbsoluteDenominator,std::abs(q));
    }
    if(out.invalidObservedPredictions){out.status=DomainStatus::Invalid;return out;}
    long double scaleBound = 0.0L, inputMagnitude = std::max(std::abs(predictionDomain.lower), std::abs(predictionDomain.upper));
    long double inputPower = 1.0L;
    for (double coefficient : model.denominator) {
        scaleBound += std::abs(static_cast<long double>(coefficient)) * inputPower;
        inputPower *= inputMagnitude;
    }
    if (!std::isfinite(scaleBound)) { out.status = DomainStatus::Uncertain; return out; }
    const long double threshold = std::max(static_cast<long double>(minimumAbsoluteDenominator),
        64.0L * std::numeric_limits<double>::epsilon() * scaleBound);
    const double checkedThreshold = threshold > std::numeric_limits<double>::max()
        ? std::numeric_limits<double>::max() : static_cast<double>(threshold);
    struct Cell { double lo,hi;unsigned depth; };
    std::vector<Cell> pending{{predictionDomain.lower,predictionDomain.upper,0}};
    bool uncertain=false;
    constexpr unsigned maximumDepth=48;
    while(!pending.empty()) {
        if(out.intervalCellsChecked>=maximumCells){uncertain=true;break;}
        const auto cell=pending.back();pending.pop_back();++out.intervalCellsChecked;
        const auto range=genetik_rational_detail::polynomialRange(model.denominator,cell.lo,cell.hi);
        if(range.lo>checkedThreshold||range.hi< -checkedThreshold)continue;
        const auto qlo=genetik_rational_detail::polynomialRange(model.denominator,cell.lo,cell.lo);
        const auto qhi=genetik_rational_detail::polynomialRange(model.denominator,cell.hi,cell.hi);
        const auto insideThreshold=[&](genetik_rational_detail::BoundedInterval q) {
            return q.lo>=-checkedThreshold&&q.hi<=checkedThreshold;
        };
        const bool oppositeCertified=(qlo.hi < -checkedThreshold && qhi.lo > checkedThreshold)||
                                     (qhi.hi < -checkedThreshold && qlo.lo > checkedThreshold);
        if(insideThreshold(qlo)||insideThreshold(qhi)||oppositeCertified) {
            out.possiblePoleIntervals.push_back({cell.lo,cell.hi});
            out.status=DomainStatus::Invalid;return out;
        }
        const double mid=cell.lo+(cell.hi-cell.lo)*0.5;
        if(cell.depth>=maximumDepth||mid==cell.lo||mid==cell.hi) {
            uncertain=true;out.possiblePoleIntervals.push_back({cell.lo,cell.hi});continue;
        }
        pending.push_back({mid,cell.hi,cell.depth+1});
        pending.push_back({cell.lo,mid,cell.depth+1});
    }
    out.status=uncertain?DomainStatus::Uncertain:DomainStatus::Valid;
    return out;
}
