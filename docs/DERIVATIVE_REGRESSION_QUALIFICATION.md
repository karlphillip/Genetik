# Phase 7B local qualification

Start: clean `efa21e4716c6a3ffa9c612e7c9f9026c62475251` on
feature/scalar-derivatives. Work is on feature/derivative-regression. Implementation
commit: `a17d109a55ea09965a60d6739fb9f57e06c08a3f`; compatibility repair
`b4feb5614a30ec420c5e489b28e16cf6cde8d9cd`; invocation-local value workspace
`71164b3c8ef73d0b993518357ac88358b24b6c54`. No remote writes, versions,
main/tag changes or release operations. Hosted calculus qualification remains for
the final checkpoint after 7C; earlier hosted records qualify only their named SHAs.

## Scope and architecture

DerivativeRegression.hpp owns immutable value/partial channels and implements the
weighted normalized objective documented in DERIVATIVE_REGRESSION.md. The mathematical
RegressionProblem now has a virtual measurement/sample-count/destructor and explicit
parallel capability, plus a protected constrained schema constructor without fake
value labels. Existing braced row-list constructors remain unambiguous. The generic
candidate/algorithm/selection/variation interfaces have no new math dependencies.
RegressionProblem's existing MSE body is unchanged apart from rejecting a sliced
schema-only adapter. All dependent TUs must be recompiled; header-only has no stable ABI.

The shared TrainingObjective, ConstantRefinement, fitter and SymbolicRegression driver
handle admission, staging/publication, cadence, failures and counters. The narrow
protected row counter supports trusted objective compute overrides. The existing noexcept worker-merge hook is preserved. Separate post-merge
validation reports supplemental overflow/type errors without widening its signature.
No new search loop, optimizer, executor or primitive global budget was introduced.
New observations are scored by the same objective for fitness, fits and verification.

## Independent engineering fixtures

* Exact value mean2.5 / derivative normalized mean.5 / composite1 fixture; duplicating
  either channel leaves it unchanged. Positive weight/scale and all finite/schema,
  unique-channel, count/checked input-product and normalization validations.
* f=sqrt(x): value at0 and partial at1 are valid together. Required partial at0 is
  unavailable; all-row reporting counts that and a later invalid primal separately.
* Derivative-only x*x, x*x+1, x*x+7 tie at0. Adding f(0)=1 gives.5,0,18. Multi-input
  two-partial fixture checks original indices0 and2 and canonical channel ordering.
* Owned input mutation, problem copying/identity, candidate edit invalidation,
  separate reporting, context-change rejection and cached fitness preservation.
* Raw-unit square overflow/underflow remains diagnostic-only when normalized
  objective is valid. Normalized arithmetic and zero-underflow rejection are tested.
* Per-observation, aggregate arithmetic, value plan/payload and partial resource
  boundaries, including exact admitted work, charged resource/throwing calls and
  no invalid-fitness conversion. Allocation-failure sweep retains partial observation
  work and original bad_alloc causes; successful prefixes do not become full scores.
* Fixed quadratic: independent coefficients recover(1.5,-.5,1.25) with score0; same
  composite score certifies the expression. An accepted a=1 persists after a c=0
  square-root probe has valid value but unavailable partial. Template fitting selects
  only inserted constants; frozen -1 remains unchanged (a whole-model fit would fail it).
* Nonbinding1/2/4-worker comparisons: raw/prepared initialization, matching fitness,
  best model/generation, all legacy/initial/preparation/supplemental counters, variation,
  history, stopping, discarded work and final RNG agree. Serial detailed diagnostics
  preserve computations; incompatible N>1 diagnostics are rejected without mode changes.
* Forced two-worker preparation/fitting gates: a private fit finds a **strictly better**
  model than the committed archive before a sibling throws/cancels/resources. All
  admitted work drains, consumed starts/counters remain, committed genomes/fitness/
  generation/archive remain unchanged. Retrying the same prospective generation works.
  Actual restrictive objective resources, tiny global allowance9 and all-unavailable
  initial populations exercise real failure paths through the existing engine.

No unit test requires a stochastic search to find an optimum. The five-seed campaign
is separate measured evidence, with no tuning/restarts; see DERIVATIVE_REGRESSION_RESULTS.md.

## Commands and execution environment

Jetson Orin NX, native ARM64 Ubuntu24.04.4, GCC13.3/GNU13 headers,
libstdc++/libtsan14.2, glibc/libm2.39, default rounding/gradual underflow. Recorded
compiler/runtime packages, flags and manifests are retained under
`docs/derivative-regression-results/qualification/`. C++20, strict
-fno-fast-math -ffp-contract=off, private warnings-as-errors and private instrumented
builds remain in use. CMake/CTest4.4.4 tools are ../.tools/bin; exact3.16.0 tools
remain ../delivery-tools/cmake-min/bin. Public commands use ordinary installed tools:

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release \
  -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
cmake --build out/build/release -j2
cmake -E chdir out/build/release ctest --output-on-failure
# Debug: out/build/debug, CMAKE_BUILD_TYPE=Debug.
# ASan+UBSan: out/build/sanitize, Debug, GENETIK_SANITIZERS=ON.
# Exact minimum: same commands using3.16.0 under out/build/minimum.
# TSan: out/build/tsan, Debug, GENETIK_THREAD_SANITIZER=ON,
# GENETIK_SANITIZERS=OFF, GENETIK_BUILD_EXAMPLES=OFF, recorded reference OFF.
```

TSan uses CTest4.4.4 --repeat until-fail:3 for the actual nine selections in
qualification/tsan-tests.json, including the new derivative-regression overlap.
No ASan/TSan combination, suppressions or reduced instrumentation. The authored CI
job adds that selection and the portable new contracts/CLI; it has not run for 7B.

Isolated installation checks compile/run the actual new quickstart against ordinary
and relocated prefixes with spaces, after deleting disposable producer trees. Both
consumer TUs include/use the new API. Generic permutation dependencies explicitly
exclude it. Standalone headers, strict-FP negative compilations, target propagation,
source inclusion, version/components and private development-runtime checks remain.
No mandatory dependency was introduced; Python remains development tooling only.

## Development issues and record scope

The initial compile attempted to pass a volatile weight-normalization result directly
to vector::push_back. An explicit double value copy fixes the qualifier mismatch
while retaining the checked rounding boundary. Diagnostics are retained in
out/logs/derivative-regression-first-build.log and selected qualification evidence.
Constructor review found potential overload ambiguity with legacy braced row lists;
the constrained MeasurementSchema constructor and two compile/runtime regression
fixtures remove it without changing ordinary constructor semantics. Required final
builds were refreshed after these repairs. Final review restored the original
`mergeWorker noexcept` signature and moved supplemental validation into its own
post-merge hook; a compile/runtime fixture checks the inherited contract. Value
rows reuse the existing lexical workspace across observations; valid/invalid/valid
row coverage confirms clean logical stacks. Neither repair changes objective
arithmetic, search policy or random draws. No numerical expectation or instrumentation
was weakened and no campaign settings or outcomes changed.

Detailed logs remain in out/logs/derivative-regression-*; independent replay captures
are in out/runs/derivative-regression/compatibility/. Frozen historical evidence,
old experiments and authoritative published assets are preserved. Windows/MSVC,
macOS/AppleClang and libc++ remain unqualified. Remaining implementation is solely
7C bounded numerical integration, followed by final qualification/delivery.

## Final-source verification

The implementation tested by the final gates is
`71164b3c8ef73d0b993518357ac88358b24b6c54`. Subsequent changes record documentation
and selected evidence only; the completion SHA is recorded outside tracked source
under out/runs/derivative-regression/COMPLETION.md to avoid self-referential SHA edits.

The local manifest contains49 tests, including the qualified Jetson captured
snapshot. The portable manifest contains48 actual names; its only exclusion is
`genetik_evaluation_reference`, the recorded runtime-specific snapshot. Portable
strict arithmetic, typed/scientific/parser/template/derivative contracts, OOP,
concurrency and package checks remain enabled. Per-configuration names are retained
in qualification/*-test-names.json; the exact portable manifest passes
scripts/check_ci_manifest.py.

Separate TSan9-selection coverage, each repeated3 times with halt_on_error=1:

* genetik_concurrency_contracts
* genetik_oop_parallel
* genetik_typed_overlap
* genetik_typed_preparation_overlap
* genetik_scientific_contracts
* genetik_parsing_overlap
* genetik_template_overlap
* genetik_derivative_overlap
* genetik_derivative_regression_overlap

The repetition record verifies each name has3 passing executions (27 total), rather
than relying on a total alone. The complete derivative-regression contract executable
also passes under TSan. Full ASan+UBSan remains separately instrumented, without
suppressions. Earlier pre-repair suites and logs are retained under out/logs/ for
provenance; final selected logs identify the refreshed gates.


All three independent serial captures from the accepted pre-edit executable match
final-source output byte-for-byte: prime interval1, strict arithmetic interval1,
and strict arithmetic interval4. This includes initialization, expressions/fitness
bits, generations, work counters, best-seen traces, stop and final RNG. The typed
search/fitting smoke matches the accepted earlier data, protocol, frozen models
and counters/results, excluding only runtime. Comparison hashes are retained in
qualification/compatibility.json. No historical multi-seed campaign was rerun.

The127 frozen hashes, authoritative published archive/checksum identities, main and
the annotated v0.1.0 object remain unchanged; preservation.txt records identities.
Workspace hygiene (including parent audit/self-test), actionlint and exact manifest
checks pass. Output stays in out/; selected new records are deliberately retained
here with SHA256SUMS. Public/installed examples use real --threads; no nested pool
or unqualified automatic callback concurrency is introduced.


The final-source installed/public quickstart independently reproduces the supplied
quadratic recovery and173 composite attempts. Its small8-candidate search stops at
the declared generation ceiling with objective.5, demonstrating that program
success does not depend on stochastic equation recovery; see quickstart.txt.


| Final-source gate | Result |
| --- | --- |
| Release, CMake4.4.4 |49/49 passed,250.76s |
| Debug, CMake4.4.4 |49/49 passed,331.78s |
| Exact minimum CMake/CTest3.16.0 |49/49 passed,261.04s |
| Debug ASan+UBSan, CMake4.4.4 |49/49 passed,734.50s |
| TSan, separate Debug build |9 selections x3 passed,6.09s; full new contracts also pass |

These elapsed suite times are verification observations under the current device
load, not performance comparisons. Warnings-as-errors, standalone headers,
strict-FP checks, all installed quickstarts and ordinary/relocated multi-TU consumers
are included. The generic OOP consumer remains independent of mathematical headers.

All four final-source suites pass with no skipped tests or runtime failures. Actual
per-test logs and results.json retain the49-name accounting. Hosted execution for
this feature remains pending; these results qualify only the recorded local setup.

Selected JSON manifests are whitespace-normalized; raw CTest JSON and environment
captures remain in out/runs/derivative-regression/. No manifest fields are removed.
