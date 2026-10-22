# Architecture and external OOP extension contract

Audited from source at delivery checkpoint 8b633264, then qualified by the OOP
milestone. This is a header-only library; there is no compiled implementation in
need of a src directory. Public include spellings and installed include/Genetik
remain unchanged. See [API](API.md) and [packaging](PACKAGING.md).

## Responsibilities and dependencies

| Layer / source | Responsibility / extension boundary |
| --- | --- |
| [Fitness.hpp](../genetik/Fitness.hpp), [EvaluatedCandidate.hpp](../genetik/EvaluatedCandidate.hpp) | Finite Valid scores versus Unevaluated/Invalid; common controlled cache lifecycle. Abstract evaluate(), stats(); virtual prepareEvaluation() and problemIdentity(). No representation, regression or IO dependency. |
| [Creature_i.hpp](../genetik/Creature_i.hpp), [Creature.hpp](../genetik/Creature.hpp) | Owned arithmetic scalar genomes with final validated edits; protected virtual mutatedGene(). Creature is the retained concrete character-demo implementation, not the generic candidate base. |
| [Population_i.hpp](../genetik/Population_i.hpp), [Population.hpp](../genetik/Population.hpp) | Abstract lifecycle/snapshot/edit/fitness interface and value-owned concrete staging, evaluation, ranking, replacement, committed archive and no-throw publication. FitnessOrdering<T> policy is numeric by default. |
| [GeneticAlgorithm_i.hpp](../genetik/GeneticAlgorithm_i.hpp), [GeneticAlgorithm.hpp](../genetik/GeneticAlgorithm.hpp) | Abstract evolve/evolutions/roulette/crossover/mutation; concrete serial bounded generation orchestration. All three behavior hooks dispatch virtually during evolve. Representation-specific Variation policy implements crossover/mutation; optional role hooks are duck-typed and have no expression types in their signatures. |
| [RandomContext.hpp](../genetik/RandomContext.hpp), [ScalarVariation.hpp](../genetik/ScalarVariation.hpp) | Run-owned seed/engine/candidate-attempt accounting; default scalar variation using the borrowed engine. |
| [Expression.hpp](../genetik/Expression.hpp), [ExpressionVariation.hpp](../genetik/ExpressionVariation.hpp) | Typed owned postfix expressions, strict interpreter/export, bounded random initialization and subtree variation. |
| [Regression.hpp](../genetik/Regression.hpp) | Immutable copied rows, checked whole-data MSE, ExpressionCandidate, runtime-polymorphic RegressionPreparation, expression-only RegressionOrdering (loss, nodes, text). |
| [ConstantFitting.hpp](../genetik/ConstantFitting.hpp), [SymbolicRegression.hpp](../genetik/SymbolicRegression.hpp) | Deterministic coordinate fitting, runtime TrainingObjective::compute extension, actual-loss/row budgets and optional cadence; bounded symbolic driver composes the same Population/GA. |
| Application sources and support headers | File parsing, column mapping, sequence/reference generation, scaling, least squares baselines, diagnostics and reporting. Not installed library dependencies. |

All library layers depend only on C++20 standard headers and the compiler-selected
standard/runtime math libraries. Native binary64 arithmetic and std::log implement
expression math. Genetik implements checked MSE and bounded coordinate search itself.
The prime example implements its sieve, diagnostics and small QR least-squares
baselines; these are not generic library facilities. No Eigen, BLAS, Boost, CUDA,
OpenMP, external optimizer or mandatory mathematical package is present.
CMake is delivery tooling. Python 3.9+ is test/measurement/script tooling, not a
public dependency; optional plotting uses matplotlib outside C++ runtime usage.
The tests use their own runtime checks, not a third-party C++ testing framework.
MIT license/attribution remain unchanged.

## Application-defined candidates and algorithms

Derive a concrete value type from EvaluatedCandidate for a non-scalar genome.
Implement evaluate() and stats(); optionally override prepareEvaluation() for
private refinement and problemIdentity() for immutable objective binding. Do not
use Creature_i to encode a different representation. calculateFitness() is final:
it clears fitness, calls the preparation hook, rejects Unevaluated, and installs
its returned Fitness. Preparation must return the score of exactly the installed
candidate against its fixed objective. Custom implementations are trusted to honor
this contract, just as evaluators are trusted to compute their declared objective.

Own and validate your genome; expose no mutable aliases. Complete throwing work
before replacement/invalidation, and provide independent value copies and no-throw
destruction. Copy/assignment must preserve matching genome and cache; a potentially
throwing representation should use transactional copy/swap. An immutable shared
objective may be borrowed through an owned shared_ptr. Null problemIdentity is
permitted for simple fixed objectives; explicit identities detect mixed problems.
Externally changing an objective requires a new run, not reuse of old fitness.

Initialize a vector of concrete candidate values with the run-owned engine and
construct Population<Candidate> from it. A constructor accepting mt19937& also
supports size-based initialization. Supply a representation-specific Variation
policy to GeneticAlgorithm<P,C,V>. Runtime subclasses may override roulette,
crossover, mutation or evolve through GeneticAlgorithm_i. Population_i has virtual
setSize/creatures/mean/bestCreature/setCreature/updateValues/stats. Replacing the
whole algorithm/population implementation additionally requires the concrete
orchestration protocol (randomContext, ready/generation/publication, etc.); those
abstract interfaces alone are not a representation-erasure engine or clone factory.
Concrete values are copied with their declared template type: do not store derived
candidates by base value and expect polymorphic cloning.

FitnessOrdering is a replaceable template policy used consistently for selection
weights, elites and archive; non-expression candidates need no node/depth members.
Replacement is concrete Population::publishGeneration, not a separate virtual
replacement-policy object. Its preparation hook is generic and virtual on the
candidate. Optional setPreparationGeneration receives the prospective committed
index; it does not mention constants, MSE or regression. RegressionPreparation is
only the specialization's expression/fitness pair adapter. There is no second
transaction engine and no expression/MSE/CSV/prime branch in core control flow.
Core includes do not pull in symbolic headers; Population_i now includes only its
representation-independent candidate dependency rather than Creature_i.

### Repaired selection dispatch

The audit found evolve called private select directly, bypassing the public virtual
roulette extension. It now calls roulette, as it already did crossover and mutation.
Default roulette uses Population::selectionView(), a const committed view valid
only until the next successful publication/destruction. Do not retain it across
engine operations; creatures() remains the detached owned-snapshot API. Custom
population types without this optional view keep the snapshot fallback. Default
selection arithmetic, draws and ordering are unchanged; subclasses overriding
roulette now actually influence parent selection (intentional behavior repair).
Custom selection must return an evaluated valid candidate of the same fixed problem.

## Installed non-regression qualification

[tests/oop_consumer](../tests/oop_consumer) is a complete external application.
Its four-job permutation is owned as an array, not a scalar encoding; a virtual
application objective computes weighted completion time. Initialization shuffles
with RandomContext; order crossover and swap mutation preserve permutations.
A runtime algorithm subclass intercepts selection/crossover/mutation through a
GeneticAlgorithm_i reference. It runs five bounded generations, never requiring
stochastic discovery of the optimum. Known costs 35 and 85, detached input/copies,
failed edits, invalidation, operator invariants, actual dispatch, nested causes,
failed variation/evaluation, all-invalid initial/successor populations and consumed
RNG/work are checked. No symbolic-regression header is included or required.

After installation (see README):

```sh
cmake -S tests/oop_consumer -B ../oop-consumer -DCMAKE_PREFIX_PATH="$PWD/../genetik-prefix"
cmake --build ../oop-consumer -j2
../oop-consumer/permutation_consumer
```

The ordinary/relocated installed fixture also builds/runs this source after deleting
its disposable producer trees, alongside multi-TU scalar/symbolic consumers. The
active checkout is never removed. Package and support policy remain unchanged.

## Execution now (supersedes the serial-only audit finding)

The OOP checkpoint had no runtime concurrency. The subsequent concurrency milestone
adds generic ExecutionContext dispatch in Population::publishGeneration. Default
one retains caller-thread preparation; opt-in workers evaluate/refine private
candidate slots. Coordinator RNG/selection/variation/ordering/publication and all
virtual algorithm hooks remain intact. Worker-local expression preparations fold
statistics after draining. Threads::Threads is now a public platform dependency;
there is still no third-party mathematical dependency or installed compiled binary.
See [CONCURRENCY.md](CONCURRENCY.md) for callback qualification, execution ownership,
limits and failure contracts, and CONCURRENCY_RESULTS.md for executed evidence.

## Explicit run-state persistence

RunCheckpoint.hpp contains generic owned state snapshots, fresh-object restoration,
codec customization, bounded field/envelope encoding and POSIX atomic persistence;
it includes no symbolic-regression headers. ScalarExpressionCheckpoint.hpp provides
the first mathematical codec and objective/refinement accounting adapters. Candidate
codecs are trusted cache-restoration boundaries after immutable context validation,
not writable genome access. Arbitrary subclasses and custom virtual-hook state need
explicit codecs/adapters. See RUN_CHECKPOINTING.md for ownership, idle-access and
qualified continuation contracts. ExpressionFormatting.hpp is separate presentation.
