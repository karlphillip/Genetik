# D6 scientific discovery (local development)

Genetik remains a generic C++20 genetic-algorithm library. These optional headers
are mathematical representations and scientific objective adapters; the GA core
has no scientific type dependency. No historical grammar, optimizer, mutation,
prime objective, package version or checkpoint node identity is changed.
Recompile dependent translation units consistently: the additive polymorphic
measurement seam changes header-defined interfaces and does not establish ABI
compatibility. D5's negative discovery results remain unchanged.

## Shared outcomes and work

`ScientificWork.hpp` supplies `ScientificStatus`, `ScientificLimits`,
`ScientificWork`, and `ScientificResult<T>`. Success owns a complete finite value;
Invalid, NumericalFailure, ResourceLimit, Nonconvergence, Cancelled and
Unavailable remain distinct. Configuration/shape/nonfinite caller inputs throw
`invalid_argument`; allocation and operational exceptions propagate. Constructors
can throw `ScientificResourceExhausted`. Native-call numerical failure is not a
finite penalty. No partial tensor, solve or system score becomes successful.

Limits bound arithmetic dispatch/intermediates, elements, logical workspace and
iterations; cancellation is polled at charged work. Standard-library native
functions count as one opaque mathematical call, not their implementation's
machine instructions. LAPACK uses a declared conservative admission charge
(4n^3+4n^2 solve, 32n^3 eigensystem), followed by explicitly charged residual
arithmetic; this is a work policy, not a measured internal flop count. Its call
cannot be cooperatively interrupted. Dimensions1..16 bound that blocking domain.
Logical workspace includes operand/output buffers; allocator metadata, installed
backend resident memory and caller-owned immutable data are not byte-exact RSS
bounds. System/model construction separately caps owned observation storage.

Each complete training objective invocation, including a fitter probe, consumes
one existing loss attempt. `TrainingObjective::supplementalWork()` records actual
charged mathematical operations and field evaluations, separately from loss,
row and fitting counters. `RegressionProblem::measureWithWork` gives optional
worker-owned accounting; old custom measurements continue dispatching through
`measure`. Built-in scientific failures record completed work before throwing.
No shared mutable evaluation scratch, reporter thread, executor or RNG is added.
`ScalarMathematicsRun::supplementalWork()` exposes these counters for new full-model
runs. Legacy D5 run payloads deliberately do not gain new fields.

## Coupled systems and differential observations

`ScientificEquationSystem` owns external schema, equations, shared-parameter
slots, channels, observations, limits and optional dimensions. Bindings are
external `[0,N)` followed by explicit shared parameters `[N,N+P)`. Reusing a
parameter slot means genuine shared identity; repeated literal constants are
still independent constant occurrences. All construction buffers detach,
including caller rvalues. Equations and public views are immutable.

Each `ScientificChannel` identifies one equation and owns nonempty rows, positive
finite scale and weight. Traversal follows supplied channel order, then row order.
The score is the sum of normalized squared residual means times normalized channel
weights. Residuals are divided by scale before squaring. The positive finite
weight sum and nonzero normalized weights are validated; no enabled channel is
silently omitted. Failed observations invalidate/interrupt the complete objective.
A channel's sample count does not implicitly change its relative weight.
`ScientificAssessment` retains normalized channel errors and actual field/work
counts; these are diagnostics, not predictions after failure.

`fitScientificParameters` jointly scores the entire system using the existing
coordinate kernel, deterministic plus/minus probes and per-parameter bounds.
A temporary owned scalar coefficient pack carries parameter coordinates; its
ordinary scalar value is never used as a prediction. The returned parameter
vector and loss refer to the same accepted complete system. No gradient or
alternative optimizer is introduced.

`scientificSystemProblem(system,index,limits)` evolves one selected equation's
ordinary expression while every equation/channel is scored. Other equations and
explicit shared parameters are fixed context in that structural run. Its ordinary
constant occurrences fit against the full objective. This initial adapter does
not simultaneously evolve an arbitrary vector of unknown ASTs; shared-parameter
fitting and selected-equation structural evolution are separate workflows.

`ObservedOdeSystem` records time input, state inputs and corresponding RHS equation
indices, rejecting duplicate roles. Optional SI declarations check RHS units
against state/time. This is identification from observed state/derivative pairs,
not trajectory integration. The oscillator uses t,x,v and two shared parameters:
x'=v; v'=-d*v-k*x, with independent reference d=.25,k=2.

`ObservedPdeResidualSystem` records field identity and ordered derivative indices
(order0..2) for supplied observations. Heat residuals bind u_t,u_xx,u and kappa,
using u_t-kappa*u_xx plus prescribed initial/boundary value channels. Reference
u=exp(-.25t)sin(x) gives u_t=-.25u and u_xx=-u. The field is supplied rather than
reconstructed; fixed initial/boundary data are not evidence of general PDE solving.
Dimensions, domain validity, numerical success and physical validity are separate.

## Second partials

`ExpressionSecondDerivative.hpp` provides `evaluateSecondPartial(e,x,i,j,limits)`.
It completes checked ordinary primal evaluation first, then propagates a
hyperdual (d_i,d_j,d_i d_j) iteratively. The complete primal bits match ordinary
evaluation. A later primal error takes precedence over derivative unavailability.
Structural target activity is independent of derivative zero. Active singular
sqrt and abs corners are conservatively unavailable; inactive unary subtrees
contribute zero derivatives. Each required intermediate is checked.

Qualified rules: add/subtract/multiply/divide, nonnegative/negative metadata Powi,
log/log1p/log2/log10, exp/expm1, sin/cos, sqrt, sinh/cosh/tanh, abs away from zero,
and min/max away from ties. General Pow, atan2, other inverse trig and ternary
rules currently return Unavailable. Mixed partials are qualified on this subset;
there is no full Hessian or finite-difference fallback. Nonfinite derivative
arithmetic yields Unavailable with the completed primal retained. Resource failure
retains diagnostic primal only when completed. Numerical chain rules provide no
universal derivative accuracy guarantee or global differentiability proof.

## Linear algebra

`AdvancedLinearAlgebra.hpp` accepts existing owned `TypedValue` vectors/matrices.
Vector norm uses sequential hypot (avoids gratuitous square overflow); squared
norm, normalize (zero invalid), 3D cross, trace, partial-pivot determinant and
integer matrix powers0..32 are standard-library bounded operations. Reductions
and multiplication use checked ordered arithmetic, not reassociation/FMA.

Square solves and real-symmetric eigenpairs use optional **LP64 LAPACK**, maximum
16x16. Pivoting uses DGESV; DSYEV supplies ascending eigenvalues and column
orthonormal eigenvectors. Independent Ax-b and Av-lambda*v residuals are returned
and checked against declared tolerances. A small backward residual does not prove
forward accuracy in an ill-conditioned problem. Repeated eigenvectors are not
unique and their signs are arbitrary. Symmetry is checked exactly at import.

`symmetricMatrixExponential(A,t)` uses Q*diag(exp(t*lambda))*Q^T. Only real symmetric
matrices are supported; general nonsymmetric exponentials are not implemented.
Convergence here means eigensolver/residual acceptance and finite arithmetic,
not a rigorous exponential error bound. Independent diagonal and 2x2 fixtures
qualify this restricted domain.

The base package has no new dependency. Opt in at build with
`GENETIK_SCIENTIFIC_NUMERICS=ON`, or installed
`find_package(Genetik 0.4 COMPONENTS ScientificNumerics REQUIRED)` and link
`Genetik::ScientificNumerics`. `Genetik::Genetik` stays independent of LAPACK.
Without the optional target, solve/eigen/exponential return Unavailable.
Only the local Linux LP64 OpenBLAS/LAPACK configuration is qualified. Set
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1` before launching applications: numerical
backend parallelism is outside Genetik's executor and must not create nested pools.
CMake locates local libraries; no runtime network access or downloads occur.
LAPACK/OpenBLAS licensing/provenance is retained in the qualification record;
no library source is copied. D5 optional arbitrary precision remains deferred.

## Tensors

`BoundedTensor.hpp` owns finite binary64 row-major values and `TensorShape` with
rank0..3 (rank0 is a scalar tensor, distinct from `TypedValue::scalar`). Positive
extents and checked shape products obey element/byte limits before allocation.
Checked access, add/subtract, scalar scale, outer product, one explicit paired-axis
contraction and selected-axis sum reduction are supported. Remaining axes are
A's in original order followed by B's. For A[i,j,l],B[l,k], contract axes2,0 gives
C[i,j,k]. Multiple contractions/reductions require explicit successive calls and
caller aggregate allowance; there is no unrestricted Einstein network language.

`tensorScaleRegressionProblem` is the initial additive typed adapter. An evolved
scalar coefficient expression predicts an entire supplied tensor via scale;
fitness is mean squared error across all components and observations. It does not
derive pointwise scalar labels from tensor targets. General tensor-AST structural
evolution is not implemented; bounded scale-structure evolution and coefficient
fitting are supported through the existing engine.

## Complex mathematics

`ComplexExpression.hpp` owns validated postfix complex ASTs. Explicit IDs0..13
cover variables/constants,+,-,*,/,conjugation,magnitude,exp,log,sqrt,sin,cos,Powi0..8.
All operands are binary64 complex values; Magnitude explicitly embeds the real
absolute value as (r,+0). It is not an implicit conversion to real fitness.
Division by complex zero and log(0) are invalid. Every intermediate/component
must be finite; underflow and finite signed zeros are allowed. Log and sqrt use
qualified `std::complex` principal branches, including signed-zero sides of the
negative real cut. Native complex operations may suffer intermediate overflow;
there is no universal error bound or complex AD engine.

`parseComplexExpression` reads bounded canonical prefix syntax with finite c(re,im)
constants; depth<=64, bounded bytes/nodes. Export does not fold/reassociate trees.
Unicode and LaTeX are presentation, not executable serialization.
`checkpointComplexExpression`/`restoreComplexExpression` preserve component bits,
operator/exponent metadata, limits and branch identity. Model codec:
`genetik.complex-expression/1`. Tensor model codec: `genetik.bounded-tensor/1`.

`complexRegressionProblem` scores checked real nonnegative mean |prediction-target|².
It uses an explicitly complex-interpreted scalar topology/real-coefficient genome
for the supported common arithmetic/exp/log/sqrt/sin/cos/Powi subset. Original input
slots hold complex observations (including fixed i where declared); no scalar
prediction or imaginary-part discard is used. This initial evolutionary profile
fits real coefficient occurrences, not arbitrary independent real/imaginary
constant pairs. The standalone complex AST and codec accept full complex constants.

## Finite statistics and probability

`FiniteStatistics.hpp` supports mean, variance with explicit degrees of freedom,
covariance, correlation, finite nonnegative weighted mean and discrete expectation.
Empty data and dof>=N are API errors; population variance uses0, sample variance1.
Correlation of zero variance is invalid; all-zero/negative/nonfinite weights are
rejected. Probability mode requires sum1 within declared tolerance. Positive
weights that vanish under normalization fail numerically. Two-pass variance uses
mean terms x/N before summing, followed by checked centered squares; numerically
unrepresentable variance still fails rather than clipping. This is deterministic
finite-data arithmetic, not stochastic sampling or Bayesian inference.

Opt-in scalar operators append stable IDs31..38:
`erf`, `erfc`, `gamma`, `log_gamma`, `bessel_j0`, `normal_pdf`, `normal_cdf`,
`poisson_pmf`. Normal functions bind (x,mu,sigma), sigma>0; every residual/z/square
intermediate is checked. Poisson binds (k,lambda), integral0<=k<=1e6 and
0<=lambda<=1e6; lambda0 gives mass1 at k0, else0. Positive-lambda mass uses the
log-domain formula k*log(lambda)-lambda-lgamma(k+1). Finite underflow is valid.

Gamma/log-gamma initially require positive arguments; poles/negative arguments
are unsupported domains. Bessel is explicitly order0, x in[0,1e6]. Native
std::erf/erfc/tgamma/lgamma/cyl_bessel_j are finite-checked; unexpected native
operational exceptions propagate. Scalar native calls have the same qualified
binary64/strict-FP/runtime policy as D5. Parser/export, Unicode/LaTeX, optional
grammars, ordinary constant fitting and append-only scalar node codecs all apply.
Erf/erfc numerical first derivatives are implemented. Gamma/log-gamma/Bessel/
distribution derivatives explicitly return unavailable on active paths; valid
primal values survive. Their symbolic derivatives and second-partial rules are
not implemented. No typed vector/matrix operator set is silently expanded.

## Evolution, storage and reproducibility

`ScientificRegression.hpp` factories bind owned complete problem contexts to
`ScalarMathematicsRun`. The generic engine and transactional publication loop are
reused. Mathematical invalidity returns invalid fitness; resource/cancellation
abort/drain preparation; numerical nonconvergence/failure throws the typed/nested
scientific failure rather than a fabricated finite loss. Accepted coordinate
improvements and global admission follow the established fitting contract.

New full-model runs use `genetik.scientific-measurement-evolution/1`, reusing the
explicit scalar candidate codec for topology/coefficient bits. Identity includes
all observations, scales/weights, equations, parameter/domain/shape/branch policy,
limits, grammar, fitting, seed/workers and evolution settings. The payload includes
committed population, archive, cached fitness, RNG and all training and new
supplemental counters. Restore requires matching runtime, worker count and total
budget; it does not support budget extension. Cross-domain/task/shape/grammar
resumes are rejected. Existing v0.4/D5 run payloads and scalar operator IDs0..30
are unchanged; old readers reject new operator IDs rather than reinterpret them.
Atomic fsync/rename/retention and cooperative signal machinery are reused without
serialization in signal handlers. No ODE solver, field reconstructor, formal
physical-law proof, general complex fitter or general tensor network is claimed.

## Build and examples

```sh
cmake -S . -B out/build/d6 -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/d6 -j2
out/build/d6/genetik_scientific_discovery_quickstart
out/build/d6/genetik_d6_scientific_benchmarks oscillator 42 out/runs/d6/example
```

The optional backend requires `-DGENETIK_SCIENTIFIC_NUMERICS=ON`; base quickstart
also runs without it and explicitly reports its unavailable backend. All actual
frozen outcomes, failures and revision-specific qualification are recorded in
`docs/d6-results/`. No historical campaign is replaced or retuned.

## Reference provenance

The optional algorithms call the published [LAPACK DGESV](https://netlib.org/lapack/explore-html/d8/da6/group__gesv_ga831ce6a40e7fd16295752d18aed2d541.html)
and [DSYEV](https://www.netlib.org/lapack/explore-html/d8/d1c/group__heev_ga8995c47a7578fef733189df3490258ff.html)
interfaces rather than reimplement general solvers. Complex branch contracts follow
the [C++ complex transcendental specification](https://eel.is/c++draft/complex.transcendentals).
`tests/d6-scientific-references.tsv` is generated independently with mpmath at100
decimal digits from exact binary64 inputs; it never calls Genetik. The Poisson
reference relation is also documented by [Boost.Math](https://boostorg.github.io/math/doc/html/math_toolkit/dist_ref/dists/poisson_dist.html).
No third-party implementation code was copied into the headers.
