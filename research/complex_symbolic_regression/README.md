# Autonomous complex symbolic regression

Base `1344982fe798108abf2993ba204cd399d93ab947` exposes complex evaluation of
real-coefficient expression structures and `fitComplexCoefficients`, which
optimizes separate real and imaginary coordinates for each constant. It has no
ordinary-search adapter joining those pieces. The new public
`complexSymbolicRegression` entry point uses the existing training-only
structure search, then refines the best structure and bounded Pareto frontier
with complex coefficients. The real-coefficient search path remains the base
arm. Arbitrary symbolic fit callbacks are not used here.

## Frozen comparison

The fixed study is `study.json`; `frozen-v5/manifest.json` binds the spec,
runner, source, public API header, binary, and synthetic rehearsal. Effective
CPU, memory, affinity, and inherited rlimit ceilings were read before selecting
the per-invocation 60 CPU second, 90 wall second, 4 GiB address-space, two-CPU
limits. The actual outer runner executed both synthetic cases through the same
CLI, adapter, spec, and binary before freezing. Both completed and fitted the
synthetic complex constant. The six target runs then used that frozen package.
All six completed; each arm was capped at 1,312 candidates and 15,744 training
row evaluations. The candidate arm spent at most 135 additional fitter
objective evaluations per run. No target topology, target constants, or pure
imaginary training input was supplied to either autonomous arm. Seed 101 was a
development pilot and was excluded before the final three-seed protocol.

The generator uses twelve fixed complex training inputs and six disjoint held-
out complex inputs, listed in the spec. Targets are `a*z+b` with genuinely
complex `a,b`, and `a*cos(b*z)` with genuinely complex `a,b`. The supplied
generic topologies are reported only as diagnostic controls. Strict structural
recovery requires independently adjustable scale and offset for the affine
target, or independently adjustable amplitude and frequency for the cosine
target. Operand swaps and the even-cosine sign symmetry are accepted; exact
target parameter values are assessed separately by held-out prediction.

## Results

Held-out RMSE is computed from squared complex modulus. The base and candidate
arms receive identical search seeds, grammar, and work caps. Candidate
search structures match the base best structures in every run; gains therefore
come from complex coefficient refinement and choosing among the searched Pareto
topologies. The diagnostic fit is not counted as autonomous recovery.

| Case | Seed | Base expression | Base RMSE | Candidate topology | Candidate RMSE | Fitter objective evaluations | Full parameterized structure recovered |
|---|---:|---|---:|---|---:|---:|---|
| affine | 401 | `((x0 + (cos(1.0) * x0)) - cos((-1.0)))` | 0.619647 | same | 4.64e-16 | 40 | yes |
| affine | 503 | `(x0 - cos((-1.0)))` | 0.647161 | same | 0.488034 | 18 | no; fixed unit scale |
| affine | 601 | `(x0 - cos(1.0))` | 0.647161 | same | 0.488034 | 9 | no; fixed unit scale |
| nonlinear | 401 | `cos((-1.0))` | 0.453315 | `0.0` | 0.301861 | 27 | no |
| nonlinear | 503 | `(cos(cos(cos((-1.0)))) * cos(x0))` | 0.308049 | same | 0.191777 | 135 | no; fixed unit frequency |
| nonlinear | 601 | `cos((-1.0))` | 0.453315 | `0.0` | 0.301861 | 27 | no |

All supplied-topology diagnostics fit successfully. Affine diagnostic held-out
RMSE was `1.03e-15`; nonlinear diagnostic RMSE was `1.09e-15`. The fitted
coefficients matched the target to floating-point precision. This confirms the
complex fitter can solve both targets when supplied a compatible structure,
while the autonomous structure search remains the limiting step. The full
parameterized topology was recovered in 1/6 runs. Five candidate results show
prediction improvement with restricted subfamily structures, not full
structural recovery: two affine runs fixed the scale to one, one nonlinear run
fixed the frequency to one, and two nonlinear seeds selected constants.

Raw records for every seed, expression, learned coefficient, work counter,
failure status, and held-out result are in
`results/target-runs-v5.json`. Earlier outputs are retained to document the
initial matcher revisions. In v4, seeds affine/503 and affine/601 exposed a
temporary-iterator bug in the revised classifier and exited with signal 11;
both failures are preserved in `results/target-runs-v4.json`. After fixing the
classifier, v5 reran every frozen target arm using the same symmetry-aware
full-parameter criterion, seeds, inputs, grammar, and work caps. There were no
v5 seed failures. An autonomous success requires both
the full parameterized structure and held-out accuracy.
