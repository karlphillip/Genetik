# Genetik
*Genetik* is an open-source framework that facilitates the implementation and testing of Genetic Algorithms in C++. 

---

## Introduction

The core of the framework are abstract classes that define the API that needs to be implemented in order to extend and incorporate new behavior or add completely new types of creatures and fitness functions. This guarantees compatibility with the rest of the framework and speed-up development because of code reuse. 

The names of the abstracts classes always end with an `_i`, as in: `GeneticAlgorithm_i`, `Population_i` and `Creature_i`. The framework also provides default implementations for each of them.

---

## Example

A sample application `demo.cpp` is available and demonstrates how to instantiate and configure those objects in order to set up and run a genetic algorithm pipeline to solves a simple character-based search problem:

```
* Generation #1 avg fitness: 0.040 best fitness: 0.041
  str=[ aw*xv^pBk&ZKeQW\|-'%]hGP[X>BEp\.9"3L}Ff tTHgh9AQ';.?IDVaw(Sg_YzWVs<?l 1bek ]  f=0.041 fP=0.000 rouletteRange=(0.010, 100.000)
...
...

* Generation #100 avg fitness: 0.641 best fitness: 0.649
  str=[ 'NeveBSu!dZrestiS(,eHUudroid'I- Gen[ral>)e!W drga:a, jhePRkQe 4f +7yYalker ]  f=0.649 fP=0.000 rouletteRange=(0.010, 100.000)
...
...

* Generation #200 avg fitness: 0.881 best fitness: 0.892
  str=[ 'Never+uoderestim.te a|droid' - General }eiW Organa, The Rize af Skywalker ]  f=0.892 fP=0.000 rouletteRange=(0.010, 100.000)
...
...

* Generation #327 avg fitness: 0.974 best fitness: 1.000
  str=[ 'Never underestimate a droid' - General Leia Organa, The Rise of Skywalker ]  f=1.000 fP=0.000 rouletteRange=(0.010, 100.000)
...
...
```

---

## Build and verification

C++20 and CMake 3.16 or later are required. The library uses only the C++ standard library and is MIT licensed. Link the header-only `genetik_lib` CMake target to inherit includes and the language requirement.

```sh
cmake -S . -B ../build-release -DCMAKE_BUILD_TYPE=Release
cmake --build ../build-release -j2
ctest --test-dir ../build-release --output-on-failure
../build-release/genetik_demo 42 1000
```

The example accepts `[seed [max_generations]]`, defaulting to 42 and 1,000. It reports configuration, best seen candidate, elapsed time and stopping reason; reaching the generation limit is a normal result. The historical output above illustrates the original implementation, not a current search-quality guarantee. On the supplied Jetson, CMake is installed locally as `../.tools/bin/cmake` (and `../.tools/bin/ctest`).

For AddressSanitizer and UndefinedBehaviorSanitizer, configure a separate Debug directory with `-DGENETIK_SANITIZERS=ON`. Tests contain runtime checks that remain active in Release; no test requires eventual stochastic target discovery.

## Explicit randomness and migration

```cpp
#include <Creature.hpp>
#include <Population.hpp>
#include <GeneticAlgorithm.hpp>

RandomContext run(42); // must outlive the population and algorithm
Population<Creature<char>> population(100, run);
GeneticAlgorithm<decltype(population), Creature<char>> ga(0.5f, 0.9f, run);
const auto outcome = ga.evolve(population); // check Committed vs NoViableCandidates; caller owns termination
```

Initialization, resizing, selection, crossover and mutation consume this run's engine. There is no implicit random-device seed. Custom creatures accept `std::mt19937&` for initialization and mutation; direct character-creature construction/mutation also needs an engine. This is a source compatibility change from the original API. Fitness has explicit unevaluated, valid and invalid states. Valid scores are finite doubles, including zero and negatives. Maximization is the default; pass `ObjectiveDirection::Minimize` to the population for minimization. Roulette uses linear ranks with averaged ties and excludes invalid candidates. Custom scalar candidates now derive from `Creature_i` and implement `Fitness evaluate() const` and `mutatedGene(...) const`; final edit methods enforce cache invalidation.

`setCreature()` stages edits; `updateValues()` evaluates and publishes them. A generation publishes genomes, fitness, generation number and best seen together only after evaluation and preparation succeed. All-invalid attempts return `NoViableCandidates`. Unexpected exceptions abort the attempt and retain their original cause as a nested exception. Consumed random draws and attempted evaluations remain consumed; `run.evaluationAttempts()` includes invalid and throwing evaluations. Objective/data must remain fixed during a run. See the [current fitness/publication contract](docs/BASELINE.md#fitness-validity-and-generation-publication-milestone) for migration and edge cases. Repeatability is supported with the same build/toolchain, input, configuration, seed, call order and deterministic serial evaluation.

See [baseline and audit](docs/BASELINE.md) for verified commands, corrected defects, compatibility details and remaining limitations. The [roadmap](docs/ROADMAP.md) defines the later real-valued, expression, polynomial and prime-sequence milestones. Manually constructed expression evaluation is available below. Bounded expression structure evolution is available below; optional bounded continuous constant fitting is also available; prime regression remains future work.


## Mathematical expression evaluation

```cpp
#include <Expression.hpp>
#include <array>

const auto x = Expression::variable(0);
const auto square = Expression::binary(ExpressionKind::Multiply, x, x);
const auto f = Expression::binary(ExpressionKind::Add,
    Expression::binary(ExpressionKind::Add, square, x), Expression::constant(1, 1));
const auto result = f.evaluate(std::array{2.});
// f.toString() == "(((x0 * x0) + x0) + 1.0)"; result.value() == 7
```

Run `../build-release/genetik_expression_demo` for the manually constructed polynomial, its node count/depth, the five supplied input/output examples and explicit handling of arithmetic overflow. This demonstrates representation/evaluation; it performs no evolution or fitting.

Expressions own typed postfix nodes, enforce a single complete tree and configurable node/depth/constant bounds, and expose no mutable internal storage. Mathematical non-finite arithmetic returns an invalid `ExpressionResult`; malformed construction, wrong/non-finite bindings and allocation failures are exceptions. Text export preserves grouping and stored constant values independently of locale. Default limits are 1,023 nodes, depth 64 (leaf depth 1), and any finite constant.

The public CMake target propagates `-fno-fast-math -ffp-contract=off` on GCC/Clang because evaluation is in a public header. Consumers must retain strict binary64 semantics, default rounding and gradual underflow, including when building without CMake. See [expression contracts and integration plan](docs/EXPRESSIONS.md) for the full API, floating-point assumptions, work bounds, faithful export and the bridge now used by structure evolution.


## Polynomial symbolic regression

```cpp
#include <SymbolicRegression.hpp>

const auto problem = RegressionProblem::create({
    {{-1.0}, 1.0}, {{0.0}, 1.0}, {{1.0}, 3.0}
});
RandomContext run(0);
SymbolicRegressionConfig config;
const auto result = symbolicRegression(problem, config, run);
// result.best (if present) owns the discovered expression and its training fitness.
```

Run `../build-release/genetik_polynomial_regression` for the prescribed five-seed search on 17 training rows, followed by separate frozen interior/exterior evaluations. This evolves typed expression structure through the existing engine, with checked MSE, minimization, node-count/text ties, two reevaluated elites, finite subtree proposals and generation-boundary evaluation budgets. All five initial campaign seeds had zero maximum error on the three specified sample sets; two discovered fits during random initialization. These measured fits are separate from deterministic engineering tests and do not claim algebraic equivalence or recovery on arbitrary problems.

See [symbolic-regression API and contracts](docs/SYMBOLIC_REGRESSION.md) and the [complete campaign report](docs/POLYNOMIAL_CAMPAIGN.md), including actual expressions, discovery generations, counters, errors and runtimes. All supplied rows are copied into an immutable problem; changing it requires a new run. The scalar/character example remains supported.

## Continuous constant fitting

Include `ConstantFitting.hpp` to refine a fixed expression with `fitConstants(expression,
TrainingObjective&, ConstantFitConfig)`. Enable it in the shared search with
`config.fitting = ConstantFitConfig{};` and set `config.maxTrainingLossAttempts` for
the aggregate training-loss budget. The fitter makes no random draws, changes only
constant occurrences, and returns its accepted expression with matching fitness.
See [API and failure/budget contracts](docs/CONSTANT_FITTING.md) and the
[paired five-seed comparison](docs/CONSTANT_FITTING_CAMPAIGN.md).
Run `../build-release/genetik_constant_campaign` for that bounded comparison;
`genetik_polynomial_regression` retains the earlier discrete-constant example.

## Search an equation from numerical data

The public regression API supports ordered multi-input rows and one scalar target.
The small file example accepts an unquoted comma-delimited numerical table:

```sh
../build-release/genetik_multivariate_demo --generate ../multivariate-data
../build-release/genetik_regression_search ../multivariate-data/training.csv \
  --inputs first,second --target target --seed 42 --loss-budget 100000 \
  --evaluate ../multivariate-data/interior.csv --evaluate ../multivariate-data/exterior.csv
```

Input order defines x0,x1,... independently of file column positions. Constant
fitting is optional (`--no-fit`). Only training data enter search; evaluation
files are scored after selecting the final expression. See the [generic API,
file format and complete workflow](docs/MULTIVARIATE.md), and the [five-seed
multivariate campaign](docs/MULTIVARIATE_CAMPAIGN.md). Prime regression remains a
later example of the general module, after strict division/logarithm support.
