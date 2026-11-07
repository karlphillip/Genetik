# Architecture and dependencies

- The core remains standard-library-only and header-only. Public headers install
  from `genetik/` and compile as the first include.
- Generic candidate ownership, evaluation, fitness, selection, variation,
  publication, `RandomContext`, and checkpoint codecs stay separate from
  symbolic-regression specializations.
- Expressions own validated typed postfix trees. Operator metadata is centralized
  in `genetik/detail/ScalarOperators.hpp`; evaluation and objective policy stay
  in their existing expression and regression components.
- Additive optional mathematics must not become a dependency of ordinary GA
  consumers or alter defaults.
- Preserve strict checked evaluation and documented floating-point settings;
  ordering is part of numerical behavior.
- Place application/file parsing in `examples/` or `apps/`, tests in `tests/`,
  measurement harnesses in `benchmarks/`, and contracts in `docs/`.
