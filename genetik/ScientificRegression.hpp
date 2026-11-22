#pragma once
#include "BoundedTensor.hpp"
#include "ComplexExpression.hpp"
#include "ScientificEquationSystem.hpp"

struct ComplexObservation {
  std::vector<std::complex<double>> inputs;
  std::complex<double> target;
};
class ComplexRegressionProblem final : public RegressionProblem {
public:
  ComplexRegressionProblem(const std::vector<ComplexObservation> &rows,
                           ExpressionLimits expressionLimits = {},
                           ScientificLimits limits = {})
      : RegressionProblem(MeasurementSchema{schema(rows), expressionLimits}),
        limits_(limits) {
    genetik_science::Budget b(limits);
    if (rows.size() > limits.maxElements)
      throw ScientificResourceExhausted("complex rows");
    std::size_t bytes = 0;
    for (const auto &row : rows) {
      if (row.inputs.size() != dimensions() || !finiteComplex(row.target))
        throw std::invalid_argument("complex row schema");
      for (auto z : row.inputs)
        if (!finiteComplex(z))
          throw std::invalid_argument("complex row input");
      auto count = dimensions() + 1;
      if (count > limits.maxWorkspaceBytes / sizeof(std::complex<double>) ||
          count * sizeof(std::complex<double>) >
              limits.maxWorkspaceBytes - bytes)
        throw ScientificResourceExhausted("complex observations storage");
      bytes += count * sizeof(std::complex<double>);
    }
    rows_ = rows;
  }
  bool parallelEvaluationSafe() const noexcept override { return true; }
  std::size_t sampleCount() const noexcept override { return rows_.size(); }
  const std::vector<ComplexObservation>& observations() const noexcept {
      return rows_;
  }
  ScientificLimits scientificLimits() const noexcept {
      return limits_;
  }
  RegressionMetrics measure(const Expression &genome,
                            std::uint64_t *rows = nullptr) const override {
    RegressionSupplementalWork ignored;
    return measureWithWork(genome, rows, ignored);
  }
  RegressionMetrics
  measureWithWork(const Expression &genome, std::uint64_t *rows,
                  RegressionSupplementalWork &supplemental) const override {
    auto e = ComplexExpression::fromScalarGenome(checkedExpression(genome));
    return genetik_science::accountAssessment(
        [&](ScientificProgress &progress) { return assess(e, &progress); },
        rows, supplemental);
  }
  ScientificAssessment assess(const ComplexExpression &e,
                              ScientificProgress *progress = nullptr) const {
    if (e.variableCount() != dimensions())
      throw std::invalid_argument("complex problem schema");
    ScientificAssessment r;
    genetik_science::Budget b(limits_);
    genetik_science::ProgressScope progressScope{progress, r, b};
    try {
      double loss = 0;
      for (const auto &row : rows_) {
        b.charge();
        ++r.observationsAttempted;
        ScientificLimits left = limits_;
        left.maxOperations -= b.work.operations;
        if (!left.maxOperations)
          throw genetik_science::Stop{ScientificStatus::ResourceLimit,
                                      "complex residual work"};
        ++r.fieldEvaluations;
        auto p = e.evaluate(row.inputs, left);
        b.charge(p.work.operations);
        b.work.workspaceBytes =
            std::max(b.work.workspaceBytes, p.work.workspaceBytes);
        if (!p.success())
          throw genetik_science::Stop{p.status, "complex required prediction"};
        double re = b.sub(p.value->real(), row.target.real()),
               im = b.sub(p.value->imag(), row.target.imag());
        loss = b.add(loss, b.div(b.add(b.mul(re, re), b.mul(im, im)),
                                 static_cast<double>(rows_.size())));
      }
      r.loss = loss;
      r.status = ScientificStatus::Success;
    } catch (const genetik_science::Stop &s) {
      r.status = s.status;
      r.reason = s.reason;
    }
    r.work = b.work;
    return r;
  }
  std::string identity() const {
    CheckpointWriter w;
    w.text("complex-regression;real-coefficient-AST;principal-branches/1");
    ScientificEquationSystem::writeLimits(w, limits_);
    w.integer(dimensions());
    w.integer(rows_.size());
    for (const auto &r : rows_) {
      for (auto z : r.inputs) {
        w.real(z.real());
        w.real(z.imag());
      }
      w.real(r.target.real());
      w.real(r.target.imag());
    }
    return w.data();
  }

private:
  static std::size_t schema(const std::vector<ComplexObservation> &rows) {
    if (rows.empty() || rows.front().inputs.empty())
      throw std::invalid_argument("nonempty complex regression");
    return rows.front().inputs.size();
  }
  std::vector<ComplexObservation> rows_;
  ScientificLimits limits_;
};
// Explicit opt-in fitting for the complex regression adapter. Each executable
// complex constant contributes independent real and imaginary LM coordinates.
// The ordinary scalar constant fitter remains real-only and unchanged.
struct ComplexCoefficientFitConfig {
    double lower = -1e6;
    double upper = 1e6;
    LMConfig optimizer{};
};
struct ComplexCoefficientFitResult {
    ComplexExpression expression;
    Fitness trainingLoss;
    Fitness startingLoss;
    LMWork work{};
    LMStop stop = LMStop::InvalidStart;
    std::size_t objectiveEvaluations = 0;
};

inline ComplexCoefficientFitResult
fitComplexCoefficients(const ComplexRegressionProblem& problem, const ComplexExpression& input,
                       const ComplexCoefficientFitConfig& config = {}) {
    if (!std::isfinite(config.lower) || !std::isfinite(config.upper) || config.lower > config.upper)
        throw std::invalid_argument("invalid complex coefficient bounds");
    if (input.variableCount() != problem.dimensions())
        throw std::invalid_argument("complex fit schema");
    const ComplexExpression constrained(input.nodes(), input.variableCount(), problem.limits());
    const double coefficientLimit = problem.limits().maxAbsConstant;
    const double lowerBound = std::max(config.lower, -coefficientLimit);
    const double upperBound = std::min(config.upper, coefficientLimit);
    if (lowerBound > upperBound)
        throw std::invalid_argument("complex coefficient bounds do not intersect problem limits");

    std::vector<std::size_t> constants, freeCoordinates;
    std::vector<double> initial, lower, upper;
    for (std::size_t i = 0; i < constrained.nodes().size(); ++i) {
        const auto& node = constrained.nodes()[i];
        if (node.kind != ComplexKind::Constant)
            continue;
        for (double value : {node.value.real(), node.value.imag()}) {
            if (value < lowerBound || value > upperBound)
                throw std::invalid_argument("complex fit starting coefficient outside bounds");
            initial.push_back(value);
            lower.push_back(lowerBound);
            upper.push_back(upperBound);
            if (lowerBound < upperBound)
                freeCoordinates.push_back(initial.size() - 1);
        }
        constants.push_back(i);
    }
    if (constants.empty())
        throw std::invalid_argument("complex fit requires at least one coefficient");
    if (initial.size() > config.optimizer.maxParameters)
        throw std::length_error("complex fit parameter limit");
    const auto& rows = problem.observations();
    if (rows.size() > std::numeric_limits<std::size_t>::max() / 2)
        throw std::length_error("complex fit residual count");
    const std::size_t residualCount = rows.size() * 2;
    if (residualCount > config.optimizer.maxResiduals ||
        residualCount > config.optimizer.maxWorkspaceBytes / sizeof(double))
        throw std::length_error("complex fit residual workspace limit");

    auto fullParameters = [&](std::span<const double> theta) {
        auto values = initial;
        for (std::size_t j = 0; j < freeCoordinates.size(); ++j)
            values[freeCoordinates[j]] = theta[j];
        return values;
    };
    auto expressionAt = [&](std::span<const double> theta) {
        const auto values = fullParameters(theta);
        auto expression = constrained;
        for (std::size_t i = 0; i < constants.size(); ++i)
            expression = expression.withConstant(constants[i], {values[2 * i], values[2 * i + 1]});
        return expression;
    };

    std::size_t peakExpressionWorkspace = 0;
    bool operationLimitReached = false;
    auto predictionsAt = [&](std::span<const double> theta,
                             std::vector<std::complex<double>>& predictions,
                             std::size_t& operations, std::size_t allowance,
                             std::size_t& forwardEvaluations, std::size_t& rowsProcessed) {
        auto expression = expressionAt(theta);
        predictions.clear();
        predictions.reserve(rows.size());
        for (const auto& row : rows) {
            if (operations >= allowance) {
                operationLimitReached = true;
                return false;
            }
            auto limits = problem.scientificLimits();
            limits.maxOperations = std::min(limits.maxOperations, allowance - operations);
            auto value = expression.evaluate(row.inputs, limits);
            if (forwardEvaluations == std::numeric_limits<std::size_t>::max() ||
                rowsProcessed == std::numeric_limits<std::size_t>::max())
                return false;
            ++forwardEvaluations;
            ++rowsProcessed;
            peakExpressionWorkspace = std::max(peakExpressionWorkspace, value.work.workspaceBytes);
            if (value.work.operations > std::numeric_limits<std::size_t>::max() - operations)
                return false;
            operations += value.work.operations;
            if (!value.success()) {
                operationLimitReached = value.status == ScientificStatus::ResourceLimit;
                return false;
            }
            predictions.push_back(*value.value);
        }
        return true;
    };
    auto residualsFrom = [&](const std::vector<std::complex<double>>& predictions,
                             std::vector<double>& residuals, std::size_t& operations,
                             std::size_t allowance) {
        residuals.clear();
        residuals.reserve(residualCount);
        for (std::size_t i = 0; i < predictions.size(); ++i) {
            if (operations > allowance || allowance - operations < 2) {
                operationLimitReached = true;
                return false;
            }
            const double re = predictions[i].real() - rows[i].target.real();
            const double im = predictions[i].imag() - rows[i].target.imag();
            if (!std::isfinite(re) || !std::isfinite(im))
                return false;
            operations += 2;
            residuals.push_back(re);
            residuals.push_back(im);
        }
        return true;
    };
    const auto checkedBytes = [](std::size_t n, std::size_t width) {
        return width && n > std::numeric_limits<std::size_t>::max() / width
                   ? std::numeric_limits<std::size_t>::max()
                   : n * width;
    };
    std::size_t jacobianWorkspace = checkedBytes(residualCount, sizeof(double));
    const auto addWorkspace = [&](std::size_t bytes) {
        if (bytes > std::numeric_limits<std::size_t>::max() - jacobianWorkspace)
            jacobianWorkspace = std::numeric_limits<std::size_t>::max();
        else
            jacobianWorkspace += bytes;
    };
    addWorkspace(checkedBytes(rows.size(), 3 * sizeof(std::complex<double>)));
    const auto jacobianElementBytes = checkedBytes(freeCoordinates.size(), sizeof(double));
    addWorkspace(checkedBytes(residualCount, jacobianElementBytes));
    addWorkspace(checkedBytes(freeCoordinates.size(), sizeof(double)));

    std::size_t callbacks = 0;
    Fitness starting = Fitness::invalid();
    LMResidualFunction evaluator = [&](std::span<const double> theta, bool jac,
                                       std::size_t allowance) {
        LMResidualEvaluation out;
        ++callbacks;
        operationLimitReached = false;
        std::vector<std::complex<double>> basePredictions;
        if (!predictionsAt(theta, basePredictions, out.mathematicalOperations, allowance,
                           out.forwardEvaluations, out.rowsProcessed)) {
            out.operationLimitReached = operationLimitReached;
            return out;
        }
        if (!residualsFrom(basePredictions, out.residuals, out.mathematicalOperations, allowance)) {
            out.operationLimitReached = operationLimitReached;
            return out;
        }
        out.workspaceBytes =
            residualCount * sizeof(double) + rows.size() * sizeof(std::complex<double>);
        if (callbacks == 1) {
            const auto sum = genetik_detail::checkedSSE(out.residuals);
            if (std::isfinite(sum))
                starting = Fitness::fromScore(sum / static_cast<double>(rows.size()));
        }
        if (jac && !freeCoordinates.empty()) {
            const auto effectiveWorkspace = std::min(config.optimizer.maxWorkspaceBytes,
                                                     problem.scientificLimits().maxWorkspaceBytes);
            const auto requiredWorkspace =
                peakExpressionWorkspace >
                        std::numeric_limits<std::size_t>::max() - jacobianWorkspace
                    ? std::numeric_limits<std::size_t>::max()
                    : jacobianWorkspace + peakExpressionWorkspace;
            if (requiredWorkspace > effectiveWorkspace) {
                out.workspaceBytes = requiredWorkspace;
                return out;
            }
            const std::size_t p = freeCoordinates.size();
            out.jacobian.resize(residualCount * p);
            std::vector<std::complex<double>> plus, minus;
            auto trial = std::vector<double>(theta.begin(), theta.end());
            for (std::size_t j = 0; j < p; ++j) {
                const double h = std::cbrt(std::numeric_limits<double>::epsilon()) *
                                 std::max(1.0, std::abs(theta[j]));
                const auto coordinate = freeCoordinates[j];
                const double lo = std::max(lower[coordinate], theta[j] - h);
                const double hi = std::min(upper[coordinate], theta[j] + h);
                if (!(lo < hi))
                    return out;
                trial[j] = hi;
                if (!predictionsAt(trial, plus, out.mathematicalOperations, allowance,
                                   out.forwardEvaluations, out.rowsProcessed)) {
                    out.operationLimitReached = operationLimitReached;
                    return out;
                }
                trial[j] = lo;
                if (!predictionsAt(trial, minus, out.mathematicalOperations, allowance,
                                   out.forwardEvaluations, out.rowsProcessed)) {
                    out.operationLimitReached = operationLimitReached;
                    return out;
                }
                trial[j] = theta[j];
                if (rows.size() >
                    (allowance - std::min(allowance, out.mathematicalOperations)) / 2) {
                    out.operationLimitReached = true;
                    return out;
                }
                const double scale = hi - lo;
                for (std::size_t r = 0; r < rows.size(); ++r) {
                    out.jacobian[(2 * r) * p + j] = (plus[r].real() - minus[r].real()) / scale;
                    out.jacobian[(2 * r + 1) * p + j] = (plus[r].imag() - minus[r].imag()) / scale;
                }
                out.mathematicalOperations += rows.size() * 2;
            }
            out.derivativeEvaluations = p;
            out.workspaceBytes = jacobianWorkspace;
        }
        if (peakExpressionWorkspace > std::numeric_limits<std::size_t>::max() - out.workspaceBytes)
            out.workspaceBytes = std::numeric_limits<std::size_t>::max();
        else
            out.workspaceBytes += peakExpressionWorkspace;
        out.valid = true;
        return out;
    };

    std::vector<double> freeInitial, freeLower, freeUpper;
    for (auto i : freeCoordinates) {
        freeInitial.push_back(initial[i]);
        freeLower.push_back(lower[i]);
        freeUpper.push_back(upper[i]);
    }
    auto optimizer = config.optimizer;
    optimizer.maxWorkspaceBytes =
        std::min(optimizer.maxWorkspaceBytes, problem.scientificLimits().maxWorkspaceBytes);
    optimizer.maxMathematicalOperations =
        std::min(optimizer.maxMathematicalOperations, problem.scientificLimits().maxOperations);
    LMResult lm;
    if (!freeInitial.empty()) {
        lm = levenbergMarquardt(freeInitial, freeLower, freeUpper, evaluator, optimizer);
    } else {
        const auto one = evaluator({}, false, optimizer.maxMathematicalOperations);
        lm.stop = one.operationLimitReached                          ? LMStop::EvaluationLimit
                  : one.workspaceBytes > optimizer.maxWorkspaceBytes ? LMStop::WorkspaceLimit
                  : one.valid                                        ? LMStop::GradientTolerance
                                                                     : LMStop::InvalidStart;
        lm.work.objectiveEvaluations = 1;
        lm.work.forwardEvaluations = one.forwardEvaluations;
        lm.work.rowsProcessed = one.rowsProcessed;
        lm.work.mathematicalOperations = one.mathematicalOperations;
        lm.work.peakWorkspaceBytes = one.workspaceBytes;
        if (one.valid)
            lm.sumSquaredResiduals = genetik_detail::checkedSSE(one.residuals);
    }
    auto fitted = expressionAt(lm.parameters);
    Fitness training =
        lm.sumSquaredResiduals
            ? Fitness::fromScore(*lm.sumSquaredResiduals / static_cast<double>(rows.size()))
            : starting;
    return {std::move(fitted), training, starting, lm.work, lm.stop, callbacks};
}

inline ScalarMathematicsProblem
complexRegressionProblem(const std::vector<ComplexObservation> &rows,
                         ExpressionLimits expressionLimits = {},
                         ScientificLimits limits = {}) {
  auto p = std::make_shared<const ComplexRegressionProblem>(
      rows, expressionLimits, limits);
  return ScalarMathematicsProblem::measured(p, p->identity());
}
struct TensorScaleObservation {
  std::vector<double> inputs;
  BoundedTensor tensor, target;
};
// Additive typed adapter: scalar expression chooses a scale; the authoritative
// prediction is the entire tensor. No scalar label is derived from tensor
// labels.
class TensorScaleRegressionProblem final : public RegressionProblem {
public:
  TensorScaleRegressionProblem(const std::vector<TensorScaleObservation> &rows,
                               ExpressionLimits expressionLimits = {},
                               ScientificLimits limits = {})
      : RegressionProblem(MeasurementSchema{schema(rows), expressionLimits}),
        limits_(limits) {
    genetik_science::Budget b(limits);
    if (rows.size() > limits.maxElements)
      throw ScientificResourceExhausted("tensor regression rows");
    std::size_t bytes = 0;
    for (const auto &r : rows) {
      if (r.inputs.size() != dimensions() ||
          r.tensor.shape() != r.target.shape() ||
          r.tensor.shape() != rows.front().tensor.shape())
        throw std::invalid_argument("tensor regression shape/schema");
      genetik_science::inputs(r.inputs);
      auto size = r.tensor.shape().size();
      if (size > limits.maxElements ||
          size > limits.maxWorkspaceBytes / (2 * sizeof(double)))
        throw ScientificResourceExhausted("tensor regression elements");
      auto cost = size * 2 * sizeof(double) + dimensions() * sizeof(double);
      if (cost > limits.maxWorkspaceBytes - bytes)
        throw ScientificResourceExhausted("tensor regression data");
      bytes += cost;
    }
    rows_ = rows;
  }
  bool parallelEvaluationSafe() const noexcept override { return true; }
  std::size_t sampleCount() const noexcept override { return rows_.size(); }
  RegressionMetrics measure(const Expression &e,
                            std::uint64_t *rows = nullptr) const override {
    RegressionSupplementalWork ignored;
    return measureWithWork(e, rows, ignored);
  }
  RegressionMetrics
  measureWithWork(const Expression &e, std::uint64_t *rows,
                  RegressionSupplementalWork &supplemental) const override {
    return genetik_science::accountAssessment(
        [&](ScientificProgress &progress) { return assess(e, &progress); },
        rows, supplemental);
  }
  ScientificAssessment assess(const Expression &e,
                              ScientificProgress *progress = nullptr) const {
    if (e.variableCount() != dimensions())
      throw std::invalid_argument("tensor scale input schema");
    ScientificAssessment r;
    genetik_science::Budget b(limits_);
    genetik_science::ProgressScope progressScope{progress, r, b};
    try {
      double loss = 0;
      for (const auto &row : rows_) {
        b.storage(row.tensor.shape().size(), 2 * sizeof(double));
        b.charge();
        ++r.observationsAttempted;
        if (e.arithmeticCost() > limits_.maxOperations - b.work.operations)
          throw genetik_science::Stop{ScientificStatus::ResourceLimit,
                                      "tensor scale expression work"};
        ++r.fieldEvaluations;
        auto p = e.evaluate(row.inputs);
        b.charge(p.arithmeticOperations());
        if (!p.valid())
          throw genetik_science::Stop{ScientificStatus::Invalid,
                                      "invalid tensor scale"};
        double error = 0;
        for (std::size_t i = 0; i < row.tensor.shape().size(); ++i) {
          double d = b.sub(b.mul(p.value(), row.tensor.values()[i]),
                           row.target.values()[i]);
          error = b.add(error,
                        b.div(b.mul(d, d),
                              static_cast<double>(row.tensor.shape().size())));
        }
        loss = b.add(loss, b.div(error, static_cast<double>(rows_.size())));
      }
      r.loss = loss;
      r.status = ScientificStatus::Success;
    } catch (const genetik_science::Stop &s) {
      r.status = s.status;
      r.reason = s.reason;
    }
    r.work = b.work;
    return r;
  }
  std::string identity() const {
    CheckpointWriter w;
    w.text("tensor-scale-full-model/1");
    ScientificEquationSystem::writeLimits(w, limits_);
    w.integer(rows_.size());
    for (const auto &r : rows_) {
      w.integer(r.inputs.size());
      for (double x : r.inputs)
        w.real(x);
      w.text(checkpointTensor(r.tensor).payload);
      w.text(checkpointTensor(r.target).payload);
    }
    return w.data();
  }

private:
  static std::size_t schema(const std::vector<TensorScaleObservation> &rows) {
    if (rows.empty() || rows.front().inputs.empty())
      throw std::invalid_argument("tensor rows required");
    return rows.front().inputs.size();
  }
  std::vector<TensorScaleObservation> rows_;
  ScientificLimits limits_;
};
inline ScalarMathematicsProblem
tensorScaleRegressionProblem(const std::vector<TensorScaleObservation> &rows,
                             ExpressionLimits expressionLimits = {},
                             ScientificLimits limits = {}) {
  auto p = std::make_shared<const TensorScaleRegressionProblem>(
      rows, expressionLimits, limits);
  return ScalarMathematicsProblem::measured(p, p->identity());
}

// Statistical/scientific scalar observations with full-model supplemental
// counters included in continuation, while legacy D5 payloads remain
// byte-compatible.
inline ScalarMathematicsProblem
scientificScalarRegressionProblem(const std::vector<RegressionRow> &rows,
                                  ExpressionLimits limits = {}) {
  auto p = RegressionProblem::create(rows, limits);
  CheckpointWriter w;
  w.text("scientific-scalar-observations/1");
  w.integer(rows.size());
  for (const auto &r : rows) {
    w.integer(r.inputs.size());
    for (double x : r.inputs)
      w.real(x);
    w.real(r.target);
  }
  return ScalarMathematicsProblem::measured(p, w.data());
}
