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
