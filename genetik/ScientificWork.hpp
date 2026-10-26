#pragma once
#include "Execution.hpp"
#include "Expression.hpp"
#include <atomic>
#include <functional>

// Local mathematical work; independent of GA objective admission.
enum class ScientificStatus {
  Success,
  Invalid,
  NumericalFailure,
  ResourceLimit,
  Nonconvergence,
  Cancelled,
  Unavailable
};
struct ScientificLimits {
  std::size_t maxOperations = 1000000, maxElements = 65536,
              maxWorkspaceBytes = 8 * 1024 * 1024;
  std::size_t maxIterations = 1024;
  double tolerance = 1e-12;
};
struct ScientificWork {
  std::size_t operations = 0, iterations = 0, workspaceBytes = 0;
  bool operator==(const ScientificWork &) const = default;
};
class ScientificResourceExhausted : public EvaluationBudgetExceeded {
public:
  using EvaluationBudgetExceeded::EvaluationBudgetExceeded;
};
class ScientificNumericalFailure : public std::runtime_error {
public:
  ScientificNumericalFailure(ScientificStatus status, const std::string &reason)
      : std::runtime_error(reason), status_(status) {}
  ScientificStatus status() const noexcept { return status_; }

private:
  ScientificStatus status_;
};
namespace genetik_science {
inline std::size_t checkedCount(std::size_t a, std::size_t b) {
  if (b > std::numeric_limits<std::size_t>::max() - a)
    throw ScientificResourceExhausted("scientific count overflow");
  return a + b;
}
struct Stop {
  ScientificStatus status;
  const char *reason;
};
class Budget {
public:
  ScientificLimits limits;
  ScientificWork work;
  const std::atomic<bool> *cancelled;
  explicit Budget(ScientificLimits l = {}, const std::atomic<bool> *c = nullptr)
      : limits(l), cancelled(c) {
    if (!l.maxOperations || !l.maxElements || !l.maxWorkspaceBytes ||
        !l.maxIterations || !std::isfinite(l.tolerance) || l.tolerance <= 0 ||
        l.tolerance >= 1)
      throw std::invalid_argument("invalid scientific limits");
  }
  void charge(std::size_t n = 1) {
    if (cancelled && cancelled->load(std::memory_order_relaxed))
      throw Stop{ScientificStatus::Cancelled, "cancelled"};
    if (n > limits.maxOperations - work.operations)
      throw Stop{ScientificStatus::ResourceLimit, "arithmetic allowance"};
    work.operations += n;
  }
  void iteration() {
    charge();
    if (work.iterations == limits.maxIterations)
      throw Stop{ScientificStatus::ResourceLimit, "iteration allowance"};
    ++work.iterations;
  }
  void storage(std::size_t count, std::size_t bytes) {
    if (count > limits.maxElements || count > limits.maxWorkspaceBytes / bytes)
      throw Stop{ScientificStatus::ResourceLimit, "storage allowance"};
    work.workspaceBytes = std::max(work.workspaceBytes, count * bytes);
  }
  double finite(double x) {
    if (!std::isfinite(x))
      throw Stop{ScientificStatus::NumericalFailure, "nonfinite arithmetic"};
    return x;
  }
  double add(double a, double b) {
    charge();
    volatile double x = a + b;
    return finite(x);
  }
  double sub(double a, double b) {
    charge();
    volatile double x = a - b;
    return finite(x);
  }
  double mul(double a, double b) {
    charge();
    volatile double x = a * b;
    return finite(x);
  }
  double div(double a, double b) {
    charge();
    if (b == 0)
      throw Stop{ScientificStatus::Invalid, "zero denominator"};
    volatile double x = a / b;
    return finite(x);
  }
};
inline void inputs(std::span<const double> x) {
  for (double v : x)
    if (!std::isfinite(v))
      throw std::invalid_argument("nonfinite scientific input");
}
} // namespace genetik_science
template <class T> struct ScientificResult {
  ScientificStatus status = ScientificStatus::Unavailable;
  std::optional<T> value;
  ScientificWork work;
  std::string reason;
  bool success() const noexcept { return status == ScientificStatus::Success; }
};
template <class T, class F>
ScientificResult<T> scientificCalculation(ScientificLimits limits,
                                          const std::atomic<bool> *cancelled,
                                          F &&f) {
  genetik_science::Budget b(limits, cancelled);
  ScientificResult<T> r;
  try {
    r.value = f(b);
    r.status = ScientificStatus::Success;
  } catch (const genetik_science::Stop &s) {
    r.status = s.status;
    r.reason = s.reason;
  }
  r.work = b.work;
  return r;
}
