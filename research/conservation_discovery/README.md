# Grouped-trajectory conservation discovery

## Protocol and boundaries

The base `1344982fe798108abf2993ba204cd399d93ab947` had no grouped-trajectory
objective or conservation discovery command. Its symbolic search accepted scalar
regression rows only, so a matched base discovery baseline could not express this
task. The candidate uses the existing `Expression` representation, ordinary
variation, `symbolicRegression`, and the grouped scorer. Search sees only `(y,v)`
state rows. Trajectory grouping is retained by the scorer. Time, trajectory IDs,
initial-condition labels, dynamics, and reference invariant values are not search
inputs. No invariant expression is seeded.

`study.json` fixes the same Add/Subtract/Multiply grammar, limits, three seeds,
and 640 candidate evaluations for each oscillator. Each system has six training
initial conditions and two held-out initial conditions, split by whole initial
condition. The model is written to the selected-expression artifact before the
separate verifier process receives it. The verifier checks the exact polynomial
Lie derivative against the declared vector field and separately fits a nonzero
affine mapping to the reference law on held-out states.

Trajectories use classical RK4 with `dt=0.025`, 400 steps, and 91 retained states
per initial condition. The reported numerical error is the largest Euclidean
state difference against the same generator at half step (`dt=0.0125`) over
training and held-out initial conditions. The generator refinement differences
were `6.1034994552631315e-08` for the harmonic system and
`1.4217458745128926e-04` for the quartic system.

The effective host process affinity was CPUs 0–7, cgroup CPU and memory limits
were unbounded, and host available memory was 12.9 GB at preflight. Each child
was then capped at CPUs 0–1, 60 CPU seconds, 90 wall seconds, and 4 GiB address
space. The declared aggregate maximum was 360 CPU seconds and 540 wall seconds.
All six target runs completed in 0.075–0.089 seconds each.

The synthetic rehearsal used the same outer runner, adapter arguments, frozen
specification, compiled executable, selection serialization, and verifier path.
Its hashes are recorded in the frozen package manifests. The first rehearsal
and target pass exposed that cancellation-generated near-constants could pass
the scale-free separation floor. That pass is preserved as v1 diagnostic
evidence. The scorer now also requires normalized between-trajectory standard
deviation >= `1e-12`; deterministic tests exercise the cancellation case. The
final study package is `frozen-v2/`, with results in `results/target-runs-v2.json`.

## Scoring and outcome definitions

For candidate values divided by their maximum absolute magnitude, the scorer
computes observation-weighted within-trajectory and between-trajectory sums of
squares. It minimizes `within/(within+between) + 1e-6 * physical_node_count`,
rejecting constants, invalid predictions, insufficient between fraction
(`<0.01`), and normalized separation below `1e-12`. The reported held-out drift
is the within sum divided by total within-plus-between sum; separation is the
standard deviation of held-out trajectory means in the candidate's original
output units.

Exact conservation requires a nonconstant polynomial and an identically zero
exact symbolic Lie derivative. Reference-law recovery is reported separately:
the candidate must have a nonzero affine relation to the supplied law on held-out
states within `1e-8` relative tolerance. An exact different invariant would
count as exact conservation but not reference-law recovery.

## Final frozen runs

`x0=y`, `x1=v`. All six runs stopped at the 640 candidate-evaluation budget;
each charged 349,440 training row evaluations. All returned models. The held-out
drift is dimensionless; separation is in candidate-output units.

| System | Seed | Selected expression | Nodes | Held-out drift | Separation | Work | Exact nonconstant | Lie derivative zero | Reference law affine |
|---|---:|---|---:|---:|---:|---|---|---|---|
| Harmonic | 101 | `((0.5 - (x1*x1)) * (x1*x1))` | 9 | 0.6766583421 | 2.5523127812 | 640 candidates; 349,440 rows; 0.0886 s | yes | no | no |
| Harmonic | 211 | `(((x1*((x1+1)+(x1-1))) + ((((-0.5+x0)*1)*x0)-(-2))) + (-1))` | 21 | 0.1166824474 | 2.8700821745 | 640; 349,440; 0.0841 s | yes | no | no |
| Harmonic | 307 | `(x1 * (x1 - (x1*(x1*x1))))` | 9 | 0.7029797327 | 2.0836453633 | 640; 349,440; 0.0854 s | yes | no | no |
| Quartic | 101 | `((-2.0) - ((-2.0) - (((1.0 - (x0 * x0)) - ((x1 * x1) + (-0.5))) * 0.5)))` | 17 | 0.2070510040 | 3.0133068031 | 640; 349,440; 0.0806 s | yes | no | no |
| Quartic | 211 | `((((1-(x1*x1))-((-2)-(-2))) * (x1*x1)) * (-2))` | 15 | 0.5048330192 | 135.1171958253 | 640; 349,440; 0.0887 s | yes | no | no |
| Quartic | 307 | `(((-1+1)-x1) * x1)` | 7 | 0.3727889791 | 5.2396221706 | 640; 349,440; 0.0751 s | yes | no | no |

| Harmonic | 101 | numerical best-affine relative RMSE `0.5442450` |
| Harmonic | 211 | numerical best-affine relative RMSE `0.2260021` |
| Harmonic | 307 | numerical best-affine relative RMSE `0.5547293` |
| Quartic | 101 | numerical best-affine relative RMSE `0.3166880` |
| Quartic | 211 | numerical best-affine relative RMSE `0.4945004` |
| Quartic | 307 | numerical best-affine relative RMSE `0.4249368` |

Outcome: 0/6 exact nonconstant invariants, 0/6 reference-law recoveries, and no
candidate with low numerical reference-law error. The independent best-affine
RMSE is normalized by reference-law RMS on held-out states. Every seed
completed; there were no timeouts or missing models. The quartic seed 211 model
has large output separation but also substantial within drift, so separation
alone does not imply conservation.

The damped harmonic negative control changed energy from `0.9265926100` to
`0.2387244377` (change `-0.6878681724`) under `v'=-y-0.15v`.

The earlier v1 selection artifact is retained alongside its first frozen
package as an invalidated diagnostic. v1 exposed the numerical-degeneracy issue;
the table above and v2 machine artifacts are the final measured comparison.

## Exact verifier recheck (no search rerun)

The six saved v2 selections were re-read from `results/target-runs-v2.json`
without rerunning search. `scripts/verify_conservation_exact.py` uses SymPy
1.12 exact rational polynomial coefficients to separately establish support,
nonconstancy, exact Lie derivative, and coefficient-wise `F=aE+b` recovery.
Its output is `results/exact-verification-v3.json`; the frozen v2 records above
were not rewritten. All six expressions are supported and nonconstant; all six
have a nonzero exact Lie derivative and fail exact affine recovery. The
independent fixtures establish that `E` and `3-2E` pass affine recovery, `E^2`
is exactly conserved but not affine in `E`, and `y^2` is not conserved.
Unsupported syntax or algebra exhaustion is represented as inconclusive by the
verifier contract; none occurred for these six short polynomials.

## Deterministic polynomial method (base `44524894a295cbdd4656d1b26de273ffe543d3c1`)

The base had the grouped scorer and stochastic symbolic expression search, but
no monomial nullspace discovery API. Its six saved v2 selections (above) are the
before result. The candidate adds `PolynomialConservationDiscovery.hpp`, whose
public function receives only grouped `(y,v)` observations and limits. It does
not receive a system name, time, initial-condition labels, equations of motion,
or reference energies.

The method enumerates all nonconstant monomials in `y,v` by total degree 1–4.
Each feature is divided by its largest observed absolute value and centered
within each training trajectory; columns are then L2-normalized. A symmetric
Gram matrix is diagonalized by deterministic Jacobi rotations. Fixed rules are:
relative smallest-eigenvalue rank tolerance `1e-8`; Jacobi off-diagonal stop
tolerance `1e-14`; at most 10,000 rotations per degree and 10,000,000 cumulative
monomial-observation values; coefficient ratios reconstructed by nearest
rational with denominator at most 1024; reconstructed within-fraction at most
`1e-6`; and between-fraction at least `0.01`. Selection checks degrees in increasing order
and accepts the first supported degree. The returned expression uses ordinary
bounded `Expression` nodes, and the existing grouped scorer validates the
reconstruction. Work reports feature values and Jacobi rotations separately.
There is one minimum-eigenvector candidate at the first accepted degree; the
fixed monomial order is the deterministic tie rule, followed by node count.

The selector was run on the same six training initial conditions and trajectory
generator as v2. It wrote `results/polynomial-v1.selected.json` before the
comparison process loaded the v2 selections or assessed any held-out trajectory.
Assessment used fresh whole initial conditions `(0,2)`, `(-0.5,0)`, and
`(1.5,-0.5)`, all absent from training. The same three held-out groups were used
for the selected polynomial and every saved v2 candidate. The comparison artifact
is `results/polynomial-v1.comparison.json`; it is additive and does not modify
any frozen v1/v2 study record.

| System | Base: best saved PR #72 candidate on these held-outs | Candidate equation (`x0=y`, `x1=v`) | Degree | Candidate drift | Candidate separation | Candidate work | Exact conservation / affine recovery |
|---|---|---|---:|---:|---:|---|---|
| Harmonic | seed 211; drift `0.199949`; neither | `y² + v²` | 2 | `3.97446499e-19` | `0.385275875` | 2,730 feature values; 6 rotations; 0.000320 CPU s | yes / yes |
| Quartic | seed 101; drift `0.138737`; neither | `y⁴ + v²` | 4 | `8.60747595e-14` | `0.419919106` | 7,644 feature values; 166 rotations; 0.001577 CPU s | yes / yes |

The independent exact verifier reports the exact reference affine scales as one
for both returned laws. For the six saved PR #72 selections on the same fresh
held-out groups, drift fractions were harmonic
`0.694628, 0.199949, 0.709774` and quartic `0.138737, 0.575192, 0.443942`;
none was an exact invariant or an exact affine recovery. Their original search
work remains 640 candidate evaluations and 349,440 training rows per run. Exact
verifications for these records are carried forward from
`results/exact-verification-v3.json`; the new comparison independently reruns
the exact verifier on the expression strings.

The retained damped harmonic negative control uses `v'=-y-0.15v`: reference
energy changed from `0.9265926100` to `0.2387244377` (change `-0.6878681724`).
The candidate laws do not establish a conserved reference energy for damped
dynamics. Trajectory generation remains RK4 with `dt=0.025`; the retained
generator refinement errors against half-step RK4 are `6.10349946e-8` harmonic
and `1.42174587e-4` quartic on the original generator conditions. On the three
fresh held-out starts, the same refinement comparison gives `6.10349946e-8`
harmonic and `1.63058940e-5` quartic. These estimates compare two numerical
solutions and are not global error bounds.

Qualification for this change: the focused Release and ASan/UBSan conservation
contracts passed. A clean install from the final Release tree and a separate
one-translation-unit CMake consumer using only `find_package(Genetik)` and
`Genetik::Genetik` built and ran the new public API successfully (exit 0). The
full package workflow's `installed_consumer` target also built and ran (exit 0)
against its clean prefix, including the API regression.
The repository's broader `tests/package_integration.py` run reached 300 seconds
in its `ordinary consumer build` step at 89% quickstart compilation; the newly
added installed consumer target had already built and run successfully. Resuming
the killed full build encountered a partial-object link failure in the unrelated
`automatic_separable_quickstart`, so the all-quickstarts package stage remains
incomplete. The failure log is retained in the worktree build output and is not
part of the frozen scientific results.

## Measurement corrections on PR #73

The original `polynomial-v1.*` artifacts above are preserved verbatim. Review
found that v1's `feature_values` reported only the final degree matrix, not the
sum of all cumulative matrices evaluated through the selected degree. The public
result now exposes `cumulativeFeatureValues`, the selector uses that same
counter for its work budget and study output, and the corrected rerun is recorded
in `results/polynomial-v2.selected.json`. Its separate
`final_matrix_feature_values` field identifies the selected degree's matrix
size. Expected values are 3,822 cumulative / 2,730 final-matrix values for
harmonic and 16,380 cumulative / 7,644 final-matrix values for quartic.

The scorer's held-out separation is the standard deviation of trajectory means
after dividing all candidate outputs by their maximum absolute value. It is
dimensionless and invariant under nonzero output scaling. Earlier v2 records
retain their original units as historical evidence; the additive v3 comparison
corrects the report for both the polynomial selections and all six saved PR #72
candidates. A public regression confirms that equal-sized `y=1,2` trajectories
have separation `0.25` for both `y` and `8*y`.

The corrected API also checks zero-within-variation monomials directly. A
degree-one grouped fixture with fixed `y` values `-2, 1, 2` and varying `v`
selects `y` (up to nonzero scale); a globally constant coordinate remains
rejected. A separate exact-ratio fixture with `v=C-3y` selects an integer
multiple of `3y+v`. Rational coefficient ratios are cleared to primitive
integers only when denominator growth, coefficient magnitude, expression
complexity, and arithmetic limits permit; otherwise the result marks exact
representation unavailable and does not publish an approximate reconstruction.
The independent SymPy verifier now checks the exported shear law under
`y'=1, v'=-3` exactly. The deterministic test exported
`((3.0 * x0) + x1)`; the verifier reports nonconstant, exactly conserved, Lie
derivative `0`. The `C-y` fixture exports a three-node form of `y+v` under a
three-node expression limit; the verifier confirms zero Lie derivative under
`y'=1, v'=-1`. This check is recorded in
`results/polynomial-v2.linear-shear-verification.json`.

The corrected selection and comparison are additive artifacts. All six saved
PR #72 expressions are re-assessed on the same held-out trajectories and
re-verified without rerunning their searches.

The additional correction is in `results/polynomial-v3.*`; it leaves all v1
and v2 artifacts untouched. The v3 `heldout_separation_units` value is
`dimensionless_normalized_by_max_abs_candidate_output`.

| System | Selected equation (unchanged) | Cumulative work | Final matrix | Held-out drift | Separation (candidate-output units) | Exact / affine |
|---|---|---:|---:|---:|---:|---|
| Harmonic | `y^2+v^2` | 3,822 values; 6 rotations | 2,730 values | `3.97446499e-19` | `0.385275875` | yes / yes |
| Quartic | `y^4+v^2` | 16,380 values; 166 rotations | 7,644 values | `8.60747595e-14` | `0.419919106` | yes / yes |

## Additive v3 corrections (2026-11-26)

The v2 comparison evidence remains unchanged. The study command now returns held-out
separation in dimensionless units after dividing candidate outputs by the maximum
absolute candidate output. `polynomial-v3.comparison.json` records this unit for both
oscillator selections and all six saved PR #72 expressions. The six numeric values
are unchanged because the public scorer already returned normalized separation; v2
had mislabeled those values as candidate-output units. The regression confirms the
same value `0.25` for `y` and `8*y` on equal-sized `y=1,2` groups.

Polynomial export now omits the zero initializer and coefficient-one multipliers.
The unchanged oscillator laws shrink from 13 to 7 nodes (harmonic) and 17 to 11
(quartic). Cumulative work remains 3,822 / 16,380 feature values; final matrices
remain 2,730 / 7,644. The `C-y` degree-one fixture exports `x0+x1` in three nodes
under `maxNodes=3`; the independent exact verifier reports Lie derivative `0` under
`y'=1, v'=-1`. A two-node budget rejects it. The exact result is recorded in
`results/polynomial-v3.compact-linear-verification.json`.

| Saved candidate | Held-out drift | Normalized separation |
|---|---:|---:|
| Harmonic PR #72 seed 101 | 0.6946278345 | 0.1516822294 |
| Harmonic PR #72 seed 211 | 0.1999491149 | 0.2517026352 |
| Harmonic PR #72 seed 307 | 0.7097735026 | 0.1464864412 |
| Quartic PR #72 seed 101 | 0.1387369474 | 0.4722842001 |
| Quartic PR #72 seed 211 | 0.5751915867 | 0.2277242113 |
| Quartic PR #72 seed 307 | 0.4439416784 | 0.2832792626 |
