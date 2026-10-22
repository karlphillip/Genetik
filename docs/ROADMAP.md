# Current phased development plan

The published v0.1.0 checkpoint is `f9299ea2cb5136d06e2de8f68bb83ecf460ca369`.
Delivery and hosted qualification below are completed history. Phase 1 starts from
that current clean checkpoint; no typed-math work was present to checkpoint.
Existing `ExpressionNode` operator tags are not a completed general typed-value API.

1. **Workspace/repository cleanup** — complete: inventory before action,
   indexed compact archives, stable tools, fresh canonical builds, output hygiene,
   unchanged frozen evidence and public interfaces. See [CLEANUP.md](CLEANUP.md).
2. **Typed values and evaluation** — implemented: additive owned scalars/vectors/
   matrices, fixed ordered schemas, inferred shapes, strict checked CPU arithmetic,
   explicit local limits and faithful inspection/export. See TYPED_EXPRESSIONS.md
   and TYPED_EVALUATION_QUALIFICATION.md for executed local gates. Existing scalar/GA
   contracts remain intact.
3. **Typed evolution** — implemented locally: immutable typed problems, checked
   row/component MSE, generic candidate/preparation/worker integration, constructive
   shape-correct bounded variation, shared coordinate fitting over aggregate
   coefficients and finite search budgets. Five operation-discovery seeds and the
   separate eight-coefficient fixture are in TYPED_DISCOVERY_RESULTS.md and
   TYPED_FITTING_RESULTS.md. Qualification in TYPED_REGRESSION_QUALIFICATION.md.
4. **Scientific functions** — implemented locally: opt-in scalar Sin/Cos/Exp/Sqrt
   and unary Powi exponent metadata0..8, shared checked kernels, scalar/typed
   grammars, bounded discrete exponent variation, fitting exclusion and faithful
   export. See SCIENTIFIC_OPERATORS.md, SCIENTIFIC_RESULTS.md and
   SCIENTIFIC_QUALIFICATION.md. Existing defaults/replays remain unchanged;
   hosted qualification of accumulated feature work passed at the identified candidate
   in [TYPED_SCIENTIFIC_HOSTED_QUALIFICATION.md](TYPED_SCIENTIFIC_HOSTED_QUALIFICATION.md).
5. **Text input and adapters** — bounded scalar/typed expression import implemented
   locally, with canonical round trips, explicit schemas/limits, installed usage and
   a supplied-formula file evaluator. See EXPRESSION_PARSING.md and
   PARSING_QUALIFICATION.md. Optional external adapters remain separately scoped;
   parsing and data formats do not enter the generic evolutionary core. Selected hosted
   qualification of Phases 5/6A is recorded in PARSER_TEMPLATES_HOSTED_QUALIFICATION.md.
6. **Constraints and templates** — Phase 6A implemented locally: one immutable
   scalar scaffold/site, original-index variable/operator restrictions, constrained
   structure search, full-model loss and selected-constant fitting through shared
   preparation/publication. See SCALAR_TEMPLATES.md, SCALAR_TEMPLATE_RESULTS.md and
   SCALAR_TEMPLATE_QUALIFICATION.md. Multiple/repeated holes, shared parameters,
   typed templates, units and general constraints require separate Phase 6B scope.
   The combined parser/template hosted checkpoint is recorded in
   PARSER_TEMPLATES_HOSTED_QUALIFICATION.md; this does not expand the one-site scope.
7. **Calculus** — Phase 7A implements bounded first-order scalar input partials
   by forward-mode numerical chain rules, with independent activity, explicit
   unavailable/resource outcomes and strict primal equivalence. See
   SCALAR_DERIVATIVES.md and DERIVATIVE_QUALIFICATION.md. Symbolic trees, gradients,
   higher derivatives, typed Jacobians and gradient optimizers remain outside the
   agreed completion scope; no general symbolic algebra is implied.
   **Phase 7B** now adds supplied value/partial observation channels, weighted
   normalized loss, shared-engine search and coordinate fitting, with the bounded
   anchored-antiderivative campaign in DERIVATIVE_REGRESSION_RESULTS.md.
   **Phase 7C** adds bounded owned root integral models, deterministic proper GK7/15
   evaluation with estimated convergence and explicit failure/resources, full-model
   integrand regression and coordinate fitting. See SCALAR_INTEGRALS.md,
   INTEGRAL_RESULTS.md and INTEGRAL_QUALIFICATION.md. 7A scalar derivatives and
   7B derivative-observation regression are locally complete; 7C is locally
   implemented and ready for acceptance after its recorded gates.
   **Next: combined calculus hosted qualification and final development-delivery
   review. No further mathematical capabilities or feature phases precede it.**
8. **Additional model families** — historical backlog only, outside the agreed
   7B/7C then final-qualification/delivery scope; no additional implementation phase
   is scheduled by this roadmap entry.

Cleanup introduced no mathematical API; Phase 2 added typed evaluation and Phase 3
now adds typed search/refinement. Phase 5 adds bounded text import; 7A/7B/7C calculus implementation is locally complete, with final combined qualification/delivery next. Optional adapters, additional constraints and model families are backlog outside this completion scope. Further delivery/platform qualification,
profiling, checkpoint/resume, cache policy and GPU work remain separately scoped.
The remaining sections preserve the historical engineering and experiment record;
old “next”/“unreleased” statements describe those checkpoints, not current status.

---

# Genetik roadmap

The baseline and fitness/publication contracts are implemented and verified. The bounded expression value and strict CPU evaluator are implemented; bounded polynomial structure evolution is also implemented through the shared engine. Bounded deterministic continuous constant fitting and a verified generic multivariate/file-data workflow are implemented; strict division/natural logarithm and mixed-arity evolution are implemented; the first reproducible prime experiment and optional deterministic fitting cadence are implemented. See [BASELINE.md](BASELINE.md) for evidence and API migration.

Historical delivery order (completed checkpoints; current phased plan above):

1. Generic multivariate/data-input milestone — complete; public row API, strict application loader, 2/3-input contracts and all five campaign seeds reported.
2. Strict ordinary division/natural logarithm throughout typed construction, evaluation, variation, limits, export and domain tests — complete; the first prime regression campaign is also complete.
3. Prime regression as an example of generic indexed-sequence regression — first campaign complete, including all weak results and baselines.
4. Refinement profiling and optional cadence — complete; keep interval 1 default, preserve mixed twenty-run results. Checked evaluator workspace profiling/reuse is complete.
5. Local install/export/relocation, compiled quickstarts and version/support policy are complete. The three-job hosted matrix has executed successfully; see HOSTED_QUALIFICATION.md for exact supported combinations and candidate. Development-release review remains next. Checkpoint/resume and acceleration remain separately scoped.

## 1. Baseline, C++20 and first hardening — complete

Acceptance: clean starting-state/environment inventory; unchanged build and bounded example exercised; transitive C++20; tests that fail in Release and integrate with CTest; explicit run-owned seed/state across initialization, selection, crossover and mutation; regression tests; supported local sanitizer run; public-header/downstream consumer checks; documented compatibility. All satisfied on the current Jetson/GCC 13 platform. Wider platform qualification remains milestone 6.

## 2. Evolutionary contracts — fitness/publication and reference constant fitter complete

Implemented: explicit run-owned randomness; private scalar genomes and enforced invalidation; Unevaluated/Valid/Invalid results with finite double scores; independent objective direction; rank selection excluding invalids; staged evaluation, ranking and committed-best preparation; no-throw publication of a viable population; nested operational errors; accurate attempted-evaluation accounting without refunding failed work. Fixed objective/data are assumed for each run. Deterministic fault-injection tests verify committed state preservation and consumed RNG state. See BASELINE.md for the exact scope and compatibility changes.

The expression value/evaluator, candidate/variation bridge and bounded polynomial demonstration are complete. Bounded continuous expression terminals and coordinate constant refinement are implemented independently of ASCII. General real-valued scalar variation and additional numerical solvers remain future work. Raw MSE can now be minimized using the explicit direction; do not confuse score validity with optimization direction.

The symbolic driver now has generation-boundary candidate-call budgets and committed result/counter/stop reporting. General time budgets, cancellation and broader configuration/work reporting remain pending; manual GA callers still own their stopping policy. Budgets charge attempted calls, including Invalid and throwing evaluations, without refunding discarded generations. Preserve the serial publication boundary and no-implicit-retry policy. Constant fitting now charges baseline/probe/invalid/throwing loss and row attempts; a global exhaustion inside a generation discards private work and keeps committed state.

## 3. Bounded mathematical expressions — initial grammar and variation complete

Implemented: a dedicated Expression value with typed postfix nodes for variables, finite double constants, +, -, *; inclusive node/depth/absolute-constant limits; one-root/arity/schema validation; iterative validation/evaluation/export/destruction; private detached storage and strong replacement semantics; strict intermediate-result checking; separate mathematical-invalid/API-error outcomes; faithful locale-independent parenthesized export; and a manually constructed polynomial example. The evaluator has no dataset or RNG access. The operator list is configurable; strict Divide and unary natural Log are now available as opt-in families, with unchanged +,-,* defaults.

See [EXPRESSIONS.md](EXPRESSIONS.md) for the strict evaluator and [SYMBOLIC_REGRESSION.md](SYMBOLIC_REGRESSION.md) for the implemented candidate bridge. EvaluatedCandidate now owns representation-independent fitness/cache behavior; scalar Creature_i and value-owned ExpressionCandidate share Population ranking, staged evaluation, publication, best-seen tracking and failure semantics. GA delegates scalar or expression variation through a defaulted policy; configured expression initialization uses an external factory and the vector constructor.

Implemented bounded random initialization, typed postfix subtree replacement mutation/exchange crossover, destination-depth/node/constant validation, finite proposals and unchanged-parent fallback. All random decisions use the run-owned engine. Checked all-sample training MSE maps invalid predictions/loss arithmetic to Fitness::invalid(); unexpected errors propagate with nested context. Candidate-call budget boundaries count initialization, invalid/throwing calls and reevaluated elites. The search shares publication/selection rather than implementing another transaction loop.

Qualified deterministic tests cover variation/invalidation/ownership, fixed problems, consistent objective/complexity/text ordering, injected failures, all-invalid and budget behavior, and same-seed short serial repeatability. Stochastic polynomial recovery is a separate campaign below. Aggregate training-loss and row counters now include fitting work; per-node profiling remains future work.

Implemented ordinary division and unary natural logarithm with signed-zero/nonpositive domain checks, strict intermediate finiteness and arity-aware structure/limits/export/variation. No protected arithmetic or mathematical-validity resampling. Public C++ grammar and file --operators opt-in retain old defaults/draw order. See [STRICT_ARITHMETIC.md](STRICT_ARITHMETIC.md) and [STRICT_ARITHMETIC_CAMPAIGN.md](STRICT_ARITHMETIC_CAMPAIGN.md): three successful and two budget-limited discovery runs, all retained separately from engineering acceptance. Preserve CPU correctness and the runtime math-library contract before performance work.

## 4. Polynomial symbolic regression — structure search and reference constant fitting complete

Implemented a public data-driven search and polynomial example for y = x*x + x + 1. The fixed five-seed, 128-slot, 250-attempt/32,128-call campaign recovered every declared training/interior/exterior sample numerically, with three post-initialization discoveries and two generation-zero discoveries. No configuration adjustment was needed; held-out ranges were evaluated only after the training-based decision and representative selection. All seed outputs and unsuccessful initial fitness values are retained in [POLYNOMIAL_CAMPAIGN.md](POLYNOMIAL_CAMPAIGN.md) and its raw log. This is evidence for this discrete grammar and sample protocol, not an algebraic proof or universal convergence guarantee.

Implemented optional deterministic coordinate fitting, fixed topology/independent postfix constant occurrences, strict improvements, finite proposal/sweep/loss limits, bounds [-8,8] in the campaign, training-only refinement and checked accounting. Local stops retain earlier improvements; invalid starts skip; operational/global-budget aborts preserve committed state. Controlled dyadic/non-dyadic quadratic fixtures pass within 1,024 calls. In the frozen paired campaign all five enabled seeds recover training/interior/exterior at max error <=1e-6, while none of the five disabled seeds do under the common 100,000-call allowance. Actual work differs and is recorded in [CONSTANT_FITTING_CAMPAIGN.md](CONSTANT_FITTING_CAMPAIGN.md); see [CONSTANT_FITTING.md](CONSTANT_FITTING.md) for API, limitations and counters.

Generic multivariate/data-input extension is complete: arbitrary positive input dimensionality (zero-input constants retained), fixed input order, one scalar target, typed variable-preserving variation, dimension-independent constant fitting and exact separate-problem cache rules. No core restriction or defect required a library change. A bounded application-layer unquoted table loader and small file-search CLI accept caller-selected ordered inputs, target, seed/loss budget and separate evaluation files; in-memory and parsed paths share the public search. Two/three-input contracts, round trips, malformed files, same-seed counters/RNG and downstream consumption are qualified. The frozen affine two-input campaign recovers training/interior/exterior on every seed without adjustment; see [MULTIVARIATE.md](MULTIVARIATE.md) and [MULTIVARIATE_CAMPAIGN.md](MULTIVARIATE_CAMPAIGN.md). This is sample agreement, not a universal search guarantee.

Strict ordinary division/natural logarithm are complete and qualified across construction/evaluation/export, mixed-arity variation, fitting/domain failures, budgets/publication and downstream/file workflows. Prime regression is now implemented as a generic-module example using the protocol below; see PRIME_REGRESSION.md and PRIME_CAMPAIGN.md. Keep stronger numerical solvers separate from this extension. Track error and complexity separately; a small bounded accuracy/complexity archive remains pending. Do not conceal a complexity penalty inside reported error.

Acceptance: useful bounded polynomial experiment, tested fit rollback on failures/worsening, matching evaluator/export semantics and reproducible reporting. Any input/output normalization derives only from fitting data and includes the inverse transformation in the exported standalone model.

## 5. Generic sequence regression and prime experiment — first campaign complete

### Data and application boundary

The user-facing example accepts a plain numerical list, builds one-based (index, value) pairs, and runs the generic regression pipeline. It returns candidate standalone expressions, generated sequences and metrics. Sequence loading/pairing remains generic; sieve generation, primality diagnostics and reference checking belong outside the evolutionary core.

Generate at least the first 1,200 reference primes deterministically, keeping integers exact in the dataset. Verify the sieve independently (for example trial division) and against [OEIS A000040](https://oeis.org/A000040), [indexed reference table](https://oeis.org/A000040/b000040.txt). Required indexing fixtures: 1 → 2, 2 → 3, 3 → 5, 1000 → 7919. The expression takes n, never a prime value; evaluate it directly by position. Reference generation may use a sieve. Candidates receive only n, permitted constants and declared operations: no target-array lookup, prime generator, hidden reference access or primality repair.

Allow configurable operators including +, -, *, ordinary / and natural log, with the numerical semantics from milestone 3. Default search initializes independent allowed structures and discovers combinations. Any initialization from a known prime approximation must be a separately labeled experiment. Do not force evolution to follow a fixed known-prime formula.

### Fitting and untouched extrapolation

1. An optional future development protocol may use an ordered internal split, e.g. fit 1–800, validate 801–1000. The first implemented campaign instead fits all 1–1000 directly; none of that prefix is a holdout. Any random split within the first 1,000 is an additional interpolation experiment.
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

The implemented first campaign uses the superseding declared seeds **[0, 1, 2, 3, 42]**. Before running, record numerical budgets (population/generations, node/depth/constant limits, fitter iteration/evaluation caps, total evaluation-work cap, archive size and any time cap), solver versions and all configuration. The first campaign selected 100,000 actual loss calls per seed from a separate runtime-only 4,096-call pilot; population 64, generation ceiling 2,000, nodes 31/depth 6, five operators and accepted optional coordinate fitting. Full fixed settings, all five outcomes and five fitted/fixed baselines are recorded in PRIME_CAMPAIGN.md. Scaling is predeclared u=n/1000, v=p/10000, inverse output 10000*g(u), all application-only. No target templates, lookups or repair enter candidate search. Report every run including unsuccessful ones, distribution of outcomes and computation used. Baseline comparisons establish what structure evolution contributes; avoid selecting the best seed using extrapolation.

Acceptance: independently checked exact data; verified indexing; bounded structure and constant search; prefix reconstruction and separately frozen extrapolation report; identical-data baselines; complete metrics and invalid accounting; declared repeated-run budgets; no hidden answer access or repair. Matching a finite prefix and predicting later primes are separate empirical results. Claims about all primes require mathematical justification beyond these experiments.

First campaign acceptance is complete: exact offline sieve/trial-division reference,
validated supplied prefix, full-precision generic file workflow, fitting-only selection,
frozen baseline/model assessment, safe int64 rounding and complete raw-unit metrics/
prediction tables. All five runs use the loss cap; only 8–12 generations publish.
Representative seed 2 approaches the separately fitted logarithmic baseline, without
an established accuracy advantage or exact-prime recovery. All unsuccessful results
are preserved. The 1001–1200 range is now examined; do not call it fresh after informed
tuning. A bounded accuracy/complexity archive, stronger numerical solver, and an
800/200 development/refit study remain future work, not implemented claims.

Historical recommendation after the first prime campaign (profiling/cadence now
completed below): inspect evaluator/refinement work under unchanged immutable-
problem/publication contracts. Cache policy remains a separate decision. The
measured call allowance is mostly spent refining fewer than 13 generations, not on
invalidity or rejected structure. Reserve a new later range for future informed
extrapolation assessment. Keep production delivery below on the roadmap; no GPU work
before representative CPU profiling.

## 6. Library delivery — local and selected hosted qualification complete

Implemented the header-only Genetik package and canonical Genetik::Genetik source/installed target, retained source genetik_lib, GNUInstallDirs metadata, relative install includes, consumer frontend/language strict-FP requirements and private development flags. Ordinary and relocated multi-TU consumers/quickstarts pass after disposable producer removal. Version 0.1.0 remains unreleased; SameMinorVersion/ARCH_INDEPENDENT behavior and required-component rejection are tested. Exact minimum CMake 3.16.0 and current 4.4.4 pass locally with GCC13. API/support/release guides and a pinned three-job workflow are present. The subsequent hosted matrix has now verified selected Clang/x86 and GCC/x86/ARM combinations; see HOSTED_QUALIFICATION.md. Do not extend that claim to untested combinations. See DELIVERY.md, PACKAGING.md and SUPPORT.md.

Checkpoint/resume remains a separate subsequent task. Specify serialization of engine state (not only original seed), population, objective/cache state, work counters, configuration and archive. Test interrupted/resumed equivalence under declared supported conditions. Add useful cancellation and result persistence before promising resumability.

## 7. Further profiling and performance — separately scoped

Profile representative CPU runs and evaluator/constant-fitting workloads. Improve measured bottlenecks under correctness/repeatability tests. Consider batching, parallel execution and Jetson GPU acceleration only with a declared execution/random-stream model, measured benefit, numerical qualification and an already correct CPU reference. Do not assume CUDA or threading is justified by this hardware alone.

## Refinement cadence and profiling — complete

Implemented positive/default-one fitEveryGenerations in the shared private
preparation path, initialization generation zero and prospective committed schedule,
fresh ordinary scoring between scheduled generations, role/work accounting and
optional bounded exact-repeat/timing observation. No cache or optimizer replacement.
All ten controls match accepted numerical results/legacy counters; paired raw and
prepared initialization and two profile/control RNG/work comparisons match.

See [FITTING_CADENCE_RESULTS.md](FITTING_CADENCE_RESULTS.md): interval 4 publishes
more generations on every seed, but fitting-MSE wins/losses are 3/2 on both problems,
strict-arithmetic recovery falls 3/5 to 2/5, and paired median runtime ratios exceed
one. Keep interval 1 as default. Prime's fitting-selected representative worsens
with interval 4; all results, including misses, are retained. No evaluation-driven
tuning was performed; indices 1001–1200 remain an examined benchmark.

That recommendation is now completed by invocation-local evaluation workspace
reuse; see EVALUATION_WORKSPACE.md. Allocation counts drop from one per visited row
to one per loss; fixed-work timings support keeping the change, and accepted
numerical/search outcomes and work/RNG counters match exactly. The inclusive
97.5% region was not itself evidence that allocation dominated.
Exact input/result reuse is a separate later decision: observed repeat no-change
work is measurable (~4.5% prime/~6.9% arithmetic calls), but inherited fitted outputs
can still improve. Any cache needs exact content/constraints/problem/configuration
identity and explicit budget semantics. No already-optimized flag, cache, new solver,
algebraic rewrite or GPU execution is implemented. Production packaging/platform
qualification/documentation and checkpoint/resume remain pending.


## Checked evaluation workspace — complete; delivery is next

Measured a fixed polynomial/prime/arithmetic/mixed/boundary/domain corpus against
archived accepted headers before changing storage. One internal, loss-owned vector
now retains capacity across rows, through the same strict interpreter as public
single-point evaluation. No arithmetic, validation, reduction, cache, RNG or budget
changes. Independent finite bits/invalidity/errors/row work match, including full
prime seed-2 and arithmetic seed-42 control replays and a short cadence-4 check.
Five paired fixed-work batches show useful multirow gains; noisy/slower single-row
cases are retained. See EVALUATION_WORKSPACE.md and evaluation-results.

The delivery scope below is now locally implemented (DELIVERY.md): install headers and exported
CMake targets/package configuration; verify a clean separately configured consumer
against an installed prefix; declare supported compiler/platform versions and add
CI coverage; publish API/ownership/numerical documentation and a release/versioning
policy with license notices. Start from the qualified C++20/strict-FP CPU behavior.
Checkpoint/resume needs its own state/equivalence design rather than being implied
by packaging. Exact-result caching, cross-loss storage, stronger solvers, expression
compilation, algebraic rewrites and GPU execution remain separate decisions.


## Delivery completion and next release-readiness task

Install/export, isolated relocated consumption, compiled scalar/symbolic quickstarts,
source-parent behavior, minimum CMake, public API index and coherent 0.x source/
numerical compatibility policies are complete locally. Historical campaign evidence
and exact reference files are preserved. Captured Jetson bits remain opt-in outside
that qualified environment; portable contracts and within-build determinism run in CI.

Next: execute and review the configured GCC13/x86 minimum-CMake, Clang18/x86 sanitizer
and native ARM64 jobs, investigate actual differences, then apply the documented
release checklist before any explicitly authorized push/tag/publication. Broader
platform/stdlib qualification remains pending. Packaging does not imply checkpoint/
resume, ABI stability, new solvers, cross-call caches, compiled expressions or GPU
execution; keep those as separately measured/designed future decisions.

## Generic OOP qualification and organization

The public value-owned candidate/preparation and runtime algorithm hooks are audited
in ARCHITECTURE.md; installed permutation qualification exercises a non-regression
representation. Virtual roulette dispatch is repaired while default serial behavior
is retained. Next implement the generic bounded execution layer in CONCURRENCY.md:
serial default, consistent eventual --threads N, exclusive worker state, explicit
admission/draining/failure guarantees and TSan/performance qualification. No runtime
population concurrency exists today. Preserve all historical campaign outcomes.

The organized source/output layout is recorded in ORGANIZATION.md; accepted campaign
artifacts and executable names remain unchanged. OOP_QUALIFICATION.md records the
repeated delivery and serial compatibility gates. Next: implement CONCURRENCY.md,
then execute hosted support qualification and production-delivery checks. Checkpoint/
resume, cross-call reuse, stronger solvers and GPU execution remain separate work.

## Generic CPU concurrency — implemented

ExecutionContext now owns a bounded reusable pool; RandomContext execution defaults
to one. Coordinator retains RNG/variation/order/publication and virtual selection.
Private candidate/refiner factories, checked shared loss admission, stable collection,
cooperative batch cancellation and drain/error precedence are implemented. Historical
serial-only statements above describe the OOP checkpoint. See CONCURRENCY.md and
CONCURRENCY_RESULTS.md for API, local tests/TSan, throughput and limitations.
Next: review the first development release using executed hosted qualification,
then prioritize measured remaining costs; caching, stronger optimizers, expression
compilation, checkpoint/resume and GPU execution remain separate decisions.


Hosted qualification now has actual passing jobs for the repaired implementation,
including minimum CMake, isolated installed consumers and separate TSan. See
HOSTED_QUALIFICATION.md for failure repairs, exact environments and test accounting.
Proceed to development-release review and broader supported-platform qualification;
no tag/publication is implied. Preserve recorded benchmark scope and keep caching,
new optimizers, rewriting, compilation and GPU execution separately scoped.
