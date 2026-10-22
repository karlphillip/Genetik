# Genetik 0.3.0 — development pre-release (proposed, unreleased)

Genetik is a general-purpose C++20 genetic algorithm library with runtime-polymorphic
owned candidates and initialization, fitness, preparation, selection and variation
extension points. Symbolic regression is a reusable specialization; prime regression
is one application. This development release adds operational tooling, not new
mathematical operators or a new evolutionary strategy.

Since published v0.2.0:

- Opt-in exact-prefix prime search minimizes rounded mismatches across the first
  1,000 supplied primes. Historical approximation/campaign commands remain available.
- Coordinator-owned progress reports show readable formulas, normalized presentation
  fitness, exact matches, RMSE, generation/budget limits and checkpoint age. Six
  significant digits are the default; rounded equations use `≈`. Raw and versioned
  JSONL records preserve full canonical coefficients and search metrics.
- Reusable Unicode/LaTeX formatting preserves expression grouping and operand order.
  Canonical executable export remains unchanged.
- Generic committed-state checkpoint interfaces use explicit candidate codecs. The
  first built-in codec supports ordinary scalar expression candidates. Arbitrary
  external subclasses require their own codec and persistent-service state handling.
- Format-1 storage uses checksums, same-directory temporary files, fsync, verification,
  atomic replacement and bounded numbered retention. New directory entries are synced
  through their parents. Periodic saves and SIGINT/SIGTERM cancellation run only in
  coordinator flow after workers drain; signal handlers never serialize.
- Resume restores committed candidates/fitness, archive, RNG and actual saved counters.
  The loss budget is the total run allowance. Qualified clean-boundary continuation
  matches uninterrupted search; crash recovery may repeat work after the last save.

[Install and use Genetik](../../../README.md),
[compiled scalar/checkpoint quickstart](../../../examples/quickstarts/checkpoint.cpp),
[prime commands and checkpoint contracts](../../RUN_CHECKPOINTING.md),
[migration](MIGRATION.md), and
[local qualification](../../CHECKPOINT_PREFLIGHT_QUALIFICATION.md).

## Support and limits

The proposed release retains Genetik::Genetik, genetik_lib, established include
spelling, header-only C++20 delivery, strict FP and Threads::Threads propagation,
CMake3.16 and SameMinorVersion matching. Existing v0.2.0 platform evidence retains
its exact SHA scope. Local operational qualification is recorded separately; the
three-job hosted matrix is configured and awaits upload approval. No new libc++,
Windows/MSVC or macOS/AppleClang qualification is claimed.

Checkpoint continuation requires the qualified matching compiler, runtime, FP
settings, code/configuration and worker count. Checksum verification detects
accidental damage, not hostile forgery; load only trusted files. POSIX fsync/rename
behavior is filesystem/hardware dependent. SIGKILL cannot save a final checkpoint.
Coordinate fitting and numerical/search limitations from v0.2.0 still apply.

Reproducing all first 1,000 supplied primes would demonstrate an exact fit to that
finite prefix. It would not establish a general closed-form formula for all prime
numbers. Smoke results are engineering demonstrations, not recovery/performance claims.
