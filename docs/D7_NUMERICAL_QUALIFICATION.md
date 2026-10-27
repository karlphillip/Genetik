# Genetik 0.5.0 D7 mathematical qualification

## Tested implementation and evidence scope

This report covers the integrated D5/D6 mathematical APIs and the additive
0.5.0 checkpoint import policy on the D7 qualification branch. The exact
implementation/test-configuration SHA and full local Release, Debug, CMake
3.16.0, ASan+UBSan, TSan, strict-FP, consumer, workflow and packaging outcomes
are recorded in the D7 qualification summary and approval packet. The
competitor benchmark's fixed implementation SHA is
`5a95ea47f983d4ba99704fb9ea955b14264d6741`; it ran before the portable CI
manifest correction, which only added two existing CTest names and did not
change executable code or benchmark configuration.

Mathematical reference tests use the frozen D5/D6 reference assets below and
the D7 full CTest suite. Independent numerical references validate selected
inputs and assumptions, not every expression, all floating-point platforms,
or all ill-conditioned problems. A passing mathematical contract does not
establish that genetic search can discover the corresponding expression.

## Qualified mathematical scope

| Area | Locally implemented and reference-tested scope | Limits |
|---|---|---|
| Checked integer mathematics | Euclidean integer division, GCD/LCM, checked integer and modular powers, factorial, binomial and bounded `first`; 3,652 independent Python-integer reference rows | Signed 64-bit only; overflow/invalidity is explicit; arbitrary precision is incomplete and not qualified |
| Recurrences and scoped iteration | First/second-order integer recurrences; bounded sum/product with iterator binding and scope rules; independent square-sum and divisor references | Fixed recurrence order/initial conditions in regression; finite step, node and storage limits; no unbounded history or recursive call language |
| Extended scalar math | `pow`, integer powers, roots, logarithmic/exponential variants, trig/inverse trig, hyperbolic, `abs`, `min/max`, `clamp`; local domain and derivative contracts | Strict domains; several singular/boundary partials are unavailable; derivative propagation is not a formal universal symbolic proof |
| Symbolic algebra | Bounded rational-polynomial differentiation, simplification, expansion, supported factorization, CSE, conditional relation/domain obligations and explicit conversion | Conservative supported subset; unsupported transformations report unknown/limited; no universal expression equivalence |
| Dimensions/constraints | Seven SI exponent axes, bounded rational exponents, dimensional scalar regression, scoped binder checks and explicit rejection constraints | No unit magnitude or affine-temperature inference; no general constructive dimensional planner or formal proof of physical laws |
| Coupled scientific systems | Owned equation/channel models and shared bounded parameters; selected-equation structure evolution; joint residual fitting | Evolution changes one unknown equation at a time; no arbitrary vector-AST evolution |
| ODE/PDE residuals | ODE value/derivative observation residuals; PDE residuals over supplied field/derivative observations, including heat-equation references | No trajectory integrator, general ODE/PDE solver, or reconstruction of an unobserved field |
| Higher derivatives | Checked second partial derivatives on the supported hyperdual/operator subset; analytic references and explicit unavailable outcomes | Not a complete Hessian API; no finite-difference fallback; unsupported derivative rules are unavailable |
| Bounded linear algebra | Bounded vectors/matrices, solve, real-symmetric eigenpairs and symmetric matrix exponential; optional LP64 LAPACK package path | Optional LAPACK is platform/dependency-specific; no nonsymmetric eigensystem/general exponential; native library work/conditioning limits apply |
| Tensor operations | Owned ranks 0–3, checked elementwise arithmetic, outer product, paired-axis contraction, reduction and selected fitting | Not unrestricted tensor networks; arbitrary tensor-valued AST evolution is unsupported |
| Complex expressions | Bounded complex evaluation, parsing and checkpoint codecs, restricted real-coefficient topology regression | No general complex constant fitter, complex automatic differentiation, or arbitrary-vector evolution; intermediate overflow remains possible |
| Statistics/probability | Finite mean/variance/covariance/correlation, weighted mean, discrete expectation, normal and Poisson functions; independent probability references | Finite deterministic data/functions, not sampling or Bayesian inference; domains and resource caps apply |
| Special functions | `erf`, `erfc`, gamma, log-gamma, Bessel J0, normal PDF/CDF and Poisson PMF; high-precision references | Derivative support is limited; most special-function second derivatives are unavailable; runtime `libm` qualification is platform-specific |

## Independent references

| Reference | Independent source and retained identity |
|---|---|
| Integer/recurrence | Python `math` arbitrary-integer oracle in `scripts/references/d5_integer.py`; 3,652 rows in `tests/d5-integer-references.tsv` |
| D5 scalar functions and partials | mpmath 1.2.1, 100 decimal digits, exact binary64 input identities and analytic formulas; 70 rows in `tests/d5-scientific-references.tsv`, provenance in `docs/d5-results/scientific.json` |
| D6 special functions | mpmath 1.2.1 at 100 digits; 32 rows in `tests/d6-scientific-references.tsv`, generated by `scripts/references/d6_scientific.py` |
| Probability distributions | mpmath 1.2.1, 100 digits and exact binary64 input identities; 21 rows, tolerance and SHA256 in `docs/d6-results/references/probability.json` |
| Matrices, tensors and fields | Analytic 2x2 solve/eigenproblem, symmetric matrix exponential, independent expanded tensor dot products, supplied-observation heat-field partials and finite-statistic fixtures in `tests/d6_science.cpp` |
| D7 checkpoint import | Current-source CTest writers/readers for published v0.3 scalar and v0.4 scalar/discrete contracts; full-population and continuation equality, plus explicit generic-reader rejection of mismatched versions |

D5's frozen search study recovered 17/24 held-out cases, but fractional-power
and trigonometric tasks were each 0/3. D6's frozen discovery study recovered
4/12 held-out runs (tensor 3/3, Gaussian 1/3, oscillator 0/3, complex 0/3);
the three tensor successes were generation-0 fitted initializations. All
three fixed-structure D6 fits recovered their fixtures. Complex seed 0 retained
an initialization failure. These are search/fitting outcomes, not mathematical
reference failures; the negative and failed results remain retained.

## Compatibility and qualification boundaries

The ordinary generic GA defaults, `Genetik::Genetik`, C++20 and CMake >=3.16
remain the package contract. The D7 reader is explicit and additive: generic
checkpoint loading remains exact-version, while named readers import only the
documented v0.4.0 scalar/discrete and v0.3.0 scalar payload contracts. New D5,
D6 and experimental scientific payloads are not silently reinterpreted.
Deterministic resume requires matching data, operator/type/context identity,
resource limits, worker/runtime contract and RNG state as documented.

The D6 full driver retains its parent-workspace hygiene failure for the
user-owned `/home/karlphillip/workspaces/primeResearcher/session.md`. Repository
and build-output hygiene passed; the parent file was not changed. D7 records
the exception without weakening repository/output checks. A qualification
claim must distinguish this inherited parent-scope failure from the D7
repository hygiene result.
