# Whole-box polynomial value and derivative assessment

`PolynomialBounds.hpp` provides `assessPolynomialBounds` for candidate
assessment over finite closed boxes. The supported grammar is variables,
finite constants, addition, subtraction, and multiplication. Division,
transcendentals, and custom operators return `Inconclusive` with an unsupported
reason. The box dimension is limited to 16. Invalid boxes or requests also
return `Inconclusive`; they are not interpreted as a mathematical violation.

The evaluator propagates outward-rounded binary64 intervals. Each arithmetic
endpoint is expanded with `nextafter`; multiplication checks all four endpoint
products, and indeterminate overflow widens to the entire real line. When an
enclosure does not yet fit the requested range, the largest finite input
interval is bisected. At most `maximumSubdivisions` splits are made. A
`Certified` result means every remaining box enclosure lies inside each
requested bound. A coarse enclosure crossing a bound can only cause
`Inconclusive`.

An actual violation is reported only when the outward enclosure at a concrete
midpoint is entirely beyond a requested bound. The result includes that input
point and the point enclosure; ordinary midpoint samples do not turn an
overlapping enclosure into a violation. This yields three distinct outcomes:
`Certified`, `Violated` with a verified witness, and `Inconclusive`.
`MathematicalConstraints::assessWholeBox` exposes these outcomes for a model
candidate as a non-sampled constraint report and retains the complete certificate.

The optional derivative request names one input coordinate and an inclusive
derivative interval. Derivative intervals are propagated with the product rule
alongside primal intervals, and a midpoint witness is cross-checked with the
existing checked `evaluatePartial` path. This first version certifies ranges;
for example, a derivative lower bound of zero certifies nondecreasing behavior.
It does not claim strict monotonicity unless the requested derivative lower
bound is strictly positive.

Two deterministic assessment fixtures expose endpoint sampling failure:
`x^3 - x/4` has positive derivatives at both endpoints of `[-1,1]` but a
verified derivative witness `-1/4` at zero. The control `x + x^3` is certified
inside value range `[-2.1,2.1]` and derivative range `[0.5,5]` on the same box.
The tests also confirm that a logarithmic expression remains inconclusive.

All bounds are for the real polynomial represented by the finite binary64
coefficients and closed box endpoints. The arithmetic proof relies on Genetik's
strict floating-point build policy and native binary64 behavior. Subdivision is
bounded and may stop inconclusive; it never weakens the requested property.
