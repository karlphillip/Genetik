# Generic multi-input symbolic regression

Genetik remains a reusable C++20 genetic-algorithm library. Its symbolic-regression
module accepts ordinary rows `(x0,...,x[d-1]) -> y`, for any positive dimension that
fits the caller's resources. A row owns its ordered double input vector and scalar
double target. The library also retains zero-input constant-only problems for
compatibility; the file example requires at least one selected input.

```cpp
#include <SymbolicRegression.hpp>
const auto problem = RegressionProblem::create({
    {{-1., 2., 0.}, 4.},
    {{ 0., 3., 1.}, 7.},
    {{ 2., 0., 4.}, 9.}
}, {31,6,8});
SymbolicRegressionConfig config;
config.population = 64;
config.maxGenerationAttempts = 2000;
config.maxEvaluations = 100000;             // outer candidate requests
config.maxTrainingLossAttempts = 100000;   // actual baseline/probe/throwing loss calls
config.trainingLossThreshold = 1e-14;
config.grammar.continuousProbability = .5;  // remaining source defaults: [-8,8), discrete {-2,-1,0,1,2}
config.fitting = ConstantFitConfig{};       // optional; reset for no fitting
config.fitting->lossThreshold = 1e-14;
RandomContext random(42);
const auto result = symbolicRegression(problem, config, random);
if (result.best) { /* faithful text, cached training MSE and expression */ }
```

These are caller-provided data, not a target recognized by the library. More data,
a larger vocabulary or a different procedure may be needed; a bounded run may not
find a useful fit. No particular expression or convergence is guaranteed.

## Input order, evaluation and cache validity

x0 binds to inputs[0], x1 to inputs[1], through x[d-1] to inputs[d-1]. The first row
establishes dimensions; every later row must match. Rows must be nonempty as a
dataset, and all inputs/targets finite. Strict evaluation requires exactly d finite
bindings, even when a particular expression uses fewer variables. Typed variable
indices >=d are construction/API errors. No single-variable or polynomial assumption
exists in loss, initialization, mutation, crossover or constant fitting. Strict
evaluation checks all d bindings, so loss work scales as O(rows*(d+nodes)); larger
arity also requires sufficient expression limits to represent useful combinations.

Initialization and newly generated mutation subtrees sample uniformly over all
indices [0,d). Subtree replacement/exchange copies typed nodes without renaming
variables. Different dimensions cannot be crossed. Text preserves x-index identity;
column-name mapping belongs to the caller/application, not the expression value.
Fitting changes only independent constant occurrences using the same immutable
training rows. It cannot add a missing variable or alter expression topology.

RegressionProblem copies rows and nested input vectors and exposes no mutable data.
Changing data, selected input order, targets or limits requires a new problem/run.
Even numerically equal separately constructed problems have different identities:
a population rejects a candidate from another problem before evaluation. An old
candidate's cached score still belongs to its original problem. To score its
expression on a separate problem with the same dimension count, call that problem's
measure (fresh evaluation), or create a new candidate/run. Do not transfer cached
fitness across problems. Separate equivalent problems can reproducibly run the
same search when data order, configuration, seed, toolchain/build and serial strict
FP environment match; no cross-toolchain distribution equivalence is promised.

The existing final fitness wrapper, checked full-data MSE, finite/invalid states,
exact-MSE/nodes/text ordering, private staging and no-throw generation publication
are unchanged. There is no second engine or regression implementation. Invalid
mathematical evaluations remain ordinary invalid fitness. Operational exceptions
retain their nested causes; aborts preserve committed state and consume attempted
work/RNG. See [CONSTANT_FITTING.md](CONSTANT_FITTING.md) for local versus global
budgets, fresh baselines and caller-retained objective diagnostics on exceptions.

## Searching from a file

From the repository root, after building Release:

```sh
../build-release/genetik_multivariate_demo --generate ../multivariate-data
../build-release/genetik_regression_search ../multivariate-data/training.csv \
  --inputs first,second --target target --seed 42 --loss-budget 100000 \
  --evaluate ../multivariate-data/interior.csv --evaluate ../multivariate-data/exterior.csv
```

Replace the files and column names with your own numerical table. `--inputs`
order is explicit: `--inputs second,first` makes x0=second, x1=first. Reordering
physical file columns alone does not change bindings; each file resolves names
independently. Selected names are case-sensitive. Surrounding spaces/tabs in
header names and CLI selections are trimmed. Empty/repeated inputs, unknown names
and selecting the target as an input are errors. All column names in a file must
be unique after trimming. Every evaluation file must contain those same selected names and target name; its
physical column order may differ. Extra columns are allowed but must also be numeric.

Required options: inputs, target, seed (unsigned decimal uint32), loss-budget
(unsigned decimal, 0..10,000,000). Optional `--operators add,sub,mul,div,log` opts into strict division/log (default add,sub,mul); empty, repeated or unknown names fail before search. The printed operator policy is uniform over structurally feasible entries. Optional `--no-fit` disables refinement; at most
eight repeated `--evaluate FILE` options supply evaluation sets. `--help` shows the
small interface. Duplicate singleton/unknown options and malformed numbers fail.
Exit status: 0 for a completed bounded run with a candidate, 2 for a bounded stop
without a committed candidate, 1 for input/operational errors. Numerical recovery
is not required for status 0. A very small fitting allowance can expire during
initialization, leaving no candidate; this is reported without a restart.

Defaults printed by the program: 64 slots, generation cap 2,000, nodes 31, depth 6
(leaf=1), constants bounded by 8; linear ranks, two elites, crossover .9, mutation
.35, initial depth cap 4, mutation cap 3, half full/grow, grow .65, eight variation
proposals, +,-,* and every selected variable. Constant terminals: half discrete
{-2,-1,0,1,2}, half uniform [-8,8). Fitting is on by default for every private slot,
including elites/clones: bounds [-8,8], step 1 down to 1e-8, 256 local loss calls,
128 sweeps, 8,192 proposals, threshold 1e-14. Run threshold is training MSE <=1e-14;
reported recovery independently uses max error <=1e-6. Outer request and actual
training-loss limits both use the supplied budget; these are distinct counters.

Output includes column mappings and one-based file column positions, target,
seed/configuration, raw and refined initialization MSE, selected faithful expression,
cached training MSE, complexity, discovery generation, attempts/publications, actual
loss/row/fitting/variation work, unused allowance, search runtime and stop reason.
Missing candidate losses are printed as -1 (MSE is nonnegative). Training max-error
reporting performs predictions only, without an uncounted MSE recomputation.

All training and supplied evaluation files are fully validated before search_start.
Only training rows enter RegressionProblem/TrainingObjective used by search.
Evaluation paths/data do not enter fitting, selection, configuration or stopping.
After selection_finalized, each evaluation set has one checked loss calculation
with the frozen expression against its own immutable problem. Its MSE/max error
and attempted loss rows/call are reported separately. An additional full prediction
scan reports total invalid predictions/errors and its own row count; totals are
never inferred from the early-stopping MSE. Invalid loss arithmetic is
explicit, with no partial favorable metric. These reporting calls do not consume
training allowance or change the selected candidate. Search runtime excludes file
loading and post-search metrics. No serialization/second-process model loading is
provided in this milestone.

## Deliberately limited table format

The loader is application support in examples/RegressionData.hpp, outside the public
library. It has no mandatory dependency beyond the standard library.

- First nonblank physical line is the header; subsequent nonblank lines are numeric
  rows of exactly that width. Blank lines contain only spaces/tabs (after removing
  one terminal CR) and are ignored; physical row numbers still include them.
- Commas delimit fields. Trim ASCII spaces/tabs at each field's edges. LF and CRLF
  are accepted, as is a final line without a newline. One terminal CR is removed;
  embedded CR/control characters are rejected. No comments or quoted fields:
  any double quote is rejected rather than interpreted. BOM/Unicode headers are
  unsupported; names must be printable ASCII, nonempty, <=128 bytes.
- Locale-independent binary64 decimal/scientific syntax, including .5, 2., -0,
  +1.25 and 1e-3. Parsing must consume the complete trimmed field. Hexadecimal,
  NaN/infinities, missing values, trailing junk and values outside representable
  finite binary64 range (including underflow reported out of range by from_chars)
  are errors. Representable subnormals are allowed. There is no output clamping.
- Finite application resource limits per file: 16 MiB including line terminators,
  16,384 bytes per physical line excluding LF but including a terminal CR, 50,000
  numeric rows, 64 columns (at most 63 selected inputs plus a distinct target),
  128 bytes per trimmed header name and 256 per trimmed numeric field. Up to eight
  evaluation files. Limits are inclusive. General library rows have no such file
  limits; their resource policy belongs to the caller.

The loader validates all fields, including unselected columns, reads through EOF,
and returns only a complete dataset. Missing/read-failing files, invalid limits,
headers/selection, widths or numbers produce clear errors; file-content diagnostics
include filename and one-based physical row/column. Selection errors identify the
filename/name; CLI option errors identify the option. No silently accepted prefix
is used to start a search.

## Reproducible demonstration and verification

`genetik_multivariate_demo --generate DIR` writes training.csv, interior.csv and
exterior.csv with first,second,target headers and max_digits10 decimal precision
under the classic locale, sufficient to round-trip the computed binary64 values.
Targets live only in the fixture generator. Row ordering is first input ascending
outer loop, second input ascending inner loop. Full data definitions and frozen
campaign results are in [MULTIVARIATE_CAMPAIGN.md](MULTIVARIATE_CAMPAIGN.md).
Running `genetik_multivariate_demo` without arguments uses in-memory rows and the
same public symbolicRegression path as the file executable.

Engineering tests independently check 2/3-input binding (including x2), dimensions,
invalid inputs/indices, higher-index initialization/mutation, subtree identities,
three-input constant fitting and separate-problem cache rules. File tests check
format/limits/errors, custom comma locale, generated binary64 round trips, physical
column reordering, and exact same-seed parsed/in-memory search results, counters,
trace and RNG state. CLI smoke tests ensure malformed training/evaluation files
fail before search and evaluation paths cannot change the chosen expression.
Downstream C++20 code uses only public headers/target to search three-input rows.
Existing publication/ownership/fault-injection suites remain applicable unchanged.

Current restrictions: one scalar output, fixed immutable dimensions/objective per
run, +,-,* only, strict binary64/FP assumptions, bounded heuristic structure search
and coordinate fitting (which can stall on coupled/irrelevant parameters). No
categorical/missing-value handling, normalization, parser for exported expressions,
serialization, cancellation or threading. Strict ordinary division and natural logarithm are now supported; see [STRICT_ARITHMETIC.md](STRICT_ARITHMETIC.md) for domains, operator selection and full-precision files. Prime regression is the next example
using its preserved protocol. Profile representative CPU workloads before GPU work
and complete packaging/platform/resume production requirements in the roadmap.

The additive `--loss-threshold MSE` option sets the run and per-fit training
thresholds together; default 1e-14 is preserved, and zero requests exact-zero stops
only. Values must be finite and nonnegative. The [prime example](PRIME_REGRESSION.md)
uses this option to reproduce its normalized one-input experiment through this same
file executable, with identical numerical input and search counters.
