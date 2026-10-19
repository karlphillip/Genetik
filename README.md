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

See [baseline and audit](docs/BASELINE.md) for verified commands, corrected defects, compatibility details and remaining limitations. The [roadmap](docs/ROADMAP.md) defines the later real-valued, expression, polynomial and prime-sequence milestones. Symbolic regression and prime regression are not implemented yet.
