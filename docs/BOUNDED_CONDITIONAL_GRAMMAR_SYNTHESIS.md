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
remain bounded by the existing grammar limits. `maximumConditionalNesting` defaults
to zero, preserving the single output-selector grammar. A positive value opts into
bounded Int64 `IfThenElse` productions inside generated output branches. Those nodes
reuse the declared predicate pool, checked lazy evaluator, deterministic node-size and
grammar ordering, and shared construction, pool, candidate, evaluation-work, and
logical-memory limits. No new operator identity is added. First-search and
iteration/binder productions remain excluded. Generated expressions
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

## Nested conditional qualification

Historical S17 recurrence synthesis deliberately restricted candidates to one root
conditional and reported no qualifying conditional recovery after its corrected
gate. S18's constraint-guided recurrence synthesis reduced compositions by matching
components against observed recurrence constraints, but remained research-only and
does not provide this finite-table function synthesizer with bounded nested Int64
branches. The current extension reuses the production typed expression pool and
selector; it does not revive either research runner or add their execution framework.

The frozen multi-region controls, rules, training and held-out values, grammars, and
resource-failure records are documented in
[MULTIREGION_CONDITIONAL_PROTOCOL.md](MULTIREGION_CONDITIONAL_PROTOCOL.md). Under
identical one-selector versus nested grammar bounds, deterministic C++ qualification
recovers the sign/modular three-region family and a guarded `INT64_MIN` overflow family
exactly on training and held-out rows. The atomic baselines remain partial. A separate
multivariate rule combining independent modular predicates is included, but under the
frozen one-million selector-tuple cap its 34-predicate, 516-branch pool implies
9,052,704 combinations and returns `ResourceExhausted` before selection. Another
guarded modulo grammar exhausts the frozen aggregate-work allowance during pool trace
reduction. These results identify a useful narrow extension and a significant
combinatorial limit; they do not establish broad synthesis efficiency or equivalence
beyond the finite observations.
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

## Bottom-up training-trace cache profile

The cache optimization was measured on the same GCC 13.3 Release host and fixed
controls, with grammar, observations, and limits unchanged. It constructs typed
per-row traces from exact child traces and the existing checked arithmetic kernel.
Construction, node visits, operator work, lookups, reuse, and root signature row
copies are charged. `std::set<BehaviorSignature>` ordering comparisons remain
uncharged, as in the baseline; their work is bounded by existing candidate and
retained-pool limits.

| Control | Metric | PR #42 baseline | Cached traces |
| --- | --- | ---: | ---: |
| Conjunction | Charged evaluation work | 5,208,156 | 2,858,764 |
| | Generated / construction attempts | 40,449 / 20,625 | 40,449 / 20,625 |
| | Retained predicate + branch pools | 278 + 4 | 278 + 4 |
| | Candidate combinations | 4,448 | 4,448 |
| | Root training trace rows | 554,792 | 554,792 |
| | Cache node visits / operations | — | 555,268 / 407,772 |
| | Cache lookups / construction work / reuse hits | — | 191,721 / 575,099 / 171,890 |
| | Direct evaluator visits / operations | — | 402,684 / 170,560 |
| | Estimated peak logical bytes | 81,760,942 | 107,382,602 |
| Disjunction | Charged evaluation work | 8,415,650 | 4,776,342 |
| | Generated / construction attempts | 40,449 / 20,625 | 40,449 / 20,625 |
| | Retained predicate + branch pools | 668 + 4 | 668 + 4 |
| | Candidate combinations | 10,688 | 10,688 |
| | Root training trace rows | 693,490 | 693,490 |
| | Cache node visits / operations | — | 694,085 / 501,235 |
| | Cache lookups / construction work / reuse hits | — | 191,721 / 713,916 / 171,890 |
| | Direct evaluator visits / operations | — | 1,379,881 / 600,579 |
| | Estimated peak logical bytes | 83,549,146 | 113,751,767 |

Charged work fell by 45.1% and 43.3%. Estimated logical memory rose by 31.3% and
36.1%, within the existing 256 MiB cap. These are conservative storage estimates,
not allocator measurements or RSS. Across five runs of the same two-control executable,
median measured synthesis time fell from 0.430 s to 0.305 s. Peak process RSS measured
from `/proc/<pid>/status` rose from 28,432 KiB to 54,188 KiB. Thus the work reduction
also produced a runtime improvement on this host, at a memory cost. Invalidity remains
per row, and lazy conditionals use only the selected branch's value and work. The same
rules and exact training and held-out outcomes were recovered.

## Finite-specification branch-pair pruning

The optional `enableSpecificationGuidedPruning` grammar flag derives branch
validity and match masks only from the supplied training observations. For a
branch pair, its optimistic match bound is the number of rows where either arm
is valid and matches. A pair is infeasible if both arms are invalid on any row.
Once a fully valid incumbent exists, the selector skips a structurally feasible
tuple only when this bound is strictly below the incumbent's match count. Equal
bounds remain evaluated so node-count and lexicographic tuple tie-breaking are
unchanged. Invalid unselected arms remain eligible whenever the other arm can
be selected. Survivors are checked with the existing evaluator. Pruning is
bypassed when the per-expression quotas or remaining aggregate budget cannot
admit a conservative full candidate evaluation, and candidate peak memory is
admitted before pruning.
Training behavior is finite-domain evidence and does not claim universal
equivalence.

The theoretical combination preflight cap is unchanged. Mask construction,
branch-pair row scans, pair checks, tuple bound comparisons, and retained trace
copies are charged to the same cumulative evaluation-work budget. Mask storage,
pair tables, and trace copies are included in the conservative logical peak
estimate. Cancellation and work exhaustion during preprocessing terminate with
their distinct statuses. This opt-in changes which tuples consume evaluator
work; the default path remains unchanged.

On the PR #43 Release controls (same grammar, rows, held-out observations, and
limits), selected expressions, scores, statuses, and held-out matches were
identical to the unoptimized selector. The exact-training controls retain their
established expressions, and their matched atomic-predicate baselines still
fail to recover the composed rules.

| Control | Baseline work | Pruned work | Reduction | Candidate tuples pruned | Evaluated tuples retained | Estimated logical peak |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Conjunction | 2,858,764 | 2,372,906 | 17.0% | 3,840 / 4,448 | 608 | 107,385,522 B |
| Disjunction | 4,776,342 | 3,110,215 | 34.9% | 9,082 / 10,688 | 1,606 | 113,754,995 B |

The conjunction improvement is below the 20% engineering target; both controls
show net savings after all new work is charged. Five-run median constructor time
was measured separately: conjunction 134.9 ms to 98.6 ms, disjunction 161.6 ms
to 114.9 ms. The focused paired-test process peaked at 56,140 KiB RSS; this
process-level RSS includes both baseline and pruned runs and is not a per-mode
allocation measurement. Logical estimates are conservative storage accounting,
not actual allocation totals or RSS.

Direct composition controls compare complete selected expressions and outcomes
on negative modulo, arithmetic branches, ambiguous and partial tables, masked
invalid branches, and fully invalid branch pairs. Tight work and cancellation
controls preserve distinct resource and stop statuses. Deterministic C++ tests
are the qualification basis; no scientific campaign or target-specific
constraints were used.

## Partition-directed conditional composition

`enablePartitionDirectedComposition` is a separate opt-in selector path. It indexes
branch candidates by exact node count and depth, scores each branch only on rows
selected by the current predicate, and joins the best regional representative for
each feasible structural-class pair. This preserves the exhaustive selector's
training score, node-count preference, and original tuple tie-break whenever the
directed search completes. Branch invalidity is evaluated only on its selected
partition, so lazy unselected failures remain valid. `maximumPartitionDirectedJoins`
is an independent hard cap; the historical `maximumCandidateCombinations` retains
its original Cartesian-tuple meaning and its original B-control exhaustion result.

The opt-in work budget charges traces, partition scans, class indexing and joins,
candidate construction, and final-winner evaluation. It does not emulate evaluator
calls for Cartesian tuples that were never materialized, so resource stop statuses
can differ from exhaustive tuple traversal. The result is rechecked using the
production evaluator and its combined limits. On the frozen multivariate modular
control, 153 class joins recover the training and held-out table under fixed limits;
the separate protocol records full work, memory, runtime, RSS, and partial/ambiguous
oracle comparisons. This finite-table outcome is not a universal-equivalence claim.

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
