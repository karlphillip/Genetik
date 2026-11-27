# Depth-setting comparison protocol (frozen before target runs)

## Provenance and task

- Freshly fetched `origin/main` base: `5425e68726337c038d88112f9b91d817d9f7eed6`.
- This base contains the PR #76 merge; PR #76 head `15cc90deb32a8c97cd60877d78ca3754ec5b88cb` is an ancestor.
- The historical diagnosis at seed `20261127` is preserved separately and is not included among the 32 runs.

## Fixed search protocol

Use `tests/rank_aware_fitting.cpp::ordinarySearch` as the source of configuration and data. All four arms use ordinary single-start LM with rank-aware coefficient selection disabled (`lm(false, 512)`). Data split is training `x={-2,-1,0,1,2}` and held-out `x={-0.5,2.5,4}`, with target `2*x+3`; held-out rows are constructed only after each search returns. Grammar operators are Add and Multiply; continuous constants are sampled with probability 1 from `[-2,2)`; proposal limit 16. Population 10, one elite, four generation attempts, a maximum of 50 candidate evaluations, and 50,000 training-loss attempts. Training-loss threshold is 0. Fitted constants are bounded by `[-8,8]`. Prediction-loss minimization is the default selection policy. Only initial and mutation depth settings vary:

| Arm | initialMaxDepth | mutationMaxDepth |
|---|---:|---:|
| original | 2 | 2 |
| deeper initialization | 4 | 2 |
| deeper mutation | 2 | 3 |
| library defaults | 4 | 3 |

Each arm runs seeds `76001` through `76008`, exactly once each: 32 searches. No seed or setting is retried or selected based on an outcome. Candidate-tree observation is passive local trace output; it consumes no RNG draw, search objective call, candidate evaluation, or held-out value. The historical seed is not replayed.

## Frozen scoring rules

- A pre-fit tree is capable if there exists an assignment to its existing literal occurrences, each in the fitter's closed interval `[-8,8]`, that makes its polynomial exactly `2*x+3`. Literals are independent parameters by occurrence. Classification is offline after each search; no extra fitting or target scoring is allowed for classification. A positive must include a witness assignment. A negative must include an algebraic reason. If the solver cannot establish either, mark unresolved.
- “First capable” is the earliest evaluated candidate by search evaluation order. Report whether it came from initialization or a later offspring; also retain inferred mutation/crossover origin if trace events identify it.
- The selected returned expression is scored after search on the fixed held-out rows. Numerical recovery means held-out MSE `<= 1e-12`.
- Report candidate evaluations, public training objective calls (`trainingWork.lossAttempts`), fitter arithmetic (`optimizerWork.mathematicalOperations` and `linearAlgebraOperations`), expression-generation/preparation/selection work where exposed, and search CPU time. Equal candidate caps do not imply equal costs. All searches use identical non-depth fitting and global-work caps.
- Paired gains/losses compare numerical-recovery outcomes for each seed against the same-seed `(2,2)` arm. Ties and changes in opposite directions are reported directly; no aggregate causal claim is made from unpaired results.

## Immutable analysis constraints

Do not change production defaults, operators, search, fitting, or D7. Do not use held-out rows during search or structural classification. Preserve all rows and raw traces, including missing or non-capable cases.
