# Owned typed values and strict expression evaluation

Phase 2 is additive: `<TypedValue.hpp>` and `<TypedExpression.hpp>`, linked through
`Genetik::Genetik`. Existing scalar `Expression`, generic GA and scalar regression
APIs keep their behavior and have no dependency on these new headers. No third-party
numerical library, RNG, dataset, loss, fitter or execution backend is introduced.
This contract was recorded before implementation on feature/typed-evaluation.

## Types, construction and ownership

`TypedShape::scalar()`, `vector(length)` and `matrix(rows, columns)` are distinct.
Dimensions are positive and checked for integer overflow. A vector is a column
vector for matrix–vector multiplication; it is never implicitly a scalar or matrix.
Matrices store coefficients in row-major order. `TypedValue::scalar(double)`,
`vector(span<const double>, limits)` and `matrix(rows, columns, span, limits)` copy
finite coefficients, even from temporary input buffers. `shape()`, `coefficients()`
(a read-only span), `scalarValue()` and checked `at()` inspect owned values.

A schema is an ordered span of shapes, copied by the expression. Schema equality
means ordered shape equality, not an application-level problem identifier. Index i
binds xi to binding i, including unused entries. Constants own their coefficients.
Copies are independent. Tokens, values, expressions and results use strong copy
assignment, including allocation failure between different kinds/statuses. No
supported API returns mutable representation access. Moved-from value, expression
and result inspection throws logic_error; those objects may be destroyed,
reassigned or swapped. Reassign moved-from tokens before reuse. Returned coefficient
views require their owner to remain alive and unmodified. Casting away const is
not a supported edit.

`TypedExpressionNode::variable`, `constant`, `unary`, `binary` construct typed
postfix tokens. `TypedExpression(postfix, schema, limits)` validates exactly one
complete acyclic tree, infers each node shape and checks the complete plan before
copying tokens. Static `variable`, `constant`, `unary`, `binary` factories offer
composition; operand schemas must match exactly, including unused entries. Composite
limits are explicitly supplied (default limits otherwise), not implicitly merged.
Node/schema inspectors return detached copies or shape values. Derived construction
and failed assignment never modify an existing expression.

## Operations and numerical order

| Operation | Accepted operands | Result |
| --- | --- | --- |
| Add / Subtract | same kind and shape | same shape |
| Multiply | scalar/scalar, scalar/vector or matrix, vector or matrix/scalar | scalar or the nonscalar shape |
| Divide | scalar/scalar | scalar |
| Log | scalar | scalar, natural logarithm |
| Dot | two equal-length vectors | scalar |
| MatVec | matrix(m,k), vector(k) | vector(m) |
| MatMul | matrix(m,k), matrix(k,n) | matrix(m,n) |
| Transpose | matrix(m,n) | matrix(n,m) |

No broadcasting, flattening, vector–vector `Multiply`, implicit conversions or
componentwise logarithms/division. Divide rejects both signed-zero denominators;
Log requires a strictly positive argument. Native binary64 operations and qualified
std::log are used. Every executed product and sum is stored as double and checked
before the next operation. Invalid children stop evaluation before a parent can
mask them. No simplification, contraction, reassociation or algebraic domain rewrite.

Nodes execute in increasing postfix order. Output coefficients execute row-major
(vectors increasing index). Each nonempty reduction executes product 0, checks it,
and initializes its accumulator from that product. Then for k=1,... in increasing
inner index, it checks each product, then each accumulator addition. There is no
initial addition to +0. Signed zero, finite subnormals and finite underflow follow
the existing scalar policy; domain checks still apply to an underflowed zero.
Strict CMake FP requirements, native binary64, default nearest/ties-even, gradual
underflow, nontrapping arithmetic and the qualified runtime/libm remain required.
No cross-platform/libm bitwise promise or FP-environment management is added.

## Bounds, work and failure categories

`TypedValueLimits` defaults: vector length 4096, matrix rows/columns 256 each,
coefficients per value 65536. `TypedExpressionLimits` additionally defaults to
1023 nodes, depth 64 (leaf 1), 64 schema variables, 1,000,000 schema coefficients,
256 constant occurrences, 65536 total constant coefficients, largest-finite absolute
constant, 262144 temporary coefficients, and 1,000,000 arithmetic operations.
All shape/count/product/sum/work arithmetic is checked before allocation/iteration.
Limits are inclusive. Node/depth/value dimensions and per-value coefficient limits
must be positive. Zero variable/constant counts and zero arithmetic allowance permit
appropriate constant-free or arithmetic-free expressions.

`arithmeticCost()` is the full valid execution plan: one unit per scalar add,
subtract, multiply, divide or log call (not CPU instructions or libm internals).
A k-term reduction costs k products plus k-1 additions. There is no charged add
for accumulator initialization. Transpose costs zero arithmetic. Evaluation's
`arithmeticOperations()` counts actually executed primitives, including overflowing
ones; failed pre-operation domain checks consume no primitive. Invalid evaluation
may stop early. This is independent of engine candidate/loss/row budgets.

`temporaryCoefficients()` is peak logical live operand/output doubles, including
new output while input operands are still alive. It excludes immutable expression
constants, caller bindings, metadata, allocator/container overhead and export text;
it is **not** a cap on process memory. Per-value bounds, schema totals and node
counts bound validation, copying and transpose work separately. A crude bound on
coefficient copying/non-arithmetic traversal is O(schema coefficients + nodes ×
maximum value coefficients). Export/storage are bounded by node and constant totals. Postfix validation/inspection
is iterative. Repeated factory composition while growing a chain can take quadratic
total copying time across those calls; batch postfix construction avoids that pattern.

Evaluation accepts a borrowed span of owned TypedValues for the synchronous call,
retains no references, validates exact binding kinds/shapes and finite coefficients
(including unused inputs), and uses only invocation-owned scratch. The returned
value owns its result. Immutable expressions may be evaluated concurrently using
independent bindings/calls; there is no shared mutable scratch or nested worker pool.
Expressions and borrowed binding objects must outlive every call; concurrent
reassignment, mutation or destruction of those objects is unsupported.

`TypedEvaluationLimits` can lower arithmetic and temporary-coefficient allowances
for a call. Evaluation first validates all bindings, then admits the known complete
plan. If either allowance cannot cover it, status is ResourceLimit, work is zero,
and no partial mathematical result is exposed. Otherwise it evaluates until success
or first invalid arithmetic/domain result. Construction also enforces its plan caps.

- Bad type/shape/schema/bindings/configuration/nonfinite construction inputs:
  invalid_argument; checked index errors: out_of_range; invalid access/move use:
  logic_error. Nonfinite inputs follow the scalar API's error policy, not invalidity.
- Construction count/capacity/work overflow or exceeded structural/resource limits:
  `TypedLimitError` (length_error subclass), never mathematical invalidity.
- Evaluation status Success: only then `value()` is usable. MathematicalInvalidity:
  no value; first `failedNode()` and row-major `failedCoefficient()` are reported.
  ResourceLimit: no value or failed arithmetic node; not evidence of a bad formula.
- Allocation failures/unexpected operational exceptions propagate unchanged, never
  as invalidity/resource outcomes. A throwing call returns no result or local work
  record; arithmeticOperations describes returned outcomes only. No engine counter
  is changed by this evaluator. All scratch is lexical; unsuccessful calls leave
  expressions, bindings and previously returned values unchanged.

## Faithful inspection/export

`toString()` preserves operators, grouping, operand order, variable indices and
row-major constants. Explicit dot/matvec/matmul/transpose names avoid ambiguous `*`.
Typed constant literals include their kind/dimensions; associate expression text
with `schemaString()` and `shape().toString()` to preserve variable/result types.
Constants use locale-independent to_chars/max_digits10 and explicit negative zero.
No parser, serialization round-trip or algebraic-equivalence claim is made.

Useful plan inspectors: nodeCount, depth, nodeShape, schema, nodes, constantCount,
constantCoefficients, bindingCoefficients, arithmeticCost, temporaryCoefficients.
Typed datasets/loss and bounded typed variation/search/fitting are now implemented
additively in TYPED_REGRESSION.md. Parsers, scientific functions and calculus remain
future work; this expression value itself retains no problem or fitness.

## Installed public example

The complete [typed quickstart](../examples/quickstarts/typed.cpp) constructs:

```cpp
#include <TypedExpression.hpp>
#include <array>
using K = TypedExpressionKind;
const std::array schema{TypedShape::vector(3), TypedShape::vector(2)};
const auto A = TypedExpression::constant(
    TypedValue::matrix(2,3,std::array{1.,2.,3.,-1.,0.,4.}), schema);
const auto x = TypedExpression::variable(0, schema);
const auto b = TypedExpression::variable(1, schema);
const auto f = TypedExpression::binary(K::Add,
    TypedExpression::binary(K::MatVec, A, x), b);
const std::array inputs{TypedValue::vector(std::array{2.,-1.,3.}),
                        TypedValue::vector(std::array{5.,-2.})};
const auto result = f.evaluate(inputs);
// Success: vector[2](14.0,8.0), 12 arithmetic operations, peak payload 11.
// Persist f.schemaString(), f.shape().toString(), and f.toString() together.
```

Build it against an installation (ordinary CMake >=3.16):

```sh
cmake -S . -B out/build/library -DBUILD_TESTING=OFF -DGENETIK_BUILD_TESTS=OFF -DGENETIK_BUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX="$PWD/out/install/typed"
cmake --install out/build/library
cmake -S examples/quickstarts -B out/consumers/typed -DCMAKE_PREFIX_PATH="$PWD/out/install/typed"
cmake --build out/consumers/typed -j2
out/consumers/typed/typed_quickstart
```

The standalone source-build target is `genetik_typed_quickstart`. It writes only
stdout (redirect to out/logs/ when saving). Its deterministic acceptance uses known
values, not stochastic discovery. Matrix kernels are serial even if the application
also owns a Genetik execution context; independent evaluation calls can overlap.

## Current scope and compatibility

This is post-v0.1.0 local development, not a new published release. Existing scalar
Expression/ExpressionNode, fitness/preparation, RNG, OOP and search APIs are unchanged.
TypedValue reuses Expression.hpp's strict-FP/native-binary64 build diagnostics; no
existing header includes the new typed API. Value factory limits govern each new
construction; an expression independently revalidates schema and constant shapes
under its own limits. There is no retained objective identity, dataset or fitness.
A resource outcome is never mathematical invalidity. The additive Phase 3 adapter
now maps it to TypedResourceExhausted through failed preparation; see TYPED_REGRESSION.md.

Executed local checks and remaining platform scope are recorded in
[TYPED_EVALUATION_QUALIFICATION.md](TYPED_EVALUATION_QUALIFICATION.md).

Phase 3 adds an optional evaluateWithWork diagnostic sharing this same interpreter;
its exclusive counter retains executed primitive counts on thrown calls. The
original evaluate API and arithmetic traversal are unchanged.
