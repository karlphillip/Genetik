# S14 Results

## Source and protocol identity

- Research branch: `research/constructive-multivariate-rational-s14`.
- Protected-main base verified from `origin/main`: `d4f55dde8378878a6bef151705a38deed35fdf02`.
- Campaign implementation SHA: `62df550cc3eb2a9462ef211c49e1844949431c5b`.
- Protocol: `s14-multivariate-rational-confirmation-v1`; its exact SHA and all dataset hashes are in the durable evidence manifest.
- Preflight: 24/24 task/track entries returned `domain_validated`; the execution wrapper required the exact manifest identity and the runner checked data hashes and effective grammar/resource settings before creating run records.
- Runs: 192 expected, 192 completed, zero timeouts, zero failed scientific outputs. All ran sequentially on CPU 0 under the per-run 120 s, 6 GiB RSS and 6 GiB address-space ceilings. Campaign wall time was about 10 s; slowest run 0.145 s; maximum isolated child RSS about 12.4 MiB (per-run values are retained).

An initial pre-freeze parser smoke is retained separately and excluded. One runner launch failed before executing a scientific process because GNU `time` was absent. That setup failure is preserved separately; the runner was changed to use Linux `wait4` for per-child CPU and peak RSS, then the exact frozen 192-run campaign completed. No outcome was selected for retry.

## Mathematical correctness and gates

**Gate A passed.** The deterministic contract test validates the total-degree basis order `1,x,y,x²,xy,y²`; linear and quadratic polynomials; proper and improper rational functions; mixed `xy`; denominator normalization; coefficient and variable binding; expression materialization; deterministic serialization; and consistency between coefficient vectors and executable expressions. Clean identifiable fixtures reproduce values to roughly `1e-15`–`1e-16` RMSE. Tests also cover duplicate/insufficient observations, rank deficiency, nearly dependent columns, large and small input scales, nonfinite inputs, domain failure, near-poles, coefficient/resource bounds, and regression use of the existing scalar rational fitter.

**Gate B passed for bounded use, with an explicit near-pole limitation.** On the three non-stress rational families, all clean tracks and all 9 noisy tracks per constructive method met the frozen predictive thresholds. The noisy near-pole fits had low training error but failed held-out/extrapolation thresholds on all three noise realizations. Their full-box domain assessment was `Uncertain`; the clean fit's assessed rectangle correctly returned `Invalid / DemonstratedZero` because its denominator zero set crosses the rectangle. A finite set of stress points can still be predicted accurately by the clean fit; that does not make the entire rectangle pole-free. QR+LM did not improve threshold-pass count over QR and cost substantially more work.

The gate therefore supports a bounded mathematical method in well-separated domains. It does not support a general near-pole or extrapolation-safety claim.

## Independent scoring method

The C++ runner reports useful in-process measures, but the retained primary scores were independently recomputed by `benchmarks/s14/analyze_results.py`: a small Python AST interpreter for Genetik's serialized expression syntax, Python CSV input, and independent RMSE/invalid-point calculations. The interpreter accepts only variables, finite literals, arithmetic, integer-power calls, and sine. It does not use the Genetik evaluator. Scored datasets contain clean held-out and extrapolation references; noisy training observations were never used in scoring.

The frozen predictive threshold is not a symbolic-equivalence test. A GA equation can pass a finite-observation score without having the rational target topology. The analysis labels this `predictive_threshold_pass`, not equation recovery. Exact noiseless coefficient recovery is established separately for the constructive fixtures by comparing recovered coefficients against independently declared test equations.

## Rational-task outcomes

| Family | Constructive QR | Constructive QR+LM | GA coordinate | GA LM |
|---|---:|---:|---:|---:|
| Mixed numerator with `xy` | 4/4 predictive threshold passes | 4/4 | 0/12 | 2/12 |
| Denominator with `xy` and `x²` | 4/4 | 4/4 | 0/12 | 0/12 |
| Quadratic numerator and denominator | 4/4 | 4/4 | 0/12 | 0/12 |
| Near-pole stress | 1/4 | 1/4 | 0/12 | 0/12 |
| **Total** | **13/16** | **13/16** | **0/48** | **2/48** |

The four tracks per constructive family are one clean and three noisy datasets. GA has three paired seeds per track. The two GA-LM threshold passes were noisy mixed-xy cases; their expressions were not the supplied rational topology. They count as predictive passes only. For paired rational GA runs there were 46 failures under both methods and two transitions from coordinate failure to LM predictive pass; no reverse transition. The exact two-sided McNemar p-value for those two discordant pairs is 0.5, and the small sample is inconclusive.

All four clean rational targets were recovered to numerical precision by constructive selection, including the full quadratic-over-quadratic case. Noisy model-family selection was less stable: QR selected the reference degree pair on 8/12 noisy rational tracks; LM on 6/12. The training-only BIC policy can choose a simpler/different pair under noise while still predicting well on the sampled domain. Degree agreement is not proof of parameter uniqueness.

Representative clean mixed-`xy` fit:

```text
P = 1 + 0.3999999999999993 x - 0.2999999999999997 y
    - 3.49e-16 x² + 0.2000000000000005 xy - 1.37e-16 y²
Q = 1 + 0.0999999999999992 x + 0.1500000000000003 y
```

Independent held-out and extrapolation RMSE were both approximately `2.2e-16`. Representative clean full-quadratic fit recovered numerator coefficients approximately `[1, .2, -.4, .1, .2, -.15]` and denominator `[1, .08, -.05, 0, .03, 0]`, in the declared basis order, with held-out RMSE `2.8e-16` and extrapolation RMSE `5.3e-16`.

For noisy near-pole seed 14011, QR estimated `Q=1 - .7583x - .4934y` against reference `1 - .7x - .5y`. Training RMSE was `0.0082`, while independent held-out RMSE was `0.339` and extrapolation RMSE `12.35`; the rectangular assessment was uncertain. This directly shows the instability of denominator/pole location under noise despite a low training residual.

## Controls

The polynomial control was reconstructed by the rational family with denominator degree zero. Median normalized held-out/extrapolation RMSE was about `0.0033/0.0051` for QR and QR+LM. The rational-family model approximated the sine control with median normalized errors around `0.0047` held out and `0.0165` on extrapolation. These are approximation measurements, not recovery of a sine equation. GA control-task median errors were materially larger under the fixed short candidate budget; this does not establish broad superiority because the constructive model family and computational work differ.

## QR versus QR+LM

Across 24 datasets, LM reduced training RMSE on 15, but reduced independent held-out RMSE on only 7 and extrapolation RMSE on only 7 (ties included in neither count). Rational-only training improvement occurred on 11/16 datasets; rational predictive threshold passes remained 13/16 for both methods.

Median work across all 24 datasets:

| Measure | QR | QR+LM |
|---|---:|---:|
| Training-loss evaluations | 9 | 9 |
| Jacobian evaluations | 0 | 68 |
| Mathematical operation units | 22,002 | 526,286 |
| Linear-algebra operation units | 34,519 | 467,004 |
| Wall seconds | 0.023 | 0.043 |
| Peak RSS | about 12.5 MiB | about 12.5 MiB |

LM's training refinements did not reliably translate to held-out or extrapolation gains. It used roughly 24x the mathematical-operation work and 13.5x linear-algebra work at the medians, and about 1.9x wall time. It remains optional.

## GA comparison

Each GA method used 72 runs total and reached the 512-candidate cap in every run. Median wall time was approximately 0.043 s for coordinate and 0.062 s for LM. Median training-loss attempts were about 4,959 and 2,202 respectively; LM consumed additional derivative/Jacobian work, and the existing GA interface does not expose a comparable exact mathematical-operation count, so those cells remain unavailable. Median held-out RMSE fractions across rational families were about 0.144 (coordinate) and 0.123 (LM); extrapolation fractions were about 0.231 and 0.239. These aggregate scores include different functions/scales and are descriptive, not a pooled superiority test.

The constructive method receives a rational-family inductive bias. It is not an equal hypothesis-space replacement for stochastic symbolic regression.

## Remaining limitations and interpretation

- Denominator normalization `Q(0,0)=1` excludes some coordinate representations; common-factor cancellation is not inferred.
- Degree two, two variables, 11 parameters, one worker, finite row/workspace limits only.
- No formal machine-verified interval arithmetic proof; assessments are bounded and conservative with explicit uncertainty.
- Noisy pole location is poorly conditioned, and extrapolation near poles failed despite small training residuals.
- Noisy training can select a different degree pair; model-degree recovery is less stable than clean coefficient recovery.
- The campaign tests several frozen functions, not arbitrary multivariate symbolic regression.
- GA math-operation accounting is unavailable; runtime and loss/work counters are reported separately.

No production API or default changed. This is worthwhile as a research capability for bounded, identifiable, pole-separated multivariate rational fits, but S14 does not justify automatic production integration or a universal rational-discovery claim.
