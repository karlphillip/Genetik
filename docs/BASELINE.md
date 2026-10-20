# Baseline and first hardening milestone

Verified 2026-10-19 on the supplied NVIDIA Jetson. Sections through the checkpoint reconciliation describe the original hardening baseline; later sections record the subsequent fitness/publication, expression and polynomial structure-search milestones and supersede historical limitations they explicitly resolve.

## Starting state

- Repository: `Genetik`, initial revision `b8059f08ececa9bda6b2c19f35a925c070052f56` (2021 initial design).
- Working tree was clean; no repository or ancestor AGENTS.md was present. No prior milestone work existed here.
- OS: Ubuntu 24.04.4 LTS; Linux `6.8.12-1021-tegra`, aarch64.
- GCC/G++ 13.3.0 (`13.3.0-6ubuntu2~24.04.1`), `/usr/bin/c++`; only g++-13 found, no clang++ on PATH.
- libstdc++ release 13, `__GLIBCXX__=20240904`. A compiled/executed `-std=c++20` probe including `<version>`, `<span>`, `<concepts>`, and `<random>` passed (`__cplusplus >= 202002L`). No C++20 blocker for this milestone; this does not qualify every C++20 facility.
- CMake initially absent from PATH. Installed CMake 4.4.4 in `../.tools` via a Python virtual environment, without system changes. Make and Ninja were available. Actual project builds used Unix Makefiles.
- Initial memory: 15 GiB RAM, 9.9 GiB available, no swap. Storage: 1.8 TiB volume, 834 GiB free. These are point-in-time measurements.
- Existing project: six public template headers, no compiled implementation files. Default creature is the fixed 74-character target example; templating its gene type does not make its initialization/fitness generic.
- Existing targets: an empty static library, character demo, print-only test executable. Root `main.cpp` is an unbuilt stale sketch referencing nonexistent `Settings` and `run()` APIs.
- Dependencies: C++ standard library only; MIT license, copyright Karl Phillip (2021). No bundled testing framework, external runtime libraries, or package dependency manager. New tests retain the dependency-free convention; CMake is a build tool, not a library dependency.

## Reproducing the unchanged baseline

Commands ran from the repository before source changes:

```sh
# From workspace parent, needed only because cmake was missing:
python3 -m venv .tools
.tools/bin/pip install cmake   # resolved 4.4.4 on this date
cd Genetik
../.tools/bin/cmake -S . -B ../build-baseline -DCMAKE_BUILD_TYPE=Release
../.tools/bin/cmake --build ../build-baseline -j2
../.tools/bin/ctest --test-dir ../build-baseline --output-on-failure
timeout 10s ../build-baseline/tests/genetik_tests > ../baseline-tests.log
timeout 3s ../build-baseline/genetik_demo > ../baseline-demo.log
```

Configure/build succeeded, with a CMake minimum-version deprecation warning. CTest reported **No tests were found**. The test executable returned 0 but contained no assertions or pass/fail checks. The bounded demo returned 0 after finding fitness 1 at generation 324 on this particular unseeded run. Its source had `while (true)` and no deterministic budget; this observation is not a termination guarantee.

Original source and a reproduction harness were retained in the build workspace. The harness is also saved in `docs/reproductions/baseline.cpp` for durability and targets only the initial API. Reproduce from the repository root:

```sh
mkdir -p ../baseline-source
git archive b8059f08ececa9bda6b2c19f35a925c070052f56 | tar -x -C ../baseline-source
g++ -std=c++11 -I../baseline-source/genetik docs/reproductions/baseline.cpp -o ../baseline-repro
../baseline-repro
```

Observed output includes `stale_fitness=1`, `equal_scores_first_range=0,0.00503309`, `singleton_best_matches_stored=0`, and a vector range-check exception for a zero population. The harness also printed `zero_selected=0 of 1000`; that zero-score observation alone is consistent with weighted selection and is not evidence of the roulette defect. The incorrect positive equal-score ranges and source arithmetic establish that defect.

## Confirmed findings and dispositions

References in this table name files/lines **at the initial revision**, retrievable using `git show b8059f0:path | nl -ba`.

| Finding | Evidence | Disposition / regression coverage |
| --- | --- | --- |
| Uncontrolled independent randomness | `genetik/Creature.hpp:23-25` has a header-static random-device-seeded engine; `GeneticAlgorithm_i.hpp:26` independently reseeds another. Creature initialization/mutation use the first; algorithm selection/crossover use the second. | Removed both hidden sources; one explicit run context and engine references. Exact genome comparisons across 25 generations, with independent interleaved evolution, and concrete character initialization/mutation tests. |
| Roulette units and truncation are wrong | `Population.hpp:146-165`: unsigned accumulator truncates small fractional scores and divides fractions by 100; ranges mostly cover 0–0.01, with the final range forced to 100 (`:185-187`). Algorithm draws integers 50–100 (`GeneticAlgorithm.hpp:170`). | Double accumulation, percentage weights totaling approximately 100, continuous draw over [0,100), half-open ranges. Fixed-seed 1:3 weights test with a broad bound and exact range checks; zero scores have equal weights. |
| Fitness cache can return an old perfect score after gene writes | `Creature.hpp:130-137` and `:115-123` modify genes without changing cached fitness. Repro reports 1 after corrupting a perfect chromosome. | Writes/mutations reset cached fitness to 0; regression tests verify recalculation. Full validity tracking remains a design gap below. |
| Crossover caches an unrelated default length | `GeneticAlgorithm.hpp:100-108` caches a default creature's chromosome size per template instantiation, rather than checking parents. Existing eight-gene print test does not check the result. | Uses actual parent length on each call; rejects empty/mismatched parents. Tests cover lengths 8 and 1 with the same template and complementary children. |
| Empty and singleton populations have incorrect behavior | `Population.hpp:48` accesses element zero on size 0; `:84-88` returns a random temporary instead of the actual singleton. | Zero sizes explicitly rejected, singleton returned/evolved correctly; rejected resize preserves the old population. |
| Invalid rates and fitness are unchecked | `GeneticAlgorithm_i.hpp:18-26`, `Population.hpp:125-165` accept out-of-range/non-finite values, including unsafe float-to-unsigned conversion during weighting. | Rates finite in [0,1]; fitness finite and nonnegative or a domain exception. Tests include negatives, NaN, infinity, equal and fractional scores. |
| Verification does not report incorrect results | `tests/tests.cpp:290-336` invokes print functions; `tests/CMakeLists.txt` registers no tests. | Runtime checks return nonzero on failure in Release and Debug; CTest registration and timeouts. |
| Example termination depends on discovery | `demo.cpp:35` has unbounded loop. | Explicit generation bound, including zero; seed/configuration, best seen candidate, elapsed time and stopping reason reported. |

## Ownership, evolutionary behavior, and remaining concerns

Confirmed safe existing mechanisms: all three abstract interfaces already have virtual destructors; genomes and populations store values in vectors, and copying a creature owns its genome independently. `clear()` destroys elements and releases their owned resources; it may retain vector capacity. The README/inline memory TODO is not evidence of a leak. There is no requirement to introduce raw-pointer ownership.

`creatures()` has always allocated a **copy**, despite returning shared_ptr; it is now documented and tested as a snapshot. Evolution takes one snapshot per generation instead of repeatedly copying the population for every selection/size check. Population sorting now uses stable ascending fitness order, best last. Equal scores keep their relative order, all-zero fitness selects uniformly. Replacement is generational with no elitism; odd sizes retain exactly the requested count. The demo separately tracks best seen, so it may exceed the current generation's best.

New `RandomContext` owns seed and mt19937 state and cannot be copied or moved. Consumers borrow it; it must outlive them. Population/algorithm copies, where constructible, borrow the same context, so they are not independent runs. Creature copying retains no RNG pointer. The engine is not thread safe for shared use. Users implementing custom creatures/operators must use the supplied engine and deterministic fitness; the library cannot prevent custom code from introducing external randomness. Default library sources contain no random_device, global RNG, or hidden reseeding.

Remaining design concerns, **not implemented guarantees**:

- Fitness still uses float and maximization of nonnegative scores. Double-valued genomes work (tested with a deterministic custom type), but bounded real-valued variation and constant fitting are not provided. Raw MSE must not be passed to this maximizer without a declared ordering/fitness adaptation.
- Reset-to-zero is only a partial cache contract: zero is also a valid score and no dirty flag exists. `setCreature()` requires `updateValues()` before summaries/selection; derived implementations can violate invalidation. Low-level gene access still silently ignores invalid writes/returns zero for invalid reads. Selection after unrefreshed replacements can observe stale ranges.
- `updateValues()` and `evolve()` can leave partially evaluated/replaced state when user evaluation or assignment throws. A failed resize preserves population contents but consumes RNG draws. Transactional generation semantics and explicit evaluation results are next work, not implied by exception rejection.
- No library run/result object, cancellation token, evaluation/time budget, archive, or checkpoint exists. A generation is bounded for finite populations/chromosomes and terminating custom callbacks; the demo imposes a generation budget. It does not enforce a wall-clock limit on arbitrary callbacks.
- Single-point equal-length crossover and one-to-one-percent mutation are suited to fixed genomes. Variable-size expression trees need validated subtree operators and node/depth/constant bounds; default crossover deliberately rejects unequal lengths. Custom algorithm implementations remain possible through existing interfaces.
- Existing user-defined copy operations suppress efficient implicit moves for the character creature/interfaces; moving currently uses valid copies. Performance consequences have not been profiled. Reference members in population/algorithm restrict assignment. Installation/export/package metadata are not provided.
- The fixed target macros and ASCII behavior remain in the concrete demonstration creature header. Future reusable real-valued/expression implementations should be separate from that example. The unused root main.cpp remains an unbuilt historical sketch.
- No GPU, parallel, alternate compiler, cross-platform bitwise reproducibility, ThreadSanitizer, checkpoint, or search-quality qualification was performed.

README TODO verification: timing is now available in the demo; no Google Test/Mock existed or was needed for this small dependency-free suite. Concurrency, persistence, additional examples and a separate GeneticOperator interface remain roadmap items. There is no expression evaluator to verify yet; prime regression is intentionally deferred.

## Completed changes and compatibility

CMake minimum 3.16; `genetik_lib` is now an INTERFACE target with transitive `cxx_std_20`, no empty archive. The old install rule for the print-only test executable is removed; library installation/exports remain deferred. Public headers compile individually, and an external `add_subdirectory` consumer requires no manually supplied include paths or C++ standard flags. `BUILD_TESTING` and `GENETIK_BUILD_EXAMPLES` are supported. `GENETIK_SANITIZERS` supplies GCC/Clang ASan+UBSan compile/link flags.

Necessary source migration (no legacy path with hidden random state is retained):

```cpp
RandomContext run(42);  // declare before the population and algorithm
Population<Creature<char>> population(100, run);
GeneticAlgorithm<decltype(population), Creature<char>> ga(0.5f, 0.9f, run);
Creature<char> individual(run.engine());
individual.mutateGene(0, run.engine());
```

Custom creature initialization must accept `std::mt19937&`; mutation changes to `mutateGene(const unsigned int&, std::mt19937&)` when deriving from Creature_i. Custom GeneticAlgorithm_i constructors forward a RandomContext. Custom populations used by the default algorithm expose `randomContext()` returning the borrowed context. Population and algorithm must reference the same context; mismatches throw. Vector-based creature construction remains deterministic and unchanged. Direct gene mutation also needs an explicit engine. FitnessPercent now means a percentage in [0,100], consistent with roulette coordinates; callers assuming the old malformed fractions must migrate. Zero population/rate/score/shape errors now throw as documented, and out-of-range setCreature throws instead of silently ignoring the call. The example's target and character-search purpose are preserved, but stochastic trajectories necessarily change after correcting selection.

## Verification of the completed implementation

Exact commands from repository root:

```sh
../.tools/bin/cmake -S . -B ../build-release -DCMAKE_BUILD_TYPE=Release
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake -S . -B ../build-debug -DCMAKE_BUILD_TYPE=Debug
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake -S . -B ../build-sanitize -DCMAKE_BUILD_TYPE=Debug -DGENETIK_SANITIZERS=ON
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure
../build-release/tests/genetik_tests
timeout 10s ../build-release/genetik_demo 42 1000 > ../hardened-demo.log
```

Outcomes: **5/5 CTest tests passed in each configuration**. Seven independent public-header compilation checks passed in every build. Contract executable reports **255 checks passed**. ASan+UBSan reported no findings in the instrumented contracts and bounded demo; leak detection uses the platform default. The downstream test configures/builds/runs a separate ordinary Release consumer in all three configurations; that consumer is intentionally not instrumented by the parent sanitizer option. Debug/Release main targets and all contract checks execute normally; no assert-only tests. As a negative control, a temporary copy of the contract source deliberately expected fitness 0 for the perfect target, compiled using `g++ -std=c++20 -O2 -DNDEBUG -Igenetik ../negative-contracts.cpp -o ../negative-contracts`; it returned 1 with `FAILED after 1 checks`, confirming Release checks report failures. The production source was not changed.

CTest cases: contracts; seed-42 demo with 20-generation cap; zero-generation demo; invalid CLI rejection; downstream configure/build/run. Every case has a timeout, and none requires finding the target. The 1,000-generation Release demo returned 0, stopped at generation 1,000 with `generation_limit`, and best-seen fitness 0.351 (three decimal places). It did **not** solve the target. This is an observation, not a quality threshold or comparative search study.

Supported repeatability conditions: same library source/build, toolchain and standard library, input, seed, configuration, call ordering, deterministic fitness and serial execution. Tests compare exact genomes, not merely rounded output. Different runs with different contexts cannot consume one another's random draws. Standard distributions and floating arithmetic are not guaranteed identical across standard libraries/build flags/platforms. Elapsed time is deliberately not repeatable.

Milestone acceptance: starting-state inventory and unchanged build exercised; C++20 toolchain verified; meaningful Release/Debug/CTest foundation; run-owned explicit randomness across all four stochastic stages; regression coverage for corrected defects; bounded preserved example; supported local sanitizer run; standalone-header/downstream checks; compatibility and deferred work documented. No blocker remains for this milestone. Prime expressions, fitting, baselines, and extrapolation remain future capabilities.

## Checkpoint reconciliation before the fitness-state milestone

The next assignment rechecked the uncommitted tree against this report. All modified/untracked repository files belonged to this baseline (including the reproduction fixture and downstream consumer); no unrelated edits were found. Existing Debug and sanitizer LastTest.log evidence was present. Rebuilding Release required no recompilation and its five CTest tests passed again (including the final stopping-reason assertions). This baseline is committed separately before fitness-state/publication work; the following milestone records the checkpoint identifier.

## Fitness-validity and generation-publication milestone

### Checkpoint and scope

Baseline checkpoint: **99907b03a50e48cb638fa26a9b973b91a2d63754** (`Establish C++20 baseline and explicit run-owned randomness`). It includes all reviewed baseline files and no unrelated edits. The working tree was clean immediately after that commit. Existing toolchain, build directories, runtime-check test harness and CTest conventions were reused. No dependencies, threading, general transaction system or new cancellation framework were introduced.

### Fitness and mutation contract

[Fitness.hpp](../genetik/Fitness.hpp) separates `FitnessState::{Unevaluated,Valid,Invalid}` from a numerical score. `Fitness::valid(double)` admits all finite scores, including zero and negatives, and rejects NaN/infinity. `Fitness::invalid()` is an ordinary expected mathematical outcome. `Fitness::fromScore(double)` adapts numerical evaluators, mapping non-finite output to Invalid. It never catches exceptions. Reading a score from Unevaluated or Invalid throws `logic_error`; these states cannot enter ordinary score comparisons. Objective direction is a separate population setting, default Maximize, optionally Minimize.

[Creature_i.hpp](../genetik/Creature_i.hpp) now owns private scalar-genome and fitness storage. Candidates derive from it and implement `Fitness evaluate() const` and `T mutatedGene(unsigned int, std::mt19937&) const`, not cache setters. The base's final `calculateFitness()` clears old fitness, invokes the evaluator, and requires Valid or Invalid. Returning Unevaluated is a programming/operational failure. An exception leaves a standalone candidate Unevaluated, never carrying an old score.

Supported scalar genes are arithmetic values; pointers and compound/shared mutable gene objects are deliberately rejected at compile time. The default population requires a candidate derived from this base. This is a source compatibility restriction that makes the current ownership guarantee enforceable. Expression trees will have a separately validated owned representation, not shared mutable nodes smuggled into this scalar base. Custom evaluator metadata must not mutate candidate genomes/cache or change the objective behind the const evaluator contract; external diagnostics/side effects are outside the preservation guarantee.

Every supported edit (`setGene`, `mutateGene`, `replaceChromosome`) clears fitness to Unevaluated and clears selection metadata. Reads return gene values or a chromosome copy. No mutable/const reference to internal genome storage escapes. Genome inputs are copied even when passed with `std::move`, preventing retained writable input references from changing the owned genome. The concrete character constructor follows the same rule. Out-of-range reads/writes now throw rather than silently return/ignore; rejected edits leave the original genome/cache unchanged. Copies transfer a value-owned matching genome/evaluation pair; moves also invalidate the moved-from cache. Whole-candidate copying is not a new evaluation. The objective and all its input data must remain fixed during a run; changing them requires a fresh run. No evaluator-versioning system is promised.

The character evaluator returns Invalid for a wrong target length. Other mathematical domain failures must similarly be returned explicitly by custom evaluators; arbitrary exceptions, including `domain_error`, are never automatically interpreted as mathematical invalidity. A numerical evaluator can migrate from `score = compute()` to `return Fitness::fromScore(compute())`; zero/negative results remain valid.

### Publication and best-candidate contract

[Population.hpp](../genetik/Population.hpp) privately owns a state containing members, committed generation number and best seen over committed generations. Generation zero is the first viable initial publication. Every later successful publication increments its number. `bestCreature()` is the best valid member of the current generation; `bestSeen()` returns an optional copy of the best valid candidate from any committed generation, keeping the previous archive entry on equal scores. A worse viable generation can commit without lowering that archive. Invalid members never become best.

Initialization, resizing, explicit replacement and evolution use the same staging path. It detaches all supplied candidate objects (even rvalue vectors with retained element references), evaluates **every** candidate, validates states, ranks members, computes roulette metadata and prepares the archive/state allocation before the commit point. Parent genomes and cached evaluations are never evaluated or edited in place. `unique_ptr::swap` publishes the entire prepared state without throwing. Candidate destructors must be nonthrowing. No potentially throwing allocation/copying runs between that swap and incrementing the algorithm's successful-evolution counter; disposing the old state requires nonthrowing candidate destructors. This is a serial, non-reentrant API boundary, not a concurrent snapshot/atomicity guarantee; user hooks must not reenter the same run.

`setCreature()` now queues an independently owned edit batch and invalidates the replacement. `updateValues()` evaluates/publishes the batch; without edits it explicitly reevaluates a detached copy of the current population. Pending edits make current-best, mean and parent-selection/evolution queries fail rather than use stale scores. `creatures()` always returns a detached snapshot of the **committed** population, and `bestSeen()` always reports committed history, including while edits are pending. An exception during edit preparation discards the pending batch; an attempted publication also consumes/discards that batch on failure. Public population input containers are copied to prevent retained candidate references from mutating committed state.

Population copying/moving is disabled to keep ownership/publication identity unambiguous. Custom population implementations used with the default GA must now supply `requireReady`, `generation` and `publishGeneration` with the same guarantees, in addition to the shared run context and snapshot methods. The old protected Population_i vector/fitness helper hooks and protected Creature_i genome/cache fields are removed; the interfaces describe the new state boundary.

### Invalidity and failure policies

| Situation | Behavior |
| --- | --- |
| Mixed Valid/Invalid members | Commit if at least one is Valid. Invalid members are kept for diagnostics, ranked before valid members and given zero selection weight. Mean uses valid members only; population size is preserved. |
| One valid member, including score zero | It is the only selectable parent and can fill an entire successor. Singleton populations and odd population sizes remain supported. |
| All-invalid initial candidates | Evaluate the finite supplied population once; return `lastOutcome()==NoViableCandidates`, with no committed population, generation or best. `bestSeen()` is empty and `creatures()` is empty; score/selection queries throw. `evolve()` returns NoViableCandidates without retry/draws/work. Explicit `setSize()` or `publishGeneration()` may attempt initialization again. |
| All-invalid successor | Evaluate the requested finite successor once; return NoViableCandidates. Preserve the prior committed population, fitness/selection metadata, generation, archive and GA evolution count. No regeneration loop. |
| Unevaluated at an evaluation/selection boundary | Reject. The final evaluation wrapper detects a custom evaluator returning Unevaluated; pending edit boundaries reject stale selection/best queries. Never treat an uninitialized score as zero. |
| Unexpected evaluator exception | Stop evaluating immediately, discard offspring and preserve committed state. Propagate with nested candidate index and attempted-call number; evolution adds completed-evolution context. Preserve the original exception object/type, including non-standard exceptions. Set lastOutcome to OperationalFailure where the attempt was handled. |
| Variation/copy/allocation/preparation failure | No state publication. Propagate actual error, with generation context from evolve. A failure before entering population evaluation is recorded by the algorithm, not falsely as a completed population attempt. |

`GeneticAlgorithm_i::evolve()` now returns `GenerationOutcome` rather than void. Expected outcomes are Committed or NoViableCandidates; operational failures propagate as exceptions and set the algorithm's lastOutcome. `evolutions()` counts only successor generations committed by that algorithm instance. Population `generation()` counts publications, including explicit edits/resizes, independently of the algorithm instance. No state has generation zero until viable initialization commits.

Roulette changed from magnitude weighting to **linear-rank weighting**, independent of numerical sign/scale and objective direction: valid candidates sorted worst to best receive ranks 1..N; equal-score groups receive their average rank. Weights normalize to percentages, and ranges cover [0,100) with the final endpoint fixed at 100. All valid candidates have positive mathematical weights; all-equal scores select uniformly. Invalids receive zero. This avoids subtracting extreme double scores and avoids sign-dependent validity. It intentionally changes trajectories from the baseline. Fitness/percentage/range accessors are final to prevent custom candidates overriding score-state guarantees. The standard library's floating distributions/rounding still limit cross-platform reproducibility; supported conditions remain the same build/toolchain, fixed input/objective/configuration/seed/call order and deterministic serial evaluation.

### RNG, attempted work, stopping and callbacks

The guarantee preserves **committed population state**, not the whole run. Consumed RNG draws stay consumed on any failed attempt. `RandomContext::evaluationAttempts()` is a monotonically increasing uint64 counter, incremented immediately before each population-driven `calculateFitness()` call. Valid, Invalid and throwing evaluations all count; discarded generations do not refund work. Counter overflow is rejected before another evaluator is invoked. The context outlives construction failures, so attempted initialization work remains inspectable even if a Population constructor throws. Standalone manual `candidate.calculateFitness()` calls outside a population are not run-managed work and are not included in that counter.

There is no evaluator-call budget in the existing implementation. Future evaluation budgets must check/charge attempts at this same boundary, counting invalid and throwing calls and never refunding discarded work. The demo's existing generation limit remains checked between completed evolve calls; failed attempts return a no-viable outcome or throw, with no implicit retry. Evaluation hooks and variation hooks run before publication. External evaluator side effects are not rolled back.

Source inspection confirmed no cancellation token, progress observer or library stop callback exists. No new framework was added. Callers can cooperatively stop between evolve calls; there is no promise to interrupt a blocking evaluator. Demo progress printing occurs after publication; a caller's own reporting/callback failure after evolve returns cannot undo the generation that already committed. The demo now checks no-viable outcomes, reads committed best seen, prints linear-rank/objective configuration and attempted calls, and prints nested operational error causes.

### Focused verification

The new runtime-check `genetik_fitness_publication` suite covers valid zero/negative and explicit invalid/unevaluated states; all supported edits and retained references; scalar/candidate-buffer detachment; copying/moving; non-finite adapters; both objective directions and extreme finite scores; mixed validity; one viable parent; all-invalid initialization/successor; pending-edit rejection; exact preservation of genomes, Fitness objects, roulette metadata, generation and committed archive; injected evaluator, copy and archive-preparation failures; preserved standard/non-standard nested errors; and exact consumed-RNG/attempt-count behavior. A mirrored successful attempt constructs identical offspring to verify RNG consumption after an aborted evaluator attempt. An improved first offspring followed by a throwing second proves partial-generation improvements never enter the archive. Existing contract tests are adapted to states/rank weighting; Release checks remain runtime failures, independent of NDEBUG.

Exact commands from repository root, reusing the verified cached Release/Debug and Debug+GENETIK_SANITIZERS=ON configurations:

```sh
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure
../build-release/tests/genetik_tests
../build-release/tests/genetik_generation_tests
timeout 10s ../build-release/genetik_demo 42 1000 > ../fitness-demo.log
```

For a fresh build workspace, use the configure commands in the earlier verification section; CMake automatically regenerates the reused directories for the added test/header. Final outcomes follow. No stochastic convergence assertion is part of any correctness test.

Final qualification: **6/6 CTest cases passed in Release, Debug and GCC ASan+UBSan Debug**, with no sanitizer diagnostics. Both runtime-check executables run in Release: **259 existing contract checks and 270 fitness/publication checks**. All eight public-header compilation targets passed. The downstream consumer configures/builds/runs separately and now checks successful publication, valid committed best, generation counters and evaluation accounting; it remains an ordinary Release consumer even when the parent is instrumented.

The additional syntax/warning review `g++ -std=c++20 -Wall -Wextra -Wpedantic -Igenetik -fsyntax-only tests/tests.cpp tests/generation.cpp demo.cpp` passed without warnings. `git -c core.whitespace=cr-at-eol diff --check` passed (preserving the existing interface header's CRLF convention). One routine development compile failed on a mixed-type multi-declarator `auto` in a test; separating the declarations fixed it before qualification. Retained-reference tests were extended during review to cover both gene buffers and whole-candidate vectors, and all affected configurations were rebuilt after the fix.

The bounded seed-42 Release example returned 0 at generation 1,000 with `generation_limit`, **100,100 attempted evaluations** (100 initialization + 100 × 1,000 offspring), best-seen fitness 0.892 and final-generation best 0.865 (three decimals). It did not find the target. This is an observation, not a convergence assertion or a search-quality comparison with the previous selection policy.

Milestone acceptance is satisfied: explicit enforceable cache states; ordinary invalid outcomes separate from unexpected nested exceptions; detached staging and no-throw publication of genomes/fitness/generation/archive; defined mixed/all-invalid/single-valid behavior; exact attempted-work and consumed-RNG failure policy; no stale best/selection through supported edit paths; deterministic failure/alias tests; relevant platform verification; durable migration/roadmap updates. No concrete blocker remains for this milestone. General budgets, cancellation, observer callbacks, parameter fitting and expression candidates are still future work.

Recommended next scope: a value-owned bounded expression representation and strict double CPU evaluator for variables, finite constants, +, -, *. Acceptance is deterministic structural/limit and arithmetic-error tests, equivalent exported semantics, bounded evaluation work, no target-data access, no mutable aliases, and Release/sanitizer/downstream qualification. Integrate structure evolution afterward, using y = x*x + x + 1 as the first symbolic-regression demonstration. Preserve ordinary division/logarithms, bounded constant fitting and the full indexed-prime fitting/extrapolation protocol in the roadmap.

## Bounded expression value and strict CPU evaluator milestone

### Starting point and integration review

Started from clean **be3e7f62f9882d646fda1d79c045a31ff014fcdc**, with baseline checkpoint **99907b03a50e48cb638fa26a9b973b91a2d63754** present. Read the repository guidance, milestone evidence/roadmap and current public scalar/fitness/population/GA APIs. Reused GCC/libstdc++ 13, C++20, workspace CMake 4.4.4 and the existing three out-of-source build directories. No unrelated edits were present. No dependency or evolutionary-core API changes were needed for expression values.

Actual integration findings: Creature_i has private/final arithmetic-scalar genome operations and Population statically requires that base. Its virtual size-based initializer also instantiates T(engine). GA's scalar virtual crossover/mutation definitions require chromosome/gene operations and equal lengths. A typed variable-size tree cannot legitimately enter that pipeline through a dummy scalar chromosome. The smallest concrete next bridge—common evaluated-candidate base, conditional initialization, defaulted variation policy and an owned expression candidate using the same publication path—is specified in [EXPRESSIONS.md](EXPRESSIONS.md#concrete-next-integration-change). It is documented, not implemented in this milestone.

### Delivered contracts

[Expression.hpp](../genetik/Expression.hpp) provides typed postfix nodes, validated value-owned Expression construction/factories, constant-derived replacements, detached node inspection, structural counts/limits, explicit ExpressionResult evaluation and deterministic text export. The initial grammar is variables, finite double constants, +, -, *. Construction enforces exactly one complete tree, arity and variable schema with no reference edges/cycles/fragments, inclusive node/depth bounds, and an optional finite absolute-constant bound. Defaults are 1,023 nodes, depth 64 with leaf depth 1, and largest finite absolute constant. Checks avoid wrapping count/depth arithmetic even with SIZE_MAX configured.

Public inputs are copied even when supplied as rvalues, to prevent retained token references from bypassing validation. No mutable internal access is exposed. Copies/derived values are independent; failed construction/replacement preserves existing valid values. Moved-from values are for assignment/swap/destruction, with execution/inspection rejecting the empty state. All traversals and destruction are iterative/flat. Evaluation is bounded by O(bindings + nodes) time and O(nodes) temporary storage; export is O(nodes + output length) with O(depth + output length) temporary storage. See EXPRESSIONS.md for construction/copy details and the cost of repeatedly composing a growing tree.

Successful evaluation returns a finite double, including negative/signed-zero/subnormal values. Non-finite intermediate arithmetic returns MathematicalInvalidity with the first failing postfix index; accessing its numerical value throws. Malformed construction/configuration, wrong/non-finite bindings and operational/allocation failures are exceptions, not poor scores. Evaluators have no RNG or dataset access. Postfix ascending order preserves left/right grouping and operand order; each arithmetic node rounds/stores/checks its own result. Overflow under multiplication by zero stays invalid. Under default non-trapping round-to-nearest/gradual-underflow settings, finite underflow to signed zero succeeds. No floating-point environment manager or alternate rounding-mode qualification is provided.

Faithful export is fully parenthesized infix with indexed variable names x0,x1,..., locale-independent max_digits10 decimal constants, explicit negative-constant parentheses and signed-zero preservation. It describes the same mathematical tree/constant values; a general parser/JIT/serialization layer or cross-language execution guarantee is not provided. The prime experiment's input n remains a later one-based data index bound to the single variable x0, not a change to schema indexing.

Compatibility/build implications: genetik_lib remains header-only, with existing scalar/core APIs unchanged. Because evaluation is public-header code, GCC/Clang consumers now inherit `-fno-fast-math -ffp-contract=off`; MSVC gets `/fp:strict` but is not qualified here. Other compilers require equivalent options and qualification. The header rejects known fast/finite-only configurations and statically requires native IEEE binary64/FLT_EVAL_METHOD==0; it cannot detect every unsafe individual flag. All using translation units must retain the documented settings. Tests inspect actual reference behavior, including a product/addition rounding counterexample that would differ under FMA. Alternate FP control modes, compilers, architectures and bitwise cross-platform reproducibility remain unqualified.

The new public [expression_demo.cpp](../expression_demo.cpp) explicitly demonstrates a **manually constructed** x*x+x+1 candidate, 7 nodes and depth 4. Outputs are 3,1,1,3,7 at -2,-1,0,1,2. It handles an explicit invalid outcome for zero times an overflowing inner product. It does not perform fitting or structure evolution.

### Verification and routine repair

Focused public-API tests exercise valid constants/variables, exact known arithmetic and subtraction grouping/order, binding count/schema/finite errors (including unused bindings), constant/configuration errors, per-node overflow/first-failure order, zero-masked overflow, separate-node rounding/no FMA, signed zero, subnormals and underflow, exact/exceeded node/depth/constant limits, malformed postfix fragments/arity, SIZE_MAX arithmetic, copy/alias safety, derived/replacement failure preservation, moved-from recovery, faithful groupings, bit-exact constant spellings and a custom decimal locale. Left/right skew trees at depth 20,001 (40,001 nodes) exercise iterative construction, evaluation, export, copy and destruction. Nine public headers compile independently; the downstream consumer constructs, evaluates, inspects and prints the polynomial through the public CMake target.

The first Release and Debug suites passed. The initial sanitizer expression test exceeded its 30-second timeout. A temporary instrumented copy in the build workspace localized the time to the deliberate deep-tree stress cases: left validation ~4.4 s, evaluation ~2.7 s, export ~10.3 s; right construction/evaluation/export and copy completed the entire test in ~41.1 s with no diagnostic. This was a bounded workload/time allowance issue on this Jetson, not a mathematical/ownership failure. Increased only that test's finite timeout to 60 seconds and reran the final tree, retaining both deep tests. A warning check also found and fixed an unnecessary copied vector in a test range loop; an independent-returned-limits check and a fail-closed unsupported-kind guard were added during review. All affected builds were repeated after those changes.

Exact final commands from repository root:

```sh
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure
g++ -std=c++20 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic -Igenetik -fsyntax-only tests/tests.cpp tests/generation.cpp tests/expressions.cpp demo.cpp expression_demo.cpp
../build-release/genetik_expression_demo
timeout 10s ../build-release/genetik_demo 42 1000 > ../expression-milestone-character.log
git -c core.whitespace=cr-at-eol diff --check
```

Cached configurations remain Release, Debug, and Debug+GENETIK_SANITIZERS=ON respectively; CMake regenerates the existing directories for added targets. Fresh-workspace configure commands remain those in the baseline section. Generated Release and downstream consumer compile flags include C++20 and both strict FP options. The downstream fixture remains an ordinary separate Release build even when run from the parent sanitizer suite. The unsafe-mode CTest compiles the public expression header under -ffast-math and -ffinite-math-only and requires the intended rejection diagnostic, rather than treating arbitrary compiler failures as success.

Final outcomes: **9/9 CTest cases passed in Release, Debug and ASan+UBSan Debug**. The final sanitizer deep-tree contract completed in 40.72 seconds under its 60-second cap; no sanitizer diagnostics were reported. The existing fitness/publication and scalar suites passed unchanged. Nine standalone-header targets and the separate downstream C++20 Release consumer passed. The complete warning command above passed without warnings after the range-loop fix. Public expression-example output matched every requested table value and reported invalidity at postfix node 3. The bounded character example returned 0 at generation 1,000 with `generation_limit` and 100,100 attempted evaluations, preserving its prior behavior. No correctness test relies on stochastic discovery.

Acceptance is satisfied for a downstream consumer constructing, evaluating, inspecting and faithfully printing bounded expressions, with ownership, invalid-result, numerical/build and export contracts verified. Limits remain: no expression search/candidate bridge, parser, division/logarithms, constant fitting, dataset loss, alternate platform/FP-environment qualification or allocation-fault injection. No concrete blocker remains for this evaluator milestone. The documented next task is the small representation-independent candidate/variation bridge and bounded polynomial structure evolution; all later prime fitting/validation/extrapolation, complexity, constant fitting and CPU-first profiling requirements remain in the roadmap.

## Polynomial symbolic-regression milestone

### Starting state, design and public compatibility

Started clean at **5028904382ad5b88263d5675c6497701c686138d**. Reviewed applicable AGENTS, baseline, expression guide/bridge and roadmap plus actual scalar/evaluation/selection/publication code. No unrelated changes were present. Reused the existing Jetson/GCC 13 C++20 toolchain, strict FP settings, workspace-local CMake, dependency-free runtime-check tests and build directories.

The integration is implemented without a second evolution transaction: `EvaluatedCandidate` extracts representation-independent private Fitness/roulette state and final evaluation/invalidation operations; scalar Creature_i retains its private arithmetic vector/final edit methods. New ExpressionCandidate owns a validated Expression and faithful text plus a shared immutable, deeply copied RegressionProblem. Base cache copying/assignment is protected to prevent forging a matching fitness through a public sliced base assignment. All expression installation work validates/copies first and uses no-throw swaps before clearing fitness; copies/offspring/elites/archive own independent genomes.

Population now admits the common base and optionally a stateless Ordering; default scalar ordering/API remains available. A fixed shared problem identity (including after all-invalid initialization) prevents mixing/change of expression evaluation problems. Size-based initialization conditionally requires C(engine); expression users use a configured factory/vector initialization, and an unavailable setSize is an explicit API error. The existing ranking/staging/validation/archive/ownership swap is reused. Its state adds bestGeneration, committed together with the archive. GA adds a defaulted Variation policy (old scalar bodies extracted to ScalarVariation), optional bounded elitism (default zero for compatibility), and attempt count. Existing character construction/two-template-argument usage remains unchanged.

The generic symbolicRegression function only supplies initialization and boundary stopping/budgets around this shared GA. Dataset rows, loss and hard expression limits belong to an immutable problem; changes require a new run. RegressionOrdering requires Minimize, comparing exact finite MSE, then node count, then faithful text. This complete ordering drives rank weights, elite selection and archive updates; numerical recovery tolerance never enters a sort. Every slot including elites/unchanged copies is reevaluated. Returned best is exclusively from committed generations, including generation zero if random initialization already fits.

Regression loss validates data before search and checks every prediction, residual, square, sequential double sum and final full-count mean. Invalid arithmetic yields Fitness::invalid with no partial metric/score, including sum overflow even when a scaled mathematical mean could be finite. API errors and unexpected exceptions are propagated with existing nested context; no catch-all turns them into invalid scores. Candidate-call accounting charges initialization, invalid results and throwing calls. No whole-run rollback is claimed: draws/proposals/evaluator calls remain consumed after a discarded generation.

Typed postfix initialization and subtree mutation/crossover are iterative, seeded entirely by RandomContext, support leaves/root/internal replacement and unequal sizes, and enforce full-tree destination depth/node/constant limits. Generation uses finite child budgets directly; variation has a finite proposal cap and unchanged-parent fallback on structural infeasibility. Operational failures remain exceptions. Statistics distinguish proposals, structural rejection, fallback and actual changes; counts are not refunded on failure. The high-level driver checks a full slot set fits before initialization/each generation, so no budget-driven partial population is published. With 128 slots and <=250 attempts the maximum is 128*(1+250)=32,128 candidate calls.

Full API/ownership/numerical/budget semantics and minimal public usage are in [SYMBOLIC_REGRESSION.md](SYMBOLIC_REGRESSION.md). Existing public strict expression/evaluator semantics and the later prime protocol are preserved. No cancellation, threading, general observer framework, continuous fitter, division/logarithm or prime regression is implemented here.

### Deterministic qualification

Added focused tests for independently understood zero target MSE and f(x)=x MSE 3; malformed/non-finite datasets/configuration/schema; invalid prediction/residual/square/sum and no favorable partial-data score; data/input alias detachment; fixed problem identity; controlled replacement/cache preservation; root/internal unequal-size subtree operations/subtraction order; destination depth and node boundaries; bounded fallback/rejection and actual node-count changes; parent/archive preservation; injected variation and post-work evaluator exceptions with retained cause/context, RNG/call/attempt accounting and committed-state preservation; all-invalid initialization/successors; one viable elite; complexity/text ordering across selection/archive; singleton Minimize validation; budget refusal before incomplete initialization/generation; generation-zero fit; and independent identical-seed short searches matching expression/fitness/discovery/stop/counters/engine state. The fractional-target short fixture cannot fit exactly with integer terminals at x=0,1, and must complete its three configured generations; no test depends on stochastic recovery.

A preliminary operator test failed because its chosen terminal-only donor could not violate the recipient limits; corrected the fixture to use nonterminal parents of different sizes, then observed bounded rejection/fallback. This was a test-fixture defect, not an ineffective operator or campaign configuration adjustment. Later review added variable-only grammar support and early singleton direction validation, both outside the default campaign's unchanged random stream/settings. The final test-only stop-reason repeatability assertion was rebuilt and rerun in all configurations after full-suite qualification. No scalar regression or production search defect was exposed by the checks.

Exact commands from repository root (reuse the recorded cached configurations):

```sh
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure
g++ -std=c++20 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic -Igenetik -fsyntax-only tests/tests.cpp tests/generation.cpp tests/expressions.cpp tests/symbolic_regression.cpp demo.cpp expression_demo.cpp polynomial_regression.cpp
timeout 10s ../build-release/genetik_demo 42 1000 > ../polynomial-milestone-character.log
timeout 180s ../build-release/genetik_polynomial_regression > docs/polynomial-campaign-initial.txt
git -c core.whitespace=cr-at-eol diff --check
```

Final assertion-only qualification used these commands for each of build-release, build-debug and build-sanitize (no library source changed after the full-suite pass):

```sh
../.tools/bin/cmake --build ../build-release --target genetik_symbolic_tests -j2
../.tools/bin/ctest --test-dir ../build-release -R '^genetik_symbolic_contracts$' --output-on-failure
../.tools/bin/cmake --build ../build-debug --target genetik_symbolic_tests -j2
../.tools/bin/ctest --test-dir ../build-debug -R '^genetik_symbolic_contracts$' --output-on-failure
../.tools/bin/cmake --build ../build-sanitize --target genetik_symbolic_tests -j2
../.tools/bin/ctest --test-dir ../build-sanitize -R '^genetik_symbolic_contracts$' --output-on-failure
```

Full suites: **10/10 passed in Release, Debug and ASan+UBSan**, with no sanitizer diagnostics. Short symbolic sanitizer test ~1.4 s; the retained deep-expression stress test ~40.7 s under its existing 60-second cap. Fourteen standalone-header targets passed. The separate downstream C++20 Release consumer now runs actual expression evolution with a bounded one-generation problem, then checks the existing expression/GA interfaces. The full five-seed campaign runs only in Release, outside CTest. Warning review passed without diagnostics. The bounded character example retained best-seen 0.892, generation_limit at 1,000 and 100,100 candidate calls; no target-discovery assertion was used. A no-hidden-RNG source search found no random_device/srand/rand in the library (rg exit 1 denotes no matches).

### Search evidence, independent of engineering acceptance

Remaining campaign choices were declared in docs/POLYNOMIAL_CAMPAIGN.md before execution. The initial campaign achieved zero training MSE and zero training/interior/exterior maximum absolute error for **all five seeds**. Seeds 0,1,3 improved from initial MSE 1 after 3,3,4 published generations; seeds 2,42 naturally fit during initialization and correctly report discovery generation zero. No configuration adjustment/restart/second search campaign was used. The training-based no-adjustment decision and representative selection preceded evaluation-only metrics. Representative seed 42 exports `((x0 * x0) + (x0 + 1.0))`.

All five actual exports, size/depth/discovery counts, attempted/published generations, candidate calls, variation rejection/fallback/change counters, runtimes and recovery flags are retained in [POLYNOMIAL_CAMPAIGN.md](POLYNOMIAL_CAMPAIGN.md) and its unedited raw log. Total search work was 1,920 candidate calls; post-run metrics separately used 15 candidate/dataset metric calls over 185 samples. Each seed stayed below its 32,128 cap. These numerical results on declared sample sets are not a proof of algebraic equivalence or a general search-success guarantee.

Engineering acceptance is satisfied: expressions evolve through the shared engine with the required ownership, validity, ordering, checked loss, bounded variation/budget and failure/publication contracts. The bounded search evidence supports proceeding to continuous constant fitting, with training-only optimization, rollback on invalid/failure/worsening and charged fitting work. Keep diagnostics for more difficult structure-search problems. No concrete blocker remains for this milestone. Division/log domain semantics, prime index/value data and frozen prefix/extrapolation reporting, complexity archives, package/platform qualification and CPU profiling before GPU work remain future roadmap requirements.

## Bounded continuous constant fitting — 2026-10-19

Started clean at `758651440f4d7a982f47bde944ecc5f9572a73de`; no unrelated
local changes. Reused Jetson GCC/libstdc++13, C++20, strict FP interface flags,
workspace CMake 4.4.4 and existing Release/Debug/ASan+UBSan directories. No build
requirements/dependencies changed. Inspected the candidate/variation extension,
immutable problem, publication and candidate-call budget paths before integration.

Implemented public `ConstantFitting.hpp`: deterministic bounded coordinate search
on independent postfix constant occurrences, fresh checked baseline, finite bounds,
strict improvement acceptance, retained local improvements on later invalid/worse
trials, explicit start/stop outcomes and separate improvement flag. Fixed expression
topology/limits and input value are preserved. No RNG, algebraic replacement,
reparse, target template or additional solver. Coupled all-zero parameters can
stall, as the regression test honestly demonstrates.

Added a default protected preparation hook to the existing final fitness wrapper;
ordinary const evaluate overrides and scalar behavior remain supported. An optional
trusted expression preparation service returns an exact expression/score pair,
installed by validated replacement and controlled cache installation. Driver and
manual users share the existing Population/GA publication transaction unchanged.
All initialization/offspring/elite/clone slots get fresh baselines and optional fits;
parents and archive values own their expressions. Mathematical invalidity remains
Invalid fitness. Operational exceptions retain original nested causes and attempt
context, discard private work and preserve committed state; consumed RNG/work
remain consumed. No retry, regeneration or second transaction loop was added.

New run-owned TrainingObjective exposes read-only counters for actual training-loss
and row attempts. Local loss/sweep/proposal limits are independent of global loss
and existing outer candidate-call limits. A local stop returns its best accepted
value. Global exhaustion inside a generation propagates the explicit budget type
through the transaction; only the high-level driver translates this type into
EvaluationBudget and returns committed state (or no initial best). No partially
refined/unpublished best leaks into the archive. TrainingLossThreshold is distinct
from ExactTrainingFit; old zero-stop and discrete defaults remain unchanged.

Continuous terminal sampling is optional, uses the existing run engine, and retains
the old discrete-only RNG sequence by default. Added fields at the end of grammar
configuration preserve prior positional aggregate initialization. The optional
preparation argument defaults empty. Existing maxEvaluations still means outer
candidate requests; maxTrainingLossAttempts controls actual loss calls including
fitting. P*(1+G) is no longer a total-loss bound in enabled mode.

API, numerical/ownership/failure/counter contracts and usage are in
[CONSTANT_FITTING.md](CONSTANT_FITTING.md). Complete frozen settings, all ten paired
results/expressions and raw traces are in
[CONSTANT_FITTING_CAMPAIGN.md](CONSTANT_FITTING_CAMPAIGN.md). No search setting
adjustment was needed. A final qualification rerun matched the first campaign's
expressions, metrics, work/traces/variation counts exactly (elapsed time excluded;
positive-threshold reporting label clarified). Fixed-structure dyadic/non-dyadic
fits used 44/324 loss calls (<=1,024), max errors on all declared ranges 0 and
<=7.92e-10. Enabled evolution recovered all five seeds on all three ranges at
1e-6; disabled recovered none, consuming more actual loss calls. These are sample
results, separate from correctness and not algebraic/global-optimum claims.

Exact verification commands from repository root (existing configurations):

```sh
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure
../build-release/tests/genetik_fitting_tests
timeout 600s ../build-release/genetik_constant_campaign
timeout 60s ../build-release/genetik_polynomial_regression
timeout 10s ../build-release/genetik_demo 42 1000
g++ -std=c++20 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic -Igenetik -fsyntax-only tests/constant_fitting.cpp constant_fitting_campaign.cpp tests/symbolic_regression.cpp tests/expressions.cpp tests/generation.cpp tests/tests.cpp demo.cpp polynomial_regression.cpp expression_demo.cpp
git -c core.whitespace=cr-at-eol diff --check
```

Final outcomes: **11/11 CTest cases passed in Release, Debug and ASan+UBSan**,
with no sanitizer diagnostics. All fifteen standalone public-header targets built.
The independent downstream Release consumer invokes the fitter and a short fitting-
enabled search through public CMake targets/headers (it is not parent-sanitizer
instrumented). Fitting contracts include the two independent quadratic fixtures,
known single-constant optimum, irrelevant/independent constants, invalid/threshold
starts, bounds, strict acceptance, all local stops, duplicates/unrepresentable/
nonfinite probes, retained matching loss/topology/limits/inputs, deterministic
repetition, continuous sampling, fitted inheritance/cache safety, all-invalid
initial/successor states and global/operational failures after partial work. A
caller-retained objective keeps detailed counters observable after high-level
exceptions; stale/mismatched run objectives are rejected.

Warning review and whitespace check passed without diagnostics. The retained
20,001-depth ASan expression stress remains within its 60-second test cap; the
new fitter suite takes about 20 seconds instrumented, including the independent
fixtures and short bounded integrated searches. Full paired campaigns are Release
only, outside CTest. The earlier discrete polynomial campaign still recovers all
five seeds exactly on all three sample ranges; its post-search training verification
now uses the accounted objective and reports search/global totals separately. The
character demo retains fitness .892 at generation_limit=1,000 with 100,100 outer
calls. No hidden random_device/srand/rand source was found in genetik.

Engineering acceptance is complete with no blocker. Coordinate coupling/stalling
and unoptimized repeated baseline/probe work are documented limitations, not a
promise of unique/global coefficient recovery. Recommended next scope: strict
ordinary division/logarithm nodes, domain handling, faithful export and configurable
variation/tests/downstream support; preserve the later prime protocol and profile
CPU workloads before proposing acceleration.

## Generic multivariate regression and numerical-file workflow — 2026-10-20

Started clean at accepted constant-fitting commit
`bff0f3e1c2a9f327efb8d787b76cf17d7b0cdb14`. No unrelated changes were present.
Reused the existing Jetson/GCC 13.3.0/libstdc++13, strict C++20 interface flags,
workspace CMake/CTest and Release/Debug/ASan+UBSan build directories. Inspected
AGENTS, regression/fitting documentation, roadmap, immutable row validation,
dimension schemas, initialization/mutation index sampling, typed subtree copying,
constant refinement, fitness/problem identity and the shared publication driver.

Confirmed the public library already supports positive arbitrary input dimensions
and one scalar target. Initialization/mutation sample all permitted variable indices;
loss/fitting iterate supplied rows and bind the whole input span without univariate
or polynomial assumptions. No core defect or restriction required a library change.
Zero-input constant-only problems remain supported for compatibility. Separate
immutable problems require separate candidate fitness/run identities even when
numerically equal; fresh expression measurement is allowed on matching dimensions.

Added application-layer table loading, a generic public-API file-search executable
and a two-input in-memory demonstration/reproducible data generator. No format or
file-system dependency was added to genetik_lib. The loader validates entire files,
including unused numerical columns, under inclusive byte/line/row/column/field
limits; it handles trimmed spaces/tabs, blank lines, CRLF and locale-independent
complete decimal/scientific parsing, with useful filename/physical-row/column
errors. Unsupported quoting, malformed headers/selections/widths/numbers and
unrepresentable/nonfinite values are rejected before search starts. Evaluation
files may be supplied upfront and validated, but are scored only after training
selection is frozen. No target generator/template reaches the library search.

Two/three-input tests cover independent binding changes, x2 export/generation by
initialization and mutation, subtree identity preservation, dimensions/finite data,
three-input constant fitting, separate-problem caches and preserved committed
state. Parsed/generated/reordered rows match in-memory binary64 values; bounded
same-seed searches match expressions, scores, counters, trace and RNG state.
CLI smoke tests prove malformed training/evaluation files fail before search_start
and evaluation paths do not change the selected expression. Downstream public-only
code now runs three-input search as well as previous fitting/scalar checks.

The frozen five-seed affine campaign recovered all five seeds on training/interior/
exterior at max error <=1e-6, with training-only selection and no configuration
adjustment. All reported candidates were discovered after initialization. Initial
raw/refined losses, all faithful expressions, work, times, stops and errors are in
[MULTIVARIATE_CAMPAIGN.md](MULTIVARIATE_CAMPAIGN.md) and its complete raw log. A
qualification rerun matched all existing non-time observables. The actual file
workflow with seed 42 matched its in-memory expression, score and 39,323 loss calls.
Correctness and this sample recovery evidence are separate conclusions.

Exact commands from repository root, using the established configurations:

```sh
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure
timeout 600s ../build-release/genetik_multivariate_demo
../build-release/genetik_multivariate_demo --generate ../multivariate-data
timeout 60s ../build-release/genetik_regression_search ../multivariate-data/training.csv --inputs first,second --target target --seed 42 --loss-budget 100000 --evaluate ../multivariate-data/interior.csv --evaluate ../multivariate-data/exterior.csv
timeout 10s ../build-release/genetik_demo 42 1000
g++ -std=c++20 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic -Igenetik -fsyntax-only regression_search.cpp multivariate_regression.cpp tests/multivariate.cpp tests/downstream/main.cpp tests/constant_fitting.cpp constant_fitting_campaign.cpp tests/symbolic_regression.cpp tests/expressions.cpp tests/generation.cpp tests/tests.cpp demo.cpp polynomial_regression.cpp expression_demo.cpp
git -c core.whitespace=cr-at-eol diff --check
```

Final qualification: **13/13 CTest cases passed in Release, Debug and ASan+UBSan**
(no sanitizer diagnostics). All fifteen public headers and all three new application
support headers compile standalone. The new sanitizer contract/smoke paths use
short bounded searches; the full five-seed campaign is Release only. The separate
downstream Release consumer uses only public headers/target, now including three-
input evolution; it remains uninstrumented by the parent sanitizer option.
Warning review and whitespace check passed without diagnostics. Character demo
retains best .892, generation_limit at 1,000 and 100,100 outer calls. No longer
previous campaigns were repeated because production library code is unchanged;
the existing deterministic fitting, discrete/scalar and publication suites all pass.

Generic API/data-input acceptance and measured multivariate recovery both pass.
No public API migration or compatibility break is required. The deliberately small
application format has finite resource limits, requires the same selected column
names across files, and rejects quoting/missing/categorical values. Library data
remain ordinary vectors and are not subject to file limits. General restrictions
remain fixed immutable problem/order per run, one scalar output, strict binary64,
heuristic structure/coordinate search and the current +,-,* grammar; there is no
serialization or exported-expression parser. No unresolved implementation blocker.

Next: strict ordinary division and natural logarithm across representation,
evaluation, variation, limits/export and domain tests. Then implement prime
regression as a generic-module example using the preserved protocol, and profile
representative CPU workloads alongside production delivery work.

## Strict division, natural logarithm and mixed arity — 2026-10-20

Started clean at `5436c194c58fe29684cf9a5a87739c7f16ecb990`, with no unrelated
working-tree changes. Read repository, expression, regression, fitting, multivariate
and roadmap contracts. Audited all expression/variation paths; the initial grammar
was deliberately binary-only. The extension required changing stack consumption,
depth/subtree metadata, export and child-node allocation, not the shared transaction.
Retained C++20, workspace CMake and existing Release/Debug/ASan+UBSan directories.
Qualified environment: Jetson AArch64, GCC 13.3.0/libstdc++13, glibc 2.39 and
`/lib/aarch64-linux-gnu/libm.so.6`, strict interface flags and default nearest,
gradual-underflow, nontrapping binary64. Runtime math library is now explicitly
part of the reproducibility conditions; no cross-platform bitwise-log claim.

Implemented ExpressionKind::Divide/Log (appended; old kinds retain values), public
unary factories and a shared expressionArity definition (0/1/2). Validated flat trees
include two-node unary/six-node mixed forms and arbitrary parity; every occurrence
counts once, leaf depth remains 1. All traversal/copy/destruction paths remain
iterative. Division checks either signed-zero denominator before native division;
log requires a positive finite child before std::log; both check finite results.
Subnormal operands/results and signed-zero underflow remain valid, with each parent
applying its own domain. Invalid children cannot be masked or algebraically canceled.
Structural errors remain API exceptions; log(-1) and 1/0 remain valid structures
with ordinary mathematical-invalid results. Export preserves grouping/order,
variable identities, full-precision constants and signed zero, using / and log(...).

Random initialization/mutation choose uniformly over feasible enabled entries,
allocate one child budget for log or the prior positive binary split, and never
retry for mathematical validity. Subtree operations check complete destination
depth and bounds. Existing +,-,* defaults/lists retain meaning and draw order;
the accepted binary-only affine campaign reproduced all prior expressions,
metrics and existing work/variation counts (time excluded). Empty C++ operator
lists still support terminals. CLI --operators add,sub,mul,div,log validates names,
empty/repeated/unknown selections before search and prints the effective policy.

Fitting/loss/private publication use the same engine unchanged. One-sweep a+log(c)
and a+x0/c fixtures retain a=1,c=1 (~.04 loss) after worse and domain-invalid probes.
Invalid starts still skip; earlier accepted progress survives local limits. The
existing operational fault fixture now carries log, the global-cap fixture carries
division, and all-invalid prepared cases use log/div domain failures. Consumed
randomness/loss/row work remains consumed; committed populations/cache/generation/
archive remain safe on abort. Added invalidLosses for all returned-invalid training
losses (baselines included), distinct from the existing invalid fitting-probe subset.
Throwing attempts remain operational, not returned-invalid mathematical results.

Generic file loading still permits finite zero/negative inputs. A shared application
report helper calculates checked MSE (early invalid stop) then inspects all prediction
rows for total invalid counts. CLI prints both row counts separately; it never
reports an early observed count as a total or scores only the valid subset. A
constructed log model is training-valid on two linear observations but invalid on
separate evaluation rows; reporting retains its exact training expression/cache.
This is a deterministic helper test, not a stochastic discovery gate. Those labels
come from the everywhere-defined line ln(2)*(x-1), not from the undefined log/div
target. Constructed-target zero/negative domain probes have no fabricated labels.

The frozen Release campaign and every seed are in
[STRICT_ARITHMETIC_CAMPAIGN.md](STRICT_ARITHMETIC_CAMPAIGN.md). Seeds 0,2,42 recovered
all three ranges at <=1e-6 max error; 1,3 exhausted 100,000 loss attempts and missed.
No configuration adjustment: the training-only decision found three successes
before held-out evaluation. All discoveries were after initialization. Seed 42 is
the training-selected representative. Seeds 1/3 discarded partial generation
attempts and correctly retained their last committed models while counting the
private work. Full-precision file seed 42 matched the in-memory expression, score
and all non-time work counters, including 51,624 loss attempts. No template, target
feature or mathematical-validity regeneration was supplied to initialization.

Development checks caught a mixed const-pointer initializer-list deduction in a
test (fixed with an explicit pointer array) and a test-only empty discrete list
with mixed continuous sampling (fixed to the intended variable-only configuration).
These were repaired before qualification; no evaluator/search defect was hidden as
poor fitness. CLI smoke tests remain search-quality agnostic.

Exact verification commands (existing configurations):

```sh
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure
timeout 600s ../build-release/genetik_multivariate_demo --arithmetic
../build-release/genetik_multivariate_demo --arithmetic --generate ../arithmetic-data
timeout 60s ../build-release/genetik_regression_search ../arithmetic-data/training.csv --inputs first,second --target target --seed 42 --loss-budget 100000 --operators add,sub,mul,div,log --evaluate ../arithmetic-data/interior.csv --evaluate ../arithmetic-data/exterior.csv
timeout 60s ../build-release/genetik_multivariate_demo
timeout 10s ../build-release/genetik_demo 42 1000
g++ -std=c++20 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic -Igenetik -fsyntax-only tests/strict_arithmetic.cpp regression_search.cpp multivariate_regression.cpp tests/multivariate.cpp tests/downstream/main.cpp tests/constant_fitting.cpp constant_fitting_campaign.cpp tests/symbolic_regression.cpp tests/expressions.cpp tests/generation.cpp tests/tests.cpp demo.cpp polynomial_regression.cpp expression_demo.cpp
git -c core.whitespace=cr-at-eol diff --check
```

Final qualification: **14/14 CTest cases passed in Release, Debug and ASan+UBSan**,
with no sanitizer diagnostics. Fifteen public headers and four application support
headers compile standalone, including the new generator header. The independent
downstream Release consumer constructs/evaluates/exports the six-node mixed tree
and runs expanded-grammar search through public headers/target; as before, that
separate fixture is not instrumented by the parent sanitizer option. The focused
arithmetic checks include signed zeros, denominator/log domains, subnormals,
quotient overflow, nested/masking/underflow errors, malformed and mixed/unary arity,
inclusive/exceeded limits, iterative 4,096-log stress, ownership and mixed variation.
Short search/fitting/file smoke tests cover the integrated sanitizer path; the full
five-seed campaign runs only in Release.

Warning review and whitespace check passed without diagnostics. The strict FP
negative-build tests still reject fast/finite-only math. Character demo preserves
fitness .892, generation_limit=1,000 and 100,100 outer calls. The default affine
campaign was repeated for the concrete binary draw-order regression risk and
matched accepted non-time results; no additional previous long campaign was needed.
The final expanded file run still matches the representative's score/expression
and counters after the reporting helper was shared with deterministic tests.

Compatibility: existing factories, typed kind values, default +,-,* grammar and
meaning of explicit lists remain supported; new unary factories/enum kinds and
invalidLosses are additive. No existing same-build trajectory change was necessary
for binary-only configurations. CLI reporting now distinguishes total invalid
predictions/errors and loss/prediction row passes; scripts consuming the old
example reporting_rows/invalid_predictions fields should use the new explicit
labels. Runtime libm is part of supported log reproducibility, with tolerant math
reference checks rather than cross-platform bitwise assertions.

Arithmetic/structural and fitter/publication/workflow acceptance pass independently
of the three recoveries/two misses. No unresolved engineering blocker. Stochastic
search and coordinate-fitting limitations remain documented. Next is prime
regression as another generic-module example using the preserved fitting/baseline/
validation/extrapolation protocol, followed by measured CPU profiling and production
delivery work.

## Prime-prefix application milestone (2026-10-20)

Starting revision fd1e7fe14b7a55c768edb528a2dec641a286aaaa, clean working tree.
Preserved the accepted strict-arithmetic campaign and all five outcomes unchanged.
Same Jetson AArch64, GCC 13.3.0/libstdc++13, qualified glibc/libm and workspace
CMake/CTest 4.4.4, C++20 strict FP interface flags; 15 GiB RAM (7.2 GiB available
at verification), no swap, 833 GiB free storage. No toolchain/dependency change.

Implemented application-only bounded sieve plus independent exhaustive trial-division
reference check/anchors, exact ordered user-prefix loading, full-precision numerical
CSV exports, fixed index/output scaling, checked int64 rounding and all-row raw-unit
metrics/sequence diagnostics. Added a small Householder QR reference solver and five
separate baselines. The public symbolic engine, immutable problems, candidate fitness,
coordinate fitter, run-owned randomness, budgets and generation publication are
unchanged. New genetik_prime_regression modes generate, pilot and campaign use those
public APIs. No oracle feature, target template, formula/residual seeding or prime
repair enters initialization or evaluation. Added CLI --loss-threshold for faithful
file reproduction; old default and all file numerical contracts stay unchanged.

Focused contracts cover exact indexing/anchors, malformed/incomplete/incorrect/reordered
prefixes and limits, fixed scaling/decode overflow, independent raw-unit errors,
rounding ties/negatives/int64 bounds/invalidity, denominators/adjacent pairs/duplicates,
full-range invalid aggregate behavior, known mean/affine/cubic/log QR fixtures,
frozen fitting-selected choice, exact generated/parsed doubles and same-seed bounded
public search counters. Existing failure, fitting, arithmetic and publication suites
are reused. A downstream public-only consumer additionally runs normalized sequence
rows with fitting and checks exact expression/fitness pairing. New application header
joins all 15 public and existing application standalone-header builds.

Reproducible verification (from repository):

```sh
../.tools/bin/cmake -S . -B ../build-release -DCMAKE_BUILD_TYPE=Release
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake -S . -B ../build-debug -DCMAKE_BUILD_TYPE=Debug
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake -S . -B ../build-sanitize -DCMAKE_BUILD_TYPE=Debug -DGENETIK_SANITIZERS=ON
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure

g++ -std=c++20 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic -Igenetik -fsyntax-only prime_regression.cpp tests/prime_regression.cpp regression_search.cpp tests/strict_arithmetic.cpp multivariate_regression.cpp tests/multivariate.cpp tests/downstream/main.cpp tests/constant_fitting.cpp constant_fitting_campaign.cpp tests/symbolic_regression.cpp tests/expressions.cpp tests/generation.cpp tests/tests.cpp demo.cpp polynomial_regression.cpp expression_demo.cpp
timeout 10s ../build-release/genetik_demo 42 1000
git -c core.whitespace=cr-at-eol diff --check
```

Release and Debug: all 16 CTest cases passed. Sanitizer results are recorded in
PRIME_CAMPAIGN.md. Includes public/downstream, invalid FP compilation, character,
generic-file and prime-file workflows. New sanitizer smoke is a short fixed-data
search/refinement and bounded file run, not the complete campaign. Warnings/whitespace
checks clean. Character example retains best-seen fitness .892, 1,000 published
generations and 100,100 outer evaluations. No older long campaign repeated because
no library arithmetic, fitting, variation or default search policy changed.

The runtime-only seed-2026 pilot selected 100,000 calls per seed with margin; five
campaign runs consumed 500,000 actual loss calls and 499,319,422 actual row attempts.
All five stopped at budget, publishing only 8–12 generations; no adjustment or
restart. Main runs plus file reproduction took ~49 search seconds versus the
conservative ~76-second estimate. All model and baseline choices were frozen before
extrapolation. Complete expressions, counters, trace/prediction tables, SHA-256 data
checksums and optional offline residual plot are retained in docs/prime-results.
See PRIME_CAMPAIGN.md for every seed, baseline and separately accounted work.

Engineering acceptance is separate from quality: seed 2 is fitting-selected and
approaches the fitted logarithmic baseline; it matches only 48/1000 fitting and
6/200 extrapolation rounded values. Other seeds are weaker and retained; there is
no exact-prime recovery or universal formula claim. The later range is now examined.
Recommended next: CPU profiling of refinement/evaluation and a bounded fitting
schedule/cache policy experiment to permit more structure exploration under the
same work contract, followed by production delivery. Stronger solvers, archives and
checkpoint/resume remain pending rather than implied by this application.

## Refinement profile and optional cadence milestone (2026-10-20)

Starting revision e944ffbafff164ed7c548fc7b66abe840ada77c1, clean tree; accepted
prime and strict-arithmetic reports/prediction artifacts preserved byte-for-byte.
Same C++20/GCC13.3/libstdc++13/qualified libm Jetson environment and workspace-local
CMake/CTest 4.4.4. Orin NX Seeed recomputer classic Super, AArch64 Linux
6.8.12-1021-tegra, schedutil governor observed; 15 GiB RAM, 9.4 GiB available,
no swap, 833 GiB free storage. Timings are single contemporaneous observations per
seed/mode with alternating pair order, not isolated/fixed-frequency benchmarks.

Added positive/default-one fitEveryGenerations and --fit-every across file and
prime/multivariate campaign workflows. Generation zero always receives enabled
fitting. Later eligibility uses the prospective committed population generation,
never attempts or origin. Ineligible slots receive fresh ordinary checked loss;
no fitness cache is reused. All eligible slots, including elites/clones, retain the
old coordinate fitter and local limits. Optional preparation context preserves old
custom two-argument preparations and scalar variation. Generation publication and
failure/RNG contracts are unchanged. Result fields add complete initial audit/work,
disjoint preparation accounting and work consumed without publication.

Added optional bounded exact-expression repeat observation and boundary timing;
no per-node/row clocks, no cache, optimizer change, rewrite or GPU. Actual objective
calls split into ordinary, fitting baseline and fitting probe categories; their
sum equals the old total. Exact comparisons include typed nodes, schema and constant
bits, including signed zero. Origin labels are diagnostic only. Optional Population
publication timing makes no clock calls by default. Full profile/control RNG/results/
work agree on both seed-2 workloads. All ten contemporaneous default controls match
accepted expressions, scores, stop reasons and every legacy non-time work counter.
All ten paired raw/prepared initial populations, fitness and initial counters agree.

The first audit implementation exposed a C++20 temporary-snapshot lifetime bug in
new initial-audit capture. Retaining the detached shared owner through iteration
fixed it; focused pair checks caught it before any evaluation reporting. Partial
runs are retained under docs/cadence-results/audit-failure and excluded from measured
policy conclusions. No search configuration changed. The initially oversized CLI
qualification cases also timed out under ASan at 60 and 120 s; reducing parser/smoke
work to 64/128 calls fixed this, with the original 60-second test ceiling restored.
Full scheduling/failure/budget assertions remain in deterministic C++ fixtures and
the Release file/in-memory reproduction; no search-quality assertion was weakened.

Verification commands (from repository):

```sh
../.tools/bin/cmake -S . -B ../build-release -DCMAKE_BUILD_TYPE=Release
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake -S . -B ../build-debug -DCMAKE_BUILD_TYPE=Debug
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake -S . -B ../build-sanitize -DCMAKE_BUILD_TYPE=Debug -DGENETIK_SANITIZERS=ON
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure

g++ -std=c++20 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic -Igenetik -fsyntax-only cadence_campaign.cpp prime_regression.cpp tests/prime_regression.cpp regression_search.cpp tests/strict_arithmetic.cpp multivariate_regression.cpp tests/multivariate.cpp tests/downstream/main.cpp tests/constant_fitting.cpp constant_fitting_campaign.cpp tests/symbolic_regression.cpp tests/expressions.cpp tests/generation.cpp tests/tests.cpp demo.cpp polynomial_regression.cpp expression_demo.cpp
timeout 10s ../build-release/genetik_demo 42 1000
../build-release/genetik_cadence_campaign --output NEW_RESULT_DIRECTORY
python3 examples/summarize_cadence.py NEW_RESULT_DIRECTORY
git -c core.whitespace=cr-at-eol diff --check
```

Focused checks reuse the existing ThrowingObjective/publication fixtures and establish
zero validation/disabled fitting, initialization equality, generations around 4,
scheduled/unscheduled failures and cap exhaustion, unchanged committed members and
archive, same schedule on retry, correct changed-expression score, inherited bounded
refitting progress (1 -> 2), all-invalid unscheduled behavior, disjoint actual work/
policy skips, same-seed/RNG and diagnostics equivalence, hard diagnostic capacity,
exact signed-zero identity, and omitted/explicit-one compatibility. CLI and downstream
fixtures exercise the option; standalone coverage includes the timing support header.
Character best-seen remains .892, 1,000 generations, 100,100 outer calls. Full-suite,
strict FP negative-build, warning and downstream outcomes are recorded with the new
campaign evidence; long historical campaigns were not otherwise repeated.

Complete new measurements are in FITTING_CADENCE_RESULTS.md and docs/cadence-results.
Twenty comparisons plus two separate seed-2 profiles, not a parameter sweep. Interval
4 publishes more generations for every seed, but has 3/2 fitting-MSE wins/losses on
both problems; arithmetic recovery falls from 3/5 to 2/5. Paired median runtime ratios
are 1.03461 prime and 1.29718 arithmetic (experimental/control). Interval 1 stays
default. Prime profiles spend ~97.5% in objective evaluation; exact repeated-input
no-change work is at least 4.472% of calls (6.897% arithmetic). Some prior fitter
outputs improve again, ruling out an already-optimized shortcut. Next: inspect
checked CPU evaluation workspace/allocation costs; exact reuse and stronger solvers
are separate future choices. Keep production delivery and any later fresh prime
assessment range explicit. No cache or policy tuning is implemented.

Final cadence qualification: 16/16 Release (2.07 s), 16/16 Debug (2.26 s),
16/16 ASan+UBSan (147.20 s), original CLI 60-second gate restored. Full warnings
and whitespace checks clean. Disjoint per-origin work verified on all twenty
campaign runs; complete 1–1200 tables and bounded nonworsening traces checked.
Checksums and raw successful CTest evidence: docs/cadence-results. The refreshed
prime default file run also reproduces all accepted non-time fields and expression;
helper comparison now handles additive counters without altering historical logs.

## Invocation-local checked-evaluation workspace milestone (2026-10-20)

Started clean at accepted cadence commit 94f05814afaaf26cc48fa4d22438cf56bb8943bc.
GCC/libstdc++ 13.3.0, glibc/libm 2.39, Linux 6.8.12-1021-tegra/AArch64,
workspace CMake/CTest 4.4.4; 8.6 GiB available RAM/no swap, 833 GiB free storage.
Read the current evaluation, loss, fitting, preparation, publication, reporting and
cadence contracts. No unrelated work was present.

Confirmed: every accepted per-row evaluate allocated/reserved/destroyed its own
operand vector; it did not zero-initialize the reserved doubles. The inclusive
97.5% objective profile had not isolated allocation. Instrumented ordinary-new
counts confirm 1,000 allocations/128,000 requested bytes for one prime representative
loss and 1,000/248,000 for a 31-node fixture. Early failures visit/allocate fewer
rows. A contained lexical workspace now allocates once per loss (128 and 248 bytes
respectively), without cross-loss reuse or mathematical caching. All-row example
reporting uses the same internal owner; direct public calls remain independent.

There is one private interpreter, with unchanged traversal, binding checks, strict
per-node binary64/domain checks and checked residual/square/sum/mean. Only operand
storage management changed. First growth remains after actual-loss and first-row
charges. Fault injection confirms nested bad_alloc, consumed work/RNG, preserved
committed population/generation/fitness/best, and later successful evaluation.
Focused growth/reset/ownership checks and the existing failure/budget suite pass.

Archived accepted headers/helpers were preserved before editing. Independent
reference artifacts compare exact finite uint64 representations, signed zeros,
invalid nodes, losses, reporting, API errors and attempted rows/work. Full prime
seed-2 and arithmetic seed-42 cadence-one replays compare initial/prepared values,
all deterministic counters, accepted fitting improvements, committed traces, stop
reason and final RNG state exactly; short cadence-four replay also matches.
No historical campaign artifacts were modified. This is optimization qualification,
not a new quality/tuning campaign. See EVALUATION_WORKSPACE.md and evaluation-results
for the complete corpus, raw paired batches, exact replay records and limitations.

The next milestone is library delivery: installation/exported CMake packages,
clean installed downstream use, declared compiler/platform support, CI, API docs,
and release/versioning policy. Cross-call scratch, exact caches, new optimizers,
compilation and GPU execution remain separate decisions.

Final qualification: 18/18 Release (2.01 s), 18/18 Debug (2.35 s),
18/18 ASan+UBSan (159.26 s), including standalone-header builds, separately configured source-tree
CMake consumer, strict-FP negative builds and bounded existing workflows. The new
fault test's initial ASan interceptor mismatch was repaired with paired nothrow
new/delete overrides, without disabling checks; the full suite was rerun. A short
ASan arithmetic cadence-four replay also exactly matches accepted initialization,
results, counters, trace and RNG. Full warnings/whitespace checks are clean.

Final uninstrumented five-pair medians: prime representative 104.851 → 98.120 us
per checked loss (6.42% reduction); arithmetic target 2.039 → 1.624 us (20.39%).
Other declared multirow gains and noisy/slower one-row failures are retained in
all raw batches. The single full-search observations are prime 7.9163 → 6.4173 s
and arithmetic .17614 → .15982 s; these are not repeated speedup estimates. All
work/result/RNG counters match, so no extra search generations or quality gain
is claimed. Production APIs and supported numerical environment are unchanged.
Exact commands and raw evidence: docs/EVALUATION_WORKSPACE.md and evaluation-results.

## Installable package and delivery policy milestone (2026-10-20)

Started clean at d7b6352ee30f6283cd07cba25847b4e8f49511aa. Inspected all public and
transitive headers, target/options, version history and MIT attribution. The library
is header-only, standard-library-only; no install/export rules or local release tags
existed. Root project version 0.1.0 remains the single authority and unreleased.

Implemented Genetik config package / Genetik::Genetik canonical source/installed
INTERFACE target, retaining genetik_lib for source compatibility. Relative
GNUInstallDirs includes preserve <Expression.hpp>/<SymbolicRegression.hpp> spelling;
all 15 public headers plus detail/ScopedTiming.hpp and MIT license install. Target
export/config/version metadata is relocatable, SameMinorVersion/ARCH_INDEPENDENT,
with clear required-component rejection. Config exposes version metadata even with
no requested version. No test/example/campaign/data artifacts install.

Required C++20 and strict FP settings propagate to consuming C++ translation units;
consumer-frontend selection is performed when loading the installed package, never
baked from the producing compiler. C compilation is unaffected. Sanitizers, warnings
and Werror are now private development settings, not public usage requirements.
Parent compiler/standard/extensions/build type and CTest ownership are preserved;
embedded tests/examples/install default OFF. Existing explicit choices remain effective.
No public C++ API, arithmetic, RNG, loss, fitting or search logic changed.

Disposable current-source producer installation passes before and after relocation
to prefix B with spaces; temporary producer source/build are then removed and a
fresh external consumer compiles/runs with only installed headers. Registries are
disabled and package resolution pinned/checked. Overlapping multi-TU C++ usage,
a C source, actual installed quickstarts, C++20 propagation/C++23 opt-in, metadata/
compile-command/runtime dependency inspections, source parent and unrelated C++14
sibling all pass. Source/installed version requests 0.1 and exact 0.1.0 succeed;
0.0.9/0.2.0/newer 0.1.1/1.0 and unknown required component reject as specified.

Qualified Jetson GCC/libstdc++13.3.0/glibc-libm2.39: 21/21 Release, 21/21 Debug,
21/21 ASan+UBSan with workspace CMake4.4.4, and 21/21 Release with exact CMake3.16.0.
The minimum tool was built from checksum-verified official source without patches.
All builds use warnings-as-errors; standalone headers, strict FP negative tests,
qualified captured bits, old workflows and new installed quickstarts pass. New
metadata/no-version checks were rerun as focused package tests in all four builds.
An optimized warning in the test-only replacement allocator was repaired by keeping
deallocation entry points out of line; no warning was suppressed and ASan passes.
Character seed42/1000 remains best .892, 100100 evaluations. No historical campaigns
were rerun or altered. See DELIVERY.md and delivery-results for exact commands/logs.

README now presents generic GA and symbolic-regression quickstarts using the actual
compiled/installed files; API.md indexes the established contracts. SUPPORT.md defines
0.x source compatibility, numerical/trajectory release notes, reproducibility scope,
no stable ABI, exact qualification status and release preparation. CHANGELOG records
important modernization/migration changes. A pinned/read-only three-job GitHub
Actions workflow configures GCC13/x86 minimum CMake, Clang18/x86 sanitizers and native
ARM64/GCC13. No hosted job has executed; local GCC13 is the only available compiler.
Windows/MSVC, AppleClang and libc++ remain unqualified. No push/tag/publication was
performed. Next: execute/review hosted qualification and the release checklist.

## Generic OOP extension qualification

Starting checkpoint 8b633264 was clean. The source audit and installed permutation
consumer are documented in ARCHITECTURE.md. Repaired evolve's bypass of virtual
roulette; retained default rank/draw behavior and private staged publication. The
core's population interface no longer transitively requires scalar Creature_i.
Include your own candidate headers explicitly. No symbolic types entered core.
A const short-lived selection view avoids copying snapshots for each default draw.
Custom selections are checked for valid fitness and matching problem identity.

Before organization: Release CTest 22/22 (84.70 s); focused installed/relocated
package repeat passed (67.72 s), including the new permutation consumer and an
include-dependency check proving no symbolic headers. The focused custom consumer
passes known costs/invariants, invalidation and failure/work contracts. Preserved
accepted executable versus modified default engine gives byte-identical prime2,
arithmetic42 and short cadence4 replay captures (including RNG and counters).
Post-organization/final-tree gates are recorded separately in OOP_QUALIFICATION.md.
No concurrency backend exists; CONCURRENCY.md records the next design requirements.

Source/output organization and final-tree gates are complete: Release/Debug/
ASan+UBSan/exact CMake 3.16.0 each pass 22/22, with standalone headers, Werror,
strict-FP, installed/relocated multi-TU/quickstart/permutation and source-parent
checks. Final serial replays remain byte-identical; 127 historical evidence hashes
are unchanged. See OOP_QUALIFICATION.md and its separate qualification-results.
The library still executes candidate preparation serially; CONCURRENCY.md is a
subsequent implementation plan, not an implemented backend.
