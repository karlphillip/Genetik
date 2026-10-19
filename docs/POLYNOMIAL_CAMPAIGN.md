# Initial polynomial structure-search campaign

## Predeclared settings

Frozen before the first campaign run. Serial Jetson CPU, GCC/libstdc++ 13, C++20, Release with strict FP flags. Seeds **0,1,2,3,42**, separate caller-owned RandomContext per seed. No target injection, restart, repair or constant fitting.

- Population 128; at most 250 generation attempts and 32,128 attempted training candidate evaluations including initialization. Two elites (reevaluated), existing linear-rank roulette.
- Training ordering: exact MSE ascending; equal finite MSE prefers fewer nodes, then lexicographically smaller faithful expression text. Exact equality through all keys preserves prior committed best; cross-seed representative uses that same ordering followed by lowest seed. Recovery tolerance never enters ordering.
- Hard limits: 31 nodes, depth 6 (leaf=1), absolute stored constant bound 2. One variable; operators +,-,*; uniform constant terminals {-2,-1,0,1,2} as doubles.
- Initialization: independently draw a depth cap uniformly from 1..4; full/grow choice with probability .5 each. Full branches wherever remaining node/depth budget permits; grow branches with probability .65. Split remaining child budgets uniformly; unused budget is not forced into a template. At terminals choose variable with probability .5, otherwise a uniform constant. No polynomial-specific structure is supplied.
- Crossover rate .9, mutation rate .35 per offspring. Uniform postfix-root selection (including roots and leaves); subtree replacement mutation draws a new independently generated subtree with depth cap uniformly from 1..3. Subtree exchange crossover produces a pair. Both enforce whole-tree destination limits.
- At most 8 proposals per enabled mutation/crossover call. If no valid proposal, return unchanged parent(s). Valid but identical proposals are accepted and counted as zero actual changes. No exception is treated as a structural rejection.
- Stop at exact zero training MSE, otherwise boundary budget/attempt limit, NoViableCandidates, or operational exception. Discovery generation zero is possible through ordinary random initialization and must be reported.

Data: 17 training rows x=-2+k/4, k=0..16, y=x*x+x+1. Evaluation-only interior: 16 rows x=-15/8+k/4, k=0..15. Evaluation-only exterior: {-4,-3,3,4}. All search/selection/stopping decisions use training rows only.

All training searches finish first. Calculate each selected candidate's training maximum absolute error. If any seed meets 1e-9 training max-error recovery, predeclare **no configuration adjustment** and freeze the representative using training ordering before evaluation-only metrics. If none recover, do not evaluate interior/exterior yet; inspect training diagnostics for demonstrated defects and permit at most one documented configuration adjustment followed by the same five seeds within the same per-run bounds. Preserve both campaign results.

Each publication reevaluates every population slot, including elites/unchanged copies: initialization costs 128 calls; each attempted generation costs at most 128, including invalid/throwing calls, so 128*(1+250)=32,128. Search refuses to start a generation unless all 128 calls fit the remaining budget. Post-run metrics are outside this count: one training, one interior and one exterior metric call (37 expression/sample evaluations in total per fully reported seed). Consumed RNG draws, operator statistics and attempted training work are not refunded on an aborted generation.

## Results

Executed once in Release after deterministic integration qualification. All five seeds recovered training data; therefore no search-configuration adjustment or second campaign was performed. The program finalized that decision and selected its representative using training ordering before any interior/exterior evaluation. The complete unedited output, including every exported expression, counter and runtime, is in [polynomial-campaign-initial.txt](polynomial-campaign-initial.txt).

| Seed | Initial MSE | Final MSE | Training max error | Interior max error | Exterior max error | Nodes | Depth | Discovery generation | Attempts/published | Search evaluations | Search seconds |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 1 | 0 | 0 | 0 | 0 | 7 | 3 | 3 | 3/3 | 512 | 0.002668 |
| 1 | 1 | 0 | 0 | 0 | 0 | 17 | 5 | 3 | 3/3 | 512 | 0.002488 |
| 2 | 0 | 0 | 0 | 0 | 0 | 11 | 4 | 0 | 0/0 | 128 | 0.000424 |
| 3 | 1 | 0 | 0 | 0 | 0 | 7 | 4 | 4 | 4/4 | 640 | 0.003127 |
| 42 | 0 | 0 | 0 | 0 | 0 | 7 | 3 | 0 | 0/0 | 128 | 0.000403 |

All stopped with exact_training_fit. All three numerical-recovery flags (max error <=1e-9) are true for each seed. Seeds 2 and 42 discovered their selected expression naturally in random initialization; they are not successes attributable to later variation. Seeds 0,1,3 improved from initial MSE 1 through the shared engine. The campaign does not establish algebraic equivalence or generalization beyond the declared sets.

Actual exports:

```text
0:  ((x0 * x0) - ((-1.0) - x0))
1:  (((0.0 - (-1.0)) + (x0 + x0)) - ((x0 - (-2.0)) + ((-2.0) - (x0 * x0))))
2:  ((x0 * 1.0) + ((2.0 + (-1.0)) + (x0 * x0)))
3:  (x0 + (1.0 + (x0 * x0)))
42: ((x0 * x0) + (x0 + 1.0))
```

Representative: seed **42**, selected by training MSE, node count, text order, then seed. No evaluation-only metric participates in that choice. Runtime excludes post-run metrics and is an observation on this Jetson, not a performance qualification.

| Seed | Initialization proposals | Mutation proposals | Crossover proposals | Structural rejects | Mutation/crossover fallbacks | Mutation changes | Crossover changes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 128 | 134 | 166 | 0 | 0/0 | 121 | 234 |
| 1 | 128 | 136 | 169 | 0 | 0/0 | 126 | 254 |
| 2 | 128 | 0 | 0 | 0 | 0/0 | 0 | 0 |
| 3 | 128 | 198 | 227 | 2 | 0/0 | 173 | 340 |
| 42 | 128 | 0 | 0 | 0 | 0/0 | 0 | 0 |

Crossover proposals count pairs; changes count changed child expressions, so changes may exceed proposals. Changes include changed constants as well as structure. Initialization counters are attempted builds; rejected/abandoned work is not refunded. The tests separately verify node-count changes for both operators and exercise actual bounded rejection/fallback paths. No target-specific initialization or prime/sequence knowledge exists in the search library.

Aggregate search candidate evaluations: **1,920**, each seed below its 32,128 cap. Post-run reporting additionally made **15 candidate/dataset metric calls and 185 expression/sample evaluations**, outside the search counters (3 calls and 37 samples per seed). No continuous constant fitting occurred.

Reproduce the campaign from the repository root after a Release build:

```sh
timeout 180s ../build-release/genetik_polynomial_regression
```

The declared discrete grammar recovered this polynomial within the supplied bounds. The recommended next application task is bounded continuous constant fitting, with training-only optimization, worsening/invalid rollback and explicit accounting. This five-seed result does not qualify more difficult grammars/datasets; keep structure-search diagnostics and unsuccessful-run reporting for subsequent experiments.
