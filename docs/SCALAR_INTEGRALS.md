# Bounded root integral models (Phase 7C)

Contract recorded before implementation. IntegralExpression owns N external bindings,
lower/upper ordinary Expressions of arity N, and an integrand of arity N+1. Its
explicit local index must be N. No binder enters Expression or its default grammar.
Copies detach buffers; immutable models support synchronous concurrent evaluation
with invocation-owned scratch. Inputs are borrowed for the call and never edited.

The planned proper-integral policy evaluates lower then upper, treats signed-zero
equal bounds as an empty interval (+0/error0/no samples), otherwise checks original
lower then upper endpoints and uses ascending GK7/15 panels. Reversal negates only
the complete estimate. Endpoint validity does not establish global domain validity.

Coefficients and the variation/magnitude/roundoff estimator design follow published
QUADPACK dqk15 (Piessens/de Doncker, 1980), https://netlib.org/quadpack/dqk15.f.
This is an independently written bounded C++ implementation, not a translated
Fortran runtime or a Boost dependency. See also Boost's Gauss–Kronrod discussion:
https://www.boost.org/doc/libs/latest/libs/math/doc/html/math_toolkit/gauss_kronrod.html.
Numerical table values and the published mathematical rule are attributed here.

Sample order: endpoints in original order; panel center; seven negative/positive
pairs in decreasing positive-abscissa order. No reuse: first complete panel17
samples, each completed binary split30 more. Refine largest estimated error,
leftmost panel on ties; children left then right; replace parent only when both
children and the recomputed partition totals are complete. Sum partition entries
left to right. Failure retains the last complete partition for diagnostics only.

Success means ESTIMATED convergence: error<=max(absTol,relTol*abs(estimate)), with
finite checked comparison arithmetic. The QUADPACK-style variation rescaling and
50*epsilon*L1 floor are heuristic safeguards, not a rigorous error bound. Native
binary64/default rounding/gradual underflow and qualified libm remain required.

Finite bounds may still lack resolvable interior coordinates: midpoint/half-width
and all15 distinct interior sample coordinates must be representable. Report
Resolution instead of silently integrating a collapsed nonzero interval as zero.
All quadrature arithmetic is checked; finite samples can produce NumericalFailure.

Limits bound integrand attempts, total expression arithmetic/node visits, quadrature
arithmetic, active panels, depth, model nodes/bindings and planned scratch payload.
Scratch includes panel capacity, two private child panels, binding vector, operand
stack and fixed panel sample storage; this is not a process/allocator-memory cap.
Whole expression plans are admitted before each ordinary interpreter call; actual
executed primitives/visited nodes are recorded. A rejected plan is not an executed
expression. Integrand attempt starts after cancellation/allowance admission and
before expression-plan admission. Unexpected allocation exceptions propagate.

Search evolves only the integrand through the existing restricted variation and
shared publication driver. Bounds/binding/policy/data are fixed context. Full-model
MSE is used for every score and coordinate probe; only integrand constants fit.
One whole training-objective request is one loss attempt. Mathematical bound/sample
invalidity and loss arithmetic yield Invalid fitness; numerical/resolution/
nonconvergence and resources throw classified integral exceptions, cancellation
uses EvaluationCancelled. Thus failed adaptive approximations cannot be published
as finite penalties or shortened local fits. Operational causes remain nested.

No nested/improper integrals, integral AST nodes, derivative-through-quadrature,
symbolic integration or full integral parser is provided. Export is descriptive;
only its three ordinary expression fields can be imported by ExpressionParsing.
After 7C the next checkpoint is combined calculus qualification and delivery.

## Public API

```cpp
#include <IntegralRegression.hpp>
#include <ExpressionParsing.hpp>
auto a=parseExpression("0",1), b=parseExpression("x0",1);
IntegralExpression model(1,a,b,parseExpression("2*x1+1",2),1);
auto prediction=model.evaluate(std::array{2.}); // accepted estimate approximately6
// Observations contain external inputs only. Candidate/fitter expressions have N+1.
auto problem=IntegralRegressionProblem::create(1,a,b,{{{1},1},{{2},6}});
IntegralTrainingObjective objective(problem,512);
ConstantFitConfig fitting; fitting.maxLossAttempts=512;
auto fit=fitConstants(parseExpression("1*x1+1",2),objective,fitting);
SymbolicRegressionConfig config;config.population=8;config.elites=1;
config.maxEvaluations=32;config.maxTrainingLossAttempts=32;config.maxGenerationAttempts=3;
RandomContext run(42,ExecutionConfig{2});
auto searched=integralRegression(problem,config,run);
```

The compiled installed example is examples/quickstarts/integrals.cpp. It also checks
an external-parameter-dependent integrand. Canonical commands, from the checkout:

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/release -j2
out/build/release/genetik_integrals_quickstart --threads 2
out/build/release/genetik_integral_campaign --output out/runs/integral-new --threads 1
# Bounded integration smoke; campaign seeds remain sequential:
out/build/release/genetik_integral_campaign --smoke --output out/runs/integral-smoke-new --threads 2
```

IntegralRestrictions can supply original-index variable and operator subsets. With
N=1, `variables=vector<size_t>{1}` restricts evolution to the LOCAL x1, not x0.
Omitted subsets allow all existing scalar variables/operators; default evolutionary
grammar remains +,-,*. Supplied expressions are validated too. The identity
whole-expression substitution context reuses the established bounded restriction
variation; its placeholder is not an initial population seed. The candidate is the
ordinary integrand Expression. Selection/reporting complexity is its node count,
then faithful text (the fixed model is identical for all candidates). Fitness identity
includes the immutable problem and run context; equal arities do not transfer scores.

Both bound and integrand node limits are checked before owning copies. Defaults:
model nodes65536, external count<=65536 (zero is supported); samples4096,
expression arithmetic1000000, node visits1000000, interpreter binding validations
1000000, quadrature units1000000, panels256, depth32, scratch8388608 bytes. Zero
execution allowances give resource outcomes; model/schema/shape errors throw before
execution. Tolerances are finite, abs>=0 and0<=rel<=1, not both zero. rel<=1 also
makes rel*abs(estimate) a finite comparison term; underflow cannot create false success.

Expression work is the existing checked primitives (opaque native math calls cost1).
Nodes and interpreter binding validations are charged separately; complete expression
plans are admitted conservatively, so an invalid early child does not gain an
unbounded plan. Quadrature units count checked add/subtract/multiply/divide, plus1
for midpoint or sqrt. Comparisons, copying and finite-input validation are bounded
by counts/sizes, not claimed as arithmetic instructions. Fixed rule coefficients
and 50*epsilon are precomputed. Samples count attempted integrand requests, even
when expression-plan admission or sampled arithmetic fails; denied sample requests
are not samples. Bounds count attempts separately. Peak workspaceBytes is a maximum,
never summed as arithmetic work. Scratch accounts for reserved panel capacity,
parent/two children, coordinate/value arrays, private bindings and operand capacity;
control objects, owned expression metadata, allocator overhead and reporting vectors
are not a process-memory cap. No references to scratch escape a call.

Ordinary MSE checks residual, square, row-ordered sum from+0, and division by row
count. Loss arithmetic units are a separate supplemental field. IntegralDataLimits
bound rows4096/input coefficients1048576; checked worst-case rows*per-integral
allowances cap samples16777216 and expression/node/binding/quadrature work4294967296
each before copying data. These are local problem bounds, not a new global search
budget. At most4096 bounded predictions are retained by an all-row assess call.
Data/scales/policy never change mid-run. measure stops at its first unsuccessful
row; assess inspects every row and returns status counts and per-row diagnostics.
A raw approximation after failure is never accepted. Reporting is separate from
training allowance/cached fitness.

IntegralStatus/IntegralReason distinguish estimated convergence, invalid bounds or
samples, nonfinite quadrature arithmetic, specific resources, roundoff nonconvergence,
collapsed/resolution intervals and cancellation. IntegralResourceExhausted and
IntegralEvaluationFailure retain these classifications during private preparation;
mathematical invalidity is ordinary Invalid fitness. Cancellation uses the existing
EvaluationCancelled mechanism and admission/draining. Fitter probes with invalid
samples are rejected while earlier improvements remain; resource/numerical stops
abort the attempt, never a shortened publishable fit. External callback side effects
cannot be undone by library publication guarantees. Operational causes remain nested.
Detailed refinement diagnostics retain their serial-only restriction. Parallel custom
objectives require explicit independent factories. No worker accesses coordinator RNG.

## Numerical rule and limitations

Panel center first, then seven negative/positive pairs (positive GK abscissae in
decreasing order); weighted sums accumulate in this order. Gauss uses the subset
of odd-indexed pairs plus center. Compute Kronrod/Gauss sums, weighted absolute
magnitude, and weighted absolute variation about half the Kronrod sum. Scale by
half-width. Rescale the absolute embedded difference using the variation and
min(1,(200*difference/variation)^1.5), with a saturation comparison before the
ratio to avoid overflow. The1.5 power is ratio*sqrt(ratio), checked. Apply the
50*epsilon*magnitude roundoff floor. There is no guarantee true error<=estimate.
Each refinement recomputes full partition value/error/floor in increasing interval
order, before publishing that partition. Roundoff-floor-dominated unmet tolerance
returns Nonconvergence. Resource bounds remain distinct outcomes.

Midpoint uses std::midpoint; half-width uses a direct finite difference when safe,
otherwise separate halves for opposite-sign extremes. Each panel requires15 distinct
strict interior binary64 coordinates; collapsed coordinates return Resolution.
A subnormal nonzero width whose half-width vanishes is not accepted as an empty
interval. Native expression underflow remains valid. No broader range arithmetic,
reassociation, protected values, FMA or automatic domain repair is introduced.

Concrete limitation: exp(-((t-0.123456789)*1000000)^2) on[0,1] underflows to zero at
all17 sampled points. The implementation reports estimate0/error0/estimated
convergence, although the true integral is at least2e-6/e (>7e-7), using the small
interval within1e-6 of its center. This intentionally retained fixture demonstrates
why the estimator is heuristic. No random probes or general singularity detection.

The discovery protocol is fixed in the campaign's PROTOCOL.txt before search:
seeds0,1,2,3,42, population128,9 nodes/depth4, local slot1 only,+,-,*, discrete
{-2,-1,0,1,2}, no fitting,10000 objective attempts,2000 generations, training MSE
threshold1e-20, and the exact policy above. Full configuration and all outcomes
will be retained in INTEGRAL_RESULTS.md. Training targets are x*x+x, not integrand
labels; all models are frozen by training loss/nodes/text before assessment. The
separate supplied linear-integrand fit uses bounds[-8,8], step1/min1e-8,128 sweeps,
8192 proposals,512 objective attempts and no early threshold. This supplies F(0)=0
structurally; it does not recover an arbitrary additive constant.
