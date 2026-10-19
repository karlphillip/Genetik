# Genetik roadmap

The first milestone is implemented and verified; later milestones describe acceptance criteria, not existing functionality. See [BASELINE.md](BASELINE.md) for evidence and API migration.

## 1. Baseline, C++20 and first hardening — complete

Acceptance: clean starting-state/environment inventory; unchanged build and bounded example exercised; transitive C++20; tests that fail in Release and integrate with CTest; explicit run-owned seed/state across initialization, selection, crossover and mutation; regression tests; supported local sanitizer run; public-header/downstream consumer checks; documented compatibility. All satisfied on the current Jetson/GCC 13 platform. Wider platform qualification remains milestone 6.

## 2. Evolutionary contracts and real-valued fitting — next

Explicit randomness is complete for the default serial pipeline. Next implement explicit fitness validity and refresh/commit semantics: genome writes invalidate evaluation; summaries and selection cannot silently consume stale fitness; invalid evaluations have a declared policy; a failed generation leaves a usable prior population and reports consumed work. Add meaningful tests with a throwing/invalid deterministic evaluator, and verify copies/moves and run-context lifetime expectations.

Then provide a reusable real-valued creature/variation policy independent of ASCII and target macros. Define maximization/minimization explicitly, distinguish objective from selection weights, and use double objectives. Add validated bounds, configurable initialization and mutation, equality and invalid-score behavior, and explicit selection/replacement policies. Preserve ordinary fixed-genome GA usage.

Acceptance: deterministic bounded real-parameter optimization test, fitness/cache/error contracts, clear value ownership, execution budget and cancellation semantics, and a result object containing effective seed, configuration, work, elapsed time, best seen and stopping reason. Avoid relying on stochastic target discovery for correctness tests. Count evaluation failures and all attempted work. Preserve run-owned randomness; custom operators receive the same engine.

## 3. Bounded mathematical expressions — pending

Represent validated variable-size expressions (value-owned tree or indexed node arena; choose after the lifecycle contract is established). Start with variable x, finite bounded double constants, addition, subtraction, multiplication. Validate arity, indices, acyclicity where applicable, node count, depth, number/range of constants. Mutation/crossover must produce valid structures within configured limits; use bounded retries and a documented unchanged-parent fallback. Structure changes are required; this is not only coefficient search over a fixed formula.

Implement double evaluation with an explicit value/error result. Add ordinary division and natural logarithms before milestone 5. Reject zero divisors, nonpositive logarithm arguments, overflow and all non-finite intermediates/results. Invalid samples invalidate the candidate or receive a declared worst objective; never drop difficult samples or reward invalid evaluations. Keep an operator registry/vocabulary in configuration for later experiments. Reject unsupported configurations rather than silently changing semantics.

Formatter/export must preserve exactly the evaluator's operations, parentheses, round-trip constants and transformations. Tests cover known trees, boundary domains, arithmetic errors, constant bounds, structural validity after operators, and exported evaluation equivalence. No hidden protected division or altered logarithm semantics. If a special operator is later introduced, give it a distinct declared name/definition and faithfully export it.

Acceptance: deterministic evaluator and formatter tests, bounded structural-operator invariants, standalone expressions with no reference-data access, runtime and evaluation work limits, and a verified CPU path before performance work.

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
