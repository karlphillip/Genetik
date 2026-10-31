# Typed symbolic regression — Phase 3 contract

This additive specialization uses EvaluatedCandidate, Population and GeneticAlgorithm;
the generic engine does not include mathematical headers. Existing scalar search
and ordering remain unchanged. This design is recorded before implementation.

Checkpoint 1 owns an ordered schema, rows and fixed output shape in
TypedRegressionProblem. Values are finite, dimensions match exactly, and all data
is detached before a run consumes randomness. Equal schema does not imply equal
objective identity. A new dataset requires a new problem/run. Each preparation
scores freshly; evaluation-only measure calls never update training fitness.

Loss is equal-weight MSE over rows and row-major output coefficients (vectors in
increasing index). Start sum at +0; for each residual check subtraction, square,
and sequential sum; divide once by the checked row/component product. Invalid
predictions or reduction arithmetic invalidate the whole loss. Reporting inspects
all rows when requested and counts invalid rows, never inferred invalid components.
Resource exhaustion is TypedResourceExhausted, distinct from invalid fitness and
operational errors. Admission charges a loss before validation/allocation; attempted
rows are charged before evaluation. Primitive work counts expression primitives
and checked loss subtraction/multiplication/addition/division separately from rows
and loss calls; it is diagnostic, not a new global engine budget.

Checkpoint 2 constructs from a finite shape vocabulary and enabled productions.
Subtrees match exact shapes and schemas, then complete replacements are revalidated
against all expression limits. Finite proposals fall back to an owned unchanged
value. Feasible witness plans permit result shapes without direct terminals.
Selection/elitism/archive order: finite MSE, nodes, depth, constant occurrences,
adjustable scalar coefficients, then faithful text. No fuzzy score sorting.

Checkpoint 3 uses the existing coordinate-search rules with independent constant
coefficients in postfix occurrence then row-major coefficient order. Refinement is
optional, scheduled at initialization and prospective generation multiples, private
to each task; accepted expression and score travel together. No random fitting,
shared scratch, nested workers, result cache or cross-problem fitness reuse.

All phases retain the existing admission/drain/failure-priority/publication policy.
Parallel callbacks require explicit qualification and independent worker factories.
Only committed generations can update best-seen state; abandoned work remains
counted. No typed dataset parser or new arithmetic operation is introduced.

Checkpoint 1 public API: TypedRegressionRow owns vector<TypedValue> inputs and one
TypedValue target; TypedRegressionProblem::create(schema, output, rows, expressionLimits,
datasetLimits) validates/copies them. Dataset limits default to 100,000 rows,
10,000,000 stored coefficients and 1,000,000,000 arithmetic units per loss.
checkedExpression rejects predictable full-loss work violations before search.
measure(expr, rowCounter, arithmeticCounter, inspectAllRows, localArithmeticCap)
can lower that allowance for a synchronous reporting call; exhausted calls throw
TypedResourceExhausted with attempted rows/work retained. Expression work plus three
loss primitives per component plus final division form the valid full-loss bound.
Invalid arithmetic can stop earlier. Reporting invalidRows counts expression-invalid
rows only; loss-overflow invalidity does not imply an invalid component count.
TypedTrainingObjective reuses shared TrainingLossAdmission and TrainingWork, charging
before compute; exceptions acquire nested attempt context. TypedScoring forks
exclusive objectives on workers, merging counters after drain. TypedExpressionCandidate
uses the same controlled edit/final fitness wrapper and problemIdentity contract as
scalar expression candidates; public replacements invalidate fitness even if equal.

## Constructive grammar and search

`TypedGrammar` declares enabled operators, finite admissible shapes, constant shapes,
coefficient terminal choices, depth ceilings, operation probability and proposal cap.
Default operators are add/subtract/scalar multiply/dot/matvec/matmul/transpose. Strict
scalar Divide/Log and scientific Sin/Cos/Exp/Sqrt/Powi can be enabled explicitly.
Empty shapes resolves distinct ordered
schema/output/scalar shapes. Each input shape must occur in the vocabulary. Planner
bounds are 16 shapes, 64 initial/mutation levels, fourteen distinct operator kinds
(nine original kinds plus the five scientific additions) and
4096 proposals. Constants sample each coefficient independently from declared doubles.

The planner builds one least-node validated witness per shape and allowed depth,
then enumerates feasible productions using those witnesses. It is conservative:
limits may exclude a witness even when a different non-minimal plan would work.
An unsupported/unwitnessed requested output is an API error before RNG consumption,
not NoViableCandidates. This is a bounded constructive grammar, not a complete
proof of realizability for arbitrary resource combinations. Operations remain
possible when their result has no terminal. Selection is uniform over feasible
operator/operand-shape productions. Remaining node budget is allocated subject to
both child minima. Construction and complete-tree limit rejections have finite
proposal caps and validated-witness fallback; counters expose both events.
No mathematical-validity retry is performed. Restrictive construction may bias the
search space and should be declared with results.

`TypedVariation` supplies initialize/mutation/crossover to the unchanged GA policy
interface. `typedSubtree`, `typedSubexpression`, `replaceTypedSubtree` preserve schema
and exact result shapes. Replacement checks the whole new postfix tree, including
destination depth, coefficient counts, primitive plan and payload. Metadata is
recomputed by validation, never reused after an edit. Only TypedLimitError represents
a rejected structural proposal; unexpected allocation/errors propagate.

`typedSymbolicRegression(problem, config, run, optionalObjective)` uses the existing
Population/GeneticAlgorithm transaction, stable ordering, worker ownership and
publication; its outer driver only handles stopping/accounting like scalar search.
Fresh run/objective required. Result contains exact initial snapshots, committed
best/history, work/row/arithmetic counters, unpublished work, variation statistics
and stop reason. Ordinary scoring is fresh even for elites/unchanged clones. Optional
fitting is implemented in checkpoint 3 below. The generic core and scalar driver are unchanged.

## Aggregate coefficients and optional refinement

`TypedConstantFitting.hpp` provides `fitTypedConstants(expression, objective, config)`
and `withTypedCoefficient(expression, node, coefficient, value)`. Parameters enumerate
constant nodes in postfix order, then coefficients in vector-index/matrix-row-major
order. Equal-valued coefficients/occurrences remain independent. A validated owned
replacement preserves topology/schema/shape. Dataset inputs are never adjustable.
No parser/format conversion lies between scoring and installation.

Both scalar and typed adapters call the same internal coordinate-search kernel:
positive then negative probes relative to coordinate-entry value; accept only
strictly lower finite loss; halve step after a complete unimproved sweep. A fresh
baseline certifies the exact expression/problem. Bounds/proposals/sweeps/loss calls
are independently finite. Invalid or worsening probes retain earlier improvements;
local stops return that matching value/loss, with improvement separate from reason.
Invalid starts are skipped, no-constant/threshold starts return explicitly, incompatible
starting bounds are API errors. Coupled zero parameters can stall. There is no
claim of global optimality or identifiable coefficient recovery for arbitrary trees.
See CONSTANT_FITTING.md for shared numerical rules.

`TypedSymbolicRegressionConfig::fitting` is optional (default disabled), using
ConstantFitConfig. `fitEveryGenerations` defaults1, positive; initialization is0,
then prospective committed multiples are refined, including elites/unchanged clones.
Other generations score freshly. Failed attempts do not advance that schedule.
TypedConstantRefinement supplies this trusted preparation boundary and reports the
same origin/termination/skip categories as scalar refinement. No already-fitted flag
or numerical cache exists. Fitted constants become the inherited owned genome.

TypedTrainingObjective is run-owned/noncopyable; its work is read-only. Subclasses
may override compute/cloneForParallel/mergeWorker and must explicitly qualify parallel
safety. `computeWithLimits` supports resource-controlled evaluation with the same
accounting. Worker factories must return independent, fresh objective instances with
identical problem and allowance; only existing synchronized admission is shared.
Each worker has private counters, candidate, fitting state and invocation scratch.
The coordinator merges after drain, orders stable slots, handles best-seen/publication
and owns every RNG draw. No matrix kernel starts workers. Concurrent public engine
mutation/destruction remains unsupported. External callback side effects cannot be
rolled back. Operational failures outrank budget/cancellation and preserve their
nested cause. Resource exceptions are operationally failed preparations, never
Invalid fitness or NoViableCandidates.

Counters: outer RandomContext candidate calls; TrainingWork actual loss attempts,
ordinary versus fitting baseline/probe disjoint subsets, attempted rows, fit/proposal/
accepted/invalid/infeasible/duplicate diagnostics. Fitting work is a subset, never
added twice. Denied global admission is not a call; throwing/invalid/baseline/probe
calls count. No fitted score requires an extra verification evaluation. Reporting
via problem.measure is separate unbudgeted work, never a search-budget bypass.
`arithmeticOperations` counts expression primitives plus executed checked loss
subtraction/square/sum and final division. Node-copy/transpose/validation work is
bounded by dataset/expression storage limits but not arithmetic units. Local plan
limits do not create a second global primitive-work admission system.

`TypedExpression::evaluateWithWork(bindings, exclusiveCounter, budget)` is an optional
synchronous diagnostic using the same interpreter as evaluate. It increments before
each primitive, including work before an allocation/unexpected failure. Invalid domain
checks have zero primitive charge. Counter overflow is operational. Do not share a
mutable counter concurrently. Returned per-call counts and objective totals are
alternative observations, not additive work; no result or binding reference escapes.
Existing evaluate signature/semantics remain unchanged.

## Public workflow

Complete installed usage: [typed_search.cpp](../examples/quickstarts/typed_search.cpp).
It uses caller-owned rows, optional fitting/cadence, a finite allowance and real
run-owned workers. Search completion never depends on recovering a random target.

```cpp
#include <TypedSymbolicRegression.hpp>
const std::array schema{TypedShape::vector(2)};
const std::vector<TypedRegressionRow> rows{
    {{TypedValue::vector(std::array{1.,2.})}, TypedValue::vector(std::array{2.,4.})}};
auto p = TypedRegressionProblem::create(schema, TypedShape::vector(2), rows);
TypedSymbolicRegressionConfig c;
c.population=8; c.elites=1; c.maxEvaluations=40; c.maxGenerationAttempts=4;
c.maxTrainingLossAttempts=256; c.fitting=ConstantFitConfig{};
c.fitting->maxLossAttempts=4;
RandomContext run(42, ExecutionConfig{2});
auto result=typedSymbolicRegression(p,c,run);
// Inspect result.stoppingReason; best may be absent if initialization never commits.
// If present: best->text(), expression().schemaString(), shape(), evaluation().
```

Application commands (stdout/files remain under explicit destinations):

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j2
build/release/genetik_typed_regression --output out/runs/typed-discovery-new
build/release/genetik_typed_regression --smoke --fit --threads 2 --output out/runs/typed-search-smoke-new
build/release/genetik_typed_regression --fit-fixture --output out/runs/typed-fitting-new
```

Default discovery uses the frozen no-fit protocol. --fit enables bounded default
refinement (four calls per fit in --smoke); --threads applies the existing execution
limit inside each sequential seed run, never across seeds. Fixed-structure fitting
is synchronous and rejects --threads other than1. It uses no RNG. App parsing/data
and independent target loops remain outside the library. Typed text input, aggregate
CSV syntax/parsers, new operations, shared parameters, calculus and numerical backends
are future scope. The conservative finite witness grammar is a current restriction.

Operation recovery and supplied-structure coefficient recovery are separate evidence:
[TYPED_DISCOVERY_RESULTS.md](TYPED_DISCOVERY_RESULTS.md),
[TYPED_FITTING_RESULTS.md](TYPED_FITTING_RESULTS.md). No broader mathematical-equivalence
claim follows solely from sample metrics.

## Scientific scalar productions (Phase 4)

TypedGrammar may opt into Sin, Cos, Exp, Sqrt and Powi; all require scalar
operands/results. For example set `config.grammar.operators` to
`{TypedExpressionKind::Multiply, TypedExpressionKind::MatVec,
TypedExpressionKind::Exp, TypedExpressionKind::Powi}` and
`config.grammar.exponents = {2,3}` on a TypedSymbolicRegressionConfig. The finite
shape vocabulary must contain the operand/result shapes needed by those productions.
A scalar exp result can scale matvec(A,x); there is no elementwise exponential.
Complete-plan work and temporary bounds still apply. Each fitted coefficient remains
independent; integer exponents are discrete metadata and never fit coordinates.
The existing typed application keeps its declared operation-discovery fixture;
custom scientific typed problems use these public controls. No typed file parser
is added. See [SCIENTIFIC_OPERATORS.md](SCIENTIFIC_OPERATORS.md).
