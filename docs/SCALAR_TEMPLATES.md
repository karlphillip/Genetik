# One-site scalar expression templates (Phase 6A)

A caller can fix an owned model scaffold and evolve one unknown scalar expression
through the existing symbolic-regression engine. This is a structural constraint,
not a different loss, evaluator or evolutionary loop. The generic GA/OOP interfaces
remain independent of templates. Include `<ScalarExpressionTemplate.hpp>` for
composition, or `<ScalarTemplateRegression.hpp>` for constrained search/fitting.
Neither header requires typed mathematics or a third-party dependency.

## Construction and public usage

Select one **postfix subtree root occurrence** in an ordinary validated expression.
The selected subtree is removed; the prefix and suffix are copied into an immutable
`ScalarExpressionTemplate`. It is not identified by a special constant, variable or
text substitution. Its removed contents are never injected into initialization.
There is exactly one insertion interval, used once; no unresolved placeholder is
executable by `Expression`.

```cpp
#include <ScalarTemplateRegression.hpp>
#include <ExpressionParsing.hpp>

// Full ordered input schema: x0=v, x1=T. Root occurrence 1 is this x1 terminal.
auto source = parseExpression("((-1.0)*x1)*x0", 2);
ScalarExpressionTemplate scaffold(source, 1, {9,4,8}, {13,6,8});
auto data = RegressionProblem::create(
    {{{1,0},-1}, {{2,1},-4}, {{0,2},0}}, {13,6,8});
auto problem = ScalarTemplateProblem::create(scaffold, data, {1},
    {ExpressionKind::Add, ExpressionKind::Subtract, ExpressionKind::Multiply});

auto supplied = parseExpression("x1*x1+1", 2); // independent demonstration
Expression model = problem->materialize(supplied);
// model is (((-1.0) * ((x1 * x1) + 1.0)) * x0)

SymbolicRegressionConfig cfg;
cfg.population=8; cfg.elites=1; cfg.maxEvaluations=32;
cfg.maxTrainingLossAttempts=128; cfg.maxGenerationAttempts=3;
cfg.fitting=ConstantFitConfig{}; cfg.fitting->maxLossAttempts=4;
RandomContext random(42, ExecutionConfig{2});
auto result = scalarTemplateRegression(problem, cfg, random);
if (result.best) {
    Expression evolved = problem->substitution(result.best->expression());
    // result.best->text() faithfully exports the complete ordinary model.
}
```

The supplied expression is **not** a search seed. The installed compiled
[quickstart](../examples/quickstarts/templates.cpp) additionally evaluates the model,
rejects `x0-x0`, reparses its complete export and reports the stopping outcome.
Acceptance of that program does not depend on exact stochastic discovery.

```sh
cmake -S . -B build/library -DGENETIK_BUILD_TESTS=OFF -DBUILD_TESTING=OFF -DGENETIK_BUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX="$PWD/out/install/local"
cmake --install build/library
cmake -S examples/quickstarts -B out/consumers/quickstarts -DCMAKE_PREFIX_PATH="$PWD/out/install/local"
cmake --build out/consumers/quickstarts -j2
out/consumers/quickstarts/templates_quickstart --threads 2
```

## Restriction and ownership contracts

`ScalarTemplateProblem` binds the immutable scaffold, the original immutable full
`RegressionProblem`, explicit allowed full-schema variable indices, and allowed
operator kinds. Empty variable/operator sets are permitted when terminals suffice.
Indices are never remapped: `{1}` means original `x1`, not local `x0`. Duplicate or
out-of-schema indices and duplicate/unsupported operators are configuration errors.
Terminals remain controlled by `ExpressionGrammar`; its enabled operators must be
a subset of the allowed set. Scientific operations remain opt-in. Powi metadata
retains its existing supported range; grammar exponent subsets control generation.

Restrictions are checked on every supplied or recovered substitution, candidate
edit, initialization, mutation, crossover and installation after preparation. Even
`x0-x0` or `0*x0` is forbidden when x0 is excluded. The frozen scaffold may use those
same variables/operators. Immutable `ScalarTemplateContext` validates exact frozen
fields before candidate replacement. `ExpressionCandidate::constrained` is the
optional mathematical context boundary; it does not reopen mutable storage.

Copies own all expression nodes. The scaffold is copy-constructible, deliberately
nonassignable, and stores no source-subtree or text aliases. Materialization returns
an ordinary owning `Expression`; extraction returns a detached expression. Candidate
copies/clones, committed parents and best-seen snapshots cannot mutate each other.
A successful edit invalidates fitness; a failed edit preserves expression and score.
Context identity includes the specific template/problem/preparation instance, so
same-shaped datasets or identical substitutions do not permit cross-context fitness
reuse. Use a new run for changed problems. Evaluation-only scoring never edits the
training candidate or its cached fitness.

## Initialization, variation and limits

Random construction reuses the existing bounded iterative generator. It selects
only permitted variable indices, retaining their original values. No target-specific
templates or mathematical-validity retries are involved. Mutation replaces a random
subtree inside the evolving expression. Crossover exchanges its subtrees between
compatible candidates. Both validate complete replacements. Structural rejection
uses the configured positive finite `proposalLimit`; exhaustion returns unchanged
owned clones. Allocation or other operational exceptions are not ordinary proposal
rejections. Statistics record proposals, rejections, fallbacks and actual changes.

Apply both `substitutionLimits` and `modelLimits`. The source must itself be a valid
model under the latter. `effectiveSubstitutionLimits()` deducts frozen nodes,
ancestor depth and frozen arithmetic cost from full-model limits and intersects
with local limits. All constant bounds are enforced; a frozen constant need not fit
the *local* constant bound. Depth counts nodes along the longest root-to-leaf path
(terminals depth 1). For this scaffold four nodes and two ancestor levels are added:
local 9 nodes/depth 4 permits complete 13 nodes/depth 6. Limits are checked at the
complete destination, not just inside the replacement. Native function calls retain
existing one-unit arithmetic accounting; Powi charges its ordered multiplications.

`maxMaterializationBytes` defaults to 1 MiB. A conservative per-full-node allowance
of `4*sizeof(ExpressionNode)+4*sizeof(size_t)` bounds simultaneous node/metadata
storage used by composition/validation; integer division is checked before size
addition or allocation. Source construction also meets this allowance. It is not a
cap on allocator overhead, persistent candidates, export strings or process memory.
Expression node limits bound those other representations. The effective node cap
also respects this allowance. Configuration errors use `invalid_argument`, invalid
selected roots use `out_of_range`, and structural/resource limits use `length_error`.
`bad_alloc` and unexpected failures remain operational. No partially composed value
is installed. Traversals are iterative; there is no new recursion or executor.

## Fitting and full-model loss

Search always scores the **complete model against the original rows**, with the
existing checked MSE, row order and strict interpreter. It never divides targets by
v or transforms them into targets for g. A zero scaffold factor cannot conceal an
invalid child (for example `log(-1)`). Mathematical invalidity remains Invalid
fitness, without favorable prefix scores or regeneration.

`fitScalarTemplateConstants(*problem, substitution, objective, config)` materializes
and returns the ordinary `ConstantFitResult` containing the **full** accepted model
and its matching loss. The objective must belong to that exact original problem.
Extract its hole with `problem->substitution(result.expression)`.
Only constant occurrences inside the inserted postfix interval are adjustable, in
increasing postfix order. Frozen coefficients retain their exact bits, even when
outside the fitting interval. Independent equal constants stay independent; Powi
exponents are not parameters. Local bounds apply to adjustable coefficients, and
all full expression bounds still apply.

The reusable additive `fitSelectedConstants` entry accepts increasing distinct
constant node indices. Its default counterpart `fitConstants` is unchanged. Both
use the same coordinate kernel: positive/negative probes, strict finite improvement,
retained incumbent on ties, local-limit accepted progress, invalid-probe rejection
and no randomness. `ConstantParameterSelection` optionally supplies immutable
selection to `ConstantRefinement`; parallel use requires explicit capability.
Existing no-fit mode, initialization/elite/clone treatment and `fitEveryGenerations`
cadence are preserved. The independently weighted fixture finds c≈2.6 with full
MSE=1.6, whereas incorrectly fitting divided observations would find c=2.

## Work, publication and concurrency

Materialization/validation consumes no loss attempt and no RNG draws. Every actual
full-model objective start, including a baseline, invalid probe or throwing call,
consumes one existing loss attempt. Row work counts visited original rows. Local
fit limits retain accepted work; a binding global allowance discards incomplete
private generation work. There is no additional composition/objective cache.

The shared driver delegates selection/variation/publication to the existing engine;
full-model MSE, then full node count, then faithful full text order selection,
elites and best-seen values consistently. Hole complexity is reported separately.
No hole-only complexity ordering or second evolution loop exists.

Run-owned `ExecutionConfig{N}` and `--threads N` retain their existing meaning.
Coordinator owns RNG and structural work. Workers share immutable scaffold/data and
own candidate/refinement/objective scratch; no nested pool is introduced. Built-in
preparation is explicitly parallel-qualified; custom services need their existing
capability/factory contract. Detailed refinement diagnostics remain serial-only.
Cancellation, draining, original exception causes, operational-over-budget priority,
actual consumed work and staged publication are unchanged. Failed attempts preserve
committed population, fitness, generation and best-seen state. Same-environment
nonbinding serial/parallel comparisons include full models, work/history and RNG;
binding-limit trajectories retain the existing qualified concurrency scope.

## Evidence and scope

Run the fixed no-fit discovery campaign once per new output directory:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j2
build/release/genetik_scalar_template_campaign --output out/runs/templates/my-campaign --threads 1
# Bounded integration smoke, not the research protocol:
build/release/genetik_scalar_template_campaign --output out/runs/templates/my-smoke --smoke --threads 2
```

[All five outcomes and separate fitting evidence](SCALAR_TEMPLATE_RESULTS.md) and
[executed qualification](SCALAR_TEMPLATE_QUALIFICATION.md) are recorded separately.
Repeated holes, multiple holes, shared parameters, typed templates, units, general
domain constraints, template text syntax/serialization and formula-seeded search
remain outside this phase. Canonical complete exports use the existing parser with
sufficient limits; no algebraic rewrite or global-equivalence claim is implied.
