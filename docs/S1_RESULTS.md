# S1 experimental results

## Fixed-structure regressions

The frozen comparison is in `s1-results/fixed-structure.tsv`. It records both
wall and process CPU seconds. It uses four
scalar targets (fractional power, trigonometric, exponential and rational), two
starts per topology, and the D6 coupled oscillator. The supplied topology is
correct by design; these measurements test parameter fitting, not structural
discovery.

LM recovered all four scalar coefficient sets from both starts to close to
floating-point precision, and fit the coupled oscillator parameters near
`(0.25, 2)`. On the difficult fractional-power start, coordinate fitting ended
at MSE about `6.38e-2`, while LM reached about `3.92e-25`. For the trigonometric
target both fitters were already near machine precision. On the exponential
and rational starts, LM reached near-zero error with fewer objective calls than
coordinate fitting. The D6 comparison also reached near-zero held-out error.
These are small, fixed topologies and do not establish general superiority.

## Bounded evolutionary comparison

The frozen two-seed comparison is in `s1-results/evolutionary.tsv`; it records
wall and process CPU seconds. The
pre-rank-check implementation's completed output is retained as
`evolutionary-pre-rank-diagnostic.tsv`. The campaign used seeds 701 and 709,
the same population engine and grammar under each fitter, 50 candidate calls
and four committed generations. All four conditions consumed 50 candidate
evaluations.

The results are mixed and neither LM seed selected a high-quality model. Seed
701 with LM selected `2*x+4.058...` with training MSE `15.51` and held-out RMSE
`3.69`; its coordinate counterpart had training MSE `0.0470` and held-out RMSE
`0.217`. Seed 709 with LM selected `exp(x)` and had training MSE `7.36` and
held-out RMSE `2.71`; its coordinate counterpart had training MSE `0.608` and
held-out RMSE `0.775`. This small comparison does not establish an evolutionary
advantage for either method. Fixed-topology fitting was strong, while these
evolutionary runs show that fitting cannot compensate reliably for poor
structures and can leave selection with poor candidates. The LM rows report
derivative evaluations, model work, linear solves, and accepted and rejected
steps. Coordinate fitting remains the default.

The evolutionary app initially rejected an inconsistent preflight configuration
before any valid campaign run: the regression problem's maximum constant was
4 while the declared fitter bounds were `[-8,8]`. That failed launch is retained
in `evolutionary-initial-invalid-config.txt`. The protocol and problem limit
were aligned at 8 before the frozen valid comparison; no outcomes were used to
tune the grammar, seeds or optimizer.

The final TSVs were regenerated with the rank guard, complete CPU/linear
algebra work instrumentation, and the final fitter bookkeeping correction.
Intermediate completed TSVs remain preserved under `pre-*` filenames; the
scientific seeds, datasets, grammar and budgets remained fixed.

## Interpretation

The fixed-topology results show that parameter derivatives and LM can fit these
smooth models from the declared starts. Evolutionary results remain dependent
on whether useful structures survive the small fixed budget; the optimizer can
also settle on poor structures. S1 did not run any prime or successor campaign.
LM is suitable as opt-in regression research functionality, not as a changed
production default.
