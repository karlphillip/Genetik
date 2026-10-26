# D7 scalar symbolic regression benchmark protocol

Protocol status: frozen for the primary six-task matrix after neutral
engineering preflight; commit this file before starting any target-task run.
No D7 target benchmark has been run. A result report must preserve every seed,
timeout, invalid model, and unsupported framework/task cell.

## Host and acquisition

- Host: NVIDIA Jetson AGX Orin, Ubuntu 24.04.4 LTS, Linux 6.8.12-1021-tegra,
  aarch64 Cortex-A78AE, 8 online cores (0–7). Compiler: GCC 13.3.0. Python:
  3.12.3. Existing CMake: 4.4.4 workspace build and 3.28.3 system build;
  exact 3.16.0 is reserved for the separate engineering gate.
- No cgroup CPU quota or memory maximum is configured. Benchmark processes
  will run serially, pinned to CPU 0, with one worker/thread. The process-tree
  supervisor polls summed RSS every 50 ms and sends SIGTERM above 6 GiB; shared
  mappings are conservatively counted once per process. Preserve an exceeded
  cap as a failed run. Record actual RSS, CPU time, process/thread counts, and
  enforcement outcomes.
- Primary sources are official Git checkouts, isolated below `out/tmp/d7/`.
  PySR v2.7.2 (`04d550c446db6594f399e8686831c1bee801975c`, Apache-2.0) uses
  SymbolicRegression.jl v2.7.2 (`53d0dd2c6138ffed6208cffbe8396149d22320ac`,
  Apache-2.0) and upstream requires Julia 1.10.0 or 1.10.3. The qualified
  acquisition pins Julia 1.10.3 on aarch64. Record exact resolved Julia,
  Python, and transitive package versions and preserve lock/manifest files.
  PyOperon v0.6.2
  (`adf7e0e34041238e22e7ff7cd4330451de97bb36`, MIT) pins its Operon core to
  `fad10f087c592812b1a9ecfabce669f37bf459f7`; it also pins native dependencies
  in its official script. Record every resolved revision and build tool. Do
  not label the pinned core commit as an Operon release tag.
- PySR is built in `out/tmp/d7/venvs/pysr` and uses the private Julia depot
  `out/tmp/d7/julia-depot`. PyOperon must be built in its own venv/prefix under
  `out/tmp/d7/`; no system/global dependency installation is allowed. Source
  checkouts are immutable; any build-script adaptation is a separately
  retained local patch and must not alter the upstream checkout. Its official
  dependency script hardcodes Clang and an x86-64-v3 core preset; the isolated
  aarch64 build adaptation uses GCC 13, two build jobs, and a generic
  `-march=armv8.2-a` core flag override. The original incompatible configure
  failure remains retained in the D7 setup log.
- If an official implementation cannot be installed or run under these
  constraints, mark it unsupported and preserve the acquisition failure. Do
  not substitute a different version or report external-machine measurements.
- PySR Python dependencies are pinned in
  `docs/d7/environments/pysr-python-requirements.txt`; the PythonCall and
  SymbolicRegression.jl project/manifest and Julia runtime archive hash are
  retained alongside it. PyOperon Python dependencies and every official
  native source revision are pinned in the adjacent environment files. The
  built PyOperon wheel SHA256 is
  `0474159102f15efe3dc5a8b2a084d11565aee9fd23f0d42a096767aa49b5a357`.
  Its upstream x86-64 compiler preset required a local, separately retained
  GCC/aarch64 build adaptation; the official source checkout is unchanged.

## Dataset identity and splits

The frozen task IDs, in order, are `polynomial_x3`, `rational_x2`, `trig_sine`,
`exponential_decay`, `fractional_power_positive`, and `kinetic_energy_mv2`.
The targets are respectively `x^3-2*x+1`, `(x+2)/(x^2+1)`,
`2*sin(1.5*x+0.3)`, `3*exp(-0.7*x)`, `3.5*x^1.7` for `x>0`, and
`0.5*m*v^2`. No target equation is supplied to a search or used as an initial
candidate. These are structure-discovery tasks; any fixed-structure fit is
reported in a separate table and never counted as discovery.

Dataset generator: `docs/d7/benchmark_data.py`, Python 3.12 / NumPy version
recorded in the result manifest. It uses `SeedSequence(20261026).spawn(12)`
to derive separate task/train and task/held-out `PCG64` streams in task order.
One-variable tasks use 160 train and 512 held-out rows; multivariate
energy uses 256 train and 1024 held-out `(m,v)` rows. Domains are: polynomial
and rational `[-2,2]`; trig `[-pi,pi]`; exponential `[-2,2]`; fractional power
`[0.1,3]`; energy `m in [0.5,10]`, `v in [0.1,8]`. Split streams are separate
and their arrays, generator version, and SHA256 hashes are retained. The
assessment target values are computed independently from the stated formulas.

Each task has two fixed tracks. `clean` trains on exact target values.
`noise01` adds independent Gaussian training noise with standard deviation
0.01 times the RMS of the noiseless training targets, generated with
PCG64(`20261027`); held-out values remain noiseless. Noise is not regenerated
per search seed. Search seeds are 0, 1, and 42 for each framework/task/track.
All runs share byte-identical training and held-out arrays.

No SRBench task is included in this frozen matrix. The upstream benchmark
repository is GPL-3.0 and its ground-truth catalog combines Feynman and ODE
Strogatz datasets; task-level source/redistribution terms and the exact local
dataset snapshot have not been verified for this release. The primary matrix
is already frozen and independently generated. A later SRBench subset would
require task-level license review, fixed dataset hashes, and a protocol
amendment committed before those tasks are run. Sources: [SRBench repository](https://github.com/cavalab/srbench)
and [dataset catalog](https://cavalab.org/srbench/datasets/).

## Grammar and search-space accounting

- Variables are exactly the task inputs. Requested scalar operator families
  are binary `+`, `-`, `*`, `/` and unary `sin`, `exp`, `log`. Powers must be
  represented through available arithmetic; there is no hidden fractional
  power operator. A framework/task is unsupported if this grammar cannot be
  expressed faithfully by its public configuration.
- Maximum size is 25 expression nodes and maximum depth is 8 where the API
  exposes both. Record the closest actual limits otherwise. Genetik uses
  strict checked arithmetic, rejecting zero division and nonpositive `log`;
  protected arithmetic differences in other tools are recorded and invalid
  predictions are counted, not silently clipped.
- All frameworks minimize mean squared error on the same training rows.
  PySR uses its public L2 loss and constant optimizer as configured below.
  Operon uses its public MSE criterion with coefficient optimization enabled
  only if the same policy can be configured without a free affine model.
  Genetik's optional bounded constant fitter is enabled and its complete
  charged loss attempts are recorded. These fitting algorithms differ; this
  comparison does not assert identical coefficient optimization.
- Genetik's fixed configuration is population 128; expression limits 25 nodes,
  depth 8, and absolute constants at most 8; continuous terminal constants
  uniform on `[-8,8]`; coordinate fitting enabled, capped at 64 loss attempts,
  32 sweeps, and 2,048 proposals per candidate fit. One worker. Search
  initialization, proposals, and fitting are all inside the fit wall budget.
- PySR's fixed configuration is one serial population of 128, `niterations`
  1,000,000 with its 120-second fit timeout, 25 maximum size, depth 8,
  `L2DistLoss()`, linear loss scaling, no parsimony penalty, Float64, and the
  exact common operator lists. Constant optimization is enabled with upstream
  BFGS defaults except the recorded two restarts, 10,000 function-call cap and
  0.14 optimize probability. PySR's constant range/policy is not identical to
  Genetik's bounded coordinate fitter.
- PyOperon's fixed configuration is population/pool 128, 1,000,000 maximum
  generations, 1,000,000,000 maximum evaluations, 25 maximum length, depth 8,
  initialization length 10/depth 5, one thread, and a 120-second time cap.
  `c2` is the search objective (monotonic with MSE on the same fixed target
  vector); final model selection minimizes training MSE. Its Levenberg–Marquardt
  constant optimizer runs 10 iterations with local search and Lamarckian
  retention enabled. Model scale/intercept terms are disabled. Constant bounds
  and fitting mechanics differ from the other implementations.
- Constant magnitude limits and initialization differ by API. Record them
  explicitly in each run manifest. No model is seeded with a target formula,
  scaffold, or target-specific constant.

## Resources, timing, and budgets

One process at a time; CPU affinity is core 0 and worker count is one. Every
target run receives an externally enforced 120-second total process wall cap
and 6 GiB process-tree RSS ceiling. The external cap includes adapter import,
compilation/JIT, search initialization, proposals, and coefficient fitting;
framework-native timeout settings are retained but are not treated as the
enforcement mechanism. The budget is fixed by this protocol, not by target
recovery. Each available framework completed a non-target linear smoke
preflight (64 rows, SHA256
`333efef3e9a46f2ffc411d7d2f477cbbe4065ef449b8cc3d06dac43d8be6039f`). The
preflight adapter timeout was 60 seconds; its outer cold-start allowance was
180 seconds. No preflight result enters the benchmark. The accepted PySR
preflight used `deterministic=True`; earlier attempts and their
failures/configuration differences remain retained. Genetik completed in
3.30 s wall / 2.65 CPU s / 0.0043 GiB peak RSS and found zero training MSE.
PyOperon completed in 62.51 s / 52.68 CPU s / 0.140 GiB, with 60.03 s
reported fit time. The accepted deterministic PySR neutral run completed in
126.42 s wall, including 7.65 s Python-side import, with 117.48 s inside
`fit()`, 104.09 CPU s, and 0.908 GiB peak RSS. Two earlier runs completed in
126.77 and 126.60 s with fit times 118.11 and 117.57 s. PySR therefore has
substantial Julia/JIT overhead inside its fit call; the 120-second outer cap
still governs target runs and every over-budget run is retained as timeout.
All cold-start and fit measurements are retained in `out/logs/d7/preflight/`;
there is no claim that PySR's native timeout alone enforces the common wall
budget. A framework that cannot complete its preflight is unavailable for the
target matrix unless the cause is repaired without inspecting target outcomes
and the protocol revision is committed first.

PySR iterations, Operon generations and Genetik generations are not treated
as equivalent work. Capture wall time, process CPU time, peak RSS, proposal or
objective-call counters where their definitions are available, and disclose
non-comparable counters. Include constant-fitting work in wall/CPU time.
Capture initialization, compilation and warm-up overhead separately and also
report cold-start elapsed time. The supervisor stops a run at 120 seconds and
preserves any emitted best-so-far candidate and timeout state. The PySR Python
process did not emit a final candidate until the full fit call completed in
preflight, so any externally stopped PySR target is reported as an invalid or
missing model while its native checkpoint and raw logs remain available.

## Shared assessment and reporting

An independent scorer, `docs/d7/assess_results.py`, evaluates canonical model
exports on frozen train and held-out arrays and independently computes train
RMSE, held-out RMSE, maximum absolute error, relative error where target
magnitudes permit it, invalid prediction count, expression node count/depth,
wall time, CPU time, peak RSS, and framework-native objective counts. A result
that cannot be parsed by the shared evaluator is retained as invalid with its
raw output. Recovery means the held-out RMSE is at most `1e-6 * max(1, target
RMS)` for clean data and at most `0.02 * target RMS` for the noisy track;
this operational criterion is not proof of identity. For known formulas,
attempt symbolic equivalence only when the framework export can be parsed into
the documented symbolic subset and domain assumptions are stated. Numerical
agreement on finite samples is approximation, not universal equivalence.

Publish all three per-seed rows, success count, median and interquartile range
for numeric metrics, and timeout/invalid/unsupported counts. Do not report a
best seed as typical performance. Report exact recovery, approximation,
structure discovery and fixed-structure fitting separately. No competitor
performance claim is made until all qualified runs for a task/track finish.
