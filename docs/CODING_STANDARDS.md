# C++ coding standards

Genetik uses C++20 and ships header-only code. The repository's `.clang-format`
defines formatting for new or intentionally edited C++ files. Keep unrelated
files out of formatting-only changes.

## Readability

- Put one declaration or operation on a line when it clarifies control flow,
  ownership, accounting, or a mathematical step.
- Use names that expose mathematical roles and resource units. Preserve public
  names and append-only enum values.
- Prefer explicit branches and checked failure paths over compressed expressions
  when they improve reviewability.
- Comments explain invariants, contracts, numerical ordering and limitations.
  Avoid comments that only restate syntax.

## Numerical code

- Preserve expression and operation order. Rounding, checked intermediates,
  invalidity and resource charging are observable contracts.
- Never enable fast-math or finite-only assumptions in code that consumes
  Genetik's expression interface.
- Keep interval, derivative and fitting assumptions next to their detailed
  contract documentation; do not imply broader guarantees than tested.

## Diagnostics

`.clang-tidy` selects a small set of high-signal checks for expression and
regression implementation headers. LLVM 18 is the checked toolchain for this
scope. The `clang-format18-clang-tidy18` CI job installs the pinned Ubuntu 24.04
packages, checks their versions, builds a compile database, and runs the same
script used locally. Missing tools, an absent compile database, formatter
violations, or configured diagnostics fail the command; the checks never skip
silently.

From the repository root, run:

```sh
cmake -S . -B build/quality -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DGENETIK_BUILD_TESTS=ON \
  -DGENETIK_BUILD_EXAMPLES=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
scripts/check_cpp_quality.sh build/quality
```

The formatter scope is limited to implementation headers deliberately modified,
the introduced quickstart, and the smooth-operator contract test. Static analysis
compiles the expression, regression, and smooth-operator test translation units
and filters diagnostics to the configured public implementation headers.
Historical source files are not mass-formatted.
