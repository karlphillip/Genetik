# Interactive exact rounded-prefix search (application only)

`genetik_prime_regression --search-exact` is separate from the frozen historical
normalized-MSE campaign. Existing --generate/--pilot/--campaign forms, budget
restrictions and defaults are unchanged. The current observability/checkpoint extension adds explicit fresh-state restore
seams without changing ordinary evolution, objectives or an observer. The demo controls one
existing `GeneticAlgorithm::evolve()` at a time and inspects the existing const
`Population::selectionView()` only after synchronous preparation/publication drains.

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON
cmake --build out/build/release --target genetik_prime_regression -j2
out/build/release/genetik_prime_regression --search-exact --seed 42 --threads 4 --loss-budget 1000000 --report-seconds 30
```

This mode defaults to seed 42, one worker, 1,000,000 actual loss attempts, 30 seconds
and the single current best. Use --top N for compact additional candidates and
--report-format raw for the previous dense diagnostic form (raw defaults to top5). It retains population 64, 2,000 generation ceiling, 31 nodes/depth6,
+,-,*,/,log grammar, the existing mixed discrete/uniform constants and deterministic
coordinate fitting (including initialization/elites/clones). No known nth-prime
approximation or target expression is injected. --fit-every retains the positive
cadence convention; --no-fit explicitly disables optional refinement. --threads uses
the existing 1..256 execution limit, not affinity. No run is unbounded by default.

The existing sieve, independent trial division and exact anchor/prefix validation
are reused. Inputs are x0=double(n)/1000.0, normalized targets double(p_n)/10000.0,
n=1..1000; the faithful standalone predictor is 10000.0*g(double(n)/1000.0).
Optional --prime-file validates the supplied exact prefix before randomness/search.
--output DIR explicitly exports the existing reference datasets; without it, this
mode writes only console output. Redirect logs into out/runs/ or out/logs/ yourself.
The examined 1001..1200 range is neither training nor assessment for this mode.

## Objective and rounding

Use the existing `rounded` rule: native std::round once, halfway away from zero,
with checked int64 representability (-2^63 inclusive,2^63 exclusive). A predictor
has only expression nodes, constants and x0; it cannot access the reference, test
primality or repair outputs. The reference belongs only to immutable objective data.

Each wrong rounded prediction contributes 1 + (error/(1+error))/1000, where
error=abs(10000*g(x0)-p_n). Correct rounded predictions contribute zero even when
raw errors are nonzero. Implementation visits ascending n, counts mismatches as
integers, checks and sums wrong-error ratios in that order, divides the secondary
sum by 1000, then adds the mismatch count. Bounds on at most 1,000 terms keep any
one-fewer-mismatch individual strictly better; secondary error orders equal counts.
A large error's ratio may round to 1, which still cannot reverse mismatch priority.

Fitness zero iff every required rounded prediction matches. Stop immediately on
that committed zero, including initialization. Invalid child arithmetic, nonfinite
decoding, unrepresentable integers or checked loss arithmetic produce Invalid
fitness, never finite penalties. Search loss may stop at the first invalid row;
it never accepts a successful prefix as a full score. Each complete objective
request/probe is one loss attempt, with attempted rows separately charged. The
existing coordinate fitter scores this same full objective on every probe.

## Snapshots, ordering and work

- Always emit an initial snapshot after initialization commits and a final snapshot
  for budget/generation/no-viable/cancelled outcomes. A winning final snapshot is
  immediate on zero loss, independent of the timer. If initialization cannot commit,
  emit a final empty snapshot (generation=none), not a fictitious initial population.
- Use steady_clock and deadlines aligned with search start. Initial preparation
  counts in elapsed time. Report at the first successful publication at/after a
  deadline; a long generation can delay a 30-second report. Catch up to the next
  future aligned deadline without a burst. Formatting time does not reset the phase.
- No reporter thread, no live-state inspection while workers run. The coordinator
  reads the committed population only; failed/unpublished preparation never appears.
- Use the same Minimize/RegressionOrdering score, nodes, faithful-text rules as
  selection/archive. Equivalent display ties retain committed slot order, independent
  of worker completion. Do not deduplicate identical formulas; label duplicates.
- Top counts valid **individuals**. Report invalid/unevaluated counts and clamp to
  available valid population members. The fixed population 64 bounds expensive
  reporting even for a very large --top request. Only displayed expressions are
  evaluated, at most 64*1000 predictions/snapshot (default 5*1000).
- Diagnostics use prediction-only existing decode/round/metrics code outside the
  objective. They scan all requested rows and report raw RMSE/MAE/max and relative
  errors, matches/mismatches, duplicate rounded outputs, increase violations and
  invalid/unrepresentable predictions. Counters `reporting_prediction_rows` and
  `reporting_loss_attempts=0` are separate from training work. No fitting, RNG,
  candidate/cache edits, best-seen updates or training budget are used for display.
- Flush each complete snapshot. Output/allocation failure is operational: publication
  already happened and is not rolled back because console formatting later failed.

Snapshot headers include elapsed time, committed generation, attempted/published
subsequent generations, candidate requests, actual loss work/allowance/remaining,
unpublished loss attempts, seed/population and requested/effective task ceilings.
Each rank includes its exact fitness, 1000-denominator matches, raw diagnostics,
node/depth/constant complexity, canonical g(x0), and decoded formula with explicit
x0 binding. Reporting time is wall time and can affect when the next snapshot is
seen; it cannot change scoring, RNG or search work within a nonbinding run.

--report-seconds accepts finite positive fractional decimal seconds through 86,400,
with at least one representable clock tick. --top is a positive size_t integer;
--loss-budget is a positive uint64 integer; --seed is a uint32 (including 0). Signs,
overflow, trailing junk, repeated/unknown options and missing values fail before
search. Use --search-exact --help for the actual syntax.

## Cancellation and limits

Ctrl+C and SIGTERM set a lock-free atomic flag in a signal-safe handler. The coordinator and
per-row objective checks observe it; workers request existing cooperative batch
cancellation and all admitted work drains. No mutex, formatting, allocation or
ExecutionContext::requestStop call occurs inside the signal handler. The previous
handler is restored on exit. No extra worker/reporter thread is created. Normal
cancellation and budget exhaustion exit successfully as bounded outcomes, explicitly
labeled separately from exact recovery. Operational exceptions exit unsuccessfully
with existing nested cause reporting. Global exhaustion retains the last committed
population/best, consumed RNG/work and unpublished-work accounting. Detailed fitting
diagnostics remain separate/serial-only; this mode does not enable them.

The objective and console helpers are in examples/PrimeExactSearch.hpp and
PrimeExactOptions.hpp, not installed library headers. Mathematical partials, typed
APIs and nonmathematical OOP consumers gain no prime dependency.

> Reproducing all first 1,000 supplied primes would demonstrate an exact fit to that finite prefix. It would not establish a general closed-form formula for all prime numbers.

This is heuristic search. Rounding creates plateaus and coordinate fitting can miss
useful coefficients; no exact recovery, global optimum or extrapolation is promised.
Historical campaign results (including misses), published tags/assets and the
approximately fourfold fixed-work benchmark retain their original measured scope.

The current human formatter, JSONL sink and durable checkpoint/resume APIs are
documented in [RUN_CHECKPOINTING.md](RUN_CHECKPOINTING.md). The original smoke and
qualification record describes the preceding dense-output milestone unchanged.
