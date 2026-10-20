# CPU execution qualification and fixed-work measurements

Starting revision 968e992e897ac77019b1bd3e0547faad878a424b, clean. Implementation
and API contracts: [CONCURRENCY.md](CONCURRENCY.md). No historical campaign or its
unsuccessful outcomes was replaced. All 127 historical evidence hashes match.
Version 0.1.0 remains unreleased; no push/tag/publication occurred.

## Local engineering results

Jetson ARM64 Linux 6.8.12-1021-tegra, Ubuntu 24.04.4; GCC 13.3.0 with libstdc++
release-13 headers (__GLIBCXX__ 20240904). Actual dynamic runtime packages are
libstdc++6 and libtsan2 **14.2.0-4ubuntu2~24.04.1**, libc6 **2.39-0ubuntu8.8**.
Earlier shorthand “GCC/libstdc++13” describes compiler/headers, not this installed
DSO package version. Runtime metadata and ldd are recorded separately.

| Gate | Outcome | Suite time (not a performance comparison) |
| --- | --- | ---: |
| Release / CMake 4.4.4, Werror | 25/25 | 79.67 s |
| Debug / CMake 4.4.4, Werror | 25/25 | 101.48 s |
| Debug ASan+UBSan / CMake 4.4.4, Werror | 25/25 | 271.02 s |
| Release / exact CMake+CTest 3.16.0, Werror | 25/25 | 75.16 s |
| Separate Debug TSan, explicit halt_on_error=1 | 2 cases × 10 executions passed | 4.98 s |

After the final CLI-only change protecting option values named `--threads`,
Release passed the complete suite again (time above); affected CLI/character checks
also passed 3/3 in Debug (0.24 s), ASan+UBSan (29.73 s), and exact CMake 3.16.0
(0.16 s). The earlier full suites qualify the unchanged backend.

All full builds compile standalone public/support headers, including Execution and
ExecutionOptions. Existing strict-FP negative checks, source inclusion/parent
isolation, ordinary/relocated package/version checks, multi-TU scalar/symbolic
consumers and quickstarts pass. Installed consumers link only Genetik::Genetik;
Threads::Threads resolves through package find_dependency, without manual flags
or development runtimes. Installed permutation and quickstarts run serial and
parallel; source consumption also runs parallel refinement/cadence and character
work. Producer source/build trees are removed in the isolated relocation fixture.
The existing CMake 3.16.0 tool/setup was reused, not a relaxed minimum.

Forced barriers/latches establish real overlap and ceiling compliance at 1/2/3/4,
small/singleton/odd populations, reversed arrival and stable tie ordering. Tests
include serial-subclass rejection without opt-in, virtual preparation/fitness plus
installed roulette/crossover/mutation overrides, partial startup/growth failure,
submission-copy/detach/active allocation failure, queue cancellation/draining and
clean reuse. Independent engines share immutable data with private run state.

Training-loss admission is checked against an independent atomic compute-start
record, including simultaneous final-permit contention at 0/1/3/4, local limits,
exact allowance/one short, and tiny remaining budgets. Scheduled and unscheduled
fault/budget attempts preserve all committed candidate data, fitness bits, generation
and best; a controlled sibling accepts private fitting progress before another
throws. Retried preparation keeps the prospective generation/cadence. Multiple
errors retain their slot causes and operational priority over budget/cancellation.
All-invalid and mixed validity behave without retries. Tests run strict numerical
cases on workers (signed zero/subnormal/domain/inner-overflow/higher-index inputs),
and check FP rounding restoration after a callback changes its worker environment.

Contradictory training rows ensure the bounded equivalence fixture runs all five
generations without relying on discovery. Across N=1/2/3/4, cadence 1/4, exact
candidate values/fitness, raw/prepared initialization, every work/variation/preparation
counter, committed history, stops and final RNG match. Three full serial stdout
captures (prime seed2, arithmetic42, short cadence4) match accepted recorded snapshots
byte-for-byte. The short cadence4 replay also matches at four threads. Timings and
new stderr configuration are excluded. No serial-identical binding-limit/failure
promise is inferred from these nonbinding results.

TSan preflight succeeded immediately with the actual compiler/runtime. The full
instrumented forced-overlap and external OOP fixtures passed repeatedly without
suppression. Standard/runtime libraries were not rebuilt with instrumentation;
this qualifies tested header backend code and callback fixtures, not arbitrary
user implementations. The hosted matrix now includes these contracts and a separate
ARM64 TSan step, **configured but unexecuted**. No other architecture/compiler/stdlib
combination or Windows/AppleClang support is newly claimed.

## Declared fixed work and measurement method

The predeclaration and raw CSV are in [concurrency-results](concurrency-results).
Each batch prepares the same 64 owned expression values: the accepted prime
representative on 1,000 fitting rows, or the larger mixed strict-arithmetic fixture
on 25 fitting rows. First-constant offsets are .125*(i%5-2); topology stays fixed.
Refinement uses bounds [-8,8], step1/min1e-8, 256 losses/128 sweeps/8192 proposals,
no local threshold and a nonbinding 1e9 global cap. These are fixtures, not GP target
injection. Exact full-population expression/fitness fingerprints and all listed work
increments are compared against the first serial batch on every timed run.

One untimed fit warms each workload. Five rounds alternate order 1,2,4 / 4,2,1.
Each mode first measures a fresh context/population (startup-inclusive), then
prepares the same original values with its existing pool (reuse). Timings include
service creation/forks, actual scoring/fitting, merge/order/publication; exclude
immutable corpus construction, fingerprint/output and post-selection reporting.
No per-node/row clock or detailed profiler runs in the timed process. Results are
consumed/validated; volatile strict arithmetic and observable fits/work prevent
elimination. The preliminary run predating serial cancellation guard qualification
was retained only under ignored out/concurrency; final measurements use final code.
No workload/configuration was tuned from timing or accuracy outcomes.

Prime batches: **13,888 actual loss attempts / 13,888,000 rows / 64 fits**.
Arithmetic mixed batches: **3,776 attempts / 94,400 rows / 64 fits**. Each cold/reuse
mode performs identical measured work and produces identical numerical results.
These contain valid full-row losses, not throughput inflated by cheap invalidity.
| Workload / phase | N | Median seconds [min, max] | Loss/s | Rows/s | Paired speedup median [min, max] |
| --- | ---: | --- | ---: | ---: | --- |
| prime / startup | 1 | 1.356705 [1.355657, 1.405371] | 10237 | 10236568 | 1.000 [1.000, 1.000] |
| prime / startup | 2 | 0.689492 [0.686571, 0.711018] | 20142 | 20142364 | 1.975 [1.967, 2.022] |
| prime / startup | 4 | 0.365066 [0.352506, 0.371382] | 38042 | 38042465 | 3.715 [3.653, 3.966] |
| prime / reuse | 1 | 1.355184 [1.353599, 1.357552] | 10248 | 10248056 | 1.000 [1.000, 1.000] |
| prime / reuse | 2 | 0.680368 [0.679183, 0.681249] | 20412 | 20412495 | 1.992 [1.987, 1.995] |
| prime / reuse | 4 | 0.340789 [0.340561, 0.343069] | 40753 | 40752510 | 3.977 [3.946, 3.984] |
| mixed / startup | 1 | 0.035876 [0.020099, 0.040582] | 105252 | 2631308 | 1.000 [1.000, 1.000] |
| mixed / startup | 2 | 0.011681 [0.011452, 0.012722] | 323262 | 8081546 | 3.093 [1.604, 3.474] |
| mixed / startup | 4 | 0.006802 [0.006357, 0.008070] | 555157 | 13878924 | 4.926 [2.955, 5.644] |
| mixed / reuse | 1 | 0.027606 [0.017409, 0.029389] | 136781 | 3419531 | 1.000 [1.000, 1.000] |
| mixed / reuse | 2 | 0.011014 [0.010433, 0.012600] | 342848 | 8571207 | 2.333 [1.554, 2.667] |
| mixed / reuse | 4 | 0.007089 [0.006086, 0.008391] | 532686 | 13317153 | 3.502 [2.505, 4.536] |

Eight online/allowed CPUs (0–7), schedutil governor, no pinning/power setting changes.
No Genetik build/tests ran during final timing; ordinary desktop background activity
remained. This is not an isolated/fixed-frequency machine. Prime medians show useful
near-fourfold reused-pool throughput at four tasks; startup-inclusive is ~7% slower
than reuse at N=4, which includes more than isolated thread-launch cost. Arithmetic
batches are short and variable; observed superlinear ratios are not a scaling law
or an attributed CPU effect. No parallel slowdown occurred in these two measured
workloads; cheap/no-fit workloads are not thereby guaranteed faster. Serial stays
default. More tasks/generations or better equations are not claimed by this experiment.

Process VmRSS and getrusage high-water snapshots are in memory-and-work.txt/CSV.
They cover the whole process including retained allocator/reporting/data pages,
not per-worker allocations or isolated pool peaks. Kernel accounting and sample
points differ; do not subtract the two readings. Both workloads stay below 5 MiB in
these coarse resident snapshots; virtual stack reservations are not measured.

## Reproduction

From Genetik (substitute ordinary cmake/ctest when available):

```sh
../.tools/bin/cmake -S . -B ../build-release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake -S . -B ../build-debug -DCMAKE_BUILD_TYPE=Debug -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake -S . -B ../build-sanitize -DCMAKE_BUILD_TYPE=Debug -DGENETIK_SANITIZERS=ON -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure
../delivery-tools/cmake-min/bin/cmake -S . -B ../build-min-contracts -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../delivery-tools/cmake-min/bin/cmake --build ../build-min-contracts -j2
../delivery-tools/cmake-min/bin/cmake -E chdir ../build-min-contracts ../delivery-tools/cmake-min/bin/ctest --output-on-failure
../.tools/bin/cmake -S . -B ../build-tsan -DCMAKE_BUILD_TYPE=Debug -DGENETIK_THREAD_SANITIZER=ON -DGENETIK_SANITIZERS=OFF -DGENETIK_BUILD_EXAMPLES=OFF -DGENETIK_WARNINGS_AS_ERRORS=ON
../.tools/bin/cmake --build ../build-tsan --target genetik_concurrency_tests genetik_oop_consumer -j2
TSAN_OPTIONS=halt_on_error=1 ../.tools/bin/ctest --test-dir ../build-tsan -R 'genetik_concurrency_contracts|genetik_oop_parallel' --repeat until-fail:10 --output-on-failure
mkdir -p out/concurrency-new
../build-release/genetik_concurrent_benchmark >out/concurrency-new/timings.csv
../build-release/genetik_evaluation_workspace --replay prime >out/concurrency-new/prime.txt
cmp out/concurrency-new/prime.txt docs/evaluation-results/accepted-prime.txt
../build-release/genetik_evaluation_workspace --replay arithmetic 4 --threads 4 >out/concurrency-new/interval4.txt
cmp out/concurrency-new/interval4.txt docs/evaluation-results/accepted-interval4.txt
```

README/CONCURRENCY.md provide CLI and C++ usage. Package uses the documented CMake
[Threads imported target](https://cmake.org/cmake/help/v3.16/module/FindThreads.html)
and [dependency macro](https://cmake.org/cmake/help/v3.16/module/CMakeFindDependencyMacro.html).
No mandatory mathematical dependency or compiled package binary was added. Detailed
refinement diagnostics remain serial-only; arbitrary callbacks need explicit safety
and fresh independent factories. Next qualification is actual hosted execution and
release readiness; caching, cross-call scratch reuse, optimizers, GPU and broader
cancellation/observer facilities remain separately scoped.
