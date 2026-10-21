# Bounded scalar partial derivatives (Phase 7A)

Design contract, recorded before implementation: `ExpressionDerivative.hpp` is an
optional header over immutable `Expression`. `evaluatePartial(expression, bindings,
inputIndex, DerivativeLimits)` computes the ordinary value and one first-order
chain-rule partial in native binary64. It is neither symbolic differentiation nor
the derivative of the rounded program as a discontinuous map. A finite result is
not a global differentiability proof or an accuracy bound. Rounding, underflow,
conditioning and intermediate overflow can limit the result. The existing strict
FP, default rounding, gradual underflow and qualified compiler/libm contract applies.

## Outcomes and ownership

`DerivativeStatus` distinguishes Success, PrimalInvalidity, Unavailable and
ResourceLimit. Only Success/Unavailable expose a complete finite `value()`; only
Success exposes `derivative()`. `DerivativeFailure` identifies a SingularRule or
NonfiniteArithmetic and its first postfix node. This diagnostic is retained even
if a later primal failure or resource stop takes precedence. Primal invalidity
always dominates derivative unavailability; no partial value is returned on a
resource stop. API errors (wrong binding count, nonfinite inputs, out-of-schema
target, moved-from expression) retain existing exception conventions. All bindings,
including unused ones, must be finite. Allocation failures propagate unchanged.

No mutation, randomness, loss attempt, fitting state, worker creation, callback or
result cache is involved. Bindings are borrowed synchronously; results own scalars.
Expression and binding storage must remain alive and unchanged until return.
Invocation-local operand storage permits independent concurrent calls sharing an
immutable expression. General engine access rules do not change.

## Rules and order

Postfix traversal and checked primal kernels are shared with ordinary evaluation.
Structural activity means a subtree contains the selected input, regardless of its
current derivative. Inactive subtrees evaluate normally and yield canonical +0.
Unavailable active children propagate; later zero factors, cancellation and Powi
zero do not remove the failure. This conservative rule does not prove that the
complete real function lacks a derivative (e.g. sqrt(powi(x0,4)) at zero).

For active, available children, checked derivative operations execute in this order:

* Add/Subtract: both active, da +/- db; left only, da; right only, db or unary -db.
* Multiply: both active, da*b, a*db, then their sum; one active, only its term.
* Divide: left only, da/b; right only, q*db, negate, divide by b; both active,
  q*db, da minus that product, then divide by b, reusing primal q=a/b.
* Log: da/a. Exp: primal*da. Sin: native cos(a), then product with da.
  Cos: native sin(a), then checked unary negation, then product with da.
* Sqrt: active argument must be strictly positive; either zero makes the local
  rule singular even for da=0. Then form 2*primal and da divided by that result.
* Powi: validate/evaluate child first. Zero yields +0 only if its active path is
  available, one copies da. For each sequential primal multiplication p*base,
  compute d*base, p*da, then sum, using the previous p and d. No condensed power
  formula, std::pow, exponentiation by squaring, simplification or reassociation.

Every required derivative primitive rounds/stores and checks finiteness before the
next. Finite signed zeros and subnormals remain valid. No errno/FP-flag tests are
added. A derivative failure stops further derivative work but ordinary evaluation
continues, subject to remaining resources; later primal errors cannot be masked.

## Resources

`DerivativeLimits` bounds total operations (default 1,000,000), node visits (65,536),
validated bindings (65,536), and operand payload bytes (8 MiB). Zero allowances are
valid and can return ResourceLimit. Exact schema/target validation precedes limits;
finite-binding validation is bounded by maxBindings, before allocation/arithmetic.
One operation is a checked primal arithmetic/native call, derivative arithmetic
(including unary negation), or additional native derivative call. Opaque libm calls
are not instruction counts or equal-time units. Domain rejection costs no operation.
Counters split these three disjoint categories and also expose visited nodes,
validated bindings and requested operand bytes. Admission precedes each operation;
a rejected operation is not counted. Powi charges every explicit multiplication.

One operand array has at most N entries for N validated nodes. Count multiplication
and payload limit checks precede allocation. The payload cap excludes immutable
expression storage, allocator overhead, stack locals and process memory. Traversal
is iterative, O(B+N+P) work and O(N) storage; P is bounded Powi recurrence work.
No expression copy or unbounded prepass is required. Limits are local evaluation
limits, independent of expression construction and engine loss budgets. Ordinary
and derivative evaluation need not complete with the same work allowance.

## Public and installed usage

```cpp
#include <ExpressionDerivative.hpp>
#include <ExpressionParsing.hpp> // optional: direct Expression construction works too
#include <array>
const auto f = parseExpression("x0*x0+x0*x1+sin(x1)", 2);
DerivativeLimits limits; limits.maxOperations = 100;
const auto dx = evaluatePartial(f, std::array{2.,0.}, 0, limits);
if (dx.status() == DerivativeStatus::Success) {
    // dx.value() == 4, dx.derivative() == 4; target 1 instead gives 3.
}
```

`primalFailedNode()`, `derivativeFailedNode()` and `resourceNode()` are zero-based
postfix locations. `derivativeFailure()` and `exhaustedResource()` identify the
reason. Resource rejection before traversal has no node. Counters include attempted
nonfinite operations, but exclude domain-rejected operations and primitives whose
admission failed. `nodesVisited` includes an operator visited before an operation
stop. `bindingsValidated` includes the binding being checked (nonfinite bindings
throw); workspaceBytes is the admitted operand-array payload, not peak live entries.
No partial computed scalar is exposed on PrimalInvalidity or ResourceLimit.

The compiled [derivatives quickstart](../examples/quickstarts/derivatives.cpp) covers
multivariate partials, parsed decay, a materialized template with original (v,T)
bindings including zero velocity, and a singular-rule diagnostic. It starts no search.

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/release -j2
out/build/release/genetik_derivatives_quickstart
# Independently build the same source against an installed package:
cmake -S . -B out/build/library -DGENETIK_BUILD_TESTS=OFF -DBUILD_TESTING=OFF \
  -DGENETIK_BUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX="$PWD/out/install/local"
cmake --install out/build/library
cmake -S examples/quickstarts -B out/consumers/quickstarts \
  -DCMAKE_PREFIX_PATH="$PWD/out/install/local"
cmake --build out/consumers/quickstarts --target derivatives_quickstart
out/consumers/quickstarts/derivatives_quickstart
```

## Numerical reference and scope

The retained `tests/fixtures/derivatives/references.{hpp,json}` are generated by
`scripts/generate_derivative_references.py --output DIR`. Development-only mpmath
1.2.1 computes independently derived analytic rules at 200 and 320 decimal digits
from each exact binary64 input's integer ratio; the two binary64 roundings must
agree. In particular 1e20 and 1e100 trig inputs use their **binary64** values, not a
decimal approximation to their phase. No implementation helper supplies the oracle.
Fixtures cover cos(x), -sin(x), exp(x), 1/x and 1/(2*sqrt(x)). Four ULPs at the
reference scale accommodate qualified libm rounding and, for sqrt, the sequential
rounded denominator and quotient. This is a selected-fixture tolerance, not a
universal derivative-error promise. Exact dyadic analytic fixtures cover arithmetic,
all exponents, independent variables and full template partials.

`genetik_derivative_contracts` also compares primal status, first-failed node, work
and successful value bits against ordinary evaluation, with a bounded generated
corpus and deep iterative chain. `genetik_derivative_overlap` gates allocations
inside independent calls on the existing executor at 1/2/4, including simultaneous
success and unavailability. Local/hosted qualification scopes are recorded separately
in DERIVATIVE_QUALIFICATION.md. No existing grammar or fitting default changes.

Unavailable is conservative: no symbolic cancellation, limit analysis or domain
reasoning is attempted. No gradients, constant derivatives, derivative losses,
optimizers, parser diff syntax, typed Jacobians or integration are implemented.
