# Refinement profiling and deterministic cadence

## Predeclared comparison (before implementation/results)

Exactly interval 1 (control/default) versus interval 4; no interval sweep or tuning.
Two problems: the accepted first-1,000-prime fitting set and strict-arithmetic
log(x0)+x1/x0 fixture. Seeds 0,1,2,3,42 for each, 100,000 actual loss attempts,
64 slots, 2,000 generation attempts, 31 nodes/depth 6, five equally weighted feasible
operators add/sub/mul/div/log, mixed half-discrete/half-uniform [-8,8) constants.
Keep accepted initialization/variation/rank/elitism policies and fitting settings
(bounds [-8,8], step 1/minimum 1e-8, local 256 calls/128 sweeps/8192 proposals).
Prime thresholds remain exact zero; arithmetic thresholds remain MSE <=1e-14.

Both modes refine generation zero identically; thereafter fit the generation being
prepared iff its prospective committed index is divisible by the interval. All
scheduled slots retain the old treatment, including elites/clones. No cache or
already-optimized flag. Unscheduled slots obtain fresh ordinary checked loss.

Run twenty serial Release searches without detailed diagnostics for runtime results.
Within each problem, order pairs by seeds 0,1,2,3,42. Even pair positions run control
then experimental; odd positions run experimental then control. Problem order:
prime then arithmetic. Verify raw and prepared initial populations/fitness/work
for each pair. Select each mode's representative by fitting MSE, nodes, text, lowest
seed. Freeze every selected model and representatives before any evaluation reports.
Prime 1001–1200 is the previously examined range, never a fresh holdout here.
No change of seeds/configuration based on results. Keep all unsuccessful outcomes.

Before the comparison, profile interval 1 on prime seed 2 (100,000-call ceiling)
and strict arithmetic seed 2 (same ceiling). Bounded exact-repeat diagnostics record
at most 1024 distinct input/output expressions per run; calculations always execute.
Compare each profiled result/RNG/work to its unprofiled counterpart. Diagnostic
clock calls occur at objective, fit/preparation and generation/publication boundaries,
never per node/row. Final runtime comparisons use diagnostics disabled.

The first implementation attempt aborted its new initialization-equivalence check
after the first prime pair, before any evaluation reporting. A C++20 lifetime error
in newly added audit capture dereferenced a temporary detached snapshot; retaining
its shared owner for the loop fixes it. The focused paired-initialization regression
also caught this. Partial diagnostic/timing outputs are retained under
cadence-results/audit-failure, excluded from policy conclusions. No search setting,
seed, objective, fitter or interval changed. The complete comparison restarts only
after this concrete engineering repair; this is not an accuracy-driven adjustment.

## Public API and preparation contract

```cpp
SymbolicRegressionConfig config;
config.fitting = ConstantFitConfig{};
config.fitEveryGenerations = 4; // default 1; positive uint64, validated even if fitting is off
RandomContext random(42);
const auto result = symbolicRegression(problem, config, random);
```

Initialization is generation 0 and is always refined when fitting is enabled.
Subsequent preparation uses the prospective committed generation: 4,8,12,... for
interval 4. Population supplies that generation to each private candidate, rather
than using attempt count. A failed attempt leaves the prospective generation and
its eligibility unchanged. Every slot on an eligible generation retains the old
fitting treatment. An ineligible slot receives a fresh ordinary score on the same
immutable training problem; **no score cache is reused in either mode**. Fitting
disabled remains distinct from a policy skip. No fit flag certifies convergence.
Local fitting/acceptance rules, RNG draws, checked loss and publication are unchanged.

ExpressionCandidate stores generation/origin metadata by value. Population has a
small optional preparation-generation hook; scalar candidates are unaffected.
ExpressionVariation annotates elites and final offspring using exact typed-tree
comparison against the immediate selected parent (father for first child, mother
for second), after crossover/mutation. Changed means representation changed, not
novel anywhere in the population. Same representation includes variable schema,
node order/kind, variable indices and every constant's binary64 bits, including
signed zero; it does not require equal structural-limit objects. No algebraic
normalization, score comparison or string approximation defines equality.
Custom two-argument RegressionPreparation implementations remain compatible; a new
context-taking overload delegates to the old method by default. Manual direct
ConstantRefinement.prepare calls without context use generation zero. Population
always overwrites the generation before scoring, including retries. Origin is
only diagnostic, never a scheduling/cache decision.

All preparation occurs on private copied candidates. Operational errors retain
nested cause/context; global exhaustion and all-invalid populations retain the
established no-publication/no-retry contracts. Work and RNG are never refunded.
The old objective's `measure(expr,bool)` remains supported; `TrainingLossKind`
distinguishes ordinary/baseline/probe work explicitly for new callers.

File and existing campaign commands accept `--fit-every N`; omission is interval 1:

```sh
../build-release/genetik_regression_search ../cadence-data/training.csv --inputs first,second --target target --seed 42 --loss-budget 100000 --operators add,sub,mul,div,log --fit-every 4
../build-release/genetik_multivariate_demo --arithmetic --fit-every 4
../build-release/genetik_prime_regression --campaign --output ../prime-four --loss-budget 100000 --fit-every 4
../build-release/genetik_cadence_campaign --output ../cadence-comparison
```

The last command runs two detailed seed-2 profiles followed by exactly twenty
unprofiled comparison runs. It refuses to overwrite an existing campaign log.
Training-data construction and file output are outside search timers. The full
prime/strict constants and policies are printed per run. Dataset/fitter/problem
configuration stays immutable. No candidate uses evaluation rows or target templates.

## Counters and timing

TrainingWork retains its legacy fields and adds ordinaryLossAttempts,
fittingBaselineAttempts and fittingProbeAttempts. Their disjoint sum equals
lossAttempts, while baseline+probe equals the legacy fittingLossAttempts. All charge
when a call begins, including invalid/throwing calls; a cap-denied call does not
begin. fittingImproved counts fit invocations with at least one accepted step,
including fits later abandoned. accepted still counts individual accepted steps.
A skipped request has no fitting calls; ordinary scoring actually begun is charged.

Result.preparationWork records all attempted requests by initialization, changed
child, unchanged child, elite or unspecified manual origin. It includes scheduled
fits, policy skips, ordinary/baseline/probe calls and actual rows per origin.
Completed outcomes are improved, unchanged, NoConstants return or InvalidStart
return, with independent ConstantFitStop counters. Aborted fits are separate.
Policy skips include cap-denied ordinary preparation requests, whereas ordinary
loss attempts count only calls actually begun. No skipped counter is incremented
when fitting is disabled. Unpublished loss/row/candidate counters subtract the
last committed work boundary from final attempted work (including no initial commit).
InitialWork and prepared expression/fitness audit vectors preserve initialization
evidence; these are owned, population-bounded copies, not writable score aliases.

Optional `config.diagnostics = std::make_shared<RefinementDiagnostics>(1024)` observes
one fresh run; a supplied TrainingObjective must use that same diagnostics object.
The table stores at most capacity input/output values (capacity 1..4096); verified
exact equality, not a hash alone, establishes repeats. A request is repeated only
if its input was presented for fitting before, under the same immutable problem and
fitter configuration. Previously returned fitter outputs are tracked separately;
being a prior output does not mean a new fit will leave it unchanged. The two
categories overlap and must not be summed. Capacity drops/untracked requests are
reported; repeat counts/work are lower bounds when capacity is reached. All actual
fits still execute. The observer supplies neither values nor scores to preparation.
Storage is O(capacity*maxNodes), with bounded faithful text and detached expressions;
it does not grow with every generation. This is instrumentation, not a cache.

Clock calls bracket objective calls, fitting, preparation and generation/publication,
never rows/nodes. Objective timing includes checked dataset loss and its wrapper,
not just arithmetic instructions. Fit/preparation/generation timings are inclusive
and overlap. The disjoint wall-time decomposition is: all objective time +
(fitting inclusive minus its objective time) + publication rank/archive/swap time +
remaining search time (other preparation, variation, copying and driver/audit work).
Publication timing is opt-in on Population; defaults make no clock calls. Profile
wall times include observer overhead; final paired runtimes have no detailed timers
or repeat-table lookup. No sampling profiler setup was needed. Counters and result
snapshots remain active in both comparison modes. Runtime is observational, not a
cross-platform or isolated-host benchmark guarantee.
