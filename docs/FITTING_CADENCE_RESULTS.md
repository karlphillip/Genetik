# Refinement profile and interval-4 comparison

The procedure was fixed before measurement in [FITTING_CADENCE.md](FITTING_CADENCE.md).
Accepted prime/strict-arithmetic historical reports remain unchanged. No tuning,
interval sweep, fitter change, new solver, cache or algebraic rewrite was introduced.
All twenty contemporaneous comparison runs use detailed diagnostics **off**.

Environment: NVIDIA Jetson Orin NX Seeed recomputer classic Super, AArch64,
Linux 6.8.12-1021-tegra, GCC/libstdc++ 13.3.0, Release -O3/NDEBUG with the public
-fno-fast-math -ffp-contract=off policy, qualified glibc/libm, workspace CMake/CTest
4.4.4. Governor observed schedutil; not a fixed-frequency/isolated-host benchmark.
15 GiB RAM, about 9.4 GiB available, no swap, 833 GiB free storage. Pair ordering
alternates as predeclared. Full-suite builds started after the twenty measured
searches finished. Reference generation, input construction, baseline computation
(none in this comparison), post-search reports, RNG text export and file output
are outside search timers. Initialization and owned result/audit construction are
inside them. Runtimes are single observations per fixed seed/mode, not confidence
intervals or cross-platform performance claims.

## Profiles (control interval 1, seed 2)

Optional timers and bounded observation execute all ordinary computations. Exact
result, old/new counters, initial audits, preparation statistics and final RNG state
match the corresponding unprofiled control for both full workloads. No profiler
installation was needed. Timing rows below are **disjoint**; inclusive times remain
in the full profile logs and must not be added again.

| Workload | Search s | Objective s | Fitting overhead s | Publication s | Other preparation/variation/driver s | Calls | Actual rows | Published / attempts | Unpublished calls |
|---|---:|---:|---:|---:|---:|---:|---:|---|---:|
| prime | 8.009729 | 7.812587 | 0.183529 | 0.000984 | 0.012629 | 100000 | 99864635 | 11/12 | 5967 |
| arithmetic | 0.213478 | 0.111694 | 0.095064 | 0.000650 | 0.006070 | 60044 | 1495096 | 9/9 | 0 |

Prime objective evaluation accounts for about 97.5% of profiled wall time; coordinate
fitting overhead about 2.3%. For the 25-row arithmetic problem these are about
52.3% and 44.5%. The best next CPU optimization target depends on dataset size;
fitter overhead alone is not the prime workload's main runtime cost. Dataset time
includes strict checked evaluation, its allocations, error/loss arithmetic and
wrapper; these timings do not isolate node-level arithmetic instructions.

Disjoint preparation categories (changed is relative to the selected source parent):

| Workload | Origin | Requests | Fits | Ordinary calls | Fit baselines | Fit probes | Actual rows | Fits with progress |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| prime | initialization | 64 | 64 | 0 | 64 | 3004 | 3052016 | 30 |
| prime | changed_offspring | 612 | 612 | 0 | 612 | 81613 | 82113611 | 477 |
| prime | unchanged_offspring | 103 | 103 | 0 | 103 | 10042 | 10138007 | 21 |
| prime | elite | 24 | 24 | 0 | 24 | 4538 | 4561001 | 12 |
| prime | unspecified | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| arithmetic | initialization | 64 | 64 | 0 | 64 | 2498 | 62980 | 26 |
| arithmetic | changed_offspring | 492 | 492 | 0 | 492 | 48742 | 1226204 | 313 |
| arithmetic | unchanged_offspring | 66 | 66 | 0 | 66 | 5733 | 144783 | 11 |
| arithmetic | elite | 18 | 18 | 0 | 18 | 2431 | 61129 | 3 |
| arithmetic | unspecified | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

Outcomes and existing termination rules (one reason per completed fit):

prime:

```text
initial_loss_calls=3068 initial_rows=3052016 completed_improved=539 completed_unchanged=95 no_constants=121 invalid_starts=47 aborted_fits=1 disjoint_check=1
fit_stop=no_constants count=121
fit_stop=loss_threshold count=0
fit_stop=invalid_start count=47
fit_stop=minimum_step count=402
fit_stop=sweep_limit count=0
fit_stop=proposal_limit count=0
fit_stop=evaluation_limit count=232
```

arithmetic:

```text
initial_loss_calls=2562 initial_rows=62980 completed_improved=353 completed_unchanged=84 no_constants=107 invalid_starts=96 aborted_fits=0 disjoint_check=1
fit_stop=no_constants count=107
fit_stop=loss_threshold count=1
fit_stop=invalid_start count=96
fit_stop=minimum_step count=360
fit_stop=sweep_limit count=0
fit_stop=proposal_limit count=0
fit_stop=evaluation_limit count=76
```

Repeated-input observations are exact within a fixed immutable problem and fitter
configuration. The table stores both inputs and returned values; a previous output
can be encountered for its first fitting request and is not automatically an
identical-input repeat. These overlapping categories must not be summed.

| Workload | Stored/capacity | Untracked requests | Exact repeated inputs | Repeat loss calls | Completed no-change repeat calls | Repeated inputs accepting progress | Prior-output requests | Prior-output requests accepting progress / completed no-change |
|---|---|---:|---:|---:|---:|---:|---:|---|
| prime | 1024/1024 | 9 | 168 | 5731 | 4472 | 16 | 227 | 33 / 194 |
| arithmetic | 755/1024 | 0 | 157 | 6059 | 4141 | 19 | 192 | 16 / 176 |

The prime table hits its 1,024-value capacity (nine input requests untracked);
counts are lower bounds, not extrapolated estimates. Arithmetic tracks all inputs.
Exact repeated inputs consume at least 5.731% of prime calls and 10.091% of arithmetic
calls; completed repeated no-change fits consume 4.472% and 6.897% respectively.
This is measurable potential for a later exact-input/result reuse experiment,
but not the dominant prime cost. An inherited/previously fitted output is **not**
a convergence certificate: 33 prime and 16 arithmetic prior-output requests accept
further progress. A simple already-optimized flag would discard real improvements.
Any future exact cache must key full expression content and compatible constraints,
immutable problem and fitter configuration, preserve exact returned expression/
score pairing, and separately define budget accounting. No cache is implemented.

## All paired outcomes

The [complete generated report](cadence-results/runs/summary.md) includes every seed,
mode, initial/final score, faithful expression, complexity, discovery, generations,
work breakdown, unpublished work, runtime/stop/unused budget, raw prime metrics,
integer diagnostics and strict-arithmetic errors. [Machine-readable summary](cadence-results/runs/summary.csv)
and [raw log](cadence-results/runs/campaign.txt) retain full precision. All traces are
under cadence-results/runs/*-trace.csv; ten prime prediction tables each contain
1,200 rows, with no repaired or omitted invalid outputs.

1. **More structure evolution:** yes, every paired run publishes more generations.
   Prime median 10 -> 51 (ranges 8–12 -> 35–51); arithmetic median 9 -> 35
   (8–11 -> 24–43). This is not by itself a quality gain.
2. **Better equations:** mixed. Both problems have three exact fitting-MSE wins and
   two losses for interval 4. Prime median normalized MSE improves from 5.36020e-6
   to 2.41909e-6, while its fitting-selected best gets worse (1.46364e-6 ->
   2.12507e-6). The frozen representative remains seed 2 in each mode; raw fitting
   RMSE 12.0981 -> 14.5776, examined extrapolation RMSE 18.7845 -> 34.4293,
   rounded matches 48/1000 -> 32/1000 and 6/200 -> 3/200. Some other seeds improve
   later-range errors, but those results never replace the fitting-selected model.
3. **Recovery reliability:** arithmetic training/interior/exterior recovery falls
   from 3/5 (0,2,42) to 2/5 (2,42) under the separately declared 1e-6 maximum-error
   criterion. Seeds 2 and 42 find zero sample loss with interval 4 using fewer calls;
   seed 0 loses recovery and uses the full allowance. Prime remains approximation,
   not exact-prefix recovery. All finite negatives and integer diagnostics remain
   reported, rather than being repaired.
4. **Runtime:** no broad speedup. Paired experimental/control median ratios are
   1.03461 (prime, range .89042–1.12419) and 1.29718 (arithmetic, .50157–1.73352).
   Prime aggregate search time 40.5432 -> 41.6301 seconds with 500,000 calls each;
   arithmetic 1.35036 -> 1.59910 seconds with 380,513 -> 369,487 calls. Different
   trees, rows visited and stopping reasons mean equal ceilings are not equal work.
   Arithmetic's two faster recoveries do not compensate for three slower pairs.
   Prime's unpaired mode median runtime happens to decrease (8.53194 -> 8.26320 s);
   paired ratios and aggregate times expose why that is not a paired speedup claim.
5. **Repeated work:** present, with the bounded lower bounds above, but most prime
   work is fresh dataset evaluation. Repeated fits can legitimately improve bounded
   results. A later exact-reuse experiment is plausible, not a demonstrated large
   end-to-end gain or justification for skipping all inherited candidates.

The cadence can discard a promising new structure before its constants receive
refinement. The seed-0 arithmetic recovery loss is consistent with that risk and
population/trajectory divergence, but these runs do not isolate its causal mechanism.
No claim that this is the sole cause is justified. Keep interval 1 as default;
interval 4 is an optional measured tradeoff, not a generally superior policy.
No second configuration was tried after examining these outcomes.

Both arithmetic representatives are training-selected: interval 1 seed 42, interval
4 seed 2. All twenty selected expressions and four representatives were finalized
before the evaluation pass. Prime 1001–1200 remains explicitly previously examined;
new ranges are reserved for a later, separately declared assessment.

## Work outside the twenty comparisons

The twenty runs consume 1,750,000 actual loss attempts and 1,015,760,152 actual row
attempts. Separate accepted profiles consume 160,044 calls / 101,359,731 rows.
File reproduction adds 32,384 calls / 801,514 rows. Reporting makes 12,000 prime and
490 arithmetic prediction calls, separate from search, with no new search objective
calls. No baseline was recomputed. Short tests/default-reference checks are
qualification work, not included in the twenty-run statistics.

The aborted initial audit run is retained under cadence-results/audit-failure and
excluded from runtime/quality conclusions. It includes two profiling runs and the
first prime pair; no evaluation metrics were calculated. This extra work was a
reporting-lifetime defect repair, not an interval/seed/accuracy adjustment. See the
predeclaration's repair note and raw partial logs for its consumed counters.

## Reproduction

```sh
../.tools/bin/cmake -S . -B ../build-release -DCMAKE_BUILD_TYPE=Release
../.tools/bin/cmake --build ../build-release -j2
../build-release/genetik_cadence_campaign --output ../cadence-comparison
python3 examples/summarize_cadence.py ../cadence-comparison
../build-release/genetik_multivariate_demo --arithmetic --generate ../cadence-data
../build-release/genetik_regression_search ../cadence-data/training.csv --inputs first,second --target target --seed 2 --loss-budget 100000 --operators add,sub,mul,div,log --fit-every 4
```

All ten rerun controls reproduce the accepted expressions/scores/legacy counters
and stopping reasons exactly. Omitted/explicit interval 1 agree in API and CLI
checks. Complete pair audits verify every raw/prepared initial expression, fitness
state and initial work counter. [Compatibility evidence](cadence-results/runs/compatibility.txt)
and [file reproduction evidence](cadence-results/file-compatibility.txt) are retained.
No historical campaign report was edited.

The additional default prime-file compatibility run consumes 100,000 loss attempts /
99,804,196 rows, separately from comparison/profile totals; it reproduces accepted
seed 0 exactly and validates additive-counter handling in the existing reproduction
helper. See cadence-results/prime-workflow-compatibility.txt. Historical reports and
checksums are unchanged. The arithmetic interval-4 file run reproduces every printed
deterministic counter and faithful expression, not only its final score.

Focused tests cover initialization for both policies, boundary generations, failed
retry, changed-expression fitness, further inherited fitting progress, budget and
operational failures on both policies, all-invalid staging, independent work/skip
counters, deterministic RNG, exact identity/capacity, CLI input validation and a
public-only downstream cadence search. Existing arithmetic/fitter/failure tests are
reused. Tests require correctness, never stochastic recovery. Oversized initial
CLI smoke budgets caused sanitizer timeouts; final 64/128-call smoke budgets pass
under the original 60-second gate, while full correctness assertions remain in
C++ tests and Release reproduction. This does not alter campaign work ceilings.

Final verification: all 16 CTest cases pass in Release (2.07 s), Debug (2.26 s)
and ASan+UBSan (147.20 s), including standalone/downstream/strict-FP checks and
short cadence integration. Full warning review emits no diagnostics; whitespace
checks pass; character fitness/counters are unchanged. Per-origin work partitions
sum exactly to actual global work in all twenty runs, traces remain within budget
and nonworsening in committed loss, and all ten prime tables have every index 1–1200.
Raw final test evidence and SHA-256 checksums are under cadence-results. No unresolved
engineering blocker. The profile capacity, unreplicated timing observations and
mixed five-seed evidence are the concrete limitations described above.
