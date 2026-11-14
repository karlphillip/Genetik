# Bounded conditional composition

`DiscreteConditionalComposition.hpp` adds an optional deterministic constructor
that selects a conditional expression from a finite caller-supplied set of Bool
predicates and Int64 branch expressions against a `DiscreteSpecification`.
Predicates are the caller's declared Boolean grammar productions and branch
expressions are independently available components. The constructor receives
only these pools and the finite training table. It has no reference-expression,
hidden-component, or assessment-data input.

It enumerates predicate/true-branch/false-branch triples in stable input order,
uses `DiscreteExpression::conditional` and the existing lazy checked evaluator,
and selects by training matches, then node count, then pool indices. It reports
`ExactTraining`, `Partial`, `Invalid`, `ResourceExhausted`, or `Cancelled`.
Resource limits bound total combinations, complete expression node count/depth,
cumulative evaluator node visits plus operations, and a conservative logical
memory estimate. Memory figures are estimates, not process RSS caps. The caller
owns and has already allocated the immutable specification and component pools.
Candidate results never imply equivalence outside the listed rows.

Deterministic contracts construct arithmetic and modular piecewise controls from
small expression pools, evaluate the returned trees on the finite rows, and
include a pool-mismatch negative control, lazy unselected overflow, all-invalid
overflow, combination/work/structure limits, and cancellation. These are
correctness examples, not stochastic recovery evidence or a search-performance
claim.

## Architectural review

Discrete variation already supports checked typed operators; reusable discrete
components can assemble conditionals; exact specification validation checks
finite tables; and one-site/multi-component expression templates handle known
scalar regression scaffolds. S17/S18 research explored conditional recurrence
enumeration and constraint-guided partitioning. S17 did not qualify conditional
discovery: its corrected criterion found 0/4 qualifying conditional rules and
one task exhausted its training-point limit. S18 found bounded candidates in a
small recurrence grammar, but remained research-only and its independent
information conditions did not provide a clean attribution to decomposition.

This API therefore does not add a new expression language, grammar synthesizer,
GA path, or recurrence framework. It fills the smaller production gap of
deterministically selecting a lazy conditional over already available typed
components under direct construction/evaluation budgets.
