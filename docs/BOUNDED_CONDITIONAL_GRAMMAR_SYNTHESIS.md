# Bounded conditional grammar synthesis

`DiscreteConditionalGrammarSynthesis.hpp` adds an optional deterministic constructor for
the conditional selector in `DiscreteConditionalComposition.hpp`. It enumerates typed
Int64 branch and Bool predicate expressions from caller-declared finite typed
productions and separate branch/predicate terminal sets,
reduces expressions with identical behavior on the supplied training rows to their
first shortest grammar-order representative, then composes
`if(predicate, branch_true, branch_false)` candidates through the existing checked
selector. The grammar and training specification are the only constructor inputs.
There is no target-expression, hidden-component, assessment-data, RNG, or GA input.

Integer productions are Add, Subtract, Multiply, Negate, and Euclidean Modulo;
atomic Boolean productions are integer comparisons. `maximumBooleanNesting` defaults
to zero, preserving the atomic grammar and its ordering. Positive nesting together
with individually enabled `allowBooleanAnd`, `allowBooleanOr`, or `allowBooleanNot`
opts into bounded Boolean composition using the existing lazy `IfThenElse` node: AND is
`if(p,q,false)`, OR is `if(p,true,q)`, and NOT is `if(p,false,true)`. No public AND,
OR, or NOT operator identities were added. `maximumPredicateIntegerNodes` optionally
caps each integer operand tree of a comparison; zero inherits the overall predicate
node limit. Boolean nesting is capped at 16, while expanded predicate nodes and depth
remain bounded by the existing grammar limits. Nested integer conditionals,
first-search, and iteration/binder productions remain excluded. Generated expressions
contain only declared input variables and literals, so they are closed under the
existing lexical-scope validator; caller components with outer bindings are not
accepted by the selector.

Enumeration order is expression node count, then declared operator order, child-size
partition order, and declared terminal order. No commutative rewriting or algebraic
canonicalization occurs. When enabled, the additional Boolean templates follow the
declared productions in AND, OR, NOT order. Training-behavior reduction preserves
per-row mathematical invalidity markers: a branch that overflows on an unselected
row remains eligible for the lazy conditional. Reduction retains the shortest
grammar-order representative whose component depth fits beneath the selector's outer
conditional. If a shorter trace-equivalent expression is too deep, a larger shallower
candidate can remain eligible. This preserves candidate feasibility while keeping
the established shortest-representative choice among candidates that can be selected.
Integer memo pools share cache entries across Boolean nesting allowances because that
allowance does not affect integer productions.

`DiscreteConditionalGrammarLimits` bounds retained/generated expression instances,
construction attempts, predicate/branch combinations, specification schema scans,
cumulative grammar-pool and selector evaluation work, estimated peak logical bytes,
and evaluator work. The
expression grammar separately caps predicate nodes, branch nodes, and expression
depth. Cancellation is checked during enumeration, training-trace reduction, and
selection. Memory accounting is a conservative logical estimate, not an operating
system RSS or wall-clock cap.

Results distinguish exact finite-training agreement, partial agreement, invalid
grammar/candidates, resource exhaustion, and cancellation. `ExactTraining` means only
that the selected expression matches every supplied training row. It does not prove
uniqueness, generalization, or mathematical equivalence beyond those rows. Any held-out
assessment belongs in caller code after selection and must not be supplied to this
constructor.

Contracts construct two rules from grammar productions rather than complete
predicate/branch expressions: a periodic residue-controlled piecewise rule and an
arithmetic comparison-driven piecewise rule. They separately evaluate held-out
points, modular boundaries, a missing-production negative control, checked overflow,
repeated deterministic ordering, and pool, attempt, combination, work, memory, and
cancellation limits. This qualifies a bounded deterministic capability only; it makes
no stochastic search-performance or universal-recovery claim.

## Boolean composition qualification

The deterministic C++ contract adds two non-prime two-input controls. On the first
training grid, the selected rule agrees exactly with a piecewise output controlled by
`x mod 3 = 0 AND y < 0`; on the second it agrees exactly with `x < 0 OR y mod 2 = 0`.
Each training target is specified independently as a table-producing function. The
constructor receives only the table and declared grammar. Eight separate held-out
observations per family, including negative values and modulo boundaries, are checked
after selection. Both selected expressions contain a Boolean conditional below the
outer output selector.

The matched atomic-predicate baselines use identical integer productions, constants,
branch pools, operand-node bound (3), expanded predicate-node bound (10), and depth
bound (4), with Boolean nesting set to zero. They match 24/28 and 27/35 rows and do
not recover either complete training grid. The composed runs match all rows. Their
respective generated expression counts are 40,469, construction attempts 20,641,
retained predicate pools 278 and 668, selector combinations 4,448 and 10,688,
cumulative evaluation work 5,208,156 and 8,415,650, and peak logical-memory estimates
186,747,366 and 190,616,040 bytes. The tests cap estimated memory at 256 MiB and
evaluation work at 50 million. These estimates reflect the current exhaustive finite
enumerator; callers need to raise the default 64 MiB estimate for similarly broad
grammars. The work limits bound completion but do not make exhaustive enumeration
cheap.

The same contracts cover lazy Boolean short-circuiting with an invalid unselected
predicate branch, invalid modular candidates, negative inputs, modulo boundaries,
stable tie-breaking, malformed Boolean nesting, cancellation and resource exhaustion.
Training-trace reduction keeps a validity marker at each invalid row. Equal finite
training behavior remains only a finite-data reduction and is not treated as universal
mathematical equivalence.

An independent architectural review found no blocking issue and specifically asked
for direct NOT-production coverage; the contract enumerates and checks the generated
`if(x0 < 0, false, true)` predicate in addition to the end-to-end AND/OR recoveries.

## Memory optimization profile

The two fixed controls were profiled in one deterministic C++ contract executable
using GCC 13.3 Release on the same host. The baseline is PR #41 at `6cccf1d`; after
uses this optimization. Training and held-out observations, grammar declarations, and
all limits are unchanged. “Generated” is the public work counter: before it included
copies used to collect memoized size pools; after it counts logical output entries
while roots are moved out of the memo. “Trace evaluations” counts candidate-row
evaluations during behavior reduction.

| Control | Metric | PR #41 baseline | After |
| --- | --- | ---: | ---: |
| `x mod 3 = 0 AND y < 0` | Generated expressions | 40,469 | 40,449 |
| | Construction attempts | 20,641 | 20,625 |
| | Pre-reduction predicate + branch candidates | 19,810 + 4 | 19,810 + 4 |
| | Retained predicate + branch pools | 278 + 4 | 278 + 4 |
| | Candidate combinations | 4,448 | 4,448 |
| | Training trace evaluations | 554,792 | 554,792 |
| | Cumulative evaluation work | 5,208,156 | 5,208,156 |
| | Estimated peak logical bytes | 186,747,366 | 81,760,942 |
| `x < 0 OR y mod 2 = 0` | Generated expressions | 40,469 | 40,449 |
| | Construction attempts | 20,641 | 20,625 |
| | Pre-reduction predicate + branch candidates | 19,810 + 4 | 19,810 + 4 |
| | Retained predicate + branch pools | 668 + 4 | 668 + 4 |
| | Candidate combinations | 10,688 | 10,688 |
| | Training trace evaluations | 693,490 | 693,490 |
| | Cumulative evaluation work | 8,415,650 | 8,415,650 |
| | Estimated peak logical bytes | 190,616,040 | 83,549,146 |

Estimated peak bytes fell by 56.2% and 56.2%. The generator preserves the generated
expression counter and cap, accounts retained expressions once plus transient
construction separately, moves roots rather than deep-copying their node storage,
uses compact typed signatures instead of formatted integer strings, and shares
integer pools across Boolean nesting states. These
changes do not reduce trace evaluation work or selector combinations. Observed peak
RSS fell from 319,108 KiB to 27,716 KiB for the complete contract executable;
aggregate allocation volume was not instrumented. Total executable wall time was
0.717 s before and 0.713 s after (about 0.5%, within run-to-run noise), so no
computational speedup is claimed. Training and held-out matches and selected rules
were unchanged.

The trace reducer records validity separately from typed values, so an invalid row
cannot collapse into a valid Boolean false or an integer value. Negative controls
check that distinction, depth-feasibility retention, and lazy skipping of an
invalid unselected arm. The logical estimate remains a conservative storage estimate,
not RSS or allocator traffic.

## Architectural basis

`DiscreteVariation::generate` is a randomized, GA-coupled generator;
`DiscreteExpressionComponents` composes caller-provided components;
`validateDiscreteSpecification` checks expressions already constructed. The S16
constructive recurrence enumerator exhausts a narrow integer recurrence grammar and
explicitly excludes conditionals. S17's corrected conditional qualification found
zero of four rules and one task exhausted its training-point budget; S18's bounded
constraint-guided recurrence work stayed recurrence-specific. The production gap was
therefore typed deterministic pool generation, not another conditional selector,
recurrence framework, or stochastic operator. See
[`BOUNDED_CONDITIONAL_COMPOSITION.md`](BOUNDED_CONDITIONAL_COMPOSITION.md) and
[`CONSTRUCTIVE_RECURRENCE_SYNTHESIS.md`](CONSTRUCTIVE_RECURRENCE_SYNTHESIS.md).
