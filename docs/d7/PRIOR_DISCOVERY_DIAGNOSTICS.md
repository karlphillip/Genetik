# D5/D6 discovery-limit evidence retained for D7

This is a bounded reading of the committed D5/D6 records. It does not rerun
campaigns or infer a single causal defect from aggregate recovery counts.

## D5 scalar discovery

The frozen campaign reports 17/24 held-out recoveries. Fractional power and
trigonometric tasks each report 0/3. The recorded D5 grammar did include `Pow`
with exponent metadata containing 1.7 for the fractional-power task and `Sin`
for the trigonometric task. The latter target was
`1.5*sin(0.75*x+0.25)`. Thus the records do not show a simple absence of the
named target operator. They also do not prove that the entire useful structure
was reachable under the finite expression bounds.

In the retained result rows, all three fractional-power runs stopped at the
training-loss allowance, with training MSE approximately 0.00605–0.05785 and
no held-out matches. The three trigonometric runs stopped at loss budget for
two seeds and evaluation budget for one; training MSE was approximately
0.00608–0.07993 and none met held-out recovery. These observations establish
that the selected candidates failed both training/reference fit and held-out
criteria. They do not separate structure discovery from coefficient-fit
quality or show whether a larger budget would help.

The protocol allowed bounded continuous constant fitting. Candidate and
training-loss budgets were finite. The records do not expose a controlled
ablation that independently varies the grammar, initialization, fit kernel,
domain invalidity, and objective. No causal attribution to one of those factors
is supported.

## D6 scientific discovery

The D6 campaign remains 4/12 held-out recoveries: tensor 3/3, Gaussian 1/3,
oscillator 0/3, and complex 0/3. All tensor successes have generation 0 and a
three-node candidate, so those runs are fitted/valid initial-population
solutions; they are not evidence of successful evolutionary structure
discovery. The measured D6 held-out success count is preserved unchanged.

All three fixed-structure fitting controls recovered their reference fixtures.
This demonstrates that those supplied structures and fixtures were fit under
the recorded setup; it does not establish general constant-optimization
success during structure search.

The oscillator discovery candidates stopped at the finite evaluation budget.
Their held-out residuals were nonzero, while the fixed-structure control
recovered its reference. This separates fitting success from the failed
discovery outcomes but does not identify whether structure, grammar, objective,
or resource allocation was the cause.

The complex seed-0 record is a retained initialization failure: it failed
before population publication with a `complex required prediction` numerical
failure. Seeds 1 and 42 published one-node models with nonzero training and
held-out losses. General complex constant fitting is explicitly outside D6
support. The seed-0 failure is not counted as an invalid candidate that the
evolutionary search successfully handled.

## Evidence boundary

The D5 records and protocol are under `docs/d5-results/benchmarks/`; the D6
campaign, protocol, initialization diagnostics, and full qualification records
are under `docs/d6-results/`. Their source identities remain in those records.
The D7 benchmark is a new, independently frozen collection. It must report
accuracy, structure recovery, held-out behavior, invalid runs, resource use,
and fixed-structure fits separately. These historical outcomes are not
replaced by D7 measurements.
