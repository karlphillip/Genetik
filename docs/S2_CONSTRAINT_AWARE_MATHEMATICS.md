# S2 — Constraint-aware symbolic mathematics

## Implementation

S2 adds an optional exact-rational layer in `genetik/BoundedRational.hpp` and an optional quantized-constraint API in `genetik/QuantizedConstraints.hpp`. Both require Boost.Multiprecision headers and are excluded from ordinary Genetik headers, targets, and installed dependency requirements. `BoundedBigInteger.hpp` remains the arithmetic backend. The exact-rational tests are enabled explicitly with `GENETIK_BUILD_EXACT_RATIONAL_TESTS=ON`.

`BoundedRational` stores a reduced numerator and positive denominator, captures finite binary64 values as their exact dyadic fractions, and implements exact arithmetic, comparison, floor, ceiling, and canonical `n/d` serialization. The configured BigInt bit, storage, intermediate, and work limits gate normalization and arithmetic before the expensive operations. Limit violations are surfaced as `length_error`; division by zero and malformed values remain mathematical/input errors. Exact rational values are separate from Genetik's executable Real64 values.

`QuantizedConstraints.hpp` exposes exact open/closed interval intersection and constraints for a single parameter in `round(c*alpha+d)==y`. Floor, ceiling, truncation toward zero, and nearest with halfway-away-from-zero use distinct integer cells. `constrainAffineObservations` intersects multiple observations for a fixed affine structure. Negative and zero coefficients are handled exactly. A contradiction is reported as `ProvenInfeasible` only after exact interval intersection.

Repeated parameters in a difference of floors are represented by one alpha in both affine forms. `constrainFloorDifference` enumerates possible values of the second floor only within a caller-supplied bounded parameter interval and a branch cap. It returns a union of exact intervals. Unbounded domains are `Unsupported`; an exceeded branch/work cap is `ResourceExhausted`; cancellation is `Unknown`. This is a bounded one-parameter fragment, not a general symbolic solver for nested quantizers.

`selectRationalWitness` chooses a deterministic exact midpoint for a bounded feasible interval. `selectAndVerifyBinary64` then searches adjacent binary64 values from that midpoint, checks exact interval membership of each dyadic value, and evaluates every supplied integer observation. It distinguishes a verified executable coefficient, a proved absence of binary64 values between adjacent floats, executable mismatch, and an inconclusive search-limit outcome. A feasible rational coefficient is not automatically an executable coefficient.

The D5 symbolic DAG remains distinct from executable postfix expressions. `simplify` now recognizes the bounded pattern `(powi(x,2)-1)/(x-1)`, returns `x+1`, and attaches a `NonZero` assumption for the original denominator. It does not rewrite the executable tree and does not claim bit-exact floating-point equivalence. Existing polynomial expansion/factorization, exact rational64 coefficients, and assumption provenance remain unchanged.

## Public interfaces and limits

- `BoundedRational`, `fromInt64`, `fromBinary64`, `parse`, `floor`, `ceil`, and exact operators.
- `RationalInterval`, `quantizedCell`, `constrainAffine`, and `constrainAffineObservations`.
- `constrainFloorDifference` and `constrainFloorDifferenceObservations` for bounded shared-parameter branches.
- `selectRationalWitness`, `verifyBinary64Witness`, and `selectAndVerifyBinary64`.

Constraint statuses are `Feasible`, `ProvenInfeasible`, `Unsupported`, `ResourceExhausted`, and `Unknown`. Constraint solving is an independent fixed-structure operation. It does not change GA selection, variation, coordinate fitting, S1 LM, fitness, or checkpoints. Exact parameter/interval state can be serialized canonically and reconstructed deterministically; it does not participate in a run checkpoint. Existing mixed-expression candidate checkpoints continue to serialize shared parameter identities and binary64 bits unchanged.

The rational arithmetic work limit uses a conservative bit/limb estimate; `ExactConstraintResult::work` accumulates a conservative estimate for affine operations and shared-floor branches. It is not a platform-specific count of Boost limbs. No standard-library transcendental is promoted to an exact operation or certified error bound.

See [qualification](S2_QUALIFICATION.md), [results](S2_RESULTS.md), and the checksummed records in `s2-results/`.
