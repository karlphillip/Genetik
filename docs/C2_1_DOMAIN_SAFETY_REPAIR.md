# C2.1 — S14 multivariate domain-safety repair

## Scope and decision

C2.1 repairs the research-only S14 denominator assessor. It does not add a
regression method, increase polynomial degree, create a public API, or change
Genetik's genetic algorithm. The accepted model has exactly two inputs and
numerator and denominator bases drawn from the ordered total-degree-two sets
`1, x, y, x^2, xy, y^2` (or an exact prefix for a lower degree).

The assessor now validates a caller-constructed `Model` with the same rules as
a fitted one. It rejects degrees above two, basis/order/duplicate mismatches,
wrong coefficient counts, nonfinite coefficients, invalid `Q(0,0)=1`, a wrong
parameter count, and expressions that do not exactly materialize the stored
coefficients and basis. Rejection returns `Invalid` with
`invalid_or_unsupported_model`; no unsupported term is omitted or truncated.

The old long-double interval plus heuristic evaluator allowance was replaced
with binary64 intervals. Each supported monomial is evaluated in the same
structural order as its S14 expression, and every add, multiply, square, and
divide widens its result by one representable value. Nonfinite intermediate
bounds fail closed and can only lead to `Uncertain`. The assessor requires
IEC-559 binary64, an active gradual-underflow arithmetic probe, and
round-to-nearest at runtime; it refuses
fast-math/finite-only compilation at preprocessing time. Compilation is
restricted to GNU/Clang frontends, the only frontends qualified in this
milestone. Its validity argument
also relies on Genetik's strict floating-point compile contract
(`-fno-fast-math -ffp-contract=off` on supported GNU/Clang builds).
`maximumCells` and `maximumDepth` bound subdivision; `intervalOperations`
counts interval primitive applications for all four corner enclosures, the
rectangle bound, and any numerator and quotient bound.

It distinguishes:

- `Valid / SeparatedFromZero`: every remaining rectangle has an outward
  enclosure wholly beyond the configured absolute separation threshold.
- `Invalid / DemonstratedZero`: certified opposite signs at points in the
  connected rectangle prove a zero by the intermediate value theorem.
- `Uncertain / Inconclusive`: the cell, numerical enclosure, depth, or work
  bound did not establish separation or a sign-changing zero.
- `Invalid` with a reason: malformed model or domain configuration.

An exact zero that does not produce a sign change can remain uncertain. This
is intentional: rounded point evaluation equal to zero is not by itself proof
that the represented real polynomial is exactly zero. The assessor separately
reports prediction numerical bounds. Pole separation does not establish
prediction accuracy relative to an unknown target.

## Interval arithmetic assumptions and limitations

The primitive intervals enclose exact real operations on binary64 endpoints by
computing under round-to-nearest and extending one adjacent representable value
in each direction. This relies on the supported IEEE-754 basic-operation
rounding behavior and strict compilation; it does not rely on sampled points
to establish safety. Multiplication checks all four endpoint products; square
uses zero as the lower bound whenever its input spans zero. All accepted
monomials participate in the bound. Overflow, NaN, or a nonfinite interval
endpoint prevents a certificate. Subnormal results are retained; the runtime
requires gradual underflow.

This is a conservative engineering interval implementation, not a machine
checked proof of every platform's floating-point conformance. The certificate
is intentionally unavailable under unsupported floating-point environments.
Callers must use the strict Genetik floating-point build contract. No arbitrary
precision dependency was introduced. Bounded subdivision may return
`Uncertain` even for a pole-free rectangle. A finite collection of samples is
used only to demonstrate opposite signs; it is never used to certify an entire
rectangle as pole-free.

## Compatibility

The S14 QR design matrix, basis enumeration, QR solver, optional existing LM
refinement, normalization, expression materialization, model selection, and
fit resource limits were left unchanged. The existing S14 deterministic fit
contracts and representative proper, improper, linear, quadratic, and noisy
models pass. The near-pole negative control remains unresolved/uncertain under
its frozen safety region. No S13 scalar API, recurrence API, GA behavior, RNG,
checkpoint, package default, dependency, or published v0.7.0 file changed.

See `C2_1_MATHEMATICAL_VALIDATION.md` for independent reference cases and
`C2_1_QUALIFICATION.md` for test and environment results. This repair is
research-only and does not resume C2 Gate B or authorize integration.
