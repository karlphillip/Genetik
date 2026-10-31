# S1 — Parameter derivatives and optional nonlinear fitting

S1 adds an opt-in parameter-Jacobian and Levenberg–Marquardt path for smooth
scalar expressions, plus an adapter for coupled D6 scientific systems. The
existing coordinate fitter remains the default and ordinary regression APIs
retain their defaults.

## Architecture and API

`evaluateParameterJacobian` in `ParameterDerivatives.hpp` takes an existing
`Expression`, a complete binding vector, and an ordered list of parameter
binding indices. It delegates each partial to Genetik's checked scalar
derivative evaluator; repeated references to one variable share a column.
This deliberately reuses the scalar kernel and its domain checks. The current
implementation evaluates one partial per parameter, so its work scales with
parameter count rather than using a vector-valued dual number in one tree pass.
It reports unavailable derivatives explicitly for discontinuous or unsupported
rules.

`levenbergMarquardt` in `LevenbergMarquardt.hpp` accepts a residual/Jacobian
callback and bounded parameter vectors. `expressionLeastSquares` adapts scalar
expressions. `scientificSystemLeastSquares` in
`ScientificLevenbergMarquardt.hpp` builds residuals over all declared channels,
using the existing weights and scales and common parameter bindings.
`ConstantFitMethod::LevenbergMarquardt` opts into the scalar fitter; coordinate
fitting remains the default. D6 fitting is exposed by the explicit scientific
API.

The solver uses bounded Levenberg damping and a small pivoted dense solve. It
checks the undamped normal matrix for deficient or poorly conditioned pivots
before regularization, returns explicit invalid, singular, domain, derivative,
budget and cancellation outcomes, and retains the last accepted parameter
vector when a trial is rejected. This is a conservative small-parameter
implementation, not a general linear algebra package. It has no external
dependency.

## Differentiability and numerical contract

The qualified scalar evaluator supplies checked rules for addition,
subtraction, multiplication, division, general powers, integer powers, `sqrt`,
`cbrt`, `log`, `log1p`, `log2`, `log10`, `exp`, `expm1`, `sin`, `cos`, `tan`,
`asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `erf`, `erfc`, `abs` away from
zero, and `min`/`max` away from ties. The existing evaluator's operator/domain
semantics are authoritative. A valid primal whose derivative is singular,
nondifferentiable or unsupported returns derivative unavailability; it is
never silently assigned derivative zero. Gamma, log-gamma, Bessel J0, Poisson
PMF, and non-clamp ternary derivatives are unavailable. Floor/ceiling, integer
conversions, modulo and other discontinuous operations are not differentiated.
Binary64 remains the arithmetic model, so the API makes no exact-real claim.

Parameter identities are binding indices. Reusing an index at multiple nodes
accumulates all occurrences into that parameter's derivative. Parameter order
is explicit and deterministic. In scalar constant fitting, each selected
constant occurrence remains its own parameter; callers needing a shared
parameter use an expression variable referenced at every occurrence.

## Budgets and checkpoints

LM bounds iterations, objective calls, Jacobian batches, parameters, residuals,
workspace (64 MiB default), and model mathematical operations. The adapters
preflight their row/Jacobian storage; generic callbacks must report their
workspace in `LMResidualEvaluation`. `LMWork` separately reports forward and
derivative evaluations, rows, parameter updates, linear solves, accepted and
rejected steps, model operations, estimated linear algebra operations, peak
workspace and elapsed time. Coordinate evaluation work and supplemental
derivative work remain separately visible.

Native scalar run checkpoints use codec `/2` for optimizer-work persistence;
native `/1` checkpoints remain readable. Coordinate configuration identity is
preserved. LM policy and numerical limits participate in identity, so a
checkpoint cannot resume with a different fitting method or LM configuration.
Measured-system codec `/1` retains its existing payload contract. Replay
guarantees are limited to the same qualified compiler/runtime floating-point
environment.

## Scope limitations

The automatic Jacobian is assembled by repeated calls to the existing scalar
partial evaluator and may be costly for many parameters. The scalar
`ConstantFitMethod` adapter currently accepts native scalar regression rows;
specialized derivative, dimensional and typed objectives are not silently
adapted. The D6 coupled-system adapter is explicit and includes all channels.
Mixed integer expressions continue to evaluate, but their discontinuous
integer boundaries are not passed to LM. No new optimizer, population policy,
or fitting cadence is introduced.
