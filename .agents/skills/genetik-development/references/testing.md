# Tests and qualification

- Tests assert outcomes at runtime and remain meaningful with `NDEBUG`.
- Keep deterministic contracts separate from stochastic campaigns and timing
  harnesses. Never make CI depend on a lucky GA discovery.
- Assign CTest categories through the explicit metadata table in
  `tests/CTestCategories.txt`; do not infer them from test names. CMake checks
  that every registered test has category metadata. Use labels to select unit,
  integration, numerical, compatibility, smoke, or scientific tests.
- Preserve performance measurements as explicit build/run targets, not ordinary
  CTest cases. They must not enter the default correctness suite.
- Bound tests with timeouts and resource budgets. Preserve failed qualification
  evidence; a timeout is not a pass.
- For public headers and packaging, compile headers alone and run installed,
  relocated, multi-translation-unit consumers using `Genetik::Genetik`.
- Check compatibility of RNG sequences, checkpoint identity, and existing
  defaults whenever shared execution paths are touched.
