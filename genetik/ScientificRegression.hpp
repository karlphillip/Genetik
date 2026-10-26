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
  RegressionMetrics measure(const Expression &genome,
                            std::uint64_t *rows = nullptr) const override {
    RegressionSupplementalWork ignored;
    return measureWithWork(genome, rows, ignored);
  }
  RegressionMetrics
  measureWithWork(const Expression &genome, std::uint64_t *rows,
                  RegressionSupplementalWork &supplemental) const override {
    auto r =
        assess(ComplexExpression::fromScalarGenome(checkedExpression(genome)));
    return genetik_science::objectiveResult(r, rows, &supplemental);
  }
  ScientificAssessment assess(const ComplexExpression &e) const {
    if (e.variableCount() != dimensions())
      throw std::invalid_argument("complex problem schema");
    ScientificAssessment r;
    genetik_science::Budget b(limits_);
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
        auto p = e.evaluate(row.inputs, left);
        b.charge(p.work.operations);
        b.work.workspaceBytes =
            std::max(b.work.workspaceBytes, p.work.workspaceBytes);
        ++r.fieldEvaluations;
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
    return genetik_science::objectiveResult(assess(e), rows, &supplemental);
  }
  ScientificAssessment assess(const Expression &e) const {
    if (e.variableCount() != dimensions())
      throw std::invalid_argument("tensor scale input schema");
    ScientificAssessment r;
    genetik_science::Budget b(limits_);
    try {
      double loss = 0;
      for (const auto &row : rows_) {
        b.storage(row.tensor.shape().size(), 2 * sizeof(double));
        b.charge();
        ++r.observationsAttempted;
        if (e.arithmeticCost() > limits_.maxOperations - b.work.operations)
          throw genetik_science::Stop{ScientificStatus::ResourceLimit,
                                      "tensor scale expression work"};
        auto p = e.evaluate(row.inputs);
        b.charge(p.arithmeticOperations());
        ++r.fieldEvaluations;
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
