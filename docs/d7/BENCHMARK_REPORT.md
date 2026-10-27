# Genetik 0.5.0 D7 competitor benchmark report

## Scope and protocol

The primary frozen matrix contains 108 serial runs: three implementations,
six tasks, two training tracks, and seeds 0, 1, and 42. Each run used the
same generated CSVs, one CPU pinned to core 0, one requested worker, a 120 s
outer wall limit, and a 6 GiB process-tree memory ceiling. All runs used
implementation SHA `5a95ea47f983d4ba99704fb9ea955b14264d6741` and protocol
SHA256 `b0b07d2ae418ca71b024fbaa1374264348d92f2b8290606efda2969a432f1ffd`.
The row-level outcomes and independently assessed candidates are retained in
[`results/benchmark-seed-results.jsonl`](results/benchmark-seed-results.jsonl)
and [`results/benchmark-summary.json`](results/benchmark-summary.json).
Full logs, native PySR checkpoints/HOF CSVs, process-supervisor records and
all failures remain in `out/logs/d7/benchmark_runs/`.

The frameworks were pinned to PySR 2.7.2 with SymbolicRegression.jl 2.7.2
(Apache-2.0), and PyOperon 0.6.2 (MIT; Operon core commit
`fad10f087c592812b1a9ecfabce669f37bf459f7`). Exact source, Python/Julia
dependency locks, runtime hash, build patch and installation details are in
[`BENCHMARK_PROTOCOL.md`](BENCHMARK_PROTOCOL.md) and `environments/`.
PyOperon required a retained GCC/aarch64-only build adaptation; its upstream
checkout was left unchanged. Genetik was built from the D7 candidate.

All tasks used the same grammar families (`+`, `-`, `*`, `/`, `sin`, `exp`,
`log`), the task's fixed input variables, and a 25-node/depth-8 ceiling where
the public API supports both. Protected-arithmetic behavior and the
coefficient-fitting policy differ and are described in the protocol. The
comparison does not claim identical search spaces or equivalent native
iteration/evaluation counts. Search candidates are not target-seeded. There
were no fixed-structure fits in this matrix. The shared held-out assessor
computes RMSE, maximum absolute/relative error, invalid predictions and
expression nodes/depth. Clean recovery is the protocol's stringent
`RMSE <= 1e-6 * max(1, target RMS)` threshold; noisy recovery is
`RMSE <= 0.02 * target RMS`.

Data are noiseless for the `clean` track. `noise01` adds fixed, seeded Gaussian
training noise with standard deviation 1% of the noiseless target RMS; test
targets remain noiseless. The six targets are cubic polynomial, rational,
trigonometric, exponential, positive-domain fractional power, and
multivariate kinetic energy. SRBench was excluded before target runs because
the upstream repository is GPL-3.0 and redistribution terms for the selected
underlying task snapshots had not been verified.

## Run outcomes and held-out results

| Framework and result interpretation | Runs completing within budget | Expressions assessed | Clean recoveries | Noisy recoveries | Total recoveries |
|---|---:|---:|---:|---:|---:|
| Genetik adapter result, including emitted best-so-far on cancellation | 14/36 | 30/36 | 8/18 | 12/18 | 20/36 |
| PySR HOF snapshots from timed-out runs (secondary diagnostic only) | 0/36 | 36/36 snapshots | 13/18 | 18/18 | 31/36 snapshots |
| PyOperon final adapter result | 0/36 | 0/36 | 0/18 | 0/18 | 0/36 |

**PySR's 31/36 figure is not a completed-run result.** Every PySR process hit
the external cap before the adapter returned. The protocol therefore treats
the primary result as 36 timed-out/missing model returns. Its persisted native
HOF snapshots were separately extracted, hashed and scored after the run; the
31/36 threshold hits are reported only as best-so-far diagnostic evidence.
They do not change the process status or qualify a completed PySR run.
Likewise, PyOperon only prints a model after `fit()` returns and persisted no
recognized HOF, so there is no model to score for its 36 timed-out cells.

| Task | Genetik clean / noisy recovered | PySR HOF clean / noisy recovered | PyOperon completed model |
|---|---:|---:|---:|
| Cubic polynomial | 2/3, 2/3 | 3/3, 3/3 | 0/6 |
| Rational function | 0/3, 2/3 | 0/3, 3/3 | 0/6 |
| Trigonometric sine | 0/3, 3/3 | 3/3, 3/3 | 0/6 |
| Exponential decay | 3/3, 2/3 | 3/3, 3/3 | 0/6 |
| Fractional power | 0/3, 3/3 | 1/3, 3/3 | 0/6 |
| Kinetic energy | 3/3, 0/3 | 3/3, 3/3 | 0/6 |

These are observed finite-seed outcomes, not estimates of universal success
probability. Clean/noisy per-seed RMSE, normalized RMSE, relative and maximum
errors, complexity, invalid counts, timing, CPU/RSS, native counters, and
missing candidates are available in the row-level JSONL. For groups with
assessable models, the companion summary reports median and interquartile
range; missing-output runs are not imputed with scores. PySR's symbolic
equivalence test conservatively proved only its two exact kinetic-energy
forms on the declared input domain. The remaining formulas are not claimed to
be universally equivalent, even where their held-out error is small.

## Resources and fair-comparison limits

Median process wall time was 120.02 s for Genetik, 120.22 s for PySR, and
120.08 s for PyOperon; the small excess over 120 s is supervisor teardown and
recording. Median CPU time was 108.95 s, 107.74 s, and 100.05 s respectively.
Maximum observed RSS was 0.0046 GiB, 0.915 GiB, and 0.141 GiB. Runs were
serial; no benchmark jobs overlapped. PySR had up to three OS threads (Julia
runtime threads) while still constrained to one compute core and serial
search. CPU utilization, actual process/thread counts, and each run's memory
peak are retained per cell.

Neutral preflight measured PySR at 126.42 s cold and 126.60 s in a separate
cache-warm process, including 7.65 s Python import and 117.48 s fit-call time
in the accepted cold record. The fit-call duration includes Julia/JIT and
search; those components were not separable in the adapter. Its compilation
and startup overhead therefore consumed the fixed elapsed budget. PyOperon
preflight returned from its allowed fit call but retained MSE 0.406 on the
neutral linear smoke task; this smoke validated adapter execution, not
accuracy, and is not a successful mathematical recovery. Genetik's preflight
returned exact smoke fit in 3.30 s. All primary target jobs include startup,
initialization, search and constant fitting in the same elapsed-time cap.

The implementations expose different native counters: Genetik candidate
evaluations and charged training-loss attempts, PySR's persisted HOF equation
rows (not a total proposal count), and no returned PyOperon evaluation count
when its process is killed before `fit()` returns. These counters are not
combined or compared as equivalent work. The result supports no claim that
Genetik outperforms PySR, PyOperon, or another symbolic-regression framework.

## Search limitations and interpretation

Genetik's held-out performance on trigonometric clean cases (0/3), fractional
power clean cases (0/3), and rational clean cases (0/3) is weak under this
frozen grammar and budget. Its noisy-track recovery is higher in several of
those categories because the declared noisy threshold is different and
assesses prediction error against noiseless test targets; it should not be
read as evidence that noise improves the search. The six Genetik jobs with no
emitted expression and all competitor timeouts are recorded as such.

The available diagnostics establish that some tasks had accurate candidates
within the cap while other runs timed out or had weak fit; they do not isolate
causation among structural search, constant fitting, grammar, initialization,
invalid domains, objective design, complexity, or resource limits. In
particular, the comparison did not add target-informed initial candidates or
change the optimizer. Previous D5 (17/24, with fractional power 0/3 and
trigonometric 0/3) and D6 (4/12; tensor 3/3, Gaussian 1/3, oscillator 0/3,
complex 0/3) experimental outcomes remain historical and are not overwritten
by this different benchmark.

## Limits

This is one ARM host and a modest three-seed campaign. The 120 s cap plus
startup overhead prevented all PySR and PyOperon primary runs from returning;
the PySR HOF analysis is secondary. There is no SRBench subset, no
cross-machine performance claim, and no broad inference from these six
problems. Target recovery is empirical and separate from the independent
mathematical correctness qualification. No hosted benchmark or hosted CI was
run as part of this local matrix.
