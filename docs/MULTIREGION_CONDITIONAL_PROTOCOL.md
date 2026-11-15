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
