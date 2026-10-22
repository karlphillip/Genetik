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
copyright Karl Phillip (2021). Version **0.1.0** is available as a
[development pre-release](https://github.com/karlphillip/Genetik/releases/tag/v0.1.0).
This checkout may contain subsequent local development; the published tag and assets
remain unchanged.
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
- [Owned scalar/vector/matrix evaluation](examples/quickstarts/typed.cpp): constructs
  rectangular `A*x+b`, checks `[14,8]`, matrix multiplication, transpose and dot product,
  and handles a local resource limit. This additive evaluator is separate from
  scalar regression/search; see [typed contracts](docs/TYPED_EXPRESSIONS.md).
- [One-site scalar template quickstart](examples/quickstarts/templates.cpp): freeze
  a model scaffold, restrict an evolving expression to selected original variables
  and operators, optionally fit only its constants, and export the complete model.
  See [template contracts and campaign](docs/SCALAR_TEMPLATES.md).
- [Bounded scalar partials](examples/quickstarts/derivatives.cpp): ordinary value
  plus one selected input derivative, parsed scientific expressions, materialized
  templates and explicit unavailable-derivative diagnostics.
- [Supplied value/partial regression](examples/quickstarts/derivative_regression.cpp):
  explicit weighted observation channels drive shared search and coordinate fitting;
  a value anchor accompanies derivative observations without supplying extra value labels.
- [Bounded root integral models](examples/quickstarts/integrals.cpp): prescribed
  bounds and a local binding, estimated-convergence quadrature, and shared-engine
  integrand discovery/coordinate fitting against complete integral observations.
- [Application-owned permutation consumer](tests/oop_consumer/main.cpp): a non-scalar
  scheduling genome with custom fitness and virtual selection/variation, no symbolic
  headers, and deterministic correctness checks independent of discovering an optimum.

Obtain the source (use `--branch v0.1.0` for the published checkpoint):

```sh
git clone https://github.com/karlphillip/Genetik.git Genetik
cd Genetik
```

Alternatively extract the published source archive and enter `Genetik-0.1.0/`.
The typed quickstart and canonical output conventions below describe post-0.1.0
local development. The published tag contains the original scalar/symbolic
quickstarts, not the additive typed API; omit the typed program when using that tag.
From the corresponding source root, install and build the available programs:

```sh
cmake -S . -B out/build/library -DBUILD_TESTING=OFF -DGENETIK_BUILD_TESTS=OFF -DGENETIK_BUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX="$PWD/out/install/local"
cmake --build out/build/library
cmake --install out/build/library
cmake -S examples/quickstarts -B out/consumers/quickstarts -DCMAKE_PREFIX_PATH="$PWD/out/install/local"
cmake --build out/consumers/quickstarts -j2
out/consumers/quickstarts/scalar_quickstart
out/consumers/quickstarts/symbolic_quickstart
out/consumers/quickstarts/typed_quickstart
out/consumers/quickstarts/templates_quickstart --threads 2
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
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON
cmake --build out/build/release -j2
cmake -E chdir out/build/release ctest --output-on-failure
out/build/release/genetik_demo 42 1000
python3 scripts/check_hygiene.py --workspace # optional local parent-workspace audit
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
- [Support/version policy](docs/SUPPORT.md) and [executed feature hosted qualification](docs/TYPED_SCIENTIFIC_HOSTED_QUALIFICATION.md).
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
out/build/release/genetik_regression_search training.csv --inputs first,second --target target --seed 42 --loss-budget 4096 --operators add,sub,mul,div,log --fit-every 1
```

Tests and examples use deterministic execution bounds; stochastic recovery is
reported separately and is never a required CI campaign. See [delivery evidence](docs/DELIVERY.md),
[version/release policy](docs/SUPPORT.md), [changelog](CHANGELOG.md) and [roadmap](docs/ROADMAP.md).

## Custom genomes and project layout

The installed [permutation scheduling consumer](tests/oop_consumer) demonstrates
application-owned representation, virtual fitness/selection/variation and the shared
publication lifecycle without symbolic headers. [Architecture and extension contracts](docs/ARCHITECTURE.md).

```sh
cmake -S tests/oop_consumer -B out/consumers/oop -DCMAKE_PREFIX_PATH="$PWD/out/install/local"
cmake --build out/consumers/oop -j2
out/consumers/oop/permutation_consumer
```

Demos live in examples/, numerical file search in apps/, performance harnesses in
benchmarks/, compiled campaigns in apps/campaigns/ and development helpers in scripts/. Executable names and explicit output
options are unchanged. Put new generated results under out/build/, out/runs/, out/data/, out/logs/ or out/tmp/;
accepted evidence remains in docs/. [Layout and historical path mapping](docs/ORGANIZATION.md).
Population evaluation/refinement has an opt-in library-owned CPU backend. Use
`RandomContext(42, ExecutionConfig{4})` or `--threads 4` in evolution applications;
serial/default 1 remains supported. Candidates/callbacks require explicit parallel
qualification; immutable regression data and independent built-in refiners are
supported. [Ownership, budgets, cancellation and repeatability](docs/CONCURRENCY.md).

```sh
out/build/release/genetik_demo 42 20 --threads 4
out/consumers/quickstarts/symbolic_quickstart --threads 4
out/consumers/oop/permutation_consumer --threads 3
```

Genetik::Genetik supplies platform threading requirements through Threads::Threads;
installed consumers need no manual thread flags. Public engine access remains
single-coordinator; only the documented stop controls are concurrently callable.

Local concurrency [qualification and fixed-work measurements](docs/CONCURRENCY_RESULTS.md)
include exact serial replays and separate ThreadSanitizer execution. Hosted GCC13 x64/minimum-CMake, Clang18 x64/ASan+UBSan and GCC13 ARM64/TSan
[qualification has executed](docs/TYPED_SCIENTIFIC_HOSTED_QUALIFICATION.md) for the
accumulated typed/scientific feature candidate. The published v0.1.0 pre-release
remains unchanged; this feature branch is subsequent development.

Current output conventions and retained workspace records: [ORGANIZATION.md](docs/ORGANIZATION.md)
and [Phase 1 cleanup evidence](docs/CLEANUP.md). Use `out/build/` for fresh builds,
`out/install/` and `out/consumers/` for delivery, and `out/runs/`, `out/data/`,
`out/logs/`, `out/tmp/` for generated work. Do not move configured CMake caches.

### Additive typed symbolic regression (post-v0.1.0 development)

Owned scalar/vector/matrix expressions now participate in the same generic engine.
Use ordinary immutable typed rows, exact shape-aware variation and optional bounded
coefficient fitting, with serial default or run-owned `--threads N` execution.
See [the public typed-regression guide](docs/TYPED_REGRESSION.md), the compiled
[installed quickstart](examples/quickstarts/typed_search.cpp),
[operation discovery](docs/TYPED_DISCOVERY_RESULTS.md) and separate
[coefficient recovery](docs/TYPED_FITTING_RESULTS.md). Typed file syntax is not supported.
The generic OOP interfaces and existing scalar APIs remain independent/compatible.

Scientific scalar functions are opt-in in both expression APIs and generic search;
see [scientific operators and file example](docs/SCIENTIFIC_OPERATORS.md).

### Import mathematical expressions

Use `<ExpressionParsing.hpp>` and `<TypedExpressionParsing.hpp>` to import bounded
text into the existing owned expressions, with an explicit variable count/schema.
For example `parseExpression("sin(x0)+powi(x0,2)", 1)` or
`parseTypedExpression("matvec(x0,x1)+x2", schema, TypedShape::vector(2))`.
No parser dependency enters generic OOP consumers. The compiled installed
[parsing quickstart](examples/quickstarts/parsing.cpp) demonstrates both APIs,
owned aggregate literals, canonical round trips and diagnostics.

Evaluate a supplied expression without evolution:

```sh
out/build/release/genetik_regression_evaluate training.csv \
  --expression formula.txt --inputs first,second --target target
```

Add `--fit --fit-budget 256` for explicit training-only fixed-structure fitting,
and `--evaluate other.csv` for post-finalization assessment. Formula file reading
is bounded; column mapping and full invalid-prediction reporting reuse the existing
numerical file workflow. See [grammar, limits and usage](docs/EXPRESSION_PARSING.md)
and [local Phase 5 qualification](docs/PARSING_QUALIFICATION.md). The combined
[parser/template hosted record](docs/PARSER_TEMPLATES_HOSTED_QUALIFICATION.md)
identifies its executed candidate; later derivative changes remain locally qualified.

Bounded first-order scalar partials are available through the separately included
`ExpressionDerivative.hpp`; see the [contract and installed quickstart](docs/SCALAR_DERIVATIVES.md).
This additive capability does not change search grammars or fitting.

Supplied value and first-partial observations can drive the same search and coordinate
fitter through [DerivativeRegression.hpp](genetik/DerivativeRegression.hpp). See the
[API/objective contract](docs/DERIVATIVE_REGRESSION.md), [installed quickstart](examples/quickstarts/derivative_regression.cpp)
and [bounded anchored-antiderivative evidence](docs/DERIVATIVE_REGRESSION_RESULTS.md).

Bounded root integral models: [API and numerical policy](docs/SCALAR_INTEGRALS.md),
[all five integrand-discovery outcomes](docs/INTEGRAL_RESULTS.md), and
[local qualification](docs/INTEGRAL_QUALIFICATION.md). This completes the agreed
calculus implementation; combined hosted qualification and final delivery review follow.
