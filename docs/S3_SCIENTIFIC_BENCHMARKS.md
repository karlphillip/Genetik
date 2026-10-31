# S3 Scientific Benchmarks (protocol and status)

## Category separation

Results are reported in three independent categories: (A) mathematical correctness of evaluators and exact constraints; (B) fixed-structure parameter fitting; and (C) autonomous evolutionary structure discovery. A result in one category is not evidence for another.

## Frozen source and comparison protocol

The existing S1, S2, B1/B2 and R9 records remain immutable historical evidence. S1's LM evolutionary results are preserved, including unfavorable seeds. B1/B2 Wythoff results are fixed-topology checks and must not be represented as autonomous discovery. No prime-successor campaign is part of S3.

The frozen S3 external protocol is `docs/s3-results/benchmark-protocol.md` (SHA256 recorded in the evidence manifest). It pins six smooth tasks, clean and 1%-RMS noisy tracks, seeds 0/1/42, 160 training rows, 512 held-out rows, the D7 data manifest, and the D7 grammars. All 108 framework/task/track/seed cells ran sequentially on the same Jetson AGX Orin host, pinned to CPU 0, with one worker, a 120-second wall cap and 6-GiB process-tree RSS cap. Initialization and JIT/compilation are included in the cap. Genetik was built from GCC 13.3.0 with C++20, `-O3 -DNDEBUG -fno-fast-math -ffp-contract=off`; PySR/SymbolicRegression.jl 2.7.2 and PyOperon 0.6.2 used the retained pinned environments. No dependency was updated after the protocol freeze.

Training and held-out datasets are independent. Held-out observations were not used for search, model fitting, or the training Pareto archive. Coupled-system fitting and exact quantized regression remain standalone mathematical/fixed-topology qualification workloads because the competitors do not expose equivalent semantics in this protocol.

## Existing outcomes

The frozen S1 fixed-structure rows and mixed two-seed evolution results are in `s1-results/`; S2 exact qualification and its fixed-topology Wythoff result are in `s2-results/`. M3/B1/B1.1/B2 records are separately preserved and have their own protocols. They are not a newly frozen S3 external comparison.

The nearest qualified external comparison is the frozen D7 six-task, 108-run matrix, using PySR/SymbolicRegression.jl 2.7.2 and PyOperon 0.6.2 on the same Jetson host with pinned data, 3 seeds, serial CPU allocation, and 120-second caps. Its code/protocol identity and all rows are recorded in `docs/d7/BENCHMARK_REPORT.md` and `docs/d7/results/`. The report records 20/36 Genetik assessable-expression threshold hits, 31/36 PySR persisted-HOF diagnostic hits (all 36 processes timed out), and no returned PyOperon candidate (all 36 timed out). The PySR HOF scores are explicitly secondary, and these results support no general outperformance claim. This is qualified evidence for the prior scalar Genetik baseline, not a measurement of the S1/S2 candidate SHA; it cannot be relabeled as an S3 run. The frozen tasks include polynomial, rational, trigonometric, exponential, fractional-power and multivariate kinetic-energy equations; the coupled D6 oscillator and exact quantized task are covered separately by fixed-topology protocols.

## Frozen S3 autonomous-discovery outcomes

The target implementation source is `c52f026eb87c886a20f0d2d669f660d500504c5c`; the protocol was committed at `ee9d82f61bee26380b443698cd80a0e7f785d376` before target runs. The full 108-row run record and 68 independent equation assessments are in `docs/s3-results/external-comparison/`. The dataset and harness identities are those frozen in the protocol. Every outcome is included below; PySR snapshots from timed-out runs remain a secondary diagnostic, not a completed-run success.

| Framework / interpretation | Completed processes | Equations independently assessed | Held-out threshold recoveries | Notes |
|---|---:|---:|---:|---|
| Genetik default coordinate fitter | 17/36 | 32/36 | 20/36 | Best-so-far expressions were assessed even when cancellation ended the run; 4 cells emitted no expression. |
| PySR 2.7.2 Hall-of-Fame snapshots | 0/36 | 36/36 | 30/36 | All processes timed out; results are diagnostic snapshots and do not count as completed PySR runs. |
| PyOperon 0.6.2 returned models | 0/36 | 0/36 | 0/36 | All processes timed out before `fit()` returned and no model was emitted. |

Recovery counts by task (clean/noisy, each out of three seeds) were:

| Task | Genetik | PySR HOF diagnostic | PyOperon returned model |
|---|---:|---:|---:|
| Cubic polynomial | 2/3, 1/3 | 3/3, 3/3 | 0/6 |
| Rational function | 0/3, 2/3 | 0/3, 3/3 | 0/6 |
| Trigonometric sine | 0/3, 2/3 | 3/3, 3/3 | 0/6 |
| Exponential decay | 2/3, 2/3 | 3/3, 2/3 | 0/6 |
| Fractional power | 0/3, 3/3 | 1/3, 3/3 | 0/6 |
| Kinetic energy | 3/3, 3/3 | 3/3, 3/3 | 0/6 |

Across assessable models, symbolic equivalence was conservatively proved for 0/32 Genetik formulas and 3/36 PySR snapshots (the exact kinetic-energy formulas). Equivalence is not inferred from prediction agreement. The held-out recovery threshold follows the frozen D7 scorer: clean normalized RMSE at most `1e-6`; noisy RMSE at most 2% of noiseless target RMS. PySR’s 30/36 score uses saved Hall-of-Fame equations selected deterministically by minimum recorded training loss, then complexity, then expression text. PyOperon emits no intermediate best-so-far structure, so its 36 timeouts remain missing model outputs.

All processes used the common cap; median wall times were 120.04 s Genetik, 120.22 s PySR, and 120.08 s PyOperon, including teardown. Median CPU times were 108.65 s, 108.94 s, and 100.32 s. Maximum recorded process-tree RSS was 0.0046, 0.977, and 0.141 GiB, respectively. The largest PySR/Operon wall values include supervisor cleanup after the 120-second cap. Genetik reports candidate-evaluation and charged-loss counters; PySR exposes saved HOF rows, and killed PyOperon runs return no equivalent count. These work measures are not comparable. Framework fit completion and time-to-target are unavailable for timed-out processes; no target timing was inferred. The finite seed collection is small and supports no claim of general superiority.

All three neutral adapter preflights completed before the target run. The preflight is an execution smoke test, not an accuracy comparison. PySR reported 47.4 s import/setup and a 109.9 s fit-call on that neutral input; PyOperon reported 27.3 s import and a 60.0 s fit-call; Genetik returned its smoke fit in about 3.0 s. Target runs start fresh processes, so framework startup, initialization and any JIT time are inside the same supervised 120-second allowance. The preflight times are reported separately and are not added to target times.

Machine-readable run rows, independent assessments, missing-model reasons and summary are `docs/s3-results/external-comparison/{run-records,assessments,missing-candidates,benchmark-seed-results,benchmark-summary}.jsonl/json`. The candidate extraction and scorer are retained with the evidence. Historical v0.5 D7, including its 20/36 Genetik and 31/36 PySR snapshot results, remains separately identified in `docs/d7/BENCHMARK_REPORT.md` and was not substituted for these S3 runs.

Exact quantized constraints and the D6 coupled oscillator are qualified separately under fixed structures. They are not included in the autonomous framework ranking. The output-only equation archive is built from training objectives only; held-out observations are reserved for evaluation.
