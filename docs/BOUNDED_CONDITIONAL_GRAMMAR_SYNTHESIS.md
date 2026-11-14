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
Boolean productions are integer comparisons. The outer lazy conditional is constructed
by the existing selector. Nested conditionals, first-search, and iteration/binder
productions are intentionally excluded from the first grammar version. Generated
expressions contain only declared input variables and literals, so they are closed
under the existing lexical-scope validator; caller components with outer bindings are
not accepted by the selector.

Enumeration order is expression node count, then declared operator order, child-size
partition order, and declared terminal order. No commutative rewriting or algebraic
canonicalization occurs. Training-behavior reduction preserves per-row mathematical
invalidity markers: a branch that overflows on an unselected row remains eligible for
the lazy conditional. Equal training traces retain only the shortest earliest
representative because substituting it cannot alter any conditional's training
behavior or improve the node-count tie break.

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
