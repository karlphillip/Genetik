# Public API guide

Start with the compiled [scalar quickstart](../examples/quickstarts/scalar.cpp),
application-owned [permutation consumer](../tests/oop_consumer/main.cpp), or
[symbolic quickstart](../examples/quickstarts/symbolic.cpp). Link Genetik::Genetik through
[installed or source inclusion](PACKAGING.md); all headers are self-contained.
Class names remain in their existing global namespace in 0.1; the CMake namespace
is not a C++ namespace migration.

| Area | Public entry points | Contract guide |
| --- | --- | --- |
| Randomness | RandomContext(seed), engine(), evaluationAttempts() | [Migration](MIGRATION.md) |
| Execution | ExecutionConfig, ExecutionContext, --threads N | [Concurrency](CONCURRENCY.md) |
| Scalar candidates | Creature_i<T>, controlled gene edits; Creature<char> demo | [Quickstart](../examples/quickstarts/scalar.cpp) |
| Evolution | Population, GeneticAlgorithm, GenerationOutcome | [Shared-engine contracts](SYMBOLIC_REGRESSION.md) |
| Fitness | Fitness, FitnessState, ObjectiveDirection | [Validity/publication](BASELINE.md) |
| Text import | parseExpression, parseTypedExpression, ExpressionParseLimits, ExpressionParseError | [Bounded grammar/import](EXPRESSION_PARSING.md) |
| Expression values | Expression, ExpressionNode, ExpressionLimits, ExpressionResult | [Expressions](EXPRESSIONS.md) |
| Data/loss | RegressionRow, RegressionProblem::create, measure | [Multivariate/data workflow](MULTIVARIATE.md) |
| Search | symbolicRegression, SymbolicRegressionConfig/Result | [Symbolic regression](SYMBOLIC_REGRESSION.md) |
| Fitting/work | TrainingObjective, fitConstants, ConstantFitConfig | [Constant fitting](CONSTANT_FITTING.md) |
| Scheduling | fitEveryGenerations (default 1) | [Cadence](FITTING_CADENCE.md) |
| Domains | Divide, unary Log (opt-in grammar) | [Strict arithmetic](STRICT_ARITHMETIC.md) |

## Ownership, evaluation and stopping

Own one explicit RandomContext per run; it must outlive borrowed population/algorithm
references. It is noncopyable. No hidden reseeding or fitter RNG draws. Repeatability
requires the exact revision/version, toolchain, standard/runtime math libraries,
FP environment, ordered input, configuration and execution mode; see SUPPORT.md.

Scalar genomes and Expression values own their data. Copies detach mutable storage;
public edits invalidate cached fitness. Expression inspection exposes no writable
aliases; moved-from expressions support reassignment/destruction, not evaluation.
Candidates share immutable copied problems, never mutable target arrays. A changed
objective/data requires a new run. Cache values cannot be transplanted between
separate problems or changed expressions.

Unevaluated is distinct from Valid and Invalid. Valid zero/negative finite scores
are ordinary scores; set minimization/maximization independently. Invalid mathematical
results cannot win selection, contribute a partial-data MSE or appear as a valid best.
API errors (shape/configuration) throw; unexpected failures propagate with nested
context and original cause. A failed attempted generation preserves committed
population/genomes/fitness/generation/best while consumed RNG/work stays consumed.
No implicit retry or regeneration occurs; inspect GenerationOutcome/SearchStop.
ExecutionContext::requestStop() provides cooperative cancellation of an active
preparation batch; it does not interrupt blocking callbacks or provide a sticky
engine-wide token. There is no general observer or checkpoint/resume framework.
See [the execution contract](CONCURRENCY.md).

Regression binds x0..x[d-1] to caller input-vector order and returns one scalar.
Rows must have consistent dimensions and finite values/targets; zero-input constant
problems remain supported. Keep evaluation-only data out of fitting, stopping and
selection, and score it only after finalizing the training-selected expression.

Bound candidate requests, generations and actual training-loss attempts explicitly.
Actual loss counts include fresh baselines, fitting probes, invalid and throwing
attempts; rows count visited work, which may stop early. Local fit limits retain the
accepted expression/score pair. Global exhaustion discards incomplete private work;
it does not promote an uncommitted candidate. Post-search reporting work is separate.

Constant fitting changes independent constant occurrences with fixed topology;
it accepts only strictly lower finite training loss and may stall. Initialization
is generation zero; later fits are eligible at multiples of fitEveryGenerations.
Unscheduled candidates still need matching fitness. Interval 1 remains the default.

## Numerical scope and export

The grammar is variables, finite double constants, +, -, *, strict / and natural
log. Default operators remain +,-,*; configure extra operators explicitly. Division
rejects either signed-zero denominator; Log requires positive input. Every
intermediate result must be finite; invalid children cannot be masked by a parent.
Finite subnormals and signed-zero underflow are valid under the qualified FP
settings. No epsilon denominators, protected values, simplification or prime oracle.

Node/depth/constant limits bound validated expressions and structure variation.
Checked full-data MSE uses the documented ordered residual/square/sum checks.
Exact score ties rank smaller node counts then faithful text, not a success tolerance.
Expression::toString preserves topology, operand/variable identity and full-precision
constants, including signed zero, with locale-independent spelling. It is a faithful
text description, not a general serialization/parser/code-generation contract.

The header target's strict FP options affect consuming C++ translation units.
Default IEEE rounding, gradual underflow, nontrapping arithmetic and qualified libm
are required. [Expression](EXPRESSIONS.md) and [strict-arithmetic](STRICT_ARITHMETIC.md) guides define the details; do not mix
translation units built with incompatible options. Workspace reuse is invocation-
local storage management, not cached mathematical evaluation (EVALUATION_WORKSPACE.md).

## Custom representations and OOP

See [architecture and external extension guide](ARCHITECTURE.md) for runtime virtual
candidate, evaluator, preparation, selection and variation contracts and the installed
permutation consumer. Scalar and symbolic candidates share the same engine; generic
core includes do not require regression. [Concurrency](CONCURRENCY.md) is implemented as an opt-in library-owned CPU
backend, defaulting to one. Use RandomContext(seed, ExecutionConfig{N}); custom
callbacks need explicit qualification and independent mutable state.


## Additive typed mathematical evaluation

`TypedValue.hpp` owns distinct finite scalars, column vectors and row-major matrices.
`TypedExpression.hpp` validates typed postfix trees against an ordered shape schema,
infers shapes, exports faithful text and evaluates with explicit arithmetic/payload
limits. See [TYPED_EXPRESSIONS.md](TYPED_EXPRESSIONS.md) and the compiled installed
[typed quickstart](../examples/quickstarts/typed.cpp). The generic GA and existing
scalar symbolic-regression APIs do not include or require typed mathematical headers.
Typed datasets, aggregate loss, shape-preserving evolution and coefficient fitting
are implemented; see the linked typed-regression guide below.

Typed mathematical search: [TYPED_REGRESSION.md](TYPED_REGRESSION.md), owned aggregate datasets, shape-aware evolution and optional coefficient fitting.

Scientific scalar functions in either API: [SCIENTIFIC_OPERATORS.md](SCIENTIFIC_OPERATORS.md).
Sin/Cos/Exp/Sqrt and discrete Powi are opt-in, scalar-only productions that may
compose with aggregate scaling; default grammars are unchanged.
