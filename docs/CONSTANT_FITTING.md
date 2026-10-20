# Bounded continuous constant fitting

`ConstantFitting.hpp` supplies a deterministic reference coordinate search. It uses
only strict CPU expression evaluation and checked training MSE; it draws no random
numbers. The older polynomial example charges its post-search training verification through a
caller-retained objective and prints search/global totals separately.
No extra dependency, algebraic simplification or analytic target recognition
is involved. The original discrete-constant polynomial configuration remains supported.

```cpp
#include <SymbolicRegression.hpp>
const auto problem = RegressionProblem::create({{{0}, 2.5}, {{1}, 2.5}}, {31,6,8});
TrainingObjective objective(problem); // run-owned accounting; optional second argument: loss cap
ConstantFitConfig fit;               // [-8,8], step 1 down to 1e-8
const auto fitted = fitConstants(Expression::constant(1,1), objective, fit);
// fitted.expression and fitted.loss are an exact matching pair; input remains unchanged.

SymbolicRegressionConfig config;
config.fitting = fit;                // omit/reset for no refinement
config.maxTrainingLossAttempts = 100000;
config.maxEvaluations = 100000;      // independent outer candidate-request cap
config.trainingLossThreshold = 1e-14;
config.grammar.continuousProbability = .5;
RandomContext run(42);
const auto result = symbolicRegression(problem, config, run);
if (result.best) { /* result.best->text(), result.best->fitness() */ }
```

Change rows and create a new problem/run for a different objective. Input rows are
copied and immutable. No evaluation-only rows belong in this problem. `TrainingObjective`
is noncopyable/nonmovable; its `work()` diagnostics are read-only.
`problem->measure` remains available for explicit unbudgeted reporting, but must not
be used to hide search or training verification work from the objective's allowance.

## Parameters, probes and acceptance

Every constant occurrence is an independent parameter, even when values are equal.
Occurrences are visited in postfix-node order. Each coordinate probes positive then
negative step relative to the value at entry to that coordinate. Accepted changes
are installed before visiting the next coordinate. Only a strictly lower finite
MSE wins; equal losses retain the incumbent. Operators, variable references,
operand order, topology, node count, depth and the input value's limits stay fixed.
Values are owned, detached copies; returned accepted doubles are never formatted
and reparsed on their way into a candidate. Export remains faithful.

A fresh baseline is always evaluated, including unchanged clones/elites. There is
no public numeric-score argument that could certify a different expression or
problem. Bounds and configuration are validated before that baseline. Starting
constants outside the fitting interval are API errors, not silently projected.
Probes must respect both the configured finite interval and expression hard
constant limits. They never clamp predictions. Nonfinite/out-of-bound values are
infeasible; unchanged values (including unrepresentable additions) and previously
visited exact expression spellings are duplicates and do not invoke the loss.
A bounded visited set prevents repeated probes of the same exact candidate in a
single fit. Positive/negative signed zero compare equal for unchanged-probe purposes.

After a complete sweep without improvement, halve the step. Stop before using a
step below minimumStep. Config also limits loss attempts (baseline included),
sweeps and proposals independently. Every proposed direction counts, including
skips: infeasible/unchanged values cannot cause an unlimited search. Set storage is
bounded by maxProposals+1 spellings; expression work per probe is O(nodes), visited-set lookup is bounded by
O(nodes*log(maxProposals+1)), and a loss is O(rows*nodes) with the evaluator's bounded temporary storage. Export/storage
cost also depends on the finite double spellings. Defaults: 256 loss attempts,
128 sweeps, 8,192 proposals, no loss threshold. Finite bounds are distinct from
strict per-node floating-point evaluation; see EXPRESSIONS.md for compiler flags,
rounding, subnormal and intermediate-error behavior.

`ConstantFitResult` returns the owned expression, its verified Fitness, starting
Fitness, improvement flag, stop reason, local loss/proposal/sweeps-begun counts. Stop
reason and improvement are independent: EvaluationLimit/SweepLimit/ProposalLimit/
MinimumStep can follow successful improvements. Invalid/worse/equal later trials
do not erase accepted improvements. No final verification call is needed: the
returned score is the last accepted evaluation of exactly that expression.

Precedence: invalid baseline -> InvalidStart with original expression and Invalid
fitness; baseline meeting optional threshold -> LossThreshold; otherwise no
parameters -> NoConstants; otherwise bounded search. Invalid-start recovery is
outside this fitter. No improvement is an honest result, not a global-optimality
claim. Coupled parameters can stall: `(a*b)*x` with both zero cannot improve by
changing either alone. Irrelevant constants and nonidentifiable parameterizations
also prevent unique coefficient claims. Predictions/loss, rather than particular
constant values, are the general criterion. `TrainingObjective::compute` is a
small protected seam for fault injection or a future stronger optimizer's use of
the same accounted objective; it is not a solver/plugin framework.

## Shared preparation and publication

`EvaluatedCandidate::calculateFitness` remains final, invalidates first and calls a
new protected nonconst `prepareEvaluation()` (default: the existing const
`evaluate()`). Scalar behavior and existing numerical evaluator overrides remain
unchanged. `ExpressionCandidate` optionally shares a trusted `RegressionPreparation`
service; it returns an expression/fitness pair for the same problem. This is an
evaluator extension contract, not permission to supply a fabricated/stale score.
The controlled candidate path validates/detaches the returned expression and text,
then installs its matching score through the final wrapper. Ordinary public
`replaceExpression` still invalidates cache and preserves old value/cache on error.

`ConstantRefinement` binds one immutable TrainingObjective and optional immutable
fit configuration. With no fit config it just performs the accounted training
baseline. The variation policy attaches this service to initialization candidates;
offspring copy the service and independently own their expression values. Thus
fitted constants actually survive publication and are inherited by subtree
operators; parents, elites and archive values have no mutable genome aliases.
Every private slot is freshly evaluated/refined, including elites and unchanged
clones. No problem-dependent cache is reused. The Population/GA transaction,
selection, ordering and best-seen code are shared unchanged.

Unexpected exceptions propagate with their nested original cause, training-loss
attempt number and the engine's candidate/generation context. A private fit may
have improved before throwing; no partial value is published. A generation abort
preserves committed genomes, cache, generation and archive, while consumed RNG
and attempted work stay consumed. Initialization failure has no committed best.
All-invalid initialization/successors retain NoViableCandidates with no automatic
regeneration. Operational failures remain distinct from mathematical invalidity.

## Accounting and global allowance

- RandomContext.evaluationAttempts / result.candidateEvaluations: outer slot
  requests begun, including ones that fail during preparation.
- TrainingWork.lossAttempts: every training objective invocation actually begun;
  baseline, accepted/rejected/invalid probes and throwing calls. Denied calls at an
  exhausted global allowance are not begun. Training verification must also use
  this objective if recomputed. Cache reuse would not count, but this fitter does
  not reuse baselines.
- fittingInvocations counts fits after config/value validation; fittingLossAttempts
  is the subset of lossAttempts inside those fits, **not an additional total**.
- proposals counts attempted signed coordinate probes, accepted counts strict
  improvements, invalid counts mathematically invalid *trial* losses (baseline
  invalidity is represented by InvalidStart), infeasible and duplicates count
  skipped probes. Rows are charged immediately before each attempted training row,
  including rows preceding an invalid result or exception. Counter overflow throws.
- Existing VariationStatistics retain attempted proposals/rejections/fallbacks and
  are not refunded. Post-search metrics/predictions are reported separately.

Local fit limits return their best accepted value. The high-level driver checks
both outer-call and training-loss allowances for at least a whole baseline slot
set before starting a generation. Refinement work is variable: if the global loss
cap is reached inside a generation, `TrainingLossBudgetExceeded` propagates through
the shared transaction; the driver recognizes only that explicit nested type and
returns EvaluationBudget with the last committed state. No implicit retry occurs. To inspect detailed attempted work after an unexpected
high-level exception, retain a `shared_ptr<TrainingObjective>` and pass it as the
optional fourth argument to symbolicRegression. It must be fresh and match the
problem identity and config.maxTrainingLossAttempts exactly; stale/mismatched
objectives are rejected. Without that argument, the driver owns its objective;
normal results carry counters, while exceptions retain loss-attempt context and
the caller's RandomContext retains outer-call/RNG state.
At initialization this can return no best. Generic manual GA callers receive the
exception and retain their population. A failing fit may consume a slot request
without beginning another loss. The attempt count includes discarded generations;
the published count does not. Do not call this an RNG/diagnostics rollback.

With fitting, P*(1+G) bounds outer requests only, **not total loss work**. The run's
explicit loss cap bounds actual loss calls. Equal allowances are not equal measured
work; unused allowance is reported. There is no cancellation or blocking-callback
interruption guarantee. External evaluator side effects are outside population safety.

SymbolicRegressionResult additionally records raw initial expression texts (slot
order), raw baseline best loss, post-refinement initial best loss, aggregate work,
and a compact committed-improvement trace (cumulative loss calls, generation, MSE).
If initialization aborts, raw baseline diagnostics cover only fits completed before failure.
Trace entries are recorded at publication, so their counts include the full
successful generation; there are no abandoned-partial best entries. Exact zero
stops retain ExactTrainingFit; a positive threshold reports TrainingLossThreshold.
The full exact-MSE/nodes/text ordering is unchanged and never uses tolerances.

## Continuous terminals and experiments

ExpressionGrammar defaults to continuousProbability=0, preserving the old discrete
mode and its RNG sequence. With a positive probability, each constant terminal
uses a Bernoulli choice between discrete list and uniform real interval
[continuousLower,continuousUpper), default [-8,8). Sampling uses only the supplied
run-owned engine. A degenerate interval produces that endpoint. Empty discrete
lists require probability 1 when continuous sampling is enabled; variable-only
terminals remain supported when probability is zero. Bounds and span are validated.
No target tree/template is injected. Repeatability claims cover the same library
build, compiler/standard library, seed, data, config, strict FP environment and
serial mode; standard distributions are not promised portable across toolchains.

See CONSTANT_FITTING_CAMPAIGN.md for the frozen paired search, all results and
traces. The independent fixed-structure tests intentionally know the quadratic
structure; those fixtures are never used by random initialization. They establish
numerical fitting independently of structure-search success.

Division and natural logarithm now participate in the same preparation/loss path.
Domain-invalid probes are ordinary Invalid fitness and retain accepted progress;
invalid starts still skip. TrainingWork.invalidLosses includes all returned-invalid
objective losses, while invalid counts fitting trial losses only. The one-sweep
domain fixtures and reused log/div fault/global-cap/publication tests are described
in [STRICT_ARITHMETIC.md](STRICT_ARITHMETIC.md). Same-build repeatability now also
requires the same runtime math library for logarithms. No new fitting algorithm,
rescue optimization or randomness was introduced.
