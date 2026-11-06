# C2.1 — independent mathematical validation

## Historical false certificate

At unmodified S14 source SHA
`bd26be819559e9338f0c57939bc0d6dd39155d88`, a caller-built model with
`Q(x,y)=1-3x^3` on `[0,1] x [0,1]` returned
`Valid / SeparatedFromZero / denominator_interval_separated_from_zero`.
Independent substitution gives `Q(0,y)=1`, `Q(0.5,y)=0.625`, and `Q(1,y)=-2`.
Continuity proves an interior zero. The exact source and stdout are preserved
in `Genetik-data/benchmarks/c2-1-multivariate-domain-safety/original/`.

The root cause was that `monomialRange()` understood powers zero, one, and two
but did not validate `Term` values before assessment. The caller-constructible
model accepted `(3,0)`, while the bound silently treated it as the constant
monomial `1`. Model/expression matching did not constrain terms to the declared
degree-two family.

## Contract checks

The repaired validation requires both degrees in `[0,2]`, exact deterministic
basis order from `basis(degree)`, coefficient lengths equal to basis lengths,
finite coefficients, a leading denominator constant exactly one, a matching
free parameter count, two-input materialized expressions, and exact node-level
agreement between stored expressions and rebuilt expressions. Therefore
duplicates, unsupported terms, stale expressions, altered coefficients,
incorrect bindings, and malformed normalization are invalid before interval
work begins.

## Independent reference cases

`tests/s14_domain_safety.cpp` specifies coefficients independently of the
interval implementation:

| Reference denominator | Rectangle | Expected mathematical result | Observed result |
|---|---|---|---|
| `1 - 2x` | `[0,1] x [0,1]` | sign change; interior zero | `Invalid / DemonstratedZero` |
| `1 - 2x^2` | `[0,1] x [0,1]` | sign change; interior zero | `Invalid / DemonstratedZero` |
| `1 + x^2 + y^2` | `[0,1] x [0,1]` | denominator at least 1 | `Valid`, certified lower bound above 0.99 |
| `1 + xy` | `[0,1] x [0,1]` | denominator at least 1 | `Valid / SeparatedFromZero` |
| `1 + xy` | `[-2,-1] x [1,2]` | zero is possible | not `Valid` |
| `1 - 3x^3` | `[0,1] x [0,1]` | unsupported basis, root exists | `Invalid / invalid_or_unsupported_model` |

Additional checks cover each supported monomial, negative coefficients,
rectangles crossing and not crossing zero, `x^2`/`y^2` on positive and negative
rectangles, duplicate basis terms, coefficient count, nonfinite coefficient,
normalization, stale numerator expression, denominator variable binding,
non-nearest rounding mode, and bounded inconclusive work. Independent endpoint
arithmetic in the test establishes the cubic values; tests do not use the
interval helper as their oracle.

Opposite signs are certified only when outward point intervals are strictly on
opposite sides of zero. The intermediate value theorem then proves a zero on
the connected rectangle. If a root merely touches zero without a proven sign
change, the result may remain uncertain; this is a safe loss of classification,
not a false pole-free certificate.

## Interval audit

The supported expression order is preserved: powers are computed before the
coefficient product, `xy` multiplies the x contribution and then y, and
polynomial terms are added in stored basis order. Every binary64 basic
operation is enclosed by adjacent representable values in both directions.
For square, a rectangle spanning zero has lower bound zero. For product, all
four endpoint combinations are bounded. Negative and signed-zero coefficients
are represented as signed point intervals and pass through the same
multiplication enclosure; signed zero can only widen the result conservatively.
Nonfinite intermediate products or sums mark that enclosure invalid; the
assessment subdivides within its configured cell/depth allowance and otherwise
returns `Uncertain`. It never turns overflow or cancellation ambiguity into a
safe result.

The runtime rejects non-nearest rounding, non-IEC-559/non-binary doubles,
platforms without gradual underflow, and active FTZ/DAZ behavior detected by a
volatile subnormal-times-one probe. Fast-math and finite-only compilation is a
compile-time error. Unqualified compiler frontends are also rejected at
compile time. This relies on the documented strict floating-point
compiler contract; exact arithmetic across arbitrary nonconforming compilers
is not claimed. A `Valid` domain result certifies denominator separation for
the represented polynomial under that contract; it does not certify fit
accuracy, noisy parameter identifiability, or extrapolation accuracy.

The result's `intervalOperations` counter charges each interval primitive
application used for the four corner checks, the rectangle denominator bound,
and any numerator and quotient bound. `maximumCells` bounds the subdivision
worklist; exhausted work returns `Uncertain`.
