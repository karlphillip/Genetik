# Complete fitting-cadence comparison

All runs use the predeclared settings. Interval 1 is the default/control.
No model was chosen using evaluation metrics.

| Problem | Seed | Interval | Raw initial MSE | Refined initial MSE | Final training MSE | Nodes/depth | Discovery | Published | Seconds | Stop | Unused |
|---|---:|---:|---:|---:|---:|---|---|---:|---:|---|---:|
| arithmetic | 0 | 1 | 2.4726090417546036 | 1.8909068540574137 | 7.5564558307909507e-16 | 10/4 | offspring 10 | 10 | 0.243544 | training_loss_threshold | 31155 |
| arithmetic | 0 | 4 | 2.4726090417546036 | 1.8909068540574137 | 0.11961059222554642 | 19/6 | offspring 40 | 43 | 0.422190 | evaluation_budget | 0 |
| arithmetic | 1 | 1 | 2.4726090417546036 | 1.890906027836402 | 0.64027194471888438 | 15/6 | offspring 11 | 11 | 0.366791 | evaluation_budget | 0 |
| arithmetic | 1 | 4 | 2.4726090417546036 | 1.890906027836402 | 0.028609476331181768 | 28/6 | offspring 36 | 39 | 0.476304 | evaluation_budget | 0 |
| arithmetic | 2 | 1 | 2.1312500000000001 | 2.0611831791271022 | 1.3197121232313256e-15 | 10/5 | offspring 9 | 9 | 0.204048 | training_loss_threshold | 39956 |
| arithmetic | 2 | 4 | 2.1312500000000001 | 2.0611831791271022 | 0 | 8/4 | offspring 33 | 33 | 0.102345 | training_loss_threshold | 67616 |
| arithmetic | 3 | 1 | 2.4726090417546036 | 1.8909060278364236 | 0.04839553743457721 | 13/5 | offspring 9 | 9 | 0.363687 | evaluation_budget | 0 |
| arithmetic | 3 | 4 | 2.4726090417546036 | 1.8909060278364236 | 0.12457992663854668 | 20/6 | offspring 33 | 35 | 0.471767 | evaluation_budget | 0 |
| arithmetic | 42 | 1 | 1.9073039749738052 | 1.2076509306233403 | 5.3110430571409987e-16 | 10/4 | offspring 8 | 8 | 0.172293 | training_loss_threshold | 48376 |
| arithmetic | 42 | 4 | 1.9073039749738052 | 1.2076509306233403 | 0 | 10/5 | offspring 24 | 24 | 0.126494 | training_loss_threshold | 62897 |
| prime | 0 | 1 | 0.020580668229999998 | 2.6425241448526163e-05 | 5.3601967624741316e-06 | 24/6 | offspring 8 | 8 | 8.681790 | evaluation_budget | 0 |
| prime | 0 | 4 | 0.020580668229999998 | 2.6425241448526163e-05 | 2.4190884250155986e-06 | 21/6 | offspring 32 | 35 | 9.352612 | evaluation_budget | 0 |
| prime | 1 | 1 | 0.0083271314398999929 | 0.00011684692849506492 | 1.0000040243893693e-05 | 13/5 | offspring 9 | 9 | 8.531940 | evaluation_budget | 0 |
| prime | 1 | 4 | 0.0083271314398999929 | 0.00011684692849506492 | 1.1216120500313497e-05 | 12/5 | offspring 48 | 51 | 7.596993 | evaluation_budget | 0 |
| prime | 2 | 1 | 0.0016721220378997103 | 0.00011681874737724468 | 1.4636376958468287e-06 | 16/6 | offspring 10 | 11 | 7.986783 | evaluation_budget | 0 |
| prime | 2 | 4 | 0.0016721220378997103 | 0.00011681874737724468 | 2.1250709832216529e-06 | 12/5 | offspring 48 | 51 | 8.263205 | evaluation_budget | 0 |
| prime | 3 | 1 | 0.0083271314398999929 | 0.00046762533640472888 | 8.8640860262019499e-06 | 11/5 | offspring 12 | 12 | 6.637018 | evaluation_budget | 0 |
| prime | 3 | 4 | 0.0083271314398999929 | 0.00046762533640472888 | 8.8605689663577789e-06 | 15/5 | offspring 36 | 39 | 7.461249 | evaluation_budget | 0 |
| prime | 42 | 1 | 0.0083271314398999929 | 0.00046762533640472926 | 3.0452604371448425e-06 | 23/6 | offspring 10 | 10 | 8.705675 | evaluation_budget | 0 |
| prime | 42 | 4 | 0.0083271314398999929 | 0.00046762533640472926 | 2.1966362114825966e-06 | 17/6 | offspring 48 | 51 | 8.955991 | evaluation_budget | 0 |

| Problem | Seed | Interval | Actual losses | Ordinary | Fit baselines | Fit probes | Fits | Fits with accepted steps | Policy skips | Unpublished losses | Actual rows | Initial losses |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| arithmetic | 0 | 1 | 68845 | 0 | 704 | 68141 | 704 | 381 | 0 | 0 | 1716988 | 3006 |
| arithmetic | 0 | 4 | 100000 | 2112 | 706 | 97182 | 706 | 528 | 2112 | 340 | 2491102 | 3006 |
| arithmetic | 1 | 1 | 100000 | 0 | 813 | 99187 | 813 | 496 | 0 | 8251 | 2496320 | 2561 |
| arithmetic | 1 | 4 | 100000 | 1920 | 661 | 97419 | 661 | 485 | 1920 | 4971 | 2491945 | 2561 |
| arithmetic | 2 | 1 | 60044 | 0 | 640 | 59404 | 640 | 353 | 0 | 0 | 1495096 | 2562 |
| arithmetic | 2 | 4 | 32384 | 1600 | 576 | 30208 | 576 | 258 | 1600 | 0 | 801514 | 2562 |
| arithmetic | 3 | 1 | 100000 | 0 | 681 | 99319 | 681 | 498 | 0 | 7869 | 2498067 | 3177 |
| arithmetic | 3 | 4 | 100000 | 1728 | 636 | 97636 | 636 | 503 | 1728 | 11994 | 2489258 | 3177 |
| arithmetic | 42 | 1 | 51624 | 0 | 576 | 51048 | 576 | 326 | 0 | 0 | 1287392 | 2707 |
| arithmetic | 42 | 4 | 37103 | 1152 | 448 | 35503 | 448 | 266 | 1152 | 0 | 921794 | 2707 |
| prime | 0 | 1 | 100000 | 0 | 628 | 99372 | 628 | 482 | 0 | 10649 | 99804196 | 4163 |
| prime | 0 | 4 | 100000 | 1728 | 606 | 97666 | 606 | 479 | 1728 | 5635 | 99866134 | 4163 |
| prime | 1 | 1 | 100000 | 0 | 695 | 99305 | 695 | 496 | 0 | 12873 | 99883485 | 3302 |
| prime | 1 | 4 | 100000 | 2496 | 852 | 96652 | 852 | 585 | 2496 | 3801 | 99247819 | 3302 |
| prime | 2 | 1 | 100000 | 0 | 803 | 99197 | 803 | 540 | 0 | 5967 | 99864635 | 3068 |
| prime | 2 | 4 | 100000 | 2496 | 876 | 96628 | 876 | 608 | 2496 | 7333 | 99442314 | 3068 |
| prime | 3 | 1 | 100000 | 0 | 833 | 99167 | 833 | 574 | 0 | 143 | 99831169 | 3572 |
| prime | 3 | 4 | 100000 | 1920 | 701 | 97379 | 701 | 537 | 1920 | 13056 | 99784348 | 3572 |
| prime | 42 | 1 | 100000 | 0 | 741 | 99259 | 741 | 493 | 0 | 7717 | 99935937 | 3035 |
| prime | 42 | 4 | 100000 | 2496 | 884 | 96620 | 884 | 604 | 2496 | 8493 | 99410639 | 3035 |

## Paired and distribution summaries

Entries are median [minimum, maximum] across the declared five seeds. Timing ratios are paired experimental/control; a ratio above one is slower.

prime: fitting-MSE wins/losses/ties **3/2/0**; paired runtime ratio **1.0346099 [0.89041806, 1.124187]**.

| Metric | Interval 1 | Interval 4 |
|---|---|---|
| training_mse | 5.3601968e-06 [1.4636377e-06, 1.000004e-05] | 2.4190884e-06 [2.125071e-06, 1.1216121e-05] |
| published_generations | 10 [8, 12] | 51 [35, 51] |
| training_loss_attempts | 100000 [100000, 100000] | 100000 [100000, 100000] |
| fits | 741 [628, 833] | 852 [606, 884] |
| schedule_skips | 0 [0, 0] | 2496 [1728, 2496] |
| unpublished_losses | 7717 [143, 12873] | 7333 [3801, 13056] |
| row_attempts | 99864635 [99804196, 99935937] | 99442314 [99247819, 99866134] |
| seconds | 8.5319398 [6.6370176, 8.7056748] | 8.2632048 [7.4612487, 9.3526118] |

Aggregate search seconds: 40.543205 / 41.630050.

arithmetic: fitting-MSE wins/losses/ties **3/2/0**; paired runtime ratio **1.2971779 [0.5015712, 1.7335248]**.

| Metric | Interval 1 | Interval 4 |
|---|---|---|
| training_mse | 1.3197121e-15 [5.3110431e-16, 0.64027194] | 0.028609476 [0, 0.12457993] |
| published_generations | 9 [8, 11] | 35 [24, 43] |
| training_loss_attempts | 68845 [51624, 100000] | 100000 [32384, 100000] |
| fits | 681 [576, 813] | 636 [448, 706] |
| schedule_skips | 0 [0, 0] | 1728 [1152, 2112] |
| unpublished_losses | 0 [0, 8251] | 340 [0, 11994] |
| row_attempts | 1716988 [1287392, 2498067] | 2489258 [801514, 2491945] |
| seconds | 0.24354449 [0.17229291, 0.36679141] | 0.42219041 [0.10234453, 0.47630439] |

Aggregate search seconds: 1.350364 / 1.599101.

## Prime raw-unit assessment (1001–1200 previously examined)

| Model | Range | RMSE | Max absolute | Matches / total | Invalid / unrepresentable | Negatives | Raw violations/pairs | Rounded violations/pairs | Duplicates |
|---|---|---:|---:|---|---|---:|---|---|---:|
| prime-0-1 | fitting | 23.15209874 | 63.67593578 | 18/1000 | 0/0 | 0 | 0/999 | 0/999 | 0 |
| prime-0-1 | examined_extrapolation | 399.248342 | 831.006547 | 0/200 | 0/0 | 0 | 0/199 | 0/199 | 0 |
| prime-0-4 | fitting | 15.553419 | 55.17092961 | 23/1000 | 0/0 | 0 | 0/999 | 0/999 | 0 |
| prime-0-4 | examined_extrapolation | 52.0727301 | 78.85321527 | 0/200 | 0/0 | 0 | 0/199 | 0/199 | 0 |
| prime-1-4 | fitting | 33.49047701 | 153.4569274 | 7/1000 | 0/0 | 22 | 0/999 | 0/999 | 0 |
| prime-1-4 | examined_extrapolation | 137.6856847 | 240.6341431 | 0/200 | 0/0 | 0 | 0/199 | 0/199 | 0 |
| prime-1-1 | fitting | 31.62284023 | 110.305994 | 4/1000 | 0/0 | 17 | 0/999 | 0/999 | 0 |
| prime-1-1 | examined_extrapolation | 148.7517438 | 249.728594 | 0/200 | 0/0 | 0 | 0/199 | 0/199 | 0 |
| prime-2-1 | fitting | 12.0980895 | 45.83964744 | 48/1000 | 0/0 | 0 | 0/999 | 0/999 | 0 |
| prime-2-1 | examined_extrapolation | 18.78449767 | 42.98120954 | 6/200 | 0/0 | 0 | 0/199 | 0/199 | 0 |
| prime-2-4 | fitting | 14.57762321 | 46.34969923 | 32/1000 | 0/0 | 10 | 0/999 | 0/999 | 0 |
| prime-2-4 | examined_extrapolation | 34.42928837 | 81.22782718 | 3/200 | 0/0 | 0 | 0/199 | 0/199 | 0 |
| prime-3-4 | fitting | 29.76670786 | 137.0206715 | 10/1000 | 0/0 | 21 | 0/999 | 0/999 | 0 |
| prime-3-4 | examined_extrapolation | 111.2811092 | 198.5637056 | 0/200 | 0/0 | 0 | 0/199 | 0/199 | 0 |
| prime-3-1 | fitting | 29.77261498 | 138.5837981 | 8/1000 | 0/0 | 21 | 0/999 | 0/999 | 0 |
| prime-3-1 | examined_extrapolation | 109.0746534 | 195.5292661 | 0/200 | 0/0 | 0 | 0/199 | 0/199 | 0 |
| prime-42-1 | fitting | 17.45067459 | 67.28920109 | 32/1000 | 0/0 | 0 | 0/999 | 0/999 | 0 |
| prime-42-1 | examined_extrapolation | 75.18052906 | 100.3578287 | 0/200 | 0/0 | 0 | 0/199 | 0/199 | 0 |
| prime-42-4 | fitting | 14.82105331 | 62.01908206 | 28/1000 | 0/0 | 11 | 0/999 | 0/999 | 0 |
| prime-42-4 | examined_extrapolation | 21.66442168 | 50.14759954 | 1/200 | 0/0 | 0 | 0/199 | 0/199 | 0 |

## Strict-arithmetic assessment

| Model/range | Max absolute error | Recovery <=1e-6 | Invalid predictions/errors |
|---|---:|---:|---|
| arithmetic-0-1-training | 3.2353417056540934e-08 | 1 | 0/0 |
| arithmetic-0-1-interior | 3.0861122324310486e-08 | 1 | 0/0 |
| arithmetic-0-1-exterior | 3.4904511725386556e-08 | 1 | 0/0 |
| arithmetic-0-4-training | 0.81419644100719646 | 0 | 0/0 |
| arithmetic-0-4-interior | 0.69413276756113274 | 0 | 0/0 |
| arithmetic-0-4-exterior | 2.4551117987126738 | 0 | 0/0 |
| arithmetic-1-4-training | 0.34609477454200754 | 0 | 0/0 |
| arithmetic-1-4-interior | 0.2107490895755737 | 0 | 0/0 |
| arithmetic-1-4-exterior | 4.5300928367273272 | 0 | 0/0 |
| arithmetic-1-1-training | 2.4179848721598773 | 0 | 0/0 |
| arithmetic-1-1-interior | 1.1287100459063972 | 0 | 0/0 |
| arithmetic-1-1-exterior | 9.8937649500349476 | 0 | 0/0 |
| arithmetic-2-1-training | 9.9536507036646071e-08 | 1 | 0/0 |
| arithmetic-2-1-interior | 4.9768253518323036e-08 | 1 | 0/0 |
| arithmetic-2-1-exterior | 2.9860952110993821e-07 | 1 | 0/0 |
| arithmetic-2-4-training | 0 | 1 | 0/0 |
| arithmetic-2-4-interior | 0 | 1 | 0/0 |
| arithmetic-2-4-exterior | 0 | 1 | 0/0 |
| arithmetic-3-4-training | 0.69427789204005652 | 0 | 0/0 |
| arithmetic-3-4-interior | 0.41531765079984329 | 0 | 0/0 |
| arithmetic-3-4-exterior | 5.4927203397794759 | 0 | 0/0 |
| arithmetic-3-1-training | 0.3022626863808251 | 0 | 0/0 |
| arithmetic-3-1-interior | 0.33758344906089977 | 0 | 0/0 |
| arithmetic-3-1-exterior | 1.8977914962503575 | 0 | 0/0 |
| arithmetic-42-1-training | 7.0562792053863177e-08 | 1 | 0/0 |
| arithmetic-42-1-interior | 2.3520930536591322e-08 | 1 | 0/0 |
| arithmetic-42-1-exterior | 4.2337676120496326e-07 | 1 | 0/0 |
| arithmetic-42-4-training | 0 | 1 | 0/0 |
| arithmetic-42-4-interior | 0 | 1 | 0/0 |
| arithmetic-42-4-exterior | 0 | 1 | 0/0 |

prime assessment distributions (median [min,max]), distinct from training-based wins:

| Mode | Range | RMSE | Maximum absolute error |
|---|---|---|---|
| 1 | fitting | 23.152099 [12.09809, 31.62284] | 67.289201 [45.839647, 138.5838] |
| 1 | examined_extrapolation | 109.07465 [18.784498, 399.24834] | 195.52927 [42.98121, 831.00655] |
| 4 | fitting | 15.553419 [14.577623, 33.490477] | 62.019082 [46.349699, 153.45693] |
| 4 | examined_extrapolation | 52.07273 [21.664422, 137.68568] | 81.227827 [50.1476, 240.63414] |

arithmetic assessment distributions (median [min,max]), distinct from training-based wins:

| Mode | Range | RMSE | Maximum absolute error |
|---|---|---|---|
| 1 | training | not recomputed | 9.9536507e-08 [3.2353417e-08, 2.4179849] |
| 1 | interior | not recomputed | 4.9768254e-08 [2.3520931e-08, 1.12871] |
| 1 | exterior | not recomputed | 4.2337676e-07 [3.4904512e-08, 9.893765] |
| 4 | training | not recomputed | 0.34609477 [0, 0.81419644] |
| 4 | interior | not recomputed | 0.21074909 [0, 0.69413277] |
| 4 | exterior | not recomputed | 2.4551118 [0, 5.4927203] |

## Faithful frozen expressions

Prime trees are g(x0), x0=n/1000.0; output is 10000.0*g(x0), preserving grouping. Arithmetic input order is x0=first,x1=second.

arithmetic, seed 0, interval 1:

```text
(((x1 / x0) - (-2.9802322387695312e-08)) + (log(x0) * 0.9999999963195485))
```

arithmetic, seed 0, interval 4:

```text
(((((2.3758094310760498 + 1.28125) / x0) * (0.33593751490116119 + (x1 / (-1.2753367821386217)))) - (x0 + x1)) * ((-0.5) / 1.796875))
```

arithmetic, seed 1, interval 1:

```text
((x1 + 0.032454761435163348) - (((-3.3154540779453523) + x1) * (0.10101879714180839 * ((x0 + (-3.0295090737776365)) + 1.9566235244274139))))
```

arithmetic, seed 1, interval 4:

```text
((((x0 / 4.9350659796528564) - log(x0)) * (((-0.93022537231445312) + (x1 * 2.0234375)) - (0.69314718246459961 * x1))) + (0.21820616722106934 * (x0 + ((2.866642951965332 * x1) - ((-1.046875) * x1)))))
```

arithmetic, seed 2, interval 1:

```text
(log(x0) + (((4.1999999284744263 + (-3.199999953358553)) * x1) / x0))
```

arithmetic, seed 2, interval 4:

```text
(((x1 * 1.0) / x0) + log(x0))
```

arithmetic, seed 3, interval 1:

```text
(((0.3565363944009361 - x1) * ((1.5620880126953125 / x0) * (-0.64016857005124805))) + (x0 * 0.29329147934913635))
```

arithmetic, seed 3, interval 4:

```text
(1.7543306350708008 - (log((x0 * 0.171875)) * (((x1 * 0.69314718317137824) + (-0.98719704902484384)) - ((x1 * (-0.0062499940395355225)) * (x0 / (-4.017303466796875))))))
```

arithmetic, seed 42, interval 1:

```text
(log(x0) + ((x1 + 0.0) / (x0 + (-8.8203488957105947e-09))))
```

arithmetic, seed 42, interval 4:

```text
((0.0 + (log(x0) + 0.0)) + (x1 / x0))
```

prime, seed 0, interval 1:

```text
((x0 / (x0 - ((5.0069351196289062 + (-1.98480224609375)) * (x0 - 1.1793319410959722)))) * ((x0 - log(0.15600989721665748)) + (x0 * (((-2.6280710109623868) + x0) * x0))))
```

prime, seed 0, interval 4:

```text
(((7.999755859375 * (x0 + (x0 * (-0.84209047625945388)))) / (((x0 * x0) + (x0 + (-2.330572973028783))) + (x0 * (-5.681640625)))) + x0)
```

prime, seed 1, interval 1:

```text
(x0 - (((0.34853261709213257 - x0) - ((-0.84645298747583286) * x0)) * (x0 + 0.032958984375)))
```

prime, seed 1, interval 4:

```text
((log(0.97730334290363885) + x0) / (x0 - (((-1.1844110488891602) + x0) * 1.2242557146928128)))
```

prime, seed 2, interval 1:

```text
(x0 * (((((-3.0) / (-3.0)) * x0) * (0.79185613989830017 / x0)) + (0.1121049374658476 * log(x0))))
```

prime, seed 2, interval 4:

```text
(((0.032399281859397888 + ((-3.0251187235116959) * x0)) / ((-6.9464208769968456) + log(x0))) * 1.8410873413085938)
```

prime, seed 3, interval 1:

```text
(1.1105825901031494 * (((x0 * 0.12480717897415161) - (-0.607757568359375)) * (x0 + (-0.021231532096862793))))
```

prime, seed 3, interval 4:

```text
((x0 * (0.78535673022270203 - 0.037488355328683554)) - (((-0.14601972065988988) - x0) * ((0.69611284769948512 - x0) * (-0.13943827152252197))))
```

prime, seed 42, interval 1:

```text
((x0 / x0) / (((((-1.2226836979389191) + x0) - (x0 * 5.1611535400152206)) / x0) / (((-1.4906127750873566) + (x0 / x0)) - (x0 * 3.7531566172838211))))
```

prime, seed 42, interval 4:

```text
((log((log(2.6888970732688904) + (x0 * 0.95337569573865277))) * log((log(5.0944146513938904) + x0))) * (1.0 - (-0.23407804981361835)))
```
