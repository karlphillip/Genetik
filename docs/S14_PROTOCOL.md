# S14 Frozen Protocol

## Identity and datasets

Protocol identity: `s14-multivariate-rational-confirmation-v1`.
The manifest is stored at
`Genetik-data/benchmarks/s14-multivariate-rational-discovery/frozen-protocol/manifest.json`.
Its SHA256 and dataset SHA256 values are recorded with the evidence. The source SHA is
frozen to the implementation commit before campaign execution. The campaign runner
requires a successful `domain_validated` result for the exact manifest hash and checks
every dataset hash before creating the output directory.

There are six two-input tasks: mixed-xy rational, rational with xy and x² denominator,
quadratic-over-quadratic rational, near-pole rational stress, quadratic polynomial
control, and bivariate sine control. Each has one clean training track and three
independent 1%-of-training-target-RMS Gaussian-noise realizations (seeds 14011, 14017,
14023). Each training set has a separate clean 6-by-6 held-out grid and clean finite
extrapolation points. The near-pole training grid is restricted to `[-0.4,0.4]^2`;
extrapolation is a finite stress sample. No reference observation is undefined at a
listed point. The finite sample does not certify an entire extrapolation region.

The preflight references, grammar and all train/held-out/extrapolation observations are
validated before execution. The references are unavailable to candidate construction,
model selection and fitting. The held-out and extrapolation files are only read after a
model has been selected.

## Grammar and mathematical family

GA grammar has two variables and operators `+, -, *, /, powi, sin`; integer powers are
configured as in the manifest. Limits: 127 nodes, depth 16, 2,048 arithmetic
operations, constants bounded by magnitude 8, and the declared fit-parameter bounds
`[-8,8]`. Population 32, 512 candidate evaluations, 12,000 global training-loss
attempts, at most 16 generation attempts, one worker, and fixed seed per GA run.

Constructive S14 models use total-degree numerator and denominator degree at most two,
11 free parameters maximum, `Q(0,0)=1`, coefficient magnitude at most 8, 1,000 input
rows maximum, 16 MiB workspace, and 50,000,000 arithmetic and linear-algebra work
units. Expression limits are 127 nodes, depth 16, constant magnitude 8, and 2,048
arithmetic operations. QR+LM uses existing LM, at most 16 iterations, 32 objective
evaluations, 16 Jacobian evaluations, 16 MiB workspace, and 50,000,000 mathematical
operations. Combined QR and LM work is checked against the configured applicable caps.

Constructive degree families are enumerated in the implementation's deterministic
order and selected using training-only BIC-style scoring. No degree is chosen from the
reference equation.

## Conditions and run count

For each of 24 task/noise datasets:

- `constructive_qr`: one deterministic run.
- `constructive_lm`: one deterministic run.
- `ga_coordinate`: seeds 14101, 14103, 14107.
- `ga_lm`: the same three paired seeds.

Total: 192 runs. Identical deterministic constructive fits are not treated as
independent replicates. The GA's random seed is run-owned. The only algorithmic change
between GA conditions is the existing fitter selection. Constructive fitting uses a
restricted rational-family inductive bias and is reported separately from GA results.

## Supervision and measurements

Runs execute sequentially pinned to CPU 0, with 120 seconds per run, a 6 GiB process
RSS cap and 6 GiB address-space cap. The supervisor samples process-tree RSS every
20 ms and terminates a run that exceeds the cap; GNU `time` records isolated process
maximum RSS and CPU user/system time. The binary, manifest and data identities are
recorded in every output. Setup/data loading and command startup are within each
supervised wall allowance; total campaign wall time is also recorded.

Measurements include training/held-out/extrapolation RMSE and invalid counts,
expression and coefficients, selected degree, parameter count, QR rank/pivot ratio,
LM status/work, candidate/loss counts, mathematical and linear-algebra work, domain
assessment, stop reason, runtime and memory. The GA currently does not expose an exact
mathematical-operation count; this is recorded as unavailable rather than inferred.

## Frozen advancement criteria

- Exact clean identifiable rational fixtures: held-out RMSE at most `1e-8`.
- Noisy rational: held-out RMSE at most 2.5% of held-out target RMS and safe-region
  extrapolation RMSE at most 5% of extrapolation target RMS.
- Near-pole stress: extrapolation RMSE at most 10% of target RMS is descriptive only;
  invalid or uncertain domain results are reported, not converted into successful
  validity claims.
- No invalid predictions in declared safe prediction regions.
- QR+LM fixed-family training loss should not regress by more than 10% against QR-only
  to be considered a training-fit improvement; no extrapolation improvement is assumed.
- Non-rational controls assess approximation only, never symbolic recovery.

These criteria are frozen before running confirmation. Every failed, invalid, timed-out,
or incomplete run remains in the results.
