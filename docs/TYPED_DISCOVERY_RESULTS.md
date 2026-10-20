# Typed operation-discovery results

Protocol: [TYPED_DISCOVERY_PROTOCOL.md](TYPED_DISCOVERY_PROTOCOL.md). Retained raw
reports, frozen models and full-precision data: [typed-regression-results/discovery](typed-regression-results/discovery/).
SHA256.json identifies all retained bytes. Jetson/GCC13 Release, strict native binary64,
serial; no continuous fitting or target templates. All eleven independent input
coefficients and both output components vary, checked before acceptance. Data reference
uses ordinary row-major loops, independent of TypedExpression.

| Seed | Initial MSE | Final MSE | Discovery generation | Published | Loss calls | Primitive work | Seconds |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 0 | 0 | 0 | 0 | 128 | 88320 | .021231 |
| 1 | 0 | 0 | 0 | 0 | 128 | 74880 | .017144 |
| 2 | 1.732421875 | 0 | 5 | 5 | 768 | 576192 | .032260 |
| 3 | 1.732421875 | 0 | 6 | 6 | 896 | 664192 | .029602 |
| 42 | 0 | 0 | 0 | 0 | 128 | 75008 | .011044 |

All five frozen candidates have zero maximum component error on all three sample
sets, no invalid rows and stop ExactTrainingFit. All 64 rows count on every loss.
Unused calls: 9872,9872,9232,9104,9872. No abandoned work. Representative seed0 was
chosen by training ordering before evaluation. Seeds0/1/3 export
`(matvec(x0,x1) + x2)`, seed2 `(x2 + matvec(x0,x1))` (5 nodes/depth3/no constants),
seed42 `(matvec(x0,x1) + (scalar(1.0) * x2))` (7 nodes/depth3/one coefficient).
All bind x0=matrix[2,3], x1=vector[3], x2=vector[2], output vector[2].

Initialization discoveries are retained honestly; they do not demonstrate variation.
Independent bounded operator tests establish actual changed mutation/crossover;
seeds2/3 additionally exercise 5/6 published search generations (116/132 mutation
changes,250/314 crossover changes). This small feasible grammar makes the operation
combination readily discoverable. Exact sample agreement is the measured result;
no claim about arbitrary unseen expressions or matrix-equation discovery follows.
No configuration adjustment or rerun was performed after results.

Baselines (fitting / interior / exterior MSE): mean output11.59264/6.29338/313.30795;
b alone9.31982/4.65715/299.44482; A*x alone1.73242/1.24023/10.80469. Their maximum
errors are respectively10.6875/6.64453125/42.03515625, 8.5/5.703125/38.5,
and2/1.875/4. Mean coefficients use training rows only. Baselines never enter GP.

Runtime includes initialization/planning/search, excludes data generation, file output,
baselines and evaluation scoring. These short individual observations are not a
performance benchmark. Results predate checkpoint3 aggregate fitting; no-fit search
remains supported. Reproduce with the command in the protocol, using a fresh output
directory. Full campaign is not a stochastic correctness gate.
