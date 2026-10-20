# Phase 3 typed regression qualification

Local feature branch starts at Phase 2 `5770b1c` and retains cleanup `b444bdd`/
`e906cbd` and typed evaluation `d63f06c`/`5770b1c`. Checkpoint1 `f99b7a4` adds owned
problems/loss/preparation; checkpoint2 `8731344` adds bounded shape-aware evolution
and frozen operation-discovery evidence. The final coefficient-fitting checkpoint
also shares the existing coordinate kernel with the scalar adapter. Its commit is
reported at completion; qualified source identities accompany the records, avoiding
a self-referential documentation/SHA commit loop.

Environment: Jetson Orin NX, Ubuntu24.04.4 ARM64 Linux6.8.12-1021-tegra; GCC13.3.0
with release13 libstdc++ headers, installed libstdc++6/libtsan2 14.2.0-4ubuntu2~24.04.1,
glibc/libm2.39-0ubuntu8.8. CMake/CTest4.4.4 and exact3.16.0. Native binary64,
-fno-fast-math -ffp-contract=off, default nearest rounding/gradual underflow.
New hosted workflow definitions are configured only; no hosted Phase 3 job executed.
Historical hosted/support results retain their own recorded revisions.

## Contracts exercised

- Independent scalar, vector/component-normalized and rectangular matrix MSE;
  bad schemas/output kinds, detached data, same-schema different-target identity,
  changed-expression invalidation and evaluation-only cache isolation.
- Late-row invalidity cannot produce a favorable prefix loss. All-row reporting
  counts invalid predictions without inventing component counts. Local resource
  exhaustion after partial rows retains accounting and preserves its exception
  classification through failed preparation. Loss overflow is Invalid, never a
  favorable nonfinite sentinel.
- Shape-aware root/internal replacements with unequal sizes; destination depth,
  node/coefficient limits, higher aggregate variable identities, operation-only
  output without terminals, dot/rectangular matmul/transpose construction, impossible
  grammars and changed mutation/crossover. Complete plans are revalidated.
- Independent aggregate coefficients, exact returned value/fitness correspondence,
  unchanged structure/input values, deterministic repetition, invalid probes after
  accepted progress, infeasible/local-limit/coupled-zero outcomes and repeated bounded
  inherited refinement. Cadence uses prospective committed generations.
- Nonbinding serial/2/3/4 worker comparisons preserve initial values/fitness, final
  value/fitness, work/arithmetic counters and final RNG. Fitting comparisons include
  preparation/cadence statistics. Forced two-worker overlap includes actual fitting.
- Scheduled/unscheduled binding budgets never exceed admission; committed best/fitness/
  generation survive partial private improvements then sibling exception, resource
  failure and cancellation. Original operational cause is retained. All-invalid
  initialization has no committed generation/best. Existing generic failure tests
  continue to cover executor startup/drain/multiple-failure precedence and OOP hooks.
- Public primitive diagnostics share the original interpreter, including no-charge
  domain failures and allocations failing after arithmetic already executed. Invalid
  or throwing calls cannot contaminate later independent evaluation.

The scalar adapter's shared-kernel refactor was checked against an independently
captured pre-edit executable and the unchanged historical files: full prime seed2,
strict-arithmetic seed42 and short cadence4 replays match byte-for-byte (initialization,
fitness bits, populations, accepted fitting/work, improvements, stop and final RNG).
No unrelated research campaigns were rerun.

## Reproduce local gates

Substitute ordinary cmake/ctest when available. On this workspace they live at
`../.tools/bin/{cmake,ctest}`; minimum at `../delivery-tools/cmake-min/bin/`.

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
cmake --build out/build/release -j2
cmake -E chdir out/build/release ctest --output-on-failure
```

Repeat with Debug/out/build/debug; Debug/out/build/sanitize plus
GENETIK_SANITIZERS=ON; exact CMake/CTest3.16.0/Release in out/build/minimum.
GENETIK_RECORDED_REFERENCE=ON is qualified Jetson-only. Portable manifest has32 cases;
local captured snapshot adds one. New cases are typed regression contracts, forced
preparation overlap, installed-style typed search quickstart and bounded app CLI.

```sh
cmake -S . -B out/build/tsan -DCMAKE_BUILD_TYPE=Debug -DGENETIK_THREAD_SANITIZER=ON -DGENETIK_SANITIZERS=OFF -DGENETIK_BUILD_EXAMPLES=OFF -DGENETIK_WARNINGS_AS_ERRORS=ON
cmake --build out/build/tsan --target genetik_concurrency_tests genetik_oop_consumer genetik_typed_tests genetik_typed_regression_tests -j2
TSAN_OPTIONS=halt_on_error=1 cmake -E chdir out/build/tsan ctest -R 'genetik_concurrency_contracts|genetik_oop_parallel|genetik_typed_overlap|genetik_typed_preparation_overlap' --repeat until-fail:3 --output-on-failure
python3 scripts/check_hygiene.py --workspace --self-test
```

CTest3.16 predates --repeat: run its four selected tests explicitly in a small shell
loop if qualifying TSan with that tool. The local TSan repetition above uses4.4.4.
Full search campaigns run in Release only, outside routine CTest. See frozen
TYPED_DISCOVERY_PROTOCOL.md and TYPED_FITTING_PROTOCOL.md; data/reference generation
and post-selection reporting are excluded from recorded search/fit elapsed times.

Delivery gates exercise all new headers standalone, compiler warnings as errors,
unsafe FP modes rejected by affected public headers, source inclusion and ordinary/
relocated installed multi-TU C++20/C++23 consumers after disposable producers are
removed. Actual typed search/fitting quickstart sources compile/run serial and with
workers; permutation dependency inspection excludes every typed header. No new public
or numerical dependency, compiled binary, sanitizer runtime or minimum-CMake change.
The FP check timeout is90s to cover twelve intentional header/compiler failures;
its numerical/diagnostic assertions are unchanged.

Results and retained log/source identities are recorded below after completion.

Two contained typed-adapter defects were repaired during qualification: ordinary
preparation now records its work once even if returning the scored expression
allocates and throws; dataset configuration validation and grammar witnesses no
longer require an unused large binding to fit operand scratch. A constant scalar
model with a large unused vector and a one-coefficient temporary limit is valid.
Infeasible variable terminals are excluded while the original complete-expression
limits remain enforced. Focused regression fixtures cover both cases. These repairs
do not alter the frozen campaign's feasible plans, RNG or numerical rules.

## Executed results

| Gate | Outcome |
| --- | --- |
| Release, warnings as errors |33/33; final forced new-target rebuild/full rerun155.21s |
| Debug, warnings as errors |33/33; final affected eight cases also pass |
| Debug ASan+UBSan, full instrumentation |33/33,509.41s; latest source forced rebuild of combined contract also passes109.41s |
| Exact CMake/CTest3.16.0 Release |33/33,132.64s; final affected eight also pass |
| Separate Debug TSan |Four selected overlap/OOP/backend cases, each three repetitions; all pass |
| Public headers, warnings, strict FP |All affected sole-header units compile; twelve unsafe-mode compilations reject as intended |
| Installed/source/relocated/multi-TU consumption |Pass in each full suite, including typed search and public fitter, serial/parallel quickstarts, generic OOP dependency exclusion |
| Serial scalar reference replays |Prime2/arithmetic42/cadence4 identical to independent pre-edit captures and historical files |
| Final typed replay |All five frozen discovery models/work/traces/metrics and coefficient-fixture results identical; timings/new fields excluded |
| CI manifests/workflow syntax |32 portable, four TSan; manifest assertions and actionlint pass; hosted not run |
| Workspace/preservation |Hygiene passes;127 frozen evidence files, published archive/checksums, main and v0.1.0 unchanged |

Retained logs, exact source SHA256s, environment, manifests, replay comparisons and
preservation identities are in [typed-regression-results/qualification](typed-regression-results/qualification/).
Complete local build/test output remains in out/logs/typed-regression; builds are
out/build/{release,debug,sanitize,minimum,tsan}. No producer/source paths or private
sanitizer/profiling dependencies leak through the installed package.

An initial combined ASan fixture timed out at90s (32 other cases passed); the complete
failure is retained. Stage isolation and a debugger sample found CPU-bound sanitizer
fake-stack bookkeeping in small accessor calls. Predictably incompatible grammar
shapes now use the validator's shared shape rule before construction, avoiding
routine exception-based planning, without changing feasible choices/RNG. All fixed
operator/numerical/failure fixtures and original sanitizer instrumentation remain.
The combined bounded fixture allowance is300s and CLI180s; full qualification uses
default sanitizer options, **not** the debugger-only leak-check adjustment. Final
combined case110.48s and latest-source rerun109.41s pass, with no memory/UB report.
No timing speedup is claimed from this diagnostic. Forced overlap still executes.

Engineering acceptance is complete locally. Five-seed operation discovery and the
separate supplied-structure eight-coefficient recovery are measured sample evidence;
no mathematical-equivalence/global-optimizer claim is inferred. Known limitations:
finite conservative witness planning, fixed schemas/shapes and equal-component MSE,
independent constants, coordinate stalls on coupled parameters, no typed text input,
and no new hosted qualification. Next scope is Phase4's individually specified
scientific functions, not a parser/calculus/backend expansion in this milestone.
