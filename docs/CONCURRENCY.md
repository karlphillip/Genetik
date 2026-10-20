# Generic candidate execution (opt-in CPU backend)

Genetik now has a library-owned reusable worker pool. Serial remains the default.
Version 0.1.0 remains unreleased. No genetic representation, MSE, expression grammar,
prime or file-format knowledge is present in Execution.hpp or the generic dispatch.
See CONCURRENCY_RESULTS.md for executed qualification/performance evidence.

## Public configuration and ownership

```cpp
#include <SymbolicRegression.hpp>
auto problem=RegressionProblem::create({{{0},1},{{1},3},{{2},7}}, {31,6,8});
SymbolicRegressionConfig config;
config.population=8; config.elites=1; config.maxGenerationAttempts=4;
config.maxEvaluations=40; config.maxTrainingLossAttempts=512;
config.fitting=ConstantFitConfig{};
config.fitting->maxLossAttempts=8;
RandomContext run(42, ExecutionConfig{4}); // seed and explicitly owned execution context
const auto result=symbolicRegression(problem,config,run);
if(result.best) { /* result.best->text(), matching result.best->fitness() */ }
```

The same RandomContext constructor works with Population<C> / GeneticAlgorithm
for scalar and application-owned non-scalar candidates. ExecutionConfig::threads
is 1..256; ExecutionConfig::parse accepts an entire unsigned decimal token, rejecting
empty, zero, signs, overflow, junk and values above 256. N=1 constructs no workers
and Population uses the original serial call path. N>1 uses dedicated workers;
the caller coordinates and does not evaluate candidates. Thus at most N Genetik
candidate evaluation/refinement tasks overlap, counting the caller when it does
participate (serial). Effective concurrency is min(requested, batch/population size),
a task ceiling, **not** measured CPU utilization or guaranteed overlap. peakTasks()
records observed simultaneously active executor tasks in the most recent batch.
No automatic CPU-count mode, CPU pinning, affinity, or bound on callback-created
threads is implied. Multiple independent runs require independent mutable run/RNG
state; their task ceilings are separate. Campaign seeds still execute sequentially.

RandomContext owns its ExecutionContext, whose pool starts lazily and is reused
across generations. Alternatively construct ExecutionContext execution({4}) and
RandomContext(seed, execution); the explicitly borrowed context must outlive the
run and can be reused sequentially. Neither object is copyable/movable. Pool size
can grow up to the requested ceiling if a later batch is larger. A bounded range
of stable slots replaces individual queued job objects. There is no unbounded job
queue or per-loss/per-candidate thread creation. Header-only delivery still has no
architecture-specific installed binary; ARCH_INDEPENDENT package matching remains
appropriate, without qualifying untested platforms.

Only one external coordinator may access a running engine/context/population.
No concurrent evolve(), RNG/member/stat/snapshot access or destruction while an
operation is active. Only ExecutionContext::requestStop()/stopRequested() are
concurrently callable control methods. Internal workers do not make other public
methods thread-safe. run() is synchronous and rejects reentrancy; it drains all
started tasks before returning/throwing. All task references remain lexical; no
worker is detached. Destruction joins idle workers. Partial startup failure joins
already-created workers and makes that context unavailable for further execution.
The protected launchWorker seam permits platform launch customization/fault testing;
its returned thread must be joinable and execute the supplied body exactly once.

## Callback capability and preparation

Existing serial subclasses need no changes. The base EvaluatedCandidate defaults
to parallelEvaluationSafe()==false. A custom candidate opts in by overriding it
only when its full virtual prepareEvaluation()/evaluate() path is concurrently
callable, including captured/shared state, or after arranging independent task-local
services. Const or shallow callable copies do not prove safety. Initialization and
variation run with the coordinator, including virtual roulette/crossover/mutation;
workers invoke the final calculateFitness wrapper, which retains virtual preparation
and fitness dispatch. Private staged candidates own their genomes and mutable caches.

The coordinator checks capabilities before any candidate callback starts. Optional
beginParallelEvaluation(ExecutionContext&) detaches mutable services, with strong
exception safety, before dispatch. finishParallelEvaluation() runs for all successfully
detached slots after draining, even on failure; it must restore service bindings,
including if accounting merge throws. These hooks do no fitness evaluation/RNG work.
Detached snapshots do not grant concurrent use of shared mutable preparation services;
bind genuinely independent services before evaluating them alongside a run.
No unrestricted genome/cache access was added. Custom replacement must still validate
an owned value and invalidate fitness. Custom candidate copies must own independent
genomes; shared immutable problem data is allowed.

ExpressionCandidate opts in for its exact built-in type and independent preparation.
Unknown subclasses must explicitly qualify their overrides. RegressionPreparation
has parallelEvaluationSafe, forkParallel and mergeParallel seams. Factories must
return fresh exclusively owned instances, not null/the original/shared cached
objects. ConstantRefinement forks one private fitter and TrainingObjective per
candidate, preserving the immutable problem, settings, occurrence order, cadence
and local bounds. Only a small global loss-admission object is shared. Coordinator
merges all work and preparation statistics by slot, then restores original service
bindings before publication/archive copies. Accepted fitted constants remain in the
published candidate and are inherited by later offspring; no optimized flag/cache.

TrainingObjective subclasses opt in explicitly and override protected
cloneForParallel() to return a fresh same-problem/same-allowance objective. Virtual
compute() dispatch is retained. Factories must independently own mutable evaluator
state or use genuinely concurrently callable shared state. mergeWorker() is an
optional coordinator-only no-throw custom accounting hook. The built-in objective
and refiner support parallelism; detailed RefinementDiagnostics are explicitly
unsupported with N>1 (rejected before initialization). Cadence campaigns skip the
serial-only diagnostic profiles when N>1 and label that omission; their unprofiled
search runs use the real backend. This is not whole-callback serialization.

## Numerical and repeatability contracts

All construction, selection/variation and RNG access stay on the coordinator.
Candidates are assigned stable slots before evaluation. Only candidate evaluation
and optional fitting run on workers. Postfix traversal, operand grouping, native
arithmetic/domain checks, per-row residual/MSE accumulation and sequential coordinate
search decisions are unchanged. Worker scratch and visited/proposal state are private.
Each worker task restores the coordinator's captured FP environment; parallel calls
require FE_TONEAREST. The existing default-rounding/gradual-underflow/nontrapping/
qualified-libm contract still applies. Callbacks must not depend on sticky FP flags
or change the coordinator's environment. No cross-toolchain/libm bit guarantee.

Default/explicit-one preserves qualified serial results, counters, initialization,
traces, stop and RNG state. With deterministic independent callbacks and nonbinding
limits, parallel completed generations match candidate values/fitness bits, ordering,
complexity ties, committed best/history, fitting decisions, work and RNG. Arrival
order is never a ranking/tie-breaker. Mathematical invalidity is an ordinary candidate
result, does not cancel siblings, and all-invalid attempts retain NoViableCandidates.
Fitting generation zero and prospective committed-generation cadence are unchanged;
failed attempts do not advance the schedule.

## Admission, cancellation and failure

Outer requests count when calculateFitness is actually invoked. Workers mark stable
slots; the coordinator applies that count to RandomContext after draining. Queued,
unstarted slots are not requests. RNG/counters are not safe to inspect from workers.
Loss attempts start at TrainingObjective::measure admission, before compute and
workspace allocation. A mutex-protected check-and-charge rejects exhausted/stopped
requests without reserving future permits. Each granted start proceeds and is
counted, including invalid results, allocations and throwing computations. Shared
allowances cannot be exceeded. Local worker work counts are folded exactly once;
fit baseline/probe subsets are not double-counted. Rows count actual visited rows.
Private local fit limits retain progress; a global denial throws, never shortens a
fit into a publishable result. Counter-fold overflow is diagnosed operationally
rather than wrapping; an unrepresentable diagnostic counter saturates on that error.

requestStop is cooperative for an active preparation batch, not a sticky engine-wide
cancellation token. It stops new candidate admission; running arbitrary callbacks
must finish. The allocation-free serial path checks between candidates (and at the end of preparation); parallel built-in refinement also checks before beginning each loss. No attempt
to interrupt a blocking callback, coordinator construction/variation, or an active loss
row loop is promised. New run() resets the batch stop state. Cancellation returns
SearchStop::Cancelled from the symbolic driver (including no best if initialization
never committed); generic population/algorithm calls propagate EvaluationCancelled
with normal context. All admitted work is drained, counted and discarded on failure.

On any operational callback failure the executor stops admission, signals cooperative
stop and drains. All worker exceptions remain accessible by slot in failures() until
the next accepted batch. Lowest-slot operational exception wins, ahead of any budget
or cancellation; otherwise lowest-slot budget exception wins ahead of cancellation.
Nested candidate/training contexts retain the original cause. Parallel training-loss
attempt numbers in nested messages are task-local; the slot identifies the task.
A coordinator completion failure is propagated ahead of control stops (all completion
hooks still run); an existing worker operational failure stays primary. Startup,
batch allocation/callable-copy and detach failures propagate without expired task
references; submission allocation occurs before dispatch, not during per-job enqueue.
External callback side effects cannot be undone.

Population, genome/fitness, generation and best-seen state publish together only after
all preparation, validation, ordering and archive construction succeed. Failure or
global exhaustion preserves the previous committed state; initialization can leave
none. Already consumed work/draws remain consumed; no implicit retry. At a binding
limit/failure/cancellation parallel task admission, discarded work and participating
callbacks can differ from serial. No serial-identical boundary/RNG promise is made
there. An unpublished private improvement cannot enter committed best/history.

## Applications and delivery

Evolution modes accept --threads N: character demo (old seed/generation positional
arguments retained), polynomial/multivariate/strict arithmetic/prime campaigns,
constant-fitting/cadence campaigns, generic numerical-file search, compiled
quickstarts and installed permutation consumer. Pure expression demos/generation/
standalone prime pilot do not implement population parallelism; data/pilot modes
reject non-one requests. Configuration reports requested/effective task ceilings.

```sh
../build-release/genetik_demo 42 20 --threads 4
../build-release/genetik_regression_search training.csv --inputs first,second --target target --seed 42 --loss-budget 10000 --threads 4
../build-release/genetik_prime_regression --campaign --output out/prime-parallel --loss-budget 100000 --threads 4
../oop-consumer/permutation_consumer --threads 3
```

Genetik::Genetik carries Threads::Threads; GenetikConfig uses find_dependency(Threads)
before loading exports. Consumers add no manual thread flags. C++20/strict-FP
requirements, private development flags, casing/include layout and CMake >=3.16
remain unchanged. TSan uses a separate development build with
-DGENETIK_THREAD_SANITIZER=ON; combining it with GENETIK_SANITIZERS is rejected.
Hosted concurrent checks and the separate ARM64 TSan step have now executed
successfully for the combinations/revision in HOSTED_QUALIFICATION.md. That record
distinguishes actual support from untested compilers/platforms and arbitrary callbacks.
