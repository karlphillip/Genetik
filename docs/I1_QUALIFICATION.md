# I1 — Qualification record

## Identity and scope

- Starting source: published Genetik 0.5.0, `f95a204e25a6de36a54230be0cba750bde5e2ff7`.
- I1 adds an optional pybind11 extension, a Python-independent C++ regression facade,
  sklearn estimator/package files, and an SRBench adapter. Existing production C++
  implementation headers and tests are unchanged.
- No remote writes, release assets, tags, PRs, or publication are part of this work.
- Frozen configuration and provenance: `i1-results/protocol.json`.

## Environment

See `i1-results/host.txt`. Qualification host: Jetson Orin NX, Ubuntu 24.04.4,
Linux AArch64, Cortex-A78AE (8 cores, 15.97 GiB RAM), GCC 13.3.0, Python 3.12.3.
The tested package dependency versions and full environment lock are retained in
`i1-results/requirements-qualified.txt`. The source/core CMake minimum is exactly
3.16.0. No Windows/macOS or other Python ABI claim is made.

## Checks and evidence

Exact commands and raw logs are retained under `i1-results/logs/`. The first
CMake 3.16 optional-extension configure attempt used `Development.Module`, a
component unavailable in CMake 3.16; that failure is preserved. The optional
binding was corrected to request the compatible `Interpreter` and `Development`
components, then configuration and extension compilation succeeded with CMake
3.16.0. The initial wheel compile also exposed a facade mismatch against the
actual `Fitness::state()` API; it was corrected and the final wheel compiled.

The qualification matrix below is finalized alongside the logs. Native source
and tests remain unchanged from the qualified 0.5.0 baseline; I1-specific
verification is separately identified from inherited release qualification.

| Gate | Scope | Result / evidence |
|---|---|---|
| Optional extension build | CMake 3.16.0 Python-enabled Release build; standard C++ configure with Python disabled | Passed; `logs/cmake316-python-build.log`, `logs/core-no-python-configure-cmake316.log` |
| Python tests | 11 focused tests: conversion, fit/predict, determinism, parallelism, errors, SymPy and native parity | Passed; `logs/python-tests-qualified-final.log` |
| SRBench layout/import | Official checker and official import test at pinned upstream SHA | Passed: 31 algorithm/45 method directories; import 1 passed, 5 deselected; `logs/srbench-layout-final.log`, `logs/srbench-import-final.log` |
| SRBench local evaluator | Generated local data through official evaluator, bounded 3-second smoke, seed 42 | Passed; `logs/srbench-evaluate-smoke-qualified.log`, `srbench-smoke-results.json`; not a campaign |
| Package relocation | Fresh isolated venv, execution from outside repository, native import/fit/predict | Passed; `logs/clean-wheel-smoke-qualified.log` |
| Native Release | Exact CMake 3.16.0 full 60-test run originally had 59/60 because new I1 layout was not yet allowlisted; after explicit hygiene-scope update, all other 59 tests passed and hygiene passed separately (1/1) | Passed as combined evidence, not a single uninterrupted 60/60 run; `logs/release-ctest-cmake316-corrected.log`, `logs/release-ctest-cmake316-after-hygiene.log`, `logs/hygiene-ctest-repaired.log` |
| Native Debug | Targeted strict arithmetic, fitting, symbolic, multivariate, concurrency, generation, generic OOP and checkpoint contracts | 12/12 passed; `logs/debug-target-ctest-final.log` |
| ASan+UBSan | Targeted arithmetic, fitting, symbolic, expression, OOP, concurrency, typed and checkpoint contracts | 10/10 passed; Python extension fit/predict smoke passed with ASan runtime preloaded and leak detection disabled; `logs/asan-target-ctest.log`, `logs/asan-python-smoke.log` |
| TSan | Targeted forced-overlap concurrency, generic OOP, typed, parsing, template and checkpoint contracts | 7/7 passed; `logs/tsan-overlap-ctest.log` |
| Installed C++ consumers | Full Release run included installed/relocated consumer contract | Passed in the original run (282.77 seconds); not rerun after hygiene-only policy update; `logs/release-ctest-cmake316-corrected.log` |
| Hygiene | Explicitly recognizes I1 package directories and preserves pre-existing D7 `out/release` and `out/source-archive-check`; rejects unclassified output in self-test | Self-test and CTest passed. Workspace audit is separately recorded; external `session.md` was not changed. `logs/hygiene-self-test-repaired.log`, `logs/hygiene-ctest-repaired.log` |

## Native/Python equivalence

The fixed fixture is specified in the protocol. A standalone C++ reference executable
uses the same Python-independent facade and production `symbolicRegression` API.
The binding run compares raw and prepared initial expression sequences, selected
expression, final RNG state, training loss, objective/row/fitting/candidate counters,
committed generations, and predictions. The final checked result is recorded in
`i1-results/native-equivalence.json`.

All seven parity fields in that artifact are exact matches: raw and prepared
initial populations, selected expression, RNG state, authoritative counters,
training MSE and predictions. This is one fixed deterministic fixture, not a claim
of universal cross-implementation behavior beyond the shared facade/API path.

## Sanitizer limitations

The optional extension can be built with Genetik sanitizer flags. Importing an
ASan-instrumented extension into an ordinary Python process may require preloading
the compiler ASan runtime; meaningful Python-runtime sanitizer coverage is claimed
only if the qualified invocation and its evidence are recorded here. Core sanitizer
and TSan claims continue to be tied to their exact build/test configurations.

## Scope and qualification limits

The full CMake 3.16 Release run initially passed the installed consumer and all
but the repository-hygiene test. That failure was caused by the newly added I1
package paths not yet being permitted by the explicit repository layout policy.
After correcting that scope, the other 59 CTest tests passed and the hygiene test
passed separately (1/1). This combined evidence is not described as one uninterrupted
60/60 run. Debug, ASan+UBSan and TSan runs were targeted selections, not complete
suite reruns. The installed/relocated C++ consumer passed in the original Release
run and was not repeated after the hygiene-only change.

The hygiene self-test and repository hygiene CTest pass. The required parent
workspace audit reports only the external user-owned
`/home/karlphillip/workspaces/primeResearcher/session.md`; that file was preserved
unchanged. The audit result is retained at `i1-results/logs/hygiene-workspace-final.log`.

Python qualification is limited to Python 3.12.3/Linux AArch64 on the recorded
host. The final wheel and source archive were each installed into isolated
environments and exercised from outside the checkout. The source archive was
built twice byte-identically and contains the SRBench adapter while excluding
`out/`. Full sklearn estimator checks, full SRBench benchmarking, Windows/macOS,
and other Python/platform ABIs were not qualified. The Python API does not expose
checkpoint save/resume; native checkpoint contracts are unchanged.
