# Current operational migration

For proposed0.3.0, see [0.3 migration](releases/0.3.0/MIGRATION.md).
The0.2 migration record below retains its historical scope.

# Migrating to the 0.2.0 development API

The original repository declared 0.1.0 but had no published release history.
This modernization is intentionally not source-compatible with its original
interfaces. There is no compatibility promise for intermediate untagged revisions.
Once released, the [pre-1.0 policy](SUPPORT.md) applies within each minor series.
The names below are the actual public API; CMake's Genetik:: namespace is not a
C++ namespace. Existing header spelling/casing remains unchanged.

## From published 0.1.0 to 0.2.0

0.2.0 is an unreleased new minor series. Change explicit installed package requests
from `find_package(Genetik 0.1 CONFIG REQUIRED)` to `find_package(Genetik 0.2 CONFIG REQUIRED)`
(or omit a version deliberately). SameMinorVersion intentionally rejects a 0.1
request for an installed 0.2 package; patches within 0.2 preserve documented source
API, while compatible defect fixes may change numerical results with release notes.

Recompile **all** dependent translation units consistently, including application
libraries, when changing these headers/options. RegressionProblem now has a virtual
measurement/sample-count interface and destructor; protected objective extension
hooks support complete-model measurements. Header-only does not imply stable object
layout, vtables, or binary ABI. Existing ordinary constructors, public includes,
`Genetik::Genetik`, source `genetik_lib`, scalar defaults and generic OOP hooks remain.
No cross-version seeded trajectory is promised. Exact compatibility comparisons
are qualified by revision, toolchain/runtime, inputs, configuration and execution.

All typed/parsing/template/calculus headers are optional separate includes; generic
OOP consumers still need none of them. Scalar templates fit inserted constants
against complete-model loss; observation regression uses its weighted normalized
channels; integral regression scores complete numerical integrals. These objectives
are not interchangeable fitness contexts. See [API index](API.md),
[0.2 notes](releases/0.2.0/NOTES.md) and the individual contracts before migrating.

Fresh builds use `build/`, installed prefixes `out/install/`, consumers
`out/consumers/`. Historical runs now reside in sibling
`../Genetik-data/research-runs/`; on this workspace `out/runs/` is the approved
compatibility symlink to that location. New durable datasets, research runs,
experimental evidence and benchmark results belong in sibling `../Genetik-data/`;
benchmark executables stay under `build/benchmarks/`. Setting `GENETIK_DATA_ROOT`
to the Genetik-data root opts supported default-output applications into
`$GENETIK_DATA_ROOT/research-runs/`; when unset, their historical `out/runs/`
defaults remain unchanged. Explicit output arguments continue to take precedence.
Campaign sources moved to
`apps/campaigns/` while executable names/explicit output options are preserved.
Historical commands and evidence intentionally keep their original paths.
The 2026-10-31 migration used a same-filesystem directory rename after recording
file inventory, sizes, symlinks and representative SHA-256 checksums. Its
preflight record is in `Genetik-data/experimental-evidence/`; it is provenance on
the same device, not an independent backup. The two `/dev/full` links are retained
as fault-injection fixtures.

## Build and includes

Require C++20 and CMake >=3.16. Link `Genetik::Genetik` through installed
`find_package(Genetik 0.2 CONFIG REQUIRED)` or add_subdirectory. The source-only
`genetik_lib` compatibility target remains; installed consumers use the canonical
target. Include `<Creature_i.hpp>` explicitly for scalar subclasses; it is no
longer pulled in transitively by Population_i.hpp. The package supplies C++20,
strict C++ floating-point options and Threads::Threads. Those FP options affect
the consuming translation units, not only the interpreter. Development warnings,
sanitizers and profiling flags do not propagate. See [packaging](PACKAGING.md).

## Runs, candidates and fitness

Declare one `RandomContext run(seed)` before the population and algorithm that
borrow it. Pass the same context to both. Custom scalar initialization accepts
`std::mt19937&`; `mutatedGene(unsigned int, std::mt19937&) const` returns a proposed
gene. Use `run.engine()` for coordinator initialization/variation, never hidden
reseeding. The compiled [scalar quickstart](../examples/quickstarts/scalar.cpp)
shows the complete signatures and a bounded loop.

Derive scalar values from Creature_i<T>; its arithmetic genome and fitness cache
are private. Use `gene`, `setGene`, `mutateGene`, `replaceChromosome` and detached
`chromosome()` values. There are no writable aliases or overridable invariant-
bypassing edits. For a different representation derive from EvaluatedCandidate,
own/validate the genome, and invalidate fitness only after a replacement succeeds.
Provide independent value copies; storing derived objects by base value slices
rather than cloning. The [permutation consumer](../tests/oop_consumer/main.cpp)
and [OOP guide](ARCHITECTURE.md) demonstrate this route.

Override `Fitness evaluate() const`, returning `Fitness::valid(value)` or
`Fitness::fromScore(value)` for admissible finite results, and `Fitness::invalid()`
for expected mathematical invalidity. Zero and negative scores are valid. Set
ObjectiveDirection::Minimize/Maximize separately. Unevaluated is distinct from
Invalid; a genome change requires a new matching evaluation. The optional virtual
`prepareEvaluation()` must return the score for the exact prepared candidate.
Do not translate arbitrary exceptions into bad fitness or transplant cached scores
between changed genomes/problems. Objective/input data must remain fixed per run.

## Algorithms and publication

Public virtual roulette/crossover/mutation/evolve hooks remain supported; evolve
now honors overridden roulette (the original implementation bypassed it).
Representation-specific Variation policies complement those runtime hooks.
Custom population implementations must also satisfy the concrete coordinator
protocol; the abstract classes are not a heterogeneous type-erased engine.
Expression complexity ordering is confined to RegressionOrdering.

`creatures()` returns detached owned snapshots. `setCreature()` stages an edit;
`updateValues()` or `publishGeneration()` prepares and commits a complete viable
population. Inspect GenerationOutcome and bound loops explicitly. Selection and
current-best queries reject pending edits; only evaluated valid candidates can be
parents/best. Do not rely on zero-score sentinels, silent out-of-range edits, or
stochastic discovery eventually terminating a run. Roulette ranges are percentages
in [0,100].

Failures preserve committed population, matching fitness, generation and best-seen
state, while attempted work and consumed RNG draws stay consumed. No implicit
retry/restart occurs. All-invalid populations report NoViableCandidates; initial
failure may leave no best. Operational failures retain nested causes. External
callback side effects cannot be rolled back. Generic callers own their stopping
policy; symbolic search additionally exposes actual-loss budgets and stop reasons.

## Optional parallel execution

`RandomContext run(seed, ExecutionConfig{N})` or application `--threads N` opts into
the real candidate worker backend. Default 1 needs no worker pool. Coordinator RNG,
virtual selection/variation, ordering and publication remain serial. N caps Genetik
evaluation/refinement tasks, not affinity, cores, or callback-created threads.

Existing serial subclasses need no change. Custom parallel callbacks must explicitly
qualify `parallelEvaluationSafe()` and arrange independent mutable services where
needed. Preparation factories and coordinator merge hooks are described in
[CONCURRENCY.md](CONCURRENCY.md); const/shallow copying alone does not prove safety.
Detailed RefinementDiagnostics remains serial-only and incompatible requests throw
before initialization. Only documented stop controls may be called concurrently
with a running engine; do not concurrently inspect/mutate/destroy it.

Same-seed results are qualified by exact revision/toolchain/runtime/inputs/settings
and execution mode. Deterministic independent callbacks with nonbinding limits
preserve completed results across tested worker counts; budget/failure/cancellation
boundaries can change speculative work/RNG. No cross-libm or cross-stdlib identity
is promised. See [fitness/API contracts](API.md) and [support](SUPPORT.md).

## Additive one-site scalar templates (Phase 6A)

Existing scalar, typed and generic OOP callers require no migration. Opt in with
`ScalarTemplateRegression.hpp`; `ScalarTemplateProblem` retains original variable
indices and binds a specific scaffold/data/preparation context. Its candidate stores
the complete model, not an unresolved hole. Extract the evolving part with
`problem->substitution(candidate.expression())`. `fitScalarTemplateConstants` also
returns the complete accepted model/score; only the inserted constants are selected.
The ordinary `symbolicRegression`, all-constant fitter, grammar defaults and execution
controls retain their behavior. See [SCALAR_TEMPLATES.md](SCALAR_TEMPLATES.md).
