# Baseline and first hardening milestone

Verified 2026-10-19 on the supplied NVIDIA Jetson. All changes are local and uncommitted.

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
