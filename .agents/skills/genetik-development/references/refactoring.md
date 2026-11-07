# Safe refactoring

- Record current behavior and the exact test inventory before changing shared
  code. Make mechanical formatting a separate change from behavior.
- Preserve statement and operation order in numerical code, enum identities,
  random draws, object ownership, cache invalidation, and serialization bytes.
- Avoid file moves unless a current dependency boundary demonstrably requires
  them. Prefer clarifying a local API or helper at the existing boundary.
- After a refactor, compare test names/counts, inspect the complete diff, compile
  public headers alone, and run consumer/package tests as applicable.
- Do not discard user data, research branches, evidence, or worktree output.
