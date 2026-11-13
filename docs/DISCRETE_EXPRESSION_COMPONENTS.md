# Reusable discrete expression components

`<DiscreteExpressionComponents.hpp>` provides an optional, bounded construction
DAG for exact Int64 and Boolean expressions. It stores immutable components once
and lets later components refer to earlier ones. This is a representation and
composition capability; it does not change what `DiscreteExpression` can express
or improve evolutionary discovery by itself.

## Example

```cpp
#include <DiscreteExpressionComponents.hpp>
#include <DiscreteExpressionParsing.hpp>

DiscreteExpressionComponents components(1); // one Int64 input, x0
const auto x = components.addExpression(DiscreteExpression::variable(0, 1));
const auto five = components.addExpression(DiscreteExpression::integer(5, 1));
const auto remainder = components.binary(DiscreteKind::Modulo, x, five);
const auto condition = components.addExpression(parseDiscreteExpression("x0 < 0", 1));
const auto correction = components.binary(DiscreteKind::Subtract, five, x);
const auto root = components.conditional(condition, correction, remainder);

auto result = components.materialize(root);
if (!result.success()) {
    // ResourceExhausted; no partial expression is returned.
    throw std::runtime_error(result.diagnostic);
}
auto ordinary = std::move(*result.expression);
auto value = ordinary.evaluate(std::array<std::int64_t, 1>{-3});
```

The complete installed-consumer example is
[`examples/quickstarts/discrete_components.cpp`](../examples/quickstarts/discrete_components.cpp).

## Model and semantics

An arena fixes one shared schema of Int64 external inputs. A component is either
an owned, closed `DiscreteExpression`, or a unary/binary/ternary operator over
earlier component handles. Handles are arena-specific; expired and foreign
handles are rejected. Edges point only to existing components, so the graph is
acyclic. Composition validates operand types using the existing `DiscreteType`
rules: arithmetic consumes Int64, comparisons produce Bool, and conditionals
require `Bool,T,T` while preserving child order. Reduction operators are
imported as existing closed expressions rather than reconstructed with a second
binding language.

Imported expressions with free outer iterators are rejected. Closed expressions
may contain nested bounded reductions. Each occurrence is expanded with fresh,
deterministic binder identities, so reusing a reduction cannot capture or
collide with another occurrence. The final value is an ordinary validated
`DiscreteExpression`; evaluation, formatting, parsing, recurrence use, and
checkpoints use the existing APIs.

Structural sharing applies only while constructing and retaining the component
DAG. Materialization expands each reference into the postfix tree. Evaluation
therefore performs every occurrence normally: there is no memoization and no
claim that a smaller stored DAG makes evaluation faster. The component arena is
not a replacement expression language and does not support component parameters
or feedback.

## Limits and accounting

`DiscreteComponentLimits` bounds unique components, dependency edges, estimated
retained payload, expanded node count/depth/constants/binders, estimated peak
materialization storage, and materialization work. Limits are checked before a
component is published or expanded. A materialization resource failure returns
`ResourceExhausted` with no expression. Invalid schemas, handles, types, and
operators throw configuration exceptions; allocation and unexpected failures
propagate.

`storageStats()` reports unique component and edge counts plus a logical retained
byte estimate and `constructionWork`. `maxConstructionWork` bounds its cumulative
logical charge. The construction work convention
charges each imported expression node once for its metadata scan, once for
copying its postfix node, once for copying its validation metadata, each binder
entry copied once, and one publication unit. Composition charges one unit per
dependency edge, one for type validation, and one for publication. Work admission
occurs before copying an imported expression or publishing a component.
If a work admission fails, the attempted component is not published; already
charged work for completed scans remains in `constructionWork`.

`expansionStats(handle)` reports expanded tree complexity, expanded component
dependency-edge occurrences, and a conservative materialization-work upper
bound. This edge count differs from `storageStats().edges`, which counts unique
edges retained in the DAG: if a shared child is referenced twice, its edges are
counted twice in expansion statistics. Materialization returns actual logical
counts for emitted nodes, expanded component occurrences, child-handle edge
visits, generated binder mappings, and every comparison performed by the
deterministic linear binder remap. It reports `totalUnits` for output,
dependency-edge, and remapping actions, and reports the final
expression validation allowance separately as
`expressionValidationWorkUpperBound`. `totalWorkUpperBound` combines these for
admission. The validation allowance charges 16 units per expanded postfix node plus
`2*B² + I*B`, where `B` is the expanded binder count and `I` is the number of
iterator terminals. This conservatively covers the existing postfix
metadata/type/depth checks, copied expression metadata, scope traversal, binder
uniqueness searches, and active-scope searches. The component remapper has a
separate upper bound `U*B`, where `U` counts both iterator terminals and
iteration declaration nodes; every comparison is also counted in the returned
actual work. The materialization admission bound includes both remapper and
validation allowances and is checked before expansion begins.

Materialization byte admission includes staged nodes, expression
validation/storage, traversal frames, and a conservative binder-map allowance.
These are deterministic admission estimates, not process RSS caps; allocator
metadata and implementation-specific over-allocation are not measured. Runtime
evaluation work is reported by `DiscreteExpression` and is separate from
construction and materialization accounting. `toString(handle, maxBytes)` has
its own output-byte limit and is not charged to either work counter.

`toString(handle, maxBytes)` emits deterministic structural text for the component prefix
through that handle. The materialized ordinary expression's canonical export is
available from `DiscreteExpression::toString()` and can be parsed using the
existing discrete expression parser.

## Scope and evidence

The reusable representation supports arithmetic, modular and advanced integer
operators, comparisons, lazy conditionals, and closed bounded reductions already
supported by `DiscreteExpression`. It adds no operator, solver, evaluator, cache,
search heuristic, GA path, default, RNG draw, or checkpoint format.

Prior compositional search evidence did not show improved exact recovery over
bottom-up search (both achieved 10/40 joint recoveries, all on arithmetic
controls). This API is justified as a reusable exact mathematical
representation only. Any search-recovery benefit would require a separate,
independently qualified comparison; finite sample agreement remains finite-domain
evidence rather than universal equivalence.
