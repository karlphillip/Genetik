# Fixed-seed ordinary search diagnosis (2026-11-27)

## Provenance and protocol

- Base: `5425e68726337c038d88112f9b91d817d9f7eed6` (merge commit PR #76).
- PR #76 head `15cc90deb32a8c97cd60877d78ca3754ec5b88cb` is the second parent of this base.
- Fixture: `tests/rank_aware_fitting.cpp::ordinarySearch`.
- Seed: 20261127. Training x = {-2,-1,0,1,2}; held-out x = {-0.5,2.5,4}; target = 2*x+3.
- Grammar: Add, Multiply; continuous constants [−2,2]; initial/mutation depth 2; proposal limit 16.
- Search: population 10, 1 elite, 4 generation attempts, 50 candidate evaluations, training-loss budget 50,000, threshold 0; fitting bounds [−8,8].
- Arms: LM with rank-aware selection disabled/enabled; identical configuration otherwise.
- Exactly one replay per arm was run. Tracing observed existing initialization, variation, fitting, and publication events. It made no RNG calls, candidate evaluations, objective measurements, or target-dependent decisions. The temporary compile-time instrumentation patch is included for audit; it was removed from the final source tree.

## Result

| Arm | Best expression | Training MSE | Held-out MSE | Candidates | Objective calls | Selector evaluations |
|---|---|---:|---:|---:|---:|---:|
| Ordinary LM | `(x0 + 2.9999999999292117)` | 2 | 7.5 | 50 | 294 | 0 |
| Rank-aware LM | `(x0 + 2.9999999999292117)` | 2 | 7.5 | 50 | 419 | 255 |

Each arm evaluated 50 candidates: 10 initialized candidates and 40 generation-publication evaluations (including reevaluated elites). There were 50 pre-fit candidate records: 10 initialization, 36 offspring, and 4 elite reevaluations. The 32 distinct pre-fit trees consist of 10 distinct initial trees, 22 distinct offspring forms (one overlaps initialization), and one additional tree first seen at elite reevaluation; 18 records repeat an already-seen pre-fit tree. The offspring depth census counts only the 22 distinct offspring forms. Across the 36 distinct offspring evaluation slots, 22 distinct offspring trees were examined; the remaining 14 were repeated structures.

Every candidate pre/post expression, generation, inferred origin (initialization, mutation, crossover, unchanged offspring, or elite reevaluation), scored training MSE, fit stop, objective calls, selector evaluations, publication membership, and best-seen status is in `candidate_trace.csv` (the source order is rank-aware first, ordinary LM second). `raw_trace.log` contains the full ordered trace and `replay_stdout.txt` preserves the public fixture output.

## Structural capability check (offline; no extra fits or target scores)

Each distinct pre-fit tree was checked as a symbolic expression with each existing literal treated as independently adjustable within [−8,8]. No evaluated pre-fit tree can equal `2*x+3`.

- Constant-only trees have zero x coefficient.
- `x0` has fixed x coefficient 1 and no intercept.
- `a*x0` can vary the x coefficient but has no intercept.
- `x0+c` has a free intercept but fixed x coefficient 1.
- The observed `x0*(a*x0)` is quadratic; its x² term can only vanish by setting `a=0`, which leaves zero, not the target.

Capable structures that were never evaluated include `(x0+x0)+c` (choose `c=3`), `(a*x0)+c` (choose `a=2,c=3`), and `a*(x0+b)` (choose `a=2,b=1.5`). The target constants are within the fitting bounds. In both arms, the best-scoring structural miss is `(x0+c)`: its fit reaches `c≈3` and MSE 2, but its x coefficient cannot be changed. `a*x0` is the complementary miss: its coefficient fits to about 2, but it cannot add an offset.

No capable candidate was evaluated, so there is no capable candidate fit or survival event to report. The first MSE-2 candidate in each arm was the generation-2 mutation `(x0 + 1.7222596099121756)`, fitted to `(x0 + 2.9999999999655489)` with `minimum_step`; it was published and became best-seen. A generation-3 mutation of another `(x0+c)` tree fitted to `(x0 + 2.9999999999292117)` at the same MSE and became the final best-seen expression. Neither tree can change the x coefficient.

## Offspring depth census

Depths are those of the distinct pre-fit offspring trees, not the post-fit rendered syntax.

| Arm | Distinct offspring trees | Depth 1 | Depth 2 | Depth 3 | First capable tree |
|---|---:|---:|---:|---:|---|
| Ordinary LM | 22 | 12 | 9 | 1 | None evaluated |
| Rank-aware LM | 22 | 12 | 7 | 3 | None evaluated |

## Diagnosis

Coefficient refinement worked on the constants exposed by each tree: it found the best additive literal for `x+c` and the best scale for `a*x`. The failure is structural coverage in the 50-candidate search: the arms did not construct a tree that exposes both slope and intercept simultaneously. The next change most directly supported by this trace is to improve the chance of generating or retaining the two-parameter affine structure `(a*x)+c` (or an equivalent such as `(x+x)+c`) under these depth and mutation limits. This is a diagnostic recommendation, not an implementation.
