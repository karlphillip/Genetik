# Opt-in scientific scalar operators — Phase 4 contract

This contract is recorded before implementation. Existing defaults and disabled-operator
random draws remain unchanged. Both expression APIs add Sin, Cos, Exp, Sqrt and Powi.
These are unary scalar -> scalar operations; aggregates are rejected, rather than
implicitly mapped elementwise. Scalar results may scale vectors/matrices.

Sin/Cos use radians and native std::sin/std::cos without argument rewriting. Exp
uses std::exp; Sqrt uses std::sqrt and rejects negative arguments (both zeros allowed).
Check finite results, not errno or floating-point exception flags. Finite subnormals
and underflow to signed zero remain valid. Children always execute first; invalid
children remain invalid even below Powi exponent zero or a masking parent.

Powi stores an unsigned discrete exponent in [0,8], separately from fitted constants.
Evaluate the base once. Exponent zero returns 1 (including a valid zero base), one
returns the base, and higher exponents multiply the running value by the original
base, checking each native multiplication, in order. No std::pow, reassociation or
exponentiation by squaring. Export is powi(child,integer). Metadata survives owned
copies and subtree operations; withExponent derives a checked new value.

Typed work counts one unit per executed native mathematical call, following Log;
this is opaque-call accounting, not internal libm instructions or equal runtime.
Powi charges max(exponent-1,0) multiplications in addition to child work. Existing
scalar loss-attempt units remain unchanged. Typed full-plan work/payload admission,
mathematical invalidity, local resource outcomes and operational exceptions retain
their existing meanings. Fitting changes constant coefficients only; fitting and
workers use no extra RNG or shared mutable mathematical state.

Repeatability requires the same revision, compiler, standard/runtime math libraries,
floating-point environment, inputs and execution configuration. Strict FP flags,
default rounding and gradual underflow remain required. No cross-platform bitwise
libm guarantee or general accuracy bound is promised. Export describes the executable
tree, without simplification or parser-round-trip claims.

## Public construction and selection

```cpp
using K = ExpressionKind;
const auto x = Expression::variable(0);
const auto decay = Expression::unary(K::Exp,
    Expression::binary(K::Multiply, Expression::constant(-1,1), x));
const auto square = Expression::power(x,2);
const auto changed = square.withExponent(1,3); // postfix power node, source unchanged
SymbolicRegressionConfig config;
config.grammar.operators = {K::Add,K::Multiply,K::Sin,K::Cos,K::Exp,K::Sqrt,K::Powi};
config.grammar.exponents = {2,3};
```

TypedExpression has the same `power(child, exponent, limits)` and `withExponent`
operations. Raw nodes use `ExpressionNode::power` / `TypedExpressionNode::power`;
`unary(Powi)` is rejected because it lacks required metadata. Inspection uses
`node.exponent()` on Powi only. Subtree copies retain that metadata. Exponent subsets
must be nonempty, unique and within [0,8], even if currently disabled. Defaults contain
all nine exponents; default operator lists are unchanged. Existing enum values remain
unchanged. Scalar generation selects a feasible enabled operator entry, then a feasible
configured exponent uniformly. Typed generation retains its existing uniform
selection over feasible (operator, operand-shape) productions, then selects an
exponent uniformly. Typed planning uses a lowest-cost exponent witness, complete-plan
validation and finite proposal fallback; it does not promise a complete feasibility
solver. Mutation on a selected power node, when power is enabled, has probability .5
of replacing its exponent from the subset; otherwise ordinary subtree mutation.
No additional random draw occurs for this decision when power is disabled or the
selected node is not power. Crossover preserves metadata without interpreting it.

Scalar ExpressionLimits adds `maxArithmeticOperations` (default SIZE_MAX) after
existing aggregate fields. Construction checks the full valid cost; `arithmeticCost()`
is the full plan cost and ExpressionResult::arithmeticOperations() is executed work,
including work before mathematical invalidity. Domain rejection executes no native
call. Scalars have no separate per-call resource outcome: exceeding a construction
limit throws length_error, consistent with node/depth limits. Typed expressions keep
the existing construction TypedLimitError and per-call ResourceLimit outcome.
Neither diagnostic changes engine loss units. Scalar initialization allocates a finite
arithmetic allowance among children when supplied; replacements check complete costs.
Typed generation validates all complete temporary/work/coefficient limits and falls
back to a validated witness after its finite proposal allowance.

File workflow (ordinary build commands are in ORGANIZATION.md):

```sh
out/build/release/genetik_regression_search tests/fixtures/scientific/decay-training.csv \
  --inputs x --target y --seed 2 --loss-budget 10000 --no-fit --discrete-only \
  --population 128 --nodes 15 --depth 5 --loss-threshold 1e-24 \
  --operators add,sub,mul,exp --exponents 2,3 --threads 1 \
  --evaluate tests/fixtures/scientific/decay-interior.csv \
  --evaluate tests/fixtures/scientific/decay-exterior.csv
```

`--operators` accepts add,sub,mul,div,log,sin,cos,exp,sqrt,powi; names and exponent
lists reject empty/duplicate/malformed entries before search. `--exponents` controls
Powi only, never the continuous fitter. Defaults remain unchanged. File-application
optional positive `--population`, `--nodes`, `--depth` (maximum4096), and
`--discrete-only` allow reproducible bounded configurations. Effective values print
in the configuration. Scientific functions do not change finite file-input rules;
negative/zero inputs may be valid data but outside a particular model's domain.
Post-search reporting examines all rows and reports invalid prediction counts and
invalid full loss, without promoting a different model.

The retained reference fixtures use mpmath at200 and320 decimal digits, exact
binary64 rational inputs and agreement after binary64 rounding. Tests allow four
ULPs around the retained reference, with a denormal floor, for qualified runtime
calls. Large trigonometric inputs are exact binary64 values, not rounded decimal
phases. These are selected independent checks, not a universal libm accuracy bound.
See scientific-results/PROTOCOL.md for the frozen campaign and data identities.
