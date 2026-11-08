# Joint multi-component expression templates

`MultiComponentTemplateRegression.hpp` extends the existing scalar-template
search to a frozen expression scaffold with two distinct, disjoint subtree
occurrences. Both components are generated and varied independently, then
materialized into one complete `Expression`. Fitness, optional fitting and
generation publication always evaluate that complete expression against the
original regression objective.

## Example model

For `f(x,y) = sin(x) + y^2`, provide the scaffold `x0+x1` and designate its two
leaf occurrences as sites 0 and 1. The search receives observations, not the
reference components. Site 0 can be restricted to variable `x0` and `Sin`; site
1 can independently be restricted to variable `x1` and `Multiply`. An ordinary
expression evaluator still evaluates the returned model.

```cpp
const auto source = parseExpression("x0+x1", 2, {}, modelLimits);
MultiScalarExpressionTemplate scaffold(source, 0, 1, componentLimits, modelLimits);
auto domain = MultiComponentTemplateProblem::create(
    scaffold, problem, {{0}, {1}},
    {{ExpressionKind::Sin}, {ExpressionKind::Multiply}});
auto result = multiComponentTemplateRegression(domain, {site0Grammar, site1Grammar},
                                               config, random);
```

`examples/quickstarts/multi_component_templates.cpp` is the complete installed
consumer example. It independently checks the discovered model on assessment
inputs. `benchmarks/multi_component_template_comparison.cpp` compares two fixed
seeds against ordinary symbolic regression.

The bounded run uses the same 20 training rows, three held-out rows, population
size, initial seed, candidate-call cap (1,600), and row-attempt cap (32,000)
for both strategies. The template search stops on exact training fit after its
64-member initial population. Ordinary search returns a 7-node approximate
model on seed 142857 at training/held-out MSE `0.13342`/`0.16133`; it finds a
6-node exact-training model on seed 271828, but that model does not structurally
match the reference scaffold components. The template method recovered the
6-node scaffold structure and scored zero training and held-out MSE for both
seeds, using 64 candidate calls and 1,280 row attempts each. This narrow
demonstration illustrates the value of a known scaffold for this example; two
seeds do not establish general superiority over ordinary symbolic regression.

## Occurrences and limits

Site identifiers refer to the two root occurrences supplied at construction,
not to expression kinds or values. The source roots must define disjoint
subtrees; ancestor/descendant pairs are rejected. Every other source node and
its left/right operand order is frozen. Matching uses expression-tree
occurrences, so repeated equal subexpressions remain distinguishable.

Each site has its own `ExpressionLimits`, allowed full-schema variable indices,
operator set and `ExpressionGrammar`. The complete model is also checked against
the supplied model limits for node count, depth, constant magnitude and
arithmetic operations. `maximumMaterializationBytes` bounds a conservative
node/workspace estimate used while assembling models; it is not a process RSS
limit. Initialization and variation attempts are bounded by the grammars'
proposal limits. A complete model that exceeds a joint limit is rejected as a
bounded structural proposal.

The regular `SymbolicRegressionConfig` controls population size, rates,
generation/candidate/loss budgets, training threshold, cancellation context,
and optional constant fitting. Per-site grammars are supplied separately;
`config.grammar` does not override them. Search requires the prediction-loss
strategy because template search currently delegates to the ordinary regression
ordering policy. Configurations asking for complexity-penalty or Pareto
strategies are rejected rather than silently ignored.

Optional constant fitting sees constants at both substitution occurrences and
optimizes the complete model objective. The existing fitting-method
compatibility rules remain in force. It never fits frozen scaffold constants.

## Determinism and limitations

Initialization, mutation and crossover consume only the run-owned random
engine. The two components are independently restricted and jointly scored;
this does not create a second GA or change ordinary symbolic-regression
initialization, selection, RNG use, checkpoint identity or defaults. Candidate
calls and training-row work use the existing symbolic-regression counters.
Variation statistics include complete-model initialization and each attempted
site mutation/crossover.

Template search is appropriate when the outer mathematical composition is
known. It does not infer decomposition, move scaffold nodes, or establish that
the supplied scaffold is uniquely implied by finite observations. A finite
training or assessment match is not proof of universal identity. Component
grammars define the searched family; a correct component excluded by those
restrictions cannot be recovered.

The bounded comparison harness uses equal training data, seed, population,
candidate-call and training-loss budgets for template and ordinary search. The
ordinary grammar is broader because it can evolve the entire expression; the
template grammar fixes the outer addition and reserves one variable per site.
Scores and structural matches are reported independently. A small fixed-seed
comparison is an integration exercise, not evidence of general superiority.
