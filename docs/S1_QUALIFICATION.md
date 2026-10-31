# S1 qualification record

## Completed checks

- Complete Release build and full CTest suite: 105/105 tests passed. This
  included standalone-header and multi-TU targets, installed-package relocation,
  downstream and generic OOP consumers, strict floating-point policy,
  checkpoint compatibility, and D6 tests.
- Focused advanced-regression contracts also passed in Debug, ASan+UBSan
  (`detect_leaks=1`), and TSan builds. These are targeted S1 tests; full Debug
  and sanitizer suites were not run.
- Parameter derivative analytic fixtures for `a sin(bx+c)`, `a x^b`,
  `a exp(-bx)`, `a/(1+bx^2)`, and a repeated shared parameter `a sin(ax)`.
- Invalid logarithm, invalid real-power derivative, and explicit rank-deficient
  parameter checks.
- Scalar opt-in fitting, D6 coupled-oscillator fitting, and bounded
  two-worker continuous-versus-resumed LM checkpoint replay, including
  optimizer-work counters and rejection of a changed optimizer policy.
- Fixed-structure and evolutionary applications completed under the frozen
  protocol. Full raw results and pre-correction failed launch evidence are
  retained under `s1-results/`.
- Two-worker continuous-versus-resumed execution matched complete committed
  candidate populations, fitness, training/refinement counters and optimizer
  work. The run checkpoint carries the RNG state; continuation reproduced the
  same candidate trajectory.
- After the final fitter bookkeeping correction, Release checks for fitting,
  advanced regression, scalar checkpoint compatibility and D6 continuation
  passed (4/4), and the focused advanced test passed again in Debug, ASan+UBSan,
  and TSan.

## Qualification not completed

The complete 105-test Release suite ran immediately before the final
loss-bookkeeping correction; the affected Release contracts were rerun after
that correction as noted above. The current environment provided CMake 3.28.3,
so an exact CMake 3.16 build
was unavailable. Full Debug and full sanitizer suites were not run; only the
focused advanced-regression test passed in those configurations. The optional
pybind11 extension was not built because this change adds C++ APIs only. The
workspace hygiene script reported three existing parent-directory entries
outside this repository (`docs/`, `examples/`, and `session.md`); they were left
untouched. Repository `git diff --check` passed.

## Limits and compatibility

The coordinate fitter remains default. Native scalar LM checkpoint codec `/2`
preserves new optimizer counters and accepts legacy `/1`; LM numerical
configuration participates in identity. Measured D6 checkpoints retain `/1`
because the explicit D6 fitter is not installed as an evolutionary
`ConstantFitConfig` policy. Replay was verified in the same Release compiler
and runtime environment, not across platforms or libm implementations.

The solver uses a bounded small dense pivoted solve and an undamped normal
matrix pivot-ratio guard, not an SVD or a rigorous condition estimator. Parameter
Jacobian assembly repeats the checked existing scalar partial evaluator once
per fitted parameter. No derivative is propagated through integer rounding,
floor, ceiling, modulo, or other discontinuous conversions. Scalar specialized
regression subclasses are not accepted by the scalar fitter adapter; coupled
D6 fitting is provided through the separate all-channel API. These limitations
are explicit and the optimizer remains opt-in.
