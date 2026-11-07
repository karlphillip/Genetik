# Tests and qualification

- Tests assert outcomes at runtime and remain meaningful with `NDEBUG`.
- Keep deterministic contracts separate from stochastic campaigns and timing
  harnesses. Never make CI depend on a lucky GA discovery.
- Use CTest labels to select unit, integration, numerical, compatibility, smoke,
  or scientific tests. Performance benchmarks are explicit build/run targets,
  not ordinary CTest cases.
- Bound tests with timeouts and resource budgets. Preserve failed qualification
  evidence; a timeout is not a pass.
- For public headers and packaging, compile headers alone and run installed,
  relocated, multi-translation-unit consumers using `Genetik::Genetik`.
- Check compatibility of RNG sequences, checkpoint identity, and existing
  defaults whenever shared execution paths are touched.
