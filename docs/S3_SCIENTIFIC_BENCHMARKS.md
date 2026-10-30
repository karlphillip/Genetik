# S3 Scientific Benchmarks (protocol and status)

## Category separation

Results are reported in three independent categories: (A) mathematical correctness of evaluators and exact constraints; (B) fixed-structure parameter fitting; and (C) autonomous evolutionary structure discovery. A result in one category is not evidence for another.

## Frozen source and comparison protocol

The existing S1, S2, B1/B2 and R9 records remain immutable historical evidence. S1's LM evolutionary results are preserved, including unfavorable seeds. B1/B2 Wythoff results are fixed-topology checks and must not be represented as autonomous discovery. No prime-successor campaign is part of S3.

Before any new comparative run, record final source SHA, compiler and runtime identities, CPU allocation, competitor lock/version, dataset hashes, seeds, grammar, operation/resource limits, wall/CPU procedure, target threshold and timeout. Runs are sequential on the same physical machine. Initialization and compilation costs are included or explicitly reported separately. Use independent training and held-out data; held-out results never enter search or the training Pareto archive.

The representative matrix covers polynomial, rational, fractional-power, trigonometric, exponential, coupled-system and exact quantized regression. PySR/SymbolicRegression.jl and Operon/PyOperon comparisons are limited to overlapping smooth autonomous-search tasks. Exact constraint qualification is standalone unless a competitor supplies equivalent exact interval semantics. Report every seed, intermediate results when available, evaluation work, runtime, failures and time to target; no broad superiority claim follows from this small set.

## Existing outcomes

The frozen S1 fixed-structure rows and mixed two-seed evolution results are in `s1-results/`; S2 exact qualification and its fixed-topology Wythoff result are in `s2-results/`. M3/B1/B1.1/B2 records are separately preserved and have their own protocols. They are not a newly frozen S3 external comparison.

The nearest qualified external comparison is the frozen D7 six-task, 108-run matrix, using PySR/SymbolicRegression.jl 2.7.2 and PyOperon 0.6.2 on the same Jetson host with pinned data, 3 seeds, serial CPU allocation, and 120-second caps. Its code/protocol identity and all rows are recorded in `docs/d7/BENCHMARK_REPORT.md` and `docs/d7/results/`. The report records 20/36 Genetik assessable-expression threshold hits, 31/36 PySR persisted-HOF diagnostic hits (all 36 processes timed out), and no returned PyOperon candidate (all 36 timed out). The PySR HOF scores are explicitly secondary, and these results support no general outperformance claim. This is qualified evidence for the prior scalar Genetik baseline, not a measurement of the S1/S2 candidate SHA; it cannot be relabeled as an S3 run. The frozen tasks include polynomial, rational, trigonometric, exponential, fractional-power and multivariate kinetic-energy equations; the coupled D6 oscillator and exact quantized task are covered separately by fixed-topology protocols.

## S3 run status

No S3-specific PySR or Operon execution has been recorded. The pinned D7 PySR/SymbolicRegression.jl 2.7.2 and PyOperon 0.6.2 environments are retained locally and must be requalified against the curated 0.7 candidate before making candidate-specific comparisons. The D7 baseline outcomes above are historical, not S3 measurements. No dependency update or target run is inferred from preflight evidence. Until the candidate-specific external suite finishes, the comparative claim is explicitly inconclusive. See `S3_RELEASE_PREPARATION.md` for engineering qualification status.
