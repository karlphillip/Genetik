---
name: genetik-development
description: Apply Genetik's C++ coding, architecture, testing, documentation, and safe-refactoring practices to repository changes.
---

# Genetik development

Use this skill for source, tests, examples, and documentation changes. The
repository's `AGENTS.md` and task-specific instructions are authoritative.
For source-changing work, use the existing
`genetik-branch-lifecycle` skill for isolation, qualification, PRs, and cleanup;
this skill does not repeat that workflow.

## Before editing

- Read the relevant public contracts and the current implementation before
  changing a shared component.
- Keep the public header-only C++20 API, `Genetik::Genetik` target, strict
  floating-point requirements, generic GA, RNG draw order, and checkpoint
  identity stable unless the task explicitly authorizes a change.
- Prefer a small local change that preserves responsibility boundaries over a
  new abstraction or file move.

## References

- [Coding and comments](references/coding.md)
- [Architecture and dependencies](references/architecture.md)
- [Tests and qualification](references/testing.md)
- [Documentation and examples](references/documentation.md)
- [Safe refactoring](references/refactoring.md)

## Completion

- Keep formatting-only edits separate from semantic edits.
- Run the narrow deterministic tests first, then applicable full qualification.
- Compile every changed public header alone and verify consumer examples through
  the installed CMake package when the public surface or example changes.
- Report test inventory deltas and any incomplete qualification precisely.
