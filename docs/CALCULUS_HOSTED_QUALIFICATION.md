# Hosted qualification: complete calculus and Genetik 0.2.0 candidate

## Tested implementation and complete scope

Workflow [39775639787](https://github.com/karlphillip/Genetik/actions/runs/39775639787)
passed all three jobs for **e30413ab119340a290c7b3c74d229097670fbb74** on2026-10-22.
This includes the reviewed0.2.0 metadata, packaging and migration preparation,
not just the integral evaluator. Sixteen commits were uploaded after hosted-qualified
parser/template tip50af98d192ce441f1d5ce5deeddbf9dcf996896e. The complete proposed
delivery is33 commits after publishedv0.1.0 f9299ea2cb5136d06e2de8f68bb83ecf460ca369:
cleanup, typed evaluation/regression/fitting, scientific operators, parsing, scalar
templates, partial derivatives, derivative-observation regression and root integrals.
All specializations retain the generic OOP engine, explicit run randomness,
execution, budget-admission and staged-publication contracts.

This report and release-note/support updates are a subsequent commit. Its own
complete push-triggered matrix must pass before delivery completion. The final
SHA/run/job links go in ignored `out/runs/hosted-calculus/COMPLETION.md`, not in
another tracked self-referential commit. Source archives/checksums are regenerated
from that exact final committed source and verified without.git. Earlier passing
jobs and assets are not relabeled as results for a later SHA.

## Executed jobs and actual environments

| Job | Actual compiler / headers | CMake / CTest | Results |
|---|---|---|---|
|[GCC13 x64 minimum](https://github.com/karlphillip/Genetik/actions/runs/39775639787/job/119813743129)|GCC/G++13.3.0, Ubuntu13.3.0-6ubuntu2~24.04.1; GNU13 headers, `__GLIBCXX__=20240904`|exact3.16.0, checksum-verified official bootstrap|Release/Werror,52/52,81.67s|
|[Clang18 x64 sanitizer](https://github.com/karlphillip/Genetik/actions/runs/39775639787/job/119813744416)|Ubuntu Clang18.1.3 (1ubuntu1); **GNU14 headers**, `__GLIBCXX__=20240908`|3.31.6|Debug/Werror/ASan+UBSan,52/52,152.48s|
|[GCC13 native ARM64](https://github.com/karlphillip/Genetik/actions/runs/39775639787/job/119813743632)|GCC/G++13.3.0, Ubuntu13.3.0-6ubuntu2~24.04.1; GNU13 headers, `__GLIBCXX__=20240904`|3.31.6|Release/Werror,52/52,145.30s; separate Debug TSan10x3 + complete integral contracts|

Times include correctness/package compilation and are not performance comparisons.
All job environment logs start with the exact same testedSHA. OS is Ubuntu24.04.5
LTS, kernel6.17.0-1022-azure for x86_64 and aarch64. The x64 image is
`ubuntu24/20261012.334.1`; native ARM is `ubuntu24-arm64/20261012.149.1`.
Each has Python3.12.3, glibc/libm2.39 (`libc6=2.39-0ubuntu8.9`), libstdc++6 and
libtsan2 `14.2.0-4ubuntu2~24.04.1`. The runtime GNU DSO version is distinct from
GCC13's selected13-series standard headers. Clang uses GNU headers/runtime, **not
libc++**. These facts were read from this run, not copied as assumed runner defaults.

[Selected evidence](calculus-hosted-results/) retains actual environments, configured
manifests, per-test result logs, TSan logs, package commands, run metadata, artifact
hash ledger and review audit. Full downloaded artifacts, compile commands and workflow
logs remain under ignored `out/runs/hosted-calculus/hosted/39775639787/`.
The final-run artifacts additionally capture actual link command files and runtime
DSO inspection, so sanitizer linking is directly reviewable as well as compilation.
That capture-only workflow improvement passed actionlint/bash syntax and local
execution against ordinary/TSan trees; it changes no production or test behavior.

## Actual portable manifest and mapping

All three downloaded configured manifests match the exact **52 unique names** in
[portable-tests.txt](calculus-hosted-results/portable-tests.txt); each name has one
successful portable execution in each job. The audit checks names, not just totals.
All41 previously hosted parser/template cases remain. The eleven new cases are:

- `genetik_derivative_contracts`, `genetik_derivative_overlap`, `genetik_derivatives_quickstart`.
- `genetik_derivative_regression_contracts`, `genetik_derivative_regression_overlap`,
  `genetik_derivative_regression_quickstart`, `genetik_derivative_regression_cli`.
- `genetik_integral_contracts`, `genetik_integral_overlap`,
  `genetik_integrals_quickstart`, `genetik_integral_cli`.

The full qualified local53-name manifest differs only by
`genetik_evaluation_reference`, the captured Jetson/GCC13/libm2.39 binary snapshot.
Hosted explicitly sets GENETIK_RECORDED_REFERENCE=OFF. Portable arithmetic,
scientific independent references, ownership, limits/resources, weighted losses,
fitting, failure/accounting/publication, OOP and concurrency contracts are retained.
No test, numerical tolerance, operator or sanitizer instrumentation was removed to pass.

The full builds also compile32 independent public headers with C++20, strict FP and
warnings-as-errors. Installed-package tests exercise ordinary and relocated source-free
multi-TU consumers, all nine public quickstarts, scalar/symbolic/typed/template/calculus
execution, nonmathematical permutation isolation, source embedding/unrelated target,
version acceptance/rejection, required-components diagnostics and no producer/dev-runtime
leakage. CMake3.31 jobs also exercise newer-standard C++23 consumption. The fixture
preserves paths with spaces, removes its disposable producer source/build before
relocated consumption and constrains package discovery to the moved prefix.

## Separate TSan and numerical review

Every name in [tsan-tests.txt](calculus-hosted-results/tsan-tests.txt) passed exactly
**three times** in the separate ARM64 Debug/TSan build:

| Selection | Successful executions |
|---|---:|
|genetik_concurrency_contracts|3|
|genetik_oop_parallel|3|
|genetik_typed_overlap|3|
|genetik_typed_preparation_overlap|3|
|genetik_scientific_contracts|3|
|genetik_parsing_overlap|3|
|genetik_template_overlap|3|
|genetik_derivative_overlap|3|
|genetik_derivative_regression_overlap|3|
|genetik_integral_overlap|3|

The10-selection repetitions took1.83s. The **complete genetik_integral_contracts**
then passed separately (0.26s), including more than the forced-overlap fixture:
transactional subdivision failures, binding/payload limits, fitting and publication.
TSAN_OPTIONS=halt_on_error=1, --repeat until-fail:3 with runner CTest3.31.6; the
x64 minimum3.16 job never receives that unsupported newer option.

Actual compilation records show `-fno-fast-math -ffp-contract=off`, C++20 and Werror.
Clang adds `-fsanitize=address,undefined -fno-omit-frame-pointer`; separate ARM TSan
adds `-fsanitize=thread` and never combines ASan. Instrumented executables built and
ran; full passed-test output was reviewed for recoverable UBSan errors as well as
ASan/TSan findings. None were found. Final-tip link/DSO files provide additional
explicit instrumentation evidence. Default rounding/gradual underflow, qualified
native binary64 and libm assumptions remain required.

Portable mathematical/API fixtures are distinct from same-build deterministic
replay and nonbinding1/2/4 worker equivalence. Neither requires hosted machines to
reproduce a Jetson-specific discovered expression. Those runtime-captured references
remain unchanged and qualified only within their declared environment.

## Repairs, preservation and delivery status

No implementation, portability, numerical-reference or test defect appeared in this
hosted run; no retry was needed. The only subsequent workflow change captures link
commands/runtime dependencies for final review. Preflight's original archive-permission
checker failure and correction (Git tracks executability, not group-write mode), and
Phase7C's original hygiene/fault-fixture development failures remain recorded in
CALCULUS_PREFLIGHT.md and INTEGRAL_QUALIFICATION.md with their retained logs.

The final binding-buffer correction afbb03d is included in the complete clean hosted
build/tests, including N=2 evaluation and installed full-model preparation/fitting.
Earlier full53 local gates and final-six affected checks retain their actual source
scopes; this run supplies the complete portable final-implementation coverage rather
than claiming the older full suites ran at a newer SHA.

All127 frozen file hashes, prior campaign records (including unsuccessful seeds),
main, v0.1.0 annotated object and published archive/checksum identities remain
unchanged. No campaign was rerun or tuned. No PR, default-branch update, tag creation
or release publication was performed. Version0.2.0 remains unreleased.

The agreed feature implementation and Linux implementation matrix are complete.
After the final report-tip matrix and regenerated source assets pass, the remaining
action is approval/execution of the prepared development delivery, not another
mathematical phase. See RELEASE_PROPOSAL.md, releases/0.2.0/NOTES.md and MIGRATION.md.
SameMinorVersion intentionally rejects0.1 requests for0.2; rebuild dependent translation
units consistently after the polymorphic measurement seam, with no ABI promise.

Limitations remain explicit: finite-precision chain rules and conservative unavailable
partials; weighted supplied observations/anchors do not prove uniqueness; quadrature
only estimates convergence and may miss unsampled features/singularities. The measured
3.55e-14 reference discrepancy is not a general accuracy bound. No nested/improper
integrals or differentiation through quadrature. Coordinate fitting/search are
heuristic; detailed refinement diagnostics are serial-only, callbacks need explicit
parallel qualification, and concurrency does not mean CPU affinity. The four-worker
prime benchmark remains its recorded fixed-work/reused-pool observation, not a
hosted or universal speedup. Windows/MSVC, AppleClang/macOS, libc++ and other untested
platforms remain unqualified.
