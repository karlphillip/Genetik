# S3 migration notes for Genetik 0.7.0

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
- Python package metadata is 0.7.0. Python remains optional; use the CPython
  3.12 Linux x86-64 or ARM64 wheel, or build the source distribution for a
  qualified environment. Other Python versions and operating systems are not
  covered by this release qualification.

The release includes explicit forward readers for published v0.6.0
scalar/discrete checkpoints. Generic checkpoint decoding remains exact-version.

See [release notes](releases/0.7.0/NOTES.md) and the detailed S3 qualification
records for platform support and current completion status.
