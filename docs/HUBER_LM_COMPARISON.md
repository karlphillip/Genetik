# Huber Levenberg–Marquardt comparison

This fixed comparison uses Huber delta `1`, expression `a*x^b`, starting
coefficients `(2, 1.4)`, and bounds `[0.1, 4]` for both coefficients. Training
inputs are `{0.5, 1, 1.5, 2, 2.5}` with targets `2*x^1.4`, except the center
point at `x=1.5` has an added outlier of `5`. Clean held-out inputs are `{3, 4}`.
The coordinate limits were fixed at 3,000 loss attempts, 40 sweeps, and 3,000
proposals. LM limits were fixed at 100 iterations, 300 objective evaluations,
and 300 Jacobian evaluations; its existing arithmetic and workspace limits
remain 100,000,000 operations and 64 MiB. These limits were set before running
the fixture.

| Fitter | Coefficients `(a, b)` | Training Huber loss | Held-out RMSE | Prediction evaluations | Stop |
|---|---:|---:|---:|---:|---|
| Existing coordinate | `(2.44336, 1.19492)` | `0.852347` | `0.810748` | `575` | `sweep_limit` |
| Existing main LM | Unsupported; `fitConstants` rejects Huber | — | — | — | — |
| New Huber LM | `(2.46663, 1.18438)` | `0.852235` | `0.858857` | `340` | `minimum_step` |

Training loss is independently checked with `RegressionProblem::measure`; the
held-out RMSE is recomputed from predictions and the clean generating rule.
Coordinate prediction evaluations count attempted training rows. LM's forward
counter includes both public objective scoring and residual/Jacobian evaluation.

As a direct analytic check, fitting a constant to `[0, 0, 0, 0, 100]` with
delta `1` gives `c=0.25` and mean Huber loss `19.875`. The existing coordinate
fitter reaches that solution in 55 loss attempts; Huber LM reaches it in 11
objective evaluations (110 counted forward evaluations, including both scorer
and residual/Jacobian passes). The regression test also checks zero residual,
both signed Huber thresholds, the linear-tail Jacobian, accepted-score
retention at the evaluation budget, and symbolic-regression refinement.
