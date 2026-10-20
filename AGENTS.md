# Genetik conventions

- C++20, CPU-first, standard library only; preserve the MIT license and ordinary GA use.
- Keep builds out of the source tree. On this Jetson CMake is workspace-local:
  `../.tools/bin/cmake` and `../.tools/bin/ctest` (4.4.4); ordinary `cmake`/`ctest` work when installed.
- Verified from this repository:
  ```sh
  ../.tools/bin/cmake -S . -B ../build-release -DCMAKE_BUILD_TYPE=Release
  ../.tools/bin/cmake --build ../build-release -j2
  ../.tools/bin/ctest --test-dir ../build-release --output-on-failure
  timeout 10s ../build-release/genetik_demo 42 1000
  ```
- Use separate Debug and sanitizer directories; sanitizer option is `-DGENETIK_SANITIZERS=ON`.
- Tests must check outcomes with runtime failures, including with `NDEBUG`; never require stochastic target discovery. Set CTest timeouts and bound examples.
- One explicit, caller-owned `RandomContext(seed)` per serial run. Population and algorithm borrow the same context; it must outlive them. No hidden engines/reseeding. Custom creatures accept `std::mt19937&` for initialization and mutation.
- `genetik_lib` is a header-only CMake interface target and propagates C++20 to consumers. Each public header must compile alone; verify the downstream consumer fixture.
- Fitness has Unevaluated/Valid/Invalid states; only Valid has a finite double score (zero and negatives are valid). Set objective direction independently. Evaluators return `Fitness`; expected mathematical errors return Invalid, operational exceptions propagate with nested context. Objective and input data remain fixed during a run.
- Both representations share `EvaluatedCandidate` fitness/cache state. Scalar candidates derive from `Creature_i`; genome/cache storage is private and final editing methods invalidate cached fitness. No writable references/views; copy input buffers even for rvalues. Do not restore protected genome/cache access.
- `setCreature()` stages edits; `updateValues()` or `publishGeneration()` evaluates and publishes a whole viable population. Invalid candidates cannot be parents. Check `GenerationOutcome`; no implicit retries.
- Publish population/generation/best-seen state only after all preparation succeeds, via no-throw ownership replacement. Failed attempts preserve committed state, but consume RNG draws and evaluation-attempt accounting. The symbolic driver checks candidate-call and baseline allowances at generation boundaries; exhaustion of the training-loss cap inside refinement discards the private attempt; generic manual GA loops own their budget policy. No cancellation/observer framework exists yet.
- `Expression.hpp` owns validated typed postfix trees separately from scalar genomes. Keep all expression traversals iterative, preserve per-node rounding/error checks, and copy public input storage even for rvalues. The reference header evaluator requires native binary64, strict FP flags (`-fno-fast-math -ffp-contract=off`), default rounding, gradual underflow and the qualified runtime math library; never enable fast/finite-only math. See docs/EXPRESSIONS.md.
- Preserve user changes. Record evidence and compatibility changes in docs/BASELINE.md; track staged extensions in docs/ROADMAP.md.
- Symbolic regression uses immutable copied rows, checked full-data MSE and Minimize; exact ties prefer node count then faithful text consistently across rank selection, elites and archive. Use the shared GA with a variation policy, never a second publication loop. See docs/SYMBOLIC_REGRESSION.md.
- Keep stochastic campaign recovery separate from deterministic tests. Charge every candidate call including initialization, invalids, throwing calls and reevaluated elites; do not inspect evaluation-only data before freezing search decisions.
- Optional coordinate fitting uses the same preparation/publication path. Charge actual training-loss attempts and rows separately from outer candidate calls; local limits return the best accepted value, global limits discard unpublished work. No fitter RNG draws; fixed immutable training data only. See docs/CONSTANT_FITTING.md. Keep generic sequence handling separate from prime reference generation and diagnostics. Do not add target lookups to candidate evaluation.

- Multi-input regression binds x0..x[d-1] to caller vector order; preserve typed variable identities in variation/export. File parsing, column mapping and target generators stay in examples, not the library. Fully validate training/evaluation files before starting, pass training rows only to search, and score held-out rows after selection. See docs/MULTIVARIATE.md.

- Operator arity is centralized in expressionArity: terminals 0, Log 1, arithmetic including Divide 2. Keep unary/binary traversals iterative and node/depth budgets arity-aware. Division rejects signed-zero denominators; natural Log requires positive arguments. No protected arithmetic or mathematical-validity regeneration. Defaults remain +,-,*; opt in through grammar. See docs/STRICT_ARITHMETIC.md.

- The prime benchmark is application-only: fixed n/1000 and prime/10000 scaling, exact prefix validation, fitting-only selection, then frozen later-range assessment. Preserve accepted campaign artifacts, including unsuccessful runs. New informed tuning must disclose that indices 1001–1200 have already been examined; use a separate output directory and record new work explicitly. See docs/PRIME_REGRESSION.md.

- Fitting cadence is generic: fitEveryGenerations is positive/default 1; initialization is generation 0, then prospective committed multiples. Never schedule from attempts or origin. Unscheduled slots still score freshly. Origin hooks are diagnostic only; preserve old scalar/custom preparation source compatibility. Optional bounded diagnostics cannot skip scoring/fits. Keep ordinary/baseline/probe loss categories disjoint and unpublished work visible. See docs/FITTING_CADENCE.md; interval 4 is optional, not the default recommendation.

- Checked loss and all-row reporting own lexical operand workspaces; keep one strict interpreter, reset logical contents each row, and never index reserved-but-uninitialized capacity. First growth stays inside the charged loss and after the first row charge. No shared mutable scratch or implicit cross-call result reuse. See docs/EVALUATION_WORKSPACE.md.

- Canonical source/installed target: Genetik::Genetik; genetik_lib is retained for source compatibility. Keep CMake >=3.16, header-only C++20/strict-FP requirements, relocatable GNUInstallDirs exports and consumer-frontend flag selection. Development flags must stay private; embedded extras/installation default OFF and the parent owns CTest/global compiler settings. See docs/PACKAGING.md.
- Packaging checks copy current intended library sources to disposable trees, relocate the prefix and remove only those producer trees. Installed multi-TU consumers and the actual quickstart files must compile/run without source includes or development runtime dependencies. Preserve the MIT license.
- Captured Jetson/GCC13/libm2.39 bits are opt-in via GENETIK_RECORDED_REFERENCE=ON on the qualified local builds; CI uses portable math/domain and within-build repeatability checks. Do not turn captured trajectories into cross-platform assertions. See docs/SUPPORT.md; configured CI is not verified hosted CI.

- Keep custom representation support through EvaluatedCandidate and public virtual
  algorithm hooks; evolve must dispatch roulette/crossover/mutation. New candidates
  own/validate their representation and invalidate only through controlled edits.
  See docs/ARCHITECTURE.md and tests/oop_consumer; no symbolic dependency in core.
- Active layout: examples/ (including quickstarts), apps/ for file regression,
  benchmarks/ for evaluation harness/corpus, scripts/ for campaigns/helpers.
  New run output goes to explicit out/ or build/temp directories. Preserve accepted
  docs evidence and historical source locations; do not blanket-ignore logs.
- No runtime population concurrency exists. Implement the generic execution-layer
  design in docs/CONCURRENCY.md before advertising thread controls or safety.
