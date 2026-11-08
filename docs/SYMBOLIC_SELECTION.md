# Optional symbolic-regression selection strategies

Genetik's ordinary symbolic-regression selection remains the default. Set
`SymbolicRegressionConfig::selectionStrategy` to opt into a complexity penalty
or Pareto search. These strategies reuse the same expression grammar, candidate
evaluation, constant fitting, variation, candidate-call budgets, and run-owned
`RandomContext` as ordinary symbolic regression.

## Complexity penalty

`SelectionStrategy::ComplexityPenalty` orders candidates by

\[
S(e)=L(e)+\lambda C(e),
\]

where `L` is the selected training objective, `lambda` is the finite nonnegative
`complexityPenalty`, and `C` is the expression's postfix node count (every
terminal and operator counts one). Ties compare prediction loss, node count,
then canonical expression text. The stored candidate fitness and all reported
training metrics remain the unpenalized prediction objective. The selected
penalty score is available as `bestSelectionScore`.
The score is computed in `long double`; if the platform cannot represent the
sum, it saturates at the largest finite `long double` and deterministic
prediction-loss/complexity tie breaks apply.

The penalty is soft. A larger, more accurate expression remains eligible and is
preferred whenever its loss reduction exceeds its added penalty. This strategy
does not impose a new hard complexity cutoff; the existing `ExpressionLimits`
still bound generated expressions.

## Pareto search

`SelectionStrategy::Pareto` minimizes the pair `(training loss, node count)`.
Candidate A dominates B only if A is no worse in both values and strictly
better in at least one. The engine performs nondominated sorting, computes
standard normalized crowding distance within each front, and assigns parent
roulette weights from front rank and crowding. Each completed generation uses
environmental survival from the combined committed parents and newly evaluated
offspring. Thus Pareto ranks affect both parent selection and which candidates
remain in the population.

The public generic GA remains unchanged for ordinary use. Pareto mode opts into
a narrow population-ordering extension; it does not replace the candidate,
evaluation, variation, or publication services. Repeated expression text is
removed when filling survivor slots where unique structures are available.
If fewer unique expressions exist than population slots, duplicates fill the
remaining slots deterministically. The final `paretoFrontier` is the first
nondominated front of the last committed population, deduplicated by canonical
expression text and bounded by `maximumParetoFrontier`. When truncation is
needed, crowding distance preserves spread, with deterministic tie breaks.

`ExpressionCandidate::fitness()` and `SymbolicParetoPoint::predictionLoss`
always report prediction loss. `structuralComplexity` is node count. The
frontier is selected from training observations only; held-out observations
must be assessed separately and never affect search or frontier membership.

## Limits and determinism

Pareto mode accepts:

- `maximumParetoComparisons`: maximum total dominance, rank-order, crowding,
  and duplicate-check comparisons across the run.
- `maximumParetoEstimatedMemoryBytes`: estimate for the combined
  parent/offspring pool and nondominated-sorting workspace. This is not a
  process-RSS cap.
- `maximumParetoFrontier`: maximum returned unique expressions.

Exhaustion returns `SearchStop::SelectionResourceLimit`; the previous committed
population remains intact. Work reports split dominance and other selection
comparisons and include peak estimated selection memory. Candidate evaluation
and training-loss work remain in their existing counters. Vector capacities,
allocator metadata, and implementation-specific temporary allocations are
approximated; this policy is not a hard process-memory guarantee. The estimate
covers selection-owned structures and expression copies, not the caller's
dataset or unrelated process memory.

Enumeration and tie breaking are stable. Pareto sorting consumes no random
draws. The default `PredictionLoss` path retains the existing ordering, random
draw sequence, checkpoint identity, and stopping behavior. Optional modes do
not change ordinary GA defaults.

The strategy assumes deterministic candidate evaluation for a fixed immutable
problem. The existing concurrency contract still applies: coordinator-owned
variation and selection are serial, while only qualified candidate evaluation
may be parallel. Seeded serial and parallel searches must agree on ordering and
published results.

## Example

`examples/quickstarts/selection_strategies.cpp` is a standalone installed
consumer. It supplies observations and a grammar but does not seed the fitter
with the generating expression:

```cpp
config.selectionStrategy = SymbolicRegressionConfig::SelectionStrategy::Pareto;
config.maximumParetoFrontier = 8;
const auto result = symbolicRegression(problem, config, random);
for (const auto& point : result.paretoFrontier)
    std::cout << point.candidate.text() << " mse=" << point.predictionLoss
              << " nodes=" << point.structuralComplexity << '\n';
```

## Frozen comparison cases

`benchmarks/symbolic_selection_comparison.cpp` is a deterministic, opt-in
measurement harness, not a CTest success criterion. It compares the default
loss ordering, a node-count penalty (`lambda=0.01`), and Pareto search under the
same seed (`104729`), operator grammar (`+`, `-`, `*`), population (48),
generation limit (24), candidate-call budget (1200), copied training rows,
and expression limits. Constant fitting is disabled in all configurations.
The four frozen cases cover exact and fixed-noise affine and quadratic
equations. Independent assessment points include extrapolation outside the
training interval. Noise vectors, equations, and points are defined directly
in the harness. The held-out values are not supplied to the search.

The harness reports the selected training expression and loss, held-out MSE,
node count, candidate and row work, generations, Pareto comparisons, and
elapsed time. Results are finite-sample comparisons, not a guarantee that one
selection strategy dominates another. Pareto selection has additional sorting
work; it is intended for consumers who want an accuracy-complexity tradeoff,
not as a claim of better prediction on every task.

One qualification run on the development machine found exact training recovery
for both clean cases under all three strategies. On the noisy cases, all
strategies exhausted 1,200 candidate calls and produced training MSE
`0.00231111` and independent assessment MSE `0.00068`. The chosen noisy
quadratic penalty model used seven nodes while the loss-only and Pareto choices
used five; the prediction values matched on those finite rows. This does not
prove structural equivalence. Pareto consumed 73,052 comparisons on the exact
linear case, 56,080 on exact quadratic, and about 384,000 on each noisy case.
Peak estimated selection memory ranged from 262,894 to 273,058 bytes in these
cases; it is not an RSS measurement. The non-Pareto modes report zero Pareto
comparisons. Candidate/row budgets were identical; exact runs stopped early
after satisfying the exact-training-fit criterion. These outcomes show the
intended tradeoff and additional work, not a general recovery advantage. The
complete comparison output is recorded in
[the qualification report](SYMBOLIC_SELECTION_QUALIFICATION.md); timings are
single-machine observations, not portable performance claims.

Run after building examples:

```sh
./build/release/genetik_symbolic_selection_comparison
```

No MDL or probabilistic complexity claim is made. Node count is a transparent
structural proxy, not a universal description-length measure.
