# Coding and comments

- Use `.clang-format` for C++20 formatting. Do not apply broad reformatting to
  unrelated historical or numerical code.
- Keep declarations and definitions readable at review width; prefer one
  statement per line and explicit branches over compressed control flow.
- Use descriptive names for mathematical roles, bindings, units, and resource
  counters. Preserve established public names and append-only enum identities.
- Comments explain contracts, invariants, non-obvious ordering, and limitations;
  do not narrate syntax or restate a function name.
- Numerical comments must describe the actual operation order and failure
  behavior. Do not reorder floating-point expressions for style.
- Keep `.clang-tidy` checks focused on high-signal defects in core expression
  and regression headers; warnings are review prompts unless a gate says
  otherwise.
