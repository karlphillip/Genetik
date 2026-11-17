# Rational-assisted screening protocol

Frozen before execution on origin/main `5d1df91d7060a45b4aed9ecf5f00f8fad76f5b6a`.

## Cases and splits

For the four independent cases, use 41 training observations on equally spaced
points in `[-1,1]`, 21 validation observations `x=-0.95+0.095*i` for `i=0..20`,
and 41 assessment observations `x=-0.975+0.04875*i` for `i=0..40`. Generate
exact clean targets independently for each split.
The historical case reuses the existing unchanged D7 clean train and test CSVs:
every fourth original training row (index mod 4 equals zero) is validation and
the other 120 rows are training; the original test observations are assessment.
No assessment label is read before all models are selected. Cases:

1. Historical D7 `rational_x2` clean case; disclose that this is a regression
   check, not an independent task. Its original files and hashes remain intact.
2. Independent normalized rational degree `(1,1)`.
3. Independent normalized rational degree `(2,1)`.
4. Independent nonrational cubic polynomial control.
5. Independent nonrational sine control.

The protocol's generator functions and constants are test-only truth metadata;
fitters and symbolic search receive only the three observation splits, with
only training passed into run APIs.

## Arms and frozen budgets

- A: existing symbolic search, `population=48`, `elites=2`, `maxGenerationAttempts=12`,
  `maxEvaluations=624`, `maxTrainingLossAttempts=624`, serial `RandomContext`.
- B: existing rational fitter, deterministic once per case, degrees 0..3 each,
  at most 8 parameters, no LM, `maximumArithmeticOperations=2,000,000`,
  `maximumLinearAlgebraOperations=50,000,000`, workspace 32 MiB.
- C: same symbolic configuration and seeds as A, reserve 2 of 48 slots (the
  provider cap is `min(8, floor(population/4))`), use
  the rational provider, initializer work allowance 50,000,000, and include
  its full execution time in end-to-end runtime.
- Stochastic seeds: 11, 29, 47. No repeated deterministic fitting.
- Fit the rational candidate batch once per case and reuse that immutable batch
  across C's three stochastic runs. Charge its measured fit time and native work
  once in preparation, and include that same preparation cost in each C run's
  end-to-end per-run cost. Do not sum the repeated attribution fields as three
  actual fitter executions.
- Grammar for A/C: `+,-,*,/,Powi`, powers 0..3, constants `{-2,-1,0,1,2}`,
  hard expression limits 127 nodes/depth 16, constant magnitude 100.
- Stop/selection are the existing training MSE threshold and budget rules.
  No validation-based tuning or winner selection is allowed. The reported
  selected result is each run's committed training-best model.

## Criteria fixed before execution

- Numerical recovery: assessment normalized RMSE <= `1e-6`, where normalization
  divides by assessment target RMS (floor `1e-12`). Report all three seeds for
  stochastic arms; do not call a seeded expression found by evolution if it is
  already present in its initial reserved slot.
- Symbolic equivalence is reported separately only when exact rational
  polynomial cross multiplication proves equality after coefficient tolerance
  `1e-8`; report the assessed interval and denominator nonzero assumption.
- Report training, validation, assessment RMSE and R2, selected expression,
  coefficients, nodes, depth, constant-node count and any identified fitted
  parameter count, initialization time/work, search time,
  total time, candidate/loss/row counts, rational QR/arithmetic counters, peak
  process RSS high-water mark for the campaign process, invalids and stop state.
  This is a process-level maximum, not a per-run memory estimate. QR estimates
  are not evaluator calls. Symbolic-search constants are reported as constant
  nodes; without explicit constant refinement they are not called fitted
  parameters.
- Clean data only. No acceptance threshold may be changed after results.
