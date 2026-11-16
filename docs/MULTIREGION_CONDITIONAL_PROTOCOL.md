# Multi-region conditional recovery protocol

Frozen before running recovery on this task branch (base `73d2403aa6c3f076982cb0dafcdb0e5fb19935e9`). The constructor receives only each listed training table and the declared grammar. Output rules below are independent generators used to create the tables; they are never passed to synthesis. Families A and B use Int64 outputs and constant-only branch terminals `{10,20,30}`, making exact one-selector recovery impossible across the three labels. The guarded cases use Int64 outputs and their separately declared terminals and arithmetic productions.

Fixed resource limits for A-C: 100,000 generated/retained pool expressions, 500,000 construction attempts, 1,000,000 selector candidate combinations, 32 complete-expression nodes, depth 10, 20,000,000 cumulative evaluation work, and 128 MiB estimated logical memory. D uses the identical selector, work, pool, attempt, node, depth and memory caps. Boolean nesting is zero in all controls. Baselines differ only in `maximumConditionalNesting=0`; nested runs use exactly one.

## Family A: sign and modular regions

- Schema: one Int64 input `x`.
- Rule for table generation: `x < 0 ? 10 : (EuclideanMod(x,3)==0 ? 20 : 30)`.
- Training inputs, in order: `[-6,-2,-1,0,1,3,4,6]`.
- Held-out inputs, kept separate until selection: `[-9,-4,-3,2,5,7,9]`.
- Grammar: variables `[x0]`; predicate integers/constants `[0,3]`; branch constants `[10,20,30]`; predicate productions `LessThan, Modulo, Equal`; no branch integer operators; predicate nodes 5, branch nodes 8, expression depth 8; Boolean nesting 0; nested integer conditional nesting 1.
- Training labels: `[10,10,10,20,30,20,30,20]`.

## Family B: independent multivariate modular regions

- Schema: two Int64 inputs `(x,y)`.
- Rule for table generation: `EuclideanMod(x,2)==0 ? 10 : (EuclideanMod(y,3)==0 ? 20 : 30)`.
- Training rows, in order: `[(-2,-3),(-2,1),(-1,-3),(-1,1),(0,2),(1,3),(2,4),(3,6)]`.
- Held-out rows: `[(-4,5),(-3,-6),(-1,0),(0,1),(1,-3),(4,2)]`.
- Grammar: variables `[x0,x1]`; predicate integers/constants `[0,2,3]`; branch constants `[10,20,30]`; predicate productions `Modulo, Equal`; no branch integer operators; predicate nodes 5, branch nodes 8, expression depth 8; Boolean nesting 0; nested integer conditional nesting 1.
- Training labels: `[10,10,20,30,10,20,10,20]`.

## Family C: guarded lazy regions

- Schema: one Int64 input `x`.
- Rule for table generation: `x==0 ? 7 : (x<0 ? EuclideanMod(x,-x) : -1)`; the `x mod -x` component is valid for selected negative inputs and has a zero divisor only at zero, where the enclosing branch must remain unselected.
- Training inputs: `[-3,-1,0,1,2]`.
- Held-out inputs: `[-8,-2,0,3,9]`.
- Grammar: variables `[x0]`; predicate integers/constants `[0]`; branch constants `[7,0,-1]`; predicate productions `Equal, LessThan`; branch productions `Modulo, Negate`; predicate nodes 3, branch nodes 8, expression depth 8; Boolean nesting 0; nested integer conditional nesting 1.
- Training labels: `[0,0,7,-1,-1]`.

The matched baseline is the unchanged selector with nested integer conditionals disabled, using identical rows, terminals, predicates, and upper node/depth limits. Since only constants can be emitted as leaf branches in A/B and there are three distinct labels, its single output conditional has at most two values and cannot be exact. For C, one selector can represent at most two regions; the three labels and invalid-at-zero guarded component require the nested branch. Held-out values are evaluated only after the synthesizer returns. No target expression or complete predicate/component is supplied to it.

## Resource-failure records and guarded overflow addendum

The first runs used fixed selector caps of 1,000,000 candidate combinations and 20,000,000 aggregate evaluation work. Family B was resource-exhausted before selector evaluation: 34 retained predicates, 516 retained branches, and 9,052,704 possible tuples. Family C's `x mod -x` grammar was resource-exhausted during pool trace reduction after generating 82,774 expressions and 41,532 construction attempts. These frozen outcomes are retained; neither cap was raised.

Before the next run, a separate guarded-overflow case D was added to test the same three-region/lazy-invalid property with a smaller declared arithmetic grammar:

- Training inputs in order: `[INT64_MIN,-3,-1,0,2]`; labels `[7,3,1,-1,-1]`.
- Independent table rule: `x==INT64_MIN ? 7 : (x<0 ? -x : -1)` with checked Int64 negation.
- Held-out inputs: `[INT64_MIN+1,-8,-2,1,9]`, assessed only after selection.
- Grammar: variables `[x0]`; predicate constants `[INT64_MIN,0]`; branch constants `[7,-1]`; predicate productions `Equal,LessThan`; branch production `Negate`; predicate nodes 3, branch nodes 7, expression depth 8; Boolean nesting 0; nested integer conditional nesting 1.
- One-selector baseline nesting 0; resource caps remain 1,000,000 tuples and 20,000,000 aggregate work, 100,000 pool expressions, 500,000 construction attempts, and 128 MiB estimated memory.

Family D's intended nested branch has a negation that overflows at `INT64_MIN` but is protected by the selected outer arm. Its composition was frozen from the independent rule above, not supplied as a synthesizer component.

## Measured outcomes

Under the fixed limits above, A exactly matches 8/8 training rows. It generates 4,516 expressions in 2,318 construction attempts, retains 17 predicates and 117 branches, evaluates 232,713 selector tuples, charges 15,959,975 total work units, estimates a 9,633,710-byte peak, and takes 560 ms in the recorded Release run. The matched one-selector baseline is `Partial`, 6/8, selecting `if((x0 < 0), 10, 20)`; the nested selection is `if((x0 < 0), 10, if((0 < (x0 % 3)), 30, 20))`.

D exactly matches 5/5 training rows. It generates 2,136 expressions in 1,071 attempts, retains 7 predicates and 118 branches, evaluates 97,468 tuples, charges 5,456,493 work units, estimates a 4,763,106-byte peak, and takes 199 ms. Its matched baseline is `Partial`, 3/5, selecting `if((x0 == INT64_MIN), 7, -1)`; the nested selection is `if((x0 == INT64_MIN), 7, if((x0 < 0), neg(x0), -1))`. Candidate reduction also observes the invalid `neg(x0)` training behavior at `INT64_MIN`; lazy evaluation keeps it unselected in the complete expression.

B's one-selector baseline is `Partial`, 7/8, selecting `if((0 == (x0 % 2)), 10, 20)`. The nested pool has 34 predicates and 516 branches, so all possible tuples number 9,052,704; the fixed one-million cap returns `ResourceExhausted` before selector evaluation. C's matched one-selector baseline is partial; the nested guarded-modulo search exhausts the aggregate 20-million work bound during trace reduction. Held-out checks are performed only for selected exact A and D expressions; no candidate is selected for B or C.

## Separate partition-directed follow-up

The historical B result above is unchanged and remains a one-million Cartesian
tuple-cap exhaustion. A separate opt-in selector contract keeps the same grammar,
training and held-out rows, pool/attempt/node/depth/work/memory limits, and adds an
independent maximum of 1,000,000 structural-class joins. It does not reinterpret
`maximumCandidateCombinations`.

For each valid predicate, the selector scores each branch on only its selected
training rows and rejects a branch only when it is mathematically invalid on one
of those rows. It indexes the highest regional score, then earliest original
branch index, for each exact `(node count, depth)` class and region. Every pair of
classes is joined under the complete node/depth limits. This reduction is sound
for the existing score and tie order: candidates in one exact structural class
pair have identical structural feasibility, and regional validity and score are
additive across the predicate's disjoint true/false partitions. Empty regions
are vacuously valid. The final selected expression is reevaluated with the
production checked evaluator, including combined evaluator limits and lazy
branch semantics. This is finite-table selection only, not universal equivalence.
The directed path's cumulative work charges actual trace handling, row scans,
index comparisons, joins, construction, and final-winner evaluation; it does not
simulate evaluator calls for Cartesian tuples it did not materialize. Thus a
resource stop can differ from exhaustive tuple traversal, while mathematical
selection and score are preserved whenever the directed search completes. This
operational difference is isolated behind the opt-in mode and its separate join
budget; the historical selector and tuple-cap status are unchanged.

On B the directed mode reaches exact training recovery using 153 joins rather
than 9,052,704 Cartesian tuples, avoiding 9,052,551 tuples. It scans 140,624
partition rows and performs 72,108 scalar structural-index comparisons. The run
charges 672,965 cumulative work units, estimates 31,968,320 B peak logical memory,
and takes 21.6 ms median over three Release constructor runs. The full focused
test process observed 55,572 KiB peak RSS; this process-level measurement is not
attributed to B alone and is recorded in `BASELINE.md`.
The selected expression (an exact finite-training fit, not structural recovery of
the frozen rule) is
`if((x1 == (x0 % 2)), 30, if((0 == (x0 % 2)), 10, 20))` and is independently
checked against all six held-out rows. That agreement does not establish
mathematical equivalence: the independent frozen target and selected candidate
disagree at `(0,0)` (10 versus 30) and `(1,2)` (30 versus 20). A separate
bounded-domain check over `x in [-4,4]`, `y in [-6,6]` reports the first
candidate disagreement at `(-4,0)`. These counterexamples leave the frozen
training table, held-out observations, selected expression, and historical
resource-exhaustion result unchanged. Small exhaustive and ambiguous partial
controls require identical selected expression, score and status; duplicate-row
results match exhaustive validation. Separate guarded-modulo, invalid-predicate,
cancellation, evaluator-quota and work-limit controls check lazy masking and
stop-status distinctions. The opt-in mode does not
change the historical B outcome or existing selector defaults.

## Identifiability diagnostic addendum

The bounded comparison is a post-selection diagnostic; it receives only two
closed candidate expressions and inclusive input ranges. The independently
implemented family B reference confirms the selected candidate's discrepancies
at `(0,0)` and `(1,2)`. Across the declared diagnostic box
`[-4,4] x [-6,6]`, direct expression comparison finds the first discrepancy
at `(-4,0)` (reference expression 10, selected expression 30). The original
eight training rows and six held-out rows remain unchanged; the selected rule
still has 8/8 training and 6/6 held-out agreement. An independent reference
function assessment over all 117 diagnostic inputs finds 33 ordinary value
mismatches and zero mathematical invalidities. These are finite-data fit
results, not structural or universal rule recovery.

The comparison stops at its first difference after 7 box points and 14
candidate evaluations: 134 node visits, 60 operations, zero iterations, and a
1,048,760-byte comparer logical-memory estimate. One local Release run measured
0.016 ms for this pair comparison. Independent reference assessment evaluated
117 selected-candidate inputs (1,467 node visits and 675 operations), taking
0.130 ms in one local Release run. The comparison defaults cap 100,000 box
points, 200,000 candidate evaluations, 10,000,000 cumulative node visits,
10,000,000 operations, and 2 MiB estimated comparer storage; evaluator limits
also apply. These timings are machine-specific observations, not a speed claim.

Independent two-variable controls also exercise ambiguity and partial fit.
The predicates `x0 % 2 == 0` and `x1 % 3 == 0`, each mapped to 10 or 20,
both fit the ambiguous rows `[(-2,-3)->10, (1,1)->20, (-1,2)->20]`, while the
diagnostic finds disagreements elsewhere in `[-2,2] x [-3,3]`. On a separate
three-row table from `x0 < 0 ? 10 : (EuclideanMod(x1,3)==0 ? 20 : 30)`, each
candidate matches one row and misses two. This does not rank either as the
intended general rule. Candidate comparison and reference assessment occur
after expression construction; no independent reference labels or function are
provided to conditional synthesis.
