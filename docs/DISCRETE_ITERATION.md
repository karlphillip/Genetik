# D2: bounded discrete iteration

D2 adds opt-in `Sum`, `Product` and lexically bound Int64 iterator references to
D1. No generic GA header, continuous grammar, prime objective, fitter, version or
release changes. Recompile dependent translation units consistently: the optional
discrete limits, metadata and work structures have additive fields. No C++ binary
ABI guarantee is made.

## Public construction and execution

```cpp
#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionFormatting.hpp>
#include <DiscreteRunCheckpoint.hpp>

auto sum = parseDiscreteExpression("sum(k0, 1, x0, k0)", 1);
auto product = parseDiscreteExpression("product(k0, 1, x0, k0)", 1);
const std::array<std::int64_t, 1> input{5};
auto s = sum.evaluate(input);      // Valid Int64 15
auto p = product.evaluate(input);  // Valid Int64 120
```

`DiscreteNode::iterator(id)` constructs a reference; `iteration(kind,id)` constructs
an explicitly bound operator. Both node IDs and operator IDs are explicit fields,
not magic external indices or string replacement. Operators 0–14 retain their D1
meaning; Iterator=15, Sum=16, Product=17 are appended. `DiscreteExpression::iteration`
combines lower/upper/body expressions with an explicit identity. An open body can
be constructed with the fourth constructor argument `outerBindings`, an owned
list of explicitly permitted outer binder IDs. Open fragments cannot execute or
become regression candidates. A complete expression validates every reference.

Bounds/body are Int64; a Bool body or bound is rejected. Bounds are evaluated
lower then upper, strictly and successfully, before the range decision. Ranges
are inclusive and advance by one. Lower > upper returns sum 0/product 1 without
visiting the body. On nonempty ranges accumulation starts at 0/1, evaluates every
term in order, then performs a checked add/multiply. There is no zero-product
shortcut: later invalid arithmetic still invalidates the result.

`sum(k0,0,3,if(k0==0,0,10%k0))` returns 1. The modulo-by-zero branch never runs.
`product(k0,1,3,if(k0==1,0,1%(k0-2)))` is invalid at k0=2 even after a zero term.

## Scope and hygiene

An identity is visible only in its own body, not its bounds or siblings. Outer
identities may appear in nested bounds/bodies. Declarations are unique across a
complete expression, including outer fragment declarations: shadowing/reuse of
an ID is rejected rather than silently captured. External `xN` inputs and local
`kID` bindings are distinct namespaces; caller input vectors are never written.
Canonical binder names spell the numeric identity; they are not the evaluator's
lookup mechanism. References resolve against active evaluation frames by ID.

Construction performs iterative structural and lexical validation. At least two
nested levels are supported; the default expression limit is eight, with a hard
configuration ceiling of 128. A declaration/binding count limit defaults to 128
(maximum 4,096). Scope and frame storage are invocation-owned. Requested storage
accounting includes nodes, metadata, validation stacks and bounded scope buffers;
allocator bookkeeping/implementation overhead is not an RSS measurement.

`scopeAt(root)`, `iterationLevelAt(root)` and `discreteSubtree` expose checked
structural context. Extracted fragments retain only required free outer bindings.
`replaceDiscreteSubtree` requires those bindings in the destination context and
renames incoming internal declarations/references if IDs collide with retained
context. This explicit replacement API treats supplied free bindings as deliberate.

Crossover is deliberately more conservative: it rejects donor fragments with any
free iterator reference, even if another individual's unrelated binder has the
same numeric ID. Closed trees, including entire nested iterations, may move and
are hygienically renamed. This prevents accidental capture across individuals.
Mutation generates using the destination's actual visible IDs; the complete
result is revalidated. Moves outside binding scope reject. Failed attempts are
bounded by the configured proposal limit, then return an owned unchanged clone.
No parent, unrelated constant or fitness cache is mutated.

## Limits, work and cancellation

`DiscreteEvaluationLimits` appends:

| Field | Default |
|---|---:|
| maxIterationsPerOperation | 10,000 |
| maxAggregateIterations | 100,000 |
| maxIterationDepth | 8 |

Iteration counts are limited to at most 1,000,000,000 and nesting to 128 in API
configuration. Zero evaluation allowances are meaningful resource bounds; empty
ranges need no body iterations. Existing node visits, primitive operations and
workspace byte limits remain effective. No recursion or worker pool is used by
the evaluator. Frames are bounded by actual pushes; unselected branches consume
no evaluator work or nesting allowance.

After both bounds succeed, unsigned subtraction computes the ordered distance
without signed overflow. A nonempty range requires distance < per-operation cap;
the inclusive count need never be formed when it would overflow. In particular
INT64_MIN..INT64_MAX rejects before looping. Cursor increment occurs only when
cursor < upper, so no increment happens after INT64_MAX's final term.

Work units are explicit: each selected node entry is a node visit; existing D1
operations are unchanged. A range setup is one operation; each accumulation is
one, including failed overflow; each advance between terms is one. Aggregate
`iterations` counts bodies admitted, including a body that fails. `iterationOperations`
counts begun bounded constructs, including empty ranges. Setup, accumulation,
advance and next-body admission are ordered; resource failure can retain an advance
without admitting another body. No partial accumulator becomes a valid value.

`evaluateWithCancellation(inputs,limits,predicate)` checks a synchronous borrowed
predicate at every frame-machine step, including iterations. Existing atomic-flag
`evaluate` delegates to it. Caller callbacks are trusted, not retained, and must
honor their own concurrency/allocation contract. Regression binds the existing
executor's stop request so cancellation can interrupt a long row, not just occur
between observations. No signal handling or nested workers are introduced.

Mathematical invalidity, ResourceExhausted, Cancelled, API errors and operational
exceptions remain distinct. Regression propagates resource/cancellation failures
through failed private preparation, not Invalid fitness or finite penalties.
One complete objective invocation remains one loss attempt. Supplemental training
work includes iterations and begun iteration operations; per-loss aggregate bodies
are separately bounded (default 1,000,000). No second global primitive budget.

## Parsing and presentation

The grammar adds `sum(kID, lower, upper, body)` and
`product(kID, lower, upper, body)`. IDs are unsigned decimal integers, stable across
canonical export/reparse and node-codec reconstruction. Bounds parse outside the
new ID's scope; only the body sees it. The existing source/token/workspace bounds,
syntactic nesting default 64/hard ceiling 128 and complete-consumption rule remain.
Canonical D1 text is unchanged. There is no arbitrary-name alias or implicit
shadowing. Unicode uses `Σ[kID=a..b] body`/`Π[...] body`; LaTeX uses `\sum`/`\prod`.
Grouping, constants, operator identities and binding IDs are not simplified.
As in D1, canonical text beyond the parser ceiling reconstructs through the node
codec rather than receiving an unbounded parser-stack promise.

## Opt-in generation

Existing `DiscreteGrammar::operators` excludes iteration, preserving D1 draws and
accepted behavior. Add Sum/Product explicitly. `maxIterationNesting` defaults to
two for enabled grammars. Generation uses bounded contextual productions; visible
iterators are additional Int64 terminal choices only within bodies. Internal IDs
are assigned in construction order, then complete scope/type/depth/storage limits
are checked. Existing witness/fallback planning remains finite. An explicit
`iterationBoundsAreTerminals` restriction optionally limits bound roots to integer
terminals/external variables/permitted outer references. It is enabled in the
frozen demonstrations to make evaluation cost predictable; it is not an evaluator
restriction. Complete mutation/crossover validation enforces it.

D2 adds no new mutation strategy or altered event probabilities. Bound-expression
mathematical invalidity is never protected. No fitting, integer division,
number-theoretic shortcut or prime-discovery experiment is introduced.

## Checkpoint continuation

`DiscreteEvolutionRun` is a coordinator-owned adapter around the same generic
Population/GeneticAlgorithm, with one `step()` per generation. It borrows a fresh
run-owned RandomContext and owns immutable problem/configuration, objective and
algorithm state. `checkpoint()` captures only idle committed populations; `save`
uses existing checksummed atomic POSIX persistence. The caller may use the existing
`saveCheckpointSeries` for durable numbered retention. Loading validates before
workers run and before changing the caller's RNG.

The candidate codec retains `/1`, with an explicitly extensible operator-ID table.
Old 0–14 fields/payloads are byte-identical; the D1 reader rejects unknown new IDs
rather than reinterpreting them. A retained fixture generated by actual D1 source
verifies backward reading and byte-identical re-encoding. The generic envelope is
still format 1. The new full-run profile is `genetik.discrete-evolution/2`; D1 had
no complete discrete-run format to migrate.

The run identity stores the exact ordered dataset/schema, all expression/evaluation/
dataset limits, grammar terminal/operator order, binding policy, population/elite/
variation controls, total budget, generation limit, seed and worker count. It is a
bounded exact identity (1 MiB), not a collision-prone hash-only match. Different
iteration policies/grammars/data/runtime settings reject. Total-budget extension
is not supported by this strict D2 profile. Larger identity payloads explicitly
reject; ordinary D1 search outside this adapter retains its own dataset limits.

Population, integer coefficients/binder IDs, fitness/selection state, archive,
RNG, generation outcomes, variation counters and complete training work restore.
No scoring/RNG occurs during load. On interrupted private generations the last
committed population is saved along with actual consumed work and post-attempt RNG;
resume does not erase failed effort. Atomic replacement/cancellation machinery is
unchanged. Exact continuation is qualified within the same compiler/runtime/worker
configuration, not across unsupported environments or schedules at binding limits.

## Public examples and frozen demonstrations

```sh
out/build/iteration-release/genetik_discrete_iteration_quickstart --threads 4
out/build/iteration-release/genetik_discrete_iteration_campaign \
  --case series --seed 0 --threads 1 \
  --output out/runs/discrete-d2/example-series
```

The installed quickstart demonstrates sums, products, nested scope, canonical
round trips and shared-engine integer regression. The campaign accepts series or
factorial, real `--threads`, and `--resume` for its frozen profile. It freezes a
checkpoint and selected canonical model before assessment. On resume, reported
initial loss/generation refer explicitly to that checkpoint, not a fabricated
original initialization.

The separately committed protocol fixes both cases before execution: seeds
0,1,2,3,42; population 64, elites 2, crossover .9/mutation .35; 10,048 maximum loss
attempts; 200-generation ceiling; 25 nodes/depth 6; initialization depth 4/mutation
3; constants {0,1,2}; operators Add/Subtract/Multiply/Sum/Product; no fitting.
Training is series n=0..8 or factorial n=0..6; assessment is 9..12 or 7..9.
Targets use independent small-integer closed forms/reference multiplication.
Evaluation policy: 200,000 visits/operations, 1 MiB frames, 64 terms per operation,
1,024 aggregate terms, depth 2. Structural bounds are terminal-only. Cases/seeds
run sequentially; no exact target expression is injected, and generation-zero
success is retained. Failures and alternative finite-data models are reported.

[Results and qualification](DISCRETE_D2_QUALIFICATION.md) retain actual outcomes,
independent references, source-scoped gates and failures. Next is the consolidated
D3 discrete-sequence discovery/controlled-prime task, then D4 hosted qualification
and development release. D2 does not begin either task or introduce other phases.

D1 external inputs remain ordered Int64 bindings; Bool expressions are produced
by comparisons, boolean terminals and lazy branches. D2 does not add a separate
boolean-input schema or implicitly convert integers to booleans.
