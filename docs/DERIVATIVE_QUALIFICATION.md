# Phase 7A scalar derivative qualification

Starting point: hosted-qualified parser/template tip
`50af98d192ce441f1d5ce5deeddbf9dcf996896e`, clean working tree. Work is on the
local `feature/scalar-derivatives` branch; main, published tag/assets and previous
campaigns are unchanged. No remote action is part of this phase. The contract and
public example are in [SCALAR_DERIVATIVES.md](SCALAR_DERIVATIVES.md).

## Implementation and numerical evidence

The additive header uses a fused iterative postfix traversal with independent
structural activity, invocation-owned operand slots and scalar-owned results.
Ordinary and derivative paths share the binary/log/scientific primal kernels.
The Powi kernel accepts an internal observer after each checked primal multiplication;
its default observer does nothing. The derivative recurrence uses its previous
primal and derivative values, never a condensed power rule or extra primal traversal.
No existing expression/grammar/default/search/budget/publication API changes.

Independent analytic fixtures cover multivariate polynomial/product/quotient rules,
original higher input indices, all powers0..8, parsed and full-template models,
inactive versus singular sqrt, signed zeros/subnormals and invalid/unavailable masks.
The weighted fitting/objective mechanisms are untouched; no gradient fitting is added.
Retained 200/320-digit mpmath analytic references from exact binary64 inputs cover
all transcendental derivative rules, including large trigonometric arguments.
The four-ULP selected-fixture tolerance is explained in SCALAR_DERIVATIVES.md.
Tests compare successful primal bits, first invalid node and executed primal work
against ordinary evaluation for direct and parsed expressions, plus a deterministic
200-expression corpus and a 20,001-node iterative chain.

Resource tests exercise below/exact/above limits, zero and SIZE_MAX settings,
accurate rejection categories and denial of incomplete values. Allocation failure
is operational and leaves an existing destination/expression unchanged; subsequent
calls and owned results remain independent. A derivative failure is retained when
a later primal domain failure or resource stop supersedes it.

The overlap fixture gates allocation **inside** active evaluator invocations, with
immutable shared expression data, private bindings/targets/scratch and independently
owned results. The existing executor runs odd-sized batches at1/2/4, mixing successful
and unavailable derivatives; exact outcome, value/derivative bits and work agree.
No separate derivative executor, nested workers or shared scratch is introduced.

## Verification commands

On this workspace `../.tools/bin/{cmake,ctest}` is4.4.4; ordinary installed commands
work elsewhere. GCC13.3, GNU13 headers, libstdc++/libtsan runtime14.2, native ARM64
Jetson Ubuntu24.04.4, glibc/libm2.39 are the locally qualified environment. Strict
`-fno-fast-math -ffp-contract=off`, C++20 and warnings-as-errors remain required.

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release \
  -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
cmake --build out/build/release -j2
cmake -E chdir out/build/release ctest --output-on-failure
# Debug: separate out/build/debug, -DCMAKE_BUILD_TYPE=Debug.
# ASan+UBSan: out/build/sanitize, Debug, -DGENETIK_SANITIZERS=ON.
# Exact minimum: ../delivery-tools/cmake-min/bin/{cmake,ctest}, out/build/minimum.
# TSan: out/build/tsan, Debug, GENETIK_THREAD_SANITIZER=ON,
#       GENETIK_SANITIZERS=OFF, GENETIK_BUILD_EXAMPLES=OFF,
#       GENETIK_RECORDED_REFERENCE=OFF; use CTest4.4.4 for --repeat.
python3 scripts/check_hygiene.py --workspace --self-test
```

The package fixture compiles the actual derivatives quickstart against ordinary and
relocated installations after deleting its disposable producer source/build. Two
consumer translation units include/use the derivative API. Generic permutation
compilation dependencies explicitly exclude this mathematical header. Standalone
headers, strict-FP negative compilation, private warning/sanitizer flags, target
propagation, version/component checks, C++23 and source inclusion remain covered.

Detailed new raw logs, independent before/after captures and manifests live under
`out/logs/derivatives-*` and `out/runs/derivatives/`. Selected final records are retained
under `docs/derivative-results/`. Historical evidence is not rewritten.

## Executed results

The implementation is in `6812e54` (full SHA in environment.json). Final review
adds exact native-call/arithmetic attribution, per-operation admission boundaries
and inactive-path zero-work assertions to the same test binary. These additions
also pass focused runs in all five build modes, including the full derivative
contracts under TSan. No evaluator behavior changed during qualification.

| Gate | Result | Full-suite wall time |
| --- | --- | ---: |
| GCC13 Release, CMake4.4.4, Werror | 45/45 | 330.05 s |
| GCC13 Debug, CMake4.4.4, Werror | 45/45 | 327.29 s |
| GCC13 Debug ASan+UBSan | 45/45 | 691.02 s |
| GCC13 Release, exact CMake3.16.0, Werror | 45/45 | 173.56 s |
| Separate Debug TSan, full instrumentation | 8 selections x3, 24 successful executions | 2.49 s |

Suite wall times were obtained with other qualification work in progress and are
not performance measurements. Release/Debug native-counter assertion additions
were rebuilt and checked separately after their full-suite runs; ASan/minimum
binaries were updated before their derivative CTest entries. Exact manifests are
retained in derivative-results: **44 portable tests**, plus the one explicitly
opt-in captured snapshot for45 local tests. No portable test was excluded. CI
uses the portable manifest; no captured Jetson search is made a portable assertion.

The eight TSan selections, each explicitly present three times in tsan-ctest.txt,
are genetik_concurrency_contracts, genetik_oop_parallel, genetik_typed_overlap,
genetik_typed_preparation_overlap, genetik_scientific_contracts,
genetik_parsing_overlap, genetik_template_overlap and genetik_derivative_overlap.
All pass with halt_on_error=1 and no suppressions. ASan and TSan are separate builds.

Prime seed2, strict-arithmetic seed42 and arithmetic cadence4 captures taken from
the accepted pre-edit binary match candidate stdout **byte-for-byte**: initial
populations/fitness, final expression/fitness, committed generations, work,
improvement histories, stops and final RNG. Typed smoke/fitting data, protocol,
frozen models and results match the earlier accepted capture, excluding only
runtime=. Comparison hashes are retained in derivative-results/compatibility.json.
No stochastic discovery campaign was rerun or tuned.

Workflow lint (actionlint), exact portable/TSan manifest checks, workspace hygiene
and all127 frozen historical hashes pass. The two authoritative published asset
hashes remain unchanged; main and the annotated v0.1.0 tag are preserved.

## Development failures and limitations

The first test-only compile rejected misleading same-line conditionals under
-Werror and mixed float/double array deduction for the INFINITY macro. Split the
conditionals and use numeric_limits<double>::infinity(); full instrumentation and
numerical expectations were retained. The original diagnostic is retained in
out/logs/derivatives-first-build.log. No numerical expectation was weakened.

Hosted workflows are updated to include the new portable tests and derivative
TSan selection. They have **not executed for this feature**. The earlier hosted
qualification covers its identified base SHA, not this branch. Windows/MSVC,
AppleClang/macOS and libc++ remain unqualified. Finite differences and stochastic
campaign recovery are not the derivative correctness oracle. No derivative error
bound or global differentiability claim is made.

Independent per-configuration CTest JSON confirms the same45-test local manifest
in Release, Debug, ASan+UBSan and exact3.16.0. Final results, environment/runtime
package versions, effective flags, counter checks, replay hashes, preservation
record and development diagnostics are retained under derivative-results/ with a
SHA256 manifest. Raw isolated-package commands remain under the corresponding
out/build/MODE/tests/genetik-delivery-fixture/commands.log. All four suites pass
without runtime failures, skipped contracts, suppressions or timeout changes.
