# Phase7C local integral qualification

Start: clean feature/derivative-regression at
`99238f35dea45849d9bf2e11a471fcbefbc90a81`. Work remains local on
feature/scalar-integrals. Implementation/campaign revision:
`2ed3724f3c034d2338fa86dd4d20e1d6801c8f56`. The installed-quickstart fixture control
fix is `a4bc2c0d6e2e334ddaef26f4c5d8c011bf722594`; it changes no library arithmetic,
search policy or work accounting. Final documentation/evidence commits do not alter
these implementations. Campaign reporting fix6e8322e reuses the counted fitter
baseline and separates verification-report work; buffer fix
`afbb03d654f4402689d017d07de3741183488d2a` initializes the private N+1 binding
payload without geometric append growth. Final completion SHA is recorded in ignored out/runs/integrals/.
No remote writes, main/tag/version/release modifications or new capability phases.

## Architecture and contract coverage

Optional IntegralExpression.hpp owns bounds/integrand, N external variables and
appended local N. No ordinary AST or default grammar changes. IntegralRegression
uses Phase7B's polymorphic mathematical measurement seam and the existing whole-
expression restricted variation, coordinate kernel, preparation, executor and
publication driver. There is no second search loop, optimizer or matrix worker pool.
Only new shared-header seam is a protected const executionContext getter on
TrainingObjective for cooperative cancellation during bounded sample work. Generic
GA/OOP headers have no new mathematical dependency. Header-only dependent TUs must
be recompiled consistently, as required since Phase7B; no stable ABI is claimed.

Checked fixtures cover:

* Independent analytic proper integrals t², exp(t),1/(1+t²),sqrt(t),log(t),
  constants, affine functions, cancellation, original/reversed bounds and empty
  signed-zero bounds. Full integral MSE2.5 for g=2 with (1,1),(2,6) observations,
  whereas accidental pointwise scoring would give8.5.
* Original external/local slot independence (x*t gives x³/2), zero-external-input
  schema, binding errors/nonfinite input, owned copies/buffers, signed-zero and
  Powi inspection/export and ordinary-field parser round trips. Full model export
  is descriptive, not an accepted integral text parser.
* Invalid lower/upper/endpoints/interior samples; finite sampled MAX overflowing
  checked quadrature; opposite-sign extreme/same-sign large interval geometry;
  adjacent representable and subnormal-width/collapsed-coordinate outcomes.
* Exact sample accounting17+30*k,16/17/18 initial-panel boundaries;32 samples
  interrupt a right-child split after a completed left child, retaining parent
  estimate/error bits, one active panel and zero published splits. Operation,
  node, binding, quad and planned-payload boundaries are checked below/at/above.
  Panel/depth/node/storage limits use checked size arithmetic before allocation.
* Roundoff-floor nonconvergence at an unattainable tolerance. Deliberately missed
  narrow exponential peak demonstrates estimate0/error0 despite known nonzero
  integral; no random probing, singularity proof or actual-error-bound claim.
* Supplied linear coefficients and matching full-model loss; one-sweep accepted
  progress followed by a domain-invalid square-root probe; integration resource
  interruption throws instead of returning invalid fitness or a publishable local fit.
* Immutable problem/fitness context, genome invalidation, reporting/cache separation,
  copied input rows, restriction/grammar validation before RNG. Bounds/local index
  remain frozen through fits; original local index1 is never remapped to0.
* Nonbinding1/2/4-worker comparisons preserve raw/prepared populations, expression/
  fitness, old and supplemental counters, fitting cadence, variation, history,
  stopping, generations and final RNG. Mathematical acceptance never depends on
  stochastic optimum discovery. Campaign recovery is separate evidence.
* Actual two-worker overlap: shared immutable model with distinct external inputs
  and parameter-dependent/reversed bounds; private fitting reaches a strictly
  better private model before a sibling throws/cancels/resources. Draining,
  original cause, actual starts, unchanged committed genomes/fitness/generation/
  archive/RNG and same-generation retry are checked. Tiny global cap9, actual
  integration operation exhaustion and all-invalid endpoint populations reuse the
  established engine paths. Bounded allocation fault sweeps retain consumed work.

## Independent numerical provenance

Published coefficients and error-estimation design: QUADPACK dqk15, Piessens/
de Doncker,1980, https://netlib.org/quadpack/dqk15.f. Source was inspected; mathematical
constants/formulas are attributed, no Fortran or Boost implementation/runtime was
copied. The handwritten C++ code remains MIT, standard-library-only. See the API
contract for exact sample/sum/refinement/error-estimation order and work units.

The independent generator scripts/generate_integral_references.py uses standard
Python Decimal80/120-digit exp/ln and Machin's identity for pi, with alternating
arctangent series. Results agree to60 digits; rounded binary64 fixtures are tested
with absolute tolerance2e-10 for these well-conditioned known cases. That comparison
tolerance is declared, not an assumed theorem about the estimator. The production
integrator never generates its own oracle. Selected reference identities/results
are under integral-results/reference/; development source download and raw runs
remain under out/runs/integrals/reference/.

## Execution environment and commands

Jetson Orin NX native ARM64 Ubuntu24.04.4, GCC13.3/GNU13 headers, libstdc++/libtsan14.2,
glibc/libm2.39; default rounding and gradual underflow. Actual package versions,
compiler/architecture/CMake commands and per-mode flags are retained with evidence.
CMake/CTest4.4.4 is ../.tools/bin; exact3.16.0 remains ../delivery-tools/cmake-min/bin.
Public ordinary-tool commands:

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release \
  -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
cmake --build out/build/release -j2
cmake -E chdir out/build/release ctest --output-on-failure
# Repeat in out/build/debug with Debug;
# out/build/sanitize with Debug and GENETIK_SANITIZERS=ON;
# out/build/minimum using exact CMake/CTest3.16.0.
# Separate TSan: out/build/tsan Debug, GENETIK_THREAD_SANITIZER=ON,
# GENETIK_SANITIZERS=OFF, GENETIK_BUILD_EXAMPLES=OFF, recorded reference OFF.
```

Strict-FP and private warnings-as-errors/sanitizers stay enabled. The current
configured manifest has53 local tests in each normal build,52 portable tests;
only the documented Jetson-specific genetik_evaluation_reference snapshot is
excluded from portable CI. Typed/scientific/parser/template/derivative/value-partial
and integral contracts, CLI, package and generic OOP isolation remain enabled.
Exact JSON manifests/names, not a guessed count, are checked by check_ci_manifest.py.

Separate TSan selections (each3 repetitions): genetik_concurrency_contracts,
genetik_oop_parallel, genetik_typed_overlap, genetik_typed_preparation_overlap,
genetik_scientific_contracts, genetik_parsing_overlap, genetik_template_overlap,
genetik_derivative_overlap, genetik_derivative_regression_overlap,
genetik_integral_overlap. Per-name repetition counts are retained. Forced new
overlap preflight and the complete integral contract executable also pass TSan.
ASan+UBSan is separate; no suppressions, weakened assertions or dropped tests.
The authored hosted workflow includes the new coverage; hosted calculus jobs have
not executed. Windows/MSVC, AppleClang/macOS and libc++ remain unqualified.

Installed checks use ordinary and relocated prefixes with spaces, remove disposable
producer trees and compile two overlapping TUs through Genetik::Genetik. Both TUs
exercise the new APIs; fitted integral scoring and parallel search run downstream.
The actual installed quickstart supports --threads2. Header spelling/layout,
Genetik/genetik_lib targets, Threads dependency, C++20/strict-FP propagation, parent
source inclusion, versions/components, C++23, mixed C and private-runtime checks remain.
No new mandatory numerical dependency or source-tree include/runtime leakage.

## Retained development failures and repairs

The first fixture compile deduced float from INFINITY in std::array; explicit
numeric_limits<double>::infinity fixes the type. The first fitting assertion read
postfix index4 (Add) as a coefficient; the correct independent intercept is index3.
No numerical expectation changed. Allocation-fault probing initially spent its
positions parsing the supplied expression before objective entry; constructing that
input first and using a bounded64-position sweep exercises failures after actual
integral admission, without relying on an exact allocation count. Original diagnostic
logs remain in out/logs/ and selected evidence.

Package fixture review removed the new quickstart from the list lacking a --threads
CLI (a4bc2c0). Installed parallel execution is verified explicitly; no library change.
The initial Debug suite passed52/53 but hygiene caught a manually generated
`tests/__pycache__/package_integration.cpython-312.pyc` from syntax-only preflight.
That generated bytecode was removed, the original failure retained, syntax checking
rerun with PYTHONPYCACHEPREFIX=out/tmp/python-cache, and the affected gate refreshed.
AGENTS/ORGANIZATION document cache-safe preflight. No blanket cleanup or suppression.

## Completion scope

The frozen five-seed campaign and separate115-attempt fit are in INTEGRAL_RESULTS.md.
No historical discovery campaign is retuned or repeated. Prior frozen evidence and
published assets stay unchanged. Full source gates, final affected gates, serial replays, typed
compatibility, preservation and selected-evidence hashes are recorded below. The next checkpoint is combined calculus hosted qualification and final
development-delivery review; no further mathematical capability precedes it.

Final review found assign(N)+push_back could grow private binding capacity beyond
its planned N+1 payload when N>1. afbb03d uses resize(N+1) then copies externals;
the independent N=2 fixture integrates x0*t+x1 over0..x0+x1 at(2,3) to40 and
checks55 binding validations. Arithmetic/sample/RNG rules and experiment outcomes
are unchanged. All affected integral tests, headers, installed/relocated consumers
and TSan selections are rebuilt/refreshed in every applicable mode. Unaffected
legacy tests/replays are reused; final gate records distinguish full suites from
these focused post-fix checks.

The original campaign record remains at2ed3724. It did one additional unfitted
measurement and one accepted-score verification outside the fitter objective;
these were reporting work, each8 integrals with per-integral4096-sample allowance
(not extra coordinate probes). Current app6e8322e removes the duplicate initial
measurement by using ConstantFitResult.startingLoss, and records actual bounded
verification work separately. Four-configuration smoke checks verify unchanged
115 probe/baseline attempts, recovered coefficients and score. Research runs are
not repeated and original runtime observations are not relabeled as later timings.



## Executed results and exact scope

| Gate | Full configured suite | Final affected integral/package/hygiene gate |
| --- | --- | --- |
| Release/CMake4.4.4 |53/53,338.38s |6/6,282.45s |
| Debug/CMake4.4.4 |53/53,275.11s after cache repair |6/6,286.00s |
| Exact CMake/CTest3.16.0 Release |53/53,261.97s |6/6,206.25s |
| Debug ASan+UBSan |53/53,884.06s |6/6,324.73s |
| Separate TSan |10 selections x3 pass |refreshed after afbb03d;4.49s, full integral executable also passes |

Suite times include compilation inside isolated package fixtures and overlap other
qualification work; they are not performance comparisons. All53 names and the final
six names per mode are retained in results.json/logs. Full-suite records qualify the
shared library changes from2ed3724; final buffer/app/fixture corrections additionally
receive focused rebuilds of both new standalone headers, tests/apps/quickstart and
all affected consumer gates. The remaining47 contracts have unchanged dependencies.
Debug's first52/53 hygiene failure remains retained, rather than relabeled passed.
Release/minimum initially omitted only the optional installed quickstart thread2
command; that command was run explicitly and all final consumer gates include it.
No suite timeout or sanitizer instrumentation was changed.

Three accepted serial replays (prime cadence1, strict arithmetic cadence1/4) match
pre-edit captures byte-for-byte: initialization, expressions/fitness bits, generations,
counters, histories, stops and final RNG. Typed fitting/search data, protocol,
frozen models and results match the accepted Phase7B capture excluding runtime.
Comparison hashes are in qualification/compatibility.json; raw captures remain
out/runs/integrals/compatibility/. Old research campaigns were not repeated.

Independent known-integral results: t²1/3 (17 samples), exp1.7182818284590451
(17), rational.78539816339744828 (47), sqrt.66666666666670216 (587),
log1..2 .38629436111989057 (17). Maximum difference from the rounded independent
reference is3.5527136788005009e-14 (sqrt), below the declared2e-10 fixture tolerance.
Estimated errors are reported separately, not asserted as rigorous bounds. The
independent full-model loss fixture returns2.5,34 samples,7 loss-arithmetic units.
Complete reference values/observations are in integral-results/reference/.

All127 frozen historical hashes, original campaign records, published source archive/
checksum identities, main and the annotated v0.1.0 object remain unchanged.
Preservation/source identities and SHA256SUMS identify selected new evidence.
Actionlint, exact portable/TSan manifests and workspace hygiene/self-test pass.
Selected JSON is whitespace-normalized; raw captures stay under canonical out/.
No remote writes occurred. Hosted qualification is pending for this calculus stack,
not inferred from earlier jobs. Next: combined calculus hosted qualification and
final development-delivery review; implementation scope is finished.
