# Calculus qualification and 0.2.0 delivery preparation

Status: local preparation; no new hosted execution/upload is claimed. Proposed
branch qualification/calculus, repository karlphillip/Genetik. The prior hosted tip
50af98d192ce441f1d5ce5deeddbf9dcf996896e passed
[matrix39664655065](https://github.com/karlphillip/Genetik/actions/runs/39664655065).
The new hosted range starts there; the **complete delivery** range starts at published
v0.1.0 f9299ea2cb5136d06e2de8f68bb83ecf460ca369. The ignored preflight packet under
`out/runs/hosted-calculus/preflight/` records exact final SHA/ranges/counts/commit
lists and changed-file summaries after stable preparation is committed.

## Local evidence audit

Integral implementation 2ed3724f3c034d2338fa86dd4d20e1d6801c8f56 received full
53-test Release/Debug/ASan+UBSan/exact-CMake3.16 runs. These are not full-suite runs
at final 7264186b1d2f3d4888cd5410e3dfd98087583149. The detailed
[INTEGRAL_QUALIFICATION.md](INTEGRAL_QUALIFICATION.md) and
[integral qualification results](integral-results/qualification/results.json)
preserve the original runs, timings and development failures. In particular,
Debug's first 52/53 run failed hygiene because manual py_compile created source
bytecode; only that generated file was removed, and the full rerun passed.

The later afbb03d654f4402689d017d07de3741183488d2a binding-buffer correction changes
only IntegralWorkspace construction of the private external-plus-local input vector:
resize(N+1) before copying, instead of assign(N) then push_back (possible geometric
capacity beyond the planned payload). This affects standalone evaluation and all
integral measurement/preparation/fitting paths through that same workspace. It
changes neither arithmetic/sample order nor RNG/loss units. The added N=2 independent
fixture integrates x0*t+x1 from0 to x0+x1 at(2,3) to40, with55 binding validations.

After this fix, six affected tests passed in each of Release, Debug, ASan+UBSan and
exact3.16: installed_package, repository_hygiene, integral_contracts, integral_overlap,
integrals_quickstart and integral_cli (all with `genetik_` prefix). Both standalone
headers and relevant targets were rebuilt; normal/relocated multi-TU consumers
exercise evaluation, measurement, fitting and preparation. Ten named TSan selections
were each repeated three times after the fix and complete integral contracts passed.
Four-configuration app smokes passed after the reporting correction 6e8322e:115
fitter objective attempts, separate accepted-score verification, unchanged coefficients.
Three accepted serial replays and typed compatibility passed. The remaining47
contracts have unchanged implementation dependencies; no uncovered buffer-dependent
path was identified. The upcoming clean full hosted matrix must cover the final
candidate, including version/package preparation, not extrapolate from these runs.

## Manifest and workflow policy

Derive the portable CTest names from a clean tracked-source configure and require
exact equality with scripts/check_ci_manifest.py. Only the explicitly runtime-specific
`genetik_evaluation_reference` is excluded by GENETIK_RECORDED_REFERENCE=OFF;
portable arithmetic, resource, ownership, numerical and concurrency tests remain.
Standalone headers compile in the full build; warnings-as-errors are private,
strict FP and Threads requirements propagate downstream. Installed-package tests
cover source-free relocation, multiple translation units, generic OOP isolation,
versions/components, new quickstarts and development-runtime leakage.

Ten separate TSan selections, each three repetitions:

- genetik_concurrency_contracts
- genetik_oop_parallel
- genetik_typed_overlap
- genetik_typed_preparation_overlap
- genetik_scientific_contracts
- genetik_parsing_overlap
- genetik_template_overlap
- genetik_derivative_overlap
- genetik_derivative_regression_overlap
- genetik_integral_overlap

Additionally run the complete genetik_integral_contracts in the separate TSan build,
matching the established local harness. The workflow previously lacked that extra
complete-contract command; preparation adds it without removing any overlap fixture
or instrumentation. The ARM job uses runner CTest supporting --repeat until-fail:3;
the x64 minimum job uses exact3.16 without that newer flag. Unrestricted push triggers
include the qualification branch and documentation-only report tip. Existing matrix:
GCC13 x64 Release/exact3.16; Clang18 x64 Debug ASan+UBSan; GCC13 native ARM64 Release
plus separate Debug TSan. Retain environment, compilation, manifests, per-test results,
package commands and complete integral TSan logs. A green badge alone is insufficient.

## Version, source delivery and limitations

Root CMake version0.2.0 is the authority; SameMinorVersion intentionally starts a new
minor series. Explicit0.1 requests fail;0.2 and exact0.2.0 succeed. Header-only package
matching stays architecture-independent; this is not execution-platform qualification.
No stable binary ABI: all dependent translation units must be rebuilt consistently
with the polymorphic measurement seam. [MIGRATION.md](MIGRATION.md) consolidates this.

Only release/package/workflow/docs preparation changes are needed; no numerical
feature or campaign retuning. The source-archive checker now derives its prefix
from the committed version and verifies every member's blob bytes and mode against
that commit before running the existing extracted-source consumer gate. Generated
assets are bound to the final SHA in ignored output; regenerate if the candidate changes.
See [full release proposal](RELEASE_PROPOSAL.md) and [notes](releases/0.2.0/NOTES.md).

The numerical tables/estimator in IntegralExpression are independently implemented
from the attributed published QUADPACK dqk15 mathematical rule. No Fortran/Boost
runtime/code is bundled. Estimated convergence is not rigorous error certification;
unsampled narrow features may be missed. The measured reference discrepancy is not
an accuracy bound. Derivatives are finite-precision chain-rule results, conservatively
unavailable on some active paths. Finite observations/anchors do not identify a unique
global function/integrand. Preserve all earlier unsuccessful campaign seeds.

The prior hosted Linux/GNU-standard-library scope does not yet qualify this calculus
candidate. Windows/MSVC, AppleClang/macOS and libc++ remain unqualified. New hosted
compiler/runtime details must be collected from the actual jobs after approval.
