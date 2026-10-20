# Genetik

Genetik is a general-purpose, header-only C++20 genetic algorithm library with
public OOP extension points. Applications own their candidates and customize
initialization, fitness, optional preparation/refinement, selection, crossover and
mutation through the supported [OOP and variation interfaces](docs/ARCHITECTURE.md).
Use scalar genomes or your own representation, such as a permutation.

Symbolic regression is a reusable specialization of that same engine: evolve
bounded expressions against ordinary multi-input, single-output numerical rows.
It is heuristic search, with no guarantee of a good fit, a particular equation or
extrapolation accuracy. Prime regression is one application example.

The library uses only the C++ standard library and is [MIT licensed](LICENSE),
copyright Karl Phillip (2021). Package version **0.1.0** is planned as a development
pre-release. It remains
**unreleased during this review**; [release proposal](docs/RELEASE_PROPOSAL.md) and
[release availability](https://github.com/karlphillip/Genetik/releases) are separate
from successful CI.
CMake **3.16 or newer** is required. Link **`Genetik::Genetik`** for includes, C++20
and the required strict floating-point options. Newer C++ standards are allowed.

## Start here

- [Complete scalar-genome program](examples/quickstarts/scalar.cpp): a custom real-valued
  candidate minimizes squared error, shares an explicit `RandomContext(42)` with
  its population/algorithm, and stops after ten generations.
- [Complete symbolic-regression program](examples/quickstarts/symbolic.cpp): caller-provided
  rows bind `x0` to the first input column. Optional constant fitting, a 256 actual
  loss-call budget and four-generation ceiling bound the search. It prints the
  stopping outcome and faithful expression. Exact discovery is not required for
  either program to finish successfully.
- [Application-owned permutation consumer](tests/oop_consumer/main.cpp): a non-scalar
  scheduling genome with custom fitness and virtual selection/variation, no symbolic
  headers, and deterministic correctness checks independent of discovering an optimum.

Obtain the current review branch (the default branch has not yet been integrated):

```sh
git clone --branch qualification/hosted-0.1.0 https://github.com/karlphillip/Genetik.git Genetik
cd Genetik
```

After approval, `v0.1.0` is the proposed tag; it does not exist during preparation.
Alternatively extract the proposed source archive and enter `Genetik-0.1.0/`.
From either source root, install and build these exact programs:

```sh
cmake -S . -B out/library -DBUILD_TESTING=OFF -DGENETIK_BUILD_TESTS=OFF -DGENETIK_BUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX="$PWD/out/prefix"
cmake --build out/library
cmake --install out/library
cmake -S examples/quickstarts -B out/quickstarts -DCMAKE_PREFIX_PATH="$PWD/out/prefix"
cmake --build out/quickstarts -j2
out/quickstarts/scalar_quickstart
out/quickstarts/symbolic_quickstart
```

The quickstarts' [CMakeLists.txt](examples/quickstarts/CMakeLists.txt) is a complete external
consumer. In your application, enable CXX and use:

```cmake
find_package(Genetik 0.1 CONFIG REQUIRED)
add_executable(application main.cpp)
target_link_libraries(application PRIVATE Genetik::Genetik)
```

Headers retain their spelling: `#include <SymbolicRegression.hpp>`,
`#include <Creature_i.hpp>`, etc. They install beneath `include/Genetik`; use the
target's include root. No Python, test framework, GPU, BLAS or file-format library
is required to install or consume the library.

Alternatively, include the source in a parent project:

```cmake
add_subdirectory(external/Genetik)
add_executable(application main.cpp)
target_link_libraries(application PRIVATE Genetik::Genetik)
```

Embedded builds default tests, examples/campaign tools and install rules to OFF;
explicit `GENETIK_…` choices are respected. The source-only `genetik_lib` target is
retained for compatibility. Genetik does not select the parent's compiler, build
type, optimization level or C++ standard. See [package details](docs/PACKAGING.md).

## Develop and verify

```sh
cmake -S . -B ../build-release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON
cmake --build ../build-release -j2
cmake -E chdir ../build-release ctest --output-on-failure
../build-release/genetik_demo 42 1000
```

Top-level tests/examples default ON; development integration checks additionally
require Python 3.9+. For sanitizers, use a separate Debug build with
`-DGENETIK_SANITIZERS=ON`. Development flags are private to Genetik's own targets.
On the supplied Jetson, tooling is workspace-local: `../.tools/bin/cmake` and
`../.tools/bin/ctest`; this is not a dependency of an installed package.

The header-only target's **required floating-point options affect consuming C++
translation units**, not just expressions inside headers. GCC/GNU-style Clang get
`-fno-fast-math -ffp-contract=off`; MSVC-style frontends select `/fp:strict` but
remain unqualified. C sources do not receive these C++ options. Do not override
with unsafe math flags. Default IEEE rounding, gradual underflow, nontrapping
arithmetic and the qualified runtime math library are part of the numerical
contract. [Support and repeatability policy](docs/SUPPORT.md).

## Documentation

- [API/contract index](docs/API.md) and [OOP extensions](docs/ARCHITECTURE.md).
- [Migration from the original interfaces](docs/MIGRATION.md).
- [Installation/package requirements](docs/PACKAGING.md).
- [Concurrency and callback qualification](docs/CONCURRENCY.md).
- [Support/version policy](docs/SUPPORT.md) and [executed hosted qualification](docs/HOSTED_QUALIFICATION.md).
- [Proposed 0.1.0 release notes](docs/releases/0.1.0/NOTES.md) and [integration/publication proposal](docs/RELEASE_PROPOSAL.md).

## API and examples

Start at the [public API guide](docs/API.md). Detailed contracts cover explicit
randomness, fitness validity, immutable datasets, work accounting, failed-generation
publication, strict arithmetic, faithful export, fitting and fitting cadence.

Applications use the same public search path:

- [Polynomial regression](docs/POLYNOMIAL_CAMPAIGN.md).
- [Multivariate regression and numerical CSV input](docs/MULTIVARIATE.md).
- [Strict division/natural logarithm](docs/STRICT_ARITHMETIC.md).
- [Prime-sequence regression](docs/PRIME_REGRESSION.md), an indexed-sequence example
  with application-level reference generation/scaling/baselines, not a prime oracle.
- [Evaluation workspace measurements](docs/EVALUATION_WORKSPACE.md).

For example, after generating ordinary numerical files as described in the guides:

```sh
../build-release/genetik_regression_search training.csv --inputs first,second --target target --seed 42 --loss-budget 4096 --operators add,sub,mul,div,log --fit-every 1
```

Tests and examples use deterministic execution bounds; stochastic recovery is
reported separately and is never a required CI campaign. See [delivery evidence](docs/DELIVERY.md),
[version/release policy](docs/SUPPORT.md), [changelog](CHANGELOG.md) and [roadmap](docs/ROADMAP.md).

## Custom genomes and project layout

The installed [permutation scheduling consumer](tests/oop_consumer) demonstrates
application-owned representation, virtual fitness/selection/variation and the shared
publication lifecycle without symbolic headers. [Architecture and extension contracts](docs/ARCHITECTURE.md).

```sh
cmake -S tests/oop_consumer -B out/oop-consumer -DCMAKE_PREFIX_PATH="$PWD/out/prefix"
cmake --build out/oop-consumer -j2
out/oop-consumer/permutation_consumer
```

Demos live in examples/, numerical file search in apps/, performance harnesses in
benchmarks/, campaigns/helpers in scripts/. Executable names and explicit output
options are unchanged. Put new generated results under out/ or build/temp paths;
accepted evidence remains in docs/. [Layout and historical path mapping](docs/ORGANIZATION.md).
Population evaluation/refinement has an opt-in library-owned CPU backend. Use
`RandomContext(42, ExecutionConfig{4})` or `--threads 4` in evolution applications;
serial/default 1 remains supported. Candidates/callbacks require explicit parallel
qualification; immutable regression data and independent built-in refiners are
supported. [Ownership, budgets, cancellation and repeatability](docs/CONCURRENCY.md).

```sh
../build-release/genetik_demo 42 20 --threads 4
out/quickstarts/symbolic_quickstart --threads 4
out/oop-consumer/permutation_consumer --threads 3
```

Genetik::Genetik supplies platform threading requirements through Threads::Threads;
installed consumers need no manual thread flags. Public engine access remains
single-coordinator; only the documented stop controls are concurrently callable.

Local concurrency [qualification and fixed-work measurements](docs/CONCURRENCY_RESULTS.md)
include exact serial replays and separate ThreadSanitizer execution. Hosted GCC13 x64/minimum-CMake, Clang18 x64/ASan+UBSan and GCC13 ARM64/TSan
[qualification has executed](docs/HOSTED_QUALIFICATION.md); version 0.1.0 remains unreleased.
