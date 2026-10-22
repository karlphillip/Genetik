# Exact rounded-prime prefix: implementation and local evidence

This is an application-only addition following published v0.2.0 (f9f300ed).
Implementation commit: `6cde4e5` on `feature/prime-exact-search`. No library public
header, generic engine, original prime configuration or historical campaign output
changed. [Usage and contract](PRIME_EXACT_SEARCH.md).

## Frozen smoke protocol and observations

One Release run, seed 42, four requested/effective workers, population 64, unchanged
31-node/depth 6 +,-,*,/,log grammar, unchanged mixed constants and coordinate fitter,
cadence1, 100,000 actual loss attempts, 0.05-second reporting deadlines, top5.
The [protocol](prime-exact-results/protocol.json) was written before execution.
No target formula was supplied. There was no recovery-driven tuning or retry.
The executable was built from the implementation working tree immediately before
6cde4e5; the subsequent commit also adds standalone compilation of its two support
headers. [Complete output](prime-exact-results/search.txt),
[summary](prime-exact-results/summary.json), [timing](prime-exact-results/timing.json)
and [hash manifest](prime-exact-results/SHA256SUMS) retain this observation.

The run stopped at the loss allowance, without exact recovery. The best committed
model reached **20/1000 rounded matches**, 980 mismatches, exact-prefix loss
980.93245930047055. Its raw-unit RMSE was 843.60242384613457, MAE 448.12885474535716,
maximum absolute error 2785.6062761362691; nodes 15, depth 6, three constants:

```text
g(x0) = (x0 * (((((-0.5) * 5.3382963691547651) / (x0 - 6.19957815074509)) + x0) - (x0 * x0)))
p_hat(n) = 10000 * g(double(n) / 1000.0)
```

There were 14 snapshots: initial, 12 periodic, final. Selected header fields:

```text
snapshot=periodic elapsed_seconds=3.149813689 committed_generation=12 attempted_generations=12 published_generations=12 actual_loss_attempts=92545 remaining_allowance=7455 threads_requested=4 threads_effective=4
snapshot=final elapsed_seconds=3.442740013 committed_generation=12 attempted_generations=13 published_generations=12 actual_loss_attempts=100000 remaining_allowance=0 unpublished_loss_attempts=7455 stopping_state=evaluation_budget
```

The final failed private preparation consumed 7,455 loss attempts and left generation 12
committed. Total work: 100,000 objective calls, 99,876,124 training rows, 879 fitting
baselines, 99,121 probes, 8,072 accepted fitting steps, 265 mutation changes and 684
crossover changes. Reporting independently evaluated 70,000 rows and charged zero
loss attempts. Parent-process elapsed time 3.475s; child maximum RSS 11,392KiB.
These are one device observation, not performance/recovery guarantees. Work near a
binding budget is allowed to be thread-scheduling dependent; the complete observed
output is retained, not presented as a portable trajectory fixture.

An initial launcher attempted `/usr/bin/time`, which is unavailable on this machine;
no search started. The same predeclared command was then launched once through
Python subprocess with monotonic time and child resource accounting. That launcher
diagnostic remains in out/runs/prime-exact/smoke/launcher-unavailable.txt.

## Qualification

The deterministic fixtures independently check the integer mismatch objective,
secondary loss, mismatch priority, halfway-away rounding, invalid/unrepresentable
predictions, late invalid rows and owned references. Fake monotonic timestamps
exercise initial/deadline/catch-up/final scheduling without sleeps. Committed known
scores exercise score/nodes/text/slot ordering, duplicates and fewer valid entries.
Reporting on/off has identical RNG, objective counters, best expression and fitness;
output failure cannot undo already committed state. A budget stop during fitting
preserves the initial committed members and best with actual unpublished work counted.

A two-party barrier forces actual candidate/fitting overlap at workers 2 and 4. Runs
at 1/2/4 compare committed individuals, fitness, best, work and final RNG. The CLI
fixture validates inputs, runs bounded periodic reports and sends SIGINT only after
an initial committed snapshot; workers drain and a cancelled final report follows.
It does not use stochastic target recovery as an acceptance gate.

Build/test results and environment are recorded below.
Hosted workflows include the three new portable tests and the additional forced-
overlap TSan selection; authored coverage does not constitute hosted execution.

Reproducing all first 1,000 supplied primes would demonstrate an exact fit to that
finite prefix. It would not establish a general closed-form formula for all prime
numbers. This smoke run did not reproduce that prefix.

## Executed local gates and retained failures

GCC13.3.0, GNU libstdc++13 (`__GLIBCXX__=20240904`), Linux aarch64 Jetson,
glibc/libm2.39. CMake4.4.4 and exact3.16.0. All configured builds use
-Wall/-Wextra/-Wpedantic/-Werror, C++20, -fno-fast-math and -ffp-contract=off.
ASan+UBSan and TSan are separate and fully instrument compilation and linking.
[Environment](prime-exact-results/environment.json),
[manifests and results](prime-exact-results/qualification.json).

| Gate | Result |
| --- | --- |
| Release full suite | 56/56,339.90s |
| Debug full suite, after contention diagnosis | 56/56,261.74s |
| Exact CMake3.16.0 Release full suite | 56/56,237.37s |
| ASan+UBSan full suite | 55/56 initially; new CLI deadline repaired; all three affected tests then passed in82.48s; all56 checks covered |
| New exact tests after harness repair | 3/3 in each of Release,Debug,ASan+UBSan,minimum-CMake |
| Separate TSan final recheck | 11 named selections,three repetitions each:33/33, plus complete integral contracts |
| Workflow/manifest/Python/hygiene | actionlint, exact portable/TSan name sets, AST syntax, repository and parent workspace passed |

The full suites build all standalone public/application headers and cover strict-FP
rejection, downstream source inclusion, installed/relocated prefixes with removed
producers and spaces in paths, multi-TU/scalar/symbolic/typed consumers, nonmathematical
OOP isolation, character evolution, existing prime CLI and the qualified local captured
reference. There is no shared numerical/engine-source change requiring a new search
replay or another historical campaign.

Full C++ builds/suites cover6cde4e5951b0ef83d6f92962d50142fc30818e8c.
The harness-only correction is16f5cc47d2e703e005f3dc41ecc62bf8d6673f09; all
three new tests were rechecked in all four configurations, and all TSan selections
were repeated at that revision. The subsequent evidence commit changes documentation
only. The ASan result is full-suite plus affected rechecks, not a claimed fresh
single-pass56/56 full suite after the repair. The other53 unchanged tests retain
their original full-suite results.

There are55 portable tests; the only exclusion is `genetik_evaluation_reference`,
the opt-in Jetson/GCC13/libm captured snapshot. All56 ran locally. The exact named
lists are retained in qualification.json, including the three new tests
`genetik_prime_exact_contracts`, `genetik_prime_exact_overlap`,
`genetik_prime_exact_cli`. The11 TSan selections are listed with per-name counts in
[tsan-repetitions.json](prime-exact-results/tsan-repetitions.json); no selection is
inferred merely from a total count.

Retained development failures:

- The first Debug run hit the existing300-second installed-package timeout while
  several qualification builds compiled concurrently on eight available CPUs.
  [Original full log](prime-exact-results/debug-timeout.txt). No compiler/assertion
  error appeared; the same full suite, unchanged, passed once competing builds ended.
  Package verification took234.21s on that rerun; its300-second timeout was retained.
- The first instrumented exact CLI test hit its new25-second subprocess deadline.
  [Original full log](prime-exact-results/asan-cli-timeout.txt). The identical256-loss
  command, fully instrumented, committed generation1 at24.99s and completed normally
  in64.67s with all256 loss calls and five reports. [Measured diagnostic](prime-exact-results/asan-cli-diagnostic.json).
  The repair increases only this harness's subprocess limit to120s, cancellation
  waits to60s and its CTest ceiling to180s. Work, assertions and instrumentation are
  unchanged. The complete three-test ASan recheck passed in82.48s with no finding.
- CTest3.16 ignores --test-dir; one initial launcher therefore ran no tests and is
  not counted. Replaced it with `cmake -E chdir out/build/minimum <ctest3.16>
  --output-on-failure`. Its two root Testing/ files were inventoried and removed;
  the valid minimum-version full suite then passed56/56. Launcher logs and the
  cleanup mapping remain under out/logs/prime-exact/ and out/runs/prime-exact/.

Complete configure/build/test logs remain under out/logs/prime-exact/. The smoke and
rechecked source identities, manifests, preservation checks and root-workflow audit
remain under out/runs/prime-exact/. Ordinary commands (with installed CMake/CTest):

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
cmake --build out/build/release -j2
cmake -E chdir out/build/release ctest --output-on-failure
python3 scripts/check_hygiene.py --workspace
```

The captured-reference option is for the documented Jetson environment; other
platforms use OFF and the55-test portable manifest. On this workspace tools are
../.tools/bin/{cmake,ctest}; minimum tools remain ../delivery-tools/cmake-min/bin/.
No hosted run or new platform qualification is claimed. Both published archives
and checksum files match their recorded identities, all127 frozen evidence hashes
match, and main, tags and historical campaign records are unchanged. Bounded prime
and character runs from the repository root created no unexpected root output.
