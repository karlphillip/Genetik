# D7 checkpoint A — integration audit

## Repository and baseline

- D7 branch `qualification/advanced-mathematics-d7` starts at `a05b3974ec636f5921184a90a2a06c3d7dbc84d5`.
- Published tag `v0.4.0` points to `c08859e462210a1ef378b0c6c47a2b4c520f7353`; this is an ancestor of the D7 start. The qualified D6 implementation `c72eaaa3f1ca817ba804d36321069e58ac288e6f` is also an ancestor. No changes were made to the tag, published archives, `main`, or remote refs.
- The working tree was clean at audit start. There were no active Genetik test, build, qualification, or benchmark processes.
- The source currently reports `GENETIK_VERSION_STRING "0.4.0"`. The project already contains D5 and D6 as additive APIs and documents `Genetik::Genetik`, C++20, CMake 3.16, and the optional `Genetik::ScientificNumerics` LP64 OpenBLAS/LAPACK component.

## Source dependency and API boundary

The base target's only discovered external link dependency is `Threads::Threads`. The optional numerical target finds LP64 OpenBLAS LAPACK; no other math dependency is required by `Genetik::Genetik`. Public API headers remain the header-only C++20 package. Existing packaging includes relocated/installed consumers and generic OOP fixtures. D5/D6 APIs are additive; the documented limitations in `ADVANCED_MATHEMATICS_D5.md`, `D5_SCIENTIFIC_RULES.md`, and `SCIENTIFIC_DISCOVERY_D6.md` remain release boundaries. In particular, arbitrary precision is incomplete, LP64 LAPACK is optional/local-qualified, ODE/PDE support is residual/observation-based, second derivatives are a subset, tensors are rank 0–3 bounded operations, complex fitting is restricted, and no formula-discovery success is claimed for prime numbers.

## Existing evidence (not rerun)

- D5 final implementation source pin `5e44bcfaf83e908d0b79a5eb7b23031fffa00284`: Release 84/84, Debug 84/84, exact CMake 3.16.0 84/84, ASan+UBSan 83/83, 20 TSan selections repeated 3 times, and 11 TSan contracts. Full records and historical failures are retained under `docs/d5-results/`.
- D6 implementation source pin `c72eaaa3f1ca817ba804d36321069e58ac288e6f`: Release 90/90, Debug 90/90, exact CMake 3.16.0 90/90, ASan+UBSan 89/89, selected TSan 21 x3 and 13 TSan contracts. The `lgamma`/`signgam` race repair is part of this qualified source. Logs and the original race are retained under `docs/d6-results/qualification/`.
- The aggregate D6 qualification wrapper failed its final `--workspace` hygiene gate on parent `session.md`. Repository hygiene passes. The script's `WORKSPACE` allowlist and `--self-test` explicitly establish that unexpected parent entries fail the workspace contract; current `out/` contents are all role directories. Do not change or delete the user-owned file. Report this environmental exception accurately in D7.

## Checkpoint A defect to repair

The version remains 0.4.0 while preparing a new 0.5.0 package. The scalar and
discrete checkpoint readers currently enable the 0.3.0 import only when the
current package version equals 0.4.0. A mechanical bump to 0.5.0 would drop the
existing 0.3.0 forward-import contract and fail to add the published 0.4.0
producer contract. Before a release candidate is frozen, add narrow read
compatibility for 0.4.0 scalar/discrete checkpoint codecs while preserving the
existing 0.3.0 compatibility; keep generic checkpoint reading exact-version.
Test an actual checkpoint written by a producer built from the published 0.4.0
source, full continuation state, wrong codec/runtime rejection, and unchanged
0.3.0 import. Do not weaken dataset, grammar, worker, numerical runtime, or
resource-limit identity checks. Do not reinterpret D6 experimental checkpoint
contexts as published 0.4.0 contracts.

## Environment and competitor acquisition feasibility

Host: Ubuntu 24.04.4 LTS, Linux 6.8.12-tegra, aarch64 Cortex-A78AE, 8 online
CPUs, GCC 13.3.0 and Python 3.12.3. Workspace CMake 4.4.4 and exact 3.16.0 are
available. `julia`, PySR, and Operon were not installed in the existing PATH or
Python environment. No benchmark experiment has been run.

Official release source tags found on 2026-10-26:

- PySR 2.7.2, source commit `04d550c446db6594f399e8686831c1bee801975c`, Apache-2.0; backend SymbolicRegression.jl 2.7.2, commit `53d0dd2c6138ffed6208cffbe8396149d22320ac`, Apache-2.0.
- PyOperon 0.6.2, release commit `adf7e0e34041238e22e7ff7cd4330451de97bb36`, MIT. Its checked-in dependency script pins Operon core at `fad10f087c592812b1a9ecfabce669f37bf459f7` and enumerates other source dependencies. The core dependency must be recorded as that commit (not incorrectly described as a v0.6.2 Operon release tag).

Source-only repositories are held under ignored `out/tmp/d7/acquisition/` for
license/API inspection, not execution. Build and Python dependencies are not yet
installed. PySR's release pins Julia 1.10.0 or 1.10.3 and SymbolicRegression
2.7.2; verify availability for Linux aarch64 before selecting the frozen
configuration. PyOperon 0.6.2 requires Python >=3.10 and a compiled C++ extension;
its dependency script defaults to clang and installs many pinned native
dependencies to `sys.prefix`. Only run it inside a disposable isolated venv with
reviewed install prefix and pinned compiler/dependencies. Nix is not assumed.

## Next permitted work

Finish checkpoint A with narrow checkpoint migration tests/repair and verify the
0.5.0 package metadata. Then freeze `docs/d7/BENCHMARK_PROTOCOL.md` before any
benchmark-target run. Keep the benchmark suites serial on one CPU allocation;
record a non-benchmark engineering preflight separately if needed. The hosted
qualification approval boundary remains untouched.
