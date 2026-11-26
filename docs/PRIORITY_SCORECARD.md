# PR #72 mathematical and conservation priority scorecard

Base: `1344982fe798108abf2993ba204cd399d93ab947` (full SHA; confirmed in the freshly fetched `origin/main` before worktree creation). The task worktree began at this exact SHA. The conservation discovery protocol is frozen and its unsuccessful six-seed results are reported below; other comparisons are deterministic contract evidence.

| Capability | Base behavior | Candidate behavior and independent expected result | Measured result / work | Remaining limitation |
|---|---|---|---|---|
| Configurable expression complexity | Selection policies order by physical node count; no operator/variable/constant weights. | Opt-in weighted count supports per-operator overrides and is used in penalty selection, Pareto dominance/crowding, and reporting. Unit weights preserve the old count. | Deterministic near-constant fixture reverses variable-vs-constant ranking; equal-size Add/Multiply trees reverse under operator weights; Pareto dominance changes the frontier; a four-point front confirms the crowding-weighted operator cost changes survivor selection. Release and ASan/UBSan selection contracts pass. | No frozen/stochastic selection comparison. Physical node count still controls resource limits. |
| Custom scalar objective | Scalar regression uses built-in loss modes; no callback loss in ordinary search/fitting. | Finite nonnegative per-row callback loss is averaged for both paths; LM explicitly rejects arbitrary callbacks. | Asymmetric absolute/pinball fixture `{0,2,10}` has unique optimum 10; symbolic search and coordinate fitting return 10. Independent MSE optimum is 4 (MSE `56/3` at 10). Installed objectives quickstart consumer passed. | Callback checkpoint serialization is rejected by design. |
| Pareto vector objectives | Pareto selection uses scalar prediction loss plus complexity. | Optional bounded callback returns up to four finite minimization objectives from training-only rows; Pareto dominance/crowding use that vector plus configured complexity. Selected points and vector-evaluation work are reported. | Actual public symbolic search on two-row constants returns hand-computed nondominated set `(0,4),(1,1),(4,0)` exactly; vector overflow is rejected. Release contract passes. | Callback purity and bounded work within each row are caller responsibilities; generic checkpoint resume must recreate the identical callback because callback code is not serialized. |
| Minimum description length | No MDL selector. Existing AIC/BIC are unchanged. | Opt-in post-search two-part fixed-precision signed Elias-gamma code for supported fixed-row polynomial regression; reports model/residual/total bits. | Independent code-length calculations give constant fixture 15 vs 21 bits and linear fixture 21 vs 15 bits, reversing the winner. Release and ASan/UBSan contracts pass. | Not equivalent to Operon's codebook; no frozen stochastic comparison. |
| Bounded multi-site/shared-function templates | Base provides one-site scalar templates and a two-site `MultiComponentTemplateRegression` API with independent component limits; no three-or-more-site path or named shared-function API. | Vector-sized site lists now carry independent roots, variables, operators, grammars, and component limits through generation, variation, fitting, extraction, and prediction; legacy two-site overloads remain, and component and range extraction remain vector-like while converting to the historical two-element arrays for old assignments. `SharedLearnableFunctionTemplate` expands named calls over distinct inputs while fitting one shared coefficient. | Deterministic three-site fixture recovers `sin(x0)`, `3*x1`, and `x2*x2` against supplied scaffold `x0+x1+x2`, MSE 0. Public variation initialized two candidates and exercised one mutation plus one crossover proposal, and coordinate fitting recovers coefficient 3 with one selected parameter. Sharing fixture `g(p)+g(q)`, body `a*t`, rows (1,0)->2 and (0,1)->6 gives one parameter `a=4`, predictions 4 and 4, MSE 4, and prediction 20 at (2,3). Body variation to `a*t^2` gives call contributions 12 and 27 at (2,3) with a=3, total 39. The analytic fit uses 6 row evaluations (two design probes plus final measured loss over two rows). Release, ASan/UBSan, and clean installed-consumer checks passed. | Named-body fitting currently supports ordinary MSE and a single direct multiplicative coefficient `a*h(t)`; callback/robust losses and multiple/nonlinear parameters are rejected or unsupported. Expanded ordinary expressions are snapshots; shared identity is preserved by the named-template API during fitting/body variation. |
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

The deterministic three-site model uses the supplied scaffold `x0+x1+x2`; the three unknown recovered components are `sin(x0)`, `3*x1`, and `x2*x2`, each on a separate variable and grammar. Across the fixture grid the complete model has MSE 0. The public variation policy initialized two candidates, proposed mutation and crossover, and retained valid component restrictions. Public coordinate fitting starts from `x1*1`, selects one component coefficient, and recovers 3 with zero MSE. Prediction is checked on the full expanded expression. These are deterministic contracts, not claims that stochastic search recovered the components.

`SharedLearnableFunctionTemplate` models named calls whose body is `a*h(t)` with one selected direct coefficient and argument-only `h`. For `g(p)+g(q)` with `g(t)=a*t`, equally weighted rows `(1,0)->2` and `(0,1)->6` have unique shared least-squares coefficient `a=4`, one fitted parameter, predictions 4 and 4, MSE 4, and prediction 20 at `(2,3)`. Independently fitting two coefficients would attain MSE 0, so this fixture distinguishes sharing. Replacing the body with `a*t^2` and setting `a=3` changes the two call contributions at `(p,q)=(2,3)` to 12 and 27, total 39. This analytic fitter supports ordinary MSE only and rejects non-identifiable features; arbitrary nonlinear coefficient placement is outside the declared body form.

The focused Release tests `genetik_multi_component_template_contracts` and `genetik_multi_component_templates_quickstart` passed. The installed quickstart now compiles and executes the vector site and named shared-function public APIs. Qualification details for this head are appended after the final installed-consumer, sanitizer, hosted CI, and hygiene runs.


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
