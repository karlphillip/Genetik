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
- Current fitness maximizes finite nonnegative floats. Call `updateValues()` after replacing members. Do not feed raw minimization losses as fitness.
- Preserve user changes. Record evidence and compatibility changes in docs/BASELINE.md; track staged extensions in docs/ROADMAP.md.
- Evolve expression structures as well as constants in future work. Keep generic sequence handling separate from prime reference generation and diagnostics. Do not add target lookups to candidate evaluation.
