# Genetik 0.5.0 D7 local qualification summary

## Candidate and host

- Branch: `qualification/advanced-mathematics-d7`.
- Tested source and test-configuration commit: `20d2e3e0db9fbd312b16ba0d9fe632ff51459786`.
- D6 qualified implementation: `c72eaaa3f1ca817ba804d36321069e58ac288e6f`.
- Benchmark implementation/protocol identities: `5a95ea47f983d4ba99704fb9ea955b14264d6741` / SHA256 `b0b07d2ae418ca71b024fbaa1374264348d92f2b8290606efda2969a432f1ffd`.
- Host: NVIDIA Jetson AGX Orin, Ubuntu 24.04.4, Linux 6.8.12-1021-tegra, aarch64 Cortex-A78AE, 8 online cores; GCC 13.3.0. CMake 3.28.3 except the exact-minimum run using CMake 3.16.0. Recordings are in `out/logs/d7/`.
- Qualification gates used the clean committed implementation at the tested SHA. Later candidate changes are documentation, benchmark reporting/tooling and this report; no executable library or test-configuration file is changed after the tested SHA.

## Engineering gates

The portable manifest was derived from CTest JSON at the tested candidate and
checked with `scripts/check_ci_manifest.py`. It contains 90 tests.

| Gate | Result | Evidence |
|---|---:|---|
| Release | 90/90 passed, 442.41 s | `final-release-build.log`, `final-release-ctest-rerun.log` |
| Debug | 90/90 passed, 497.01 s | `final-debug-build.log`, `final-debug-ctest.log` |
| Exact CMake 3.16.0 Release | 90/90 passed, 342.06 s | `final-minimum-build.log`, `final-minimum-test-manifest.json`, `final-minimum-ctest.log` |
| ASan+UBSan full portable suite | 90/90 passed, 4506.15 s | `final-sanitize-build.log`, `final-sanitize-test-manifest.json`, `final-sanitize-ctest.log` |
| TSan forced overlap/concurrency | 21 selections, each repeated 3 times; passed | `final-tsan-overlap-repeat3.log`, `final-tsan-selected-test-manifest.json` |
| TSan complete integral contracts | 1/1 passed | `final-tsan-integral-contracts.log` |
| TSan complete discrete/checkpoint contracts | 4/4 passed | `final-tsan-discrete-contracts.log` |
| TSan complete D5 contracts | 6/6 passed | `final-tsan-d5-contracts.log` |
| TSan complete D6 scientific/continuation | 2/2 passed | `final-tsan-d6-contracts.log` |
| Optional LP64 LAPACK installed consumer | 1/1 passed using system OpenBLAS LAPACK | `final-lapack-build.log`, `final-lapack-optional-package.log` |
| Workflow YAML/pin lint | Passed; push and pull-request triggers, 3 matrix jobs, 2 SHA-pinned actions | `workflow-lint.log` |
| Repository hygiene self-test | Passed | `final-hygiene-selftest.log` |
| Frozen historical evidence | All 127 inventory hashes match | `historical-evidence-integrity.log` |

Strict floating-point policy was compiled with `-fno-fast-math -ffp-contract=off`;
the `genetik_expression_fp_policy` test passed in Release, Debug, minimum-CMake
and sanitizer suites. Standalone public-header, multi-translation-unit, generic
OOP, installed/relocated consumer, checkpoint/replay, repository hygiene and
D1–D6 mathematical contracts are included in the 90-test portable manifest.
The existing `lgamma`/`signgam` race correction remains in the shared expression
kernel: `threadSafeLogGamma` serializes `std::lgamma`, and the D6 TSan scientific
and concurrency selections passed.

The earlier D6 qualification driver's parent-workspace hygiene condition still
fails on the user-owned `/home/karlphillip/workspaces/primeResearcher/session.md`.
That file is outside the repository and was not modified. Repository and
build-output hygiene are independently checked and passed; the D7 driver does
not claim the parent directory is repository-owned or clean.

## Mathematical correctness and search results

Reference-tested scope and limitations are detailed in
[`../D7_NUMERICAL_QUALIFICATION.md`](../D7_NUMERICAL_QUALIFICATION.md).
The independent integer oracle covered 3,652 rows; frozen mpmath references
cover D5 scalar functions/partials (70), D6 special functions (32) and
probability functions (21). Matrix/eigen/exponential, tensor, supplied-field
PDE, complex, statistical and checkpoint contracts are included in D6 tests.
These establish selected mathematical behavior and numerical references, not
universal equivalence or evolutionary recoverability.

| Campaign | Held-out outcome | Interpretation |
|---|---:|---|
| D5 discovery | 17/24 | Fractional power 0/3; trigonometric 0/3 |
| D6 discovery | 4/12 | Tensor 3/3, Gaussian 1/3, oscillator 0/3, complex 0/3 |
| D6 fixed-structure fits | 3/3 | Each recovered its reference fixture; structure was supplied, so these are not discoveries |
| D7 conventional SR matrix | 108 attempts | Genetik: 14 processes completed, 22 timed out; 30 emitted expressions, 20/30 held-out threshold hits. PySR: 36 timeouts, 36 HOF snapshots assessed secondarily, 31/36 threshold hits. PyOperon: 36 timeouts, no final candidate returned |

All three successful D6 tensor discovery runs were generation-0 fitted initial
populations. Complex seed 0 retained an initialization failure. D7 clean-track
Genetik results were 0/3 for rational, trigonometric and fractional-power
targets; noisy-track outcomes use a different declared threshold. No result
supports an outperformance claim. Full per-seed D7 scores, task definitions,
metrics and caveats are in [`BENCHMARK_REPORT.md`](BENCHMARK_REPORT.md) and
`results/benchmark-seed-results.jsonl`.

## Compatibility and limitations

The candidate retains C++20, CMake >=3.16, `Genetik::Genetik`, established
generic GA defaults and generic OOP use cases. Generic checkpoint loading stays
exact-version; named readers cover the documented published v0.3 scalar and
v0.4 scalar/discrete contracts. Deterministic continuation requires matching
dataset/configuration/operator/type/resource identities and supported runtime
contracts.

Arbitrary-precision integer support is incomplete and unqualified. There is no
general numerical ODE/PDE solver, complete Hessian, unrestricted tensor-network
or arbitrary-vector AST evolution, general complex constant fitting, universal
symbolic equivalence, or qualified prime-formula discovery. Optional LAPACK is
LP64 and dependency/platform-specific. The benchmark is one ARM64 host with
three seeds and a 120-second budget; all primary PySR and PyOperon cells timed
out, and PySR HOF scores are secondary timeout diagnostics.

## Release assets and remote boundary

After the final documentation commit, build the source archive from that exact
commit and record its SHA256 in `out/release/` and the approval packet. Hosted
qualification is proposed on this exact branch tip using the repository's
pinned workflow. The user authorized pushing this qualification branch after
local completion; no PR, main integration, tag or publication is authorized.
The branch push may trigger normal `on: push` CI; no manual dispatch or remote
report write is included.
