# Genetik 0.1.0 — development pre-release

Genetik is a header-only, general-purpose C++20 genetic algorithm library with
public OOP extension points and a reusable symbolic-regression specialization.
Applications supply owned scalar or custom genomes, objectives, preparation and
selection/variation behavior. Polynomial, multivariate, strict-arithmetic and prime
regression are examples of the same bounded public search API.

Start with the [README](https://github.com/karlphillip/Genetik/blob/v0.1.0/README.md),
[compiled scalar/symbolic quickstarts](https://github.com/karlphillip/Genetik/tree/v0.1.0/examples/quickstarts),
and [custom permutation consumer](https://github.com/karlphillip/Genetik/tree/v0.1.0/tests/oop_consumer).
These tag links become available only after the proposed tag is approved/created.
Install with CMake >=3.16 and consume `find_package(Genetik 0.1 CONFIG REQUIRED)` /
`Genetik::Genetik`. The package supplies C++20, strict FP and Threads::Threads;
there is no mandatory mathematical, Python, GPU or file-format runtime dependency.
The source archive and SHA256SUMS are the proposed assets; no binary package is offered.

Principal capabilities:

- Explicit run-owned seed/RNG, validated configurations, finite fitness states,
  bounded runs and safe whole-generation publication. Failed attempts retain
  committed state while consumed work/RNG remain consumed.
- Owned typed expressions with hard limits; variables, constants, +, -, *, strict
  division and natural log; checked full-data MSE and faithful text export.
- Optional deterministic constant fitting and generation cadence, actual-loss/row
  accounting, multivariate inputs and one scalar target.
- Real opt-in candidate workers through ExecutionConfig / `--threads N`. Default
  one remains serial. Coordinator owns RNG, variation, ordering and publication.
  Custom parallel callbacks require explicit qualification/private mutable state.
  Detailed refinement diagnostics remain serial-only; worker limits are not affinity.

Migration from the original unreleased interfaces is material: explicit randomness,
Fitness return values, private invariant-bearing genomes/cache, controlled edits,
publication outcomes and repaired virtual roulette dispatch. See
[the migration guide](https://github.com/karlphillip/Genetik/blob/v0.1.0/docs/MIGRATION.md).
Header spelling/casing remains; include Creature_i.hpp explicitly for scalar
subclasses. `genetik_lib` remains a source-only compatibility target.

Qualified Linux combinations include GCC13 x64 Release with exact CMake 3.16.0,
Clang18 x64 Debug ASan+UBSan with **libstdc++14 headers**, and native GCC13 ARM64
Release plus a separate TSan build. Hosted jobs execute 24 portable cases per
normal configuration; TSan separately executes two forced-overlap/OOP cases.
The local 25th case is a Jetson-specific numerical snapshot intentionally excluded
on other environments. Installed/relocated multi-TU and OOP/symbolic consumers pass.
Exact tested SHAs, jobs and runtimes are in the
[qualification record](https://github.com/karlphillip/Genetik/blob/v0.1.0/docs/HOSTED_QUALIFICATION.md).
Windows/MSVC, AppleClang/macOS and libc++ are unqualified.

Symbolic regression is heuristic search, not a promise of recovery or extrapolation.
Coordinate fitting can stall on coupled constants. Strict division/log domains
produce ordinary invalid candidates; no protected/clamped arithmetic is used.
Repeatability is scoped to exact revision, toolchain/stdlib/libm/FP environment,
data, configuration and execution mode. Binding parallel budgets/failures may
change speculative work/RNG relative to serial. The prime example makes no general
prime-formula claim and preserves weak runs/baselines; its later range is already
an examined benchmark. The recorded roughly fourfold throughput observation applies
only to the Jetson fixed-work prime-refinement benchmark with a reused four-worker
pool, not general workloads or hosted runners.

This is an early development pre-release, not a stable ABI or universal production
qualification. Before 1.0, breaking source changes require a new minor series;
patches preserve documented source API, with numerical/trajectory changes explicitly
noted. SameMinorVersion package matching stays within 0.1.x. MIT license and Karl
Phillip's 2021 attribution are unchanged.
