# Multivariate symbolic-regression campaign

Configuration frozen before the first run: deterministic serial CPU, GCC/libstdc++13
Release on the existing Jetson, seeds **0,1,2,3,42**; 64 slots, 2,000 generation
attempts, 100,000 actual training-loss calls and independently 100,000 outer
candidate requests. Limits: 31 nodes, depth 6 (leaf depth 1), absolute constants 8.
Linear-rank selection, two elites, crossover .9, mutation .35, initial depth cap 4,
mutation cap 3, half full/grow, grow probability .65, structural proposal limit 8.
Grammar: variables x0,x1 and +,-,*; constant terminals half uniform discrete
{-2,-1,0,1,2}, half uniform continuous [-8,8). No target tree or skeleton is supplied.

Refine every private slot, including initialization, elites and unchanged clones:
bounds [-8,8], step 1, minimum step 1e-8, at most 256 loss calls (fresh baseline
included), 128 sweeps, 8,192 proposals. Local and run training-MSE stopping threshold
1e-14. Numerical recovery is reported separately as maximum absolute error <=1e-6
on each dataset. Preserve all consumed work and committed-publication rules.

Target values are generated only in the application fixture: 2.5*x0-.75*x1+1.2.
Training is the Cartesian square of {-2,-1,0,1,2} (25 rows). Interior evaluation is
the square of {-1.5,-.5,.5,1.5} (16 rows). Exterior is the square of {-3,0,3} minus
(0,0) (8 rows), each with at least one coordinate outside [-2,2]. Row order is
ascending x0 outer loop, ascending x1 inner loop. No normalization or sample sorting.

Each result uses committed training MSE, node count, then faithful text. The
campaign representative uses the same ordering followed by lowest seed. Finalize
selection and any training-only adjustment decision before evaluation-only metrics.
If a seed misses training recovery, stop before held-out metrics, inspect training
work/variation diagnostics, fix demonstrated defects, and allow at most one
configuration adjustment within these ceilings, retaining original results.

Run `timeout 600s ../build-release/genetik_multivariate_demo`. The in-memory example
and file executable both call the existing public symbolicRegression API. Full
campaigns remain outside CTest; deterministic and bounded smoke tests qualify
engineering separately from recovery.

No configuration adjustment was needed: every seed met training
recovery before any evaluation-only metrics were calculated. All five also recovered
interior and exterior at the separate 1e-6 threshold. This is measured sample
agreement, not algebraic equivalence or a general convergence guarantee.

## Results

The representative is seed 42 by lowest training MSE (then nodes/text/seed ties as
predeclared). Selection was finalized before interior/exterior prediction reporting.
Every reported candidate was found in offspring, not initialization. Initialization
MSE is shown both before and after constant refinement; no target-specific
initialization was supplied.

| Seed | Raw initial MSE | Refined initial MSE | Final MSE | Train max error | Interior max error | Exterior max error | Nodes/depth | Discovery generation |
|---:|---:|---:|---:|---:|---:|---:|:---:|---:|
| 0 | 3.0448357 | 1.125 | 5.605983e-16 | 4.8147397e-08 | 3.7214622e-08 | 7.0012945e-08 | 17/5 | 3 |
| 1 | 1.8164064 | 0.125 | 8.5744111e-15 | 1.453507e-07 | 1.2911551e-07 | 1.7782106e-07 | 11/4 | 3 |
| 2 | 3.065 | 1.125 | 2.9725815e-15 | 1.1478298e-07 | 9.4884055e-08 | 1.5458082e-07 | 11/5 | 2 |
| 3 | 6.9493358 | 2.565 | 3.6196992e-16 | 4.1576375e-08 | 3.4162513e-08 | 5.6404098e-08 | 13/5 | 4 |
| 42 | 3.065 | 1.125 | 3.1402813e-18 | 1.772084e-09 | 1.772084e-09 | 1.7720847e-09 | 17/5 | 5 |

| Seed | Attempts/published | Outer requests / fits | Actual training-loss calls | Proposals | Accepted | Infeasible | Duplicate | Unused allowance | Search seconds |
|---:|:---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 3/3 | 256 | 27939 | 35509 | 3379 | 92 | 7695 | 72061 | 0.10109 |
| 1 | 3/3 | 256 | 30529 | 37982 | 3683 | 172 | 7489 | 69471 | 0.09849 |
| 2 | 2/2 | 192 | 16557 | 20930 | 1899 | 53 | 4505 | 83443 | 0.04783 |
| 3 | 4/4 | 320 | 26263 | 33465 | 3293 | 99 | 7412 | 73737 | 0.07145 |
| 42 | 5/5 | 384 | 39323 | 50107 | 4134 | 593 | 10542 | 60677 | 0.12177 |

All stops were training_loss_threshold. Fitting was invoked once per outer request,
and fitting loss calls are the entire training-loss total, not an additional total.
Invalid trial losses were zero for this bounded data/grammar. All loss attempts
visited 25 rows, so training row attempts are 25*loss calls. Every run separately
reported 49 prediction rows (25 training +16 interior +8 exterior), zero reporting
training-loss recomputations, and no invalid predictions. Work already consumed
would remain counted on an aborted attempt, though none occurred here.

Complete 17-digit results and variation counters are retained in
[multivariate-campaign.txt](multivariate-campaign.txt). The qualification rerun
matched the initial expressions, metrics and existing work counters exactly;
reporting gained additional variation counters, and elapsed times naturally differ.
The file workflow with seed 42 and the same data/configuration independently
returned the same expression, training MSE and 39,323 training-loss calls.

## Faithful discovered expressions

Seed 0:

```text
(((x1 * 0.25000001490116119) - (((-1.2029296830837) + 0.0029296875) + (x0 + x1))) + (x0 - ((-2.4999999930356127) * x0)))
```

Seed 1:

```text
((0.74999996752963138 * (1.6000001764834053 - x1)) + ((x0 * 1.5) + x0))
```

Seed 2:

```text
((((-0.48000001907348633) - x1) - (x0 + (x1 * (-1.3000000086295547)))) * (-2.4999999739657728))
```

Seed 3:

```text
(((-0.80000001192092896) + ((3.25 * x0) + 2.0)) - (0.74999999258613848 * (x1 + x0)))
```

Seed 42:

```text
((1.6800000071525574 + (((-0.75) * x1) + (x0 + (-0.48000000538047338)))) + ((0.0 + x0) * ((-0.5) - (-2.0))))
```

## Interpretation

Engineering acceptance rests on deterministic bindings, higher-index variation,
three-input fitting, exact parsed/in-memory repeatability and failure/format tests,
not on this recovery rate. Existing publication, invalid-fitness, accounting and
ownership contracts were reused without a core change. Larger input counts,
different scales, noise and harder target families may require different budgets
or procedures. Coordinate fitting can stall; it cannot introduce variables missing
from the structure. Strict division/logarithms are the next arithmetic work before
prime regression as a later generic-module example and CPU profiling/delivery.
