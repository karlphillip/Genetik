# D5 bounded mathematical recovery results

The protocols were frozen before the corresponding runs. All seed outcomes are retained. The primary mathematical benchmark source is `c1fbb28d67d72cf32259100ba05e4ab931722fb6`; the original24 pre-repair records are separate, not replaced. No production default changes follow from these small examples.

## Iterator production study

| Target | Rules | Seed | Training matches | Held-out matches | Generated dependent | Publications with refs /41 | Invalid losses | Operations | Iterations |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| squares | baseline | 0 | 2/9 | 0/4 | 79 | 36 | 88 | 18739 | 3969 |
| squares | dependent | 0 | 2/9 | 0/4 | 251 | 41 | 86 | 15627 | 2575 |
| squares | baseline | 1 | 2/9 | 0/4 | 97 | 34 | 91 | 20060 | 4175 |
| squares | dependent | 1 | 2/9 | 0/4 | 228 | 40 | 89 | 15078 | 2940 |
| squares | baseline | 42 | 2/9 | 0/4 | 92 | 36 | 107 | 16352 | 2322 |
| squares | dependent | 42 | 2/9 | 0/4 | 271 | 41 | 71 | 19914 | 3913 |
| divisors | baseline | 0 | 6/16 | 5/16 | 89 | 33 | 64 | 41342 | 9352 |
| divisors | dependent | 0 | 6/16 | 5/16 | 270 | 40 | 53 | 45299 | 8111 |
| divisors | baseline | 1 | 7/16 | 5/16 | 103 | 34 | 56 | 76225 | 13700 |
| divisors | dependent | 1 | 7/16 | 5/16 | 426 | 41 | 74 | 82191 | 16332 |
| divisors | baseline | 42 | 6/16 | 5/16 | 89 | 34 | 50 | 46416 | 9177 |
| divisors | dependent | 42 | 7/16 | 5/16 | 582 | 40 | 52 | 67344 | 10251 |

All12 runs reached generation40/1312 attempts. Every selected best was iterator-free; no best-fitness improvement was associated with an iterator-bearing best. The baseline already generated dependent structures and retained some in most committed populations. The opt-in rules increased generated dependencies (median90.5 to270.5 across these six paired cases) and publication survival, without recovering either complete target. This separates production/retention from useful fitness progress; it does not identify a unique failure cause. Square-sum selected `x0` in every run (2/9 matches). Divisor selected formulas and every counter are in the JSON records. No resource-exhaustion stop occurred. No target expression was injected.

Replays at the repaired committed source reproduced all12 scientific results and counters exactly; elapsed times are not equality assertions. Activity counters do not prove mutation causation: selection, crossover and rejection also participate.

## Fixed mathematical benchmarks

Seeds0,1,42; workers2; all runs sequential. Integer/recurrence attempts<=984, scalar full-objective attempts<=2048. Scalar fitting uses an explicit local64-attempt cap for these small demonstrations; the library default256 is unchanged. Assessment recovery uses exact integers or maximum scalar absolute error<=1e-6 on the entire fixed held-out set. Initial/fitted-initial discoveries are labelled by best generation0.

| Task | Seed | Training loss | Held-out matches | Held max abs error | Best gen | Final gen | Attempts | Runtime s | Stop |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| gcd-lcm | 0 | 0 | 9/9 | integer | 6 | 6 | 168 | 0.0571 | exact-fit |
| gcd-lcm | 1 | 0 | 9/9 | integer | 2 | 2 | 72 | 0.0552 | exact-fit |
| gcd-lcm | 42 | 0 | 9/9 | integer | 23 | 23 | 576 | 0.0661 | exact-fit |
| fibonacci | 0 | 0 | 6/6 | integer | 7 | 7 | 192 | 0.0882 | exact-fit |
| fibonacci | 1 | 0 | 6/6 | integer | 9 | 9 | 240 | 0.0859 | exact-fit |
| fibonacci | 42 | 0 | 6/6 | integer | 12 | 12 | 312 | 0.0926 | exact-fit |
| powers | 0 | 0.02536153936 | 0/5 | 0.3981834977667731 | 0 | 0 | 2048 | 0.0610 | loss-budget |
| powers | 1 | 0.0060498409 | 0/5 | 1.6931163396446394 | 0 | 0 | 2048 | 0.0631 | loss-budget |
| powers | 42 | 0.05784504339 | 0/5 | 3.555271861619019 | 0 | 0 | 2048 | 0.0686 | loss-budget |
| trig | 0 | 0.07992119204 | 0/6 | 1.43636870995624 | 0 | 0 | 2048 | 0.0688 | loss-budget |
| trig | 1 | 0.07992119226 | 0/6 | 1.4363371940003151 | 1 | 1 | 2048 | 0.0648 | loss-budget |
| trig | 42 | 0.006084219193 | 0/6 | 0.26830695809606764 | 1 | 1 | 2045 | 0.0635 | evaluation_budget |
| hyperbolic | 0 | 0 | 6/6 | 0 | 0 | 0 | 993 | 0.0539 | exact_training_fit |
| hyperbolic | 1 | 0 | 6/6 | 0 | 0 | 0 | 723 | 0.0532 | exact_training_fit |
| hyperbolic | 42 | 0 | 6/6 | 0 | 0 | 0 | 836 | 0.0627 | exact_training_fit |
| polynomial | 0 | 1.647634414 | 0/6 | 43.012332916259766 | 0 | 0 | 2048 | 0.0687 | loss-budget |
| polynomial | 1 | 0 | 6/6 | 0 | 0 | 0 | 1014 | 0.0612 | exact_training_fit |
| polynomial | 42 | 0 | 6/6 | 0 | 0 | 0 | 976 | 0.0597 | exact_training_fit |
| energy | 0 | 0 | 6/6 | 0 | 0 | 0 | 311 | 0.6529 | exact_training_fit |
| energy | 1 | 0 | 6/6 | 0 | 0 | 0 | 428 | 0.6708 | exact_training_fit |
| energy | 42 | 0 | 6/6 | 0 | 0 | 0 | 247 | 0.6967 | exact_training_fit |
| piecewise | 0 | 0 | 6/6 | 0 | 0 | 0 | 1069 | 0.0597 | exact_training_fit |
| piecewise | 1 | 0 | 6/6 | 0 | 0 | 0 | 1023 | 0.0594 | exact_training_fit |
| piecewise | 42 | 0 | 6/6 | 0 | 0 | 0 | 964 | 0.0632 | exact_training_fit |

The direct mathematical reference constructions all had zero training loss, independently of evolution. GCD/LCM targets use positive pairs with target a*b; Fibonacci evolves a transition with fixed order2 and initial0,1. Neither is a prime-discovery claim. Hyperbolic, piecewise and some polynomial successes were initialization plus ordinary coordinate-fitting discoveries, not later-generation achievements. Powers and trigonometry did not recover the frozen held-out target within this small budget. Some scalar runs exhausted their budget during a private generation, retaining the earlier committed population and counting all consumed probes.

### Actual selected expressions

**gcd-lcm, seed0**

`(x1 * x0)`

(x1 · x0)

Nodes3, depth2; rows2688; fitting attemptsnot applicable. Training operations5247, iterations0.

**gcd-lcm, seed1**

`gcd(lcm(x0, 0), (0 - (x1 * x0)))`

gcd(lcm(x0, 0), (0 - (x1 · x0)))

Nodes9, depth4; rows1152; fitting attemptsnot applicable. Training operations4883, iterations0.

**gcd-lcm, seed42**

`lcm(x0, (lcm(x0, x0) * (x1 + 0)))`

lcm(x0, (lcm(x0, x0) · (x1 + 0)))

Nodes9, depth4; rows9216; fitting attemptsnot applicable. Training operations45882, iterations0.

**fibonacci, seed0**

`(x1 + x2)`

(x1 + x2)

Nodes3, depth2; rows2102; fitting attemptsnot applicable. Training operations22248, iterations8559.

**fibonacci, seed1**

`((x2 * 1) - (0 - x1))`

((x2 · 1) - (0 - x1))

Nodes7, depth3; rows2640; fitting attemptsnot applicable. Training operations35325, iterations10800.

**fibonacci, seed42**

`(x2 + x1)`

(x2 + x1)

Nodes3, depth2; rows3426; fitting attemptsnot applicable. Training operations36731, iterations13989.

**powers, seed0**

`((1.0078125 * (x0 * 1.0)) * (x0 - ((-1.625) - x0)))`

1.0078125·(x0·1)·(x0 − (−1.625 − x0))

Nodes11, depth4; rows10200; fitting attempts2048. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**powers, seed1**

`(((1.46875 + x0) + 1.0) * pow(pow(x0, 0.5), 2.71875))`

(1.46875 + x0 + 1)·((x0)^(0.5))^(2.71875)

Nodes11, depth4; rows10231; fitting attempts2048. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**powers, seed42**

`pow((x0 + 0.6142578125), 2.5419921875)`

(x0 + 0.6142578125)^(2.5419921875)

Nodes5, depth3; rows10228; fitting attempts2048. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**trig, seed0**

`(0.77802564203739166 * x0)`

0.77802564203739166·x0

Nodes3, depth2; rows10240; fitting attempts2048. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**trig, seed1**

`((0.77801513671875 * x0) - ((0.5 + 0.19456037878990173) * (x0 - x0)))`

0.77801513671875·x0 − (0.5 + 0.19456037878990173)·(x0 − x0)

Nodes11, depth4; rows10240; fitting attempts2048. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**trig, seed42**

`(((3.9375 - x0) * (x0 * 0.0625)) + (sin(x0) - (0.6875 + (-1.0))))`

(3.9375 − x0)·(x0·0.0625) + (sin(x0) − (0.6875 + (−1)))

Nodes14, depth4; rows10225; fitting attempts2045. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**hyperbolic, seed0**

`tanh(x0)`

tanh(x0)

Nodes2, depth2; rows4965; fitting attempts993. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**hyperbolic, seed1**

`tanh(x0)`

tanh(x0)

Nodes2, depth2; rows3615; fitting attempts723. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**hyperbolic, seed42**

`tanh((1.0 * x0))`

tanh(1·x0)

Nodes4, depth3; rows4180; fitting attempts836. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**polynomial, seed0**

`(((x0 + 1.984375) * (x0 + (-1.984375))) * ((x0 * x0) + 0.484375))`

(x0 + 1.984375)·(x0 + (−1.984375))·(x0·x0 + 0.484375)

Nodes13, depth4; rows10240; fitting attempts2048. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**polynomial, seed1**

`(((-1.0) - (1.0 - x0)) + ((x0 - 2.0) * (x0 + 1.0)))`

−1 − (1 − x0) + (x0 − 2)·(x0 + 1)

Nodes13, depth4; rows5070; fitting attempts1014. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**polynomial, seed42**

`((1.0 - (x0 - x0)) * ((x0 * x0) + ((-1.0) + (-3.0))))`

(1 − (x0 − x0))·(x0·x0 + (−1 + (−3)))

Nodes13, depth4; rows4880; fitting attempts976. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**energy, seed0**

`((x0 * (x1 * x1)) * 0.5)`

x0·(x1·x1)·0.5

Nodes7, depth4; rows2799; fitting attempts311. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**energy, seed1**

`((x0 * (x1 * x1)) * 0.5)`

x0·(x1·x1)·0.5

Nodes7, depth4; rows3852; fitting attempts428. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**energy, seed42**

`((x0 * (x1 * x1)) * 0.5)`

x0·(x1·x1)·0.5

Nodes7, depth4; rows2223; fitting attempts247. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**piecewise, seed0**

`max(0.0, x0)`

max(0, x0)

Nodes3, depth2; rows5345; fitting attempts1069. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**piecewise, seed1**

`max(0.0, x0)`

max(0, x0)

Nodes3, depth2; rows5115; fitting attempts1023. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**piecewise, seed42**

`max(0.0, x0)`

max(0, x0)

Nodes3, depth2; rows4820; fitting attempts964. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

## Retained development failures and interpretation

The first24 benchmark runs are retained under PRE_REPAIR*. The dimensional generator mistakenly used the production-storage cap to truncate operator scanning. With the fixed Add/Subtract/Multiply ordering this could suppress later multiplication/coefficient productions. A structural regression fixture failed before the repair and passed afterward. Work and storage admissions are now separate; no dataset, seeds, operators, fitting or objective budgets were changed. The corrected24 runs changed only the three dimensional outcomes among the reported selection/recovery fields. The original energy failures remain evidence of an engineering defect, not a negative scientific finding about dimensional discovery.

An unrelated discrete interpreter compatibility audit removed an unnecessary new frame field by reusing an existing integer slot for the third modular-power operand. Legacy frame storage requirements are preserved. The original iterator campaign was reproduced after that repair with identical outcomes/counters.

The dimensional planner is a strong structural prior: it derives a finite unit-correct witness without observations. Recovering E=.5*m*v² in that constrained space does not demonstrate unconstrained physical-law discovery. Training and held-out agreement are finite evidence, not universal identification. The earlier prime experiments remain unchanged and no new prime campaign was run.

## Independent reference provenance

3652 integer fixtures use Python arbitrary integers, math.gcd/lcm/comb/factorial and independently calculated Euclidean quotients. Seventy scientific value/partial fixtures use mpmath1.2.1 at100 decimal digits with exact Fraction.from_float binary64 identities. Production helpers are not the oracle. Scalar tolerances1e-14 absolute plus1e-13 relative cover the declared compound native operations on these inputs and are not a general error bound. The regeneration scripts are under scripts/references/.

## Limits and next boundary

These are small frozen engineering/recovery demonstrations, not optimizer or GA superiority comparisons. Iterator-dependent production improved representation coverage without solving the chosen iteration datasets. Grammar discovery, computational cost and sample identifiability remain separate issues. Optional arbitrary precision is incomplete: Boost headers were unavailable and no production bigint backend/codec was qualified. Recurrence order and initial conditions are fixed model context; transition trees evolve. Symbolic factorization and rational coefficient ranges are bounded subsets. No D6 work is begun.

## Valid iterator survival audit

A diagnostic-only replay at `80add5791f563b9b1244deea6d96830644f04f70` reproduced every original scientific field and work counter while distinguishing valid iterator-reference members from invalid ones. It did not add objective evaluations, fitting or RNG draws. All41 snapshots per run are retained.

| Target | Rules | Seed | Snapshots with valid refs /41 | Maximum valid ref members /32 | Best valid-ref loss |
|---|---|---:|---:|---:|---:|
| squares | baseline | 0 | 29 | 4 | 7.0 |
| squares | dependent | 0 | 41 | 11 | 7.0 |
| squares | baseline | 1 | 33 | 6 | 7.0 |
| squares | dependent | 1 | 40 | 8 | 7.0 |
| squares | baseline | 42 | 34 | 5 | 7.0 |
| squares | dependent | 42 | 41 | 12 | 7.0 |
| divisors | baseline | 0 | 32 | 5 | 10.0 |
| divisors | dependent | 0 | 40 | 10 | 10.0 |
| divisors | baseline | 1 | 33 | 5 | 10.0 |
| divisors | dependent | 1 | 41 | 16 | 9.0 |
| divisors | baseline | 42 | 33 | 6 | 10.0 |
| divisors | dependent | 42 | 40 | 28 | 9.0 |

The iterator-dependent structures were not merely generated and immediately invalidated: valid reference-bearing members survived publication in29–34 baseline snapshots and40–41 opt-in snapshots. Some tied the best loss, but the archive selected simpler plain expressions under the existing tie policy. Presence of an iterator reference does not prove that it contributes to outputs; neither these counts nor the paired outcomes prove a causal explanation for stagnation.
