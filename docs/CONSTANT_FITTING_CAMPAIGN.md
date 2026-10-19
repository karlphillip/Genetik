# Continuous-constant paired campaign

Frozen before the first run: deterministic serial CPU, GCC 13/libstdc++13 Release,
seeds 0,1,2,3,42, 64 slots, 2 elites, linear-rank selection, crossover .9, mutation .35,
31 nodes, depth 6 (leaf depth 1), initial depth cap 4, mutation cap 3, half full/grow,
grow binary probability .65, 8 structural proposals. Grammar +,-,* and x0;
constant terminals are half uniform discrete {-2,-1,0,1,2}, half uniform continuous
[-8,8), absolute constant bound 8. No target structure is supplied.

Both modes: at most 100,000 actual training-loss attempts and 2,000 generation
attempts; also at most 100,000 outer candidate requests. Training stop MSE <=1e-14;
reported recovery max absolute error <=1e-6. Fitting mode: bounds [-8,8], initial
step 1, minimum step 1e-8, 256 loss calls including fresh baseline, 128 sweeps,
8,192 proposals, training-loss threshold 1e-14. Apply to every private slot,
including initialization, elites and unchanged clones. Disabled mode uses the same
continuous source, not the older discrete-only configuration.

Training: x=-2+k/4, k=0..16; target 2.5*x*x-1.75*x+.625.
Interior evaluation only: x=-15/8+k/4, k=0..15; exterior: -4,-3,3,4.
Compare exact raw initial text vectors and raw baseline minima for each pair.
Later trajectories need not match. Select per-run committed best by training MSE,
nodes, then faithful text. Across runs, use that ordering then lowest seed (disabled
mode first on identical keys). No evaluation-only predictions before the training
adjustment decision. At most one training-diagnostic-driven configuration adjustment
is permitted if no enabled run recovers training; retain earlier results.

Run `timeout 600s ../build-release/genetik_constant_campaign` in Release.
The program prints all expressions, measured work, committed improvement traces
against cumulative loss calls, and explicit adjustment decision before held-out
predictions. Training max-error reporting evaluates predictions only: it does not
recompute MSE outside the loss allowance. Reporting attempts 37 rows per final
candidate (17 training, 16 interior, 4 exterior), separately from search work.

## Results

All ten searches completed normally. No configuration adjustment was required:
the training-only decision found five enabled recoveries before any held-out
predictions. Both modes had identical exact raw initial expression vectors and
baseline minima for every seed. All enabled runs recovered training, interior and
exterior at max error <=1e-6; no disabled run recovered any of those ranges.
This is numerical sample agreement, not a claim of algebraic equivalence or general
optimizer superiority. Shared allowance is not shared actual work.

The final qualification rerun used the same frozen settings. Expressions, losses,
work counts and variation counts match the initial run; only timing and the more
precise positive-threshold stopping label changed. Complete 17-digit outputs,
all ten faithful expressions, counters and compact committed improvement traces
are retained in [constant-fitting-campaign.txt](constant-fitting-campaign.txt).

| Seed | Fit | Raw initial MSE | Refined initial MSE | Final MSE | Train max error | Interior max error | Exterior max error | Nodes/depth | Discovery generation |
|---:|:---:|---:|---:|---:|---:|---:|---:|:---:|---:|
| 0 | off | 4.827599 | 4.827599 | 0.006488429 | 0.1207572 | 0.120114 | 0.5448738 | 29/6 | 1424 |
| 0 | on | 4.827599 | 0.1726017 | 5.341902e-15 | 1.227914e-07 | 1.153408e-07 | 2.420007e-07 | 15/5 | 2 |
| 1 | off | 9.249632 | 9.249632 | 0.00664989 | 0.1595432 | 0.1293802 | 0.9006095 | 27/6 | 1532 |
| 1 | on | 9.249632 | 0.1726017 | 2.181629e-16 | 2.419565e-08 | 2.350352e-08 | 3.526979e-08 | 23/6 | 4 |
| 2 | off | 15.37852 | 15.37852 | 0.02711743 | 0.3170655 | 0.2562734 | 1.795766 | 15/5 | 1467 |
| 2 | on | 15.37852 | 0.05020203 | 5.662267e-15 | 1.130679e-07 | 1.127565e-07 | 5.576099e-07 | 23/6 | 8 |
| 3 | off | 15.24814 | 15.24814 | 0.1013691 | 0.6186023 | 0.5017081 | 3.501748 | 17/6 | 1451 |
| 3 | on | 15.24814 | 0.1726017 | 4.014318e-15 | 1.313501e-07 | 1.213203e-07 | 3.248381e-07 | 13/4 | 5 |
| 42 | off | 13.50412 | 13.50412 | 0.004280636 | 0.1632082 | 0.1418394 | 0.6507087 | 31/6 | 1018 |
| 42 | on | 13.50412 | 2.443532e-06 | 9.271587e-15 | 1.306512e-07 | 1.30846e-07 | 5.744872e-07 | 9/4 | 3 |

All reported candidates were discovered in offspring, not initialization. Disabled
runs each attempted/published 1,561 generations and stopped at a boundary with 32
unused calls. Enabled runs each published every attempted generation and stopped
at the positive training threshold. No run needed a retry or regeneration.

| Seed | Fit | Attempted/published generations | Outer requests | Actual loss calls | Fits | Proposals | Accepted | Infeasible | Duplicate | Seconds | Unused allowance |
|---:|:---:|:---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | off | 1561/1561 | 99968 | 99968 | 0 | 0 | 0 | 0 | 0 | 1.2374 | 32 |
| 0 | on | 2/2 | 192 | 21078 | 192 | 25872 | 2554 | 72 | 4887 | 0.0579 | 78922 |
| 1 | off | 1561/1561 | 99968 | 99968 | 0 | 0 | 0 | 0 | 0 | 1.2985 | 32 |
| 1 | on | 4/4 | 320 | 42752 | 320 | 53040 | 3962 | 137 | 10410 | 0.1285 | 57248 |
| 2 | off | 1561/1561 | 99968 | 99968 | 0 | 0 | 0 | 0 | 0 | 1.0317 | 32 |
| 2 | on | 8/8 | 576 | 89066 | 576 | 110784 | 12484 | 788 | 21323 | 0.2879 | 10934 |
| 3 | off | 1561/1561 | 99968 | 99968 | 0 | 0 | 0 | 0 | 0 | 1.0911 | 32 |
| 3 | on | 5/5 | 384 | 49305 | 384 | 60472 | 5540 | 229 | 11256 | 0.1380 | 50695 |
| 42 | off | 1561/1561 | 99968 | 99968 | 0 | 0 | 0 | 0 | 0 | 1.4221 | 32 |
| 42 | on | 3/3 | 256 | 29969 | 256 | 37681 | 3276 | 186 | 7737 | 0.0821 | 70031 |

Invalid probe losses were zero for all runs; this bounded grammar/data did not
overflow. All search loss calls visited all 17 training rows: row attempts are
17*loss calls. In enabled mode fitting loss calls equal the global total (including
fresh baselines); disabled mode fitting calls are zero. Full variation rejections,
fallbacks and all row counts are in the raw log. Every final report evaluated 37
additional prediction rows and zero additional training-loss calls.

The training-selected representative is enabled seed 1 (lowest final MSE, with
nodes/text/seed ties as declared). It is not selected by exterior performance.

## Discovered expressions

These are actual exports; no simplification is used for scoring or reporting.

Seed 0, fitting off:

```text
((((2.0 * x0) - 1.7779410965756455) * x0) - ((((1.0321839051226398 + x0) - 1.7779410965756455) - x0) - (((2.0 - 1.7779410965756455) * (2.0 * x0)) * ((1.0321839051226398 * x0) - (-0.062794902145230758)))))
```

Seed 0, fitting on:

```text
(((1.1000000238418579 - x0) * ((x0 * (-0.5)) - (x0 * 2.0))) + (x0 - (-0.62499999641787163)))
```

Seed 1, fitting off:

```text
(1.2354539037127612 + ((((2.0 + x0) * ((-0.94486647787105582) + 1.0)) * ((0.0 - x0) + (2.6710806781377983 - x0))) + (((x0 * 2.6710806781377983) + 1.0) * (x0 + (-1.0)))))
```

Seed 1, fitting on:

```text
(((x0 * (-1.7499999944629323)) + (x0 - x0)) + (((3.7177892029285431 - (x0 + 5.4677892000096104)) - ((x0 * x0) * (-2.5))) - ((-2.3750000102025854) - x0)))
```

Seed 2, fitting off:

```text
((((1.6533338761520309 * 1.5841116296712947) * (x0 * x0)) - (1.5841116296712947 - 2.0)) - (1.7250422931513079 * x0))
```

Seed 2, fitting on:

```text
(((x0 * (-5.1259497553110123)) + (1.8306960127819689 - (((-3.6613927483558655) * x0) * (x0 + x0)))) * ((0.341400146484375 - x0) + (x0 - (x0 - x0))))
```

Seed 3, fitting off:

```text
(((0.13198206470514329 * 2.0227994948546542) - x0) + (((1.0 + x0) - ((-1.7383145453217974) * ((-1.0) + x0))) * x0))
```

Seed 3, fitting on:

```text
(((2.5000000077672428 * x0) * (x0 + (-0.70000001788139343))) - ((x0 - x0) + (-0.625)))
```

Seed 42, fitting off:

```text
(((1.0 * ((1.0049609565666859 + x0) - (x0 * x0))) * (((-1.0) + 0.39988419363139727) + ((-0.065857222331996645) * (-1.0)))) - (((-1.1775426817271981) + ((-0.065857222331996645) * (-1.0))) - (x0 * ((-1.1775426817271981) + (x0 + x0)))))
```

Seed 42, fitting on:

```text
(0.62499988338358925 - ((x0 - 0.69999997370289346) * (x0 * (-2.5000000324342837))))
```

## Independent fixed-structure checks

These fixtures isolate constant optimization and are never fed to initialization.
Structure is a*(x*x)+b*x+c with independent a=b=c=1. Bounds [-8,8], initial step 1,
minimum step 1e-11, at most 1,024 loss calls, 512 sweeps, 8,192 proposals,
training threshold 1e-20. All three prescribed data ranges meet 1e-6.

| Target coefficients | Recovered coefficients | Loss calls | Training MSE | Train / interior / exterior max error |
|---|---|---:|---:|---|
| 2.5, -1.75, .625 | 2.5, -1.75, .625 | 44 | 0 | 0 / 0 / 0 |
| 1.7, -.65, 2.3 | 1.6999999999534339, -.65000000002328306, 2.3000000000465661 | 324 | 5.21769479467653e-21 | 1.8626434e-10 / 1.6079849e-10 / 7.9162277e-10 |

Each returned score is independently recomputed through the accounted objective
in the fixture test; that additional verification call is outside the fit's local
44/324 calls and inside that objective's global total. The coupled all-zero fixture
honestly stalls without improvement; no global or unique-optimum claim is made.

Fitter and integration correctness are qualified by deterministic contracts,
including failures and budgets, independently of this search evidence. Next add
strict division/logarithm semantics; stronger solvers and CPU profiling remain
separate work before qualifying the indexed-prime experiment.
