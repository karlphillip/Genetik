# S3 scientific benchmark protocol (frozen before target runs)

Protocol version: 1.0.0  
Candidate source commit: `c52f026eb87c886a20f0d2d669f660d500504c5c`  
Inherited D7 protocol SHA256: `b0b07d2ae418ca71b024fbaa1374264348d92f2b8290606efda2969a432f1ffd`  
Dataset manifest SHA256: `b0770197be2162108720b45d43eaa1ace727d48879e601cca779e71be2822790`  
Candidate preflight status: Genetik, PySR, and PyOperon all completed before target execution.

## Scope

The comparative matrix is autonomous smooth symbolic regression only. It uses the six frozen D7 tasks (`polynomial_x3`, `rational_x2`, `trig_sine`, `exponential_decay`, `fractional_power_positive`, `kinetic_energy_mv2`), both clean and 1%-RMS noisy training tracks, and seeds `0`, `1`, and `42`. Each task has an independent 160-row training set and 512-row held-out set; held-out values are never provided to search, fitting, or Pareto archive construction. Exact quantized regression and the D6 coupled oscillator remain standalone fixed-structure/mathematical qualification workloads and are not competitor rankings.

Frameworks and pinned versions are Genetik source commit above, PySR 2.7.2 with SymbolicRegression.jl 2.7.2, and PyOperon 0.6.2 with its recorded pinned Operon core. Reuse the existing retained venvs, Julia runtime/manifest, PyOperon build and D7 adapters without dependency upgrades. The accepted neutral preflight is retained in `out/logs/d7/preflight-results.jsonl`; its preflight source commit is metadata only and does not substitute for this target source commit.

## Frozen search and resources

Use the D7 adapters and their committed grammars, constant ranges, population/generation/search settings, and deterministic seeds unchanged. Each run receives one CPU pinned to CPU 0, a 120-second supervisor wall cap, a 6 GiB process-tree RSS cap, and one worker/thread; runs execute serially on the same Jetson AGX Orin host. Initialization and compilation/JIT time are included in the supervised wall/CPU totals. Record framework-reported fit/search time separately. Preserve timeout, invalid, missing-output, and failed runs. Do not retry or selectively omit unfavorable seeds.

Genetik uses the default coordinate fitter. The optional LM fitter is not enabled and remains a fixed-structure S1 comparison only. The Genetik candidate adapter is compiled from this candidate with GCC 13.3.0, C++20, `-O3 -DNDEBUG -fno-fast-math -ffp-contract=off`, and one thread.

## Measurements and scoring

Retain every raw adapter output, stderr, supervisor resource record, and candidate record. The independent D7 scorer will assess training RMSE, held-out RMSE, maximum absolute/relative error where defined, invalid predictions, expression node count/depth, native complexity, and mathematical-domain certificate where supported. Record candidate/evaluation/fit counters and constant optimizer work only when the framework defines them; these counters are not assumed equivalent. The common 120-second wall limit is reported as the cutoff, not as a ranking. Record time-to-target or intermediate best-so-far only where the adapter actually emits it; unavailable measures remain unavailable.

A recovery hit uses the frozen D7 held-out error threshold. Report all 108 runs per framework and do not claim general superiority from this small matrix. The D7 report’s historical v0.5.0 outcomes remain separately identified and are not relabeled as S3 results.

## Outcome separation

(A) Mathematical evaluator/constraint correctness is established by independent analytic and exact-rational fixtures.  
(B) Fixed-topology parameter fitting is reported from S1 and the exact Wythoff/D6 fixtures.  
(C) This matrix measures autonomous structure discovery only.  

No category is combined into a single success score. The training Pareto archive is computed only from training candidate records and has no connection to the live evolutionary lifecycle.
