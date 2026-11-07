# Your first Genetik search

This example fits a small one-variable data set using the ordinary symbolic
regression API. It uses only the installed public interface, a fixed random seed,
and finite candidate/loss budgets.

Build and run it from the repository root:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --target genetik_first_search_quickstart -j2
build/release/genetik_first_search_quickstart
```

The source is [`examples/quickstarts/first_search.cpp`](../examples/quickstarts/first_search.cpp).
It can also be built as part of the installed quickstarts project after installing
Genetik; see [package instructions](PACKAGING.md).

The example has one input column and one target column per training row. Its
search budget limits evaluations and training-loss calls. The displayed best
expression is the best valid result found so far. A genetic search is heuristic:
the example does not promise to rediscover a generating equation or extrapolate
well from four rows. For an exact small scalar-genome GA walkthrough, see
[`examples/quickstarts/scalar.cpp`](../examples/quickstarts/scalar.cpp).

The reusable implementation still owns fitness validation, candidate staging,
selection and publication through the generic GA; see [architecture](ARCHITECTURE.md).
