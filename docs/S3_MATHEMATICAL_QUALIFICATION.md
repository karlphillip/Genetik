# S3 Mathematical Qualification

## Source identity and inherited evidence

S1 was implemented by `7e73bee` and S2 by `31c131b` on top of mixed-expression commit `6570ff4`. The S1 full Release result (105 tests) preceded its final fitter bookkeeping correction; the correction was followed by four affected Release checks and focused advanced-regression Debug, ASan+UBSan, and TSan checks. It is not a full final-tip run. S2's 73/73 Release run preceded its final constraint-boundary additions; the unchanged package consumer passed in that run and the 72-test remainder passed after the additions. These results are not combined as a single final-source suite.

The inherited analytic derivative fixtures cover `a sin(bx+c)`, `a x^b`, `a exp(-bx)`, `a/(1+bx^2)`, and repeated `a sin(ax)`. The S1 record documents invalid logarithm and real-power domains, rank deficiency, and explicit unavailable derivatives. Parameter Jacobian assembly calls the scalar partial evaluator separately for each parameter; it is not single-pass differentiation. No finite-difference fallback is used for unsupported derivatives.

The S1 fixed-topology comparison reports coordinate versus LM fitting for four scalar families and the D6 coupled oscillator. LM reached near-zero training and held-out error on the frozen fixtures, including the difficult fractional-power start where coordinate fitting stopped near `6.38e-2` MSE and LM near `3.92e-25`. These are supplied topologies, not structure discovery. The two-seed evolutionary comparison was mixed and LM selected poor structures in both seeds; coordinate fitting remains the default. Full measurements and retained failed/pre-correction launches are in `s1-results/`.

S2's exact fixture independently establishes `floor(10 alpha)=16` and `floor(20 alpha)=32` as `[8/5,33/20)`. The S2 test includes exact normalization, signed/zero coefficients, interval boundaries and intersections, contradictions, bounded shared-floor unions, limits and executable witness checks. The Wythoff topology matched 1,000 exact training and 200 exact held-out outputs, but this was fixed-structure recovery. The executable uses a binary64 witness and verifies outputs; rational feasibility alone does not imply representability. The solver is bounded to its documented one-parameter affine and finite shared-floor fragment.

## S3 independent work

An independent standard-library Python `fractions.Fraction` fixture reconstructs the two floor cells and their intersection without calling Genetik, asserting exactly `[8/5,33/20)` and checking both integer outputs. Its executable is `s3-results/independent_exact_interval.py`; captured output and hash are retained there. A separate SymPy derivation proves symbolic equality of the analytic Jacobians for all four required families and a repeated shared parameter; the C++ S1 fixture compares the implementation against those formulas. S3 adds C++ cases for the zero sine boundary, singular rational denominator, exponential overflow, unsupported active derivative and a deliberately exhausted operation budget. The S2 exact tests exercise the solver's bounded branching/resource statuses. The new output-only archive has focused deterministic contract coverage. No new mathematical kernel or optimizer was introduced.

## Symbolic domain behavior

The inherited D5 transform retains the original executable expression and emits a separate symbolic result with a `NonZero(x-1)` assumption for `(x^2-1)/(x-1) -> x+1`. The claim is mathematical equivalence under that assumption, not bit-exact binary64 equivalence. The transform is deliberately narrow; general rational-function simplification is unsupported.

## Integrated candidate execution record

The curated candidate is based on published `v0.6.0` with the reviewed M3 mixed/shared evaluator, only its M1 integer sqrt/root kernels and optional exact integer backend dependencies, S1, S2 and the S3 archive. The initial curated implementation passed 66/66 Release tests before S3 files were added. With the archive, v0.6 forward checkpoint tests, version 0.7 package metadata and boundary fixtures integrated, the Release CTest manifest contains 69 tests; all 69 pass, including installed/extracted/relocated and C++23 multi-translation-unit consumers. The exact manifest and full log are in `s3-results/curated-s3-test-manifest.json` and `s3-results/curated-s3-ctest.log`. The working tree commit identity is recorded in the final preparation report after commit.

This result qualifies the integrated Release candidate only. Debug, exact CMake 3.16, ASan+UBSan, required TSan selections, optional LAPACK/Boost/Python consumers, extracted source archive, and final source/archive checks remain separate gates until their final candidate runs are captured. No historical S1/S2 totals are combined with this manifest.
