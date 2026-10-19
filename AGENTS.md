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
- Scalar candidates derive from `Creature_i`; genome/cache storage is private and final editing methods invalidate cached fitness. No writable references/views; copy input buffers even for rvalues. Do not restore protected genome/cache access.
- `setCreature()` stages edits; `updateValues()` or `publishGeneration()` evaluates and publishes a whole viable population. Invalid candidates cannot be parents. Check `GenerationOutcome`; no implicit retries.
- Publish population/generation/best-seen state only after all preparation succeeds, via no-throw ownership replacement. Failed attempts preserve committed state, but consume RNG draws and evaluation-attempt accounting. No evaluation budget/cancellation/observer framework exists yet.
- `Expression.hpp` owns validated typed postfix trees separately from scalar genomes. Keep all expression traversals iterative, preserve per-node rounding/error checks, and copy public input storage even for rvalues. The reference header evaluator requires native binary64, strict FP flags (`-fno-fast-math -ffp-contract=off`), default rounding and gradual underflow; never enable fast/finite-only math. See docs/EXPRESSIONS.md.
- Preserve user changes. Record evidence and compatibility changes in docs/BASELINE.md; track staged extensions in docs/ROADMAP.md.
- Evolve expression structures as well as constants in future work. Keep generic sequence handling separate from prime reference generation and diagnostics. Do not add target lookups to candidate evaluation.
