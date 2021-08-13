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

## Todo

- Add more interesting examples;
- Add timing info for measuring runtime;
- Move to Google Test/Mock for Unit Testing;
- Add support to concurrency: divide the Population into smaller groups of Creatures so they can be processed by threads;
- Add support to persistence: it should be possible to resume an evolutionary process interrupted by CTRL+C;
- Define the `GeneticOperator` interface so that crossover, mutation and other operators can be added more easily;

