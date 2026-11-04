# S14 — Constructive Multivariate Rational Discovery

## Scope

S14 is a research-only implementation for two independent inputs and rational models
`P(x,y)/Q(x,y)`. It does not change the installed one-variable API, symbolic-regression
defaults, GA, expression evaluator, or checkpoint behavior.

Both polynomials use total-degree bases up to degree two, ordered by increasing total
degree and then decreasing x exponent:
`1, x, y, x^2, xy, y^2`. Denominator normalization fixes `Q(0,0)=1`; a full
quadratic-over-quadratic fit therefore has six numerator and five free denominator
coefficients (11 total). Degree combinations are bounded and selected using the
existing training-only BIC-style criterion. Redundant factors and non-identifiable
parameterizations may produce multiple coefficient representations.

The normalization excludes rational functions whose denominator has zero constant
term in the supplied coordinates, unless an equivalent representation can be obtained
by cancelling common factors and renormalizing. No such equivalence is presumed by the
implementation. The scope is not unrestricted rational regression.

## Reconstruction

For each training row `(x_i,y_i,z_i)`, S14 builds the linearized system
`P(x_i,y_i) - z_i (Q(x_i,y_i)-1) ~= z_i`. It uses Genetik's existing column-pivoted
Householder least-squares implementation, with bounded design storage, column scaling,
rank/pivot diagnostics, finite-value checks, coefficient limits, and explicit
underdetermined, rank-deficient, ill-conditioned, coefficient-bound, and resource-limit
statuses. It does not use normal equations or invent unresolved coefficients.

A successful model is materialized as ordinary Genetik postfix expressions using the
existing arithmetic, variable, constant, and integer-power nodes. Variable 0 is x and
variable 1 is y. The expression and coefficient vectors are cross-checked before
prediction. Serialization uses the existing deterministic expression representation.

Optional QR+LM refinement uses Genetik's existing LM fitter against the original
quotient prediction residual. It is distinct from solving the linearized residual.
QR-only is the primary deterministic result; LM is separately measured. QR and LM
work counters are combined against declared arithmetic and linear-algebra limits.
Counters are bounded and overflow checked. Operation counters describe the library's
accounted mathematical/work units, not hardware FLOPs.

## Domain assessment

Point prediction rejects nonfinite inputs, invalid expression evaluations, and
scale-relative near-zero denominators. Rectangular assessment uses bounded outward
interval arithmetic and subdivision. It can report a demonstrated denominator zero,
a region separated from zero under its interval calculation, or an inconclusive result
when its resource/depth budget cannot establish separation. It separately reports
numerical prediction-bound status.

A valid domain result concerns only the fitted model over the assessed rectangle. It
is not evidence that the fitted model matches the unknown function, and it does not
promise extrapolation accuracy. The interval implementation is conservative but is
not a formal machine-verified proof system; results depend on the documented floating
rounding allowance. Near-pole sensitivity remains a known limitation.

## Research implementation

The implementation is in `benchmarks/s14/MultivariateRationalRegression.hpp`; it is not
installed or exported. Its campaign runner compares QR, QR+LM, GA coordinate fitting,
and GA LM fitting. The GA conditions use Genetik's ordinary search with the same
training observations and declared grammar. The constructive conditions have a
restricted rational-family inductive bias and are not the same hypothesis space as
the GA.

The frozen campaign includes three rational families, a quadratic polynomial control,
a bivariate sine control, and a near-pole stress case. Each uses clean training and
three independently noised training realizations. Held-out and extrapolation data are
clean and are used only for post-fit scoring. The reference expressions are used by
preflight and independent scoring only; they are not passed to fitters or search.

## Reproduction

Build the examples-enabled Release configuration and run the focused S14 contracts.
The full protocol, dataset hashes, exact limits, and commands are in
[`S14_PROTOCOL.md`](S14_PROTOCOL.md). Per-run outputs and the independent analysis are
stored outside the Git worktree under `Genetik-data/benchmarks/s14-multivariate-rational-discovery/`.

S14 makes no production-readiness or general symbolic-regression superiority claim.
