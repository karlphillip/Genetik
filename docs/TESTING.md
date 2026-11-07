# Testing and qualification

CTest labels provide focused subsets without removing tests from the default
suite. Tests can carry more than one label. The unfiltered CTest command remains
the complete qualification suite.

From a configured build directory:

```sh
ctest -L unit --output-on-failure
ctest -L integration --output-on-failure
ctest -L numerical --output-on-failure
ctest -L compatibility --output-on-failure
ctest -L smoke --output-on-failure
ctest -L scientific --output-on-failure
```

The categories mean:

- `unit`: deterministic component contracts and focused unit-level checks.
- `integration`: command-line workflows, overlaps, consumers, and multi-process
  or multi-translation-unit checks.
- `numerical`: expression, fitting, regression, recurrence, operator and
  mathematical correctness tests.
- `compatibility`: installed packages, downstream builds, checkpoint formats,
  strict floating-point policy and multi-translation-unit compatibility.
- `smoke`: quickstarts, examples, demos and bounded CLI paths.
- `scientific`: fixed mathematical references, scientific controls and
  benchmark-preflight contracts. This label does not turn an exploratory
  stochastic campaign into a required pass condition.

Performance measurements are intentionally separate from CTest. Build an
explicit benchmark target and run it only when the measurement task calls for
it; do not add wall-clock thresholds to deterministic correctness tests.

The normal full suite remains unfiltered:

```sh
cmake -E chdir build/release ctest --output-on-failure
```

Use separate Debug, ASan+UBSan and TSan build trees as documented in
[`AGENTS.md`](../AGENTS.md). Public-header-alone compilation is part of the
normal build, while `genetik_downstream` and `genetik_installed_package` exercise
external package consumption.
