# Ordinary-search depth-setting comparison

## Provenance and fixed protocol

- Freshly fetched base: `5425e68726337c038d88112f9b91d817d9f7eed6`. It contains the PR #76 merge, whose head `15cc90deb32a8c97cd60877d78ca3754ec5b88cb` is an ancestor.
- Comparison implementation: the public `symbolicRegression` entry point configured from `tests/rank_aware_fitting.cpp::ordinarySearch`, with ordinary LM (`rankAware=false`) in all arms. The fixture's rank-aware comparison arm was not run in this task.
- The protocol was written and SHA-256 recorded before any target search. See [`PROTOCOL.md`](PROTOCOL.md) and [`PROTOCOL.sha256`](PROTOCOL.sha256).
- Target: `2*x+3`; training inputs `{-2,-1,0,1,2}`; held-out inputs `{-0.5,2.5,4}`. Search receives only the training `RegressionProblem`; held-out rows are created only after search has returned.
- Fixed grammar: Add and Multiply; continuous literal probability 1; sampled literal range `[-2,2)`; proposal limit 16. Population 10, 1 elite, 4 generation attempts, max 50 candidate evaluations, 50,000 total training-loss attempts, prediction-loss selection, training threshold 0. Fitter: LM, single start, bounds `[-8,8]`, 512 loss-attempt cap, 64 iterations, 128 objective and Jacobian evaluation caps, 1,000,000 arithmetic operations, loss tolerance `1e-24`.
- Arms vary only `(initialMaxDepth, mutationMaxDepth)`: `(2,2)`, `(4,2)`, `(2,3)`, `(4,3)`. Each ran seeds 76001–76008 once, 32 searches total. Historical seed 20261127 is preserved in the separate diagnosis and was not replayed.
- Candidate tracing is compile-time gated and environment gated. It adds output only; it makes no RNG draws, objective measurements, target-dependent choices, or additional candidate evaluations. See `local_trace_instrumentation.patch` for the exact temporary delta, which was restored after the captured run.

## Capability and recovery rules

An evaluated pre-fit tree is *capable* when its existing literal occurrences can be assigned independently within `[-8,8]` to make the polynomial exactly `2*x+3`. The starting literal values do not constrain the fit assignment. A witness is recorded for every positive result. The offline checker expands each Add/Multiply tree as an exact polynomial in `x` and symbolic literal variables. It solves the coefficient equations; positive assignments are substituted and checked exactly. For negative results, it records the coefficient constraints and uses an exact rational Groebner basis check when the symbolic solver returns no solution. Any inconclusive solver case remains labeled unresolved in the candidate-level evidence.

Tree identity is the exact pre-fit expression representation, including literal values. Thus same-shape expressions with different sampled/fitted constants count as different literal-sensitive trees. The capability property is evaluated with every literal treated as adjustable, so the same topology can be capable across many literal-sensitive identities. Distinct counts are within each run before totals are summed across runs.

Numerical recovery means selected-expression held-out MSE `<= 1e-12`. Paired gains/losses compare each modified arm against `(2,2)` at the same seed. “First capable” means first by evaluated-candidate order, not merely a generated-but-unscored proposal. The origin is classified as initialization for generation 0 or offspring for later generations; it does not attempt to infer mutation versus crossover.

## Four-setting scorecard

| Initial / mutation depth | Runs with a capable evaluated tree (first origin) | Distinct capable trees, summed per run | Held-out recovery | Paired gains / losses vs `(2,2)` | Mean objective calls | Mean LM arithmetic ops | Mean LM linear-algebra ops | Mean traced search elapsed |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `(2,2)` | 2/8 (0 init, 2 offspring) | 7 | 2/8 | baseline | 204.25 | 1,214.38 | 2,173.38 | 3.25 ms |
| `(4,2)` | 6/8 (6 init, 0 offspring first) | 49 | 6/8 | 5 / 1 | 201.88 | 3,930.63 | 4,636.13 | 3.73 ms |
| `(2,3)` | 8/8 (0 init, 8 offspring) | 37 | 8/8 | 6 / 0 | 200.75 | 2,577.50 | 3,114.63 | 3.63 ms |
| `(4,3)` | 8/8 (6 init, 2 offspring first) | 67 | 7/8 | 6 / 1 | 227.00 | 5,949.38 | 5,808.88 | 3.99 ms |

All runs hit the 50-candidate limit. All 32 stopped at `generation_limit`. Mean calls/arithmetic differ despite that equal cap. The final offline pass classified all 1,007 within-run distinct pre-fit trees as capable or incapable; none remained unresolved. The `(4,3)` arm had 67 proved capable literal-sensitive trees in total.

The timing field is **monotonic search elapsed time**, not isolated process CPU time. It includes the trace writes and is consequently a coarse, instrumented timing diagnostic, not a CPU-time comparison. Structure and measured work counters are unaffected by trace output. A separate process-CPU counter was not collected, so CPU time is a remaining measurement limitation. The original raw runner labeled this steady-clock value `search_cpu_seconds`; the raw stdout is preserved, and all derived tables correctly rename it `search_elapsed_seconds`. `depth_comparison.executed.cpp` preserves the exact source used to build the executed binary; `depth_comparison.cpp` has only the corrected output-field label and was not rerun.

## Per-seed results

[`results.csv`](results/results.csv) and [`per_seed.csv`](results/per_seed.csv) give every selected equation, training and held-out MSE, candidate count, public objective calls, fitter arithmetic, and elapsed time. [`capability_assessment.csv`](results/capability_assessment.csv) gives first capable expression, witness, fit output/loss/stop, and first origin for each seed. [`candidate_classifications.csv`](results/candidate_classifications.csv) gives exact offline classification and immediate survivor membership for each distinct evaluated pre-fit expression. [`raw_summary_normalized.csv`](results/raw_summary_normalized.csv) retains the runner's key/value summaries as normalized columns.

The first capable candidate's measured fit and survivor outcome are taken from the captured trace. For example, at `(4,2)` seed 76001 the first capable structure appeared in initialization. Assigning its literals `(1.5,2,0,1)` gives `(x+1.5)*(2+0)*1=2*x+3`. The actual fitted constants stayed at their starting values, its training MSE was 66.738, LM stopped at `minimum_step`, and it did not enter the immediate survivor population. Yet the final selected equation recovered numerically. At `(2,3)` seed 76005 the first capable offspring occurred at generation 4. Assigning its literals `(3,0,0,2,1)` gives `3 + (0*0 + (2*x)*1)=2*x+3`. Its actual fit also stayed at the starting constants, MSE remained 9.62193, and LM stopped at `minimum_step`; it was retained in that generation's survivor population, though it was not the selected best. Another candidate produced the selected recovery. These are structure-presence and fitter/selection outcomes separately.

## Result interpretation

Deeper mutation alone won all eight held-out cases and had no paired losses in this small fixed-seed sample. It also produced a capable evaluated tree in every run, versus only 2/8 at the original depths. Deeper initialization alone improved recovery to 6/8 but incurred more LM arithmetic and had one paired loss. Combining both depth increases raised the capable-tree count further but recovered 7/8 and had one paired loss, with the highest objective-call and arithmetic work. This evidence favors deeper mutation for this fixture and budget; it does not establish a general setting recommendation. The one loss and small eight-seed sample leave the broad question open.

## Reproduction and verification

- `depth_comparison.executed.cpp` and `run_comparison.executed.sh` preserve the exact source/launcher used for the 32 searches. `depth_comparison.cpp` is the source with the timing field corrected for future use; no target was rerun after this label correction.
- `run_comparison.sh` is the launcher; its executed copy is preserved alongside it.
- `analyze_traces.py` is an offline checker; it performs no search, fitting, candidate scoring, or held-out model assessment. It requires SymPy 1.12, recorded in the analyzer output.
- `verify_results.py` independently checks all 32 seed/setting combinations, each exact setting, the 50-candidate cap, trace counts, selected held-out MSE and threshold flags, and every positive coefficient witness by exact polynomial substitution.
- `results/raw/` contains 32 runner outputs and 32 complete trace files. Historical diagnosis files are not copied or modified.
- `SHA256SUMS` covers the protocol, runner, scripts, normalized results, capability classifications, and all raw outputs/traces.
- The separate PR #77 publishes the prior diagnosis. This comparison is a new evidence-only task/branch.

The candidate runner was compiled with `g++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off -DGENETIK_ORDINARY_SEARCH_TRACE -Igenetik ... -pthread`. Release qualification rebuilt and passed `genetik_rank_aware_fitting_contracts` using `../.tools/bin/cmake` and `../.tools/bin/ctest`. Sanitizers and installed-consumer checks were not applicable because the committed change is evidence-only and production sources were restored unchanged. Hosted CI is tracked on the results PR.
