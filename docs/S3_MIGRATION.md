# S3 migration notes for the 0.7.0 development candidate

- No change to the ordinary C++20 generic GA or coordinate-fitting defaults is
  intended. Existing custom candidate APIs remain independent of expression
  types.
- LM remains opt-in. Its configuration is part of optimizer identity and
  checkpoint compatibility; use the same optimizer policy to resume a run.
- Mixed expressions with shared Real64 parameters preserve parameter IDs.
  Recompile translation units that include changed header-only APIs.
- Exact constraints require Boost.Multiprecision only when those optional
  headers/tests are enabled. The standard package does not acquire a mandatory
  Boost or LAPACK dependency. LAPACK remains an independent optional LP64 target.
- Exact rational feasibility and binary64 witness verification are separate
  steps. A mathematical rational witness need not be representable as a double.
- Symbolic transforms retain the source expression and assumptions. A
  mathematical identity under assumptions does not certify bit-identical
  floating-point execution.
- `EquationArchive` consumes caller-provided authoritative training records
  after a run. It does not change RNG, fitness, selection, publication,
  objective work, or checkpoint continuation. Never populate it from held-out
  observations.
- Python package version metadata follows the provisional 0.7.0 candidate;
  Python remains optional and must be rebuilt from source for that package
  version.

See [release notes](releases/0.7.0/NOTES.md) and the detailed S3 qualification
records for platform support and current completion status.
