# D6 local qualification

## Qualified source and scope

The completed D6 executable implementation is commit
`c72eaaa3f1ca817ba804d36321069e58ac288e6f`
(`Serialize log-gamma math calls during parallel evaluation`). Its source archive
was verified against all 1,396 tracked file blobs. The final D6 documentation
commit follows it and changes documentation/evidence only; it does not change
`genetik/`, `tests/`, `CMakeLists.txt`, or executable sources. Thus the D6
implementation at the final docs tip is byte-for-byte the qualified c72 source.

This is local qualification on the recorded Linux aarch64/GCC 13/glibc 2.39
environment. It does not claim hosted qualification, universal numerical
accuracy, or a published release. The exact qualification records and selected
raw logs are retained in [`d6-results/qualification/`](d6-results/qualification/).
The original TSan race and failed run are kept there alongside the passing
rerun; the original `out/logs/d6/` records remain untouched.

## Engineering gates

Every listed suite was run against source SHA c72. The Release, Debug, minimum
CMake, and sanitizer CTest logs report all tests passed:

| Configuration | Result | Retained log |
|---|---:|---|
| Release, CMake 4.4.4 | 90/90 | `qualification/c72eaaa3f1ca/release-full.log` |
| Debug, CMake 4.4.4 | 90/90 | `qualification/c72eaaa3f1ca/debug-full.log` |
| Exact minimum CMake 3.16.0 | 90/90 | `qualification/c72eaaa3f1ca/minimum-full.log` |
| ASan + UBSan, separate build | 89/89 | `qualification/c72eaaa3f1ca/sanitize-full.log` |
| TSan selected repeated matrix | 21/21, each repeated 3 times | `qualification/c72eaaa3f1ca/tsan-repeated.log` |
| TSan complete contract matrix | 13/13 | `qualification/c72eaaa3f1ca/tsan-complete.log` |

TSan includes forced D6 overlap with concurrent Poisson PMF and `log_gamma`,
checkpoint compatibility, D5 scientific/dimension contracts, D6 scientific
contracts, and deterministic D6 continuation. The repair serializes Genetik's
`lgamma` calls because this qualified glibc `lgamma` implementation updates
process-global `signgam`. The pre-repair TSan report remains retained as
`qualification/initial-tsan-race.log`; the driver failure is retained as
`qualification/initial-tsan-driver-failure.log`.

The 90-test matrices include installed and relocated consumers, optional
scientific-numerics package consumers, generic OOP and parallel OOP contracts,
and D5/D6 contract tests. Build instrumentation records strict floating-point
flags `-fno-fast-math -ffp-contract=off`; ASan+UBSan and TSan were built
separately. Workflow lint passed.

The full qualification driver exited at its final workspace hygiene check with
`unexpected parent-workspace entry: /home/karlphillip/workspaces/primeResearcher/session.md`.
That 21 MB parent-level session artifact is outside the repository and was not
modified, removed, or committed. Repository-only `python3
scripts/check_hygiene.py` passed, as did `git diff --check`. Therefore all
implementation, build, test, packaging, sanitizer, and workflow gates passed;
the workspace-wide hygiene gate remains an environmental exception, not a pass.
The original driver result is preserved in `qualification/c72eaaa3f1ca/gates.json`
and `hygiene.log`.

## Scientific correctness and numerical validation

Deterministic D6 scientific contracts passed in each ordinary build and in the
TSan contract matrix. Independent fixtures cover a 2x2 linear solve and symmetric
eigenproblem, symmetric matrix exponential, rank-3 tensor contraction expanded
as independent dot products, analytic heat-field partials, and finite-data
statistics. Thirty-two special-function reference values were generated with
mpmath at 100 decimal digits from exact binary64 inputs, independently of
Genetik. Probability reference cases are retained in
[`references/probability.tsv`](d6-results/references/probability.tsv).
These validate the declared cases and domains; they are not a global proof of
mathematical correctness or accuracy.

## Frozen discovery and fitting outcomes

The frozen protocol and all run records are in [`d6-results/`](d6-results/).
There were 15 total runs: 12 evolutionary discovery runs and 3 fixed-structure
fits. Held-out success is defined by the frozen threshold MSE `<= 1e-12`:

| Discovery family | Held-out successes |
|---|---:|
| Tensor scaling | 3/3 |
| Gaussian | 1/3 |
| Oscillator | 0/3 |
| Complex | 0/3 |
| **Total** | **4/12** |

All three tensor outcomes were fitted initialization models at generation zero,
not structural discoveries through later evolution. The Gaussian success was
seed 0. Complex seed 0 failed during initialization before a population commit;
the failure and drained-work diagnostics are retained. The two completed
complex runs did not meet the held-out criterion. All three fixed-structure
fits recovered their reference fixtures: oscillator damping/spring parameters,
heat coefficient, and Gaussian mean/standard deviation.

These outcomes keep four claims separate: deterministic contracts and independent
references assess mathematical/numerical correctness; fixed-structure fitting
assesses parameter recovery when the model is supplied; evolutionary discovery
assesses structure search; and held-out MSE assesses generalization on the
declared samples. None implies the others.

## Implemented D6 capability and boundaries

| Capability | Locally implemented and qualified | Declared boundary |
|---|---|---|
| Coupled scientific equation systems | Owned equations, channels, shared parameter slots, normalized residual objectives, joint parameter fitting, selected-equation structural evolution | Structural evolution selects one unknown equation at a time; no joint arbitrary-vector AST evolution |
| ODE residuals | Identification from supplied state/derivative observations, optional unit checks | No trajectory integration or ODE solver |
| PDE residuals | Supplied field and derivative observations, heat residual and value channels | No field reconstruction or general PDE solver |
| Higher-order derivatives | Checked second partials using hyperdual chain rules on a documented operator subset | No full Hessian, symbolic derivatives, finite-difference fallback; unsupported active rules return unavailable |
| Advanced linear algebra | Bounded vector/matrix operations; optional LP64 LAPACK solve, real-symmetric eigenpairs and symmetric matrix exponential through 16x16 | Optional local Linux LAPACK/OpenBLAS is qualified; nonsymmetric eigensystems/exponentials and universal error bounds are not supported |
| Bounded tensor operations | Owned rank-0..3 tensors, checked arithmetic, outer product, one paired-axis contraction and axis reduction, scale regression | No unrestricted tensor network or general tensor AST evolution |
| Complex expressions/regression | Complex AST evaluation, parsing/checkpoint codec, supported real-coefficient topology regression | No general independent real/imaginary constant fitter; no complex AD; native intermediate overflow remains possible |
| Statistics/probability | Finite mean/variance/covariance/correlation, weighted mean and discrete expectation; normal and Poisson operators | Deterministic finite-data math, not sampling or Bayesian inference; declared domains/limits apply |
| Special functions | `erf`, `erfc`, gamma, log-gamma, Bessel J0, normal PDF/CDF and Poisson PMF | Active derivatives are unavailable except erf/erfc first derivatives; no symbolic or second-derivative rules for these operators |
| Mathematical constraints | Bounded dimensions/domain/schema checks and explicit units rejection constraints | No general constructive dimensional planner or formal physical-law proof |
| Evolution integration | Shared GA, fitting, budgets, work accounting, transactional preparation/publication and cancellation semantics | Scientific model contexts are immutable per run; no new optimizer or general vector-structure search |
| Checkpoints | Explicit D6 measurement, tensor and complex codecs; context/runtime/worker/budget compatibility checks; deterministic continuation | No budget extension; earlier incompatible experimental D6 context versions are rejected |

D5 scalar/discrete operator identities and v0.4.0 checkpoint/package payloads remain
compatible; the ordinary 90-test suites and TSan checkpoint compatibility test
passed. New D6 operator IDs and codecs are additive. LAPACK remains optional and
`Genetik::Genetik` does not acquire a dependency on it.

## Limitations and next milestone

The qualification is platform-local and uses the declared native runtime math
library. LAPACK calls are blocking and only conservatively charged; native
internal operations are opaque to work counters. Small residuals do not guarantee
forward accuracy for ill-conditioned systems. The campaign has weak recovery in
oscillator and complex discovery, and the retained complex initialization failure
is part of the result. The workspace-wide hygiene check needs the unrelated
parent `session.md` artifact resolved by its owner before that whole-workspace
gate can pass; the repository itself is cleanly auditable.

D6 completion does not authorize publication. D7 — Competitor Benchmarking,
Hosted Qualification and Development Release — remains a separate milestone and
has not been started.
