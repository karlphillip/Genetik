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

The library uses only the C++ standard library by default and is [MIT licensed](LICENSE).
**Genetik 0.7.0 — Advanced Scientific Regression** is published on GitHub as a
development pre-release, not a stable release. It adds shared-parameter
derivatives, optional Levenberg–Marquardt fitting, bounded exact quantized
constraints, domain-aware symbolic assumptions, and an output-only
accuracy–complexity equation archive. Coordinate fitting and generic GA defaults
remain unchanged. Parameter fitting optimizes a supplied fixed expression
topology; it does not perform autonomous mathematical discovery. The release
also retains optional native Python access with
NumPy input, `GenetikRegressor`, SymPy export and a local SRBench adapter. It also includes the opt-in
[D5 advanced mathematics](docs/ADVANCED_MATHEMATICS_D5.md) and
[D6 scientific discovery](docs/SCIENTIFIC_DISCOVERY_D6.md): checked integer
math and recurrences, symbolic algebra, dimensions and constraints, coupled
equation systems, supplied-observation ODE/PDE residuals, bounded matrices and
tensors, restricted complex regression, statistics, probability and special
functions. Established GA defaults and generic OOP use cases remain supported.
Python is optional; local wheel qualification is Linux AArch64/CPython 3.12,
and hosted wheel qualification has passed on Ubuntu 24.04 with CPython 3.12 on
both x86-64 and AArch64. Python qualification covers CPython 3.12 on Linux
x86-64 and ARM64 only; other Python versions and operating systems remain
unqualified. The quantized solver supports only its documented bounded
mathematical fragment, and exact rational feasibility does not guarantee an
executable binary64 witness. The equation archive is output-only and does not
affect evolutionary selection. The external benchmarks do not establish general
superiority over PySR. ARM64 TSan qualification covers the selected checks, not
the full suite. See
[I2 Python interoperability](docs/I2_PYTHON_INTEROPERABILITY.md),
[0.7.0 notes](docs/releases/0.7.0/NOTES.md), [migration](docs/I2_MIGRATION.md),
[D7 numerical boundaries](docs/D7_NUMERICAL_QUALIFICATION.md), and
[historical benchmark report](docs/d7/BENCHMARK_REPORT.md). The release source is
the `v0.7.0` tag; its hosted qualification and release assets are recorded in
[S3 release preparation](docs/S3_RELEASE_PREPARATION.md).
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
- [Python symbolic-regression example](examples/python/quickstart.py): install the
  optional `genetik` package, fit through the native C++ engine and inspect the
  selected canonical and SymPy expressions.
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
- [Advanced mathematical quickstart](examples/quickstarts/advanced_mathematics.cpp):
  checked modular powers, a bounded first witness, Fibonacci, scientific partials,
  conditional symbolic transformations, units and bounded shared-engine discovery.
- [Application-owned permutation consumer](tests/oop_consumer/main.cpp): a non-scalar
  scheduling genome with custom fitness and virtual selection/variation, no symbolic
  headers, and deterministic correctness checks independent of discovering an optimum.

Obtain the v0.7.0 development pre-release source:

```sh
git clone --branch v0.7.0 https://github.com/karlphillip/Genetik.git Genetik
cd Genetik
```

For published 0.6.0, use its verified source archive and enter `Genetik-0.6.0/`.
This release builds on v0.6.0; D5/D6 APIs first appeared in the 0.5.0 series.
The commands below describe the v0.7.0 source tree; older release archives
retain their own package versions and instructions.
From the corresponding source root, install and build the available programs:

```sh
cmake -S . -B build/library -DBUILD_TESTING=OFF -DGENETIK_BUILD_TESTS=OFF -DGENETIK_BUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX="$PWD/out/install/local"
cmake --build build/library
cmake --install build/library
cmake -S examples/quickstarts -B out/consumers/quickstarts -DCMAKE_PREFIX_PATH="$PWD/out/install/local"
cmake --build out/consumers/quickstarts -j2
out/consumers/quickstarts/scalar_quickstart
out/consumers/quickstarts/symbolic_quickstart
out/consumers/quickstarts/typed_quickstart
out/consumers/quickstarts/templates_quickstart --threads 2
out/consumers/quickstarts/derivatives_quickstart
out/consumers/quickstarts/derivative_regression_quickstart --threads 2
out/consumers/quickstarts/integrals_quickstart --threads 2
```

The quickstarts' [CMakeLists.txt](examples/quickstarts/CMakeLists.txt) is a complete external
consumer. In your application, enable CXX and use:

```cmake
find_package(Genetik 0.7 CONFIG REQUIRED)
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
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON
cmake --build build/release -j2
cmake -E chdir build/release ctest --output-on-failure
build/release/genetik_demo 42 1000
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
- [Historical 0.3.0 release notes](docs/releases/0.3.0/NOTES.md) and [integration/publication proposal](docs/RELEASE_0.3.0_PROPOSAL.md).

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
build/release/genetik_regression_search training.csv --inputs first,second --target target --seed 42 --loss-budget 4096 --operators add,sub,mul,div,log --fit-every 1
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
benchmarks/, compiled campaigns in apps/campaigns/ and development helpers in scripts/.
Executable names and explicit output options are unchanged. Put compiled output under
build/; keep transient runs/logs/tmp under out/ and durable datasets, experimental
evidence and benchmark results under ../Genetik-data/.
accepted evidence remains in docs/. [Layout and historical path mapping](docs/ORGANIZATION.md).
Population evaluation/refinement has an opt-in library-owned CPU backend. Use
`RandomContext(42, ExecutionConfig{4})` or `--threads 4` in evolution applications;
serial/default 1 remains supported. Candidates/callbacks require explicit parallel
qualification; immutable regression data and independent built-in refiners are
supported. [Ownership, budgets, cancellation and repeatability](docs/CONCURRENCY.md).

```sh
build/release/genetik_demo 42 20 --threads 4
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
and [Phase 1 cleanup evidence](docs/CLEANUP.md). Use `build/` for fresh builds,
`out/install/` and `out/consumers/` for delivery, and `out/logs/` and `out/tmp/`
for operational output. Historical runs live in sibling
`Genetik-data/research-runs/`; the local `out/runs/` compatibility link preserves
old paths. Durable datasets, research runs, experimental evidence and benchmark
results belong in sibling `Genetik-data/`. `GENETIK_DATA_ROOT` optionally changes
supported default research outputs to `$GENETIK_DATA_ROOT/research-runs/`; unset
preserves historical defaults and explicit output paths still take precedence.
Do not move configured CMake caches. Configure a dedicated benchmark build with
`cmake -S . -B build/benchmarks -DCMAKE_BUILD_TYPE=Release`; benchmark source stays
in `benchmarks/` and resulting executables stay in that build directory.

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
build/release/genetik_regression_evaluate training.csv \
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

## Interactive exact prime-prefix demo

The additive [exact-prefix mode](docs/PRIME_EXACT_SEARCH.md) searches for1,000/1,000
rounded matches, with committed top-five reports (including duplicates) after roughly
30 seconds at safe generation boundaries:

```sh
build/release/genetik_prime_regression --search-exact --seed 42 --threads 4 --loss-budget 1000000 --report-seconds 30 --formula-precision 6
```

Ctrl+C cooperatively cancels; loss and generation limits remain finite. The default
historical approximation modes are unchanged. Exact recovery of this supplied finite
prefix would not establish a general formula for all primes.

Run-owned [checkpoint/resume and human progress](docs/RUN_CHECKPOINTING.md) are
available for this exact-prime application. Add `--checkpoint out/runs/prime-exact/seed42/checkpoints`
and optionally `--metrics-file out/runs/prime-exact/seed42/metrics.jsonl`. Continue with
`--resume out/runs/prime-exact/seed42/checkpoints/latest.gtkc --loss-budget 2000000`
(the **total**, including prior work). `--report-format raw` retains dense diagnostics.
The installed [checkpoint/formatter quickstart](examples/quickstarts/checkpoint.cpp)
uses the generic state seams and scalar-expression codec without prime dependencies.

Optional discrete modules: [D1](docs/DISCRETE_EXPRESSIONS.md), [D2](docs/DISCRETE_ITERATION.md), [D3 results](docs/DISCRETE_D3_RESULTS.md). They do not replace the generic OOP engine.

Current release: [Genetik 0.7.0 development pre-release notes](docs/releases/0.7.0/NOTES.md),
[S3 migration guide](docs/S3_MIGRATION.md),
[S3 mathematical qualification](docs/S3_MATHEMATICAL_QUALIFICATION.md),
[S3 benchmark findings](docs/S3_SCIENTIFIC_BENCHMARKS.md), and
[release preparation and hosted artifacts](docs/S3_RELEASE_PREPARATION.md).
