# Anchored antiderivative discovery and independent coefficient fitting

Phase 7B, local Release evidence. Protocol was frozen in PROTOCOL.txt **before**
the first search. No retries, configuration tuning, injected expression or extra
value labels were used. All selected models and the representative were frozen
using training-only ordering before any assessment. Complete records and data
identities are in [derivative-regression-results/](derivative-regression-results/).
This is one bounded experiment, not a success guarantee or general-function proof.

## Frozen protocol

Schema x0=x. Training contains precisely f(0)=1 and nine observations of f'=2*x
at {-2,-1.5,-1,-.5,0,.5,1,1.5,2}. Each enabled channel has scale1/weight1:
L=.5*anchor_squared_error+.5*derivative_MSE. The anchor is a soft observation,
not a constraint automatically satisfied by every candidate. No other f values
are supplied to initialization, selection, stopping or fitting.

Seeds0,1,2,3,42; population128; loss/candidate ceilings10000; generations2000;
node9/depth4 (leaf1), constant bound2, terminals{-2,-1,0,1,2}; operations+,-,*
uniform over feasible kinds; no continuous sampling, no fitting; exact-zero
composite objective stop. Rank selection, elites2, crossover.9, mutation.35,
initial depth4, mutation depth3, grow probability.65, full probability.5, finite
proposal limit8 are the existing policies. The complete resource defaults are
recorded in PROTOCOL.txt and DERIVATIVE_REGRESSION.md. Reference execution is
serial (requested/effective1), sequential seeds. A future --threads request uses
the existing candidate pool, never concurrent seeds.

Training selection: composite loss, nodes, faithful text; across seeds retain the
first declared seed if all those fields tie. Representative is **seed2**, selected
before assessment. Search time includes initialization; excludes data generation,
file output, reporting and the separate fixed-structure fitting fixture. Recorded
millisecond runtimes are single observations on this Jetson, not speedup estimates.
The recorded campaign used the initial Phase 7B implementation. Subsequent
compatibility-only worker-hook and lexical value-workspace repairs preserve its
objective arithmetic, grammar, counters and random draws; recorded runtimes are
not measurements of those later source revisions. Final-source contract tests
remeasure the supplied coefficient fixture and verify deterministic execution.

## Every seed

All initial objectives were0.5; all final objectives, final channel means and anchor
errors were0. Every run stopped with exact_training_fit. No initialization success
occurred; generation0 is retained in each trace. Every run used10 observations per
objective (one ordinary value plus nine partial evaluations). No unpublished work,
invalid primal, unavailable derivative, loss-arithmetic failure or resource stop
occurred. No failure seed is omitted.

| Seed | Final faithful expression | Nodes/depth | Discovery / published generations | Objective attempts | Unused allowance | Search ms |
| ---: | --- | --- | ---: | ---: | ---: | ---: |
| 0 | `(((x0 - x0) + 1.0) + (x0 * x0))` | 9/4 | 6 | 896 | 9104 | 7.948257 |
| 1 | `(1.0 + (x0 * x0))` | 5/3 | 3 | 512 | 9488 | 4.640836 |
| 2 | `((x0 * x0) - (-1.0))` | 5/3 | 2 | 384 | 9616 | 3.362687 |
| 3 | `(1.0 - (x0 * (x0 * (-1.0))))` | 7/4 | 3 | 512 | 9488 | 4.672549 |
| 42 | `(1.0 + (x0 * x0))` | 5/3 | 3 | 512 | 9488 | 4.162935 |

| Seed | Observations / value / partial calls | Primal operations | Derivative arithmetic | Aggregate arithmetic | Mutation proposals/changes | Crossover proposals/changes | Structural rejections |
| ---: | --- | ---: | ---: | ---: | --- | --- | ---: |
| 0 | 8960 / 896 / 8064 | 10780 | 9675 | 41216 | 270/232 | 343/520 | 25 |
| 1 | 5120 / 512 / 4608 | 5500 | 4005 | 23552 | 139/122 | 170/242 | 17 |
| 2 | 3840 / 384 / 3456 | 4850 | 3978 | 17664 | 81/67 | 116/172 | 10 |
| 3 | 5120 / 512 / 4608 | 7570 | 7065 | 23552 | 163/126 | 182/264 | 25 |
| 42 | 5120 / 512 / 4608 | 5650 | 4392 | 23552 | 154/122 | 183/278 | 30 |

Additional native derivative calls and mutation/crossover fallbacks were0. Complete
partial node visits and committed improvement traces against actual objective
attempts are retained in search.txt. Generations are completed shared-engine
publications, with initialization counted separately as0.

## Post-selection assessment (all seeds)

Only after freezing: evaluate f against independently generated x*x+1 and its
partial against2*x on all nine derivative-training abscissae, eight interior
points {-1.75,-1.25,-.75,-.25,.25,.75,1.25,1.75}, and four exterior points
{-4,-3,3,4}. Function-value labels on the first set, except the single original
anchor, were **withheld from fitting and model selection**, not additional training
values. Targets use ordinary arithmetic on the declared exact dyadic inputs,
not Expression or evaluatePartial as an oracle.

Each of the five seeds has maximum absolute value error **0** and maximum partial
error **0** on each of these three sets. Every requested prediction was valid,
all reports complete, invalid/unavailable/resource counts0. Thus all five meet
the predeclared1e-6 criterion on each declared set. Finite samples plus an anchor
do not establish uniqueness or behavior elsewhere. Faithful expressions are
reported without algebraic rewriting; unnecessary cancelling terms in seed0 remain.

## Separate supplied-structure coordinate fitting

Supply `a*x0*x0+b*x0+c`, start(1,1,1), with derivative targets3*x-.5 on the same
nine points and only the anchor f(0)=1.25. Equal weights/unit scales. No function
values outside that anchor enter the objective. Target coefficients are
(1.5,-.5,1.25), independently generated; this structure is never a campaign seed.
Limits: nodes15/depth6/constant bound8; objective cap512; fitter bounds[-8,8],
initial step1/minimum1e-8, sweeps128, proposals8192, no early loss threshold.

Initial composite objective: **1.9895833333333333**. Recovered:

```text
((((1.5 * x0) * x0) + ((-0.5) * x0)) + 1.25)
```

All three independent postfix coefficients recover exactly: (1.5,-.5,1.25).
Structure stays11 nodes/depth5. Final objective0 agrees with remeasurement;
173 composite attempts (339 unused),1730 observations,173 value calls/1557
partial calls,8650 primal operations/9342 derivative arithmetic operations/7958
aggregation operations,4 accepted coordinate changes. Termination minimum_step.
All post-fit value/partial maximum errors are0 on the same three assessment sets;
all predictions valid. Complete fixture: fitting.txt. This establishes coefficient
recovery for a supplied family, separately from operation/structure discovery.

## Reproduction and limitations

```sh
out/build/release/genetik_derivative_regression_campaign \
  --output out/runs/anchored-antiderivative-new --threads 1
```

No random generator is used for target preparation. Row and seed order, constants,
limits, scales, weights, stop and representative rule are fixed above. Same-seed
repeatability remains qualified by the exact library revision, compiler, standard
library/libm, FP environment and execution mode. Independent nonbinding1/2/4-worker
engineering comparisons are not a retuned campaign.

Derivative-only fits have additive-constant ambiguity: x*x, x*x+1 and x*x+7 all
score0 on the derivative channel. With this anchor their objectives are.5,0,18.
In multiple inputs, observing one partial leaves freedom in the others, not merely
one global constant. A point anchor is generally insufficient to remove it.
Conservative first-partial unavailability and finite-intermediate limits still apply.
Only 7C bounded numerical integration remains before final qualification/delivery;
no residual language, new optimizer or typed derivative objective is introduced.
