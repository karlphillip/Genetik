# Version, support and release policy

The authoritative version is `project(GENETIK VERSION 0.1.0 ...)` in the root
CMakeLists.txt. Package metadata derives from it. The original checkout already
declared 0.1.0; there were no local release tags or documented published releases.
This is an **unreleased development version**, not a claim that 0.1.0 was published.
See CHANGELOG.md for modernization/migration history.

Before 1.0, a minor change (0.1 to 0.2) may introduce documented public source API
incompatibilities. Patches within a minor series preserve the documented public
source API while correcting defects. Numerical or seeded-trajectory changes must
receive explicit release notes, including defect fixes in patches. CMake uses
SameMinorVersion: installed 0.1.x accepts an equal/older request in the 0.1 series,
not requests from 0.0 or 0.2 and not newer unavailable patches. EXACT requests must
match exactly. Header-only delivery does not provide a stable binary ABI; recompile
all dependent translation units consistently when changing headers/options.

The version file is ARCH_INDEPENDENT because the package installs no compiled
architecture-specific binaries. This controls package matching, **not** execution
qualification on arbitrary architectures. Compiler/runtime qualification is separate.

## Qualification status

| Configuration | Status |
| --- | --- |
| Jetson ARM64 Linux, GCC/libstdc++ 13.3.0, glibc/libm 2.39; Release, Debug, ASan+UBSan | Locally verified; delivery results in DELIVERY.md. |
| CMake 3.16.0 and 4.4.4 on the Jetson | Locally exercised minimum and current tool; install/relocation/consumption. |
| Ubuntu 24.04 x86-64 GCC 13 Release, CMake 3.16.0 | Configured for CI; hosted execution pending. |
| Ubuntu 24.04 x86-64 Clang 18 GNU frontend + libstdc++, Debug ASan+UBSan | Configured for CI; hosted execution pending. |
| Native Ubuntu 24.04 ARM64 GCC 13 Release | Configured for CI; hosted execution pending (Jetson tests are not a hosted-run claim). |
| Windows/MSVC, clang-cl, AppleClang/macOS, libc++, other compilers/runtimes | Unqualified. Flag-selection logic alone does not qualify them. |

CMake 3.16 remains the minimum; a workspace tool at 4.4.4 does not establish a new
minimum. No CMake requirement was raised. Local minimum tooling was built from the
checksum-verified official 3.16.0 source, with OpenSSL disabled (only a build-tool
choice, not a Genetik library requirement). GCC 13 is the available local compiler;
Clang 18 and x86-64 jobs have not run locally or on hosted CI in this milestone.

Same-seed repeatability is scoped to the **exact library revision/version**, compiler,
standard library, runtime math library/FP environment, input/order, configuration
and serial execution mode. A compiler/libm update can change logarithm rounding,
scores, tie ordering and trajectories. Do not compare different builds as if a seed
were a portable model identifier. CI uses portable domain/math tolerances and
within-build deterministic checks. The captured evaluation-results bit reference
belongs specifically to accepted Jetson/GCC13/libm2.39 execution; it is opt-in via
GENETIK_RECORDED_REFERENCE and is retained unchanged. Qualification on another host
requires investigation/differential evidence, not blanket cross-platform snapshots.

## Release procedure (preparation only; no release performed)

1. Review API migrations and numerical/trajectory changes; update CHANGELOG and
   support evidence. Set the single root project version; review compatibility
   policy if moving to 1.x, and update version-request fixtures for the new package
   version. Do not imply stable ABI.
2. Run the documented Release/Debug/sanitizer contracts, standalone headers, installed
   quickstarts, relocation/multi-TU/version/source-parent checks and minimum CMake.
   Require successful hosted jobs for configurations listed as CI-verified.
3. Inspect a staged install for headers, relocatable CMake metadata and unchanged MIT
   license/attribution. Package no campaign/data/tool artifacts. Reproduce clean use
   without the checkout or producer build, with no development runtime dependencies.
4. Review documentation, versioned source archive contents and checksums. Record
   exact compiler/runtime/build qualification and remaining unsupported platforms.
5. Remote pushing, release tags and publication require a separately requested
   delivery action. None are performed here.

CI workflow uses read-only repository permissions, pinned official action revisions,
bounded jobs and no required stochastic campaigns. Runner labels/tool availability
were checked against [GitHub runner documentation](https://docs.github.com/en/actions/reference/runners/github-hosted-runners),
[Ubuntu x64 image](https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2404-Readme.md)
and [ARM64 image](https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2404-Arm64-Readme.md).
The checked-in workflow is configuration, not evidence of a passing hosted matrix.

## CPU execution qualification update

Opt-in generic worker execution and private built-in refinement are locally tested
with GCC 13.3.0/release-13 standard headers, installed libstdc++6/libtsan2 packages
14.2.0-4ubuntu2~24.04.1 and libc6 2.39-0ubuntu8.8 on the Jetson. Separate TSan
forced-overlap/OOP tests pass ten repetitions each; ASan+UBSan remains a separate
build. See CONCURRENCY_RESULTS.md for exact evidence and limitations. Hosted jobs,
including the configured ARM64 TSan step, have not executed. Parallel safety of
arbitrary callbacks is not inferred from constness or from this local qualification.

## Hosted qualification preparation

The workflow now asserts its 24-test portable manifest and the separate two-test
TSan selection, explicitly enables development coverage, and records checkout/image/
compiler/header/runtime package versions. No Jetson-specific captured trajectory is
asserted on hosted runners. A focused new contract verifies that requesting detailed
refinement diagnostics together with parallel execution fails clearly before work,
without changing the requested execution mode; serial diagnostics remain supported.
See DELIVERY.md and the local ignored out/hosted-preflight/PREFLIGHT.md for upload
scope, verification and authorization status. This preparation does not change any
support-table entry to hosted-verified; jobs have not yet executed. The recorded
approximately fourfold prime-refinement result remains a Jetson fixed-work/reused-
pool observation, not a hosted performance claim or general scaling guarantee.
