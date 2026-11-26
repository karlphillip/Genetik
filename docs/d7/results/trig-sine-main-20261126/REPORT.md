# D7 clean trig_sine comparison at current main

- Base and implementation SHA: `f6810d7a1c1dff3327f42f2b760ebb5a70775e4a`
- Frozen D7 protocol SHA256: `b0b07d2ae418ca71b024fbaa1374264348d92f2b8290606efda2969a432f1ffd` (Git blob `98d7b78e731d30a6e2ee82ed825095d289f8a9a3`, introduced by commit `a33c77bac744afe4025441ab63df74b3a70356ff`)
- Training CSV SHA256: `f1a555b6c5bd32fd872dbcfc1b434359b1633f27cac0572f52432a4b5d67366e`; held-out CSV SHA256: `7c1c2bdd458a45c9109ab2c8fe3b7710b6c2e32adbc0c2f953992440c8a87259`
- Raw logs: `Genetik-data/research-runs/d7-trig-sine-current-20261126/raw/` (copied without modifying previous D7 evidence).

## Protocol and capture rehearsal

Used the frozen D7 trig_sine clean configuration, adapters and data. PySR 2.7.2 uses SymbolicRegression.jl 2.7.2 and Julia 1.10.3. PyOperon is 0.6.2 with core commit `fad10f087c592812b1a9ecfabce669f37bf459f7` and pinned wheel SHA256 `0474159102f15efe3dc5a8b2a084d11565aee9fd23f0d42a096767aa49b5a357`. The resolved source/environment pin-file hashes are in `scorecard.json`. Three seeds (0, 1, 42) ran serially, one process at a time, pinned to CPU 0; outer wall cap 120 s; process-tree RSS ceiling 6 GiB. OMP/BLAS worker limits were one; Julia search parallelism was serial. The runtime can still start helper threads, recorded per run in the machine scorecard. The host is aarch64 Cortex-A78AE with 8 online CPUs; no cgroup CPU or memory maximum was configured. Available memory at the resource check was about 11.5 GiB. The limits were inherited from the committed D7 protocol, not changed for these results. Search and fitting received training rows only. Held-out rows were first read by the independent scorer after the frozen-candidate manifest was written.

Before target runs, the existing D7 supervisor and adapter commands were exercised on the 64-row synthetic task `y=x0+0.25`; a separate 9-point assessment set included `x0=0`. Genetik returned a candidate but it was invalid at assessment x=0; PySR timed out at the 180 s synthetic outer preflight limit but its Hall of Fame captured `x0 + 0.25` (synthetic normalized RMSE 0); PyOperon completed and returned an expression with synthetic normalized RMSE 1. The synthetic run records and scores are in `synthetic-capture-runs.jsonl` and `synthetic-capture-rehearsal.json`. The rehearsal JSON also records an initial setup miss where no synthetic CSV had been materialized; that adapter exited before search, and the corrected attempt captured the Genetik candidate. The PySR timeout preserved HOF capture, so target runs used the requested 120 s wall limit and were scored from the HOF when available.

The external wall measurement includes imports/startup. Genetik reported its own search interval, so adapter overhead can be estimated for its two captured runs. PySR and PyOperon did not return their adapter timing fields before the external timeout; their startup/fitting split is unavailable. Their process CPU and wall totals remain recorded.

## Per-run comparison

| Framework | Seed | Process status | Candidate equation (best-so-far) | Held-out normalized RMSE | Numerical recovery | Sinusoid-family recovery | CPU / wall (s) |
|---|---:|---|---|---:|---|---|---:|
| Genetik | 0 | `wall_timeout` | `(sin(((x0 * sin(3.6651327288931768)) - (x0 - (-0.3000158843895111)))) * (((-4.7557570443610686) - 1.6247422933939539) * 0.31346876013164504))` | 9.59509e-05 | no | yes | 105.26 / 120.04 |
| Genetik | 1 | `wall_timeout` | `(sin((((x0 - exp((-3.9998290103113394))) * (((-0.35641673078372982) / (-1.8728758853113394)) * 0.0021241146886605833)) + ((x0 * 1.4995890767495386) + ((0.00017098968866058328 * (-0.50041092325046144)) + sin(exp((-1.1881319430333228))))))) * 2.0001709896886606)` | 8.40427e-05 | no | yes | 105.67 / 120.03 |
| Genetik | 42 | `wall_timeout` | **missing** | — | not scored | not scored | 106.97 / 120.01 |
| PySR | 0 | `wall_timeout` | `sin(x0 + ((((x0 * 0.999999999995961) * (-0.5999999999999097 - x0)) * 0.999999999995961) * (((x0 / x0) / x0) * -0.5000000000039397))) * 2.000000000000078` | 1.98585e-13 | yes | yes | 103.30 / 120.19 |
| PySR | 1 | `wall_timeout` | `sin((0.43853529846439376 - (-1.5000000000055649 * (x0 - 0.09235686564155902))) - ((x0 + 0.2892148519215121) * 5.430886401390012e-12)) * 1.9999999999969147` | 1.51581e-12 | yes | yes | 94.91 / 120.22 |
| PySR | 42 | `wall_timeout` | `sin((x0 + 0.2999999999998327) + (x0 * ((x0 + 0.4999999999987395) - x0))) + sin((x0 + 0.2999999999998327) + (x0 * ((x0 + 0.4999999999987395) - x0)))` | 2.26307e-12 | yes | yes | 91.67 / 120.29 |
| PyOperon | 0 | `wall_timeout` | **missing** | — | not scored | not scored | 92.82 / 120.09 |
| PyOperon | 1 | `wall_timeout` | **missing** | — | not scored | not scored | 93.31 / 120.14 |
| PyOperon | 42 | `wall_timeout` | **missing** | — | not scored | not scored | 91.34 / 120.12 |

Numerical recovery uses the frozen clean criterion as implemented by the D7 assessor: held-out `RMSE <= 1e-6 * max(1, noiseless training-target RMS)`. Held-out normalized RMSE is held-out RMSE divided by the held-out noiseless target RMS. Sinusoid-family recovery means SymPy simplification leaves x-dependent sine/cosine terms with affine arguments and no non-trigonometric x dependence. This is a family classification, not a proof of equality or domain safety. Symbolic equality was not proved for any candidate. PySR seed 0 contains a removable `x0/x0` singularity in its exported expression; the held-out finite sample does not contain zero, so its numerical recovery must not be interpreted as a whole-domain certificate.

### Search work and timing

- **Genetik seed 0**: Genetik candidate evaluations `32036`; training-loss attempts `1865985`.
  Adapter search time 120.007 s; outer-minus-adapter overhead 0.034 s.
- **Genetik seed 1**: Genetik candidate evaluations `29452`; training-loss attempts `1723369`.
  Adapter search time 120.007 s; outer-minus-adapter overhead 0.026 s.
- **Genetik seed 42**: native work counters unavailable because no adapter result was emitted.
  Startup versus fitting time was not separable before timeout.
- **PySR seed 0**: PySR HOF rows 9, selected loss 8.55337e-26, complexity 22.
  Startup versus fitting time was not separable before timeout.
- **PySR seed 1**: PySR HOF rows 8, selected loss 4.37554e-24, complexity 16.
  Startup versus fitting time was not separable before timeout.
- **PySR seed 42**: PySR HOF rows 9, selected loss 1.04987e-23, complexity 25.
  Startup versus fitting time was not separable before timeout.
- **PyOperon seed 0**: native work counters unavailable because no adapter result was emitted.
  Startup versus fitting time was not separable before timeout.
- **PyOperon seed 1**: native work counters unavailable because no adapter result was emitted.
  Startup versus fitting time was not separable before timeout.
- **PyOperon seed 42**: native work counters unavailable because no adapter result was emitted.
  Startup versus fitting time was not separable before timeout.

A process timeout remains `wall_timeout`, even when its best-so-far model was captured. Missing equations are not scored; all missing-candidate rows are retained in `missing-candidates.jsonl`.

## Genetik coefficient-refinement diagnostic

The frozen Genetik seed 0 and 1 structures both classified as sinusoid-family expressions. Each was passed to the existing `fitConstants` coordinate fitter with only the clean training CSV and its frozen topology. The diagnostic used a separate allowance of 64 loss attempts, 32 sweeps, 2,048 proposals, bounds [-8,8], initial step 1, and minimum step 1e-6.

| Seed | Train MSE before → after | Held-out normalized RMSE before → after | Fit work | Result |
|---:|---:|---:|---|---|
| 0 | 1.89987e-08 → 1.89987e-08 | 9.5950903e-05 → 9.5950903e-05 | 64 loss attempts, 64 proposals, 7 sweeps | `evaluation_limit`, no coefficient change |
| 1 | 1.34298e-08 → 1.34298e-08 | 8.4042749e-05 → 8.4042749e-05 | 64 loss attempts, 64 proposals, 4 sweeps | `evaluation_limit`, no coefficient change |

The refinement processes completed in 0.082 s wall each. Instrumented fitter CPU time was 0.003344 s for seed 0 and 0.004664 s for seed 1. Both fits exhausted their loss-attempt cap and left the frozen expression unchanged. This outcome does not show whether a larger or better-scaled fit budget would improve the model.

## Engineering priority indicated by these results

For Genetik, coefficient refinement is the most directly supported next experiment: the two returned structures simplify to affine sinusoids, their coefficients are close but not accurate enough for the clean threshold, and the bounded post-search fitter made no change before exhausting its loss budget. A tuning study would still be needed to determine whether more budget or another fit strategy can close the gap.

Candidate capture is independently a clear blocker for interpreting PyOperon: all three runs timed out with no equation emitted or captured. Genetik also missed seed 42. PySR shows that timeout HOF capture can preserve useful models. Therefore the overall priority between improving Genetik refinement and implementing PyOperon best-so-far capture is inconclusive from this single task; the observed candidate-capture gaps prevent a fair structure-quality conclusion for PyOperon.

## Artifacts

- `scorecard.json`: full run table, protocol and dataset identities, work/timing, classification, and outcomes.
- `frozen-candidates.jsonl` and `missing-candidates.jsonl`: frozen model identities and explicit misses.
- `assessments.jsonl`: independent held-out assessments after freezing.
- `refinement-seed-0.json`, `refinement-seed-1.json`, and `refinement-assessments.jsonl`: separate training-only fitter diagnostics.
- `synthetic-capture-rehearsal.json`: synthetic capture rehearsal and distinct assessment scores.
- `run-records.jsonl`: raw D7 supervisor/adaptor records. Raw logs and HOF files are separately copied into the sibling `Genetik-data/research-runs/d7-trig-sine-current-20261126/raw/` directory; `raw-evidence.sha256` lists their hashes.
