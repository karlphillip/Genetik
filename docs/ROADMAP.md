# Genetik roadmap

The baseline and fitness/publication contracts are implemented and verified. The bounded expression value and strict CPU evaluator are implemented; the next application milestone is structure evolution and the polynomial demonstration. Fitting and later experiments remain planned capabilities. See [BASELINE.md](BASELINE.md) for evidence and API migration.

## 1. Baseline, C++20 and first hardening — complete

Acceptance: clean starting-state/environment inventory; unchanged build and bounded example exercised; transitive C++20; tests that fail in Release and integrate with CTest; explicit run-owned seed/state across initialization, selection, crossover and mutation; regression tests; supported local sanitizer run; public-header/downstream consumer checks; documented compatibility. All satisfied on the current Jetson/GCC 13 platform. Wider platform qualification remains milestone 6.

## 2. Evolutionary contracts — fitness/publication complete; parameter fitting pending

Implemented: explicit run-owned randomness; private scalar genomes and enforced invalidation; Unevaluated/Valid/Invalid results with finite double scores; independent objective direction; rank selection excluding invalids; staged evaluation, ranking and committed-best preparation; no-throw publication of a viable population; nested operational errors; accurate attempted-evaluation accounting without refunding failed work. Fixed objective/data are assumed for each run. Deterministic fault-injection tests verify committed state preservation and consumed RNG state. See BASELINE.md for the exact scope and compatibility changes.

The expression value/evaluator in milestone 3 is complete; next implement its candidate/variation bridge and the polynomial demonstration. Reusable bounded real-valued variation and parameter/constant fitting remain pending: support validated initialization/mutation bounds and bounded numerical optimization independent of ASCII. Raw MSE can now be minimized using the explicit direction; do not confuse score validity with optimization direction.

Future execution work still needs an evaluation/time budget, cancellation semantics and a complete result object (seed/configuration/work/time/stopping reason). No such subsystem was added to this focused milestone. Any future evaluation budget must charge attempted calls, including Invalid and throwing evaluations, without refunding discarded generations. Preserve the serial publication boundary and no-implicit-retry policy. Constant fitting must additionally account for its own attempted work as specified below.

## 3. Bounded mathematical expressions — value/evaluator complete; variation next

Implemented: a dedicated Expression value with typed postfix nodes for variables, finite double constants, +, -, *; inclusive node/depth/absolute-constant limits; one-root/arity/schema validation; iterative validation/evaluation/export/destruction; private detached storage and strong replacement semantics; strict intermediate-result checking; separate mathematical-invalid/API-error outcomes; faithful locale-independent parenthesized export; and a manually constructed polynomial example. The evaluator has no dataset or RNG access. Division/logarithms and operator-vocabulary extension remain pending before the prime experiment.

See [EXPRESSIONS.md](EXPRESSIONS.md) for public API, numerical/build conditions and the concrete integration plan. The arithmetic-only Creature_i/Population restriction and scalar GA operators remain intact in this milestone. Next extract representation-independent evaluation/cache behavior into a common base, admit value-owned expression candidates to Population, and delegate GA variation through a defaulted policy so existing scalar GAs remain usable. The size-based initializer must not require an unconfigured ExpressionCandidate(engine); use configured external initialization and the vector constructor, with an explicit error for unavailable size-based initialization.

Generate expression structures with the existing run-owned engine and explicit node/depth/constant budgets. Add structure-aware subtree mutation/crossover with bounded retries, validated construction and an unchanged-parent fallback. Preserve limits and clear fitness after every accepted replacement. Keep datasets immutable and use checked all-sample MSE with ObjectiveDirection::Minimize; any mathematical invalidity/non-finite loss maps to Fitness::invalid(), while unexpected exceptions propagate through the safe publication path. Retain committed-best/RNG/attempt accounting and separate candidate, sample/node and later constant-fitting work.

Acceptance for the completed evaluator is deterministic structural/limit, arithmetic/error, ownership/alias, grouping/export and FP-policy verification through public APIs, standalone headers and a downstream C++20 consumer. Acceptance for the next integration is deterministic bounded initialization/variation invariants and failure tests plus a separately labeled bounded polynomial structure-search demonstration. Do not require stochastic convergence to pass correctness tests.

Add ordinary division and natural logarithms before milestone 5, with zero/nonpositive domain checks and strict non-finite handling. Make the allowed vocabulary configurable. Keep special operator behavior explicitly named/exported, if later introduced; never silently substitute protected operations. Preserve CPU correctness before performance work.

## 4. Polynomial symbolic regression — pending

Demonstrate structure evolution on y = x*x + x + 1 using the initial operator vocabulary. Use declared double observations, fixed seeds, bounded work and clear fitting/validation data. Tests verify the evaluator and deterministic run behavior; a separately labeled experiment assesses discovery rates and reports failures. Returned candidates include raw predictions, error and complexity. Keep the character GA example usable.

Once basic structure evolution works, add bounded constant fitting. Fitting uses only permitted fitting data and counts objective evaluations/iterations toward the overall budget. Retain the original candidate if optimization fails, becomes invalid or worsens the objective. Report number of fitted constants and actual work. Track error and complexity separately, with a small bounded archive of useful accuracy/complexity tradeoffs; do not conceal a complexity penalty inside reported error.

Acceptance: useful bounded polynomial experiment, tested fit rollback on failures/worsening, matching evaluator/export semantics and reproducible reporting. Any input/output normalization derives only from fitting data and includes the inverse transformation in the exported standalone model.

## 5. Generic sequence regression and prime experiment — pending

### Data and application boundary

The user-facing example accepts a plain numerical list, builds one-based (index, value) pairs, and runs the generic regression pipeline. It returns candidate standalone expressions, generated sequences and metrics. Sequence loading/pairing remains generic; sieve generation, primality diagnostics and reference checking belong outside the evolutionary core.

Generate at least the first 1,200 reference primes deterministically, keeping integers exact in the dataset. Verify the sieve independently (for example trial division) and against [OEIS A000040](https://oeis.org/A000040), [indexed reference table](https://oeis.org/A000040/b000040.txt). Required indexing fixtures: 1 → 2, 2 → 3, 3 → 5, 1000 → 7919. The expression takes n, never a prime value; evaluate it directly by position. Reference generation may use a sieve. Candidates receive only n, permitted constants and declared operations: no target-array lookup, prime generator, hidden reference access or primality repair.

Allow configurable operators including +, -, *, ordinary / and natural log, with the numerical semantics from milestone 3. Default search initializes independent allowed structures and discovers combinations. Any initialization from a known prime approximation must be a separately labeled experiment. Do not force evolution to follow a fixed known-prime formula.

### Fitting and untouched extrapolation

1. Develop using an ordered internal split, e.g. fit 1–800, validate 801–1000. Any random split within the first 1,000 is an additional interpolation experiment.
2. Choose the search/fitting/selection procedure using only that prefix. Freeze seeds, budget, vocabulary, complexity limits, normalization, fitter and selection policy before assessing later indices.
3. Clearly label a final fit using all first 1,000 observations and report reconstruction on indices 1–1000.
4. Freeze candidate expressions and their selection before evaluating indices 1001–1200. Report extrapolation separately. Do not tune using this range while continuing to call it untouched. If used for development, disclose that and reserve a new later range before subsequent testing.

### Objective, rounding, and reporting

Use continuous mean squared error initially: sum of squared residuals divided by the full sample count. Specify an invalid-candidate policy giving invalid evaluations no selection advantage; never silently reduce the denominator. Report raw prediction errors even when rounded matches occur. If normalization is used, report errors in the original target units and export the inverse transform.

Fix integer reporting to nearest integer with halfway cases away from zero (`std::round`). Check finiteness and representable integer range before conversion, including boundary behavior where double cannot represent an integer limit exactly. Keep out-of-range and domain errors as invalid predictions; tests must cover NaN, infinity, huge values and halfway cases. Compare predictions at the same sequence position. No nearby-prime repair in this experiment; any later repair is a distinct algorithm.

For each range and candidate report:

- MAE, RMSE, maximum absolute error and mean relative absolute error (prime targets are positive; generic zero targets need an explicitly declared relative-error policy).
- Rounded exact-match count/rate, invalid prediction count, duplicates (declare count convention), and adjacent violations of strict increasing order in rounded values. Invalid conversions must be visible rather than silently omitted.
- Node count, depth, fitted-constant count and constant values. Retain a bounded accuracy/complexity archive.
- Expression/exported standalone model; raw and rounded generated sequences; seed, complete configuration and execution mode; total work including constant fitting and failed evaluations, elapsed time and stopping reason.

### Baselines and repeated runs

Use identical fitting data and evaluation ranges for a fitted constant, fitted linear model, low-degree fitted polynomial (predeclare degree and solver), and n*log(n). The last is a fixed asymptotic formula, with n=1 defined ordinarily as 0. A refined logarithmic approximation may be added with explicit small-n domain handling and labeling; it must not dictate the evolved grammar or initialization.

Initial declared seed list for the future experiment: **[1, 7, 42, 2026, 2027]**. Before running, record numerical budgets (population/generations, node/depth/constant limits, fitter iteration/evaluation caps, total evaluation-work cap, archive size and any time cap), solver versions and all configuration. Those budgets are not yet selected or run. Report every run including unsuccessful ones, distribution of outcomes and computation used. Baseline comparisons establish what structure evolution contributes; avoid selecting the best seed using extrapolation.

Acceptance: independently checked exact data; verified indexing; bounded structure and constant search; prefix reconstruction and separately frozen extrapolation report; identical-data baselines; complete metrics and invalid accounting; declared repeated-run budgets; no hidden answer access or repair. Matching a finite prefix and predicting later primes are separate empirical results. Claims about all primes require mathematical justification beyond these experiments.

## 6. Packaging, documentation, qualification and resume — pending

Install/export targets and headers; version/package config; API and migration documentation; build matrix on supported toolchains/platforms; license/dependency notices. Specify serialization of engine state (not only original seed), population, objective/cache state, work counters, configuration and archive. Test interrupted/resumed equivalence under declared supported conditions. Add useful cancellation and result persistence before promising resumability.

## 7. Profiling and performance — pending

Profile representative CPU runs and evaluator/constant-fitting workloads. Improve measured bottlenecks under correctness/repeatability tests. Consider batching, parallel execution and Jetson GPU acceleration only with a declared execution/random-stream model, measured benefit, numerical qualification and an already correct CPU reference. Do not assume CUDA or threading is justified by this hardware alone.
