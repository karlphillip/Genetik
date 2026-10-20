# Hosted development-branch qualification

Version 0.1.0 remains unreleased. This work is confined to
`qualification/hosted-0.1.0`; it does not merge/change the default branch, rewrite
remote history, create tags or publish release assets. The user explicitly
approved uploading the complete 15-commit modernization range from remote baseline
`b8059f08ececa9bda6b2c19f35a925c070052f56` through
`4fff1180a6b8b68f851887827593edce00eb49f6`, plus targeted qualification fixes.
The earlier local-only preparation record remains under out/hosted-preflight/.

## Coverage accounting

The 24-case portable manifest is asserted by scripts/check_ci_manifest.py. It is
exactly the previous 25-case local suite minus `genetik_evaluation_reference`.
That excluded case compares captured binary64 output/classification/row-work
against the accepted Jetson/GCC13/libm snapshot. It remains opt-in locally through
GENETIK_RECORDED_REFERENCE; the snapshot and all 127 historical evidence files are
unchanged. It is not a cross-platform seeded-trajectory or libm-bit promise.

| Coverage | Portable cases |
| --- | --- |
| Arithmetic, multivariate, fitting, symbolic, expression, fitness/publication, generic contracts | 7 |
| Regression CLI, expression example, character bounded/zero/invalid argument | 5 |
| Source downstream and strict-FP policy | 2 |
| Prime contracts/CLI and evaluation workspace | 3 |
| Installed/relocated package, scalar and symbolic quickstarts | 3 |
| Generic OOP, concurrency, parallel OOP and parallel character | 4 |
| Total | 24 |

Standalone-header compilation is part of the build, not another CTest count.
`genetik_concurrency_contracts` forces overlap and checks limits 1/2/3/4, ordering,
nonbinding within-build equivalence, budgets, failures, draining and worker math;
its new diagnostics fixture verifies rejection before work and unchanged requested
concurrency. The separate TSan Debug build executes that same case plus
`genetik_oop_parallel`: two additional **instrumented executions**, not two new
unique portable tests. It does not claim to run all 24 under TSan.

Clang's normal Debug build instruments the contracts with ASan+UBSan. Installed
consumers intentionally build without producer development instrumentation, checking
that sanitizer requirements do not leak. The package case runs source inclusion,
ordinary and relocated isolated multi-TU scalar/symbolic consumers, actual compiled
quickstarts and the external permutation consumer, including parallel execution.
Disposable producer trees are removed; C++20, strict FP and Threads::Threads must
propagate from Genetik::Genetik. Clean downstream use has no source include hints.

## Failures and repairs

- The initial push was authenticated through the existing SSH agent; HTTPS had no
  credential helper. The repository destination and authorized branch stayed fixed.
- Initial run [39476788744](https://github.com/karlphillip/Genetik/actions/runs/39476788744)
  tested `4fff1180a6b8b68f851887827593edce00eb49f6`. Its artifact uploader rejected
  parent-relative paths under upload-artifact v6. Commit `a023d22` stages logs into
  an absolute RUNNER_TEMP directory before upload. It also retains bounded public
  failure annotations: unauthenticated REST log downloads return 403 despite SSH
  write access. Full build/test logs remain in the runner artifacts; bounded
  annotations are explicitly the last 100 lines / 24,000 characters.
- Run [39477077279](https://github.com/karlphillip/Genetik/actions/runs/39477077279)
  tested `a023d22f137fc6ae0ffc5ae20cb387e15d63519a` and reproduced Clang's actual
  failure: implicit discard of vector::at's nodiscard result in scalar bounds
  validation under Werror. Commit `23914b8` uses explicit void casts. Bounds checks
  still execute before genome mutation/fitness invalidation; no warning suppression,
  arithmetic change or API redesign was introduced. The same commit makes the
  recorded environment available as a bounded public annotation.

Local targeted scalar/fitness/quickstart checks pass after that repair, workflow
lint passes, and the diagnostic helper preserves command failure status and escapes
annotation text. Earlier complete local Release/Debug/ASan+UBSan/minimum/TSan
qualification remains in CONCURRENCY_RESULTS.md. Hosted results below must name
actual tested SHAs; neither configured nor skipped steps count as passing.

## Executed evidence for the repaired implementation

Run [39477303495](https://github.com/karlphillip/Genetik/actions/runs/39477303495)
tests `23914b8502dfe4665bf65a2a6e7fa94c8d8e3654`.

- [Clang 18 x64 ASan+UBSan](https://github.com/karlphillip/Genetik/actions/runs/39477303495/job/118816478366):
  24/24 pass (55.69 s); complete job successful, including artifact retention.
  Ubuntu 24.04.5, image ubuntu24 `20261012.334.1`, kernel 6.17.0-1022-azure,
  Clang 18.1.3 (1ubuntu1), **libstdc++ release-14 headers** `__GLIBCXX__=20240908`,
  CMake/CTest 3.31.6, Python 3.12.3. This is GNU-style Clang with libstdc++, not libc++.
- [GCC 13 ARM64](https://github.com/karlphillip/Genetik/actions/runs/39477303495/job/118816478681):
  24/24 pass (49.95 s); separate Debug TSan **2/2 pass** (0.47 s), forced overlap,
  `halt_on_error=1`, no suppression; complete job successful.
  Ubuntu 24.04.5, image ubuntu24-arm64 `20261018.154.1`, kernel 7.0.0-1017-azure,
  GCC 13.3.0-6ubuntu2~24.04.1, release-13 headers `__GLIBCXX__=20240904`,
  CMake/CTest 3.31.6, Python 3.12.3.

- [GCC 13 x64 / exact minimum CMake](https://github.com/karlphillip/Genetik/actions/runs/39477303495/job/118816478023):
  24/24 pass (42.16 s); complete job successful. CMake and CTest **3.16.0**, built
  from checksum-verified official source. Ubuntu 24.04.5, image ubuntu24
  `20261018.339.1`, kernel 7.0.0-1017-azure, GCC 13.3.0-6ubuntu2~24.04.1,
  release-13 headers `__GLIBCXX__=20240904`, Python 3.12.3.

All three recorded hosts use libc6/libm **2.39-0ubuntu8.9** and installed libstdc++6/
libtsan2 **14.2.0-4ubuntu2~24.04.1**. Compiler headers and runtime package versions
are separate qualifications. Test-suite wall times are observations, not new
performance comparisons. Selected raw public annotations (environment plus every
CTest outcome, and original failures) are retained in hosted-results; full runner
logs are retained as workflow artifacts. Unauthenticated log-download access is
restricted, so the review explicitly uses these bounded public annotations and
job/step statuses, not a claim to have downloaded unavailable full logs.


## Scope and remaining release work

The approximately fourfold prime-refinement improvement remains the recorded
Jetson fixed-work benchmark with a reused four-worker pool, not a hosted timing
claim. No new stochastic campaign or performance experiment was run here. Hosted
checks require deterministic correctness and within-build comparisons, not target
recovery or cross-platform expression identity.

Windows/MSVC/clang-cl, AppleClang/macOS, libc++ and other runtimes remain unqualified.
The tested callback fixtures do not make arbitrary application callbacks safe.
Parallel detailed refinement diagnostics remain explicitly unsupported; interval 1
and serial execution remain defaults. Existing release/version/source-compatibility
policy is unchanged; a successful development-branch CI run is evidence for review,
not a release/tag/publication or a declaration of universal production readiness.


All three jobs and the aggregate workflow passed for the same repaired SHA.
`23914b8-run.json` and `23914b8-jobs.json` retain exact job IDs, tested head SHA,
step conclusions and runner labels. Parsed portable-output names match the complete
24-test manifest on each host; the ARM64 TSan output matches its separate two-test
selection. Minimum-tool bootstrap is intentionally skipped on the two current-CMake
jobs, and TSan is intentionally skipped on x64 jobs; those skips are not reported
as passed coverage. Prior runs are retained with their failure conclusions rather
than replaced by successful reruns of an unexplained deterministic failure.

This evidence/report commit changes documentation only relative to that tested
implementation. Its push receives a fresh matrix run; the final branch-tip run and
SHA are recorded in the task completion report and local out/hosted evidence.
Readiness recommendation: suitable for first **development-release review** after
that final documentation-tip gate, with unsupported platforms and callback/numerical
scope stated above. Publication still requires a separate instruction.
