# Supplied value and first-partial observations — Phase 7B

Contract recorded before implementation. `DerivativeRegression.hpp` is an optional
mathematical module over the existing scalar expression, partial evaluator,
polymorphic measurement/preparation, shared search and coordinate fitter. Generic
GA interfaces remain independent. No residual language, integration, parameter
gradients, typed derivatives, optimizer or data-file format is added.

`ScalarObservationChannel` is a construction descriptor: optional partial index
(null means value), owned rows, positive finite scale and weight. The immutable
`DerivativeRegressionProblem` copies descriptors and nested rows after validation.
It requires a positive explicit dimension, at least one nonempty channel, at most
one value channel and one channel per original partial index, finite complete input
vectors and targets. Values traverse first, then partials by ascending original
index; row order within each channel is retained. No missing-value sentinel.

For channel c, compute residual prediction-target, divide by its declared scale,
square, sum in row order from +0 and divide by its own row count. Normalize weights
once in channel order: checked sum of positive weights, alpha_c=weight_c/sum;
reject nonfinite sum or a zero/nonfinite normalized weight. Combine alpha_c*E_c in
channel order from +0. This is the **weighted normalized objective**, mathematically
sum(weight*channel_mean)/sum(weight), not a pooled row mean. Residual normalization
precedes squaring. Every required intermediate is checked. Nonzero normalization,
square, mean or weighted contributions rounded to zero are explicit loss-arithmetic
failures, rather than silently ignored errors. Finite rounding/absorption remains a
numerical limitation, not a guarantee of real-arithmetic accuracy.

Only requested observations execute: ordinary evaluate for values; evaluatePartial
for each requested derivative, requiring both primal validity and available finite
partial. Unrequested singular derivatives cannot disqualify a value row. No failed
row is omitted and no finite penalty substituted. Detailed outcomes distinguish
primal invalidity, unavailable partial, loss arithmetic, resources and operational
exceptions. A resource outcome maps to ObservationResourceExhausted through shared
failed-preparation publication; it is never Invalid fitness or NoViableCandidates.

Construction limits bound dimensions/channels/total rows/input coefficients before
copying. Fixed training evaluation limits bound attempted observations and aggregate
arithmetic, plus per-value plan/storage and the existing per-partial limits. They
are not a new global operation budget. One complete objective request is one loss
attempt, including invalid, resource and throwing requests. Rows count observations;
partial primal work is not a second attempt. Supplemental counters separate value
and partial calls, primal operations, derivative arithmetic/native calls and
objective arithmetic. Private worker deltas merge after draining even on failure.
No mutable scratch/data aliases, RNG, cache, or nested executor is introduced.
Value rows reuse one invocation-local ordinary operand workspace, growing only
after row admission; partial calls retain their existing independent scratch.
The separate per-value and per-partial payload bounds can coexist, and are not
a single total-memory cap.

Training may stop at its first unscorable row. `assess` uses a separate bounded
allowance and inspects all rows unless its traversal/aggregation allowance ends;
counts are exact for inspected rows, and completeness is explicit. Individual
per-row resource failures can be counted while continuing reporting. Raw-unit MSE
and maximum errors are diagnostics only; an unrepresentable raw MSE does not
invalidate a finite normalized objective. Reporting never edits training fitness.

The shared expression candidate identifies the immutable problem pointer, including
all observations/settings. Edits invalidate fitness; equal schemas/expressions do
not transfer scores across different objectives. Coordinate fitting uses exactly
this composite objective, ordinary independent postfix constant occurrences and
existing local/global limits, cadence and rollback. Restricted template coordinates
can use fitSelectedConstants; scaffold constants stay frozen.

Derivative-only data cannot identify additive constants. A value anchor is an
observation, not a structural constraint satisfied by every candidate. Finite
samples plus an anchor do not identify a unique global function. In several
variables one partial leaves freedom in other inputs; one point anchor generally
does not remove that freedom. Recovery claims are sample agreement, not uniqueness.

## Public usage

```cpp
#include <DerivativeRegression.hpp>
#include <ExpressionParsing.hpp> // optional input boundary
// Omission disables a channel; nullopt selects the value channel, not missing data.
auto problem = DerivativeRegressionProblem::create(1, {
    {std::nullopt, {{{0},1}}, 1,1},
    {0, {{{-1},-2},{{0},0},{{1},2}}, 1,1}
}, {9,4,8});
SymbolicRegressionConfig cfg;
cfg.population=8; cfg.elites=1; cfg.maxGenerationAttempts=3;
cfg.maxEvaluations=32; cfg.maxTrainingLossAttempts=32;
RandomContext run(42, ExecutionConfig{2});
auto result = derivativeRegression(problem,cfg,run);
if (result.search.best) {
    // The cached score is the weighted normalized training objective.
    auto report = problem->assess(result.search.best->expression());
    // report.channels[c].rawMSE/maxAbsoluteError are in that channel's units.
}
// Separate fixed structure, same objective for baseline, probes and returned score:
DerivativeTrainingObjective objective(problem,512);
ConstantFitConfig fit; fit.maxLossAttempts=512;
auto fitted=fitConstants(parseExpression("1*x0*x0+1",1),objective,fit);
```

The [installed compiled quickstart](../examples/quickstarts/derivative_regression.cpp)
also supplies the quadratic coefficient-recovery observations and runs a bounded
search independent of discovering an exact equation. Installation commands are in
PACKAGING.md; build its installed target `derivative_regression_quickstart`.

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/release -j2
out/build/release/genetik_derivative_regression_quickstart --threads 2
out/build/release/genetik_derivative_regression_campaign \
  --output out/runs/derivative-regression-new --threads 1
# Bounded public application smoke (also exercised by CTest):
out/build/release/genetik_derivative_regression_campaign \
  --smoke --output out/runs/derivative-regression-smoke-new --threads 2
```

The normal campaign freezes five searches and its representative before writing
assessment metrics. Search time includes initialization, excludes data preparation,
reporting and the separate supplied-structure fit. Seeds execute sequentially;
`--threads` controls the existing candidate pool inside each run, not seed concurrency.
No target formula or supplied structure enters random initialization.

## API outcomes and accounting details

`measureObservations` uses the immutable training limits and stops at the first
unscorable observation. `assess(expression, reportingLimits)` inspects all requested
rows unless global observation/aggregation traversal is exhausted. It returns
`ObservationMetrics`: full loss, status, per-channel normalized means and raw-unit
metrics, first failing channel/row, optional derivative/resource reason, completeness
and actual work. A missing failing-row index denotes a channel reduction. Reports
count invalid primal and unavailable required-partial **observations**, not components.
Loss-arithmetic and resource counts are events (including reductions/stops), not
claims about unevaluated predictions. A failed per-row evaluation permits bounded
all-row reporting; global traversal exhaustion sets complete=false. Successful
prefixes never supply full loss or full-channel errors.

`RegressionProblem::measure` is now a virtual mathematical measurement seam, with
virtual destruction/sampleCount and an explicit concurrency capability. Its protected
`MeasurementSchema` constructor creates a schema without fictitious value labels;
a constrained constructor preserves legacy braced-row constructor resolution.
Ordinary RegressionProblem keeps exactly its previous MSE implementation. A sliced
schema-only adapter rejects ordinary base measurement instead of scoring fake rows.
Recompile dependent translation units consistently: no stable binary ABI is promised.
The generic GA includes no observation/derivative headers. The original noexcept worker-merge hook is preserved; a separate post-merge
validation hook reports supplemental accounting errors through normal failure handling.

`ExpressionCandidate` validates immutable problem identity, calls the virtual measure
and requires problem concurrency qualification. `DerivativeTrainingObjective` adds
worker-local supplemental counts to the existing TrainingObjective. It uses the same
admission, coordinate kernel, optional selection, fitting cadence, ordering, cancellation,
draining and publication. Derivatives operate on input indices, never constant gradients.
Detailed refinement diagnostics remain serial-only, explicitly rejected for N>1
without dropping diagnostics or changing requested concurrency. Custom objective
subclasses must explicitly qualify concurrency and provide independent worker clones.

`DerivativeRegressionResult.search` reuses legacy field names initialBestMSE and
Improvement::mse; for this problem those hold the **weighted normalized objective**,
not raw-unit value MSE. observationWork counts all actual training work, including
unpublished preparation; legacy rowAttempts equals attempted observations. Its
value/partial evaluation counts are disjoint. Primal operations include the primal
work inside partial evaluation; those primal values are not additional value-channel
calls or objective attempts. Additional derivative arithmetic/native-call work follows
Phase 7A; opaque runtime calls are not internal instruction counts. Aggregation work
charges residual subtraction, scale division, square, sum, channel mean, normalized
weight product and channel addition. Raw reporting diagnostics and construction-time
weight normalization are excluded from that arithmetic counter and bounded by dataset
sizes. Direct problem measurement/reporting is outside a run's loss allowance;
TrainingObjective-mediated candidate scoring/fitting charges one complete objective.

Defaults: construction maxDimensions64, maxChannels65, maxObservations65536,
maxInputCoefficients1048576; positive counts required. Check total rows/input products
before copying, per-channel counts at most2^53 for exact count conversion. Training
per-call maxObservations65536/maxAggregationOperations1000000; ordinary values admit
whole plans of at most65536 nodes/1000000 operations/8MiB double operand payload.
Partials use the independent Phase 7A default limits (1000000 total operations,
65536 node visits/bindings,8MiB slot payload). Zero evaluation allowances are valid
resource stops. Expression validation/copies remain bounded separately by expression
limits; payload bounds are not total process-memory caps. Ordinary value admission
is conservative by full plan, even if an invalid early subtree would execute less.

API/configuration/shape errors throw invalid_argument/length_error; unexpected
allocation/operational errors preserve original causes through the existing nested
attempt/publication path. ResourceLimit becomes ObservationResourceExhausted during
training preparation and aborts that generation; it cannot be Invalid fitness.
PrimalInvalidity, DerivativeUnavailable and LossArithmeticFailure map to Invalid
fitness but remain distinct in detailed diagnostics/counters. No automatic retries,
limit inflation, clipping or omitted rows. Residual subtraction can itself overflow
before normalization; finite precision can absorb small contributions. Diagnostic
raw MSE overflow/underflow is reported as unavailable while independent maximum
error may remain representable. Scientific/domain and conservative derivative
availability rules remain those of SCALAR_DERIVATIVES.md.

Complete protocol, five-seed results and separate coefficient fixture:
[DERIVATIVE_REGRESSION_RESULTS.md](DERIVATIVE_REGRESSION_RESULTS.md). Engineering
gates and compatibility records: [DERIVATIVE_REGRESSION_QUALIFICATION.md](DERIVATIVE_REGRESSION_QUALIFICATION.md).
Only 7C bounded numerical integration remains in the agreed implementation scope;
final calculus qualification/delivery follows it. No hosted feature claim is made here.
