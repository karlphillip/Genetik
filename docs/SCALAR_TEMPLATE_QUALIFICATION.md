# Phase 6A scalar-template local qualification

Base: accepted Phase 5 `d612446ae5f2fb5e1ebd91165b6e01e304b8cab5`.
Implementation: `cdd2553cab1000d709ebaa44cac6ea9ddc740881`. The executed gates
cover its production/test source contents. The following evidence-only commit changes
no implementation or test behavior.

Branch: `feature/scalar-templates`, retaining cleanup, typed evaluation/regression,
scientific operators and parsing ancestry. No remote operations were performed.
Final implementation/qualification SHAs are in the completion record; this document
does not claim a hosted run for a later local commit.

## Environment and commands

Jetson Orin NX 16GB, Ubuntu 24.04.4 ARM64, kernel 6.8.12-1021-tegra. GCC/G++ 13.3.0
(13.3.0-6ubuntu2~24.04.1), libstdc++ 13 headers, libstdc++ runtime 14.2.0,
libtsan2 14.2.0-4ubuntu2~24.04.1, glibc/libm 2.39-0ubuntu8.8. Workspace CMake 4.4.4
(`../.tools/bin/`) and exact 3.16.0 (`../delivery-tools/cmake-min/bin/`). Strict
`-fno-fast-math -ffp-contract=off`, native binary64/default rounding/gradual underflow.
Warnings are errors; sanitizer development flags remain private.

Ordinary public commands use `cmake`/`ctest`; these tool paths were used locally:

```sh
../.tools/bin/cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build out/build/release -j2
../.tools/bin/ctest --test-dir out/build/release --output-on-failure
# Same configure/build/test for out/build/debug with CMAKE_BUILD_TYPE=Debug.
../.tools/bin/cmake -S . -B out/build/sanitize -DCMAKE_BUILD_TYPE=Debug -DGENETIK_SANITIZERS=ON -DGENETIK_THREAD_SANITIZER=OFF -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build out/build/sanitize -j2
../.tools/bin/ctest --test-dir out/build/sanitize --output-on-failure
../delivery-tools/cmake-min/bin/cmake -S . -B out/build/minimum -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../delivery-tools/cmake-min/bin/cmake --build out/build/minimum -j2
../delivery-tools/cmake-min/bin/cmake -E chdir out/build/minimum /home/karlphillip/workspaces/primeResearcher/delivery-tools/cmake-min/bin/ctest --output-on-failure
```

## Executed results

| Configuration | Actual CTest result | Total elapsed seconds |
|---|---:|---:|
| Release / CMake 4.4.4 |42/42 passed|225.66|
| Debug / CMake 4.4.4 |42/42 passed|230.49|
| Debug ASan+UBSan / CMake 4.4.4 |42/42 passed|671.26|
| Release / exact CMake 3.16.0 |42/42 passed|192.97|
| Separate Debug TSan |7 selections ×3 passed (21 executions)|4.19|

The four focused template cases were rebuilt/rechecked in all four configurations
following the final smoke-label change; all passed. No gate needed a timeout
increase, suppression or test exclusion. Package/strict-FP/source/relocation/multi-TU,
character/file workflows and headers passed as parts of these builds/suites.
Recorded CTest summaries are under [scalar-template-results](scalar-template-results/ctest-release-final.log).
Elapsed suite times include concurrent development-gate load and are not performance
claims. Raw configure/build/test logs remain under out/logs/templates.

`out/tmp/actionlint`, actual portable/TSan JSON manifest checks, `git diff --check`,
and `python3 scripts/check_hygiene.py --workspace --self-test` passed. No unexpected
root output was produced. Workspace tools and historical locations are unchanged.

## Focused contracts

`genetik_template_contracts` uses independent full-model value/loss fixtures,
source/storage ownership, source-subtree discard, masked forbidden variables,
operator whitelists, original index 1 binding, local versus complete destination
node/depth/work limits, exact materialization boundary, frozen signed-zero bits,
strong edit failure, context identity and detached clones. Modest seeded sampling
shows initialization/variation preserve restrictions and can actually change trees.
Root-only limits, impossible terminals, finite rejection/fallback and different
subtree sizes are covered. No test requires stochastic discovery of the target.

Fitting checks the independently weighted c=2.6 optimum, a=.5/b=1.5 recovery,
frozen -9 outside fitting bounds, independent constants, exact accepted model/loss,
wrong objectives, invalid probes after progress and an inherited expression that
improves under another bounded fit on the scheduled generation. Nonbinding 1/2/3/4
runs match initialization, expressions/fitness, counters, histories and final RNG.
Parser reimport of complete canonical exports preserves structure and evaluation.

Controlled two-worker gates force overlap **inside training computation**, accepted
private fitting progress before sibling failure/cancellation, successful draining,
correct objective-start accounting and unchanged committed population/fitness/best/
generation. Scheduled and unscheduled preparation are exercised. A test-only
allocation fault covers composition, while an operational preparation result trying
to change the scaffold is rejected. Global budget boundaries, all-invalid initial/
subsequent populations and invalid arithmetic under a zero scaffold are checked.
Original nested causes remain distinguishable; no new failure framework or runtime
allocator API is introduced.

## Test manifest and delivery coverage

The manifest is derived with CTest JSON and checked by `scripts/check_ci_manifest.py`.
The preceding parser checkpoint had 38 local tests (37 portable plus the recorded
snapshot). Four Phase 6A tests add:

- `genetik_template_contracts`.
- `genetik_template_overlap`.
- `genetik_templates_quickstart`.
- `genetik_template_campaign_smoke`.

The resulting portable manifest has 41 tests; local `GENETIK_RECORDED_REFERENCE=ON`
adds `genetik_evaluation_reference`, for 42. The only exclusion is the established
Jetson/GCC/libm captured snapshot. Portable strict arithmetic/scientific references,
scalar/typed fitting/publication/concurrency and parsing tests remain enabled.
The exact portable names are retained in scalar-template-results/portable-tests.txt.

Package tests install all headers, run ordinary and relocated multi-TU consumers
with deleted disposable producer trees, execute scalar/symbolic/typed/parser/template
quickstarts and the external permutation consumer, check version/component behavior,
strict-FP propagation and absence of development flag/runtime leaks, and test source
embedding. Both installed TUs include the template API; the second executes parallel
search. Dependency inspection proves the permutation TU includes no expression,
parser, typed or template mathematical headers. Standalone headers include both new
public headers. Existing character and numerical-file workflows remain enabled.

## Separate ThreadSanitizer

```sh
../.tools/bin/cmake -S . -B out/build/tsan -DCMAKE_BUILD_TYPE=Debug -DGENETIK_THREAD_SANITIZER=ON -DGENETIK_SANITIZERS=OFF -DBUILD_TESTING=ON -DGENETIK_BUILD_TESTS=ON -DGENETIK_BUILD_EXAMPLES=OFF -DGENETIK_RECORDED_REFERENCE=OFF -DGENETIK_WARNINGS_AS_ERRORS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
../.tools/bin/cmake --build out/build/tsan --target genetik_concurrency_tests genetik_oop_consumer genetik_typed_tests genetik_typed_regression_tests genetik_scientific_tests genetik_parsing_tests genetik_template_tests -j2
TSAN_OPTIONS=halt_on_error=1 ../.tools/bin/ctest --test-dir out/build/tsan -R 'genetik_concurrency_contracts|genetik_oop_parallel|genetik_typed_overlap|genetik_typed_preparation_overlap|genetik_scientific_contracts|genetik_parsing_overlap|genetik_template_overlap' --repeat until-fail:3 --output-on-failure
```

All seven selections are instrumented; the new overlap fixture shares an immutable
scaffold with independent worker objectives/fitting scratch. Manifest checks and the
actual template compilation command confirm full ThreadSanitizer instrumentation.
There are no suppressions, uninstrumented backend substitutes or nested workers.
The authored ARM64 CI job now selects all seven three times; **no hosted execution
of the combined parser/template branch is claimed**.

## Independent compatibility and evidence

Before evaluator/variation/refinement integration edits, accepted existing Release
binaries captured prime seed 2, arithmetic seed 42 and arithmetic cadence4 replay
stdout. Candidate binaries reproduce all three **byte-for-byte**, including initial
populations, fitness representations, work/history, stops and final RNG. Commands:

```sh
out/build/release/genetik_evaluation_workspace --replay prime
out/build/release/genetik_evaluation_workspace --replay arithmetic
out/build/release/genetik_evaluation_workspace --replay arithmetic 4
out/build/release/genetik_typed_regression --smoke --fit --threads 1 --output out/runs/templates/compatibility/typed-after
```

Typed smoke results, frozen models, full-precision datasets and protocol match the
accepted pre-edit capture after excluding only its `runtime=` field. Hashes and
normalization policy are retained in scalar-template-results/compatibility.json;
full local captures remain under out/runs/templates/compatibility. No research
campaign was rerun to change historical results.

The new fixed campaign and separate fitting fixture are in SCALAR_TEMPLATE_RESULTS.md.
All five seeds are retained, including exact expressions, variation counters and
traces; sample recovery remains separate from engineering correctness.

All 127 frozen historical evidence hashes are unchanged. Published source archive
SHA256 remains `7f632489e45abb97385d98c9c2f703664386e358b22f3f73baa8a72c99b6b992`;
SHA256SUMS file remains `a12d576eedc38b80af11f118d82d086a992f3d8930b89bda743b91191663353b`.
Main remains `f9299ea2cb5136d06e2de8f68bb83ecf460ca369` and annotated v0.1.0 tag
object `986316039749aedcf20d1c1537bb4c91ded06151` is unchanged. Output remains under
canonical ignored directories; no historical transcript/path was rewritten.

## Repairs and limitations

Early new-code builds caught a narrowing execution-setting conversion and mixed-type
`auto` declarations; these were corrected before final tests. Test scaffolding was
corrected to catch the documented out-of-range root error and retain an owned
population snapshot before iterating. Final API review added a direct local-constant
bound check to `validateSubstitution` (materialization already rejected it). Smoke
protocol output now accurately reports its one-seed subset. Raw development failure
logs remain under out/logs/templates; no numerical tolerance was widened or campaign
setting tuned to obtain a pass.

No new scalar/typed default behavior, generic GA API, public header spelling,
package target, numerical rule, RNG draw policy, version or mandatory dependency is
changed. Selected fitting/context APIs are additive. Detailed diagnostics remain
serial-only, global-budget parallel trajectories retain the established qualified
scope, coordinate search can stall, and single-site scalar scope remains explicit.
Windows/MSVC, macOS/AppleClang and libc++ are not newly qualified. Hosted qualification
and further template capabilities require separately authorized work.
