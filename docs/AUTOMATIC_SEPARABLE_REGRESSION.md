# Automatic separable regression

Automatic separable regression is an opt-in wrapper around the existing
two-component expression-template search. It supports only two-variable scalar
observations on a complete rectangular grid. It tests the finite observations
for the hypotheses `f(x,y)=g(x)+h(y)` and `f(x,y)=g(x)*h(y)`; it does not prove
global separability.

## Quick start

See [the installed C++ quickstart](../examples/quickstarts/automatic_separable.cpp).
It creates training observations, configures independent component grammars,
calls `automaticSeparableRegression`, then scores separate assessment points.
The training equation is shown only by the example's data generator; the API
receives no reference equation or components.

The public entry point is `<AutomaticSeparableRegression.hpp>`. Configure
`AutomaticSeparableConfig::detection` for input and work bounds, set model and
component `ExpressionLimits`, then provide the two allowed component grammars
and ordinary `SymbolicRegressionConfig`. The same caller-owned `RandomContext`
rules as other symbolic searches apply. Detection itself is deterministic and
uses no RNG. Component search starts only after exactly one decomposition
hypothesis is supported.

## Detection contracts

Rows must have exactly two finite inputs and a finite target. Each coordinate
pair must occur once, and every pair in the Cartesian product of observed x and
y levels must be present. Inputs are sorted numerically before matrix checks,
so input row ordering does not affect results. Duplicate points, missing cells,
and nonfinite inputs are invalid. Observation count and mathematical work are
bounded by `SeparabilityLimits`.

Additive consistency checks all cross differences
`f(i,j)-f(i,0)-f(0,j)+f(0,0)` after scaling the observations by their maximum
absolute magnitude. Multiplicative consistency checks all 2x2 minors against
a deterministic maximum-absolute-value pivot, also after scaling. The
lexicographically first point wins ties. The pivot is the detector's numerical
normalization convention; multiplicative factors are scale-ambiguous and the
factor witness is never passed to component initialization. A zero-valued grid
or pivot below `minimumPivotMagnitude` is degenerate/uncertain. This avoids
pretending that an unidentifiable factorization is unique.

Each hypothesis reports `Supported`, `Rejected`, or `Uncertain`. Tolerances
are absolute plus relative after scale normalization. A tolerance band ten
times wider than the support threshold is reported uncertain; stronger
residuals reject that hypothesis. If both hypotheses fit, the result is
ambiguous and no template is constructed. Strict detection remains the
default. Callers may explicitly set
`SeparabilityLimits::measurementNoiseBound` to enable bounded-noise consistency.
This is an absolute per-observation target-error bound supplied by the caller;
it is not inferred from the data. For additive cross-differences, four
independent observation errors contribute at most `4*epsilon`. For a rank-one
minor `a*d-b*c`, the implementation uses normalized values and the conservative
bound `4*e*(1+e)+2*e*e`, where `e=epsilon/maxAbsObservedTarget`: each unknown
true entry has magnitude at most `1+e`, and the two product-error terms
contribute `2*e*e`. A small binary64 arithmetic allowance is added in
noise-aware mode. Residuals within the bound support compatibility with the
hypothesis under the declared error model; they do not prove separability. A
weak interaction smaller than the noise envelope can therefore be
indistinguishable from measurement error. Both supported hypotheses remain
ambiguous, and near-bound evidence remains uncertain according to the 10x
transition band. The result reports the assumed bound and normalized
acceptance bounds.

The noise-aware policy assumes strict IEEE binary64 behavior described in
`docs/SUPPORT.md`; it is a residual consistency test, not a rigorous outward
interval certificate. Rejection remains rejection of the tested finite-grid
hypothesis at the configured thresholds. If a normalized tolerance exceeds
10% of observed scale, that hypothesis remains uncertain rather than treating
a very loose tolerance as evidence.

## Construction and assessment

Only a unique supported result constructs a fixed `x0+x1` or `x0*x1`
scaffold. Site 0 is restricted to variable 0 and site 1 to variable 1. Their
operator sets, constants, and structural limits are independently configured
through the two grammars and component limits. The existing multi-component
variation, complete-model objective, constant fitting, evaluation, and
resource accounting are reused. No new optimizer, expression representation,
or GA behavior is introduced.

Held-out rows are intentionally not accepted by the discovery function. Keep
them separate and evaluate the selected complete expression independently
after search. A finite-grid detector result and good held-out score do not
prove global identity. For multiplicative models, scaling can move between
components, so component-string equality is not a structural recovery test.

`x+y+xy` is not in the supported pure additive or pure multiplicative forms,
even though it can be written as `(1+x)(1+y)-1`; fixed additive offsets around
a product are outside this milestone. `sin(x+y)` is also outside the supported
forms. Degenerate constant grids can satisfy both identities and therefore do
not choose a decomposition.

## Bounds and limitations

The detector accounts for one coordinate lookup per observation and each
cross-difference/minor/pivot check. `maxObservations` and
`maxMathematicalOperations` and `maxEstimatedMemoryBytes` produce explicit
`ResourceLimit` results. The deterministic memory estimate covers logical
storage for the detector-owned sorted coordinate arrays, grid matrix, presence
map, and vector objects. It is not a process RSS limit; allocator metadata and
implementation-specific capacity over-allocation are outside this estimate.
The observation count also bounds these arrays. Existing expression/template
limits bound search construction. Allocation failures remain operational
exceptions.

Search quality depends on the configured grammars, limits, seeds, and budget.
The method does not guarantee a discovered model, identify unique factors, or
outperform ordinary symbolic regression. Detection checks finite observations
only; held-out assessment is needed to measure prediction outside the training
grid. No benchmark result establishes universal separability.

## Comparison evidence

See the [bounded comparison report](AUTOMATIC_SEPARABLE_COMPARISON.md) for
the retained fixed-seed results and work counters.

`benchmarks/automatic_separable_comparison.cpp` is a bounded fixed-seed
comparison of automatic detection plus search, manually configured two-site
templates, and ordinary symbolic regression. It includes clean additive and
multiplicative controls, `sin(x+y)`, `x+y+xy`, separate held-out grids, and two
distinct fixed-hash noise realizations at amplitude `1e-4`. It reports training MSE, held-out RMSE, node
count, candidate and training-row work, and detection work. The benchmark is
opt-in and is not part of CTest because stochastic discovery outcomes are not
CI correctness contracts.
