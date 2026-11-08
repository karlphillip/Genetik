# Bounded-noise separability follow-up

## Result

The optional bounded-noise detector recovered the declared additive and
multiplicative hypotheses on all six noisy/clean separable training grids
(6/6; precision 6/6 and recall 6/6 over the declared positive grids). The
strict detector recovered the two clean grids and rejected four noisy ones
(precision 2/2, recall 2/6). Neither policy uniquely accepted the three
weakly nonseparable grids, the six negative-control grids, or the three
ambiguous constant grids. Zero grids remained degenerate/uncertain. This is a
small frozen synthetic suite, not a field false-positive guarantee.

A bounded error envelope cannot distinguish a true separable function from a
weakly nonseparable function whose cross-difference is smaller than the
maximum effect of measurement error. The deterministic contract test records
this case as compatibility evidence, not proof. The frozen weak-interaction
control has coefficient 0.01 and lies outside the declared `1e-4` target-noise
bound on this grid.

## Search effectiveness, kept separate from detection

Detection and fitting were reported as separate rows in the retained results.
On the 12 paired detected separable fits (two families, clean and two noisy
training grids, two seeds), automatic bounded-noise search and manual templates
had identical trajectories because detection consumes no random values and
selects the same scaffold. Both recovered the exact canonical additive
postfix structure in 3/12 runs and the multiplicative structure in 0/6 runs.
The ordinary baseline recovered neither exact structure in 0/12 runs. On this
small pairing, median assessment RMSE was 0.439655 for automatic/manual and
0.771080 for ordinary; median training MSE was 0.578738 versus 0.919160.
Median node counts were 4.5 for templates and 2.5 for ordinary. These medians
pool additive and multiplicative families and should not obscure the failed
multiplicative recovery.

For the additive family specifically, seed 29 recovered the exact structure
on clean and both noisy data, while seed 17 selected `x+y^2` and had assessment
RMSE 0.058446. For the multiplicative family, neither seed recovered the
canonical product on any noise condition; assessment errors remained large.
The search failure is downstream of correct detection: under the fixed 96
candidate / 2,400 row budget, random two-component trees often omit or lose the
needed paired structures, and the current objective has no structural
preference between factor scalings. This follow-up did not change GA behavior,
search initialization, grammar, budgets, or optimizer. It therefore does not
claim to have improved component recovery.

## Work accounting

Each detection examined 25 grid entries. Twenty nonzero grids charged 91
mathematical work units each; the clean zero grid returned degenerate after
grid indexing, for 1,845 units per policy across 21 grids. The paired search
comparison used 1,080 candidate calls and
27,000 training-row calls for automatic bounded-noise search, with 1,092
additional detector units. Manual templates used 1,080 candidate calls and
27,000 row calls; ordinary search used 1,152 candidate calls and 28,800 row
calls. These counters are reported separately because one detector arithmetic
unit, one candidate call, and one row evaluation are not interchangeable CPU
units. They are deterministic accounting measures, not timing claims.

## Reproduction

The frozen input is
`Genetik-data/benchmarks/automatic-separable-noise-followup/datasets.csv`.
Check `SHA256SUMS` before running. With examples and benchmarks enabled:

```sh
cmake --build build/release --target genetik_automatic_separable_noise_followup
build/release/genetik_automatic_separable_noise_followup \
  Genetik-data/benchmarks/automatic-separable-noise-followup/datasets.csv
```

The runner consumes only training rows for detection and fitting. Assessment
rows are separately frozen and are used only to score returned expressions.
Canonical structural scoring uses the reference expression in the independent
benchmark runner; no reference expression enters candidate generation or
selection. Finite data agreement is not a proof of global equivalence.
