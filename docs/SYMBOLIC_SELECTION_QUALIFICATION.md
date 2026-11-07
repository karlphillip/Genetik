# Symbolic-selection comparison record

This is the output of the frozen harness in
`benchmarks/symbolic_selection_comparison.cpp` on the C2.4 development source.
It was rerun after final selection-work accounting changes. Outputs and work
counters are deterministic; elapsed time is a single-machine observation. No
recovery threshold or superiority requirement was imposed.

All three configurations used the same immutable training/assessment data,
MSE, grammar, seed `104729`, expression limits, population 48, generation limit
24, candidate-call budget 1,200, and training-loss-call budget 1,200. The node
penalty was `0.01`. Exact fits stopped early; noisy cases used the full budget.

| Case | Strategy | Nodes | Training MSE | Assessment MSE | Candidate calls | Row calls | Generations | Pareto comparisons | Peak estimated bytes | Elapsed wall µs |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| exact affine | loss only | 7 | 0 | 0 | 288 | 2,592 | 5 | 0 | 0 | 2,705 |
| exact affine | penalty | 9 | 0 | 0 | 480 | 4,320 | 9 | 0 | 0 | 4,765 |
| exact affine | Pareto | 5 | 0 | 0 | 240 | 2,160 | 4 | 73,052 | 269,012 | 4,242 |
| exact quadratic | loss only | 5 | 0 | 0 | 192 | 1,728 | 3 | 0 | 0 | 1,392 |
| exact quadratic | penalty | 5 | 0 | 0 | 192 | 1,728 | 3 | 0 | 0 | 1,597 |
| exact quadratic | Pareto | 5 | 0 | 0 | 192 | 1,728 | 3 | 56,080 | 262,894 | 3,128 |
| noisy affine | loss only | 5 | 0.00231111 | 0.00068 | 1,200 | 10,800 | 24 | 0 | 0 | 9,199 |
| noisy affine | penalty | 5 | 0.00231111 | 0.00068 | 1,200 | 10,800 | 24 | 0 | 0 | 9,977 |
| noisy affine | Pareto | 5 | 0.00231111 | 0.00068 | 1,200 | 10,800 | 24 | 384,424 | 273,058 | 16,372 |
| noisy quadratic | loss only | 5 | 0.00231111 | 0.00068 | 1,200 | 10,800 | 24 | 0 | 0 | 6,852 |
| noisy quadratic | penalty | 7 | 0.00231111 | 0.00068 | 1,200 | 10,800 | 24 | 0 | 0 | 7,752 |
| noisy quadratic | Pareto | 5 | 0.00231111 | 0.00068 | 1,200 | 10,800 | 24 | 383,944 | 269,601 | 15,911 |

For the affine clean control, penalty selection reached an exact fit using a
nine-node expression while loss-only used seven nodes and Pareto used five.
This demonstrates that a scalar penalty's outcome depends on its weight and
search path; it does not guarantee minimum complexity among all possible exact
expressions. On the noisy quadratic case, the penalty selected a different
seven-node expression while finite training and assessment scores matched the
five-node models. Finite agreement is not structural or universal equivalence.

Pareto consistently incurred comparison work. The four cases show similar
finite prediction quality under this seed and budget, not a broad advantage or
reliable recovery probability. Estimated selection memory stayed below 0.3 MiB
for these small populations; these values are accounting estimates, not process
RSS measurements. Use the separate Pareto frontier when the tradeoff itself is
desired. The benchmark is opt-in and does not affect CTest success.
