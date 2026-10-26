# D6 frozen scientific recovery results

Protocol was committed before campaign execution. Population24, seeds0/1/42,
workers2,20-generation/504-candidate-request/2048-loss caps. Coordinate fitting
uses32 attempts locally for this bounded benchmark; the library default remains256.
All runs executed sequentially. Later data were assessed after the training-best
model and final checkpoint were frozen. Recovery below means held-out full-objective
MSE <=1e-12; it is not a universal identity or physical-law proof.
The original and repaired run manifests, compact checkpoints, result rows and
initialization diagnostic are retained under [`runs/`](runs/). The initial-attempt
records remain beside the repaired outcomes so their scopes stay distinguishable.

| Task | Seed | Training MSE | Held-out MSE | Generation | Attempts | Outcome |
|---|---:|---:|---:|---:|---:|---|
| oscillator | 0 | 0.046874999999999993 | 0.017578125 | 2 | 2048 | evaluation_budget |
| oscillator | 1 | 0.005208333333333333 | 0.0019531249999999998 | 2 | 2048 | evaluation_budget |
| oscillator | 42 | 0.005208333333333333 | 0.0019531249999999998 | 2 | 2048 | evaluation_budget |
| complex | 0 | — | — | uncommitted initialization | see diagnostics | numerical failure |
| complex | 1 | 0.4114748214530799 | 0.93126053805205644 | 2 | 2048 | evaluation_budget |
| complex | 42 | 0.4114748214530799 | 0.93126053805205644 | 2 | 2048 | evaluation_budget |
| tensor | 0 | 0 | 0 | 0 | 582 | exact_training_fit |
| tensor | 1 | 0 | 0 | 0 | 551 | exact_training_fit |
| tensor | 42 | 0 | 0 | 0 | 520 | exact_training_fit |
| gaussian | 0 | 0 | 1.9259299443872359e-34 | 2 | 1343 | exact_training_fit |
| gaussian | 1 | 1.2077325870824419e-05 | 1.3319026769111276e-05 | 2 | 2048 | evaluation_budget |
| gaussian | 42 | 0.010576450331939201 | 0.0091130845974060157 | 2 | 2048 | evaluation_budget |
| oscillator-fit | 0 | 0 | 0 | fixed structure | 109 | coordinate fit |
| heat | 0 | 0 | 0 | fixed structure | 53 | coordinate fit |
| gaussian-fit | 0 | 0 | 1.9259299443872359e-34 | fixed structure | 118 | coordinate fit |

Four of twelve discovery runs met the held-out criterion: tensor3/3, Gaussian1/3,
oscillator0/3, complex0/3. All three tensor successes were fitted initialization
discoveries (generation0), not subsequent structural evolution. Gaussian seed0
recovered the target at generation2. Other negative models remain retained.

These counts describe bounded search outcomes on the declared finite samples.
They do not test whether the equations or formulas are mathematically correct:
that is covered by independent numerical references and deterministic contracts.
Likewise, zero training error is a fit, structural search reaching a target is a
discovery, and meeting the separately frozen held-out threshold is empirical
generalization; none implies the others. In particular, tensor's three generation0
recoveries followed fitting-assisted initialization, while the three fixed-structure
fits below used supplied model structure and are not evolutionary discoveries.

The separate fixed-structure fits recovered damping.25 and spring2 in109 attempts,
heat coefficient.25 in53, and Gaussian mean.5/standard deviation1.25 in118.
Training MSE was zero; held-out MSE was zero for oscillator/heat and approximately
1.926e-34 for Gaussian. These are parameter-identification fixtures, not unconstrained
law discovery. Initial and boundary heat observations were supplied. The field itself
was prescribed; no PDE solver or trajectory simulator was used.

## Selected formulas and work

### oscillator seed0

Source `588a07b3d4f17d3857f84024409c42fb24d492e6`; evidence `runs/campaign-repair/oscillator-0`.

```text
(((-1.0) * x1) - (x2 + x1))
−1·x1 − (x2 + x1)
```

Nodes 7, depth 3; fitting attempts 2048; rows 61440; charged operations 448744; field evaluations 61440; recorded application seconds 0.20519723200000001.

### oscillator seed1

Source `588a07b3d4f17d3857f84024409c42fb24d492e6`; evidence `runs/campaign-repair/oscillator-1`.

```text
(x0 + (x1 * (-2.0)))
x0 + x1·(−2)
```

Nodes 5, depth 3; fitting attempts 2048; rows 61440; charged operations 463204; field evaluations 61440; recorded application seconds 0.20295576100000001.

### oscillator seed42

Source `588a07b3d4f17d3857f84024409c42fb24d492e6`; evidence `runs/campaign-repair/oscillator-42`.

```text
(((-3.0) - (0.0 - x3)) * ((x4 * x1) + (1.0 + (-1.0))))
(−3 − (0 − x3))·(x4·x1 + (1 + (−1)))
```

Nodes 13, depth 4; fitting attempts 2048; rows 61440; charged operations 482314; field evaluations 61440; recorded application seconds 0.20129862700000001.

### complex seed0

Source `c157957b331dd546540c7cafff5c06679d13f985`; evidence `runs/initialization-diagnostic-replay/complex-0`.

No complete initial population was committed; no model was selected or assessed.

```text
scientific initialization failed before population publication
attempts=330 fitting_attempts=330 rows=1646 operations=23645 fields=1646 candidate_requests=18
 parallel candidate slot 12 failed
  training loss attempt 1 failed
   scientific numerical failure: complex required prediction
```

### complex seed1

Source `588a07b3d4f17d3857f84024409c42fb24d492e6`; evidence `runs/campaign-repair/complex-1`.

```text
c(0.76715087890625,0)
(0.76715087890625+(0)i)
```

Nodes 1, depth 1; fitting attempts 2048; rows 10240; charged operations 151830; field evaluations 10240; recorded application seconds 0.20299892999999999.

### complex seed42

Source `588a07b3d4f17d3857f84024409c42fb24d492e6`; evidence `runs/campaign-repair/complex-42`.

```text
c(0.76715087890625,0)
(0.76715087890625+(0)i)
```

Nodes 1, depth 1; fitting attempts 2048; rows 10240; charged operations 169700; field evaluations 10240; recorded application seconds 0.20501335500000001.

### tensor seed0

Source `37fcf20a9e55ae6a80a38f79014942f82a1eefab`; evidence `runs/campaign-initial/tensor-0`.

```text
(1.5 * x0)
1.5·x0
```

Nodes 3, depth 2; fitting attempts 582; rows 2328; charged operations 104716; field evaluations 2328; recorded application seconds 0.227140377.

### tensor seed1

Source `37fcf20a9e55ae6a80a38f79014942f82a1eefab`; evidence `runs/campaign-initial/tensor-1`.

```text
(1.5 * x0)
1.5·x0
```

Nodes 3, depth 2; fitting attempts 551; rows 2204; charged operations 99388; field evaluations 2204; recorded application seconds 0.22989690199999999.

### tensor seed42

Source `37fcf20a9e55ae6a80a38f79014942f82a1eefab`; evidence `runs/campaign-initial/tensor-42`.

```text
(1.5 * x0)
1.5·x0
```

Nodes 3, depth 2; fitting attempts 520; rows 2080; charged operations 95076; field evaluations 2080; recorded application seconds 0.227694536.

### gaussian seed0

Source `37fcf20a9e55ae6a80a38f79014942f82a1eefab`; evidence `runs/campaign-initial/gaussian-0`.

```text
normal_pdf(x0, 0.5, 1.25)
normal_pdf(x0, 0.5, 1.25)
```

Nodes 4, depth 2; fitting attempts 1343; rows 7951; charged operations 6995; field evaluations 7951; recorded application seconds 0.194215258.

### gaussian seed1

Source `588a07b3d4f17d3857f84024409c42fb24d492e6`; evidence `runs/campaign-repair/gaussian-1`.

```text
normal_pdf(((-0.125) + 0.625), x0, normal_pdf(x0, x0, 0.3125))
normal_pdf(−0.125 + 0.625, x0, normal_pdf(x0, x0, 0.3125))
```

Nodes 9, depth 3; fitting attempts 2048; rows 12048; charged operations 13780; field evaluations 12048; recorded application seconds 0.20162039500000001.

### gaussian seed42

Source `588a07b3d4f17d3857f84024409c42fb24d492e6`; evidence `runs/campaign-repair/gaussian-42`.

```text
0.164398193359375
0.164398193359375
```

Nodes 1, depth 1; fitting attempts 2048; rows 12238; charged operations 4059; field evaluations 12238; recorded application seconds 0.20093013600000001.

### oscillator-fit seed0

Source `37fcf20a9e55ae6a80a38f79014942f82a1eefab`; evidence `runs/campaign-initial/oscillator-fit-0`.

Accepted parameter values: `0.25,2,`. Charged operations 27032, fields 3270, recorded seconds 0.0010100770000000001.

### heat seed0

Source `37fcf20a9e55ae6a80a38f79014942f82a1eefab`; evidence `runs/campaign-initial/heat-0`.

Accepted parameter values: `0.25,`. Charged operations 5724, fields 689, recorded seconds 0.00044180400000000001.

### gaussian-fit seed0

Source `37fcf20a9e55ae6a80a38f79014942f82a1eefab`; evidence `runs/campaign-initial/gaussian-fit-0`.

Accepted parameter values: `0.5,1.25,`. Charged operations 5382, fields 702, recorded seconds 0.00065105899999999995.

## Interpretation and retained failures

The oscillator evolved only its acceleration expression; x'=v was prescribed.
Bindings were x0=t=0, x1=position, x2=velocity, x3/x4=the fixed initial shared
parameter slots1/1 in the structural campaign. Coefficient recovery was separate.
Held-out state values do not establish time-domain trajectory accuracy.

Complex seed0 overflowed during initialization; its final diagnostic replay records
330 loss/fitting attempts,1646 rows/fields and23645 charged operations, with18 candidate
requests. This is a failed uncommitted initialization, not a low-quality selected model.
The other complex seeds selected the real constant0.76715087890625 and failed recovery.

The first campaign application incorrectly treated ordinary nested budget exhaustion
as an experiment failure. The engine preserved its committed state. The driver was
repaired, a bounded CLI regression was added, and affected cases were replayed using
identical seeds/configuration. Original outcomes are in PRE_REPAIR.json; successful
original cases were reused. No settings were tuned using assessment data. The initial
launcher also found no /usr/bin/time; it never launched the application and its evidence
is retained under [`runs/launch-failure-no-time-utility/`](runs/launch-failure-no-time-utility/). Per-run RSS was not measured.

These selected structures, small budgets and deliberately narrow profiles do not
establish general superiority or solve D5's negative power/trigonometric/iteration
discovery cases. The tensor profile evolves a scalar scale of a prescribed tensor;
the complex evolutionary profile uses real coefficient occurrences. Native-operation
counts are declared work units, not internal LAPACK/libm instruction counts. Scalar
Gaussian counters record checked primal operations; coupled/tensor/complex objectives
also charge their explicitly implemented residual arithmetic. Timings and counters
remain source-pinned; later ownership/outcome/accounting fixes were not used to retune
or replace successful scientific results.

Full JSON records include every formula, canonical/pretty text, complexity, budgets,
raw result fields, source identities, executable hashes and selected raw-file hashes.
The compact checkpoints, campaign logs and qualification logs retained for this report are committed under `runs/` and `qualification/`; the complete original build trees and unselected engineering logs remain in ignored `out/` directories.
