# Installable development-package qualification

Current feature hosted results: [TYPED_SCIENTIFIC_HOSTED_QUALIFICATION.md](TYPED_SCIENTIFIC_HOSTED_QUALIFICATION.md).
Earlier release results: [HOSTED_QUALIFICATION.md](HOSTED_QUALIFICATION.md). The
original packaging and preflight sections below are historical checkpoint records.

Starting point: clean accepted workspace milestone
`d7b6352ee30f6283cd07cba25847b4e8f49511aa`. No unrelated edits or local release
tags were present. The existing project version was 0.1.0; it remains the single
root CMake authority and is explicitly unreleased. Existing MIT license/copyright
are unchanged. No push, tag or publication was performed.

## Delivered interface

Package **Genetik**, target **Genetik::Genetik**, source compatibility target
**genetik_lib**. Header-only INTERFACE, stdlib/runtime only. Existing include spelling
is preserved under include/Genetik, including the required detail/ScopedTiming.hpp.
GNUInstallDirs, target export, configure_package_config_file and SameMinorVersion/
ARCH_INDEPENDENT version generation provide a relocatable development package.
No campaigns, data, tests, executables or measurement artifacts install by default.

Source and installed targets provide cxx_std_20 and strict C++ FP requirements;
newer consumer standards work. Installed Config selects the **consumer** compiler/
frontend and guards options by C++ language, avoiding producer-selected GNU flags
on another frontend or on C sources. The imported target file itself contains only
relative-prefix includes and C++ features; Config applies the shared FP helper.
Development sanitizer/warning/Werror options are directory-private. Parent compiler,
standard, extensions, build type and CTest ownership remain intact; embedded extras
and installation default OFF. See PACKAGING.md for options/layout and API.md for use.

## Executed local qualification

Jetson Orin NX, ARM64 Ubuntu/Linux 6.8.12-1021-tegra, GCC/libstdc++ 13.3.0,
glibc/libm 2.39. Workspace CMake/CTest 4.4.4 and **exact CMake/CTest 3.16.0**.
An installation produced by 4.4.4 was also consumed successfully with 3.16.0
(two translation units and both quickstarts), verifying generated-file backward
compatibility as well as same-tool installation. CMake minimum was not raised; 3.16.0 was built locally from the official source,
without source patches, after checking the published SHA256:
`6da56556c63cab6e9a3e1656e8763ed4a841ac9859fefb63cbe79472e67e8c5f`.
The tool's bootstrap used CMAKE_USE_OPENSSL=OFF/BUILD_TESTING=OFF; no Genetik public
dependency was added. No other local C++ compiler was available.

| Configuration | Result | Observed full-suite time |
| --- | --- | ---: |
| GCC13 Release, CMake 4.4.4, warnings as errors | 21/21 | 71.82 s |
| GCC13 Debug, CMake 4.4.4, warnings as errors | 21/21 | 106.47 s |
| GCC13 Debug ASan+UBSan, CMake 4.4.4, warnings as errors | 21/21 | 220.79 s |
| GCC13 Release, exact minimum CMake 3.16.0, warnings as errors | 21/21 | 60.13 s |

These are qualification wall times under varying concurrent build load, not
performance comparisons. All configurations explicitly enabled the qualified
captured reference in addition to portable contracts. Standalone headers, strict-FP
negative compilations, old source consumer, new isolated package, compiled quickstarts,
character/file/prime smoke workflows and numerical/failure/budget tests all pass.
No long stochastic search or timing campaign was rerun. The character demo remains
bounded at 1,000 generations/100,100 calls with best .892.

The optimized warnings-as-errors gate exposed GCC13's mismatched-new/delete
heuristic when inlining the existing **test-only** malloc/free replacement allocator.
Keeping replacement delete entry points out of line resolves the diagnostic without
suppressing it. The allocator's matched family and fault/accounting tests pass under
ASan. No public headers, interpreter, fitness, RNG, fitter or search logic changed.

## Isolation and compatibility evidence

The current-source disposable fixture passed on both CMake versions and all three
standard configurations. It installs with producer development sanitizer/Werror ON,
then builds **uninstrumented** normal and relocated consumers. After moving A to
`relocated prefix B`, disposable producer source/build trees no longer exist; the
active checkout is preserved. Registries are disabled, searches constrained to B,
and resolved Genetik_DIR must be in B. Compiler include roots come only from the
installed prefix; no producer paths or development flags are present in metadata.

Two C++ translation units include overlapping complete public APIs and execute
scalar evolution, strict arithmetic, loss, fitting and bounded multivariate search.
A C translation unit verifies language-specific flags. The consumer starts from
CMAKE_CXX_STANDARD=11 and relies on Genetik for C++20. A separate C++23 consumer
passes with CMake 4.4.4 (3.16 predates that standard setting). Actual linked binaries
were inspected with ldd: no ASan/UBSan/profiling runtime was introduced. A source
parent keeps C++14 on an unrelated executable and retains its BUILD_TESTING value.

Installed quickstarts compile/run from the same checked-in files used by the README.
Observed scalar output: seed 42, ten generations, 176 calls, normal generation stop.
The symbolic program reports its actual bounded outcome/expression; exact discovery
is not an assertion. Operational failures still exit unsuccessfully.

Version metadata is also populated without requesting a version.
Version resolution: **0.1** and **0.1.0 EXACT** accepted. **0.0.9**, **0.2.0**,
**0.1.1** (newer than installed) and **1.0** rejected. Required component `imaginary`
rejected with the explicit no-components message. Flag-selection probes check GNU,
GNU-style Clang, clang-cl and MSVC-style branches, but simulated branches do not
qualify compilers/platforms that have not executed.

A focused Release package run (103.89 s under concurrent build load) also
checks linked runtime dependencies after that supporting inspection was added.
Full Debug, sanitizer and minimum suites include that inspection. After making
version metadata explicit for requests with no version, final focused package
checks passed in Release, Debug, sanitizer and minimum builds (93.70, 94.75,
95.44 and 78.37 s); those figures are not runtime performance comparisons. Raw suite logs,
installed manifest/metadata, actual compile commands, source-independent fixture
commands and checksums are in delivery-results. Previous prime/arithmetic/cadence/
workspace campaign artifacts remain unchanged.

## Reproduce locally

Public commands use ordinary cmake/ctest (README). On this Jetson:

```sh
../.tools/bin/cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build out/build/release -j2
../.tools/bin/ctest --test-dir out/build/release --output-on-failure
../.tools/bin/cmake -S . -B out/build/debug -DCMAKE_BUILD_TYPE=Debug -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build out/build/debug -j2
../.tools/bin/ctest --test-dir out/build/debug --output-on-failure
../.tools/bin/cmake -S . -B out/build/sanitize -DCMAKE_BUILD_TYPE=Debug -DGENETIK_SANITIZERS=ON -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build out/build/sanitize -j2
../.tools/bin/ctest --test-dir out/build/sanitize --output-on-failure
../delivery-tools/cmake-min/bin/cmake -S . -B out/build/minimum -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../delivery-tools/cmake-min/bin/cmake --build out/build/minimum -j2
../delivery-tools/cmake-min/bin/cmake -E chdir out/build/minimum "$PWD/../delivery-tools/cmake-min/bin/ctest" --output-on-failure
```

To build the minimum tool: download cmake-3.16.0.tar.gz and its SHA-256 listing
from https://cmake.org/files/v3.16/, verify the hash above, extract into the build
workspace, then bootstrap in a separate directory:

```sh
../cmake-3.16.0/bootstrap --prefix="$PWD/../cmake-min" --parallel=4 -- -DCMAKE_USE_OPENSSL=OFF -DBUILD_TESTING=OFF
make -j4
make install
```

README gives exact install/external quickstart/source-inclusion commands.
PACKAGING.md gives the isolated-fixture-only command. Development tests require
Python 3.9+; ordinary library configure/install/consume does not.

## CI status and release boundary

.github/workflows/ci.yml configures three jobs: Ubuntu 24.04 x64 GCC13 Release
with exact minimum CMake, x64 Clang18 Debug ASan+UBSan, and native Ubuntu 24.04 ARM64
GCC13 Release. Compiler/architecture/version/stdlib/runtime checks are logged, actions
are pinned to verified official tag commits, jobs have a 30-minute timeout and
read-only repository permissions, and failure logs are retained. The minimum job builds the same checksum-pinned
official 3.16.0 source used locally (no assumption that PyPI publishes 3.16.0).
YAML parsed locally.

**No hosted job has executed.** This is configured CI, not verified hosted support.
Clang, x86-64, Windows/MSVC, AppleClang and libc++ qualification remains pending as
specified in SUPPORT.md. Captured Jetson bits default OFF in CI; portable arithmetic,
domain, within-build deterministic and isolated-package tests remain required.
The next release-readiness task is executing/reviewing the hosted matrix and its
actual compiler/runtime evidence, followed by the documented release checklist.
No remote push, release tag or publication belongs to this milestone.

## Subsequent OOP/layout qualification

The next milestone re-exercises this package interface with an external permutation
candidate, no symbolic includes, repaired virtual selection dispatch, and organized
application/tool sources. See [OOP_QUALIFICATION.md](OOP_QUALIFICATION.md) for final-
tree commands/results; earlier delivery measurements above remain historical.
Quickstart sources now reside in examples/quickstarts; package names/layout are unchanged.

## Subsequent CPU backend delivery

Threads::Threads is now a transitive platform requirement and GenetikConfig invokes
find_dependency(Threads) before exports. The package is still header-only, with
Execution.hpp installed under include/Genetik and no architecture-specific binary.
Ordinary/relocated multi-TU consumers and installed generic/symbolic/OOP parallel
work pass, including exact CMake 3.16.0. No dev sanitizer runtime leaks. See
CONCURRENCY_RESULTS.md for the new separate evidence and current local TSan status.

## Hosted qualification preflight (authorization pending)

The accepted CPU backend at `a9f28f6898c9ec83bb9d98a6ddf9369405153f18`
remains the local numerical/performance reference. No hosted results are claimed.
Read-only inspection of public `karlphillip/Genetik` found only remote main at
`b8059f08ececa9bda6b2c19f35a925c070052f56`, with no hosted workflows. Therefore
uploading the proposed `qualification/hosted-0.1.0` branch exposes the complete
modernization ancestry, not just the backend. The generated exact-candidate/range/
command record lives under ignored `out/hosted-preflight/PREFLIGHT.md`.

The workflow's unrestricted push trigger is sufficient for the new branch; no
pull request, default-branch update or workflow_dispatch is required. The matrix is:

- x86-64 Ubuntu 24.04, GCC 13 Release, checksum-pinned CMake/CTest 3.16.0.
- x86-64 Ubuntu 24.04, Clang 18 Debug, ASan+UBSan.
- native ARM64 Ubuntu 24.04, GCC 13 Release, plus a separate Debug TSan build.

All jobs explicitly enable examples/tests and disable captured Jetson references.
`scripts/check_ci_manifest.py` requires all 24 portable CTest cases, including
installed/relocated multi-TU scalar/symbolic/permutation consumers, virtual OOP,
worker limits, budgets, public file/character workflows and strict FP checks.
Standalone headers compile in the normal build. The separate TSan filter must
select exactly the two instrumented overlap/OOP fixtures. Missing/extra tests fail
qualification rather than silently reducing coverage. Runtime/image/compiler,
standard-library headers and installed DSO package versions are logged together
with the exact checkout SHA. Each job must pass; setup, sanitizer initialization,
missing runners, skips and transient failures are not successful qualification.

The preflight adds no production/API change. It adds a regression check that
parallel detailed diagnostics fail before initialization, preserving the requested
concurrency, RNG and zero work; serial diagnostics remain supported. Local checks
and their logs are recorded in the generated preflight. Earlier full local gates
remain in CONCURRENCY_RESULTS.md, including the fixed-work performance evidence.

Remote upload/execution awaits the specific approval described in the preflight.
After approval, qualify each newly pushed fix SHA, retain failing run logs, and
record job URLs/toolchains/conclusions here and in SUPPORT.md. Keep 0.1.0 unreleased;
no default-branch update, tag or release asset publication is included.

Preflight checks executed locally on the existing qualified Jetson/GCC13 runtime:

- actionlint 1.7.12: workflow passes; official action tag SHAs rechecked read-only.
- Focused new concurrency contract: Release/Debug/ASan+UBSan pass; separate TSan
  concurrency + OOP fixtures pass 2/2, with instrumented overlap and no suppression.
- Fresh source copy containing tracked inputs plus the proposed manifest checker,
  exact CMake/CTest 3.16.0, Release/Werror, captured reference OFF: **24/24 pass**
  (74.89 s), including isolated normal/relocated installed consumers and source
  inclusion. The full build compiles standalone headers and all public workflows.
- Both actual test manifests pass validation; synthetic missing/extra manifests
  are rejected. All 127 historical evidence checksums remain unchanged.

These are local preparation results, not hosted qualification. Generated raw logs,
remote refs, full upload commit inventory and exact candidate are retained under
out/hosted-preflight/. No research campaign or runtime benchmark was repeated.


## Authorized hosted execution

The user approved the complete preflight upload range and targeted fixes on
qualification/hosted-0.1.0. The SSH-authenticated development-branch push triggered
real jobs. Workflow [39477303495](https://github.com/karlphillip/Genetik/actions/runs/39477303495)
passed all three jobs for `23914b8502dfe4665bf65a2a6e7fa94c8d8e3654`, including
minimum CMake, ASan+UBSan, separate ARM64 TSan, installed/relocated multi-TU consumers
and external OOP/symbolic paths. See HOSTED_QUALIFICATION.md for exact runtime/job
records and the mapping from local 25 cases to hosted 24 plus two TSan executions.
Concrete repairs were explicit scalar bounds-check result discards for Clang Werror
and supported artifact staging/bounded diagnostic reporting; no arithmetic or
execution policy was changed. Original failures remain recorded.

The final documentation-only candidate receives a fresh push-triggered matrix;
completion reports its exact tip SHA and run. No default-branch update, history
rewrite, tag or release publication occurred. Version remains 0.1.0 unreleased.


## Current workspace convention

Fresh qualification now uses `out/build/{release,debug,sanitize,minimum,tsan}`;
installed fixtures and disposable producer paths stay under those build trees.
For manual installation use `out/install/` and `out/consumers/`. Historical commands
and evidence retain their original paths; see [CLEANUP.md](CLEANUP.md). The local
CMake tool locations are deliberately unchanged. Source copies for future independent
work should be managed worktrees, not untracked checkout duplicates.


## Additive typed-expression delivery

Phase 2 installs TypedValue.hpp/TypedExpression.hpp through the existing header-only
package, with no new public dependency or binary. The actual typed quickstart and
overlapping typed-header use across both installed translation units pass ordinary
and relocated source-free consumption, including exact CMake 3.16.0. Generic OOP
consumption remains independent of these mathematical headers. See
[TYPED_EVALUATION_QUALIFICATION.md](TYPED_EVALUATION_QUALIFICATION.md) for local gates,
commands, final test-only refinement, and the unexecuted hosted scope. No remote
release/delivery action occurred in this milestone.

## Accumulated typed/scientific hosted delivery

Workflow39582456985 at `778a30dd0c05fe74a0755dcadbe59bed7e4ebc32` passed
33/33 portable cases on GCC13 x64/exact-CMake3.16, Clang18 x64/ASan+UBSan,
and native GCC13 ARM64; separate ARM64 TSan passed five cases three times each.
All three jobs executed installed/relocated multi-TU, scalar/symbolic/typed/fitting
and nonmathematical OOP consumers. Exported target/header spelling, strict-FP and
thread requirements and private development flags remain unchanged. See the new
feature report above for audited compiler commands, test mapping and limitations.
These results are subsequent development qualification; published v0.1.0, its
assets, main and tags remain unchanged. The report commit gets its own full matrix,
with final SHA/links recorded under out/runs/hosted-typed-scientific/COMPLETION.md.

## Phase 5 parser delivery (local)

The package installs ExpressionParsing.hpp, TypedExpressionParsing.hpp and their
private transitive lexer/parser header through the existing include/Genetik layout.
Canonical target, C++20/strict-FP/Threads requirements, minimum CMake3.16 and
private development instrumentation remain unchanged. Installed/relocated multi-TU
consumers and the actual parsing quickstart pass in Release/Debug/ASan+UBSan/minimum
builds. Generic permutation dependency probes exclude parser and mathematical
headers. See [PARSING_QUALIFICATION.md](PARSING_QUALIFICATION.md) for exact source
identities, manifests, commands, limits and the pending Phase5 hosted scope.
