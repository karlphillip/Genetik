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
regression implementation headers. Run those checks when `clang-tidy` is
available; do not broaden the configured checks without reviewing existing
header diagnostics and build impact.
