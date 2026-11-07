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

`RegressionProblem::measureDetailed` returns ordinary `RegressionMetrics` plus
an optional RMSE, keeping the established `RegressionMetrics` public layout
unchanged. RMSE is independent of the selected fitness: it is the unweighted
`sqrt(sum(r_i*r_i)/n)` when representable; for non-MSE objectives it may be
absent if square accumulation overflows even though the selected objective
remains valid. Maximum absolute error and attempted-row reporting remain
available. No observation is silently omitted.

MSE is unchanged by default. Coordinate fitting compares the selected objective
and therefore optimizes it. The existing least-squares Levenberg–Marquardt
constant fitter is accepted only with unweighted MSE; MAE, Huber, and weighted
MSE are rejected before fitting. There is no silently substituted least-squares
surrogate. Other loss formulations extend the objective configuration and its
single checked measurement path; they must define normalization, validation,
parallel behavior, and compatible fitters before being added.

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
No MDL encoding is defined or implemented. Finite candidate agreement is not a
proof of structural or universal mathematical equivalence.

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
