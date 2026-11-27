# Rank-aware scalar coefficient refinement

Base: `a13aa56a17561819af1ff86822dcd7bbd2e18369` (the merge commit of PR #75).
The existing LM rejected redundant parameter sets as `SingularSystem`; selected
constant occurrences and checked parameter partials were already public. The
opt-in fitter performs deterministic rank selection before LM while leaving the
default path unchanged.

The rank rule is pivoted modified Gram-Schmidt with two re-orthogonalization
passes. Jacobian columns are globally scaled by their largest absolute entry
before factorization. On each pivot, the largest residual Euclidean norm wins;
exact ties choose the lower postfix constant-node index. A direction is retained
only when its norm is strictly greater than `rankTolerance * largest original
column norm` (`rankTolerance=1e-10` by default). This is a numerical
identifiability rule, not a claim that the original coefficients are separately
recoverable.

The synthetic target is `2*x+3`, trained at `x=-3,-2,-1,0,1,2,3` and assessed
at `x=-0.5,2.5,4`. The supplied-topology redundant fixture is
`(0.5+0.5)*x+0`; the supplied full-rank control is `1*x+0`. Both baseline and
opt-in LM use identical starting values and a 512-unit local cap. Selector
partial evaluations consume that same cap and the search-wide training-work
allowance. Selector work and LM work are reported separately.

| Fixture | Base / baseline result | Opt-in result | Work and stop |
|---|---|---|---|
| Supplied redundant topology `(a+b)*x+c` | Same starting and final MSE 13; held-out MSE 28.5; LM stop `SingularSystem` | Selected constant nodes `[0,5]`; fitted `(a,b,c)=(1.5,0.5,3)`, so identifiable `a+b=2`; training MSE `5.81e-30`, held-out `1.47e-29` | Baseline: 1 work unit. Rank-aware: 21 selector derivative evaluations + 9 objective calls (8 LM objective evaluations) = 30 units; LM stop `LossTolerance` |
| Supplied full-rank control `a*x+c` | Starting MSE 13; final training MSE `5.81e-30`, held-out MSE `1.47e-29`; LM stop `LossTolerance` | Starting MSE 13; kept both directions `[0,3]`, coefficients `(2,3)`; final training MSE `5.81e-30`, held-out MSE `1.47e-29` | Baseline: 8 objective calls. Rank-aware: 14 selector evaluations + 9 objective calls = 23 units; both `LossTolerance` |
| Tight local budget, cap 8 | Held incumbent at training MSE 13 and held-out MSE 28.5, one unit; `SingularSystem` | Held the same valid incumbent at training MSE 13 and held-out MSE 28.5; no selector columns admitted, one of eight units used; `EvaluationLimit` | Identical start and local cap |
| Tight global budget, cap 10 | Held incumbent at training MSE 13 and held-out MSE 28.5, one unit; `SingularSystem` | Spent 9 selector evaluations plus one baseline objective call, returned the same incumbent with training MSE 13, held-out MSE 28.5, and zero work remaining; `EvaluationLimit` | Identical start and global cap |
| Ordinary grammar search, seed 20261127 | `(x0 + 2.9999999999292117)`, training MSE 2, held-out MSE 7.5 | `(x0 + 2.9999999999292117)`, training MSE 2, held-out MSE 7.5 | Both: 50 candidates and `GenerationLimit`. Base: 294 objective calls, no selector. Corrected candidate: 419 objective calls plus 255 selector derivative evaluations (674 combined work units); 369 LM objective evaluations |

The independent expected redundant fit is the identifiable sum `a+b=2`,
intercept `c=3`, and zero prediction error on held-out inputs. The full-rank
expected coefficients are `(2,3)`. The direct fitted results match both; the
generated search case remains a measured miss on the slope.

The ordinary-search row is generated from the declared arithmetic grammar and
training rows. It is a feature-path check, not a guaranteed recovery campaign.
The direct fixture rows are supplied-topology diagnostics and are labeled
separately. No D7 target or historical fit is read or modified.

## Follow-up correction

On the rank-aware follow-up head, selector derivatives are now evaluated at the
actual starting coefficients, use the fitted objective's weighted residual
Jacobian (including a zero Jacobian for zero-weight rows), and pass selected
constant indices to LM in increasing postfix order. The work allowance is
admitted before each public objective call; the mathematical-operation cap is
admitted across baseline prediction, selector derivatives, orthogonalization,
and LM. Exhausted objectives return without another evaluation. Successful
results retain baseline, selector, and fitter counters.

The generated-search comparison above was replayed on base
`a13aa56a17561819af1ff86822dcd7bbd2e18369` and the corrected candidate with
seed `20261127`, identical training/held-out rows, arithmetic grammar, and
limits. Both returned the same equation and missed the slope. The prior
candidate-only accounting (`235` selector derivatives, `329` objective calls,
`279` LM evaluations) is retained here as the earlier record; it is superseded
by the corrected candidate counters in the table (`255`, `419`, `369`). The
base replay is `294` objective calls and has no selector-work counter.

Additional supplied-topology regressions cover `(a*b)*x` from `(2,3)` against
`12*x`, weighted affine rows `(1,4),(3,0)` with weights `[1,0]`, and a pivot
that selects the intercept before the slope for `a*x+c` on
`x=[-0.25,0,0.25]`. They retain near-zero scored train/held-out losses and
assert sorted LM indices. A ten-unit exhaustion/reuse control verifies that a
subsequent public `measure()` and fit admit no work. A 100-operation control
stops at exactly 100 counted operations. Successful-operation counters include
the preselection baseline and remain within the configured cap.

Checkpointed `ScalarMathematicsRun` rejects this opt-in because its existing
checkpoint work codec has no selector counter; default checkpoint payloads and
restoration remain unchanged. Ordinary `symbolicRegression`, `fitConstants`,
and `fitSelectedConstants` support the opt-in path. See
[constant fitting](CONSTANT_FITTING.md) for the public configuration and
accounting contract.
