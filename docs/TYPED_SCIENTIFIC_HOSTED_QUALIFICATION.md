# Hosted qualification: cleanup, typed mathematics and scientific operators

## Candidate and scope

Workflow [39582456985](https://github.com/karlphillip/Genetik/actions/runs/39582456985)
passed all three jobs for **778a30dd0c05fe74a0755dcadbe59bed7e4ebc32** on
2026-10-21. This qualifies the accumulated cleanup, typed evaluation, typed
regression/aggregate fitting and scientific scalar features through the existing
shared evolutionary engine. The approved eleven-commit upload starts after
published baseline `f9299ea2cb5136d06e2de8f68bb83ecf460ca369`; it is confined to
`qualification/typed-scientific`. No default-branch update, PR, tag, version change
or release action is part of this work. The published 0.1.0 remains fixed; this is
subsequent development, not qualification retroactively assigned to that release.

This report/support update is committed after that implementation run and receives
its own full matrix. Its exact final SHA and final job links are recorded in the
ignored `out/runs/hosted-typed-scientific/COMPLETION.md` and completion response,
without another tracked bookkeeping commit that would leave an untested tip.
The job links below cover the implementation candidate explicitly identified above.

## Executed jobs and environment

| Job and link | Compiler / standard headers | CMake / CTest | Result |
| --- | --- | --- | --- |
| [GCC x64 minimum](https://github.com/karlphillip/Genetik/actions/runs/39582456985/job/119163107190) | GCC/G++13.3.0, Ubuntu 13.3.0-6ubuntu2~24.04.1; libstdc++13 headers, `__GLIBCXX__=20240904` | exact3.16.0, official source/checksum bootstrap | Release/Werror, 33/33;49.90s |
| [Clang x64 sanitizers](https://github.com/karlphillip/Genetik/actions/runs/39582456985/job/119163107295) | Clang18.1.3, Ubuntu1ubuntu1; libstdc++14 headers, `__GLIBCXX__=20240908` | 3.31.6 | Debug/Werror/ASan+UBSan,33/33;71.37s |
| [GCC native ARM64](https://github.com/karlphillip/Genetik/actions/runs/39582456985/job/119163107029) | GCC/G++13.3.0, Ubuntu13.3.0-6ubuntu2~24.04.1; libstdc++13 headers, `__GLIBCXX__=20240904` | 3.31.6 | Release/Werror,33/33;86.84s; separate Debug TSan5 cases x3 passes |

These are suite wall times, not performance measurements. All jobs identify the
same full checkout SHA in their environment logs. Ubuntu24.04.5 LTS runner images:
x64 minimum `ubuntu24/20261018.339.1`, x64 Clang
`ubuntu24/20261012.334.1`, native ARM64 `ubuntu24-arm64/20261018.154.1`.
Kernels respectively7.0.0-1017-azure,6.17.0-1022-azure,7.0.0-1017-azure.
All use Python3.12.3, glibc/libm2.39 (`libc6=2.39-0ubuntu8.9`), installed
libstdc++6/libtsan2 `14.2.0-4ubuntu2~24.04.1` with the appropriate architecture.
Clang's use of GNU standard-library headers/runtime **does not qualify libc++**.

Actual compilation commands were downloaded and checked: C++20,
`-fno-fast-math -ffp-contract=off` in all three builds; Debug Clang qualification
targets additionally use `-fsanitize=address,undefined -fno-omit-frame-pointer`.
Separate ARM64 Debug targets use `-fsanitize=thread`, never ASan at the same time.
Native libm calls retain runtime-qualified behavior, default rounding and gradual
underflow assumptions. No hosted floating-point or sanitizer requirement was weakened.

## Portable manifest and exclusions

The exact33 names are retained in
[portable-tests.txt](typed-scientific-hosted-results/portable-tests.txt).
Each actual job manifest was compared as a set (including duplicate detection)
against `scripts/check_ci_manifest.py`; each test also passed in the executed log.
The mapping from the previous hosted24 tests is additive:

| Coverage added | Names / count |
| --- | --- |
| Cleanup | `genetik_repository_hygiene` (1) |
| Typed evaluation | `genetik_typed_contracts`, `genetik_typed_overlap`, `genetik_typed_example` (3) |
| Typed regression | `genetik_typed_regression_contracts`, `genetik_typed_preparation_overlap`, `genetik_typed_search_quickstart`, `genetik_typed_regression_cli` (4) |
| Scientific operators | `genetik_scientific_contracts` (1) |
| Previous portable contracts | all24 retained |

Local34 minus portable33 is **only `genetik_evaluation_reference`**, disabled by
`GENETIK_RECORDED_REFERENCE=OFF`. That recorded Jetson/GCC13/libm2.39 snapshot is
an environment-specific exact bit/classification/row-work reference. It remains
unchanged and qualified locally. Portable arithmetic/domain, ownership, resource,
fitness, failed-publication, budgets and concurrency tests are not excluded.
Independent scientific references retain exact-binary64 input identities,
mpmath200/320-digit stability checks and documented four-ULP comparisons for the
selected non-exact references. Normal CI does not need mpmath installed.

Same-build deterministic repetition and nonbinding serial/parallel1/2/3/4
comparisons remain active. Those are comparisons within one build/runtime, not
requirements for every host to reproduce a Jetson search trajectory or libm bit.
Full historical searches/campaigns are preserved observations, not CI assertions.
Only bounded public-workflow/discovery smoke tests run in this matrix.

## Separate TSan execution

The ARM64 job configures an independent fully instrumented Debug build. Its actual
five-test manifest contains:

- `genetik_concurrency_contracts`
- `genetik_oop_parallel`
- `genetik_typed_overlap`
- `genetik_typed_preparation_overlap`
- `genetik_scientific_contracts`

Runner CTest3.31.6 executes `--repeat until-fail:3`, with
`TSAN_OPTIONS=halt_on_error=1`: **each name passed three times,15 successful
executions**,1.22s total in this run. This newer repeat option is not used in the
exact-CMake3.16 x64 job. Forced backend/OOP overlap, immutable typed evaluation,
typed worker preparation/refinement and scientific worker operations are exercised.
The focused typed overlap modes intentionally select bounded overlap bodies;
scientific runs its full bounded contract body. This is not a claim that all33
portable tests ran under TSan. No relevant suppression or instrumentation exclusion
was added. Full TSan/strict-FP flags were checked for all five fixture families.

## Delivery and public workflows

Each job compiles standalone public headers and runs strict-FP positive/negative
checks, character/file/prime smoke workflows, installed quickstarts, typed search
and fitting, and external nonmathematical permutation/OOP behavior. Detailed
refinement diagnostics plus N>1 still fail clearly before work; the existing
concurrency fixture checks the unchanged requested mode/RNG/counters. Parallel
safety of arbitrary callbacks is never inferred from these built-in qualifications.

The package fixture checks source inclusion, ordinary installed consumption,
relocation to a path containing spaces, removal of disposable producer sources and
builds, overlapping public headers in multiple translation units, version matching
and component rejection. Scalar, symbolic, typed/fitting and generic OOP consumers
run against `Genetik::Genetik`, including actual worker use. The target supplies
C++20, strict FP and Threads::Threads. Installed consumer compilation metadata
confirms no development sanitizer/Werror leakage and no source-tree includes;
relocated resolution points to prefix B. C compilation receives no C++ FP flags.
C++23 consumer coverage is additional on CMake>=3.20; exact3.16 intentionally runs
the mandatory C++20 path without that optional newer-standard variant.

## Preparation changes, failures and evidence

No implementation, portability, numerical-reference or sanitizer failure occurred
in the first hosted matrix. Initial HTTPS upload attempts failed locally: no Git
credential helper, then GitHub's OAuth workflow-scope rejection. Existing SSH
credentials completed the ordinary approved push; no credentials, remote URL,
repository protection or history were changed. Rejected pushes changed no ref.

Two local preflight commits preceded upload:

- `c3969478af57b12a86f201fef6ccb7d3598da946`: repeat the five hosted TSan cases three
  times; export compile commands and retain installed-consumer metadata.
- `778a30dd0c05fe74a0755dcadbe59bed7e4ebc32`: correct the typed grammar guide's stale
  nine-kind limit to fourteen (nine originals plus five scientific additions).

Neither changed arithmetic, RNG consumption, fitter policy, tolerances, test
selection or timeouts. Clean tracked-source Release/Werror passed33/33 before
upload; all399 non-guide files in the final preflight archive matched that tested
source. Updated TSan command passed5x3 locally. actionlint, manifest checks and
workspace hygiene passed. Prior local Release/Debug/ASan/exact3.16 and replay
qualification remains independently recorded in SCIENTIFIC_QUALIFICATION.md.

Selected durable evidence is under
[typed-scientific-hosted-results/](typed-scientific-hosted-results/): exact job/run
metadata, environment logs, portable/TSan execution logs, audited manifest names,
and checksums of the complete downloaded record. Full workflow logs, compile
commands, manifests, packaging command logs and artifacts are retained locally
under `out/runs/hosted-typed-scientific/hosted/39582456985/`; GitHub artifacts retain
14 days. Final report-commit evidence is recorded separately under that same
canonical output role. No original failure was repeatedly rerun to obtain a pass.

All127 frozen historical evidence hashes, all15 scientific outcomes (including
unsuccessful oscillator seed1), typed campaigns and published asset identities are
preserved. Published source SHA256 remains
`7f632489e45abb97385d98c9c2f703664386e358b22f3f73baa8a72c99b6b992`;
SHA256SUMS identity remains
`a12d576eedc38b80af11f118d82d086a992f3d8930b89bda743b91191663353b`.
Main and the annotated v0.1.0 tag remain at their original identities.

## Remaining scope

Windows/MSVC/clang-cl, macOS/AppleClang and libc++ remain unqualified. These three
selected Linux configurations do not imply coverage of every standard library,
OS or runtime. TSan is specifically native GCC ARM64; x64 Clang uses separate
ASan+UBSan. The roughly fourfold fixed-work prime-refinement speedup remains a
Jetson reused-four-worker-pool observation from CONCURRENCY_RESULTS.md, not a hosted
performance claim. Heuristic recovery is not proved by correctness qualification.
Phase5 text input/adapters remains the next separately scoped implementation task.
