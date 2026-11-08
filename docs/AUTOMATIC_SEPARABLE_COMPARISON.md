# Bounded separability comparison

The retained run is in the workspace evidence directory
`Genetik-data/benchmarks/automatic-separable-discovery/`. Its protocol, 72-run
CSV, and SHA256 manifest accompany this report. The benchmark source is
`benchmarks/automatic_separable_comparison.cpp`.

The current evidence-manifest SHA256 is
`b8d43fefd5f5205d1f701abd3cf8d34efea8f1524490c7ee9dca5cb7c2a640da`.

## Design

The study used five functions: `sin(x)+y^2`, `(1+x)(1+y^2)`, `sin(x+y)`,
`x+y+xy`, and the degenerate zero function. Training used complete 5x5 grids;
assessment used disjoint 4x4 grids. Each function had clean training targets
and two distinct deterministic hash-noise realizations with amplitude
`1e-4`. Search seeds 17 and 29 were paired across automatic decomposition,
manual two-site templates where a supported form was specified, and ordinary
symbolic regression. Each search used a 24-member population, two elites, at
most 96 candidate evaluations and 2,400 training-row evaluations. The ordinary
grammar was the union of the two component grammars. Automatic detector work
was reported alongside candidate and row counters.

Structural recovery here is exact postfix representation equality to the
canonical reference, including its constants. It does not equate algebraically
equivalent forms or multiplicative factor rescalings. Training and assessment
errors use the generating function only in the independent benchmark scorer.

## Observed results

| Method / data | Returned models | Exact training fits | Exact canonical structure | Median held-out RMSE |
|---|---:|---:|---:|---:|
| Automatic, clean (10 cases) | 4 | 1 | 1 | 0.835285 |
| Automatic, noise A (10) | 0 | 0 | 0 | not applicable |
| Automatic, noise B (10) | 0 | 0 | 0 | not applicable |
| Manual template, clean (4 separable cases) | 4 | 1 | 1 | 0.835285 |
| Manual template, noise A (4) | 4 | 0 | 1 | 0.835285 |
| Manual template, noise B (4) | 4 | 0 | 1 | 0.835285 |
| Ordinary, clean (10 cases) | 10 | 2 | 2 | 0.596880 |
| Ordinary, noise A (10) | 10 | 0 | 2 | 0.596880 |
| Ordinary, noise B (10) | 10 | 0 | 2 | 0.596880 |

The one exact automatic recovery was the additive `sin(x)+y^2` case at seed 29;
the second seed retained a poor three-node approximation. Neither clean
multiplicative run recovered the intended model. Both negative controls were
rejected by automatic detection on their clean grids, as was the zero-valued
degenerate grid. All 20 noisy automatic grids were rejected at the configured
tolerances, including the two truly separable functions. The results show
noise sensitivity and limited search recovery; they do not establish a
predictive advantage over the ordinary search.

On the separable cases, automatic and manual templates produced identical
search trajectories for matched seeds because detection consumes no RNG and
selects the same scaffold. Automatic detection added 778 work units on the ten
clean grids and 910 on each noisy set of ten. Candidate/row work is retained
separately in the CSV rather than collapsed into a platform-dependent cost
score. The ordinary runs used 816 candidate calls/20,400 rows on clean data,
and 960 calls/24,000 rows per noisy set; automatic search used 312 calls/7,800
rows on clean data and no searches on noisy data because detection rejected
those hypotheses.

These bounded results support the usefulness of deterministic hypothesis
screening on clean data and document its false-negative risk under noise. They
do not prove global separability, model correctness, or broad search
superiority. Further scientific tuning was outside this milestone.
