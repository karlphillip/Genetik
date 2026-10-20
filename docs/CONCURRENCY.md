# Next milestone: generic bounded candidate execution

Status: design requirements, **not implemented**. No --threads option exists yet.
Keep the current serial path as reference/default. Concurrency belongs to the generic
Population/algorithm preparation execution layer, not symbolic or prime code.

Introduce one consistently exposed execution option, with demos eventually accepting
`--threads N`. Default 1. N is a ceiling on simultaneous Genetik candidate evaluation/
refinement tasks, including the caller if it participates. Report requested and
effective concurrency. This is not physical-core placement, affinity, a guarantee
of N active threads, or a limit on threads created inside user callbacks.

The coordinator initially owns RNG access and selection/variation order, task
admission, stable result collection, best-seen updates and publication. Each task
exclusively owns its mutable candidate, fitting state and temporary workspace;
immutable objective data may be shared. Current shared ConstantRefinement and
TrainingObjective contain mutable statistics/budget state: do not invoke them from
workers concurrently unchanged. Define either explicitly concurrently callable
user implementations with synchronized accounting, or genuinely independent worker-
local evaluator/refiner instances (including their captured state), before enabling
workers. Copying a shared_ptr or shallow-copying a callback is not independence.

A running engine has one external owner: unless separately documented, no concurrent
public reads/mutations, RNG access, stats queries or detached-snapshot requests while
an operation runs. Internal workers do not make all public APIs thread-safe. Define
callback/reference lifetime through completion/draining; prohibit reentrant changes
to the same run. Worker/task/context lifetimes must end before borrowed data dies.

Specify admission for outer candidate requests, actual loss probes and dataset rows.
Existing counter increments happen before attempts and allocation; use atomic or
coordinator-mediated reservations so fitting cannot exceed a shared cap. Define
what happens when an admitted fitter needs another loss slot: local progress may
be retained privately, global exhaustion aborts the attempt. Cancellation is
cooperative at declared boundaries, cannot interrupt blocking user callbacks, and
must drain/join all admitted tasks before returning or destroying their state.
Record all actually attempted work, including invalid/throwing/discarded tasks;
never refund consumed RNG or work. Select/report operational failure in a stable
order with original nested causes. Do not publish partial results or best-seen
values from an abandoned generation. Publication remains a no-throw ownership swap
only after all required candidate evaluation/ranking/archive work succeeds.

Audit speculative variation/RNG and admitted/discarded work at failure and budget
boundaries before promising serial-identical results: pre-generating all offspring
or parallel loss reservation may change where a serial run stops and how much work
is consumed. State separate successful fixed-work equivalence and failure-boundary
guarantees; do not claim whole-run rollback. Define retry semantics against the
same prospective committed generation/cadence without rolling back consumed draws.

Acceptance: serial-reference comparisons for scalar, permutation and symbolic
candidates; controlled interleavings; concurrent callback/worker-local ownership
fixtures; admission/global/local budgets; cancellation/draining; faults during
partial work and no partial publication; deterministic result collection; TSan
qualification on a supported host plus ASan/UBSan; bounded representative CPU
measurements with requested/effective tasks and actual work. Keep full stochastic
campaigns separate from correctness. Evaluate overhead at N=1 and scaling before
changing defaults. Carry platform threading requirements on Genetik's own target
(e.g. Threads::Threads when needed), not in demos only. If delivery gains compiled
architecture-specific binaries, revisit ARCH_INDEPENDENT package matching and
installation/export qualification. Hosted compiler/platform support remains pending
until actual CI execution; a design record is no qualification evidence.
