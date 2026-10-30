# S2 qualification

## Frozen scope

This is a deterministic fixed-structure math qualification. It is not an evolutionary campaign and does not modify GA policies or production fitting defaults. No prime-successor search was run. Exact rational functionality is opt-in and was built against the retained Boost 1.83 header tree at `out/tmp/m1-boost/root/usr/include`; a normal Genetik build does not include or require Boost.

## Independent fixtures

The optional test executable checks arbitrary-precision normalization, exact binary64 dyadic import, rational serialization round-trip, negative floor/ceiling, and resource-limited shared-floor branching. It verifies:

- `floor(10 alpha)=16` and `floor(20 alpha)=32` intersect to the exact interval `[8/5,33/20)`.
- Negative coefficients reverse the interval endpoints with their open/closed ownership preserved.
- Zero coefficients distinguish a feasible constant from a contradiction.
- Ceiling, truncation, and halfway-away-from-zero cells use distinct boundary rules for negative and positive integer outputs.
- A bounded shared-floor difference yields a union of intervals, and the binary64 value for gamma is contained in the exact region for a fixture transition.
- A selected binary64 coefficient belongs to its exact interval and is checked through the actual `MixedExpression` evaluator against exact Int64 observations.
- The open interval between adjacent binary64 values is proved to contain no executable coefficient.
- The B2 fixed-topology Wythoff direct formula was evaluated over 1,000 training transitions and 200 held-out transitions. References use the independent integer identity `floor((n+isqrt(5n^2))/2)`.

The D5 symbolic suite checks the difference-of-squares cancellation, its denominator nonzero assumption, and the absence of a bit-exact executable claim.

## Build and compatibility checks

The exact CMake executable is `../delivery-tools/cmake-min/bin/cmake` (3.16.0). S2 configuration and Release build are run with that tool in addition to the current workspace CMake. Optional Boost headers are attached only to the S2 test target. The normal target set and CMake interface dependency remain standard-library-only. The complete Release CTest run passed 73/73, including the installed and relocated consumers. After the final constraint-boundary assertions were added, the 72-test remainder was rerun and passed 72/72; the unchanged installed-consumer test had separately passed in the complete run.

The symbolic transform preserves the original expression object. No mixed-expression node IDs, candidate codec IDs, checkpoint payloads, coordinate fitting, or LM settings change. The existing symbolic DAG codec is unchanged because assumptions were already serialized. Solver state is reconstructed from the fixed structure, exact observations, parameter domain, and solver limits.

Evidence files and checksums are in `s2-results/`. The repository's location-based hygiene test passes. The broader workspace scan reports existing parent-level `docs/`, `examples/`, and `session.md` entries outside this repository; they were not modified. A pass of the focused optional tests is not a claim that all S1/S2 workloads are exhaustively verified.
