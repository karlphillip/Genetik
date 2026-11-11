# Discrete specification validation: frozen controls

**Protocol version:** 1  
**Purpose:** deterministic correctness qualification for a bounded finite-domain
validator over `DiscreteExpression`. This is not a discovery campaign and makes
no use of a solver or stochastic search.

## Frozen hypothesis and scope

The hypothesis is that an opt-in validator can exhaustively compare a closed
typed `DiscreteExpression` with an immutable table of exact expected values on
an explicitly listed finite set of integer input rows, while preserving the
existing evaluator's checked arithmetic, Euclidean modulo, lazy conditionals,
scoped iteration, cancellation, and work limits. It must either inspect every
declared row or return an explicit incomplete/invalid result. It makes no claim
of equivalence outside the listed rows.

The finite domain is an ordered list of input tuples. Duplicate tuples are
malformed. Each tuple has one expected `Int64` or `Bool` value; the declared
output type must be consistent across all rows. No callback oracle, generator,
or implicit interval expansion is part of this protocol. Inputs and expected
values below are independent mathematical references, never candidate
generation hints.

## Frozen deterministic controls

| ID | Specification / candidate | Frozen expected outcome |
|---|---|---|
| `integer-identity` | `x0 + 2` on `x0 ∈ {-2,-1,0,1,2}`; expected `x0+2` | Validated over all 5 rows, exact Int64 values |
| `euclidean-modulo` | `x0 % 3` on `x0 ∈ {-5,…,5}`; expected Euclidean remainder in `[0,2]` | Validated over all 11 rows; in particular `-7 mod 3 = 2` is separately checked in the independent oracle fixture |
| `conditional-branches` | `if(x0 < 0, -x0, x0)` on `x0 ∈ {-2,-1,0,1,2}`; expected absolute value | Validated; both branches and the zero boundary are exercised |
| `historical-conditional-overfit` | Candidate `if(x0 <= -2, -x0, x0)` and reference `abs(x0)` on domain `{-2,-1,0,1,2}` | `CounterexampleFound` at the first differing declared row `x0=-1`, expected `1`, candidate `-1`; rows are visited in the listed order. This is the single historical counterexample disclosed in the solver-guided report, not the historical sealed assessment dataset |
| `checked-overflow` | Closed expression `INT64_MAX + 1`, zero-dimensional input row `{}` | `InvalidCandidate` with mathematical-invalid evaluator status; no successful certificate |
| `zero-divisor` | `x0 % 0` on `x0={-1,0,1}` | `InvalidCandidate` on the first row |
| `negative-divisor` | `x0 % -2` on `x0={-1,0,1}` | `InvalidCandidate` on the first row |
| `empty-sum` | `sum(k0, 1, 0, k0)` on one zero-dimensional row | Valid and equals zero under the existing inclusive empty-range semantics |
| `negative-sum` | `sum(k0, -2, 0, k0)` on one zero-dimensional row | Valid and equals `-3` |
| `nested-sum` | `sum(k0, 0, 1, sum(k1, 0, k0, 1))` on one zero-dimensional row | Valid and equals `3`; verifies nested scope and inclusive endpoints |
| `resource-stop` | Valid expression on multiple declared rows with a cumulative node/operation allowance below the complete required work | Explicit resource-incomplete result; never `ValidatedFiniteDomain` |
| `cancel-stop` | Same deterministic valid fixture, cancellation requested before or during enumeration | Explicit cancelled-incomplete result; never a certificate |
| `training-subset-mismatch` | Candidate `if(x0 <= -2, -x0, x0)` agrees with the absolute-value reference on training subset `{-2,0,1,2}` but is checked on the complete domain including `-1` | Counterexample at `-1`; proves the validator checks the declared domain rather than a training subset |

## Independent oracle and comparison

Expected values for ordinary arithmetic, comparisons, conditional branches,
modulo, and the three bounded-sum examples are computed from their mathematical
definitions in test code that does not call `DiscreteExpression::evaluate`.
For sums, the oracle uses a separate small inclusive loop over the frozen
bounds. Overflow and invalid-modulo controls use the evaluator status contract
as the expected invalid behavior, not a fabricated numeric value.

Tests compare exact outputs and statuses, then compare validator work totals to
the sum of independently observed evaluator counters for completed rows.
Counterexamples are checked against the first deterministic mismatch, including
ordinal, tuple, expected value, and actual value. Replay must reproduce status,
certificate scope identities, counterexample, row count, and counters.

## Limits and acceptance

Correctness tests use explicit small row sets (at most 11 rows) and the
existing bounded expression evaluator. Resource/cancellation tests use tiny
validator allowances chosen to stop strictly before the full domain has been
checked. No timing-based assertion, external process, random seed, or large
scientific budget is used.

Acceptance requires deterministic replay, exact agreement with the independent
oracle on every valid control, the `-1` historical counterexample, no false
finite-domain certificates, incomplete outcomes on every early stop, and
auditable row/node/operation/iteration/workspace accounting. Any unsupported
candidate or evaluator status must remain explicit. No assessment data beyond
the named historical point may influence design or tests.
