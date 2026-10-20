# Changelog

## Unreleased — 0.1.0 development package

No release tag/publication has been made. This initial development package retains
the existing CMake version value and MIT attribution (Karl Phillip, 2021).

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
