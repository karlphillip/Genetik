#pragma once
#include "ScalarMathematicsRun.hpp"
#include "ScientificWork.hpp"

struct ScientificObservation {
  std::vector<double> inputs;
  double target = 0;
};
struct ScientificChannel {
  std::size_t equation = 0;
  double scale = 1, weight = 1;
  std::vector<ScientificObservation> observations;
};
struct ScientificParameter {
  double initial = 0, lower = -8, upper = 8;
};
struct ScientificAssessment {
  ScientificStatus status = ScientificStatus::Unavailable;
  std::optional<double> loss;
  std::vector<double> normalizedChannelErrors;
  ScientificWork work;
  std::size_t observationsAttempted = 0, fieldEvaluations = 0;
  std::string reason;
};
// Optional invocation-owned progress survives operational unwinding. It never
// aliases mutable model data or a different worker's accounting.
struct ScientificProgress {
  ScientificWork work;
  std::size_t observations = 0, fields = 0;
};
namespace genetik_science {
struct ProgressScope {
  ScientificProgress *output;
  ScientificAssessment &assessment;
  Budget &budget;
  ~ProgressScope() noexcept {
    if (output)
      *output = {budget.work, assessment.observationsAttempted,
                 assessment.fieldEvaluations};
  }
};
} // namespace genetik_science
// All equations bind [external inputs, explicit shared parameters]. Parameters
// are common input slots, never accidental shared constant occurrences.
class ScientificEquationSystem {
public:
  ScientificEquationSystem(
      std::size_t inputs, const std::vector<Expression> &equations,
      const std::vector<ScientificChannel> &channels,
      const std::vector<ScientificParameter> &parameters = {},
      ScientificLimits limits = {},
      const std::vector<PhysicalDimension> &bindingUnits = {},
      const std::vector<PhysicalDimension> &equationUnits = {})
      : inputs_(inputs), limits_(limits) {
    genetik_science::Budget budget(limits);
    if (!inputs || inputs > limits.maxElements || equations.empty() ||
        channels.empty() || parameters.size() > limits.maxElements - inputs)
      throw std::invalid_argument("scientific system schema");
    if (inputs + parameters.size() > limits.maxWorkspaceBytes / sizeof(double))
      throw ScientificResourceExhausted("system binding storage");
    if (equations.size() > limits.maxElements ||
        channels.size() > limits.maxElements)
      throw ScientificResourceExhausted("system count");
    for (auto p : parameters)
      if (!std::isfinite(p.initial) || !std::isfinite(p.lower) ||
          !std::isfinite(p.upper) || p.lower > p.initial || p.initial > p.upper)
        throw std::invalid_argument("shared parameter bounds");
    std::size_t storage = 0;
    auto account = [&](std::size_t n) {
      if (n > limits.maxWorkspaceBytes - storage)
        throw ScientificResourceExhausted("system owned storage");
      storage += n;
    };
    for (const auto &e : equations) {
      if (e.variableCount() != inputs + parameters.size())
        throw std::invalid_argument("equation bindings");
      if (e.nodeCount() > limits.maxWorkspaceBytes / sizeof(ExpressionNode))
        throw ScientificResourceExhausted("equation storage");
      account(e.nodeCount() * sizeof(ExpressionNode));
    }
    long double totalWeight = 0;
    for (const auto &c : channels) {
      if (c.equation >= equations.size() || c.observations.empty() ||
          !std::isfinite(c.scale) || c.scale <= 0 || !std::isfinite(c.weight) ||
          c.weight <= 0)
        throw std::invalid_argument("channel configuration");
      totalWeight += c.weight;
      if (c.observations.size() > limits.maxElements)
        throw ScientificResourceExhausted("channel rows");
      for (const auto &r : c.observations) {
        if (r.inputs.size() != inputs || !std::isfinite(r.target))
          throw std::invalid_argument("system row schema");
        genetik_science::inputs(r.inputs);
        account((inputs + 1) * sizeof(double));
      }
    }
    if (!std::isfinite(static_cast<double>(totalWeight)))
      throw std::invalid_argument("channel weight sum overflow");
    for (const auto &c : channels)
      if (static_cast<double>(c.weight / totalWeight) == 0)
        throw std::invalid_argument("channel normalized weight underflow");
    if (!bindingUnits.empty() || !equationUnits.empty()) {
      if (bindingUnits.size() != inputs + parameters.size() ||
          equationUnits.size() != equations.size())
        throw std::invalid_argument("system unit schema");
      for (std::size_t i = 0; i < equations.size(); ++i)
        if (DimensionalExpression(equations[i], bindingUnits).dimension() !=
            equationUnits[i])
          throw std::invalid_argument("equation output dimensions");
    }
    equations_ = equations;
    channels_ = channels;
    parameters_ = parameters;
    bindingsUnits_ = bindingUnits;
    equationUnits_ = equationUnits;
  }
  std::size_t inputCount() const noexcept { return inputs_; }
  const auto &equations() const noexcept { return equations_; }
  const auto &parameters() const noexcept { return parameters_; }
  const auto &channels() const noexcept { return channels_; }
  ScientificLimits limits() const noexcept { return limits_; }
  ScientificEquationSystem withEquation(std::size_t index,
                                        const Expression &e) const {
    auto copy = equations_;
    copy.at(index) = e;
    return {inputs_, copy,           channels_,     parameters_,
            limits_, bindingsUnits_, equationUnits_};
  }
  ScientificAssessment assess(std::span<const double> parameters,
                              const std::atomic<bool> *cancelled = nullptr,
                              ScientificProgress *progress = nullptr) const {
    return assessImpl(parameters, cancelled, {}, progress);
  }
  ScientificAssessment
  assessWithEquation(std::size_t index, const Expression &e,
                     std::span<const double> parameters,
                     const std::atomic<bool> *cancelled = nullptr,
                     ScientificProgress *progress = nullptr) const {
    if (index >= equations_.size() ||
        e.variableCount() != inputs_ + parameters_.size())
      throw std::invalid_argument("replacement equation schema");
    if (!bindingsUnits_.empty()) {
      try {
        if (DimensionalExpression(e, bindingsUnits_).dimension() !=
            equationUnits_[index])
          return {ScientificStatus::Invalid, {}, {}, {}, 0, 0,
                  "equation units differ"};
      } catch (const std::invalid_argument &) {
        return {ScientificStatus::Invalid,    {}, {}, {}, 0, 0,
                "invalid equation dimensions"};
      }
    }
    return assessImpl(parameters, cancelled, std::pair{index, &e}, progress);
  }

private:
  ScientificAssessment assessImpl(
      std::span<const double> parameters, const std::atomic<bool> *cancelled,
      std::optional<std::pair<std::size_t, const Expression *>> replacement,
      ScientificProgress *progress) const {
    if (parameters.size() != parameters_.size())
      throw std::invalid_argument("system parameter count");
    genetik_science::inputs(parameters);
    for (std::size_t i = 0; i < parameters.size(); ++i)
      if (parameters[i] < parameters_[i].lower ||
          parameters[i] > parameters_[i].upper)
        throw std::invalid_argument("system coefficient outside bounds");
    ScientificAssessment r;
    genetik_science::Budget b(limits_, cancelled);
    genetik_science::ProgressScope progressScope{progress, r, b};
    try {
      std::size_t nodes = 0;
      for (std::size_t index = 0; index < equations_.size(); ++index)
        nodes = std::max(nodes, replacement && replacement->first == index
                                    ? replacement->second->nodeCount()
                                    : equations_[index].nodeCount());
      b.storage(
          genetik_science::checkedCount(inputs_ + parameters.size(), nodes),
          sizeof(double));
      std::vector<double> bindings(inputs_ + parameters.size());
      std::copy(parameters.begin(), parameters.end(),
                bindings.begin() + inputs_);
      double weights = 0, loss = 0;
      for (const auto &c : channels_)
        weights = b.add(weights, c.weight);
      for (const auto &c : channels_) {
        double error = 0;
        for (const auto &row : c.observations) {
          b.charge();
          ++r.observationsAttempted;
          std::copy(row.inputs.begin(), row.inputs.end(), bindings.begin());
          const auto &e = replacement && replacement->first == c.equation
                              ? *replacement->second
                              : equations_[c.equation];
          if (e.arithmeticCost() > limits_.maxOperations - b.work.operations)
            throw genetik_science::Stop{ScientificStatus::ResourceLimit,
                                        "field work admission"};
          ++r.fieldEvaluations;
          auto p = e.evaluate(bindings);
          b.charge(p.arithmeticOperations());
          if (!p.valid())
            throw genetik_science::Stop{ScientificStatus::Invalid,
                                        "invalid required equation"};
          auto residual = b.div(b.sub(p.value(), row.target), c.scale);
          error =
              b.add(error, b.div(b.mul(residual, residual),
                                 static_cast<double>(c.observations.size())));
        }
        r.normalizedChannelErrors.push_back(error);
        loss = b.add(loss, b.mul(b.div(c.weight, weights), error));
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

public:
  std::string identity() const {
    CheckpointWriter w;
    w.text("genetik.scientific-equation-system/1");
    w.integer(inputs_);
    writeLimits(w, limits_);
    w.integer(equations_.size());
    for (const auto &e : equations_)
      w.text(e.toString());
    w.integer(parameters_.size());
    for (auto p : parameters_) {
      w.real(p.initial);
      w.real(p.lower);
      w.real(p.upper);
    }
    w.integer(channels_.size());
    for (const auto &c : channels_) {
      w.integer(c.equation);
      w.real(c.scale);
      w.real(c.weight);
      w.integer(c.observations.size());
      for (const auto &r : c.observations) {
        for (double x : r.inputs)
          w.real(x);
        w.real(r.target);
      }
    }
    for (const auto *units : {&bindingsUnits_, &equationUnits_}) {
      w.integer(units->size());
      for (auto u : *units)
        DimensionalExpression::writeDimension(w, u);
    }
    return w.data();
  }
  static void writeLimits(CheckpointWriter &w, ScientificLimits l) {
    for (auto n :
         {l.maxOperations, l.maxElements, l.maxWorkspaceBytes, l.maxIterations})
      w.integer(n);
    w.real(l.tolerance);
  }

private:
  std::size_t inputs_;
  ScientificLimits limits_;
  std::vector<Expression> equations_;
  std::vector<ScientificChannel> channels_;
  std::vector<ScientificParameter> parameters_;
  std::vector<PhysicalDimension> bindingsUnits_, equationUnits_;
};

namespace genetik_science {
inline RegressionMetrics
objectiveResult(const ScientificAssessment &a, std::uint64_t *rows,
                RegressionSupplementalWork *supplemental = nullptr) {
  if (supplemental) {
    bool overflow = false;
    mergeTrainingCounter(supplemental->mathematicalOperations,
                         a.work.operations, overflow);
    mergeTrainingCounter(supplemental->fieldEvaluations, a.fieldEvaluations,
                         overflow);
    if (overflow)
      throw std::overflow_error("scientific supplemental accounting");
  }
  if (rows) {
    if (a.observationsAttempted > UINT64_MAX - *rows)
      throw std::overflow_error("system rows overflow");
    *rows += a.observationsAttempted;
  }
  if (a.status == ScientificStatus::ResourceLimit)
    throw ScientificResourceExhausted(a.reason);
  if (a.status == ScientificStatus::Cancelled)
    throw EvaluationCancelled();
  if (a.status == ScientificStatus::NumericalFailure ||
      a.status == ScientificStatus::Nonconvergence)
    throw ScientificNumericalFailure(
        a.status, "scientific numerical failure: " + a.reason);
  return {a.loss ? Fitness::valid(*a.loss) : Fitness::invalid(),
          {},
          a.observationsAttempted};
}
template <class F>
RegressionMetrics accountAssessment(F &&evaluate, std::uint64_t *rows,
                                    RegressionSupplementalWork &work) {
  ScientificProgress progress;
  ScientificAssessment assessment;
  try {
    assessment = evaluate(progress);
  } catch (...) {
    // Saturate only if counters themselves are exhausted; never replace the
    // original operational cause while unwinding.
    bool overflow = false;
    mergeTrainingCounter(work.mathematicalOperations, progress.work.operations,
                         overflow);
    mergeTrainingCounter(work.fieldEvaluations, progress.fields, overflow);
    if (rows)
      mergeTrainingCounter(*rows, progress.observations, overflow);
    throw;
  }
  return objectiveResult(assessment, rows, &work);
}
class SystemStructureProblem final : public RegressionProblem {
public:
  SystemStructureProblem(const ScientificEquationSystem &s,
                         std::size_t equation, ExpressionLimits limits)
      : RegressionProblem(
            MeasurementSchema{s.inputCount() + s.parameters().size(), limits}),
        system_(s), equation_(equation) {
    if (equation >= s.equations().size())
      throw std::invalid_argument("unknown equation index");
  }
  bool parallelEvaluationSafe() const noexcept override { return true; }
  RegressionMetrics measure(const Expression &e,
                            std::uint64_t *rows = nullptr) const override {
    RegressionSupplementalWork ignored;
    return measureWithWork(e, rows, ignored);
  }
  RegressionMetrics
  measureWithWork(const Expression &e, std::uint64_t *rows,
                  RegressionSupplementalWork &supplemental) const override {
    auto expression = checkedExpression(e);
    std::vector<double> p;
    for (auto a : system_.parameters())
      p.push_back(a.initial);
    return accountAssessment(
        [&](ScientificProgress &progress) {
          return system_.assessWithEquation(equation_, expression, p, nullptr,
                                            &progress);
        },
        rows, supplemental);
  }

private:
  ScientificEquationSystem system_;
  std::size_t equation_;
};
class SystemParameterProblem final : public RegressionProblem {
public:
  SystemParameterProblem(const ScientificEquationSystem &s,
                         ExpressionLimits limits)
      : RegressionProblem(MeasurementSchema{1, limits}), system_(s) {}
  bool parallelEvaluationSafe() const noexcept override { return true; }
  RegressionMetrics measure(const Expression &e,
                            std::uint64_t *rows = nullptr) const override {
    RegressionSupplementalWork ignored;
    return measureWithWork(e, rows, ignored);
  }
  RegressionMetrics
  measureWithWork(const Expression &e, std::uint64_t *rows,
                  RegressionSupplementalWork &supplemental) const override {
    std::vector<double> p;
    for (auto n : e.nodes())
      if (n.kind() == ExpressionKind::Constant)
        p.push_back(n.constantValue());
    if (p.size() != system_.parameters().size())
      throw std::invalid_argument("parameter pack identity");
    for (std::size_t i = 0; i < p.size(); ++i)
      if (p[i] < system_.parameters()[i].lower ||
          p[i] > system_.parameters()[i].upper)
        return {Fitness::invalid(), {}, 0};
    return accountAssessment(
        [&](ScientificProgress &progress) {
          return system_.assess(p, nullptr, &progress);
        },
        rows, supplemental);
  }

private:
  ScientificEquationSystem system_;
};
} // namespace genetik_science
inline ScalarMathematicsProblem
scientificSystemProblem(const ScientificEquationSystem &s, std::size_t unknown,
                        ExpressionLimits limits = {}) {
  auto p = std::make_shared<const genetik_science::SystemStructureProblem>(
      s, unknown, limits);
  CheckpointWriter w;
  w.text(s.identity());
  w.integer(unknown);
  return ScalarMathematicsProblem::measured(p, w.data());
}
struct ScientificParameterFit {
  std::vector<double> parameters;
  std::optional<double> loss;
  TrainingWork work;
  ConstantFitStop stop;
  RegressionSupplementalWork supplemental;
  ScientificStatus status = ScientificStatus::Unavailable;
};
class ScientificFittingFailure : public std::runtime_error {
public:
  ScientificFittingFailure(TrainingWork work,
                           RegressionSupplementalWork supplemental)
      : std::runtime_error(
            "scientific fitting interrupted; input model unchanged"),
        work_(work), supplemental_(supplemental) {}
  const TrainingWork &work() const noexcept { return work_; }
  const RegressionSupplementalWork &supplementalWork() const noexcept {
    return supplemental_;
  }

private:
  TrainingWork work_;
  RegressionSupplementalWork supplemental_;
};
inline ScientificParameterFit
fitScientificParameters(const ScientificEquationSystem &s,
                        ConstantFitConfig config = {}) {
  validateConstantFitConfigForLimit(config, std::numeric_limits<double>::max());
  if (s.parameters().size() > 1024)
    throw ScientificResourceExhausted("parameter coordinates");
  ExpressionLimits l{2 * s.parameters().size() + 1,
                     2 * s.parameters().size() + 1,
                     std::numeric_limits<double>::max(), 100000};
  std::vector<ExpressionNode> nodes;
  if (s.parameters().empty())
    nodes.push_back(ExpressionNode::variable(0));
  for (auto p : s.parameters()) {
    if (p.initial < config.lower || p.initial > config.upper)
      throw std::invalid_argument(
          "starting scientific coefficient outside fitting bounds");
    nodes.push_back(ExpressionNode::constant(p.initial));
    if (nodes.size() > 1)
      nodes.push_back(ExpressionNode::binary(ExpressionKind::Add));
  }
  auto problem =
      std::make_shared<const genetik_science::SystemParameterProblem>(s, l);
  TrainingObjective objective(problem, config.maxLossAttempts);
  auto fitted = [&] {
    try {
      return fitConstants(Expression(nodes, 1, l), objective, config);
    } catch (...) {
      std::throw_with_nested(ScientificFittingFailure(
          objective.work(), objective.supplementalWork()));
    }
  }();
  std::vector<double> values;
  for (auto n : fitted.expression.nodes())
    if (n.kind() == ExpressionKind::Constant)
      values.push_back(n.constantValue());
  return {values,
          fitted.loss.state() == FitnessState::Valid
              ? std::optional<double>(fitted.loss.score())
              : std::nullopt,
          objective.work(),
          fitted.stop,
          objective.supplementalWork(),
          fitted.loss.state() == FitnessState::Valid
              ? ScientificStatus::Success
              : ScientificStatus::Invalid};
}
