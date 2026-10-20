# Bounded symbolic regression through the shared engine

The public API accepts ordinary ordered multi-input/single-output rows. See [MULTIVARIATE.md](MULTIVARIATE.md) for the positive-dimensional contract, caller-provided C++ data and a numerical-file workflow; these use the same engine and checked loss. Optional continuous refinement and its additional budgets are documented in [CONSTANT_FITTING.md](CONSTANT_FITTING.md); the candidate-call bound below describes the original no-fitting configuration. The only polynomial-specific code is the public example and tests; neither candidate initialization nor variation has a target template.

```cpp
#include <SymbolicRegression.hpp>
#include <iostream>

const auto problem = RegressionProblem::create({
    {{-1.0}, 1.0}, {{0.0}, 1.0}, {{1.0}, 3.0}
}, {31, 6, 2}); // node, depth, absolute constant limits
RandomContext run(0);
SymbolicRegressionConfig config; // 128 slots, 250 attempts, 32,128 candidate calls
const auto result = symbolicRegression(problem, config, run);
if (result.best) {
    std::cout << result.best->text() << " mse=" << result.best->fitness() << '\n';
}
std::cout << searchStopName(result.stoppingReason) << '\n';
```

This is a fit on supplied rows; three fitted observations do not prove a formula on other inputs. Replace the rows with another validated problem and create a new run. Read [POLYNOMIAL_CAMPAIGN.md](POLYNOMIAL_CAMPAIGN.md) for the prescribed 17-row experiment, frozen settings and all five results. Run `../build-release/genetik_polynomial_regression` for that campaign. Full campaigns are deliberately excluded from CTest; routine sanitizer coverage runs a short, bounded search.

## Integration and compatibility

- `EvaluatedCandidate` owns the common private Fitness state and roulette metadata. Its final evaluation wrapper invalidates first, accepts only Valid/Invalid results and propagates exceptions. Base copying/assignment is protected so users cannot assign a score cache independently of a genome through a public base reference.
- `Creature_i<T>` derives from it and retains its private arithmetic vector and final scalar editing operations. Existing character/scalar constructors, methods and two-argument GA template uses are unchanged. Default scalar variation moved to `ScalarVariation<C>` with its original draws/semantics; default elitism remains zero.
- `ExpressionCandidate` derives directly from the common base, owns a validated Expression and cached faithful text, and refers to a shared immutable RegressionProblem. There is no dummy scalar chromosome or floating-point operator encoding. Copies own independent expression vectors; problem sharing is read-only. The expression reference/text accessors are const, with ordinary object lifetime requirements. Do not use moved-from candidates before reassignment (inspection/execution can report logic errors); destruction/assignment are supported.
- `replaceExpression` constructs and validates the entire value, schema, limits and text first. Only then does it swap in the value and invalidate fitness. Failed replacement preserves old expression, text, fitness and roulette metadata. Whole-candidate assignment prepares a complete matching copy and swaps without throwing; moving a candidate invalidates the source cache.
- `Population<T, Ordering=FitnessOrdering<T>>` now requires the common evaluated-candidate base. Its existing detached staging, evaluation, ranking, committed state and no-throw swap are shared by both representations. Scalar ordering is unchanged. The optional stateless ordering policy is used for valid ranking, tied-rank grouping, current best and historical best. `bestGeneration()` records the committed generation at which the current archive entry first won the complete ordering; unchanged ties keep the earlier record.
- `GeneticAlgorithm<P,C,Variation=ScalarVariation<C>>` delegates only variation. Constructor arguments after the existing rates/context are the variation value and optional elite count. Elites are copied from the population's existing best-first tail; selection and every offspring pass still use the existing engine and publication function. Elites must be fewer than slots; fewer viable elites simply leave more offspring slots. Every slot, including elites/unchanged clones, is reevaluated before publication.
- Configured expression candidates cannot be initialized by an unconfigured C(engine). Use `ExpressionVariation(problem,grammar).initialize(run.engine())` to build a vector, then the existing Population vector constructor. Population's virtual size-based initialization uses `if constexpr`; unsupported setSize now throws an explicit API error, preserving committed state, instead of failing template instantiation. Scalar C(engine) initialization remains supported.

For manual composition:

```cpp
ExpressionVariation variation(problem);
std::vector<ExpressionCandidate> initial;
for (int i = 0; i < 128; ++i) initial.push_back(variation.initialize(run.engine()));
using P = Population<ExpressionCandidate, RegressionOrdering>;
P population(initial, run, ObjectiveDirection::Minimize);
GeneticAlgorithm<P, ExpressionCandidate, ExpressionVariation> ga(.9f, .35f, run,
                                                               std::move(variation), 2);
// Caller checks its budget at a generation boundary, then checks outcome/exception.
const auto outcome = ga.evolve(population);
```

Use a separate fresh context for each snippet/run. Custom representation authors must preserve deep genome copies, private controlled edits and the same fixed evaluation problem; the common base is not permission to share mutable genomes. Existing custom GA subclasses still have virtual crossover/mutation extension points; the policy avoids instantiating scalar bodies for expression types.

## Dataset, loss and ordering

RegressionProblem copies its rows and nested input vectors even for rvalues. It is nonassignable, exposes only values/configuration, and is shared as const. It validates nonempty rows, consistent input dimensionality (zero-variable constant problems are permitted), finite inputs/targets, finite positive structural limits/nonnegative constant bound, and the supported MeanSquaredError loss kind. A candidate must match dimensions and hard limits. No input normalization, hidden repair, rounding or held-out data is used in fitness.

Population binds a shared problem identity before initial evaluation, including an all-invalid initialization. Every later candidate must have that exact identity; mixed or changed problems are API errors before evaluation. Changing data, loss or hard limits requires a new problem and run. Clones/elite caches are not reused during publication, even when unchanged. Fixed problem/objective and deterministic serial execution remain required.

For each training row in order, evaluate the strict expression; compute/check the residual, its square and a sequential double sum; finally divide by the full row count and check the mean. Stores keep steps separate under the expression header's strict FP contract. Any non-finite prediction or loss intermediate returns Fitness::invalid() and no partial max-error metric. This deliberately rejects sum overflow even if a rescaled mathematical average might be finite. No sample is silently omitted, and no arbitrary poor score/NaN sentinel represents invalidity. Bad schemas/configuration and unexpected exceptions propagate unchanged into the engine's nested evaluation/generation context.

Primary ordering is exact finite training MSE ascending (ObjectiveDirection::Minimize is mandatory and validated even for a singleton). Only when MSE is exactly equal, prefer fewer nodes, then lexicographically smaller faithful expression text. Identical complete keys retain the prior historical best and discovery generation. Population's linear-rank roulette uses the same complete ordering and averages ranks only for identical keys; invalids receive no weight. Elitism copies the same sorted tail. No epsilon enters a comparator; training MSE == 0 is the example's early stop, and max-absolute error <=1e-9 is a separate post-run numerical-recovery report.

`RegressionProblem::measure` is also the public evaluation/reporting API: returns loss state, optional max absolute error and actual samples attempted. Its metric work does not increment RandomContext's search counter. Bind evaluation-only data to separate immutable problems and call measure only after search/selection are frozen.

## Initialization, subtree operations and limits

ExpressionGrammar contains the allowed unary/binary operator list, finite constant terminal list, initial/mutation maximum subtree depth, grow probability and finite positive proposal limit. Constants must meet the problem's bound. Empty operators allow terminal-only search; empty constants allow variable-only terminals when variables exist. No-terminal configurations are rejected. All random choices receive the existing run engine.

Initialization chooses depth uniformly up to the configured cap (clamped to the hard depth), then a half full/half grow choice. An explicit task stack generates postfix children left-to-right. A branch requires remaining budget >=arity+1 and depth >1; unary gets all remaining child budget; it divides child node budgets before generation, avoiding count overflow and producing feasible trees directly. Terminal selection is half variable/half constant when both exist, uniform within the chosen family. A full task branches wherever budgets permit; a grow task uses growBinaryProbability. Different depths and unused child budgets produce varied sizes, including leaves. Initialization never sees targets and performs no stochastic retry.

`expressionSubtree` returns a half-open postfix interval for a checked root index. `replaceExpressionSubtree` preserves all surrounding node/operand order, checks schema, uses subtraction before node-budget addition, scans the **complete proposed tree** to check destination depth/constant bounds and constructs a validated Expression. Structural infeasibility returns nullopt. Invalid API inputs/limits and allocation failures throw; they are not ordinary rejected proposals.

Mutation selects a uniform root/leaf and generates a new subtree using the mutation depth cap. Crossover selects one root from each parent, exchanging their complete subtrees; both children must fit. The configured positive proposal limit bounds rejection attempts. If every proposal is structurally inadmissible, return unchanged parent copies. Valid identical proposals are accepted with zero actual changes. A successful controlled replacement marks fitness Unevaluated even if the expression is unchanged; a fallback/disabled clone may carry its matching cache until publication reevaluates it. Root/internal replacement, different sizes, subtraction order and complete destination depth are tested.

VariationStatistics records attempted initializations, mutation/crossover proposals, structural rejections, separate fallback counts and actual expression changes (including constant changes). Crossover proposals count pairs, changes count children. Counts and consumed draws are not refunded if later preparation/evaluation aborts. Counters reject overflow rather than wrapping. These are attempted-work diagnostics, not a count of only published changes.

## Budget and failure boundary

The high-level symbolicRegression driver validates configuration and requires a context with zero prior managed evaluation calls. It initializes through ExpressionVariation, then uses the ordinary Population/GeneticAlgorithm loop. It adds only boundary budget/stopping control: it does not implement another transaction or replacement mechanism.

Before initialization, require space for all slots; otherwise return EvaluationBudget with no best, draws or evaluations. Before each generation, require enough remaining calls for a whole slot set; otherwise stop at the boundary. Each publication invokes the evaluator once per slot, including invalid/throwing results and elites. With P slots and G attempted generations, managed calls <= P*(1+G); the fixed campaign bound is 128*(1+250)=32,128. A failed attempt consumes at most its remaining slot calls, so it cannot evade this bound. Remaining budget uses subtraction to avoid overflow. Generic manual GA users must supply their own boundary budget; no hidden budget is embedded in Population.

GenerationOutcome, no-throw committed-state ownership replacement, archive, problem identity and nested-error behavior remain shared. Variation/construction/evaluation failures leave committed expressions, caches, ranking, generation and archive untouched. RNG draws and attempted candidate calls remain consumed. `ga.attempts()` advances for ready attempts, including failures; `ga.evolutions()` only advances after publication. An all-invalid initialization has no committed generation/best and stops with NoViableCandidates after the finite initial calls. An all-invalid successor preserves the last committed state and returns NoViableCandidates, without regeneration or restart. Unexpected operational failures propagate; manual engine callers retain the population and diagnostics, and high-level callers retain their run context/call count. External evaluator side effects are outside this guarantee. There is still no threading/cancellation/observer subsystem or promise to interrupt blocking callbacks.

SymbolicRegressionResult includes optional committed best/initial MSE/discovery generation, attempts/published generations, managed candidate calls, variation counters and stopping reason. Elapsed time is measured externally by the example and excluded from repeatability checks. Per-sample/node search work is bounded by rows*nodes per candidate but is not a separate aggregate search counter yet; measure reports sample attempts for post-run accounting. Optional continuous fitting now adds explicit training-loss/row accounting and global loss-budget handling through the same publication path; see CONSTANT_FITTING.md.

## Qualification and remaining work

Focused tests independently establish zero target loss and f(x)=x MSE 3; dataset/schema/configuration errors; prediction/residual/square/sum overflow and no partial score; controlled invalidation/replacement safety; fixed problem binding; subtree size/depth/destination constraints and subtraction order; bounded rejection/fallback and actual structural changes; copy/parent/archive safety; injected variation and post-work evaluator exceptions with nested causes and retained draws/calls; all-invalid initialization/successor; elitism/complexity ties; and boundary budgets. Two independent short integer-terminal searches on fractional targets must complete exactly three generations and match expression, fitness, discovery generation, counters, stop condition and engine state under the same build/seed/configuration.

See BASELINE.md for exact Release, Debug, ASan+UBSan, warning, header and downstream qualification. The five-seed campaign is evidence about this fixed discrete grammar and polynomial, not a correctness test or a guarantee on arbitrary targets.

Bounded training-only coordinate fitting is implemented and verified, preserving accepted local improvements and committed state on operational failure; the optional protected preparation hook leaves existing evaluate overrides unchanged. Keep configurable division/natural-log semantics, complexity archives, indexed-prime regression with distinct frozen fitting/validation/extrapolation reports, and CPU profiling before any GPU work in the roadmap.

Strict division and natural logarithm are opt-in through the same public grammar and shared engine. See [STRICT_ARITHMETIC.md](STRICT_ARITHMETIC.md) for domains, mixed-arity structure, fitting invalidity/counters, runtime libm and file selection/reporting. Default +,-,* draws and meaning are preserved.

## Prime-prefix application

[PRIME_REGRESSION.md](PRIME_REGRESSION.md) supplies the first 1,000 indexed primes
as ordinary one-input normalized rows, evolves structures and constants using this
API, and compares frozen models with five separate baselines. Scaling, reference
generation/validation, QR fits, int64 diagnostics and exports stay in the example.
See [PRIME_CAMPAIGN.md](PRIME_CAMPAIGN.md) for the complete unsuccessful/successful
approximation evidence; no exact-prime recovery is promised.

## Fitting cadence

Set `config.fitEveryGenerations = 4` for the optional deterministic policy: refine
generation zero, then 4,8,12,... based on the population's prospective committed
generation. Default 1 preserves accepted computations and old counters. All slots
are scored freshly; unscheduled scoring neither refines constants nor reuses cached
fitness. Origin annotations only classify diagnostics. Failure leaves the schedule
anchored to the same committed generation. The returned preparation/initial audit/
unpublished-work counters distinguish ordinary calls, fitting and policy skips.
Optional bounded `RefinementDiagnostics` observes work without changing it. See
[FITTING_CADENCE.md](FITTING_CADENCE.md) for API and [the measured comparison](FITTING_CADENCE_RESULTS.md).
