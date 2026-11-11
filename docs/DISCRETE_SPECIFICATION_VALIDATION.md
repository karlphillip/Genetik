# Exact finite specifications for `DiscreteExpression`

`DiscreteExpressionSpecification.hpp` provides an optional, deterministic
validator for a candidate `DiscreteExpression` against an immutable list of
exact observations. It does not generate candidates, call a solver, or alter
the genetic algorithm.

```cpp
#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionSpecification.hpp>

#include <iostream>
#include <vector>

int main() {
    std::vector<DiscreteSpecificationObservation> rows;
    for (const std::int64_t x : {-2, -1, 0, 1, 2})
        rows.push_back({{x}, DiscreteValue::integer(x + 2)});

    const DiscreteSpecification specification(1, std::move(rows));
    const auto candidate = parseDiscreteExpression("x0 + 2", 1);
    const auto result = validateDiscreteSpecification(specification, candidate);
    std::cout << discreteSpecificationValidationStatusName(result.status) << '\n';
    if (result.certificate)
        std::cout << result.certificate->checkedRows << " finite rows validated\n";
}
```

## Contract

The specification owns an ordered copy of its rows. Each row is a complete
Int64 input tuple and an expected `DiscreteValue`; rows must be nonempty,
dimensionally consistent, use one output type, and have unique input tuples.
The candidate must be a closed expression with matching input dimensions and
output type. Input rows are checked in their declared order, and the first
mismatch is returned with its ordinal, inputs, expected value, and actual value.

The validator delegates candidate execution to the existing
`DiscreteExpression` evaluator. Checked signed Int64 arithmetic, Euclidean
modulo with a positive divisor, lazy conditional evaluation, scoped reduction
bindings, and evaluator invalidity therefore retain their existing semantics.
Mathematically invalid candidates (including overflow, invalid modulo, or a
bounded `First` with no witness) are reported as `InvalidCandidate`. Evaluator
resource exhaustion and cancellation remain explicit incomplete outcomes.

The statuses are:

- `ValidatedFiniteDomain`: every declared row matched exactly. The certificate
  records full canonical identities for the specification and candidate, plus
  the exact declared and checked row counts.
- `CounterexampleFound`: the first declared row where exact values differ.
- `InconclusiveResourceExhausted` and `InconclusiveCancelled`: not all required
  rows were checked; neither status contains a certificate.
- `InvalidSpecification`: malformed rows or an invalid validation policy.
- `InvalidCandidate`: an open, mismatched, or mathematically invalid candidate.

Counterexample and mathematical-invalid row records carry the exact canonical
specification and postfix candidate identities, plus the failing row ordinal
and tuple. Candidate identity includes input dimension, outer binder scope,
postfix node kinds, literal/variable values and binder identities.
Specification identity includes row order, input tuples and expected typed
values. These are complete serialized identities rather than short hashes.

## Limits and accounting

`DiscreteSpecificationValidationLimits` bounds row admission, duplicate-input
checks, cumulative evaluator node visits, mathematical operations, reduction
iterations, logical retained storage, and per-row evaluator workspace. Work
counters include partial work returned by the evaluator when it stops. Limits
are applied cumulatively across rows, rather than reset for each observation.

`retainedBytesEstimate` is a deterministic logical bound based on row payload,
candidate node storage, canonical identities, the result payload, a possible
counterexample/failure tuple and a fixed 22-byte integer-formatting scratch
area. Duplicate detection runs only after row-schema and storage admission
preflight. Its work counters count input-coordinate comparisons
(zero-dimensional tuple comparisons are charged once). Allocator metadata,
capacity rounding, and process/runtime memory are excluded, so this value is
not process RSS or an operating-system memory cap. Since the caller creates
the immutable specification before validation, this admission policy cannot
retroactively limit memory already used to construct that input object.
`peakRowWorkspaceAllowanceBytes` is the maximum evaluator workspace
admitted for any one row, not a measured allocation or group-process memory
peak. The evaluator does not currently expose exact live frame allocation, so
the API reports the configured bound explicitly instead of presenting it as a
measured peak. A caller needing an operating-system memory cap must enforce
that separately.

## Meaning of a certificate

A certificate establishes exact agreement only on the finite observations
stored in that `DiscreteSpecification`. It does not establish identity on
unlisted inputs, mathematical equivalence, or a universal theorem. A finite
set may fail to distinguish different expressions; the validator makes the
first such disagreement reproducible when it lies in the declared scope.

The deterministic controls and independent references are recorded in
[`DISCRETE_SPECIFICATION_VALIDATION_PROTOCOL.md`](DISCRETE_SPECIFICATION_VALIDATION_PROTOCOL.md)
and its immutable amendments ([v2 lazy conditionals](DISCRETE_SPECIFICATION_VALIDATION_PROTOCOL_V2.md),
[v3 bool and resource controls](DISCRETE_SPECIFICATION_VALIDATION_PROTOCOL_V3.md),
[v4 modulo-domain row](DISCRETE_SPECIFICATION_VALIDATION_PROTOCOL_V4.md),
and [v5 row-count clarification](DISCRETE_SPECIFICATION_VALIDATION_PROTOCOL_V5.md)). In
particular, the conditional overfit is rejected at `x0 = -1`, and the
unselected invalid branch controls confirm that validation preserves the
evaluator's lazy `if` semantics. `validateDiscreteSpecificationWithStop`
accepts a cancellation predicate only; it does not provide a reference-value
callback.
