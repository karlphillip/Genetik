# Contributing

Genetik is a header-only C++20 library. Contributions should preserve ordinary
genetic-algorithm use, established numerical contracts, and installed-consumer
compatibility.

## Before changing code

- Read the relevant public API documentation and nearby tests.
- Follow [C++ coding conventions](docs/CODING_STANDARDS.md) and the
  [architecture guide](docs/ARCHITECTURE.md).
- Use the repository's `genetik-development` Codex skill for focused coding,
  testing, documentation and refactoring guidance.
- For source changes, follow the existing
  [branch lifecycle skill](.agents/skills/genetik-branch-lifecycle/SKILL.md).
  It defines isolation, qualification, PR and cleanup steps; this guide does
  not replace them.

## Build and test

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON
cmake --build build/release -j2
cmake -E chdir build/release ctest --output-on-failure
```

See [test categories](docs/TESTING.md) for focused labels, Debug and sanitizer
builds, consumer checks and performance harnesses. Keep generated output in
`build/` or `out/`; do not place build products in source directories.

For the focused LLVM 18 formatting and static-analysis gate, follow the
reproducible commands in [coding standards](docs/CODING_STANDARDS.md). The gate
requires `clang-format-18`, `clang-format-diff-18`, and `clang-tidy-18`; it
reports missing tools as errors.

## Public headers and examples

Each public header must compile as the first include. Public examples should use
`Genetik::Genetik` as an external consumer would and must report bounded status
without asserting stochastic discovery. Do not add dependencies to the default
library target.
