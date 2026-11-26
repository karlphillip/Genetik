# PR #72 mathematical and conservation priority scorecard

Base: `1344982fe798108abf2993ba204cd399d93ab947` (full SHA; confirmed in the freshly fetched `origin/main` before worktree creation). The task worktree began at this exact SHA. The conservation discovery protocol is frozen and its unsuccessful six-seed results are reported below; other comparisons are deterministic contract evidence.

| Capability | Base behavior | Candidate behavior and independent expected result | Measured result / work | Remaining limitation |
|---|---|---|---|---|
| Configurable expression complexity | Selection policies order by physical node count; no operator/variable/constant weights. | Opt-in weighted count supports per-operator overrides and is used in penalty selection, Pareto dominance/crowding, and reporting. Unit weights preserve the old count. | Deterministic near-constant fixture reverses variable-vs-constant ranking; equal-size Add/Multiply trees reverse under operator weights; Pareto dominance changes the frontier; a four-point front confirms the crowding-weighted operator cost changes survivor selection. Release and ASan/UBSan selection contracts pass. | No frozen/stochastic selection comparison. Physical node count still controls resource limits. |
| Custom scalar objective | Scalar regression uses built-in loss modes; no callback loss in ordinary search/fitting. | Finite nonnegative per-row callback loss is averaged for both paths; LM explicitly rejects arbitrary callbacks. | Asymmetric absolute/pinball fixture `{0,2,10}` has unique optimum 10; symbolic search and coordinate fitting return 10. Independent MSE optimum is 4 (MSE `56/3` at 10). Installed objectives quickstart consumer passed. | Callback checkpoint serialization is rejected by design. |
| Pareto vector objectives | Pareto selection uses scalar prediction loss plus complexity. | Optional bounded callback returns up to four finite minimization objectives from training-only rows; Pareto dominance/crowding use that vector plus configured complexity. Selected points and vector-evaluation work are reported. | Actual public symbolic search on two-row constants returns hand-computed nondominated set `(0,4),(1,1),(4,0)` exactly; vector overflow is rejected. Release contract passes. | Callback purity and bounded work within each row are caller responsibilities; generic checkpoint resume must recreate the identical callback because callback code is not serialized. |
| Minimum description length | No MDL selector. Existing AIC/BIC are unchanged. | Opt-in post-search two-part fixed-precision signed Elias-gamma code for supported fixed-row polynomial regression; reports model/residual/total bits. | Independent code-length calculations give constant fixture 15 vs 21 bits and linear fixture 21 vs 15 bits, reversing the winner. Release and ASan/UBSan contracts pass. | Not equivalent to Operon's codebook; no frozen stochastic comparison. |
| Bounded multi-site/shared-function templates | Base supported two independently restricted sites and no named shared function. | Three or more independently restricted sites now have vector extraction (`componentsVector`); the historical two-site `components` result is `std::array`, preserving structured binding. Shared bodies can be generated/mutated under bounded grammars and share one fitted coefficient across distinct calls. | Three-site construction control: supplied components `sin(x0)`, `3*x1`, `x2*x2`, MSE 0. Separate public coordinate fit recovers coefficient 3. Seed 99173 search selected `x0`, `(2*x1)*1.5`, `x2*x2` (miss): train MSE 0.0127774, held-out MSE 0.0317634, 32 calls, 46,476 row attempts, 1,291 fitting loss attempts, 3 generations. Shared supplied-body control: `a=4`, predictions 4/4, MSE 4, prediction 20 at (2,3). Seed 20261127 bounded body selection chose `x0*x0`, `a=3`, MSE 0 across 64 generated/mutated bodies. Scaled shared fit gives a=1 and MSE 0 for s=1, 2^600, 2^-600; all-zero feature rejected. Focused Release/ASan and clean installed consumers on base and candidate pass. | Shared function fitting is ordinary MSE for one direct factor `a*h(t)`. Body search is a bounded fixed-seed generation/mutation selection fixture, not a general shared-function GA or multi-parameter optimizer; full expanded model limits remain enforced. |
| Whole-domain value/derivative bounds | Existing assessment uses observed points; no whole-box certificate. | Opt-in outward-rounded interval dual arithmetic for bounded polynomial expressions; reports certified, verified-witness violation, or inconclusive. Overflow now widens to the full extended range, preventing an overflowed point enclosure from becoming a false witness. | `x^3-x/4` on `[-1,1]` gets a derivative violation witness at zero; `x+x^3` certifies nonnegative derivative; unsupported `Log` is inconclusive. `(x*x)*s`, `x=±2^600`, `s=2^-1000` is inconclusive without a violation witness. | Proof grammar limited to constants, variables, `+`, `-`, `*`; overflow and subdivision ceilings can yield inconclusive. |
| Autonomous complex symbolic search | Complex evaluation of real-coefficient structures and `fitComplexCoefficients` exist; no adapter joins ordinary structure search to independent real/imaginary coefficient refinement. | Public `complexSymbolicRegression` retains the best valid structure as incumbent and only replaces it with a refined topology having no worse comparable training loss. | Frozen v5 target outcomes remain unchanged: both tasks, seeds 401/503/601, 1,312 search evaluations and 15,744 row evaluations per arm; all six completed, full parameterized recovery 1/6. New deterministic identity/no-constant and no-worse-loss API regressions pass. | Autonomous search recovery remains weak for the frozen grammar/budget. Study history and target/seed reuse are documented in the study README; supplied-topology results are diagnostic. |
| Conservation-law discovery | No grouped-trajectory objective or search path; the base API cannot express this comparison. | Opt-in grouped `(y,v)` scorer and ordinary symbolic search; grouping is scorer-only. Frozen harmonic/quartic campaign uses common arithmetic grammar, held-out initial conditions, a 640-candidate cap per run, followed by exact polynomial Lie derivative and separate reference-law assessment. | Six fixed seeds completed. Every run used 640 candidates and 349,440 training-row evaluations; all selected expressions were nonconstant but had nonzero exact Lie derivatives. Reference affine recovery 0/6; exact nonconstant invariant 0/6. Per-seed expressions, held-out drift, separation, nodes, wall time, generator error and verifier outcomes are in [`research/conservation_discovery/README.md`](../research/conservation_discovery/README.md) and machine records. Damped control energy changed by `-0.6878681724`. Scorer rejects roundoff-only cancellation in Release tests. | No discovery within fixed budgets. No baseline comparison is possible at the base API. Search effectiveness on broader systems remains unqualified. |

## Qualification status

The revised Release build completed. A clean Release CTest rerun excluding the separately isolated installed-package test passed 141/141 tests. The first pass had a repository-hygiene failure from a `scripts/__pycache__` file created by the separate branch-lifecycle check; that cache was removed before the clean rerun. The isolated installed-package test used its normal 1,200-second CTest timeout and failed at its internal 300-second command cap while compiling installed quickstarts; the exact step, progress, and follow-up artifact runs are recorded in [the installed-package qualification note](../research/installed_package_qualification/README.md). The later relocation, C++23, and embedding stages were not reached.

The full Release and ASan/UBSan target builds completed. Seven relevant ASan/UBSan contracts passed (custom objective, vector Pareto, MDL, polynomial bounds, conservation scoring, two-site templates, and scientific/complex fitting); the autonomous complex adapter also passed its synthetic constant-fit smoke under ASan/UBSan. Changed-line clang-format/clang-tidy passes. On hosted head `023c24a69fb9d2cafff189941800e308063a2a3b`, formatter, sanitizer, Python, and optional-math jobs pass; GCC minimum and arm64 jobs remain pending. `python3 scripts/test_branch_lifecycle.py` passed all 10 tests. `python3 scripts/check_hygiene.py` passed after the generated cache was removed.

`python3 scripts/check_hygiene.py` and `git diff --check` pass. The workspace-wide hygiene mode reports a pre-existing sibling `Genetik-sync-backup-20261118T183118Z`; it was preserved. `python3 scripts/test_branch_lifecycle.py` passed all 10 tests.

### Follow-up head `54eab33dc9edb914811ef6a3960c94f59bc94022`

The final full Release build succeeded and the full CTest run passed 141/141.
Final targeted Release and ASan/UBSan runs passed polynomial bounds, symbolic
selection (including operator-weighted crowding survival), and D6 complex
search contracts. The exact coefficient recheck of all six frozen conservation
selections is recorded in
[`exact-verification-v3.json`](../research/conservation_discovery/results/exact-verification-v3.json).

The isolated installed-package test again hit the inner 300-second limit during
the ordinary consumer build at 93%, compiling
`complex_coefficient_fitting_quickstart`. The partial build was resumed with a
1,200-second command cap; ordinary and relocated installed consumers,
quickstart execution, metadata/version/component checks, C++23, and embedding
all passed. See the follow-up section of the
[installed-package note](../research/installed_package_qualification/README.md).
Hosted status for this exact head is tracked on [draft PR #72](https://github.com/karlphillip/Genetik/pull/72); it is not merged.


### Template follow-up on PR #72

At the unchanged base `1344982fe798108abf2993ba204cd399d93ab947`, the public multi-component template path accepted exactly two sites and could not represent a named function shared across calls. The candidate generalizes the site vector while retaining the existing two-site constructor, `create`, variation, and search overloads. Each site keeps an independent full-schema variable allowlist, operator allowlist, grammar, and expression limits. Materialization and extraction validate the complete expanded expression against model node, depth, arithmetic, constant, and materialization limits.

Three-site evidence separates construction, fitting, and discovery. The construction control supplies `sin(x0)`, `3*x1`, and `x2*x2` to `materialize()` and measures MSE 0; it makes no search claim. A separate coordinate-fit control starts from the scaffold substitution `x1*1`, selects one coefficient, and recovers `3` with zero MSE. The actual public search uses training rows only, seed `99173`, population 8, maximum 32 candidate calls, 4 generation attempts, a `32 * training_rows * 20` loss-attempt cap, and coefficient fitting in `[0,4]`. Its grammar is site 0 `{sin,multiply}`, site 1 `{multiply}`, site 2 `{multiply}` with constants and fit configuration allowing the target coefficient. The best model was `x0`, `(2*x1)*1.5`, `x2*x2`: training MSE `0.0127774`, held-out MSE `0.0317634`, 32 candidate calls, 46,476 training row attempts, 1,291 fitting-loss attempts, and 3 published generations. This is a measured miss on the sine and linear components, despite the exact construction and coefficient-fit controls. The scaffold `x0+x1+x2` was supplied; these three selected components were not.

For shared functions, the supplied-body sharing control remains `g(t)=a*t`: one parameter `a=4`, predictions 4 and 4, MSE 4, and prediction 20 at `(2,3)`; independent coefficients would attain MSE 0. A distinct fixed-seed bounded body-selection fixture (`20261127`, grammar `{multiply}`, 64 candidates split evenly between public generation and mutation) used targets `3*(p^2+q^2)` over distinct `(p,q)` rows without supplying the target body. It selected body `((x0*x0)*1.0)`, coefficient 3, training MSE 0. A separate body-generation/mutation seed `20261126` changed the body from `(((x0*x0)*(x0*x0))*1.0)` to `(((x0*(x0*(x0*x0)))*(x0*x0))*1.0)`; both call-site predictions changed (32,162 to 128,1458 at p=2,q=3), confirming shared-body changes reach both calls. The shared fitter additionally returns `a=1`, one parameter, and zero MSE for `s=1`, `2^600`, and `2^-600`; an all-zero feature fixture is rejected as non-identifiable. It supports ordinary MSE and a single direct multiplicative coefficient `a*h(t)` only.

The two-site `components(model)` return type is again `std::array<Expression,2>`, so installed consumers can use `auto [left,right] = problem.components(model)`. Arbitrary site counts use `componentsVector(model)`. A variation constructor rejects a grammar count mismatch before indexing. The complete expanded expression continues to be checked against model limits. Clean installed consumers with this structured-binding source compiled and ran on both the base package and candidate package.
The focused Release tests `genetik_multi_component_template_contracts` and `genetik_multi_component_templates_quickstart` passed. The installed quickstart now compiles and executes the three-site vector extraction, two-site structured binding, and generated/mutated named shared-function public APIs. Qualification details for this head are appended after the final installed-consumer, sanitizer, hosted CI, and hygiene runs.


Final template qualification on PR #72
--------------------------------------

The base remained `1344982fe798108abf2993ba204cd399d93ab947` after a fresh fetch. All eight focused Release tests and the same eight ASan/UBSan tests passed during final qualification: new multi-component contracts and quickstart, legacy one-site template contracts/overlap/quickstart/campaign smoke, and existing automatic-separable contracts/quickstart. `<MultiComponentTemplateRegression.hpp>` also compiled as a standalone strict C++20 first include. `clang-format-18 --dry-run --Werror` and `git diff --check` passed.

The standard isolated `genetik_installed_package` CTest run used its configured 1,200-second outer cap and failed after 302.02 seconds at its normal 300-second subprocess cap. The `cmake --build` of the ordinary installed consumer had reached 87%; the build log shows the 300-second timeout while compiling `custom_scalar_operator_quickstart` at 89%. The new multi-component quickstart had compiled and linked at 87%.

To complete the package qualification, the same integration script was run from a separate clean fixture with only its per-command cap changed from 300 to 1,200 seconds. That run passed from scratch: ordinary install/consumer, relocated consumer after deleting producer/build trees, installed quickstarts, metadata/version/component and flag checks, C++23 consumer matrix, and parent embedding/source-inclusion checks. The changed quickstart exercised three-site materialization/prediction and shared-function fitting/body variation. The command log and complete disposable workspace are retained under `out/tmp/template-installed-qualification/genetik-delivery-fixture/`; the normal-cap CTest log remains under `build/release/Testing/Temporary/`.

After the named-body expansion guard, explicit non-MSE rejection, and legacy range-conversion regression were added, the directly affected multi-component Release and ASan/UBSan tests were rerun and passed. The final-source compatibility wrapper is included in the eight-test matrices. The extended clean package run exercised the new public APIs; the final wrapper was subsequently covered by focused builds, the standalone header check, and the pushed-head hosted package workflow. Hosted checks are required on the pushed final head and are reported in the PR and final delivery. The full prior mathematical and conservation study records remain preserved and unchanged.

### Two-site factory compatibility correction

The hosted run at `ffedcc8` identified ambiguity when the new vector factory
shared the name `create` with the existing two-site overload. The N-site path
is now explicitly `MultiComponentTemplateProblem::createMany`; the two-site
`create` overload remains unchanged and its brace-initialized benchmark,
contract, and quickstart callers pass again. The three-site regression now
uses `createMany`. Focused Release and ASan/UBSan matrices both pass all four
template/automatic-separable tests after the correction. The pre-correction
hosted failures were compile ambiguity in GCC/Clang minimum, sanitizer, and
clang-tidy jobs. The separate x86 Python job failed fetching SRBench's external
`results/black-box_results.feather` LFS object (HTTP 503); wheel checks passed
before that fetch step. On final head `d40a27aa19e4e092a1575a56bf22f2a359120430`,
the clean package workflow passed using `tests/package_integration.py` with
only the per-command timeout raised from 300 to 1,200 seconds. It passed
ordinary and relocated multi-translation-unit consumers, installed quickstarts
(including the `createMany` API), metadata/flags/version/component checks,
C++23, and source-inclusion embedding. Evidence is retained at
`out/tmp/template-installed-final-d40a27a/genetik-delivery-fixture/`. The
standard isolated CTest run failed at 301.92 seconds in the ordinary consumer
build as intended by its 300-second cap; process capture showed
`other.cpp` compiling at the timeout, with no compiler diagnostic. Current-head
hosted CI status is recorded in the delivery summary.


### Final template correction qualification

On the task head, the public three-site search and body selection records above are deterministic regression outputs. The two-site source-compatibility syntax is compiled by the installed quickstart with `auto [left, right] = domain->components(...)`; three-site extraction uses the explicit vector API. The scaled least-squares controls cover unit, `2^600`, and `2^-600` feature magnitudes and preserve exact coefficient/MSE results. The fixed-seed shared-body selection is a bounded candidate-generation and mutation selection fixture, not a claim of a general-purpose shared-function GA; the scalar-template fitter remains limited to one direct multiplicative coefficient. Qualification outcomes and exact command caps are appended after final-head runs.

### Final template correction qualification

Focused Release CTest passed both `genetik_multi_component_template_contracts` and `genetik_multi_component_templates_quickstart` (2/2). ASan/UBSan passed the same two tests (2/2; 81.20 seconds total, under each target's 120-second timeout). `clang-format-18 --dry-run --Werror` on all changed C++ files and `git diff --check` passed.

The installed two-site consumer containing `auto [left,right] = problem->components(model)` was built and run against both a clean install of the base `1344982fe798108abf2993ba204cd399d93ab947` and the candidate. The candidate clean installed consumer additionally calls shared-body generation/mutation, two-site shared expansion, three-site `createMany`, and `componentsVector`. Its exact per-command cap was 300 seconds; configure, build, and execution passed from fresh directories. The standalone base consumer also configured, built, and ran successfully, confirming the legacy array API there.

On tested code head `b4539801db9770a7391efe093ec4898bfb8b8a3d`, both hosted workflow runs passed all jobs: GCC 13 x64 minimum, GCC 13 optional-math, GCC 13 arm64 including ThreadSanitizer, Clang sanitizers, clang-format/tidy, and Python cp312 x86_64/aarch64. Runs: [45801689195](https://github.com/karlphillip/Genetik/actions/runs/45801689195) and [45801699428](https://github.com/karlphillip/Genetik/actions/runs/45801699428). The frozen complex and conservation study artifacts were not changed.
