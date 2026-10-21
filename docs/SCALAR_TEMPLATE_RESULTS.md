# Scalar-template campaign and separate fitting evidence

Phase 6A, local GCC13/ARM64 Release. Source: the Phase 6A implementation descended
from accepted parser commit `d612446ae5f2fb5e1ebd91165b6e01e304b8cab5`;
final implementation/qualification commit identities accompany the completion
record. No hosted qualification is claimed. See SCALAR_TEMPLATE_QUALIFICATION.md.

## Frozen protocol

The campaign wrote [PROTOCOL.txt](scalar-template-results/PROTOCOL.txt) before
search. It ran once with no configuration adjustments or retries, serial CPU,
seeds `0,1,2,3,42` sequentially. Population 128, elites 2, rank selection, crossover
0.9, mutation0.35, generation ceiling 2000, candidate/loss ceilings 10000 each,
training MSE stop exactly 0. No continuous fitting or continuous sampling.
Constants {-2,-1,0,1,2}; enabled/allowed operators add/sub/mul, uniform feasible
operator selection. Initial max depth 4, mutation max depth 3, proposal limit 8,
grow probability 0.65, full-tree probability 0.5. Local nodes 9/depth 4/abs constant 2;
full nodes 13/depth 6/abs constant 2; arithmetic allowance SIZE_MAX (18446744073709551615 on this 64-bit run),
with two frozen multiplications deducted for the effective local allowance;
materialization allowance 1048576 bytes. No target expression/skeleton enters the
random hole population. The ordinary source terminal at the selected occurrence
is discarded.

The full input schema is x0=v, x1=T; the evolving expression permits only index 1.
Frozen full model is `(((-1.0) * HOLE) * x0)`. Four frozen nodes/two ancestor levels
are outside the evolving expression. Independent ordinary C++ arithmetic produces
`-(T*T+1)*v`; expression evaluation is not its sole oracle.

All rows were prepared before searching, Cartesian order **v outer, T inner**:

- Training: v,T each {-2,-1,0,1,2}, 25 rows.
- Interior: v={-1.5,-0.5,0,0.5,1.5}, T={-1.5,-0.5,0.5,1.5}, 20 rows.
- Exterior: v={-3,0,3}, T={-3,3}, 6 rows.

Recovery is separately reported as maximum absolute error <=1e-6 on each set.
Final models and representative were frozen before assessment. Representative order
is full training MSE, full nodes, faithful full text, then first declared seed.
All dataset files, traces and outputs are retained in
[scalar-template-results](scalar-template-results/PROTOCOL.txt), with SHA256SUMS.
The program refuses an existing output directory to avoid overwriting evidence.
The retained run preceded the final direct-validator constant-bound guard and the
smoke-only seed-label correction. All campaign expressions already met that bound;
neither correction changes this protocol's productions, random draws, objective or
accepted models. The recorded run was not overwritten or repeated. Its elapsed times
remain observations of that implementation stage, not final-revision benchmarks.

## Every seed

Every initial best full-model MSE was 2. Every final MSE was 0, with **zero maximum
error and zero invalid predictions on all three declared sets**. No initialization
discoveries occurred. All stops were `exact_training_fit`. None exhausted its
allowance or abandoned a partial generation. Search timing includes initialization,
excludes assessment/file-output work, and is an observation, not a benchmark.

| Seed | Discovery / published generations | Actual loss calls | Rows visited | Unused calls | Hole nodes/depth | Full nodes/depth | Seconds |
|---|---:|---:|---:|---:|---:|---:|---:|
|0|1 / 1|256|6400|9744|7 / 4|11 / 6|0.003713|
|1|3 / 3|512|12800|9488|5 / 3|9 / 5|0.006642|
|2|9 / 9|1280|32000|8720|7 / 4|11 / 6|0.016738|
|3|1 / 1|256|6400|9744|7 / 4|11 / 6|0.002622|
|42|9 / 9|1280|32000|8720|7 / 4|11 / 6|0.015064|

Attempted generations equal published generations for these runs. With no fitter,
candidate requests equal actual loss calls; all rows were visited. Total search work
is 3584 calls / 89600 rows. No baseline/fitting work is included in those totals.
Post-search metrics are separate calls to `measureAll`, not training work.

Faithful exported evolving expressions (no simplification):

|Seed|Evolving expression|
|---|---|
|0|`(2.0 + ((x1 * x1) - 1.0))`|
|1|`(1.0 + (x1 * x1))`|
|2|`((2.0 + (x1 * x1)) + (-1.0))`|
|3|`(((x1 - 0.0) * x1) + 1.0)`|
|42|`((1.0 + (x1 * x1)) - 0.0)`|

The complete exports are retained verbatim in [search.txt](scalar-template-results/search.txt).
The fitting-selected representative is seed 1:

```text
(((-1.0) * (1.0 + (x1 * x1))) * x0)
x0=v, x1=T
```

|Seed|Mutation proposals / changes|Crossover proposals / changed children|Structural rejections|Mutation / crossover fallbacks|
|---|---:|---:|---:|---:|
|0|60 / 50|61 / 82|12|0 / 0|
|1|140 / 128|172 / 274|9|0 / 0|
|2|436 / 335|579 / 854|118|0 / 0|
|3|35 / 29|63 / 106|8|0 / 0|
|42|426 / 336|608 / 842|135|0 / 2|

These are measured sample recoveries, not claims of unique structure or proven
identity over every binary64 input. Deterministic engineering tests do not require
any stochastic seed to succeed.

## Independent full-objective check

For observations `(v,F)=(1,-1),(2,-6)` with g(T)=c, the full-model MSE is
`((c-1)^2+(2*c-6)^2)/2`. Differentiating yields `5*c-13=0`, hence c=2.6
and minimum MSE 1.6. Dividing observations by v and fitting an unweighted mean would
incorrectly give c=2. The bounded selected fitter (512-call allowance, minimum step
1e-10) returned c=`2.5999999940395355`, loss=`1.6000000000000001`, using 71 calls.
Coefficient tolerance 1e-7 and loss tolerance 2e-14 allow the finite-loss plateau
without claiming a uniquely recoverable binary64 optimum. The accepted full model
recomputed to its matching checked loss; frozen coefficient -1 remained bitwise
unchanged. Focused tests also keep a frozen -9 outside adjustable bounds[-8,8].

## Separate supplied-structure fitting

This fixture is not the discovery population. It supplies `g(T)=a*T+b`, starting
at a=b=1, independently generated targets a=0.5,b=1.5 and the same original v/T
sample locations. Only two inserted constant occurrences are selected. Bounds[-8,8],
initial step 1, minimum 1e-8, sweep limit 128, proposal limit 8192, loss ceiling 512,
no loss-threshold early stop. Initial full-model MSE 1.5. The accepted result is:

```text
hole:  ((0.5 * x1) + 1.5)
model: (((-1.0) * ((0.5 * x1) + 1.5)) * x0)
```

Final full loss 0; 112 objective calls / 2800 rows, `minimum_step` termination.
Training/interior/exterior maximum errors are all 0. Scaffold bits and structure
are unchanged. This is coefficient recovery for a supplied identifiable structure,
not evidence that fitting was needed for the no-fit discovery campaign.
A controlled `a+log(c)` fixture retains accepted a=1,c=1 after a worsening probe
and a domain-invalid c=0 probe, using the same selected-coordinate kernel.

## Reproduction

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/release -j2
out/build/release/genetik_scalar_template_campaign --output out/runs/templates/reproduction --threads 1
```

Use a fresh directory. The original run is retained separately; numerical seeded
repeatability is qualified by exact revision, ordered rows/configuration, compiler,
standard/runtime math libraries, execution mode and floating-point environment.
