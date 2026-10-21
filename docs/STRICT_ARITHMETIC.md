# Strict division, natural logarithm and mixed arity

The public value/evaluator, grammar and shared search now support Divide and Log.
Defaults remain Add, Subtract, Multiply. No protected fallback, epsilon denominator,
clamp, absolute-value logarithm, hidden offset or algebraic simplification is used.

```cpp
#include <SymbolicRegression.hpp>
#include <array>
using K = ExpressionKind;
const auto x = Expression::variable(0,2), y = Expression::variable(1,2);
const auto model = Expression::binary(K::Add,
    Expression::unary(K::Log,x), Expression::binary(K::Divide,y,x));
// Six nodes, depth 3, export: (log(x0) + (x1 / x0))
const auto result = model.evaluate(std::array{2.,4.});
// Finite result approximately 2.6931471805599453.

SymbolicRegressionConfig config;
config.grammar.operators = {K::Add,K::Subtract,K::Multiply,K::Divide,K::Log};
// Supply ordinary immutable rows, seed, budgets and optional fitting as before.
// symbolicRegression(problem,config,random);
```

## Numerical outcomes

Each child must first complete with a finite double. Division rejects `b == 0.0`
(including either signed zero and 0/0) **before executing** `a/b`. Otherwise it
performs native binary64 division and accepts only a finite result. A nonzero
subnormal denominator is permitted; denorm_min/denorm_min is 1. No reciprocal
approximation is substituted.

Log requires `a > 0.0` before calling std::log. Both signed zeros and negatives
fail. Positive subnormal arguments are permitted. Log means natural logarithm.
The returned double is stored/checked for finiteness at that node, as with every
other operation. Every parent applies its own domain checks: underflow to a valid
signed zero later fails if used as a denominator or log argument. Invalid children
end evaluation in ascending postfix order, so zero cannot mask log(-1), and a
division cannot cancel an overflowing child. `x/x` is not rewritten to 1.

Finite subnormals and results rounded to positive/negative zero remain successful.
Default nearest/ties-to-even, gradual underflow and nontrapping arithmetic are
required. The evaluator does not clear/restore errno or floating-point flags,
change rounding mode or manage processor controls. Existing API exceptions are
unchanged. Domain/overflow failures return ExpressionResult mathematical invalidity
with the first failing postfix node; invalidity is not a NaN numeric sentinel.
A structurally valid log(-1) or 1/0 constructs successfully. Missing operands,
fragments, invalid bindings/configuration and malformed arities are API errors.
Allocation/unexpected exceptions remain operational and preserve their causes.

Supported reference environment: Jetson AArch64, GCC 13.3.0/libstdc++13, glibc 2.39
runtime `/lib/aarch64-linux-gnu/libm.so.6`, strict C++20 compile flags
`-fno-fast-math -ffp-contract=off`, native IEEE binary64 and the FP environment above.
Consumers must also exclude reciprocal/reassociation/finite-only/no-signed-zero
assumptions. Do not override the interface options or mix incompatible header
instantiations. The header rejects known fast/finite-only modes; it cannot detect
every individual unsafe flag. Log reference checks use decimal references with
absolute tolerances (2e-15 near log(2), 2e-12 near log(denorm_min)), not cross-platform
bitwise expectations. Repeatability is scoped to the same build/toolchain,
configuration, data/order, seed, execution mode **and runtime math library/FP
environment**. A libm upgrade may change scores and evolutionary trajectories.

## Arity, structure, limits and ownership

`expressionArity(kind)` defines 0 for Variable/Constant, 1 for Log, 2 for
Add/Subtract/Multiply/Divide; unsupported kinds throw invalid_argument.
ExpressionNode::unary(Log) and Expression::unary(Log,child,limits) complement the
existing binary factories (now accepting Divide). Postfix validation, stack/depth
metadata, subtree discovery/replacement, random construction and export use that
arity. Unary operators consume exactly one complete child; a raw sequence with
extra fragments is still rejected. Two-node unary and six-node mixed trees are
ordinary values; node parity has no constraint.

Every occurrence counts once. A leaf has depth 1, unary depth is child+1, binary
depth is max(children)+1. Inclusive node/depth/constant bounds are unchanged;
checks subtract before adding and check before depth increment, including SIZE_MAX.
Unary composition uses explicitly supplied result limits just like binary
composition. All traversals and destruction remain iterative/flat, including tested
4,096-log chains. Copies detach storage; changed/failed constructions cannot mutate
source values through aliases. Fitting enumerates constant occurrences in stable
postfix order without depending on operator arity. Complexity ranking remains exact
MSE, then total node count, then faithful text.

Export uses `(left / right)` and `log(child)` with the existing fully grouped
binary operations, exact variable identities and locale-independent max_digits10
constant spellings (including signed zero). It is a faithful description of the
tree and documented semantics, not a parser/serialization/code-generation format.

## Feasible grammar selection

An enabled operator needs remaining depth >1 and at least arity+1 nodes. Initialization
and mutation filter the enabled list under those limits, apply the existing
full/grow branch choice, then choose a uniform feasible **entry**. Unary gets
remaining budget minus one; binary keeps the previous random positive budget split.
No dataset access or retries until numerical validity are added. Terminal-rooted,
unary-rooted and binary-rooted subtree replacements all check complete destination
depth, node budget and constants. Rejected structural proposals retain the existing
finite proposal cap and parent-copy fallback. Mathematical invalidity is evaluated
later through ordinary fitness, not treated as structural rejection.

C++ lists retain order/multiplicity: repeated entries retain their previous weighting
meaning. The example CLI rejects duplicate names to keep its weight-1 choices
unambiguous. An empty C++ list still permits terminal-only search. Binary-only
configurations consume the same draws as before: feasibility filtering retains the
same list whenever a binary branch is possible, and no new random draw is inserted.
The accepted five-seed default affine campaign reproduced its expressions, scores
and work/variation counters exactly (time excluded). Expanded grammars naturally
have different trajectories. Within-build repeated searches remain deterministic.

## File workflow

```sh
out/build/release/genetik_multivariate_demo --arithmetic --generate out/data/arithmetic
out/build/release/genetik_regression_search out/data/arithmetic/training.csv \
  --inputs first,second --target target --seed 42 --loss-budget 100000 \
  --operators add,sub,mul,div,log \
  --evaluate out/data/arithmetic/interior.csv --evaluate out/data/arithmetic/exterior.csv
```

`--operators` is optional; absent means add,sub,mul. Names are case-sensitive and
comma-separated with edge spaces/tabs trimmed. Empty/repeated/unknown names or
malformed lists fail before search. Effective list and uniform-feasible-entry
selection policy are printed. Existing files, seed/budget options and --no-fit
retain their meaning. Negative/zero finite features remain valid table input;
domain validity belongs to the expression, not the reader.

The selected model/cached training score are finalized before any evaluation-set
metrics. Full checked MSE stops at its first invalid row and returns Invalid with
no partial-data score. The file example **also scans all evaluation rows** for
prediction validity: invalid_predictions_total counts invalid expression results;
invalid_errors_total counts finite predictions whose residual is nonfinite.
reporting_loss_rows and reporting_prediction_rows expose both passes separately.
No total is inferred from an early-stopping loss. These post-search calls do not
consume training allowance, fit constants or alter model selection. A model valid
on training may be invalid on evaluation; the training-selected expression stays
reported. The shared reporting contract test constructs a log model fitting two
observations of the everywhere-defined linear target ln(2)*(x-1); evaluation includes
zero/negative features with legitimate linear labels. Those are not fabricated
labels of the undefined log/div campaign target. CLI smoke checks remain quality-agnostic and do not require stochastic discovery of that model.

## Fitting, accounting and publication

Expected domain-invalid predictions/probes return Invalid fitness. A single invalid
training row invalidates the whole MSE; no row is silently excluded. Existing
coordinate fitting skips invalid starts, rejects invalid/worse/equal trials and
retains earlier accepted improvements at local stops. It makes no random draws.
One-sweep fixtures a+log(c) and a+x0/c start at a=0,c=1: a=1 improves, c=2 worsens,
c=0 is invalid; the returned a=1,c=1 expression has its matching checked loss (~.04).
Counters include those invalid attempts and actual rows before early termination;
independent verification is charged as another objective attempt.

TrainingWork.invalidLosses now counts all returned invalid training losses, including
starting losses, while TrainingWork.invalid remains the subset of invalid fitting
trial losses. Fitting calls remain a subset of total loss calls, not added twice.
Throwing calls count attempts/rows but are operational failures, not returned-invalid
losses. No budgets or publication rules changed. The existing fault-injection fixture
now uses log during partial fitting; the global-cap fixture uses division. Both
preserve committed population/cache/generation/archive and consumed RNG/work;
all-invalid prepared initialization/successors use division/log domain failures and
retain NoViableCandidates without regeneration. No second transaction loop exists.

See [STRICT_ARITHMETIC_CAMPAIGN.md](STRICT_ARITHMETIC_CAMPAIGN.md) for every run,
including misses and discarded budget-limited attempts. Engineering acceptance and
measured recovery are separate. Next is prime regression as an example of the
generic module under the preserved protocol; no prime-specific behavior was added.

Opt-in scientific scalar operators (Sin, Cos, Exp, Sqrt, Powi), domains,
metadata, work units and examples are specified in [SCIENTIFIC_OPERATORS.md](SCIENTIFIC_OPERATORS.md).
