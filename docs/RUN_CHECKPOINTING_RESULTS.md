# Run observability/checkpoint qualification

Implementation starts from 853e1875a67c19365e10a25daff6aa01ec50c6cc on the local
feature/run-checkpointing branch. Main, releases/tags/assets and accepted campaigns
are preserved. [API and numerical/persistence contracts](RUN_CHECKPOINTING.md).

## Frozen demonstration protocol

Seed 42, four workers, unchanged prime population 64, grammar/fitter/cadence and exact-
prefix objective. No target expression or prime approximation is injected. Default
single-best pretty display, report 0.1s, checkpoint 0.15s, retain three generations,
JSONL sink. Initial total allowance 1,000,000; after at least three displayed snapshots,
request SIGINT, retain its final checkpoint, destroy the process. Resume the same
checkpoint with total allowance 2,000,000; after continued evolution and at least
three snapshots, request SIGTERM. These limits/signals demonstrate observability
and recovery, not a stochastic recovery/performance claim. No tuning or success-
dependent retry is allowed. Selected observations and qualification follow below.

## Deterministic acceptance

The independent small three-observation problem cannot reach an exact fit with the
three-node/depth-two grammar, avoiding an optimum-dependent test gate. A fully
serialized final state compares all committed expressions/coefficient bits/caches,
selection data, archive and best generation, MT19937 state, objective/preparation/
variation counters and improvement history. Generation 100 matches a clean split at 40,
including separate processes and nonbinding worker 1/2/4 runs. Elapsed time, snapshot
timing and reporting rows are operational observations: resume adds a forced
snapshot, so these are not required to equal uninterrupted output. Their actual
counts are restored and continued separately from training work. Gate-controlled parallel
initial/preparation work forces actual overlap; reports/checkpoints remain coordinator-
only after drain. A deterministic interrupted private fitting generation retains G40,
counts attempt 41 and post-failure RNG/work, then resumes by constructing G41.

Envelope/codec tests cover malformed/truncated/corrupt files, bad checksum, old/new
format IDs, wrong runtime/codec/problem/configuration, invalid counters, input limits,
I/O errors, allocation/pre-rename injected failure, retention and rollback rejection.
Codec tests preserve every scalar operator, variable identity, power metadata,
signed zero, subnormal/extreme finite coefficients and valid/invalid cached fitness.
Unicode/LaTeX tests preserve grouping, operand order and executable canonical text;
winning output, normalized presentation endpoints, JSONL, default/top/raw modes and
total-budget resume are tested without stochastic recovery requirements.

An existing-objective boundary is deliberately retained: native binary64 secondary
ratios/sums can make valid loss 1001. The new presentation accepts fitness 0 there;
the objective is not clipped or otherwise changed. A 31-node product fixture tests
this exact behavior. Full loss remains authoritative for all search decisions.

Early retained development diagnostics under out/logs/checkpoint/ include Werror
indentation/aggregate warnings and a test-fixture name lookup error: inherited
limits() was selected instead of the intended ::limits() during test subclass base
construction. The fixture was corrected before sanitizer qualification. An initial
expanded pretty equation omitted grouping around the complete scaled expression;
that was fixed and receives an explicit regression test. No numerical expectation,
instrumentation or existing contract was weakened.


## Executed prime cancellation/resume demonstration

Frozen behavior commit: `f7f98f1ddc37c692eedeaef4d6d0d8a33ff23405`.
[Protocol and selected evidence](run-checkpoint-results/protocol.json),
[complete observations](run-checkpoint-results/summary.json),
[first process](run-checkpoint-results/sigint.txt),
[resumed process](run-checkpoint-results/resumed-sigterm.txt), and
[all eight JSONL snapshots](run-checkpoint-results/metrics.jsonl) are retained with
[SHA256 identities](run-checkpoint-results/SHA256SUMS). Binary runtime-specific
checkpoints remain under `out/runs/checkpoint/smoke/`; their identities are in the
summary. These are new observations, not replacements for historical campaigns.

| Process | Displayed committed generations | Final actual loss attempts | Attempted/published generations | Best exact matches | Signal / exit |
|---|---|---:|---|---|---|
| Fresh, total budget 1,000,000 | 0, 1, 2, 2 | 13,195 | 3 / 2 | 11 / 1000 | SIGINT / 0 |
| New process, total budget 2,000,000 | 2, 3, 4, 4 | 28,166 | 6 / 4 | 16 / 1000 | SIGTERM / 0 |

Both signals interrupted private work: final unpublished loss attempts were 1,980
and 1,541 respectively. The committed population remained at G2/G4, with actual
post-failure work and RNG saved. Reporting performed 4,000 additional prediction
rows per process and zero training-loss attempts. Retention left G2, G3, G4 and
`latest.gtkc`; latest and G4 bytes match. Resume inherited seed 42, four workers and
fitting settings, verified the problem/checksum, and continued G3 and G4. Report
and checkpoint intervals were 0.1s and 0.15s; process wall times were approximately
0.82s and 0.97s. This is a bounded functionality demonstration, not a throughput
benchmark or exact recovery claim.

Final selected canonical expression:

```text
((((-0.60684001445770264) - log(x0)) * ((0.75730464850412016 * (-0.0078383503184227266)) * (5.5261699423069501 + x0))) - (x0 * (1.0001291334629059 / (-1.3875602036714554))))
```

Its exact-prefix loss was 984.94336327007227, presentation fitness 0.016040596133793938,
raw RMSE 323.66773995756682, with 18 nodes/depth 5/six constants. No configuration was
changed based on these results and no seed was retried. This agreement concerns
only the supplied finite prefix; no general prime-number formula is claimed.

A short earlier displayed model illustrates the readable/canonical separation:

```text
Prime Search • Generation 2 • 00:00:00
Fitness        0.01101003
Exact primes   11 / 1000
Best formula
p̂(n) = 10000 · ((n/1000) − 3.2168000042438507·0.0625)
Complexity     5 nodes • depth 3 • 2 constants
Evaluations    13195 / 1000000
Stopping state cancelled
```


## Local qualification scope

All compiled sources were frozen at
`f7f98f1ddc37c692eedeaef4d6d0d8a33ff23405`. Fresh build directories were
`out/build/checkpoint-{release,debug,sanitize,minimum,tsan}`. Later changes only
record documentation/evidence; they do not change executable behavior.

Environment: native Jetson Linux ARM64 (`6.8.12-1021-tegra`), GCC13.3.0 Ubuntu
13.3.0-6ubuntu2~24.04.1, libstdc++13 headers (`__GLIBCXX__=20240904`), GNU runtime
`libstdc++.so.6.0.33`, glibc/libm2.39 (Ubuntu2.39-0ubuntu8.8). CMake/CTest4.4.4
and exact3.16.0 were exercised. Warnings-as-errors and strict C++20 FP options
`-fno-fast-math -ffp-contract=off` were enabled. Separate Debug targets used
`-fsanitize=address,undefined` or `-fsanitize=thread`, with matching link flags and
frame pointers; no sanitizer suppression was added and instrumentation stays private.

The local manifest includes the captured qualified Jetson snapshot. The exact portable
manifest excludes only `genetik_evaluation_reference`:61 tests, checked by
`scripts/check_ci_manifest.py`. All new contracts/CLI/quickstart tests are present:
`genetik_checkpoint_contracts`, `genetik_checkpoint_overlap`,
`genetik_checkpoint_process`, `genetik_checkpoint_cli`,
`genetik_checkpoint_quickstart`, `genetik_formatting_contracts`.

The 12 named separate TSan selections each passed three times (36 successful
executions, checked per name), with `TSAN_OPTIONS=halt_on_error=1`:

```text
genetik_concurrency_contracts
genetik_oop_parallel
genetik_typed_overlap
genetik_typed_preparation_overlap
genetik_scientific_contracts
genetik_parsing_overlap
genetik_template_overlap
genetik_derivative_overlap
genetik_derivative_regression_overlap
genetik_integral_overlap
genetik_prime_exact_overlap
genetik_checkpoint_overlap
```

The complete `genetik_integral_contracts` additionally passed once under TSan.
New checkpoint overlap itself completed three times in 22.30/22.40/22.69s and
forces real worker 2/4 overlap, synchronous pretty/JSONL reporting and checkpoint
save/destroy/load continuation. An earlier instrumented preflight also passed.
These are correctness checks, not controlled performance measurements.

Three accepted serial replays match their historical stdout byte for byte:
prime, strict arithmetic and arithmetic cadence4. Their grammar, numerical evaluator,
objective and default draws are unchanged. No discovery campaign was rerun.
Workflow lint (`actionlint`), Python syntax-only validation and repository/parent
hygiene passed. All 127 frozen historical SHA256 identities and all four published
v0.1.0/v0.2.0 archive/checksum identities match. Main/tags and accepted campaigns
are unchanged. No remote write or hosted workflow was requested or executed.

The hosted workflow is configured for the new 61-test manifest and 12 TSan selections;
it remains unexecuted for this operational feature. This local evidence does not
newly qualify libc++, Windows/MSVC, macOS/AppleClang or another compiler/runtime.
Checkpoint continuation specifically requires the qualified matching build/runtime,
worker count, immutable configuration and explicit application identity; its checksum
is an accidental-corruption check, not authentication of arbitrary files.


Full fresh-suite results at the frozen implementation SHA:

| Configuration | Result | Full suite elapsed |
|---|---|---:|
| GCC13 Release, CMake4.4.4 | 62/62 | 341.39s |
| GCC13 Debug, CMake4.4.4 | 62/62 | 360.87s |
| GCC13 Debug ASan+UBSan, CMake4.4.4 | 62/62 | 1238.64s |
| GCC13 Release, exact CMake3.16.0 | 62/62 | 260.42s |

These suites include standalone-header/source inclusion, installed/relocated
multi-TU and actual quickstarts, C++23 consumption, package-version rejection,
generic nonmathematical OOP, strict-FP/compiler warning checks, existing prime
modes and all new tests. No qualification gate failed at the frozen SHA and no
post-freeze numerical/code repair was needed. Installed checks removed their
disposable producer source/build trees before relocation consumption and retained
intentional paths containing spaces. Sanitizers were instrumented both when
compiling and linking their qualification targets; installed consumers correctly
remain free of private sanitizer/warning requirements.

[Named portable manifest](run-checkpoint-results/qualification/portable-tests.txt),
[per-selection TSan repetitions](run-checkpoint-results/qualification/tsan-repetitions.json),
[structured full-suite results](run-checkpoint-results/qualification/results.json),
[environment](run-checkpoint-results/qualification/environment.txt) and
[retained full transcripts](run-checkpoint-results/qualification/) provide the
executed evidence. Commands/logs and full JSON manifests also remain under
`out/logs/checkpoint/` and `out/runs/checkpoint/`. Development failures are retained
alongside successful evidence and described above. The persistence smoke used the
local ext4 filesystem; fsync/rename results do not establish power-failure behavior
for arbitrary filesystems/hardware or durable creation of new ancestor directories.
