# Bounded expression import (Phase 5)

Design contract before implementation: parsing is an additive header-only input
boundary, not an evaluator or GA dependency. `ExpressionParsing.hpp` and
`TypedExpressionParsing.hpp` construct the existing owned postfix values. No
third-party toolkit, network download, callback language or numerical backend.

Grammar: `expression := term { ('+'|'-') term }`; `term := primary
{ ('*'|'/') primary }`; `primary := number | xINDEX | '(' expression ')'
| unary '(' expression ')' | powi '(' expression ',' UINT ')'`.
Unary names are log, sin, cos, exp, sqrt; typed also accepts transpose and binary
functions dot, matvec, matmul. Typed literals use the existing exporter spelling
`scalar(number)`, `vector[N](numbers)`, `matrix[R,C](numbers)` with row-major
coefficients. Bare numbers are also scalar constants in typed input. Numbers may
have a sign, decimal point and decimal e/E exponent. Negative exported literals
remain one constant, including parenthesized `(-0.0)`. General unary `-x0` is
rejected; use explicit `(-1.0)*x0`. Operators associate left; multiplication and
division bind more tightly than addition/subtraction. No implicit multiplication,
assignments, statements, general powers, aggregate scientific functions or aliases.

All six ASCII whitespace characters are accepted (including CRLF); identifiers are
case-sensitive, canonical variables are x0,x1,... with no leading-zero alternatives.
Dimensions and powi exponents are unsigned decimal integers, not expressions.
Powi range is0..8. Aggregate dimensions are positive. Complete input consumption
is mandatory. Comments, hexadecimal/NaN/Inf numbers, quoting and locale decimal
commas are unsupported. Unknown symbols, syntax/arity, literals, shapes and limits
have distinct bounded `ExpressionParseError` categories and zero-based byte offsets.
Invalid API configurations throw invalid_argument separately. Allocation failures
and unexpected operational exceptions propagate unchanged. A parse returns a new
value; assignment only occurs after success.

Parser limits independently bound bytes, tokens, token bytes, nesting, constants,
coefficients, variables and workspace. Recursive descent is hard-capped at128
syntactic nesting levels (default64); flat infix chains are iterative. Semantic
nodes/depth, type/shape, arithmetic and payload bounds remain the existing
expression limits. No parse performs mathematical evaluation, consumes randomness,
starts workers or charges regression loss work. Invocation-local state supports
concurrent calls with immutable shared text/schema.

Round-trip contract: for the bounded import profile (at most128 syntactic nesting
levels), with the same schema and sufficient limits, export/parse
preserves every postfix kind, variable, shape, coefficient bit (including signed
zero), independent constant occurrence and exponent. Re-export is byte-identical.
Convenient input such as `1+x0*2` normalizes to canonical parenthesized text.
Programmatically built expressions requiring more than128 levels in canonical
text remain valid evaluator values but are outside this import profile. No claim
of arbitrary-depth parsing is made. No algebraic rewriting or folding occurs. Parsing log(-1) or1/0 succeeds;
evaluation reports mathematical invalidity. Resource stops remain distinct.

## Public API and error handling

```cpp
#include <ExpressionParsing.hpp>
#include <TypedExpressionParsing.hpp>
#include <array>
const ExpressionParseLimits textLimits{};
const auto f = parseExpression("sin(x0)+powi(x0,2)", 1, textLimits,
                               ExpressionLimits{});
const auto y = f.evaluate(std::array{0.5});
const std::array schema{TypedShape::matrix(2,3), TypedShape::vector(3),
                        TypedShape::vector(2)};
const auto g = parseTypedExpression("matvec(x0,x1)+x2", schema,
                                    TypedShape::vector(2), textLimits,
                                    TypedExpressionLimits{});
const auto coefficients = parseTypedExpression(
    "matrix[2,3](1,2,3,(-1.0),0,4)", {}, TypedShape::matrix(2,3));
```

`parseExpression(text, variableCount, parsingLimits, expressionLimits)` binds xI
by index I. `parseTypedExpression(text, schema, expectedOutput, parsingLimits,
expressionLimits)` additionally validates exact kinds/shapes, including unused
schema entries, through the existing typed rules. Scalar variables do not acquire
implicit shapes. Bare typed numeric literals normalize to `scalar(...)`; vector
length1 and matrix1x1 remain distinct. `*` is scalar multiplication/scaling, never
an ambiguous aggregate product; use dot/matvec/matmul explicitly. The parser does
not import the separately exported `schema{...}` annotation: supply that schema
through the API. Schema strings are inspection, not a second text language.

Catch `ExpressionParseError` to inspect `category()` (Syntax, UnknownSymbol,
Arity, Literal, Shape, Resource) and `offset()` (zero-based source byte; EOF may equal
source length). Diagnostics contain a fixed message and byte location, never an
unbounded source echo. Categories concern import, not mathematical validity.
Invalid parser counts/nesting or invalid expression/API configuration remain
`invalid_argument` (typed shape-capacity configuration can throw existing
`TypedLimitError`); allocation failures propagate as `bad_alloc`. There is no
catch-all conversion to a parse error or Invalid fitness. Structural/plan limits
on an otherwise syntactically accepted expression become Resource parse errors.
Use `existing = parseExpression(...)` for strong replacement: the new value is
fully built before existing assignment. No writable text/schema/payload aliases
are retained. Returned expressions have all the original copy/move contracts.

Decimal conversion uses locale-independent `from_chars(..., general)` with full
consumption and finite-result checks. A leading literal sign is handled explicitly
so `+1`, `-.5`, `1.`, and scientific decimal notation are accepted. Nonfinite,
hexadecimal, malformed and conversion-range-error tokens are rejected. This
includes nonzero underflow reported out of range, instead of silently storing zero.
Accepted finite subnormals and extreme binary64 exporter literals retain their bits.
`-0`, `-0.0`, and the canonical `(-0.0)` produce the same single negative-zero
constant. Parentheses are transparent and never generate extra expression nodes.
An exponent such as `2.0`, `-1`, or `x0` is not accepted by powi. Its unsigned
metadata is never a constant occurrence or continuous fitting parameter.

## Resource units

| ExpressionParseLimits field | Default | Unit / policy |
| --- | ---: | --- |
| maxSourceBytes | 1048576 | source bytes, checked before scanning |
| maxTokens | 65536 | non-EOF tokens, including delimiters/signs |
| maxTokenBytes | 256 | number or identifier bytes, checked during scanning |
| maxNesting | 64 | nested grouping/function/literal parentheses; positive, hard ceiling128 |
| maxVariables | 64 | supplied ordered schema size / scalar binding count |
| maxConstantNodes | 1024 | independent scalar/aggregate constant occurrences; zero allowed |
| maxConstantCoefficients | 65536 | sum of scalar coefficients; zero allowed |
| maxWorkspaceBytes | 8388608 | conservative parser requested-payload charge |

Workspace charge is deterministic: reserve at most `min(sourceBytes,maxNodes)`
node and shape/depth-stack slots once; charge their concrete sizeof values, plus
1024bytes per permitted nesting level and four double slots per imported constant
coefficient. The latter covers temporary literal storage/value/node copies. Checked
subtraction/division precedes multiplication/addition. It is an upper allowance on
this parser-owned capacity bookkeeping, **not total process memory**, allocator
metadata, the borrowed source/schema, or the final owned expression/its validator's
storage (those retain their existing expression limits). Charge is conservative;
not all charged slots are simultaneously live. Increasing a configured allowance
does not bypass dimension/count overflow checks. Aggregate dimensions and payload
counts are checked before reserving literal storage. Nodes/semantic depth and
constant bounds are checked as nodes are emitted. Complete typed execution-plan
arithmetic/temporary bounds are verified by the existing final constructor before
the result becomes executable.

Lexing is streaming, with no source copy or token array. Recursive descent has at
most128 guarded primary/literal nesting levels, while arbitrary-length allowed
infix chains use loops and postfix output. It requires a normal application thread
stack (at least1MiB); tiny custom thread stacks are unsupported. Final expression
validation, traversal and destruction retain their iterative behavior. Import work
is linear in bounded text/nodes/payload, apart from the existing constructor's
validated metadata work. Expression-node/depth limits and textual nesting limits
are intentionally independent: `((((x0))))` has one semantic node but four levels
of syntactic nesting.

## Installed quickstart and file workflow

The compiled installed [parsing quickstart](../examples/quickstarts/parsing.cpp)
constructs scalar scientific and rectangular typed imports, owns a matrix literal,
evaluates A*x+b=[14,8], exports/reparses and prints a rejected-input diagnostic.
Use the existing install/package commands and build examples/quickstarts against
Genetik::Genetik; `parsing_quickstart` is added to that external project.

The companion application uses the **same existing finite numerical table reader,
ordered column mapping, checked regression loss and all-row reporting**:

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/release -j2
mkdir -p out/data/formula
printf 'x,y\n-1,-1.75\n0,0\n1,1.75\n' > out/data/formula/training.csv
printf '1.0*x0\n' > out/data/formula/expression.txt
out/build/release/genetik_regression_evaluate out/data/formula/training.csv \
  --expression out/data/formula/expression.txt --inputs x --target y \
  --fit --fit-budget 128
```

Without `--fit`, the supplied expression is only evaluated. The executable never
starts evolution or requires seed/threads options. `--fit` explicitly applies the
existing deterministic bounded coordinate fitter to **training rows only**; default
bounds[-8,8], step1, minimum1e-8,128sweeps,8192proposals,256loss calls (overridden
by --fit-budget, range1..1000000). Starting constants must already satisfy bounds;
there is no projection. `--evaluate OTHER.csv` may repeat up to8 times; all files
are validated before fitting, and their rows enter reporting only after the model
is finalized. Reporting work is separate from fit loss/row counters. A mathematically
invalid model is reported as full-dataset `mse=invalid`; an independent all-row scan
reports the actual invalid prediction total, never a valid-prefix loss.
The expression reader enforces1048576bytes during reading, including CRLF/newlines;
file errors identify the path and parse errors add byte/category diagnostics.

Fixed-structure scalar and aggregate fit tests compare imported and independently
constructed representations, accepted results/losses and work counters. A parsed
sqrt fixture also retains accepted progress after invalid probes. Parsing changes
neither fitter acceptance nor global search/publication budgets. It adds no formula
initialization, parameter sharing or hidden simplification.

## Verification scope and future work

See [PARSING_QUALIFICATION.md](PARSING_QUALIFICATION.md) for exact executed commands,
manifests and limitations. Deterministic bounded generated-expression round trips
and malformed-input mutations supplement direct fixtures and allocation-failure
injection. There is no fuzz-toolchain dependency. Gated typed calls and concurrent scalar calls
share immutable source/schema and own all parser state; separate TSan executes
this fixture as well as the existing five concurrency selections.

No typed CSV serialization, model archive, templates, friendly aliases, general
expression parser callbacks, symbolic algebra, new operations, calculus or external
adapter is implemented. Existing search commands/grammars and generic OOP interfaces
are unchanged. Hosted qualification of this new phase is pending a separate upload.
