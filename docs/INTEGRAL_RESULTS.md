# Bounded prescribed-integral discovery and independent fitting

Implementation measured: `2ed3724f3c034d2338fa86dd4d20e1d6801c8f56`.
Local serial Release on Jetson ARM64/GCC13.3/libstdc++/libm2.39. The complete
protocol was written before search; selected raw records and data are retained in
[integral-results/](integral-results/). Five runs only, no tuning, retries, injected
integrand or integrand labels. Timings include initialization but exclude reference
generation, file output, assessment and separate fitting. No development builds or
qualification suites ran during these measurements. These single millisecond
observations are not speedup measurements.

## Frozen protocol

Model F(x0)=integral from0 to x0 of g(x1) dx1. External x0=x, appended local x1=t;
grammar variables ONLY original slot1. Prescribed bounds/binding are not discovered.
Training x={-2,-1.5,-1,-.5,.5,1,1.5,2}, labels x*x+x by ordinary exact dyadic
arithmetic; no additional function/integrand labels. Zero is omitted because the
model structurally imposes F(0)=0.

Seeds0,1,2,3,42, population128/elites2,10000 actual objective attempts and candidate
calls,2000 generation attempts. Integrand9 nodes/depth4, constant bound2, discrete
{-2,-1,0,1,2},+,-,* uniform feasible operators, continuous0/fittingOFF. Initial
depth4/mutation depth3, grow.65/full.5/proposals8, crossover.9/mutation.35, rank
selection. Policy GK7/15 abs/rel1e-10,4096 samples,256 panels, depth32; expression
operations/node visits/binding validations/quad units each1000000; planned scratch
8388608, model nodes65536, externals65536. Local problem worst-case work limits and
all remaining configuration are in PROTOCOL.txt and SCALAR_INTEGRALS.md.

Stop training MSE<=1e-20. Select by training MSE, integrand nodes, faithful text;
stable final tie retains first declared seed. Freeze every selected model and the
overall representative BEFORE computing assessment errors. Representative **seed3**:
its training MSE is slightly lower from binary64 operation grouping. No epsilon tie
or assessment-driven replacement is applied. Exact exported structure is retained.

## Every seed, including initialization discoveries

All models have5 integrand nodes/depth3. Stop is training_loss_threshold in all
cases. Published counts exclude the initial population (generation0). No abandoned
preparation, invalid sample, numerical failure, resource stop, nonconvergence,
resolution failure or cancellation occurred. All run integrals reached estimated
convergence using their first panel; no adaptive splits were needed. Initialization
successes are preserved faithfully, not discarded to manufacture later evolution.

| Seed | Initial MSE | Final MSE | Discovery/published generation | Attempts | Unused | Runtime ms | Faithful integrand |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
|0|.30989583333333315|6.1923981747793495e-33|1|256|9744|7.479377|`(1.0 + (x1 * 2.0))`|
|1|6.1923981747793495e-33|6.1923981747793495e-33|0|128|9872|3.265115|`(1.0 + (x1 * 2.0))`|
|2|6.1923981747793495e-33|6.1923981747793495e-33|0|128|9872|3.243643|`((x1 * 2.0) - (-1.0))`|
|3|6.1664649660111566e-33|6.1664649660111566e-33|0|128|9872|3.212665|`((1.0 + x1) + x1)`|
|42|1.3828125|6.1664649660111566e-33|1|256|9744|6.272847|`((x1 + 1.0) + x1)`|

Full representative export (not integral parser syntax):

```text
integral[external=1,local=x1,lower=0.0,upper=x0,integrand=((1.0 + x1) + x1)] policy[gk7/15,abs=1e-10,rel=1e-10,samples=4096,panels=256,depth=32,expression_ops=1000000,node_visits=1000000,bindings=1000000,quadrature_ops=1000000,workspace=8388608,model_nodes=65536,external_inputs=65536]
```

|Seed|Integrals|Integrand samples|Expression operations|Node visits|Quad units|Mutation proposals/changes|Crossover proposals/changes|Rejections|
|---:|---:|---:|---:|---:|---:|---|---|---:|
|0|2048|34816|42160|123232|250494|56/50|61/74|5|
|1|1024|17408|17272|54000|125210|0/0|0/0|0|
|2|1024|17408|23120|65696|125344|0/0|0/0|0|
|3|1024|17408|26928|73312|125468|0/0|0/0|0|
|42|2048|34816|38216|115344|250606|47/40|56/74|1|

Bounds=2*integrals, complete panels=integrals; samples=17*integrals. Binding
validations73728 for seeds0/42,36864 otherwise; loss arithmetic6400/3200 respectively.
Peak planned payload12760 bytes in every run, not a measured process-memory peak.
All mutation/crossover fallbacks0. Full counters, model exports and committed
improvement traces against actual objective attempts are retained in search.txt.

## Frozen-model assessment (all five seeds)

Reference integral x*x+x and integrand2*t+1 use ordinary arithmetic on the declared
dyadic points, never production quadrature as the target oracle. Training-set
integrand values are post-selection assessment only. Interior x/t={-1.75,-1.25,
-.75,-.25,.25,.75,1.25,1.75}; exterior={-4,-3,3,4}; also inspect0.

Every seed: training integral maximum error2.2204460492503131e-16;
interior8.8817841970012523e-16; exterior0; zero0. Integrand maximum error0 on every
set. All meet the predeclared1e-6 recovery reporting threshold for BOTH quantities.
No failed/invalid predictions. Maximum estimated error is6.6613381477509392e-14
training,5.3429483060085646e-14 interior,2.2204460492503131e-13 exterior,0 at0.
Each nonzero point uses17 samples; zero uses none. These estimates are heuristic,
not guaranteed true-error bounds. Complete per-seed assessment work is retained.

These results show agreement on supplied samples in a fixed integral model. They
neither discover the integral operator/bounds nor establish a unique global
integrand. Controlled mutation/crossover and failure fixtures qualify actual
variation independently of generation-zero campaign recoveries.

## Separate supplied-family fit

Supply g=a*x1+b with independent coefficient occurrences starting(1,1), train on
F=.75*x*x-.5*x at the same eight nonzero inputs. Fixed bounds0..x0 imply F(0)=0;
this is not an arbitrary primitive with an unknown additive constant. Integrand
limits9/depth4/constant bound8. Coordinate settings frozen before execution:
[-8,8], step1/min1e-8,128 sweeps/8192 proposals,512 complete objective attempts,
no early threshold. All probes use the same full integral MSE.

Initial MSE **4.564453125**. Final faithful integrand:

```text
((1.5 * x1) + (-0.5))
```

Recovered coefficients(1.5,-.5), final MSE **3.0911175607415135e-32**, independently
remeasured against the accepted expression.115 objective attempts (397 unused),
3 accepted changes, termination minimum_step.920 accepted integral predictions,
1840 bound attempts,15640 samples,31280 expression operations,80040 node visits,
33120 binding validations,113788 quad units,2875 loss arithmetic operations;
920 complete panels, no splits/failures. Peak planned payload12728 bytes. Neither
bounds nor binding changes; all integrand structure/occurrences remain intact.

Post-fit maximum integral error: training4.4408920985006262e-16,
interior2.2204460492503131e-16, exterior1.7763568394002505e-15. Integrand errors0.
All recover within1e-6. Assessment follows freezing, and its work is separate.
No bit-exact coefficient recovery is promised for general numerical quadrature.
The original app also made two direct reporting measurements (initial score and
accepted-score verification) outside these115 fitter attempts. Each had8 integrals
and a4096-sample per-integral ceiling; those are excluded from the fitter allowance.
After this recorded experiment the reporting-only6e8322e change removes the duplicate
initial measurement and reports verification counters separately. The later afbb03d
binding-buffer fix changes storage only. New bounded smoke verification preserves
115 attempts/coefficients/score; the original research runs/runtimes remain unchanged.

## Reproduction

```sh
out/build/release/genetik_integral_campaign --output out/runs/integral-new --threads 1
python3 scripts/generate_integral_references.py --output out/runs/integral-refs-new.json
```

Targets/data generation consume no search RNG. Same-build/runtime nonbinding
1/2/4-worker contracts are tested separately; seeds remain sequential. See
[SCALAR_INTEGRALS.md](SCALAR_INTEGRALS.md) for outcome/resource mapping, the missed
narrow-feature limitation, proper endpoint policy and work definitions, and
[INTEGRAL_QUALIFICATION.md](INTEGRAL_QUALIFICATION.md) for executed gates. After
this implementation the next checkpoint is combined calculus hosted qualification
and final development-delivery review. No additional mathematical phase is planned.
