# Typed symbolic regression — Phase 3 contract

This additive specialization uses EvaluatedCandidate, Population and GeneticAlgorithm;
the generic engine does not include mathematical headers. Existing scalar search
and ordering remain unchanged. This design is recorded before implementation.

Checkpoint 1 owns an ordered schema, rows and fixed output shape in
TypedRegressionProblem. Values are finite, dimensions match exactly, and all data
is detached before a run consumes randomness. Equal schema does not imply equal
objective identity. A new dataset requires a new problem/run. Each preparation
scores freshly; evaluation-only measure calls never update training fitness.

Loss is equal-weight MSE over rows and row-major output coefficients (vectors in
increasing index). Start sum at +0; for each residual check subtraction, square,
and sequential sum; divide once by the checked row/component product. Invalid
predictions or reduction arithmetic invalidate the whole loss. Reporting inspects
all rows when requested and counts invalid rows, never inferred invalid components.
Resource exhaustion is TypedResourceExhausted, distinct from invalid fitness and
operational errors. Admission charges a loss before validation/allocation; attempted
rows are charged before evaluation. Primitive work counts expression primitives
and checked loss subtraction/multiplication/addition/division separately from rows
and loss calls; it is diagnostic, not a new global engine budget.

Checkpoint 2 constructs from a finite shape vocabulary and enabled productions.
Subtrees match exact shapes and schemas, then complete replacements are revalidated
against all expression limits. Finite proposals fall back to an owned unchanged
value. Feasible witness plans permit result shapes without direct terminals.
Selection/elitism/archive order: finite MSE, nodes, depth, constant occurrences,
adjustable scalar coefficients, then faithful text. No fuzzy score sorting.

Checkpoint 3 uses the existing coordinate-search rules with independent constant
coefficients in postfix occurrence then row-major coefficient order. Refinement is
optional, scheduled at initialization and prospective generation multiples, private
to each task; accepted expression and score travel together. No random fitting,
shared scratch, nested workers, result cache or cross-problem fitness reuse.

All phases retain the existing admission/drain/failure-priority/publication policy.
Parallel callbacks require explicit qualification and independent worker factories.
Only committed generations can update best-seen state; abandoned work remains
counted. No typed dataset parser or new arithmetic operation is introduced.

Checkpoint 1 public API: TypedRegressionRow owns vector<TypedValue> inputs and one
TypedValue target; TypedRegressionProblem::create(schema, output, rows, expressionLimits,
datasetLimits) validates/copies them. Dataset limits default to 100,000 rows,
10,000,000 stored coefficients and 1,000,000,000 arithmetic units per loss.
checkedExpression rejects predictable full-loss work violations before search.
measure(expr, rowCounter, arithmeticCounter, inspectAllRows, localArithmeticCap)
can lower that allowance for a synchronous reporting call; exhausted calls throw
TypedResourceExhausted with attempted rows/work retained. Expression work plus three
loss primitives per component plus final division form the valid full-loss bound.
Invalid arithmetic can stop earlier. Reporting invalidRows counts expression-invalid
rows only; loss-overflow invalidity does not imply an invalid component count.
TypedTrainingObjective reuses shared TrainingLossAdmission and TrainingWork, charging
before compute; exceptions acquire nested attempt context. TypedScoring forks
exclusive objectives on workers, merging counters after drain. TypedExpressionCandidate
uses the same controlled edit/final fitness wrapper and problemIdentity contract as
scalar expression candidates; public replacements invalidate fitness even if equal.
