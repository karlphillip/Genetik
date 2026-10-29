# M3 mixed numerical expressions

## Scope and architecture

This implementation adds a bounded direct-formula expression type for
`Int64 -> Int64` candidates with typed `Real64` and `Bool` intermediates. The
generic GA remains unaware of the mathematical representation. Candidates use
`EvaluatedCandidate`, and `MixedIntegerVariation` is supplied to the existing
`GeneticAlgorithm`/`Population` templates.

The public expression is an owned, validated postfix tree in
`MixedExpression.hpp`. Validation infers each node type and rejects mismatched
operator operands before evaluation. Evaluation uses an iterative frame stack;
conditionals and boolean conjunction/disjunction are lazy. `Int64` kernels are
the checked arithmetic kernels from `DiscreteExpression.hpp`. Real arithmetic
and transcendental functions dispatch to the checked scalar kernels in
`Expression.hpp`. The mixed layer owns only the type rules, conversions, and
dispatch; it does not reimplement the arithmetic or transcendental formulas.

This is the narrower direct-expression architecture rather than a new
stateful mathematical-program runtime. The `direct-integer-formula/1` grammar
can generate only bounded acyclic postfix trees. There are no loop, sequence,
fold, scan, witness-search, or prime-specific nodes in `MixedKind`; legacy
unrestricted mathematical-program grammars remain unchanged.

## Public APIs

- `MixedNode` constructs typed terminals and typed operation nodes.
- `MixedExpression(nodes, inputCount, limits, requiredRoot)` validates and
  owns an expression; its default required root is `Int64`.
- `MixedExpression::evaluate(span<int64_t>, budget)` returns `MixedResult`.
  `MixedStatus` separates success, mathematical invalidity, and resource limit.
- `MixedExpression::canonical()` keeps conversion and operator identities in
  the output; `serialize()`/`deserialize()` use the versioned
  `genetik.mixed-expression/1` representation and preserve binary64 constant
  bits. Deserialization infers its root type unless the caller supplies a
  required type; the candidate codec explicitly requires `Int64`.
- `MixedIntegerProblem` owns copied exact integer observations.
- `MixedIntegerCandidate` adapts one-input, integer-output expressions to the
  existing candidate fitness/cache contract.
- `DirectIntegerFormulaGrammar` and `MixedIntegerVariation` provide typed
  initialization, subtree mutation, and same-type subtree crossover using the
  caller-owned `std::mt19937&`.
- `CandidateCheckpointCodec<MixedIntegerCandidate>` is provided by
  `MixedIntegerCheckpoint.hpp` and validates the reconstructed expression and
  fitness against the fixed problem context.

The tree is versioned and operator enum identities are explicit. Its current
complexity query reports node count, depth, operation count, constant count,
constant bit count, and canonical constant description bytes. The expression
limits bound node count, depth, real magnitude, and evaluated operator count.
Each evaluation also reports node visits; candidate metrics aggregate
operations and visits separately.

## Numerical contract

`Int64` addition, subtraction, multiplication, and Euclidean modulo use the
existing checked integer implementation. Modulo requires a positive divisor
and returns a nonnegative residue. Integer overflow and invalid modulo are
mathematical invalidity, never wraparound.

`Int64 -> Real64` is an explicitly rounded binary64 conversion using
round-to-nearest, ties-to-even. It is implemented from the integer significand
and IEEE-754 fields so the result is deterministic for every `int64_t`, even
where an integer-to-double cast would be inexact. Every integer with magnitude
at most `2^53` is exactly representable; larger values may round. For example,
`9007199254740993` converts to `9007199254740992`. Converting `INT64_MAX` to
Real64 produces `2^63`; a subsequent Real64-to-Int64 conversion correctly
rejects that out-of-range value.

Real-to-integer conversions first require a finite input in
`[-2^63, 2^63)`, then apply the named rounding operation and recheck the result
before the cast. This avoids undefined floating-to-integer conversion at the
upper boundary. Floor rounds toward negative infinity, ceiling toward positive
infinity, truncation toward zero, and nearest uses halfway-away-from-zero.
Thus `floor(-1.2)=-2`, `ceil(-1.2)=-1`, `truncate(-1.8)=-1`, and nearest maps
`-1.5` to `-2`. `-2^63` is accepted; `+2^63` and values below `-2^63` are
rejected. Binary64 spacing means values near the endpoints occur on coarse
representable steps, not at arbitrary decimal fractions.

Real operations share the strict binary64 domain behavior of `Expression.hpp`:
nonfinite values, division by zero, invalid logarithms, invalid fractional
powers, and results beyond `MixedLimits::maxAbsReal` are invalid. Execution
uses Genetik's strict floating-point build/runtime contract. Binary64
intermediates are approximate real mathematics. An exact checked Int64 return
does not imply exact real arithmetic or a theorem about an unbounded function.

## Fitness and evolution

Fitness compares exact `int64_t` predictions with exact labels. A valid
candidate's score is mismatch count plus one half of a normalized error in
`[0,1)`. Therefore fewer mismatches always outrank any error improvement at a
higher mismatch count. Invalid mathematical evaluations and resource-limited
evaluations are recorded separately and produce invalid candidate fitness;
they cannot be selected as parents. Absolute differences are computed with
unsigned checked-width arithmetic, including the `INT64_MIN`/`INT64_MAX`
distance.

Real constants are full-precision binary64 terminals. The grammar can draw
both configured constants and continuous values from its frozen interval, and
variation preserves their exact bits. Constants are evolved structurally and
by subtree replacement. The existing continuous coordinate fitter is not
applied through floor/ceil/truncate/nearest: those boundaries are
discontinuous, so passing its derivative or probe assumptions through them
would change the objective. No new optimizer was added.

`MixedIntegerProblem::measure` records matches, mismatches, invalid rows,
resource-exhausted rows, summed absolute error, normalized error, mathematical
operations, node visits, and score. The evaluator charges operator count against `maxOperations`; the
validated node and depth bounds cap traversal and stack memory. The problem
uses both a per-row operator ceiling and a total per-candidate operator ceiling.

## Checkpoint compatibility

Candidate records have codec ID
`genetik.mixed-integer-candidate/1`. The expression record includes its
representation version, input count, limits, explicit operator IDs, integer
values, and exact binary64 bit patterns. Candidate fitness and roulette state
are encoded and checked after reconstruction. The run-continuation qualification
uses Genetik's existing `RunCheckpointAccess`, committed-population codec,
random-engine checkpoint, and algorithm-counter checkpoint. Its run identity
binds the grammar profile, conversion/domain policies, fitness semantics,
work/complexity limits, dataset labels, seed, worker count, population,
rates,
elite count, and generation limit.

No legacy expression or checkpoint identity is reinterpreted. No full
prime-discovery run wrapper or CLI is introduced by this implementation.

## Limitations

- Real-valued coefficient fitting through integer rounding is unsupported;
  constants evolve but are not coordinate-fitted.
- The grammar has no reusable named function definitions, arbitrary precision,
  general integer roots/factorials/binomials, or iterative constructs.
- The bounded formula grammar is not a proof that an arbitrary candidate has
  no hidden enumeration. It prevents enumeration structurally because its
  complete operator set contains only finite tree evaluation.
- The implementation enables a direct prime experiment at the representation
  and generic-GA level, but does not add the prime campaign application,
  reference-data loader, or the five-seed protocol runner.
