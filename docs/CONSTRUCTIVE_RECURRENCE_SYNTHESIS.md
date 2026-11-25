# Constructive integer recurrence synthesis

Genetik exposes an optional deterministic enumerator for small integer recurrence transitions in `ConstructiveRecurrenceSynthesis.hpp`. It is independent of population initialization and evolution. It does not create candidates for, or change, the generic genetic algorithm.

## Supported grammar

The default grammar remains the S16 grammar in its original order: binary `Add`, `Subtract`, `Multiply`, and Euclidean `Modulo`; integer terminals from a caller supplied finite set; and recurrence bindings. Expressions are capped at seven nodes, matching the frozen S16 complexity ceiling. GCD is available only when explicitly added to `RecurrenceSynthesisConfig::operators`; it uses the existing checked Int64 GCD evaluator, including its arithmetic-work accounting. The default grammar and its enumeration order are unchanged. No unary negate, conditionals, iteration operators, arbitrary recursion, prime operators, or target-specific templates are synthesized. Modulo retains the existing checked semantics and requires a positive divisor. Overflow and invalid mathematical operations are invalid candidate evaluations.

The recurrence order is caller-selected and must be one, two, or three. The expression slots are `x0` for the zero-based transition index, then `x1` through `xOrder` for prior states from oldest to newest. Initial values are copied into the `IntegerRecurrence`; callers cannot supply reference states during rollout. The standalone checkpoint codec and identity are unchanged, so existing order-one and order-two checkpoint documents retain their encoding.

Expressions are enumerated by increasing node count. At each size, operators follow `config.operators` order, terminals follow variable-slot order then `config.constants` order, and binary partitions enumerate left subtree size before right subtree size, then each subtree's stable prior enumeration order. Exact expression text is used to reject duplicate structures. No algebraic rewriting or commutative canonicalization is performed.

## Deterministic Tribonacci benchmark

`GENETIK_BUILD_ORDER_THREE_BENCHMARK=ON` builds the fixed order-three capability demonstration. Search receives initial state `[0,0,1]` and training observations at indices 0–9 only; held-out indices 10–13 are constructed separately and assessed after selection. The ordinary grammar order is `[Add, Subtract, Multiply, Modulo]`, with constants `[-1,0,1,2,3]` and a five-node ceiling. Before search, the executable prints all remaining fixed search and assessment limits: 60,000 structures, 250,000 composition attempts, 1,000,000 training-point evaluations, 10,000,000 transition steps and arithmetic operations, 20,000,000 node visits, 32 MiB estimated candidate storage, and 32 recurrence steps. Expression limits are depth 5, four variables, five constants of magnitude at most 3, 4 MiB storage, iteration depth 8, and 128 binders. Evaluator limits are 4,096 node visits and operations, 1 MiB workspace, 10,000 iterations per operation, 100,000 aggregate iterations, and iteration depth 8. Assessment allows 100 observations, 10,000 transition steps, 100,000 node visits and arithmetic operations, and 32 recurrence steps. The base revision rejected the public three-term `IntegerRecurrence` constructor with `recurrence order/schema/resource policy`.

On base `a8e83b5719b1cc687f2b16c2fd7ff6f5aae1be64`, the public API request was rejected as above. The order-three candidate selected `(x1 + (x2 + x3))`, matched all 10 supplied training observations (the three initial terms separately, then 7/7 predicted training terms), and matched held-out 4/4. It examined 679 unique structures and used 2,778 arithmetic operations during search; held-out assessment used 114 operations. Stop reason was `exact_training_match`. Reusing the selected expression from `[2,0,1]` produced the next four independently generated values `3,4,8,15` exactly. Its structure recovers the three-term sum; these finite agreements do not prove the recurrence universally.

The order-two square compatibility replay reproduced `(x0 + (x0 + (x2 + 3)))`, exact training 5/5 and held-out 3/3, with 16,672 structures, 50,442 training-point evaluations, 17,525 transition steps, 86,087 node visits and 50,270 arithmetic operations, matching the recorded result. The fixed-budget Rowland replay also matched its recorded expressions and work counts: ordinary `(x1 + 1)` at 483,277 structures/1,978,028 arithmetic operations (storage limit), and GCD-enabled `(x1 + gcd(x1, (x0 + 2)))` at 334,977 structures/1,706,805 operations (exact training match).

```sh
cmake -S . -B build/order-three -DCMAKE_BUILD_TYPE=Release \
  -DGENETIK_BUILD_TESTS=OFF -DGENETIK_BUILD_EXAMPLES=OFF \
  -DGENETIK_BUILD_ORDER_THREE_BENCHMARK=ON
cmake --build build/order-three --target genetik_order_three_recurrence
./build/order-three/genetik_order_three_recurrence
```

## Opt-in GCD discovery comparison

`GENETIK_BUILD_ROWLAND_GCD_BENCHMARK=ON` builds a separate comparison executable; it is off by default. The executable fixes both arms' training indices (0–8), held-out indices (9–13), initial state `[7]`, constants `[-1,0,1,2,3]`, node and work limits, and operator order before either search. The ordinary arm uses the default grammar. The opt-in arm lists GCD first, followed by the ordinary operators in their existing relative order. The recurrence `b[0]=7; b[k+1]=b[k]+gcd(k+2,b[k])` generates observations after those choices are fixed: training values are `[7,8,9,10,15,18,19,20,21]`, and held-out values are `[22,33,36,37,38]`. The assessment rolls each candidate forward from `[7]`; held-out observations are never supplied during selection.

The report separates exact training fit, autonomous held-out agreement, reference-transition recognition, first divergence from the reference sequence, structures, evaluator arithmetic work, and stop reason. After both searches have selected their models, a structural checker recognizes the reference transition under operand swaps at any `Add` or `Gcd` node, recursively; all other operators and leaves must match in place. A transition outside that narrow equivalence class is reported as `unrecognized`. The controls check the discovered form `gcd(x1, 2 + x0) + x1`, an actually different `x1 + gcd(x0 + 3, x1)`, invalid modulo-by-zero, and a valid zero prediction. On divergence, only a valid evaluator result prints an integer; invalid/resource/cancelled results print their status and `actual=unavailable`. A finite fit or held-out match is not by itself evidence that the known recurrence rule was recovered. Run it with:

```sh
cmake -S . -B build/rowland-gcd -DCMAKE_BUILD_TYPE=Release \
  -DGENETIK_BUILD_TESTS=OFF -DGENETIK_BUILD_EXAMPLES=OFF \
  -DGENETIK_BUILD_ROWLAND_GCD_BENCHMARK=ON
cmake --build build/rowland-gcd --target genetik_rowland_gcd_discovery
./build/rowland-gcd/genetik_rowland_gcd_discovery
```

### Fixed-budget Rowland result

On source base `45929f073777e26a5742c8d189e0fe67c9dd5a77`, both arms used training indices 0–8, held-out indices 9–13, initial state `[7]`, constants `[-1,0,1,2,3]`, seven-node ceiling, 3,000,000 unique-structure cap, 15,000,000 composition-attempt cap, 1,000,000 training-point-evaluation cap, 50,000,000 transition cap, 100,000,000 node-visit and arithmetic-operation caps, 768 MiB estimated-storage cap, and 64-step cap. The ordinary order was `[Add, Subtract, Multiply, Modulo]`; the opt-in order was `[Gcd, Add, Subtract, Multiply, Modulo]`.

| Arm | Returned transition | Training | Held out | First divergence | Structures | Arithmetic operations | Stop |
| --- | --- | --- | --- | --- | ---: | ---: | --- |
| Main ordinary grammar | `(x1 + 1)` | 4/9 prefix, not exact | 0/5 | index 4: expected 15, got 11 | 483,277 | 1,978,028 | storage limit |
| GCD enabled | `(x1 + gcd(x1, (x0 + 2)))` | exact 9/9 | exact 5/5 | none through index 13 | 334,977 | 1,706,805 | exact training match |

The GCD arm's selected tree differs from `b[k] + gcd(k+2,b[k])` by operand swaps at its `Add` and `Gcd` nodes. The post-selection checker recognizes this recursive swap class, including the explicit discovered-form control `gcd(x1, 2 + x0) + x1`; `x1 + gcd(x0 + 3, x1)` remains unrecognized. This is recovery of the stated rule within the documented symmetry class, alongside exact finite-sample agreement. The ordinary arm exhausted its estimated-storage budget before finding an exact model. Search accounting is selection-only; post-selection held-out rollout is separate. Divergence reporting distinguishes valid zero from invalid evaluation, as the modulo-by-zero and literal-zero controls demonstrate.

The unchanged opt-in square benchmark was rerun as a compatibility control with GCD absent from the default grammar. It returned `(x0 + (x0 + (x2 + 3)))`, exact training 5/5, exact held out 3/3, with 16,672 composition attempts/unique structures, 50,442 training-point evaluations, 17,525 transition steps, 86,087 node visits, and 50,270 arithmetic operations, matching PR #68's recorded result.

## Usage

```cpp
#include <ConstructiveRecurrenceSynthesis.hpp>

using namespace genetik::constructive;
std::vector<SequencePoint> training{{0, 0}, {1, 1}, {2, 1}, {3, 2}, {4, 3}, {5, 5}};
RecurrenceSynthesisConfig limits;
limits.maximumNodes = 5;
limits.maximumStructures = 50000;
limits.maximumTransitionSteps = 1000000;
auto result = synthesizeRecurrenceTransition(2, {0, 1}, training, limits);
if (result.transition) {
    // result.canonicalTransition is the discovered transition text.
}
```

The standalone consumer example in `examples/constructive_recurrence.cpp` discovers arithmetic progression, triangular, Fibonacci, and a non-Fibonacci second-order recurrence without receiving their transition expressions. After selection, callers can pass the frozen `SynthesisResult` to `assessRecurrenceTransition` with a separate held-out observation set. Assessment observations must be nonempty, have strictly increasing nonnegative indices within the configured step bound, and fit the configured count limit. Assessment order, initial-state dimensions, observation constraints, and limits are validated even if synthesis returned no candidate. The assessment API does not alter the result and autonomously evaluates each held-out index from the same declared initial state.

## Limits and accounting

The configuration bounds expression nodes/depth/storage through `DiscreteExpressionLimits`, unique structures, construction attempts, training-point evaluations, total recurrence transitions, total evaluator node visits, total mathematical operations, retained candidate payload estimate, recurrence steps, and evaluator workspace/operation limits. The evaluator receives the remaining aggregate allowances before each autonomous rollout. A reached limit returns a non-success stop reason; the result can retain the best partial model found before exhaustion, but never reports it as exact. `cancelled` is an optional caller-owned atomic flag and has the same partial-result behavior.

`estimatedStoredBytes` reports the conservative estimate for the enumerated candidate/identity pool; `selectedModelBytes` reports the separate owned copy of the returned expression (zero when no model is selected). `estimatedPeakBytes` reports the highest accounted combination. Admission reserves room for both retained storage and the selected model. Replacing a selected model must also fit the transient peak containing the retained pool, the old selected copy, and the replacement copy. This is an explicit estimated-storage accounting policy, not an operating-system RSS cap: temporary allocator overhead and process runtime are not measured. Callers needing a hard process memory or wall-clock ceiling must enforce it at the process boundary. `maximumSteps` and the total-work caps provide deterministic execution bounds; enumeration itself has no RNG or hidden global state.

## Training and recovery semantics

Training observations must be exact Int64 values at contiguous indices beginning with zero. Their first `order` values must equal the immutable initial state. For every candidate and every training point, the synthesizer constructs the ordinary `IntegerRecurrence` and asks it to evaluate that index. This causes complete autonomous rollout from the initial state; observed intermediate values are never fed into the transition.

`TrainingRecoveryStatus` distinguishes `Exact`, `Partial`, and `None`; a partial result includes the best valid transition and its longest exact training prefix count. Candidates that diverge from the next training observation can be pruned; every exact-training candidate is qualified over the complete training set. `ExactTrainingMatch` means the returned recurrence reproduces every supplied training observation exactly. It does not establish a unique transition or universal mathematical equivalence. A caller may preserve the selected model and evaluate it against separate held-out observations after selection; held-out data must not be passed to construction or selection. Finite held-out agreement is still only evidence over those tested indices. Invalid and resource-limited candidates are counted separately; cancellation and every configured limit have explicit stopping reasons.

## Relationship to evolution and provenance

This deterministic enumerator complements stochastic genetic evolution. It has no RNG, no fitness hooks, no population, and no automatic integration into GA runs. Use it only when its bounded grammar and exhaustive enumeration are suitable.

The mathematical family and control evidence originate in research milestone S16 (`research/constructive-recurrence-synthesis-s16`, frozen source `92a7550ab2399f49e99d5e371b69d1769bdb10a0`, final report `28b595edd0426ea30124258ba19861992d1122b3`). S16 recovered seven frozen small controls, while the discrete GA recovered 15/21 seeded runs. These results establish feasibility on those controls, not general superiority or broad recurrence synthesis. The production interface deliberately excludes later conditional synthesis work.
