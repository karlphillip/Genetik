# Constructive integer recurrence synthesis

Genetik exposes an optional deterministic enumerator for small integer recurrence transitions in `ConstructiveRecurrenceSynthesis.hpp`. It is independent of population initialization and evolution. It does not create candidates for, or change, the generic genetic algorithm.

## Supported grammar

The initial qualified grammar is exactly the S16 grammar: integer expressions built from binary `Add`, `Subtract`, `Multiply`, and Euclidean `Modulo`; integer terminals from a caller supplied finite set; and recurrence bindings. Expressions are capped at seven nodes, matching the frozen S16 complexity ceiling. No unary negate, conditionals, iteration operators, arbitrary recursion, prime operators, or target-specific templates are synthesized. Modulo retains the existing checked semantics and requires a positive divisor. All operations use Genetik's existing checked Int64 evaluator. Overflow and invalid modulo are invalid candidate evaluations.

The recurrence order is caller-selected and must be one or two. The expression slots are `x0` for the zero-based transition index, then `x1` through `xOrder` for prior states from oldest to newest. For order two, `x1` is the older state and `x2` the newer state. Initial values are copied into the `IntegerRecurrence`; callers cannot supply reference states during rollout.

Expressions are enumerated by increasing node count. At each size, operators follow `config.operators` order, terminals follow variable-slot order then `config.constants` order, and binary partitions enumerate left subtree size before right subtree size, then each subtree's stable prior enumeration order. Exact expression text is used to reject duplicate structures. No algebraic rewriting or commutative canonicalization is performed.

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

The standalone consumer example in `examples/constructive_recurrence.cpp` discovers arithmetic progression, triangular, Fibonacci, and a non-Fibonacci second-order recurrence without receiving their transition expressions. After selection, callers can pass the frozen `SynthesisResult` to `assessRecurrenceTransition` with a separate held-out observation set. The assessment API does not alter the result and autonomously evaluates each held-out index from the same declared initial state.

## Limits and accounting

The configuration bounds expression nodes/depth/storage through `DiscreteExpressionLimits`, unique structures, construction attempts, training-point evaluations, total recurrence transitions, total evaluator node visits, total mathematical operations, retained candidate payload estimate, recurrence steps, and evaluator workspace/operation limits. The evaluator receives the remaining aggregate allowances before each autonomous rollout. A reached limit returns a non-success stop reason and no complete model. `cancelled` is an optional caller-owned atomic flag; cancellation also returns no model.

The reported retained-byte count is a conservative estimate for owned expression payload and enumeration identity/index containers. It is not an operating-system RSS cap. Callers needing a hard process memory or wall-clock ceiling must enforce it at the process boundary. `maximumSteps` and the total-work caps provide deterministic execution bounds; enumeration itself has no RNG or hidden global state.

## Training and recovery semantics

Training observations must be exact Int64 values at contiguous indices beginning with zero. Their first `order` values must equal the immutable initial state. For every candidate and every training point, the synthesizer constructs the ordinary `IntegerRecurrence` and asks it to evaluate that index. This causes complete autonomous rollout from the initial state; observed intermediate values are never fed into the transition.

`TrainingRecoveryStatus` distinguishes `Exact`, `Partial`, and `None`; a partial result includes the best valid transition and its longest exact training prefix count. Candidates that diverge from the next training observation can be pruned; every exact-training candidate is qualified over the complete training set. `ExactTrainingMatch` means the returned recurrence reproduces every supplied training observation exactly. It does not establish a unique transition or universal mathematical equivalence. A caller may preserve the selected model and evaluate it against separate held-out observations after selection; held-out data must not be passed to construction or selection. Finite held-out agreement is still only evidence over those tested indices. Invalid and resource-limited candidates are counted separately; cancellation and every configured limit have explicit stopping reasons.

## Relationship to evolution and provenance

This deterministic enumerator complements stochastic genetic evolution. It has no RNG, no fitness hooks, no population, and no automatic integration into GA runs. Use it only when its bounded grammar and exhaustive enumeration are suitable.

The mathematical family and control evidence originate in research milestone S16 (`research/constructive-recurrence-synthesis-s16`, frozen source `92a7550ab2399f49e99d5e371b69d1769bdb10a0`, final report `28b595edd0426ea30124258ba19861992d1122b3`). S16 recovered seven frozen small controls, while the discrete GA recovered 15/21 seeded runs. These results establish feasibility on those controls, not general superiority or broad recurrence synthesis. The production interface deliberately excludes later conditional synthesis work.
