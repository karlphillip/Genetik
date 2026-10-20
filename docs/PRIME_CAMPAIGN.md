# Frozen prime-prefix approximation campaign

Configuration, datasets and selection were predeclared in PRIME_REGRESSION.md.
Pilot seed 2026: 0.257541261 s; 4,096 actual loss attempts; 4,079,017 actual row
attempts; 4,077 valid full-dataset losses; 19 invalid losses; 70 fit invocations.
There are 69 completed fits and one final partial fit (70 invocations); the pilot
completed_fits diagnostic excludes that partial invocation, while all its loss/row
work remains counted. The pessimistic six-run estimate with factor-two margin is 75.8031673 s, below
1,800 s. The common campaign and reproduction allowance is fixed at **100,000**
actual loss attempts per run, with unchanged per-fit limit 256. This decision uses
runtime only. No pilot expression is retained as a search candidate.

The main experiment runs once with seeds 0,1,2,3,42. All settings and this allowance
are fixed before search. All five models and QR baseline parameters are frozen
before any extrapolation assessment. No automatic adjustment or restart is planned.

## All five searches

All runs stop at the 100,000-call actual-loss cap with zero unused allowance.
No configuration adjustment, tuning or restart was performed. All discovery stages
are offspring, not initialization. The last partial generation is discarded; only
published models appear below. Raw initialization losses are recorded before fitting;
refined initialization losses use the accepted prepared population.

| Seed | Raw initial normalized MSE | Refined initial MSE | Final normalized MSE | Nodes/depth/constants | Discovery generation | Attempts / published | Candidate requests | Seconds |
|---|---:|---:|---:|---|---:|---|---:|---:|
| 0 | 0.020580668229999998 | 2.6425241448526163e-05 | 5.3601967624741316e-06 | 24/6/5 | 8 | 9 / 8 | 628 | 8.509487 |
| 1 | 0.0083271314398999929 | 0.00011684692849506492 | 1.0000040243893693e-05 | 13/5/3 | 9 | 10 / 9 | 695 | 8.450784 |
| 2 | 0.0016721220378997103 | 0.00011681874737724468 | 1.4636376958468287e-06 | 16/6/4 | 10 | 12 / 11 | 803 | 7.948218 |
| 3 | 0.0083271314398999929 | 0.00046762533640472888 | 8.8640860262019499e-06 | 11/5/4 | 12 | 13 / 12 | 833 | 6.705925 |
| 42 | 0.0083271314398999929 | 0.00046762533640472926 | 3.0452604371448425e-06 | 23/6/4 | 10 | 11 / 10 | 741 | 8.540555 |

All 500,000 search loss attempts are fitting calls (baseline or probes); they are
not added twice. Outer candidate requests and fit invocations coincide here.

| Seed | Actual row attempts | Invalid loss attempts | Fit proposals | Accepted steps | Invalid probes | Infeasible | Duplicates | Variation rejections / fallbacks |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| 0 | 99804196 | 204 | 120591 | 19525 | 182 | 1080 | 19841 | 78 / 0 |
| 1 | 99883485 | 121 | 121242 | 17971 | 87 | 1690 | 19958 | 66 / 0 |
| 2 | 99864635 | 145 | 124865 | 19309 | 98 | 1071 | 24364 | 74 / 0 |
| 3 | 99831169 | 169 | 124966 | 19908 | 135 | 592 | 24988 | 44 / 0 |
| 42 | 99935937 | 70 | 118904 | 18880 | 34 | 956 | 18404 | 89 / 0 |

Faithful searched expressions, without simplification:

Seed 0:

```text
((x0 / (x0 - ((5.0069351196289062 + (-1.98480224609375)) * (x0 - 1.1793319410959722)))) * ((x0 - log(0.15600989721665748)) + (x0 * (((-2.6280710109623868) + x0) * x0))))
```

Seed 1:

```text
(x0 - (((0.34853261709213257 - x0) - ((-0.84645298747583286) * x0)) * (x0 + 0.032958984375)))
```

Seed 2:

```text
(x0 * (((((-3.0) / (-3.0)) * x0) * (0.79185613989830017 / x0)) + (0.1121049374658476 * log(x0))))
```

Seed 3:

```text
(1.1105825901031494 * (((x0 * 0.12480717897415161) - (-0.607757568359375)) * (x0 + (-0.021231532096862793))))
```

Seed 42:

```text
((x0 / x0) / (((((-1.2226836979389191) + x0) - (x0 * 5.1611535400152206)) / x0) / (((-1.4906127750873566) + (x0 / x0)) - (x0 * 3.7531566172838211))))
```

The representative is **seed 2**, chosen by fitting MSE before extrapolation:

```text
x0 = double(n) / 1000.0
predicted_prime = 10000.0 * (x0 * (((((-3.0) / (-3.0)) * x0) * (0.79185613989830017 / x0)) + (0.1121049374658476 * log(x0))))
```

The learned tree has 16 nodes, depth 6 and four constant occurrences. Fixed 1000
and 10000 transformations are separate application constants. Redundant nodes and
individual fitted constants are deliberately retained; no cancellations rewrite
floating-point grouping or domains.

## Decoded continuous metrics

Values below are raw prime units, not normalized search loss. Every row is assessed.
Both range losses are full-dataset losses, not averages over a valid subset.
All final predictions are finite and integer-representable in both ranges.

| Model | Range | MSE | RMSE | MAE | Max absolute | Mean relative | Max relative |
|---|---|---:|---:|---:|---:|---:|---:|
| seed-0 | fitting | 536.0196762 | 23.15209874 | 18.70239215 | 63.67593578 | 0.04445984117 | 2.482766562 |
| seed-0 | extrapolation | 159399.2386 | 399.248342 | 326.0713922 | 831.006547 | 0.03555447671 | 0.08538030895 |
| seed-1 | fitting | 1000.004024 | 31.62284023 | 26.693133 | 110.305994 | 0.1866777168 | 55.15299699 |
| seed-1 | extrapolation | 22127.08129 | 148.7517438 | 135.9656897 | 249.728594 | 0.01505298282 | 0.02614685311 |
| seed-2 | fitting | 146.3637696 | 12.0980895 | 8.984338354 | 45.83964744 | 0.005633388777 | 0.9126866684 |
| seed-2 | extrapolation | 352.8573529 | 18.78449767 | 14.87430753 | 42.98120954 | 0.001694531104 | 0.004999559095 |
| seed-3 | fitting | 886.4086026 | 29.77261498 | 23.52819926 | 138.5837981 | 0.239192976 | 69.29189905 |
| seed-3 | extrapolation | 11897.28002 | 109.0746534 | 96.62804783 | 195.5292661 | 0.0106537878 | 0.02047212503 |
| seed-42 | fitting | 304.5260437 | 17.45067459 | 13.86364908 | 67.28920109 | 0.02125721264 | 1.697626095 |
| seed-42 | extrapolation | 5652.11195 | 75.18052906 | 73.64293983 | 100.3578287 | 0.008303392872 | 0.01167358715 |
| constant | fitting | 5489270.997 | 2342.919332 | 2035.032564 | 4236.087 | 8.372590321 | 1840.4565 |
| constant | extrapolation | 26851200.39 | 5181.814392 | 5156.067 | 6050.087 | 0.5819020178 | 0.6216055687 |
| affine | fitting | 11681.87474 | 108.0827217 | 88.76177183 | 368.7702797 | 0.7084687228 | 184.3851399 |
| affine | extrapolation | 87455.13815 | 295.7281491 | 291.5825799 | 381.1238617 | 0.03278038492 | 0.03921430823 |
| cubic | fitting | 269.2973995 | 16.41028335 | 12.55498066 | 71.8190675 | 0.1134674846 | 35.90953375 |
| cubic | extrapolation | 6213.326737 | 78.82465818 | 74.72613341 | 127.5422823 | 0.008333238886 | 0.01312298408 |
| n_log_n | fitting | 315128.6132 | 561.3631741 | 475.5806585 | 1014.574451 | 0.1330798614 | 1 |
| n_log_n | extrapolation | 1280169.566 | 1131.445786 | 1130.09067 | 1227.086283 | 0.1279301402 | 0.1318258808 |
| fitted_log | fitting | 146.2944945 | 12.0952261 | 9.001343661 | 46.01672934 | 0.007075110027 | 1.432455877 |
| fitted_log | extrapolation | 354.6166325 | 18.83126742 | 14.92054814 | 43.36109602 | 0.001701352228 | 0.005043747356 |

## Rounding and sequence diagnostics

Nearest integer, halfway away from zero; signed int64 conversion is checked first.
Denominators include invalid/unrepresentable predictions. Duplicates are repeated
rounded occurrences after the first within the range. Adjacent pairs do not bridge
invalid values. No repair, snapping, oddness adjustment or sequence sorting occurs.

| Model | Range | Matches / all indices | Match rate | Invalid / out-of-int64 | Negative | Raw violations / assessable pairs | Rounded violations / assessable pairs | Duplicates |
|---|---|---|---:|---|---:|---|---|---:|
| seed-0 | fitting | 18 / 1000 | 0.018 | 0 / 0 | 0 | 0 / 999 | 0 / 999 | 0 |
| seed-0 | extrapolation | 0 / 200 | 0 | 0 / 0 | 0 | 0 / 199 | 0 / 199 | 0 |
| seed-1 | fitting | 4 / 1000 | 0.004 | 0 / 0 | 17 | 0 / 999 | 0 / 999 | 0 |
| seed-1 | extrapolation | 0 / 200 | 0 | 0 / 0 | 0 | 0 / 199 | 0 / 199 | 0 |
| seed-2 | fitting | 48 / 1000 | 0.048 | 0 / 0 | 0 | 0 / 999 | 0 / 999 | 0 |
| seed-2 | extrapolation | 6 / 200 | 0.03 | 0 / 0 | 0 | 0 / 199 | 0 / 199 | 0 |
| seed-3 | fitting | 8 / 1000 | 0.008 | 0 / 0 | 21 | 0 / 999 | 0 / 999 | 0 |
| seed-3 | extrapolation | 0 / 200 | 0 | 0 / 0 | 0 | 0 / 199 | 0 / 199 | 0 |
| seed-42 | fitting | 32 / 1000 | 0.032 | 0 / 0 | 0 | 0 / 999 | 0 / 999 | 0 |
| seed-42 | extrapolation | 0 / 200 | 0 | 0 / 0 | 0 | 0 / 199 | 0 / 199 | 0 |
| constant | fitting | 0 / 1000 | 0 | 0 / 0 | 0 | 999 / 999 | 999 / 999 | 999 |
| constant | extrapolation | 0 / 200 | 0 | 0 / 0 | 0 | 199 / 199 | 199 / 199 | 199 |
| affine | fitting | 1 / 1000 | 0.001 | 0 / 0 | 46 | 0 / 999 | 0 / 999 | 0 |
| affine | extrapolation | 0 / 200 | 0 | 0 / 0 | 0 | 0 / 199 | 0 / 199 | 0 |
| cubic | fitting | 28 / 1000 | 0.028 | 0 / 0 | 12 | 0 / 999 | 0 / 999 | 0 |
| cubic | extrapolation | 0 / 200 | 0 | 0 / 0 | 0 | 0 / 199 | 0 / 199 | 0 |
| n_log_n | fitting | 0 / 1000 | 0 | 0 / 0 | 0 | 0 / 999 | 0 / 999 | 0 |
| n_log_n | extrapolation | 0 / 200 | 0 | 0 / 0 | 0 | 0 / 199 | 0 / 199 | 0 |
| fitted_log | fitting | 41 / 1000 | 0.041 | 0 / 0 | 1 | 0 / 999 | 0 / 999 | 0 |
| fitted_log | extrapolation | 8 / 200 | 0.04 | 0 / 0 | 0 | 0 / 199 | 0 / 199 | 0 |

## Baseline fits and interpretation

Four reference fits use example-local Householder QR on 1,000 rows each, independent
of GP work. Coefficients are binary64; long-double QR working arithmetic is qualified
on this Jetson. The constant predictor is the fitting mean. Coefficient order is
[constant, u] for affine, [constant, u, u^2, u^3] for cubic, and [u*log(u), u, constant]
for the fitted log family. All coefficients, evaluated grouping and concrete
candidate-limit representability are in [baselines.txt](prime-results/baselines.txt).

```text
constant coefficients=0.36829129999999999, normalized_expression=0.36829129999999999 representable=1 nodes=1 depth=1
affine coefficients=-0.037487775375375378,0.81074740334740336, normalized_expression=((-0.037487775375375378) + (0.81074740334740336 * x0)) representable=1 nodes=5 depth=3
cubic coefficients=-0.0075744053125561314,0.59216197573305029,0.33671788501933692,-0.13141333635570568, normalized_expression=(((-0.0075744053125561314) + (0.59216197573305029 * x0)) + ((0.33671788501933692 * (x0 * x0)) + ((-0.13141333635570568) * ((x0 * x0) * x0)))) representable=1 nodes=19 depth=6
n_log_n coefficients= actual_raw_form=n*std::log(n) fitted_parameters=0 normalized_family_representable=1_algebraically_not_bitwise g=(0.1*x0)*(log(x0)+log(1000))
fitted_log coefficients=0.11186729397186387,0.79193547658527197,-0.00010567476144389897, normalized_expression=(((0.11186729397186387 * (x0 * log(x0))) + (0.79193547658527197 * x0)) + (-0.00010567476144389897)) representable=1 nodes=12 depth=6
```

The evolved representative nearly reaches the fitted logarithmic reference's
fitting MSE (146.36377 versus 146.29449 raw units); evolution provides no demonstrated
accuracy advantage over that simple separately fitted reference. Its extrapolation
RMSE is 18.7845 versus the reference's 18.8313, a small observed difference rather
than evidence for choosing or tuning by that range. Cubic fitting RMSE is 16.4103,
affine 108.0827, constant 2342.9193 and n*log(n) 561.3632 (full measured values above).

All five results are retained, including the substantially poorer seeds. Search
publishes only 8–12 generations before fitting consumes the common allowance, and
~99.8–99.9% of the maximum row work actually occurs. Local refinement repeatedly
revisits elites/clones, while the coordinate method can stall on coupled constants.
Invalid losses are uncommon relative to 100,000 calls, and variation reports actual
changes with no fallbacks; cheap invalidity/rejection is not the main observed
budget sink. These are concrete diagnostics, not proof of a particular optimizer
fix. Seed 2 independently finds a log-bearing structure without a supplied formula.
Weak small-index fits and only 48/1000 rounded matches remain; no general prime
formula or exact reconstruction is claimed. Finite negative small-index predictions
of some models and baselines remain visible.

No extrapolation-based adjustment was made. The 1001–1200 range is now examined;
future tuning informed by these observations must identify it as such and reserve
another later assessment range if claiming fresh extrapolation evidence.

## Artifacts and accounting

[Complete raw log](prime-results/campaign.txt), [frozen models](prime-results/frozen-models.txt),
[representative's full fitting prefix](prime-results/representative-first-1000.csv),
[seed-2 predictions](prime-results/seed-2-predictions.csv) and all other seed/baseline
CSV tables are under prime-results. Per-seed trace CSVs retain only committed
best-seen changes against cumulative actual losses. Each of ten tables has exactly
1,200 positions; the representative prefix has exactly 1,000. [Residual plot](prime-results/residuals.png)
is generated offline with examples/plot_prime_residuals.py (matplotlib 3.6.3), not a
C++ runtime dependency. SHA256SUMS covers data, logs and tables.

Main campaign: 500,000 actual loss attempts, 499,319,422 actual row attempts, 40.154968 aggregate search seconds.
Pilot: 4,096 calls / 4,079,017 rows, separate from search. File reproduction:
100,000 calls / 99,804,196 rows / 8.506592331 seconds, plus 1,000 post-search
prediction rows. Main post-search reporting: 12,000 prediction rows (five GP models,
five baselines), separate from training; all four QR design matrices total 4,000
rows and do not invoke the search objective. Reference generation and baseline
fitting are outside the search timers; no row upper bound is mislabeled as measured
work. Raw-unit metric aggregations use already stored predictions, without another
expression evaluation or search loss call.

Seed 0 file reproduction matches the faithful expression, both initial losses,
final normalized score, discovery, stopping reason and every printed deterministic
work/fitting/variation counter; elapsed time is excluded. Evidence:
[file log](prime-results/file-seed-0.txt), [comparison](prime-results/reproduction-check.txt).
The parsed/in-memory engineering fixture also compares all TrainingWork fields and
variation counters and exact binary64 rows on a short same-seed run.

## Engineering verification

All 16 CTest cases pass in Release, Debug and ASan+UBSan. Full-suite commands and
warning command are recorded in [BASELINE.md](BASELINE.md). Includes the strict FP
negative-compilation tests, every standalone public header, the new application
header, a public-only downstream consumer, established fitter/publication/failure
contracts, short integrated normalized-data refinement, parser/in-memory repetition,
and bounded prime/generic-file workflows. No stochastic approximation score is a
test assertion. No sanitizer finding or unresolved engineering blocker.

The established warning command (`-Wall -Wextra -Wpedantic`, strict FP) completes
without diagnostics; whitespace checks pass. The character example retains .892
best-seen fitness, 1,000 generations and 100,100 outer evaluations. The accepted
strict-arithmetic five-seed report and raw artifact are byte-for-byte unchanged.
Earlier long campaigns were not repeated: no library or existing default search
policy changed. New generic --loss-threshold is additive and defaults to 1e-14;
no file numerical-validation rule changed.

Engineering acceptance does not imply prime recovery. The next concrete task should
profile CPU evaluation/refinement and then test a bounded fitting schedule/cache
policy that permits more structure exploration while preserving exact expression/
problem/score identity, actual-work limits and publication guarantees. Production
packaging, documentation/qualification and checkpointing remain on the roadmap;
GPU acceleration is not justified before profiling.

Final full-suite observations: Release 16/16 (2.03 s), Debug 16/16 (2.52 s),
ASan+UBSan 16/16 (136.76 s); times vary with host load. The final pilot diagnostic
labels completed fits separately from invocations (a global-cap partial fit is not
a completed fit); this reporting correction changes no search draws or numerical
work. Bounded prime-file smoke was rechecked after that label-only change.
All ten 1,200-row exports and the 1,000-row representative prefix were independently
scanned: sequential raw MSE and exact positional match counts reproduce the log.
SHA-256 verification succeeds for all recorded data, traces, tables, logs and plot.
