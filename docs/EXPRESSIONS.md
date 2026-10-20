# Bounded expression values and strict CPU evaluation

This guide describes the expression value/evaluator, which represents and executes mathematical candidates without dataset or RNG access. The later shared-engine structure search and dataset loss are documented in [SYMBOLIC_REGRESSION.md](SYMBOLIC_REGRESSION.md); continuous constant fitting is implemented. Strict division/log and mixed arity are specified in [STRICT_ARITHMETIC.md](STRICT_ARITHMETIC.md). The public API is in [Expression.hpp](../genetik/Expression.hpp); link the existing `genetik_lib` CMake target to receive its include path, C++20 and floating-point options.

## Construction and inspection

```cpp
#include <Expression.hpp>
#include <array>

const auto x = Expression::variable(0); // one binding, named x0 in export
const auto square = Expression::binary(ExpressionKind::Multiply, x, x);
const auto linear = Expression::binary(ExpressionKind::Add, square, x);
const auto f = Expression::binary(ExpressionKind::Add, linear,
                                  Expression::constant(1, 1));
const auto result = f.evaluate(std::array{2.});
// result.valid() is true, result.value() is 7.
// f.toString(): (((x0 * x0) + x0) + 1.0)
// f.nodeCount(): 7; f.depth(): 4
```

Variables are zero-based schema indices, exported as `x0`, `x1`, etc. This does not change the later prime experiment's one-based **data indices**: its single binding x0 receives the declared normalized index n/1000.0, for n=1,2,...,1000 (see PRIME_REGRESSION.md). Each expression declares a binding count. Constant-only expressions default to zero bindings; declare the same schema explicitly when combining constants with variables. Binary composition requires equal schemas, even if some variables are unused. A schema may declare unused variables, but evaluation still validates all supplied values. No randomness or reference data is stored in an expression.

Typed postfix construction is also public and useful for assembling/splicing variable-size candidates in the next milestone:

```cpp
using N = ExpressionNode;
using K = ExpressionKind;
const std::array tokens{
    N::variable(0), N::variable(0), N::binary(K::Multiply),
    N::variable(0), N::binary(K::Add), N::constant(1), N::binary(K::Add)
};
const Expression samePolynomial(tokens, 1);
const auto changed = samePolynomial.withConstant(5, 2); // constant at postfix index 5
// changed is x0*x0+x0+2; samePolynomial still has constant 1.
```

`ExpressionNode` has explicit Variable, Constant, Add, Subtract, Multiply, Divide and Log kinds and private payloads; node inspectors return values. A token sequence is not executable until validated by Expression construction. Validation rejects empty input, missing operands, leftover fragments, out-of-schema variable indices, non-finite/out-of-bound constants and unsupported operator requests. Nodes have no child indices or pointers; stack consumption makes cycles and shared mutable subtrees unrepresentable. Each successful active Expression contains exactly one complete tree. Repeated occurrences, including a repeated input expression in `binary`, are separate owned nodes.

`nodes()` returns a detached vector of typed postfix tokens. It can be edited and used to construct another validated Expression; it is not a writable view. There is no public mutable access to internal nodes, metadata or limits. Input spans/arrays/vectors are copied, including rvalue vectors with retained writable references. Copying yields an independent usable expression; copy assignment prepares a valid copy before swapping. `withConstant`, `unary` and `binary` produce new values without editing operands. An exception during construction/replacement leaves existing values intact. Moves transfer ownership without recursion; a moved-from value is only for destruction, reassignment or swapping, and inspection/evaluation/export of its empty representation throws `logic_error`. Do not copy or derive from a moved-from object before reassigning it. Destruction releases flat vectors, not recursive node chains.

## Limits and counting

`ExpressionLimits` defaults:

| Setting | Default | Meaning |
| --- | --- | --- |
| maxNodes | 1023 | Every variable, constant, unary or binary occurrence counts once. |
| maxDepth | 64 | A leaf has depth 1; unary depth is child+1; binary depth is 1 + max(child depths). |
| maxAbsConstant | largest finite double | Inclusive absolute constant bound; zero is allowed, NaN/infinite/negative bounds are rejected. |

Node/depth maxima must be positive. Construction enforces inclusive limits. Composition uses the explicitly supplied limits (defaults above), rather than implicitly merging the operands' settings; it revalidates constants under the result's bounds. `withConstant` preserves its source limits/schema. Count checks subtract before adding; depth increments happen only after checking the child depth is below the maximum. `SIZE_MAX` configurations do not wrap arithmetic. Raising a limit permits larger work/storage and may cause ordinary allocation/length exceptions; it does not reserve that amount in advance. No storage is allocated proportional to the declared variable count during construction.

## Evaluation outcomes and numerical semantics

`Expression::evaluate(std::span<const double>)` has three distinct outcomes:

- `ExpressionResult` with status Success: `valid()` true and `value()` a finite double, including zero, signed zero and negatives. `failedNode()` is empty.
- Status MathematicalInvalidity: a domain check failed or an operation produced a non-finite result. `valid()` is false; `failedNode()` identifies the first failing **zero-based postfix node**. `value()` throws `logic_error`, so invalidity cannot silently masquerade as zero or a fitness score.
- API/operational exceptions: wrong binding count or non-finite bindings are `invalid_argument`. Invalid construction/configuration is also an API error (`invalid_argument` or a structural limit `length_error`). Out-of-range constant replacement is `out_of_range`; accessing a moved-from object is `logic_error`. Allocation failures and unexpected standard-library exceptions propagate unchanged. No catch-all converts them into mathematical invalidity.

There must be exactly as many bindings as the declared schema, all finite, including unused bindings. Shape/finiteness validation occurs before arithmetic. Evaluation walks postfix nodes in ascending index order, equivalently left subtree, right subtree, then parent. Unary Log consumes its one child and checks positivity before std::log. Division rejects either signed-zero denominator before native division. At every binary node it pops the right operand then the left, performs exactly the declared operation, stores/rounds the result as double and immediately checks finiteness. The first domain failure or non-finite result ends evaluation; difficult nodes/samples are never skipped. There is no clamping, simplification, reassociation or protected operator behavior. In particular `0 * (large * large)` is invalid when the inner product overflows. Separate multiplication/addition nodes are not a fused multiply-add.

Under the supported default IEEE environment, signed zeros follow ordinary binary64 operations and retain their signs when stored/exported. Finite subnormals are accepted. Underflow to a finite zero, including negative zero, is successful rather than invalid. Inexact rounding and floating-point status flags do not themselves invalidate a result. The evaluator does not clear/restore exception flags, change rounding mode, enable traps or manage processor control registers.

### Floating-point build/execution contract

The qualified reference configuration is this Jetson's GCC/libstdc++ 13, native IEEE binary64 (`is_iec559`, 53 significand bits, exponent 1024, `FLT_EVAL_METHOD==0`), serial evaluation, default round-to-nearest/ties-to-even, gradual underflow, and non-trapping arithmetic. Tests check the rounding mode and actual subnormal/underflow behavior on this platform. Callers must retain those settings; changing rounding/trapping/flush-to-zero modes is outside the supported contract. This milestone does not qualify alternate environments or cross-platform bit-identical results.

For GNU/Clang CMake consumers `genetik_lib` now propagates **`-fno-fast-math -ffp-contract=off`**. MSVC receives `/fp:strict` but has not been qualified here; other compilers emit a configuration warning requiring equivalent options and qualification. The header additionally rejects known fast-math/finite-only modes and statically checks native binary64 evaluation. A volatile double store at each operation enforces the node boundary in the reference implementation. The generated Release and downstream consumer command lines were checked for the strict options, and a roundoff counterexample verifies nodes do not fuse.

Every translation unit using this public header must meet these requirements. Direct non-CMake consumers must supply equivalent flags. Do not override these options with unsafe reassociation, finite-only, no-signed-zero, reciprocal approximations or similar flags; the preprocessor cannot detect every individual unsafe option. Do not mix incompatible inline definitions across translation units or rely on link-time optimization to repair them. Merely obtaining a finite answer under an unsupported build does not qualify its semantics. FENV/processor-control changes and floating exceptions are not caught as C++ API exceptions.

### Bounded work

For N nodes, D tree depth and B supplied bindings:

- Construction/validation and copying: O(N) time and O(N) owned/temporary storage. Binary composition copies its resulting N nodes; repeatedly growing a skew tree with binary factories can therefore cost O(N²) across the *sequence* of builds. Batch postfix construction is O(N).
- Evaluation: O(B+N) worst-case work, with at most one visit per node and one operation per unary/binary node (std::log uses the runtime math library); O(N) temporary heap storage for the operand stack. It stops at the first invalid result. Binding validation always visits B inputs first.
- Export: O(N+L) work and O(D+L) temporary storage for explicit traversal frames and output length L. Variable indices and binary64 constant spellings have bounded machine-width lengths. Subtree sizes are precomputed in O(N) owned storage.
- Destruction: O(N) flat storage release, with constant call-stack depth. All traversal stacks are explicit heap vectors; no algorithm recurses with expression depth.

Allocation failures remain operational exceptions. Tests exercise both skew directions at depth 20,001 (40,001 nodes), with iterative export, evaluation, copying and destruction; default limits remain much smaller.

## Faithful text export

`toString()` emits deterministic fully parenthesized infix: `(left + right)`, `(left - right)`, `(left * right)`, `(left / right)` or `log(child)` (natural logarithm). It preserves operand order/tree grouping, variable identity and every stored constant. Negative constants, including negative zero, are parenthesized. Constants use locale-independent `std::to_chars`, general format with `max_digits10` precision (17 for binary64); a `.0` is appended to spellings without a decimal point or exponent. Examples: `1.0`, `(-2.0)`, `0.10000000000000001`, `0.0`, `(-0.0)`.

“Faithful” means the declared tree/operations and a decimal spelling that recovers each stored finite double, including signed-zero bits. It does not promise that another language's parser, integer promotion rules, fast compiler or math system will execute those operations under Genetik's strict semantics. There is no parser, executable-code generator, serialization/JIT or algebraic simplifier in this milestone. Tests independently read representative *single constant* spellings with from_chars and compare their bits, and assert representative full-tree spellings/results without constructing a second general expression parser.

## Public example and verification

Run `out/build/release/genetik_expression_demo`. It explicitly labels the polynomial as **manually constructed**, reports 7 nodes/depth 4, and prints f(-2)=3, f(-1)=1, f(0)=1, f(1)=3, f(2)=7. It also demonstrates explicit invalidity for zero times an overflowing inner product. It performs no optimization or random search.

The dependency-free runtime-check suite covers leaves, multiple variables, unused/non-finite/incorrect bindings, subtraction order/grouping, non-finite constants and arithmetic, first-failure ordering, node rounding/no fusion, signed zeros/subnormals/underflow, inclusive/exceeded node/depth/constant limits, malformed postfix input, SIZE_MAX checks, alias-safe ownership/derived replacement, exact constant export and custom decimal locale independence. There are separate CTest checks for the public example and rejection of unsafe header-compilation modes. Existing evolutionary fault-injection tests remain separate. The downstream CMake consumer constructs, inspects, prints and evaluates the polynomial through the public header/target; it still checks the earlier GA contracts.

Exact commands and qualified outcomes are recorded in the expression milestone section of [BASELINE.md](BASELINE.md). Only the existing public build target's strict FP options affect current GA consumers; no evolutionary-core API or ownership contract changed in this milestone.

## Candidate integration bridge (implemented in the following milestone)

At this evaluator checkpoint the code had two coupling points: `Population<T>` requires `Creature_i<T::gene_type>` and instantiates `T(std::mt19937&)` in its virtual setSize; `GeneticAlgorithm<P,C>` has virtual scalar crossover/mutation bodies that call chromosome/gene setters. Overriding them in a derived class alone may still instantiate those unsuitable base bodies. Merely encoding operation tags as doubles or adding an expression field beside a dummy scalar genome would bypass the established validity contract.

The following design guided the now-implemented bridge; see [SYMBOLIC_REGRESSION.md](SYMBOLIC_REGRESSION.md) for current APIs and [POLYNOMIAL_CAMPAIGN.md](POLYNOMIAL_CAMPAIGN.md) for measured search results. The evaluator-only example above remains manually constructed.

1. Extract the existing representation-independent Fitness/roulette cache and final calculateFitness/invalidation methods into an `EvaluatedCandidate` base. Keep scalar Creature_i's private arithmetic vector/final edits in its derived class, preserving ordinary GA behavior. Add an ExpressionCandidate that privately owns an Expression and replaces it only with a successfully validated value, clearing fitness before publication. Preserve deep copies and no writable aliases.
2. Change Population's static restriction to that common evaluated-candidate contract; retain its staging/ranking/archive/swap code. For expressions, initialize a vector with a configured external bounded factory using `RandomContext::engine()`, then use the existing vector constructor. Make the size-based initialization path conditional with `if constexpr` on T(engine) availability, rejecting unsupported setSize calls explicitly instead of failing template instantiation. Do not secretly acquire data/configuration through a default constructor.
3. Add a defaulted variation-policy template parameter to GeneticAlgorithm, delegating crossover/mutation to a scalar policy for existing users and an expression policy for the new candidate. Pass the existing engine and rates explicitly. Keep the same parent selection and single publishGeneration call; do not duplicate the publication mechanism or insert mutable shared nodes.
4. Generate allowed trees iteratively under explicit remaining-node/depth/constant budgets. Use only the run-owned RNG for variable/constant/operator choices, with a configurable vocabulary. No known-polynomial skeleton is required for default structure search.
5. Perform typed postfix subtree replacement/exchange (subtree boundaries can be computed with an iterative arity scan). Construct a new Expression to validate arity, one-root structure, schema, bounds and constants before replacing a candidate. Bound retries, count attempts, and fall back to the original valid parent when no admissible variation is found; never loop until chance succeeds. Evolve both structure and constants.
6. Give the candidate immutable validated fitting data. For every sample, evaluate bindings, then check residual, squared residual, sum and final mean for finiteness. Use all samples in a documented MSE, minimize it via ObjectiveDirection::Minimize, and map any mathematical invalidity/non-finite loss to Fitness::invalid(). Never omit a difficult sample. API errors and unexpected exceptions propagate through the current nested-error path; invalidity is not an exception.
7. Keep the existing no-throw publication boundary, committed-best definition and consumed-RNG/attempt accounting on failures. The current counter counts candidate evaluator calls; separately declare/count sample/node and later constant-fitting work before claiming an aggregate computation budget.
8. Add a bounded, seeded symbolic-regression example for y=x*x+x+1, with explicit population/generation/variation limits, fixed fitting data and reported failures/best expressions. Deterministic operator/invalidation/error tests qualify correctness; repeated search-quality experiments are separate from tests and must report unsuccessful runs.

Do not add general execution or checkpoint frameworks as part of that bridge. Subsequent milestones still require bounded constant fitting with rollback on worsening, configurable ordinary division/logarithms and domain handling, independent sequence input/reference generation, the one-based index-to-prime mapping, complexity/error separation, frozen validation/extrapolation procedures and CPU profiling before GPU work. The full prime protocol remains in [ROADMAP.md](ROADMAP.md).

Bounded continuous constant fitting is now implemented through the same bridge; see [CONSTANT_FITTING.md](CONSTANT_FITTING.md). It changes only independent constant occurrences and preserves all structural/evaluator/export contracts above. Strict division/logarithms and unary-arity traversal are now implemented; see [STRICT_ARITHMETIC.md](STRICT_ARITHMETIC.md) for domains, feasible grammar selection, runtime libm qualification and tests. Prime regression and profiling remain later work.

`sameRepresentation(other)` compares active values by schema and typed postfix
content, including bitwise finite constant values (signed zeros differ), without
simplifying expressions. Structural-limit objects are constraints and are not part
of this content comparison. It exposes no mutable alias and requires usable values.
Fitting diagnostics additionally operate within one fixed problem/configuration;
a future reusable cache must also account for compatible structural constraints.

Checked dataset loss now owns one internal operand workspace and reuses capacity
across rows; all-row example reporting does likewise. Public single-point evaluate
retains independent local ownership and uses the same private interpreter. Validation,
strict arithmetic, first-invalid-node behavior and all result bits are unchanged.
No public workspace API, cross-call storage or result cache is added. See
[EVALUATION_WORKSPACE.md](EVALUATION_WORKSPACE.md) for independent accepted-output
comparisons, allocation-failure accounting and fixed-work timing evidence.
