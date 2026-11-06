# C2.1 — qualification record

## Source identity and retained evidence

- Continuation branch: `research/multivariate-rational-domain-safety-c2-1`.
- Starting S14 source: `bd26be819559e9338f0c57939bc0d6dd39155d88`.
- C2 diagnostic source: `a16fef0a0d9d200265ebcd08900dab28dfeee61f` (unchanged).
- Main: `73432014a381078fff23e09226210fd5ae144303` (unchanged).
- No C2 scientific campaign, public API, production PR, or hosted integration CI
  was started.

Original false-certificate source/output, corrected contract output, source
diff summaries, and per-command logs are retained in
`Genetik-data/benchmarks/c2-1-multivariate-domain-safety/`. The evidence
directory has a SHA256 manifest verified after capture.

## Results

| Qualification | Result | Notes |
|---|---|---|
| Original false-certificate reproduction | Reproduced | Exact S14 source says `Valid / SeparatedFromZero`; three independent values change sign. |
| Focused S14 fit contracts | Pass | Linear, complete quadratic, proper/improper rational, QR determinism, resource/error contracts, scalar rational regression. |
| Focused domain safety contracts | Pass | Independent zero, safe, mixed, all-monomial, unsupported, malformed, rounding-mode cases. |
| Full examples-enabled Release CTest | Pass | 105/105 in 505.82 s, including installed/relocated and C++20/C++23 consumers. After the final assessor-only compiler/FTZ guards and signed-zero regression, 104/104 tests excluding the installed-package test passed in 58.28 s; that package does not copy or compile the research-only S14 header. |
| Focused Debug domain contract | Pass | Rebuilt after the final signed-zero regression; separate lightweight domain test, 1/1. The historic full S14 contract is not relabeled as a focused Debug pass. |
| Focused ASan+UBSan domain contract | Pass | Rebuilt after the final signed-zero regression; 1/1 in 0.37 s, no sanitizer report. |
| CMake 3.16 | Pass | CMake 3.16.0 configured, built, and ran the domain contract. Its CTest command ran from the build directory because 3.16 lacks `--test-dir`. |
| Standalone header / strict FP | Pass | Research header syntax-only compiled as C++20 and C++23 with strict flags; `-ffast-math` was rejected at compile time. Upward-rounding mode returns uncertain. |
| Branch lifecycle | Pass | `scripts/test_branch_lifecycle.py`: 10/10 scenarios. |
| Repository hygiene | Pass with retained parent findings | CTest repository-hygiene target passed. Workspace scan only reports pre-existing parent `/Testing` and `/analysis`, left untouched. |

## Interpretation and limits

The former `SeparatedFromZero` result for an unsupported cubic is eliminated.
The accepted family is limited to two variables and exact degree-two basis
prefixes. Binary64 enclosure requires strict IEEE-754 compilation, nearest
rounding, and gradual underflow. Unsupported floating-point configurations
return uncertain or fail compilation; arbitrary precision and cross-platform
formal verification are out of scope.

S14's QR fitting code and model-selection path were not changed. The existing
near-pole negative fixture remains a negative result; no claim is made that
pole separation implies predictive accuracy. The historical S14 Debug
ASan+UBSan timeout and separate S18 limitation remain unresolved. C2 Gate B,
full Debug qualification of the historic campaign fixture, and hosted
integration qualification were not part of this repair. Evidence SHA256
verification is recorded separately in the sibling data directory.

## Decision

**A — Mathematical correctness repaired for the supported degree-two family.**
The unsupported-basis false certificate is rejected; accepted models pass
independent interval and sign-change cases, with explicit uncertainty for
unsupported numerical environments and exhausted bounds. Resume C2's
engineering assessment, then run a separately frozen scientific confirmation.
This decision does not authorize a production API or integration PR.
