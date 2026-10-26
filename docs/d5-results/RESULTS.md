# D5 bounded mathematical recovery results

The protocols were frozen before the corresponding runs. All seed outcomes are retained. The primary mathematical benchmark source is `cc398131e6480d3263574cedcf58f48e4f1954c8`; the original24 pre-repair and intermediate24 records are separate, not replaced. No production default changes follow from these small examples.

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
| gcd-lcm | 0 | 0 | 9/9 | integer | 6 | 6 | 168 | 0.0565 | exact-fit |
| gcd-lcm | 1 | 0 | 9/9 | integer | 2 | 2 | 72 | 0.0583 | exact-fit |
| gcd-lcm | 42 | 0 | 9/9 | integer | 23 | 23 | 576 | 0.0671 | exact-fit |
| fibonacci | 0 | 0 | 6/6 | integer | 7 | 7 | 192 | 0.0899 | exact-fit |
| fibonacci | 1 | 0 | 6/6 | integer | 9 | 9 | 240 | 0.0889 | exact-fit |
| fibonacci | 42 | 0 | 6/6 | integer | 12 | 12 | 312 | 0.0918 | exact-fit |
| powers | 0 | 0.02536153936 | 0/5 | 0.3981834977667731 | 0 | 0 | 2048 | 0.0621 | loss-budget |
| powers | 1 | 0.0060498409 | 0/5 | 1.6931163396446394 | 0 | 0 | 2048 | 0.0658 | loss-budget |
| powers | 42 | 0.05784504339 | 0/5 | 3.555271861619019 | 0 | 0 | 2048 | 0.0581 | loss-budget |
| trig | 0 | 0.07992119204 | 0/6 | 1.43636870995624 | 0 | 0 | 2048 | 0.0590 | loss-budget |
| trig | 1 | 0.07992119226 | 0/6 | 1.4363371940003151 | 1 | 1 | 2048 | 0.0638 | loss-budget |
| trig | 42 | 0.006084219193 | 0/6 | 0.26830695809606764 | 1 | 1 | 2045 | 0.0628 | evaluation_budget |
| hyperbolic | 0 | 0 | 6/6 | 0 | 0 | 0 | 993 | 0.0657 | exact_training_fit |
| hyperbolic | 1 | 0 | 6/6 | 0 | 0 | 0 | 723 | 0.0595 | exact_training_fit |
| hyperbolic | 42 | 0 | 6/6 | 0 | 0 | 0 | 836 | 0.0616 | exact_training_fit |
| polynomial | 0 | 1.647634414 | 0/6 | 43.012332916259766 | 0 | 0 | 2048 | 0.0615 | loss-budget |
| polynomial | 1 | 0 | 6/6 | 0 | 0 | 0 | 1014 | 0.0631 | exact_training_fit |
| polynomial | 42 | 0 | 6/6 | 0 | 0 | 0 | 976 | 0.0577 | exact_training_fit |
| energy | 0 | 0 | 6/6 | 0 | 0 | 0 | 311 | 0.6722 | exact_training_fit |
| energy | 1 | 0 | 6/6 | 0 | 0 | 0 | 374 | 0.6887 | exact_training_fit |
| energy | 42 | 0 | 6/6 | 0 | 0 | 0 | 248 | 0.7216 | exact_training_fit |
| piecewise | 0 | 0 | 6/6 | 0 | 0 | 0 | 1069 | 0.0653 | exact_training_fit |
| piecewise | 1 | 0 | 6/6 | 0 | 0 | 0 | 1023 | 0.0715 | exact_training_fit |
| piecewise | 42 | 0 | 6/6 | 0 | 0 | 0 | 964 | 0.0589 | exact_training_fit |

The direct mathematical reference constructions all had zero training loss, independently of evolution. GCD/LCM targets use positive pairs with target a*b; Fibonacci evolves a transition with fixed order2 and initial0,1. Neither is a prime-discovery claim. Hyperbolic, piecewise and some polynomial successes were initialization plus ordinary coordinate-fitting discoveries, not later-generation achievements. Powers and trigonometry did not recover the frozen held-out target within this small budget. Some scalar runs exhausted their budget during a private generation, retaining the earlier committed population and counting all consumed probes.

Integer/recurrence training loss is the exact mismatch count; scalar loss is
checked raw-unit MSE. Their magnitudes are not a cross-domain quality comparison.
The later ownership/resource repair at `5e44bcfaf83e908d0b79a5eb7b23031fffa00284`
detaches moved unit-policy storage and forwards the problem's inference policy.
The recorded campaigns used normal owned factory inputs and the same default
inference policy; no numerical rule, production rule, dataset, fitter or budget
changed. Their identified-source records remain valid and are retained rather
than replaced by retuned runs. Final full engineering qualification includes the
ownership regression and schema-resource boundaries.

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

Nodes7, depth4; rows3366; fitting attempts374. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

**energy, seed42**

`((x0 * (x1 * x1)) * 0.5)`

x0·(x1·x1)·0.5

Nodes7, depth4; rows2232; fitting attempts248. Scalar training exposes row/loss/probe counts; the conservative arithmetic ceiling is row_attempts*1000, not an actual measured operation total.

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

The first24 benchmark runs are retained under PRE_REPAIR*. The dimensional generator mistakenly used the production-storage cap to truncate operator scanning. With the fixed Add/Subtract/Multiply ordering this could suppress later multiplication/coefficient productions. A structural regression fixture failed before the repair and passed afterward. Work and storage admissions are now separate; no dataset, seeds, operators, fitting or objective budgets were changed. The first corrected24 runs changed only the three dimensional outcomes among the reported selection/recovery fields. A later terminal-coverage audit found that a single unit witness excluded other original variables sharing that unit and continuous coefficient sources. Destination composition limits also needed enforcement. Regression fixtures failed before their respective repairs and passed afterward. The final frozen replay retains the same operators, data, seeds and budgets; all non-dimensional selected outputs reproduce the intermediate runs, while two energy formulas change and all three still recover the held-out target. The qualified typed-production identity rejects earlier unqualified D5 dimensional-run contexts. All earlier records remain retained. The original energy failures remain evidence of an engineering defect, not a negative scientific finding about dimensional discovery.

An unrelated discrete interpreter compatibility audit removed an unnecessary new frame field by reusing an existing integer slot for the third modular-power operand. Legacy frame storage requirements are preserved. The original iterator campaign was reproduced after that repair with identical outcomes/counters.

The dimensional planner is a strong structural prior: it derives a finite unit-correct witness without observations. Recovering E=.5*m*v² in that constrained space does not demonstrate unconstrained physical-law discovery. Training and held-out agreement are finite evidence, not universal identification. The earlier prime experiments remain unchanged and no new prime campaign was run.

## Valid iterator survival audit

A diagnostic-only replay checks fitness validity as well as structural presence. For square sums, the baseline had valid iterator-reference candidates in29/33/34 of41 snapshots, versus41/40/41 with the dependent rules. For divisor counting the counts were32/33/33 versus40/41/40. Maximum valid reference-bearing populations were4/6/5 versus11/8/12 (squares), and5/5/6 versus10/16/28 (divisors). The best valid reference-bearing losses were7 for squares and10/10/10 versus10/9/9 for divisors. These tied or lagged the selected simpler models. Thus the observed limitation is not merely failure to generate or publish valid iterator-dependent structures. No causal claim about a particular edit follows. All original scientific outcomes and counters reproduced exactly; only new diagnostic scans and elapsed time differ. Full records are in iteration/VALIDITY_REPLAY.json and iteration/validity/.

## Independent reference provenance

3652 integer fixtures use Python arbitrary integers, math.gcd/lcm/comb/factorial and independently calculated Euclidean quotients. Seventy scientific value/partial fixtures use mpmath1.2.1 at100 decimal digits with exact Fraction.from_float binary64 identities. Production helpers are not the oracle. Scalar tolerances1e-14 absolute plus1e-13 relative cover the declared compound native operations on these inputs and are not a general error bound. The regeneration scripts are under scripts/references/.

## Limits and next boundary

These are small frozen engineering/recovery demonstrations, not optimizer or GA superiority comparisons. Iterator-dependent production improved representation coverage without solving the chosen iteration datasets. Grammar discovery, computational cost and sample identifiability remain separate issues. Optional arbitrary precision is incomplete: Boost headers were unavailable and no production bigint backend/codec was qualified. Recurrence order and initial conditions are fixed model context; transition trees evolve. Symbolic factorization and rational coefficient ranges are bounded subsets. No D6 work is begun.
