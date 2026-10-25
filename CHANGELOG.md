# Changelog

## 0.4.0 — proposed development pre-release

- Add optional exact Int64/Bool expressions, checked arithmetic, lazy conditionals,
  scoped bounded sums/products, type-safe variation and discrete regression.
- Add explicit scalar/discrete checkpoint forward readers with exact runtime/context
  validation; retain format1 and codec IDs. Rebuild all dependent TUs.
- Retain all D3 negative prime-discovery outcomes separately from representability.
- Exclude unrelated post0.3 experimental optimizers/objectives/mutation strategies
  from the clean release integration. Existing defaults are unchanged.
- Hosted qualification/publication remain pending; see RELEASE_0.4.0_PROPOSAL.md.


## 0.3.0 — unreleased

- Add opt-in exact-prefix prime search, readable committed-generation reports,
  presentation fitness, bounded Unicode/LaTeX precision and full raw/JSONL output.
- Add generic explicit checkpoint codecs/capture/restore, scalar candidate codec,
  format1 POSIX durable storage, periodic retention and cooperative signal saves.
- Qualify deterministic continuation and bounded SIGKILL crash recovery separately.
- Preserve source OOP/mathematical APIs, default historical modes and exact-prime
  search semantics. Rebuild consumers consistently; request the0.3 package series.
  See [notes](docs/releases/0.3.0/NOTES.md) and [migration](docs/releases/0.3.0/MIGRATION.md).

## 0.2.0 — published development pre-release

New pre-1.0 minor series; SameMinorVersion rejects older 0.1 package requests.
Recompile all dependent translation units consistently after the polymorphic
measurement-interface additions. See [migration](docs/MIGRATION.md) and
[complete proposed notes](docs/releases/0.2.0/NOTES.md).


- Opt-in Sin/Cos/Exp/Sqrt/Powi across scalar and typed construction, checked CPU
  evaluation, grammars, subtree/discrete variation, continuous fitting and faithful
  export. Discrete power exponents 0..8 preserve ordered checked multiplication.
  Default operator sets and qualified disabled-operator replays are unchanged.
  Scientific campaign and independent references are documented separately.

- Additive owned TypedValue and TypedExpression API for scalar/vector/matrix
  construction, strict evaluation, shape inference, bounded work/payload and faithful
  export. Rectangular matrix/vector operations and an installed typed quickstart;
  no changes to existing scalar expressions or generic GA interfaces.

- Canonical local/CI output directories, parent-workspace archival, location-based
  hygiene gate and rejection of in-source CMake configuration. Compiled campaign
  applications moved to apps/campaigns/; executable names and public APIs unchanged.
- The phased roadmap now separates cleanup from future typed mathematics work.

## 0.1.0 — development pre-release, 2026-10-20

Published at f9299ea2cb5136d06e2de8f68bb83ecf460ca369; the source asset and
annotated v0.1.0 tag remain unchanged. MIT attribution: Karl Phillip (2021).

- C++20 baseline and explicit, caller-owned RandomContext: custom initialization and
  variation use its engine; implicit/hidden reseeding is removed.
- Value-owned scalar genomes, controlled edits and explicit Unevaluated/Valid/Invalid
  fitness; numerical zero is valid. Custom evaluators return Fitness. Set objective
  direction independently. Mutable genome/cache access has been removed.
- Private generation staging with whole-generation publication, committed best,
  nested operational errors and attempted-work accounting; failed work/RNG draws
  remain consumed. Inspect outcomes instead of waiting for eventual discovery.
- Typed, bounded, value-owned expressions; generic multivariate/single-output data,
  checked MSE, structure initialization/subtree variation, faithful export, strict
  opt-in division and natural log. Default grammar remains +,-,*.
- Optional bounded coordinate constant fitting, continuous/discrete constants and
  deterministic cadence (default one). Separate candidate, loss and row counters;
  local fits retain progress and global exhaustion discards unpublished attempts.
- Invocation-local operand storage reuse without arithmetic/result caching.
- Generic file examples and polynomial, multivariate, arithmetic and prime experiments.
  All measured unsuccessful runs remain historical evidence; no universal formula
  or stochastic recovery guarantee is implied.
- Installable `Genetik` config package and canonical `Genetik::Genetik` target;
  legacy `genetik_lib` remains source-only. Required C++20/strict FP flags propagate;
  development sanitizer/warning flags no longer propagate to consumers. Embedded
  extras/install default OFF; parent settings and CTest ownership are respected.
- Installed/relocated multi-TU consumers, compiled quickstarts, configured CI matrix
  and explicit pre-1.0/support policy. CMake minimum remains 3.16.

The modernization changes numerical behavior and seeded trajectories relative to
2021/recent intermediate revisions (bug fixes, explicit RNG, corrected selection,
strict arity/arithmetic, fitting/cadence). Repeatability requires the exact revision,
toolchain/runtime, inputs/configuration and execution mode; see docs/SUPPORT.md.
This packaging milestone itself changes no evaluator, RNG or search algorithm.

- OOP qualification: evolution now honors overridden roulette selection (previously
  bypassed); default draws/ranking are unchanged. Added a read-only short-lived
  Population::selectionView and installed non-regression permutation consumer.
- Organized active demos, file application, benchmark/replay harness and campaign
  helpers; public include spelling, installed layout and executable names are unchanged.
  Include Creature_i.hpp explicitly when using scalar candidates; Population_i.hpp
  now depends only on EvaluatedCandidate.hpp. Historical evidence stays in docs/.

- Added opt-in generic candidate CPU execution (ExecutionConfig/ExecutionContext,
  RandomContext ownership, --threads), explicit callback qualification and private
  refinement factories. Default one retains serial results; binding parallel work/
  failure boundaries have separately documented speculative-work semantics.
- Threads::Threads is now a transitive package dependency. Added a private, separate
  TSan development path; the package remains header-only and 0.1.0 unreleased.

- Hosted GCC13 x64/minimum-CMake, Clang18 x64/ASan+UBSan and GCC13 ARM64/TSan
  qualification executed. Explicit scalar bounds-check discards fix Clang Werror
  without changing source API or numerical semantics. Artifact staging and bounded
  public diagnostics retain concrete failure evidence; see [hosted qualification](docs/HOSTED_QUALIFICATION.md).

Release review consolidates [migration guidance](docs/MIGRATION.md),
[proposed release notes](docs/releases/0.1.0/NOTES.md) and
[the integration/asset proposal](docs/RELEASE_PROPOSAL.md). This documentation and
archive-verification work changes no library API or numerical/search policy.
