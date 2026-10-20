# Installable development-package qualification

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
../.tools/bin/cmake -S . -B ../build-release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake -S . -B ../build-debug -DCMAKE_BUILD_TYPE=Debug -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake -S . -B ../build-sanitize -DCMAKE_BUILD_TYPE=Debug -DGENETIK_SANITIZERS=ON -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure
../delivery-tools/cmake-min/bin/cmake -S . -B ../build-min-contracts -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../delivery-tools/cmake-min/bin/cmake --build ../build-min-contracts -j2
../delivery-tools/cmake-min/bin/cmake -E chdir ../build-min-contracts ../delivery-tools/cmake-min/bin/ctest --output-on-failure
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
