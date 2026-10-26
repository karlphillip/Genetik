#pragma once
#include "ScientificEquationSystem.hpp"

// First-order ODE identification from complete supplied states/derivatives. No
// trajectory solver, hidden derivative estimator or autonomous field lookup.
class ObservedOdeSystem {
public:
  ObservedOdeSystem(
      const ScientificEquationSystem &system, std::size_t timeInput,
      const std::vector<std::size_t> &stateInputs,
      const std::vector<std::size_t> &rhsEquations,
      const std::vector<PhysicalDimension> &inputAndParameterUnits = {},
      PhysicalDimension timeUnit =
          PhysicalDimension::base(SIBaseDimension::Time))
      : system_(system), time_(timeInput) {
    if (timeInput >= system.inputCount() || stateInputs.empty() ||
        stateInputs.size() != rhsEquations.size())
      throw std::invalid_argument("ODE roles/schema");
    for (std::size_t i = 0; i < stateInputs.size(); ++i) {
      if (stateInputs[i] >= system.inputCount() ||
          stateInputs[i] == timeInput ||
          rhsEquations[i] >= system.equations().size())
        throw std::invalid_argument("ODE state/RHS roles");
      for (std::size_t j = 0; j < i; ++j)
        if (stateInputs[i] == stateInputs[j] ||
            rhsEquations[i] == rhsEquations[j])
          throw std::invalid_argument("duplicate ODE role");
    }
    if (!inputAndParameterUnits.empty()) {
      if (inputAndParameterUnits.size() !=
              system.inputCount() + system.parameters().size() ||
          inputAndParameterUnits[timeInput] != timeUnit)
        throw std::invalid_argument("ODE unit schema");
      for (std::size_t i = 0; i < stateInputs.size(); ++i)
        if (DimensionalExpression(system.equations()[rhsEquations[i]],
                                  inputAndParameterUnits)
                .dimension() !=
            inputAndParameterUnits[stateInputs[i]] / timeUnit)
          throw std::invalid_argument("ODE derivative dimensions");
      std::vector<PhysicalDimension> outputs;
      for (const auto &equation : system.equations())
        outputs.push_back(
            DimensionalExpression(equation, inputAndParameterUnits)
                .dimension());
      system_ = ScientificEquationSystem(system.inputCount(),
                                         system.equations(), system.channels(),
                                         system.parameters(), system.limits(),
                                         inputAndParameterUnits, outputs);
    }
    states_ = stateInputs;
    rhs_ = rhsEquations;
    units_ = inputAndParameterUnits;
  }
  const ScientificEquationSystem &system() const noexcept { return system_; }
  std::string identity() const {
    CheckpointWriter w;
    w.text("observed-first-order-ode/1");
    w.text(system_.identity());
    w.integer(time_);
    w.integer(states_.size());
    for (std::size_t i = 0; i < states_.size(); ++i) {
      w.integer(states_[i]);
      w.integer(rhs_[i]);
    }
    for (auto u : units_)
      DimensionalExpression::writeDimension(w, u);
    return w.data();
  }

private:
  ScientificEquationSystem system_;
  std::size_t time_;
  std::vector<std::size_t> states_, rhs_;
  std::vector<PhysicalDimension> units_;
};
inline ScalarMathematicsProblem
odeRegressionProblem(const ObservedOdeSystem &ode, std::size_t unknown,
                     ExpressionLimits limits = {}) {
  auto p = std::make_shared<const genetik_science::SystemStructureProblem>(
      ode.system(), unknown, limits);
  CheckpointWriter w;
  w.text(ode.identity());
  w.integer(unknown);
  return ScalarMathematicsProblem::measured(p, w.data());
}
// Field observations retain explicit (field, ordered derivative indices) roles.
// Residual expressions bind them as ordinary original-index inputs. This
// supplies derivative data; it does not infer a field or solve a PDE.
struct ObservedFieldRole {
  std::size_t field = 0;
  std::vector<std::size_t> partials;
  bool operator==(const ObservedFieldRole &) const = default;
};
class ObservedPdeResidualSystem {
public:
  ObservedPdeResidualSystem(const ScientificEquationSystem &system,
                            std::size_t independentDimensions,
                            const std::vector<ObservedFieldRole> &roles)
      : system_(system), dimensions_(independentDimensions) {
    if (!independentDimensions || roles.size() != system.inputCount())
      throw std::invalid_argument("PDE observation roles");
    for (std::size_t i = 0; i < roles.size(); ++i) {
      if (roles[i].partials.size() > 2)
        throw std::invalid_argument("PDE observed derivative order<=2");
      for (auto target : roles[i].partials)
        if (target >= independentDimensions)
          throw std::invalid_argument("PDE derivative binding");
      for (std::size_t j = 0; j < i; ++j)
        if (roles[i] == roles[j])
          throw std::invalid_argument("duplicate PDE role");
    }
    roles_ = roles;
  }
  const ScientificEquationSystem &system() const noexcept { return system_; }
  std::string identity() const {
    CheckpointWriter w;
    w.text("observed-pde-residual/1");
    w.text(system_.identity());
    w.integer(dimensions_);
    for (const auto &role : roles_) {
      w.integer(role.field);
      w.integer(role.partials.size());
      for (auto p : role.partials)
        w.integer(p);
    }
    return w.data();
  }

private:
  ScientificEquationSystem system_;
  std::size_t dimensions_;
  std::vector<ObservedFieldRole> roles_;
};
