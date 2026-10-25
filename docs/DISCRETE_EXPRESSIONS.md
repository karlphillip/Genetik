# Discrete expressions — D1

D1 is an additive, standard-library-only exact language. `DiscreteExpression` owns
validated postfix nodes and derived child/type/depth metadata. It does not extend
`Expression`, `TypedValue` or their floating-point contracts: Int64 and Bool are
separate values, with no implicit expression coercions. Generic candidates,
selection, execution, publication and checkpoint envelopes remain representation
independent. Existing scalar/typed grammars and prime-search commands are unchanged.
The existing scalar measurement/fitter seam accepts floating-point `Expression`
and rows. Reusing it for arbitrary Int64 operands would require the prohibited
conversion, so D1 has a discrete objective/preparation specialization on the same
`EvaluatedCandidate` and executor extension points. No generic base learns about
discrete types, and no second generation-publication mechanism is introduced.

## Public usage

```cpp
#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionFormatting.hpp>
#include <DiscreteSymbolicRegression.hpp>

const auto expression = parseDiscreteExpression(
    "if(x0 == 0, 0, 10 % x0)", 1);
const std::array<std::int64_t, 1> inputs{0};
const auto result = expression.evaluate(inputs);
if (result.valid()) {
    const std::int64_t integer = result.value->integer(); // zero
    (void)integer;
}
const auto canonical = expression.toString();
const auto reparsed = parseDiscreteExpression(canonical, 1);
// expression.sameRepresentation(reparsed) is true.
```

Inputs are exactly the declared ordered Int64 bindings `x0..x[N-1]`. Bool input
bindings are outside D1; Bool literals and computed Bool subtrees are supported.
No integer is stored in double. Value accessors check the variant's actual type;
asking for the wrong type throws `std::bad_variant_access`.

Direct construction uses `DiscreteNode::integer`, `boolean`, `variable`,
`operation`, then `DiscreteExpression(span, dimensions, limits)`. Convenience
`DiscreteExpression::integer`, `boolean`, `variable`, `unary`, `binary` and
`conditional` copy their operands into owned structure. Integer factories accept
integral operands with a checked Int64 conversion; out-of-range unsigned values
throw. Floating-point and bool arguments to integer factories are deleted.
Construction and copying retain no mutable aliases. Copy assignment uses
copy/swap so allocation failure cannot leave mismatched node/metadata buffers.
Replacement is transactional; moved-from evaluation/edit/format access throws
`std::logic_error`.

## Operations and numerical rules

- Int64 → Int64: `Negate`.
- Int64 × Int64 → Int64: `Add`, `Subtract`, `Multiply`, `Modulo`.
- Int64 × Int64 → Bool: `Equal`, `NotEqual`, `LessThan`, `LessEqual`,
  `GreaterThan`, `GreaterEqual`.
- Bool × T × T → T: `IfThenElse`, with T either Int64 or Bool.

No integer division, floating-point conversion, factorial, GCD, primality oracle,
summation or other operator is present. Add/subtract/negate check representable
bounds before signed arithmetic. Multiply checks unsigned magnitudes against the
sign-dependent representable bound before multiplying. In particular negating
INT64_MIN and INT64_MIN × −1 are invalid, without signed-overflow undefined behavior.

Modulo is Euclidean remainder for a **strictly positive** divisor. The checked
positive divisor makes native signed remainder safe even for INT64_MIN; a negative
remainder is increased by the divisor, without overflow. Thus 7 mod 3 = 1,
−7 mod 3 = 2, 6 mod 3 = 0 and 0 mod 3 = 0. Zero/negative divisors are invalid;
there is no protected operation or repair.

Conditional evaluation is genuinely lazy. A bounded explicit frame machine visits
the condition first and then only its selected branch. Unselected overflow,
invalid modulo and nested work do not run. At x0=0 the example above returns zero;
at 2 it returns zero; at −1 its selected modulo branch is invalid. Bool conditions
are mandatory; no integer truthiness exists. An invalid condition propagates.
There is no folding, reassociation, branch rewriting or mathematical-domain repair.

## Outcomes and limits

`DiscreteEvaluationResult` contains status, optional owned value, node visits,
operations and a static reason string. Status is `Valid`, `MathematicalInvalid`,
`ResourceExhausted` or `Cancelled`. Failed/incomplete evaluations contain no value.
Invalid dimensions/configuration and malformed/type-invalid structures throw API
errors; allocation and other operational exceptions retain their original cause.

`DiscreteExpressionLimits` defaults:

| Limit | Default |
|---|---:|
| Nodes | 4,096 |
| Semantic depth | 256 |
| Input dimensions | 1,024 |
| Literal occurrences, including Bool | 4,096 |
| Absolute integer literal magnitude | 2^63 (includes INT64_MIN) |
| Construction storage allowance | 4 MiB |

The storage check covers requested node, metadata and validation-stack element
bytes with division before allocation; allocator bookkeeping is not a measurement.
Builders check total node/storage sizes before concatenation. Literal magnitude
limits apply to integer literals, not runtime inputs or intermediate integers.

`DiscreteEvaluationLimits` defaults to 4,096 node visits, 4,096 primitive operations
and 1 MiB of frame elements. A visit is charged on entering a selected node.
Terminals take no primitive operation. Each arithmetic/comparison operator takes
one; `IfThenElse` takes one branch-dispatch operation after its condition completes.
Failed arithmetic still consumes its operation. A resource refusal does not charge
an operation/visit that did not start. Frame capacity is checked on actual pushes,
so an unselected deep branch cannot consume the selected branch's frame allowance.
The example at zero uses exactly five visits and two operations.

An optional borrowed `const std::atomic<bool>*` requests cancellation during direct
evaluation; no signal handler, threads or global state are installed. Frames and
results are invocation-owned. Shared immutable expressions can be evaluated or
parsed independently from multiple workers. No RNG, fitness, cache or fitting
state is touched by evaluation/formatting.

## Text and presentation

Separately includable `DiscreteExpressionParsing.hpp` provides
`parseDiscreteExpression(text, dimensions, expressionLimits, parsingLimits)`.
Canonical export is fully parenthesized binary syntax, `neg(expr)`,
`if(condition, yes, no)`, decimal Int64 literals, `true`, `false` and `x0` etc.
Operator tokens are `+ - * % == != < <= > >=`.

Noncanonical input accepts ASCII space/tab/CR/LF, parentheses, conventional
left-associative arithmetic and comparison precedence (comparison < add/subtract
< multiply/modulo), and unary minus. A sign immediately attached to digits is
one integer literal; general unary minus becomes `Negate`. Consequently `-7` and
`neg(7)` remain structurally different. `-0` normalizes to integer zero. Canonical
export/reparse preserves nodes, operand order, integer values, types, variable
indices and independent literal occurrences exactly; re-export is byte-identical
when the text fits the configured limits and the parser's hard syntactic ceiling.
Trees constructed directly or from flat left-associative input can export
canonical nesting beyond the parser's ceiling of 128. They remain evaluable/
exportable and reconstruct exactly through the
node-based candidate codec; D1 does not promise parser round trips for them.
Mixed/chained comparisons fail type validation.

Only base-10 integer digits are accepted. Floats, exponent notation, out-of-range
integers, aliases, assignments, statements, implicit multiplication and unknown
functions are rejected. Conversion uses locale-independent `from_chars` with full
range validation. Every input token and trailing byte must be consumed.
`DiscreteParseError` gives a bounded category and byte offset: syntax, unknown
symbol, invalid literal, type or resource. Type errors discovered by final tree
validation point to the completed input position. Invalid parser configuration is
an API error; operational exceptions are not translated into malformed text.

Parsing limits default to 65,536 source bytes, 64 bytes/token, syntactic nesting
64 and 1 MiB of requested node workspace (including growth margin). Recursive
precedence parsing has a hard configuration ceiling of 128 nesting levels; all
recursive entries enforce it. Source size is checked before tokenization, and
node/token limits before conversion/expansion. Semantic limits remain independent.
The parser borrows text only during the call. There is no file-reader API in D1.

`DiscreteExpressionFormatting.hpp` offers Canonical, Unicode and LaTeX formats,
an optional variable-label callback, and a byte allowance (default 1 MiB).
Unicode uses `mod`, `·`, comparisons and `if … then … else`; LaTeX uses `\bmod`
and `cases`. Grouping is explicit and operand order is preserved. The formatter
does not turn `n*n` into a different executable operator. Variable callbacks are
trusted caller code and responsible for their own allocation/label escaping.
Formatting bounds output/live string payload; transient construction is bounded
by twice that payload plus node-bounded string descriptors, excluding allocator
bookkeeping. It never replaces canonical serialization.

## Typed variation

`DiscreteGrammar` declares operators, integer and Bool terminal sources, original
allowed-variable indices, depths and finite proposal limits. Its defaults belong
only to this new language. Initialization/mutation depths default to 4/3, maximum
configured planner depth is 32, generation node allowance is at most 255, and
proposal limit defaults to 8 (maximum 256). At most 1,024 allowed-variable entries and 256 entries per integer source
are supported (larger schemas require an explicit restricted set). Operator
kinds are unique. A copied optional variable set preserves original indices:
nullopt means all inputs, an empty vector none.

`DiscreteVariation` builds finite two-type witness plans before consuming RNG.
For each allowed depth it keeps the smallest validated witness per result type
(ties prefer fewer literals). Bool outputs can be constructed from comparisons
without a Bool terminal. No Int64 output witness means rejection before search.
The planner is a bounded constructive strategy, not exhaustive grammar enumeration.

Generation chooses a feasible operator or a terminal (terminal-category chance
0.35 when both are feasible), reserves witness-sized node allowances for remaining
operands and recursively builds within the depth ceiling. Complete expression and
grammar validation enforce all limits, including literal counts. A rejected
proposal is tried at most the configured count, then an owned validated witness
is returned. This fallback may bias restrictive grammars; it is not mathematical
validity regeneration and never injects a target model.

Mutation chooses a postfix root uniformly, generates a same-result-type subtree
and checks the complete replacement; terminal roots may receive new typed
terminals or same-type trees. Crossover chooses two postfix roots, requires equal
result types and validates both complete offspring. This is stronger than arity
matching. Context restrictions are rechecked, especially modulo divisors. At most
the configured number of attempts is made; failure returns owned unchanged
parent copies. Structural edits invalidate fitness; parents and unrelated literal
occurrences remain unchanged. Mathematical invalidity is handled in preparation.

By default, grammar modulo divisors are literal terminals from the positive
`positiveDivisors` source `{2,3,5}`. They can occur outside the general constant
source only in the divisor position. Set `positiveConstantDivisorsOnly=false`
for general Int64 divisor subtrees; the evaluator still rejects runtime values
≤0. The grammar restriction changes no evaluation rule. All draws use the supplied
run-owned `mt19937`; no hidden random engine or discrete-prime oracle exists.

## Regression and shared evolution

`DiscreteRegressionProblem` owns complete Int64 input rows and Int64 targets.
Data are validated and copied before search. The objective is **mismatch count**:
no secondary term is used. It is in [0,rowCount], and zero means every required
integer output matches exactly with valid evaluation. No residual subtraction can
overflow. A failed row makes the whole score Invalid; successful prefixes cannot
produce valid scores. Reporting `measure(expression, true)` inspects all rows and
counts invalid rows without changing training fitness.

Rows are traversed in caller order. Data limits include 100,000 default rows (D1
ceiling 1,000,000), 10,000,000 input values and 10,000,000 visits/operations per loss
(each configurable work cap ≤1,000,000,000). A row's evaluator receives the remaining
allowance; spent partial work is retained. Resources throw `DiscreteResourceExhausted`
through the existing failed-preparation mechanism, never Invalid fitness.

`DiscreteTrainingObjective` is noncopyable; its explicit worker factory creates
independent counter state. It admits one loss attempt for a complete objective
invocation, including invalid/partial/throwing attempts. It records rows, visited
nodes, primitive operations and invalid losses/rows separately. Its D1 total-loss
ceiling is 1,000,000,000. Workers share only synchronized admission and immutable
data; mutable counters and evaluation frames are private, merged on the coordinator
after draining, including on failure. An optional execution pointer observes active
batch cancellation between rows; the search adapter binds it automatically.
Operational exceptions retain their causes. There is no constant fitting in D1.

`DiscreteExpressionCandidate` uses `EvaluatedCandidate`'s final cache lifecycle.
Fitness context includes both immutable problem and training-objective identity;
equal schemas/data do not authorize cache transfer. Replacement validates/copies
before invalidating. `DiscreteRegressionOrdering` minimizes mismatch count, then
nodes, depth and canonical text consistently for selection, elitism and best-seen.

`discreteSymbolicRegression(problem, config, random)` composes the existing
`Population` and `GeneticAlgorithm`, preserving virtual selection/variation,
private preparation, draining and atomic generation publication. It accepts a
fresh `RandomContext`, supports its real worker count, checks full-population
admission at boundaries, and distinguishes exact fit, generation limit, loss
budget, no viable population and cancellation. Operational/local resource errors
propagate; they are not an invented stopping fitness. No parallel sub-evaluators,
new executor or duplicate population-publication implementation is introduced.

## Checkpoint boundary

`DiscreteExpressionCheckpoint.hpp` adds explicit candidate codec
`genetik.discrete-expression-candidate/1`, with bounded node/type codes, exact
decimal integer fields, validated cached mismatch score and selection state.
It uses the existing generic envelope, not raw memory; native format 1 and existing
codecs remain unchanged. IDs/order are part of this codec's version contract.

The codec requires an externally validated `DiscreteCheckpointContext`. A complete
application checkpoint must additionally identify the exact rows/schema, limits,
grammar, objective and execution configuration and persist its objective work and
variation state using the existing generic run-state customization points. D1 does
not expose a discrete CLI resume workflow or claim arbitrary custom subclasses
are serializable. Existing prime checkpoint behavior is unaffected.

## Runnable example and qualification

```sh
cmake -S . -B out/build/discrete-release -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/discrete-release -j2
out/build/discrete-release/genetik_discrete_quickstart --threads 4
```

The compiled quickstart evaluates/exports/reparses a parity-dependent integer
model, demonstrates lazy invalidity, rejects a type error and runs a bounded
504-attempt discrete search against nine independently generated integer rows.
The target expression is **not** inserted into search initialization; discovery is
not a test requirement. The same source builds as an installed/relocated consumer.
No prime discovery campaign or recovery claim is made by D1.

[Qualification and retained failures](DISCRETE_D1_QUALIFICATION.md) records actual
commands, manifests, source scope and results. Independent reference generation is
`scripts/generate_discrete_references.py`: Python exact integers, seed 1729,
144 boundary pairs plus 256 full-width pairs across ten binary operators = 4,000
frozen fixtures in `tests/discrete-arithmetic.tsv`. The production implementation
never generates its own oracle. Lazy branches, Negate, Bool conditionals, limits,
ownership, type-preserving variation, publication failures, candidate codec and
forced-overlap 1/2/4-worker equivalence have focused additional fixtures.
