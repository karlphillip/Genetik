# D5 advanced mathematical modules

D5 is additive local development on the published 0.4.0 tree. Genetik remains a
C++20 general-purpose GA framework. The generic population, execution,
publication and checkpoint headers do not include these mathematical modules.
No default grammar, coordinate optimizer, mutation probability or prime objective
is changed. Research fitness/optimizer/mutation branches excluded from 0.4.0
remain excluded. This is not a release or cross-platform qualification.

## Scoped discovery

`DiscreteGrammar::iteratorDiscovery` is an opt-in production policy, not a new
operator. A newly generated sum/product body without its own iterator receives
one eligible Int64 terminal replacement. A terminal that is the sole reference
to a nested body's own iterator is protected. No eligible site means bounded
structural rejection. Free outer references are permitted. Crossover aligns
incoming free bindings to available destination bindings in innermost order,
renaming internal binder collisions. An insufficient destination scope rejects
that proposal. Existing proposal caps and subtree fallback remain finite.
Default false preserves D4 productions and RNG draws.

`recordIterationDiagnostics` counts generated iteration-bearing and dependent
expressions, body repairs and aligned crossover operations. Repairs count
attempts, including rejected proposals; generation counters count completed
returned expressions. Population diagnostics run only after publication, never
fit or update caches, and consume no training loss attempts. Counts associated
with fitness improvements are observational: crossover and selection prevent a
causal attribution to a single edit.

The frozen original-versus-dependent study uses the same D4 operator set,
seeds 0/1/42, population32, workers1, 40 generations/1312 attempts, nodes25,
depth6, per-operation64/aggregate4096 iterations and 100000 node/operation
allowances. Independent loop references supply square-sum training0..8,
assessment9..12; divisor training1..16, assessment17..32. The exact expressions
are direct representability checks, never initialization seeds. See
[d5-results/iteration/PROTOCOL.json](d5-results/iteration/PROTOCOL.json) and
[results](d5-results/iteration/RESULTS.json). Increased iterator production must
not be equated with successful recovery or survival.

## Checked discrete arithmetic

Include `DiscreteExpression.hpp`, optionally `DiscreteExpressionParsing.hpp`.
New explicit IDs18..25 append to existing IDs0..17. The shared integer
registry records names and arity; the integer evaluator remains separate from
binary64 scalar execution.

| Canonical operation | Contract |
|---|---|
| `integer_divide(a,b)` | b nonzero; unique q,r with a=b*q+r and 0<=r<abs(b); unrepresentable q invalid |
| `gcd(a,b)` | nonnegative gcd of unsigned magnitudes; gcd(0,0)=0; result2^63 invalid |
| `lcm(a,b)` | nonnegative; zero if either input zero; cross-divide before checked multiplication |
| `integer_power(a,k)` | k>=0; 0^0=1; checked binary powering |
| `modular_power(a,k,m)` | k>=0,m>0; Euclidean base reduction; bounded unsigned add/double, no overflowing product |
| `factorial(n)` | n>=0; 0!=1; n>20 overflows Int64 |
| `binomial(n,k)` | 0<=k<=n; exact cancellation; unrepresentable result invalid |
| `first(k0,a,b,predicate)` | inclusive range; Bool body; first true witness; empty/exhausted search returns NoWitness |

`INT64_MIN` is handled through unsigned magnitude, never undefined negation or
MIN/-1. Ordinary D1 modulo still requires a strictly positive divisor.
IntegerDivide accepts either sign of divisor with the explicitly Euclidean
quotient convention above. Its corresponding remainder is nonnegative.

All new integer inner arithmetic charges operations and checks cancellation;
resource exhaustion is not arithmetic invalidity. Primitives charge one dispatch
operation plus their bounded internal steps. Binomial uses symmetry and at most
63 cancellation steps; k=min(k,n-k)>63 implies an unrepresentable result.
The explicit full-range interval preadmission used by D2 also applies to First:
a huge search can exhaust its declared per-operation allowance even if a witness
would occur early. Only attempted predicate iterations are consumed after
admission. No witness is a distinct evaluator result, and an unscorable required
regression observation, never an arbitrary integer or finite penalty.
Integer operations do not offer real derivatives.

## Bounded recurrences

`IntegerRecurrence.hpp` owns an external schema, one or two Int64 initial
conditions, and an ordinary discrete transition. Transition slots are:
external `[0,N)`, step index `N`, oldest previous value `N+1`, then newer `N+2`
for order2. `previousSlot(state)` and `previous(state)` expose these roles.

```cpp
IntegerRecurrence fibonacci(0, {0,1}, parseDiscreteExpression("x1+x2",3));
auto value = fibonacci.evaluate(20); // 6765
```

First order computes a[n+1]=f(n,a[n]); second order computes
a[n+2]=f(n,a[n],a[n+1]). Initial queries never evaluate the transition.
Private storage holds at most two states plus bindings, not an unbounded history.
A step charges one operation and aggregate iteration plus actual transition work.
`IntegerRecurrenceLimits` caps steps/storage; discrete evaluation limits cap all
aggregate work, nested iteration and scratch. Failed steps return no partial
sequence value. Cancellation polls before steps and inside the transition.

`recurrenceRegressionProblem` accepts owned `(index, externalInputs, target)`
observations and uses the complete recurrence for each score. Fixed order and
initial conditions are immutable problem context; transition structure evolves
through `DiscreteEvolutionRun` without adding recurrence types to the GA.
The present adapter does not evolve order or initial conditions.
Standalone codec `genetik.integer-recurrence/1` records explicit operator and
binder IDs, initial conditions, binding layout and policy. No recursive call
language or unbounded recurrence is implemented.

## Scientific scalar profile

`Expression.hpp` adds opt-in scalar IDs12..30, while IDs0..11 and legacy
`Expression::power(child,unsigned)` remain stable. Use
`Expression::integerPower(child,int)` or parsed `powi(x,-n)` for negative
integer exponents, magnitude1..8. The inverse is checked first, followed by
sequential multiplication of the inverse. Existing nonnegative Powi0..8 retains
its original sequential multiplication and child-validity semantics.

New names: `pow`, `cbrt`, `log1p`, `log2`, `log10`, `expm1`, `tan`, `asin`,
`acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`, `abs`, `min`, `max`, `clamp`.
Clamp has three strict operands, requires lower<=upper, and does not skip invalid
children. Pow requires a positive base for general real exponents; a negative
base requires an exactly integral binary64 exponent. Zero base requires exponent>0
(so general Pow(0,0) is invalid, distinct from discrete/legacy Powi conventions).
Negative Powi requires nonzero base. Log1p requires x>-1, log2/log10 x>0;
asin/acos require [-1,1]; atan2(0,0) is invalid. Cbrt permits negative inputs.
All finite native results, signed zero and subnormal/underflow results remain
valid; no protected arithmetic, clipping, reassociation or stale-errno failure.
Min/max ties select the left operand. Trigonometric inputs use radians.

`detail/ScalarOperators.hpp` centralizes scalar ID/name/arity metadata used by
construction, parsing, formatting, grammar validation and serialization. Typed
vector/matrix operators are not silently expanded. Scalar scientific operators
cost one native operation per node; NegativePowi costs its magnitude, including
the reciprocal. Finite/domain tests are included in node work. Native libm
behavior retains the existing strict-FP/runtime qualification requirements.

`ExpressionGrammar` permits independent `negativeExponents`, constant-real
`realExponents`, and `powerExponentsAreConstants` (true for an enabled Pow).
False explicitly permits exponent expression trees. Neither option adds Pow to
the default grammar. Real-exponent literal occurrences are ordinary fitting
coordinates except where dimensions require freezing them.

The existing numerical forward partial API supports analytic local rules for
all these nodes. Abs0, min/max ties, clamp boundaries, cbrt0, inverse-trig
endpoints and the active atan2 branch cut return unavailable while preserving
a valid primal. Active real exponents at negative bases are unavailable.
Required nonfinite intermediates are checked; local unavailability propagates
through active paths. This is numerical chain-rule propagation, not exact
symbolic calculus or a universal accuracy guarantee. Signed-zero/subnormal
finite derivatives remain valid. Seventy independent100-decimal-digit fixtures
use exact binary64 input identities and analytic mpmath formulas, not the
implementation derivative helper; see the TSV and provenance in d5-results.

See [D5_SCIENTIFIC_RULES.md](D5_SCIENTIFIC_RULES.md) for the exact local
numerical formulas, domains, activity and work policy.

## Symbolic mathematical algebra

`SymbolicAlgebra.hpp` deliberately owns a different mathematical-only DAG.
`ExactRational` uses checked Int64 numerator/positive denominator, cross-cancelled
arithmetic; it is not arbitrary precision. Exact import of a binary64 coefficient
can exhaust this rational range. Mathematical zero collapses executable signed
zero intentionally. Imported executable domains are retained as Defined
assumptions, never discarded by simplification.

`differentiate`, `simplify`, `expandPolynomial`, and `factorPolynomial` return an
explicit status, optional complete expression, work and relation. Derivatives
are conditional derivative identities; simplification/expansion/factorization
are conditional equalities. Neither relation asserts bit-exact executable
behavior. No expression is returned after an interrupted transformation.
Domain obligations include definedness, nonzero/positive arguments, open
inverse-trig interval and exclusion of the atan2 branch cut. Piecewise min/max/
clamp symbolic differentiation is explicitly unsupported; numerical partials
remain independently available under their local rules.

Polynomial collection/expansion supports multivariate rational arithmetic,
nonnegative metadata Powi and division by nonzero constants. Factorization is
a documented subset: univariate integer-root extraction in [-16,16], finishing
with a degree0/1 residual. Unsupported factorization returns Unknown, rather
than a fabricated proof. Common-subexpression identification uses full exact
structural keys, not unchecked hash equality. `compareSymbolicPolynomials`
returns equality under assumptions, distinct normal forms, unknown or resource
limit. Distinct polynomial forms do not prove pointwise inequality everywhere.

Limits bound nodes, DAG depth, transformation steps, variables, degree, terms,
assumptions, output bytes and accounted storage. Conservative logical storage
includes coefficient/map bookkeeping; system allocator overhead is not a
portable byte-exact guarantee. CSE and explicit conversion may throw the typed
resource exception. Cancellation is supported for differentiation, simplification, expansion and
factorization. CSE/export use typed resource/cancellation exceptions; operations
returning a transform result use explicit statuses.
`toExecutable` is a separate explicit export, may round rational coefficients,
retains assumptions and never silently rewrites evolutionary candidates.
Returned obligations are not automatically enforced; their node IDs refer to
the retained immutable `SymbolicConversion::assumptionContext`, rather than
the newly expanded executable tree.
Symbolic inspection text is not accepted by the executable scalar parser;
reconstruct with the standalone codec `genetik.symbolic-rational-dag/1`.
Recurrence and dimensional full-model inspection similarly use their model codecs,
not ordinary expression text parsing.

## Physical dimensions and constraints

`PhysicalDimensions.hpp` supplies exact seven-axis SI exponent vectors.
Rational exponents normalize to numerator magnitude<=4096, denominator<=64;
checked intermediates have explicit bounds. Addition/subtraction and min/max/
clamp require matching units; multiply/divide compose exponents; roots divide
exponents exactly. Logs/exponentials and unary trig require dimensionless
arguments (angles in radians); atan2 permits equal operand units and returns a
dimensionless angle. Dimensionful Pow requires a constant, exactly representable
bounded dyadic exponent; unrestricted exponent expressions require a
dimensionless base. Discrete comparisons require matching units, branches match,
binders inherit bound units, and variable-length products require dimensionless
bodies. Inference has node, traversal-step and logical workspace caps.

`DimensionalExpression` owns ordinary strict executable trees and copied units;
codec `genetik.dimensional-scalar-model/1` preserves units and node metadata.
`DimensionalRegressionProblem` checks units before evaluating a candidate or
fitting probe. `DimensionalVariation` uses a finite deterministic unit-witness
closure independent of observations and RNG, followed by typed productions.
This planner is deliberately incomplete; an unreachable output unit fails at
configuration time. Caps bound states, planning/generation steps and storage;
generation depth<=64. Crossover exchanges matching-unit subtrees. Mutation
creates a subtree with the selected unit. Bounded rejection retains a parent.
Evolution uses dimensionless coefficient terminals; manually constructed models
also permit explicit units for constant occurrences. `DimensionalConstantSelection`
freezes dimensionful constant exponents so fitting cannot change type.

`MathematicalConstraints` owns variable domains, positive/nonzero operator
arguments, nesting/complexity caps and optional sampled first-partial monotonicity.
A structural proof and sample-local success are distinct report fields.
Unavailable derivatives are not converted into a proof. `ConstrainedRegressionProblem`
and `ScalarMathematicsProblem::constrained` apply the same policy to every
training score/probe; constraint evaluation rows are counted separately from
ordinary residual-pass rows. Exhausted constraint work throws a resource outcome,
never a finite fitness penalty. Samples do not prove global positivity,
nonzero denominators or monotonicity.

## Evolution and checkpoint continuation

`ScalarMathematicsRun` adapts existing Population, GeneticAlgorithm,
TrainingObjective and ConstantRefinement; it is not a second GA/publication loop.
`ScalarMathematicsProblem` owns measured data and derives exact context identity
from those data, limits, grammar, dimensions/constraints, fitting configuration
and worker count. Each full objective/probe consumes existing loss accounting.
No default fitter is changed. Mathematical model metadata is immutable run
context; scalar or recurrence transition ASTs are the evolved genomes.

`step()` and all inspection/checkpoint access are coordinator-only while idle.
Codec `genetik.scalar-mathematics-evolution/1` restores full populations, cached
fitness, best-seen, RNG, attempted/published generations, preparation/variation
and fitting work. Existing scalar/discrete node IDs and default checkpoint
payloads stay compatible; new grammar/policy contexts reject incompatible
resumes. New model codecs are distinct. Same runtime/worker count/configuration
is required; this adapter currently requires the same total budget, with no
budget-extension mode. Use existing atomic save/retention/signal helpers; no
serialization occurs in signal handlers. Partial generations never become
committed checkpoints; consumed work and RNG are retained after failure.
Recompile dependent translation units consistently: header-only source
compatibility does not establish ABI compatibility.

## Optional arbitrary precision

Boost.Multiprecision `cpp_int` was evaluated against its documented backend
contract and an independent30! availability probe. The probe could not compile
because the optional Boost headers are not installed on this host; the original
compiler failure is retained. Its [official backend contract](https://www.boost.org/doc/libs/latest/libs/multiprecision/doc/html/boost_multiprecision/tut/ints/cpp_int.html)
provides arbitrary precision and configurable allocation. A mere fixed MaxBits
setting is not a complete allocation/work policy. A production arbitrary-precision
expression type, limb-aware resource limits, evolution and codec were **not
implemented or qualified**. Int64 is never silently promoted. Normal Genetik
builds retain no Boost dependency.

## Running

```sh
cmake -S . -B out/build/d5 -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/d5 -j2
out/build/d5/genetik_advanced_mathematics_quickstart --threads 2
out/build/d5/genetik_d5_iteration_study squares dependent 42 out/runs/d5/example
out/build/d5/genetik_d5_mathematical_benchmarks fibonacci 42 out/runs/d5/fib --threads 2
```

The installed quickstart builds with `Genetik::Genetik`, C++20 and CMake>=3.16.
Benchmark protocols, every positive/negative outcome and revision-specific local
qualification are recorded separately under `docs/d5-results/`. Finite training
agreement never proves a universal identity or unique function. No prime oracle,
new prime campaign, remote publication or D6 work is part of D5.
