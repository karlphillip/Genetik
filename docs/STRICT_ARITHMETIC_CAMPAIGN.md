# Strict division/logarithm discovery campaign

Frozen before the first run: serial CPU, GCC/libstdc++13 and runtime libm on the
qualified Jetson; seeds 0,1,2,3,42; 64 slots; 2,000 generation attempts;
100,000 actual training-loss attempts and independently 100,000 outer requests;
31 nodes; leaf depth 1, maximum depth 6; absolute constants <=8.

Enabled operators, in order: add,sub,mul,div,log. At each branch, select uniformly
among enabled entries feasible under remaining child-node and depth budgets.
All five have weight 1 when feasible; only log is feasible with two remaining
nodes. No resampling for mathematical validity. Existing half full/half grow,
grow .65, initial depth cap 4, mutation cap 3, eight structural proposals,
crossover .9, mutation .35, two elites and linear-rank selection remain unchanged.
Terminal constants: half uniform {-2,-1,0,1,2}, half uniform [-8,8).

Refine all private slots including initialization, elites/clones. Bounds [-8,8],
initial step 1, minimum 1e-8, 256 loss attempts including fresh baseline,
128 sweeps, 8,192 proposals, local training-MSE threshold 1e-14. Run threshold
is training MSE <=1e-14. Separately report max absolute error <=1e-6 on each range.
Local invalid trials retain incumbent progress; invalid starts skip. Global stops
retain committed state without retry and count consumed work.

Data generation alone knows log(x0)+x1/x0:

- Training: x0=.5,1,2,4,8; x1=-2,-1,0,1,2; 25 rows.
- Interior evaluation: x0=.75,1.5,3,6; x1=-1.5,-.5,.5,1.5; 16 rows.
- Exterior evaluation: x0=.25,16; x1=-3,-1,1,3; 8 rows.

Each is a Cartesian product, ascending x0 outer loop, ascending x1 inner loop.
All target labels are finite. No zero/negative x0 target rows are generated.
Direct domain probes are separate, without fabricated labels. Runtime std::log
produces the target double values under the declared numerical environment;
files use classic-locale max_digits10 round-trip output.

Models use committed training MSE, then nodes, then text; representative uses that
ordering then lowest seed. Finalize all models and representative before evaluating
held-out data. If no seed reaches training threshold, inspect training-only work,
invalidity and variation diagnostics, fix actual defects and permit at most one
recorded configuration adjustment within the same ceilings/seeds. Preserve initial
results. Held-out errors cannot drive tuning. Failures/misses are reportable results,
not deterministic correctness-test failures.

Run `timeout 600s ../build-release/genetik_multivariate_demo --arithmetic`.
Generate tables with `--arithmetic --generate ../arithmetic-data`; the existing file
executable opts in via `--operators add,sub,mul,div,log`. Both use symbolicRegression
through the same shared engine. No known target tree/features are supplied to GP.

## Results

Seeds **0,2,42** reached the training stopping threshold and independently recovered
training, interior and exterior at max error <=1e-6. Seeds **1,3** exhausted the
loss allowance and missed all three recovery thresholds. No configuration adjustment
was made: the predeclared training-only decision had three successes, so the
no-success adjustment condition did not apply. Held-out results did not drive tuning.
No NoViableCandidates or unexpected operational failure occurred.

The training-selected representative is seed 42, finalized before all held-out
metrics. Every reported candidate was discovered in offspring. Complete 17-digit
outputs, expressions, diagnostics and counters are retained in
[strict-arithmetic-campaign.txt](strict-arithmetic-campaign.txt).

| Seed | Raw initial MSE | Refined initial MSE | Final training MSE | Train max error | Interior max error | Exterior max error | Nodes/depth | Discovery generation |
|---:|---:|---:|---:|---:|---:|---:|:---:|---:|
| 0 | 2.472609 | 1.8909069 | 7.5564558e-16 | 3.2353417e-08 | 3.0861122e-08 | 3.4904512e-08 | 10/4 | 10 |
| 1 | 2.472609 | 1.890906 | 0.64027194 | 2.4179849 | 1.12871 | 9.893765 | 15/6 | 11 |
| 2 | 2.13125 | 2.0611832 | 1.3197121e-15 | 9.9536507e-08 | 4.9768254e-08 | 2.9860952e-07 | 10/5 | 9 |
| 3 | 2.472609 | 1.890906 | 0.048395537 | 0.30226269 | 0.33758345 | 1.8977915 | 13/5 | 9 |
| 42 | 1.907304 | 1.2076509 | 5.3110431e-16 | 7.0562792e-08 | 2.3520931e-08 | 4.2337676e-07 | 10/4 | 8 |

| Seed | Attempts/published | Outer requests / fits | Actual loss calls | Invalid losses / invalid probes | Proposals | Accepted | Infeasible | Duplicate | Unused | Search seconds | Stop |
|---:|:---:|---:|---:|:---:|---:|---:|---:|---:|---:|---:|---|
| 0 | 10/10 | 704 | 68845 | 176/88 | 86015 | 9600 | 1400 | 16367 | 31155 | 0.24732 | training_loss_threshold |
| 1 | 12/11 | 813 | 100000 | 164/94 | 126840 | 13302 | 2268 | 25193 | 0 | 0.36515 | evaluation_budget |
| 2 | 9/9 | 640 | 60044 | 254/158 | 76888 | 8485 | 1710 | 15698 | 39956 | 0.20310 | training_loss_threshold |
| 3 | 10/9 | 681 | 100000 | 83/22 | 126088 | 13202 | 1525 | 25032 | 0 | 0.36066 | evaluation_budget |
| 42 | 8/8 | 576 | 51624 | 138/61 | 66252 | 8635 | 2526 | 12589 | 48376 | 0.17028 | training_loss_threshold |

Fitting loss calls equal the total, not an extra count. All returned-invalid losses
include baselines; invalid probes are a subset. Early invalid rows explain why row
attempts are below 25*loss calls; exact actual row/variation counts are in the log.
All finalized models were valid on every declared training/interior/exterior row.
Each run separately reports 49 prediction rows plus two direct domain probes
without target labels. There is no reporting training-loss recomputation.

Seed 1 consumed 45 slot requests in an abandoned generation after generation 11;
seed 3 consumed 41 after generation 9. Their 100,000 loss calls include that private
work; the reported models remain from committed generations, not better/worse
unpublished fits. Misses remain evidence of this bounded heuristic's limits.
Their training-best structures lacked the required log/div combination: seed 1
used polynomial operations, while seed 3 had a quotient but no logarithm. Variation
was active and had zero fallbacks in both runs; the finite loss allowance stopped
search before recovery. No implementation defect was indicated by these diagnostics; no post-hoc
budget increase, retry or model repair was performed.

The full file run (seed 42, generated max_digits10 rows, same expanded grammar)
matched its in-memory expression, score, every available non-time work counter and
51,624 loss calls. Deterministic short file/in-memory and same-seed tests qualify
this independently of recovery. The original binary-only multivariate campaign was
also repeated specifically to check draw-order compatibility: all five prior
expressions, metrics and existing work/variation counters matched exactly.

## Faithful discovered expressions

Seed 0:

```text
(((x1 / x0) - (-2.9802322387695312e-08)) + (log(x0) * 0.9999999963195485))
```

Seed 1:

```text
((x1 + 0.032454761435163348) - (((-3.3154540779453523) + x1) * (0.10101879714180839 * ((x0 + (-3.0295090737776365)) + 1.9566235244274139))))
```

Seed 2:

```text
(log(x0) + (((4.1999999284744263 + (-3.199999953358553)) * x1) / x0))
```

Seed 3:

```text
(((0.3565363944009361 - x1) * ((1.5620880126953125 / x0) * (-0.64016857005124805))) + (x0 * 0.29329147934913635))
```

Seed 42:

```text
(log(x0) + ((x1 + 0.0) / (x0 + (-8.8203488957105947e-09))))
```

## Domain diagnostics and interpretation

The constructed six-node target is invalid at x0=0 and at negative x0; these are
direct expression probes with no labels. The separately constructed quotient is
valid at negative nonzero x0. Evolved models are not required to share a target's
global domain: seed 1 was finite at both diagnostic points, and seed 3 was finite
at x0=-1 but invalid at zero. Successful seeds were invalid at both. The log records
their actual predictions, without invented target errors. No sample agreement is
claimed to establish algebraic/global/domain equivalence.

Arithmetic/structural, fitting/publication and workflow acceptance rest on focused
contracts and the full build matrix. Three successful seeds and two misses are
separate search evidence. Next is the preserved prime-regression experiment as an
example of the generic module, followed by measured CPU profiling/production work.
