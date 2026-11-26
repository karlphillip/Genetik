# Regression objectives and information criteria

`RegressionProblem` owns its copied rows and immutable objective configuration.
Existing construction continues to select mean squared error (MSE), and its
checked per-row evaluation and exact floating-point operation order are retained.
The objective setting controls the `Fitness` score used by the normal symbolic
regression population, rank selection, and best-model ordering.

```cpp
#include <ConstantFitting.hpp>

RegressionObjectiveConfig objective;
objective.loss = RegressionLoss::Huber;
objective.huberDelta = 1.0;
auto problem = RegressionProblem::createWithObjective(rows, objective);
TrainingObjective training(problem);
ConstantFitConfig fitting; // deterministic coordinate search; no RNG use
auto fitted = fitConstants(initialExpression, training, fitting);
```

## Loss definitions

For residuals `r_i = prediction_i - target_i`, MSE is the existing
`sum(r_i*r_i)/n`; MAE is `sum(abs(r_i))/n`. Huber uses a finite positive delta
and the per-row loss `0.5*r_i*r_i` when `abs(r_i) <= delta`, otherwise
`delta*(abs(r_i)-0.5*delta)`. Weighted MSE is
`sum(w_i*r_i*r_i)/sum(w_i)`. Weights are copied, finite, nonnegative, have one
entry per row, and must have positive total. A zero weight contributes zero to
the objective but the prediction still must be mathematically valid.

`CustomScalar` accepts `customScalarLoss(prediction, target)` and averages its
per-row results. The callback must return a finite, nonnegative value; invalid
results produce invalid fitness and thrown operational errors propagate. It is
used by ordinary symbolic search and coordinate fitting through the same checked
measurement path. The callback is serial by default because captured mutable
state cannot be assumed thread-safe; set `customScalarLossParallelSafe` only when
the callback is safe for concurrent const invocation. Levenberg–Marquardt does
not accept arbitrary callbacks: it is limited to MSE, weighted MSE, and Huber,
whose residual transformations and Jacobians are explicitly defined. No callback
is reinterpreted as least squares.

`RegressionProblem::measureDetailed` returns ordinary `RegressionMetrics` plus
an optional RMSE, keeping the established `RegressionMetrics` public layout
unchanged. RMSE is independent of the selected fitness: it is the unweighted
`sqrt(sum(r_i*r_i)/n)` when representable; for non-MSE objectives it may be
absent if square accumulation overflows even though the selected objective
remains valid. Maximum absolute error and attempted-row reporting remain
available. No observation is silently omitted.

MSE is unchanged by default. Coordinate fitting compares the selected objective
and therefore optimizes it. The scalar least-squares Levenberg–Marquardt
constant fitter supports MSE, weighted MSE, and Huber. For weighted MSE it scales
each residual and Jacobian row by `sqrt(w_i)/sqrt(sum(w))`, avoiding premature
underflow in `w_i/sum(w)`. Zero-weight rows are still evaluated and must have a
valid prediction, then contribute zero residual and Jacobian rows. For Huber, LM
applies the signed square root of twice each Huber contribution and its analytic
chain-rule factor to each Jacobian row, normalized so the solver's squared-residual
objective equals mean Huber loss. The returned fit loss is the checked public
scorer's value for the accepted parameters. MAE and CustomScalar remain
unsupported by this least-squares fitter; no surrogate is silently substituted.

Inputs, targets, deltas, weights, residuals, loss terms, and accumulated scores
are checked for finiteness. Mathematical invalidity returns invalid fitness.
Objective evaluation charges the usual loss invocation and every attempted row;
expression evaluation work remains in the existing supplemental counters.
Configuration validation occurs before a search or fit starts. Objective and
rows stay immutable for the run, so concurrent candidate evaluation observes the
same weights and loss settings. No new RNG calls are made.

The deterministic fixture with targets `{0,0,10}` independently gives constant
minimizers 10/3 for MSE, 0 for MAE, and 1/2 for Huber with delta 1. These are
mathematical references for the losses and coordinate fitter, not stochastic GA
recovery claims.

The installed `objectives_quickstart` also shows 0.75 asymmetric absolute loss
on targets `{0,2,10}`. Its unique constant minimizer is 10 (the 0.75 quantile),
while MSE minimizes at 4. It exercises both normal symbolic selection and
coordinate refinement with this explicit callback.

## Bounded vector objectives for Pareto selection

`SymbolicRegressionConfig::paretoObjectives` optionally supplies a finite
minimization vector from an expression and the immutable `RegressionProblem`.
The callback receives training rows only, must return the same nonempty vector
for every candidate, and is capped at four entries by
`maximumParetoObjectives`. Nonfinite values and dimension changes are rejected.
The vector replaces scalar loss in Pareto dominance and crowding; configured
expression complexity remains an additional Pareto dimension. Scalar
`candidate.fitness()` is still ordinary reported prediction loss and supplies
stable tie ordering. `SymbolicParetoPoint::objectives` reports the selected
vector and `ParetoSelectionWork::objectiveVectorEvaluations` counts callback
calls. The memory estimate includes the bounded objective storage.

The installed `selection_strategies` quickstart uses a two-objective vector of
training MSE and maximum absolute error. A deterministic search contract uses
per-row squared residuals for a two-row constant-model fixture. Its hand
calculation gives `(0,4)`, `(1,1)`, `(4,0)` for constants 0, 1, and 2; all three
are nondominated and the actual Pareto frontier returns exactly that set.

## Post-search AIC/BIC

`InformationCriteria.hpp` provides `selectInformationCriterion` as an explicit
post-search operation over caller-supplied candidate expressions. It does not
change the population, GA fitness, or evolutionary selection. The supported
likelihood is iid Gaussian with an estimated common variance and unweighted MSE.
For `n` observations and residual MSE `v > 0`, it uses

`log L = -n/2 * (log(2*pi) + 1 + log(v))`.

The likelihood parameter count is the caller-declared number of independently
fitted expression parameters plus one estimated variance parameter. The caller
must not count fixed or merely sampled constants as fitted parameters. The
reported expression node count is structural complexity and is not used as the
statistical parameter count. Criterion ties use lower structural complexity;
equal criterion and structural complexity retain the earlier candidate.

Exact zero residual variance has an unbounded Gaussian maximum likelihood and
is reported as `ZeroResidualVariance`; no finite AIC/BIC is manufactured.
Candidates with `n <= k` are marked `InsufficientObservations`. AIC/BIC reject
MAE, Huber, weighted MSE, and custom `RegressionProblem` likelihood semantics.
## Post-search minimum description length

`MinimumDescriptionLength.hpp` supplies an opt-in post-search selector for the
fixed-row, deterministic scalar regression model with unweighted MSE data and
polynomial expressions using only variables, constants, `+`, `-`, and `*`.
It does not alter AIC/BIC or symbolic-search fitness. Results identify the
formulation as `genetik-mdl-fixed-precision-signed-elias-gamma/1`; every length is
reported in bits. `selectedModel` is the selected expression with its encoded
coefficients quantized at the declared coefficient precision.

This is a two-part fixed-precision code, with no stochastic noise model. The
coefficient precision `q_c` and residual precision `q_r` are positive absolute
steps in the units of the represented coefficients and target. Constants are
rounded to the nearest integer multiple of `q_c`, and predictions are computed
from that quantized expression. Residuals are rounded to multiples of `q_r`.
The signed integer mapping is `z(n)=2n+1` for `n>=0` and `z(n)=-2n` for `n<0`.
Each positive integer `z` is encoded with Elias gamma code length
`2*floor(log2(z))+1`.

The model part starts with an 8-bit version header. Each postfix node uses a
3-bit type tag (`000` variable, `001` constant, `010` add, `011` subtract,
`100` multiply); variable nodes append the gamma code of `index+1`, constant
nodes append the gamma code of `z(quantized coefficient)`, and supported
operator tags fully identify the operation. The residual part sums the gamma
code lengths of `z(quantized residual)` over every fixed training row. Thus an
exact zero residual costs one bit (`gamma(1)`); exact fits are finite and are
not treated as zero-variance likelihoods. Total bits are model bits plus
residual bits. Inputs and row ordering are assumed shared and cost no bits.

The selector bounds candidate count, row and expression work, estimated result
memory, and total bits. It returns no winner if a resource ceiling prevents the
complete candidate set from being scored. Division, scientific operators,
custom losses, weighted data, and non-native `RegressionProblem` subclasses are
unsupported. The bit lengths are interpretable under this exact code and
precision; they are not numerically comparable to Operon's formulation unless
both codebooks and precisions are aligned. This explicitly differs from AIC/BIC,
which use an iid Gaussian likelihood with estimated residual variance.

The deterministic tests independently enumerate code lengths for two fixtures:
the constant model wins on zero targets, while the variable model wins on
targets equal to the input. These are code arithmetic checks, not claims of
universal formula identification.

The selector bounds candidates, row evaluations, expression operations, and
estimated temporary/result memory. The memory counter is an explicit estimate,
not a process RSS limit. If a limit prevents the complete supplied set from
being scored, the overall result is `ResourceExhausted` and has no selected
index, even if earlier candidates received scores. Candidate-specific invalid
models, invalid predictions, insufficient data, and numerical failure remain
visible in per-candidate results. Existing result fields named
`initialBestMSE`, `rawInitialBestMSE`, and `improvements` retain their historical
meanings and are populated only for MSE runs. Objective-generic
`initialBestObjective`, `rawInitialBestObjective`, and `objectiveImprovements`
report other configured fitness values.
