# Constructive rational-function regression

`RationalFunctionRegression.hpp` provides an optional deterministic regression
facility for one-variable observations. It does not participate in
`symbolicRegression()` or alter the genetic algorithm, its defaults, fitting
policy, RNG use, checkpoints, or candidate selection.

## Supported family

The fitter considers normalized models

\[
f(x)=\frac{p_0+p_1x+\cdots+p_mx^m}{1+q_1x+\cdots+q_nx^n}.
\]

Numerator and denominator degree limits are configurable up to three, with at
most eight free parameters. The denominator constant is fixed to one. This
removes the overall scale ambiguity, but excludes reduced rational functions
whose denominator is zero at zero (for example `1/x`). Coefficient bounds can
also exclude otherwise equivalent representations. Common factors, redundant
degree choices, and finite-data non-identifiability remain possible.

## Fitting and model selection

For each eligible degree pair, a bounded column-pivoted Householder QR solve
fits the cross-multiplied system. Rank deficiency, nonfinite inputs, coefficient
overflow, invalid configurations, and row, workspace, arithmetic, or linear
algebra limits are reported explicitly. Normal equations are not used.

The linearized residual is not the original quotient prediction residual:
cross multiplication weights the prediction residual by the squared denominator.
Optional refinement uses Genetik's existing bounded Levenberg–Marquardt fitter
on the original quotient expression. It is off by default. QR failure is not
silently replaced with another method, and LM stopping/work data is returned.

Degree candidates are selected with the existing training-only BIC-style score
and a smaller-expression tie break. Held-out observations are not part of this
API and should only be scored after model selection.

## Prediction-domain status

`predictRationalChecked()` returns a point value only with `Valid` status. It
reports `Invalid` for demonstrated expression/domain failures and `Uncertain`
when the denominator is too close to zero relative to its coefficient/input
scale. Its reason string identifies the condition. `RationalModel::predict()`
also remains available as a simple optional-valued convenience call. No
denominator is clipped and the core expression evaluator is unchanged.

`assessRationalDomain()` applies outward-rounded interval Horner bounds with a
bounded subdivision count over a caller-supplied closed interval and echoes that
interval in `PoleDiagnostic::assessedInterval`. It returns:

- `Valid` when the denominator is numerically separated from zero over the
  entire interval under the configured absolute floor and scale-aware rounding
  allowance;
- `Invalid` when a denominator zero/threshold violation or invalid observed
  prediction is demonstrated;
- `Uncertain` when finite computation cannot establish separation.

The certificate concerns this fitted denominator and interval. A successful fit
does not certify arbitrary extrapolation or global model correctness. Noisy
extrapolation near poles remains unreliable: S13.1's noisy improper-rational
case had a large near-pole error despite finite predictions. Keep prediction
domains explicit and treat `Uncertain` as unresolved.

## Example

```cpp
#include <RationalFunctionRegression.hpp>
#include <array>
#include <vector>

std::vector<RationalObservation> rows;
for (double x : std::array{-1.0, -0.5, 0.0, 0.5, 1.0})
    rows.push_back({x, (1.4*x + 0.2)/(x*x + 0.8)});

RationalConfig config;
config.maximumNumeratorDegree = 2;
config.maximumDenominatorDegree = 2;
config.refineWithLM = true; // optional; uses existing LM
auto result = fitRationalFunction(rows, config);
if (result.status == RationalStatus::Success && result.selected) {
    const auto& model = result.models[*result.selected];
    const auto domain = assessRationalDomain(model, {}, {-1.0, 1.0},
                                              config.minimumAbsoluteDenominator);
    // Inspect domain.status before relying on predictions in this interval.
}
```

## Compatibility and qualification

The header is standard-library-only and uses existing `Expression` and
`LevenbergMarquardt.hpp` facilities. It is additive and does not change ordinary
GA use or published v0.7.0 behavior. The implementation is deterministic and
single-threaded. The development candidate is not part of the published v0.7.0
package; it is an unreleased branch until reviewed and separately approved.

Integration selectively combines S13.1's `RationalReconstruction.hpp`, bounded
Householder least-squares helper, and `RationalDiagnostics.hpp` in the installed
`genetik/RationalFunctionRegression.hpp`. Campaign executables, JSON adapters,
dataset readers, frozen data, S8 enumerator code, and evolutionary research
policies were excluded. The public header owns its bounded QR helper in an
implementation-detail namespace and reuses the existing expression evaluator
and LM.

S13/S13.1 reported bounded-family recovery and independent held-out results, not
unrestricted symbolic-regression superiority. Their data remain in the separate
research archive. The integration uses the confirmation set as regression
evidence only. The S13.1 replay on the original 10 rational task/track datasets
retained the selected degree pairs and finite held-out predictions. The noisy
improper and near-pole extrapolation cases remain inaccurate, as reported in
the original study; integration does not tune those scores. The three existing
GA method configurations remain separate baselines and the rational fitter does
not alter their search behavior.
