# Frozen typed operation-discovery protocol

Recorded before the first campaign. No target tree, residual transformation or
coefficient-bearing skeleton is supplied to initialization. Continuous fitting is
disabled. The generic typed search receives ordinary immutable numerical rows.

- Target: `A*x+b`; x0=matrix(2,3), x1=vector(3), x2=vector(2), output vector(2).
- Data: 64 rows each, training/interior/exterior, generated before search by
  examples/TypedRegressionData.hpp. Independent explicit data seeds 73001/73002/73003.
  Draw A row-major (6), x (3), then b (2), each independently. Training uniform
  nine half-steps [-2,2]; interior sixteen odd eighths [-15/8,15/8]; exterior
  independently signed {2.5,3,3.5,4}. Targets use independent ordinary row loops.
  Search RNG never generates data. Binary64 full-precision CSVs accompany results.
- Seeds: 0,1,2,3,42, sequential; serial CPU. Population128, elites2, crossover .9,
  mutation .35. Actual loss and outer candidate ceilings10000; generation ceiling100.
  Last incomplete population is never admitted merely to consume unused allowance.
- Node15/depth5 (leaf1); constant occurrences4, scalar coefficients16, abs8;
  max512 expression primitives,128 temporary coefficients; default dataset limits.
- Finite shapes in order: matrix(2,3),vector(3),vector(2),scalar. Enabled operators
  add/subtract/scalar multiply/dot/matvec/matmul/transpose. Uniform among feasible
  productions (operation plus operand shapes). Terminals: all matching variables,
  scalar constants only with values {-1,0,1}. No aggregate constant terminal.
- Initial depth ceiling4, mutation3; uniform feasible depth; grow operation probability
  .65 when terminals exist. Eight proposals, then validated witness/unchanged fallback.
  No resampling for mathematical validity. Witnesses prefer least nodes, stable
  production enumeration. Unconstructible output is rejected before RNG use.
- Exact-zero training stop, separate maximum-component-error recovery threshold1e-6.
  Ordering: finite MSE, nodes, depth, constant occurrences, coefficient count, text.
  Across seeds, retain first seed in list on an exact tie. Freeze all models and
  overall training-selected representative before evaluating either evaluation set.
- Baselines: constant vector fitting mean (training only), b alone, A*x alone.
  These are reporting models only, never population templates.

One campaign only; all seeds retained, no outcome-dependent tuning. Full Release
command: `out/build/release/genetik_typed_regression --output out/runs/typed-discovery-1`.
The executable refuses to overwrite existing results and writes protocol/data before
search. Check configuration and CSV SHA256 with the retained report.
