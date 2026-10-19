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
