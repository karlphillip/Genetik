# Phase 5 local expression-import qualification

Starting checkpoint: hosted-qualified
`ae2f819b78b544eb34d146f9e8155aef283e2b60`, clean
qualification/typed-scientific. Local branch feature/expression-parsing retains all
cleanup, typed evaluation/regression and scientific ancestry. Implementation:
`898571a` (full identity is in parsing-results/implementation.json). The following
evidence commit changes documentation/evidence only. No remote action, version
change, default-branch integration, PR, tag or publication occurred.

## Implemented public boundary

[EXPRESSION_PARSING.md](EXPRESSION_PARSING.md) records the grammar, design decision,
resource units, ownership, API, diagnostics, failure classification and examples.
The dependency-free lexer/parser imports into existing Expression/TypedExpression,
never a second evaluator. No existing runtime header changed. Both APIs preserve
postfix structure, variable/schema identity, shapes, constant bits, independent
occurrences and powi metadata under the bounded import profile. Canonical re-export
is exact; noncanonical decimal/spacing/precedence forms normalize explicitly.
Programmatic expressions beyond128 canonical syntactic nesting levels are outside
this parser profile; existing deep iterative evaluator APIs remain available.

`genetik_regression_evaluate` reads a bounded expression file and uses existing
numerical tables/ordered columns and checked loss/all-row prediction reports.
Evolution is never implicit. Explicit --fit uses the existing coordinate fitter
on training rows only; assessment files are validated first and scored only after
model finalization. Parser configuration does not alter search grammars or budgets.
No external toolkit, CAS, parser package or network dependency is introduced.

## Environment and exact executed gates

Native Jetson ARM64 Ubuntu24.04.4 LTS, kernel6.8.12-1021-tegra; GCC13.3.0,
release13 standard headers (`__GLIBCXX__=20240904`), libstdc++6/libtsan2
14.2.0-4ubuntu2~24.04.1, glibc/libm2.39 (`libc6=2.39-0ubuntu8.8`), Python3.12.3.
Workspace CMake/CTest4.4.4 and exact minimum3.16.0. No compiler support claim is
inferred solely from flags or earlier hosted runs.

| Gate | Result | Observed suite wall time |
| --- | --- | ---: |
| Release / Werror / CMake4.4.4 | 38/38 | 203.88s |
| Debug / Werror / CMake4.4.4 | 38/38 | 211.00s |
| Debug ASan+UBSan / Werror / CMake4.4.4 | 38/38 | 590.55s |
| Release / Werror / exact CMake+CTest3.16.0 | 38/38 | 147.30s |
| Separate Debug TSan | six selected cases, each3 repetitions;18 passes | 2.38s |

Builds/tests overlapped under varying load: these wall times are qualification
observations, not performance comparisons. Every main configuration built all
standalone headers, applications and actual quickstarts, with `-Wall -Wextra
-Wpedantic -Werror`, C++20, `-fno-fast-math -ffp-contract=off`. ASan+UBSan and TSan
are separate fully instrumented development builds. No suppressions/timeouts were
added or enlarged. Header/library usage requirements remain unchanged.

The source-wide suite includes all prior tests, including scalar/typed numerical,
fitness, loss/admission/publication, fitting, concurrency and the qualified local
captured snapshot. New names are genetik_parsing_contracts,
genetik_parsing_overlap, genetik_parsing_quickstart and genetik_expression_file.
Actual manifests: [local38](parsing-results/local-tests.txt) and
[portable37](parsing-results/portable-tests.txt). The only portable exclusion is
still the environment-specific genetik_evaluation_reference. scripts/check_ci_manifest.py
compares exact sets/counts (no guessed count replacement). Existing independent
scientific numerical fixtures and tolerances remain enabled. The new hosted matrix
configuration includes all37 portable cases; it has **not executed** for this local
parser work.

The final reordered-column file fixture uses a noncommutative x0-2*x1 expression,
not a sum that could mask swapped inputs. Its final form was additionally repeated
successfully in all four configurations after the complete-suite runs; only that
fixture script changed, not any production code or compiled test kernel. Separate
final-file logs record these executions. All sources used for final qualification
are identified by [SOURCE_SHA256.json](parsing-results/SOURCE_SHA256.json).

## Focused contracts and independent comparisons

- Directly constructed scalar/typed references cover every supported operator,
  rectangular products/transposition/dot/scaling and all literal ranks. Comparisons
  check node fields, coefficient bits, variable/schema identity, constants/exponents,
  canonical text, evaluation classification/failure node, exact finite output bits
  and arithmetic/temporary-plan work. Public A*x+b evaluates to independently
  checked [14,8]. Scalar precedence, associativity and floating-sensitive grouping
  have independently known results.
- Zero/negative zero, subnormals, extreme finite values,500 reproducible binary64
  bit patterns (finite samples imported), alternate decimal locale, repeated
  independent constants and exponents0/1/2/3/8 are checked. Invalid child overflow
  under exponent0, log(-1), signed-zero division and resource stops preserve the
  existing classification/work semantics. No parse performs domain evaluation.
- The deterministic bounded generator uses seed2027:200 scalar expressions,
  50 typed iterations (vector and dot expressions), plus600 malformed trailing
  mutations. A fixed28-entry malformed scalar corpus, typed payload/shape cases,
  nesting/token/workspace/dimension/constant limits, out-of-range decimal/integer
  conversion and excessive token/nesting stress supplement it. There is no existing
  integrated fuzz backend; this same reproducible harness runs under ASan+UBSan.
- Local source/schema buffers are changed after import, copies remain independent,
  failed replacement preserves the original, and allocation failures are injected
  at18 scalar/28 typed allocation positions. bad_alloc is retained as an operational
  cause, never a syntax error or Invalid fitness. Subsequent successful calls work.
- Direct/imported scalar coefficient fitting recovers1.75 with identical zero
  loss/results and loss/row counts. Aggregate fitting recovers vector[1.75,-0.5]
  with matching structure, zero loss and actual-loss/arithmetic work. A parsed sqrt
  fixture retains earlier accepted progress after invalid probes, exactly as the
  independently constructed expression does.
- The documented file fit produces `(1.75 * x0)`, MSE0/max error0,56 actual training
  losses/168 rows under allowance128. Reporting adds one separate loss over3 rows;
  no search occurs. Reordered columns, malformed/missing/oversized formula files,
  explicit fitting and invalidity after valid rows are exercised. log(x0) over
  x={0,1,-1} reports full loss invalid and exactly2 invalid predictions out of3.

## Concurrency and delivery

Separate `TSAN_OPTIONS=halt_on_error=1` execution uses CTest `--repeat until-fail:3`
on exactly these names: genetik_concurrency_contracts, genetik_oop_parallel,
genetik_typed_overlap, genetik_typed_preparation_overlap,
genetik_scientific_contracts and genetik_parsing_overlap. All18 executions pass.
The parser fixture synchronizes two typed imports inside their first allocation,
proving overlap within parsing; both also repeatedly parse/evaluate the shared
scalar source. Source/schema are immutable, parser state and fault controls are
thread-exclusive. Gate cardinality is2; CTest bounds hangs. This is focused TSan,
not a claim that all38 cases execute under TSan.

Every main suite runs source inclusion and installed/relocated isolated multi-TU
consumers. Both installed TUs overlap the new parser headers and execute public
imports. The actual parsing quickstart builds/runs against both installation
prefixes and C++23 where supported. CMake3.16 exercises mandatory C++20. Producer
source/build copies are removed before relocated consumption, paths contain spaces,
package resolution/flags/dependencies are checked, and installed consumers acquire
no development sanitizer runtimes. The nonmathematical OOP consumer dependency
check explicitly excludes both parser headers as well as mathematical headers.

GCC O0/-fno-inline/-fstack-usage shows recursive parser-frame sizes: expression96,
term96, primary208(scalar)/240(typed), literal-number144 bytes. Combined nested
paths remain below the conservative1024-byte per-level charge on this qualified
compiler, with hard depth128 and ordinary8MiB process stack (documented minimum1MiB).
This is bounded parser-stack evidence, not a promise about arbitrary external stacks.

## Commands and retained diagnostics

From repository root (ordinary cmake/ctest equivalent; local tool paths below):

```sh
../.tools/bin/cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build out/build/release -j2
../.tools/bin/ctest --test-dir out/build/release --output-on-failure
# Repeat with out/build/debug and -DCMAKE_BUILD_TYPE=Debug;
# out/build/sanitize adds -DGENETIK_SANITIZERS=ON -DGENETIK_THREAD_SANITIZER=OFF.
../delivery-tools/cmake-min/bin/cmake -S . -B out/build/minimum -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../delivery-tools/cmake-min/bin/cmake --build out/build/minimum -j2
../delivery-tools/cmake-min/bin/cmake -E chdir out/build/minimum "$PWD/../delivery-tools/cmake-min/bin/ctest" --output-on-failure
../.tools/bin/cmake -S . -B out/build/tsan -DCMAKE_BUILD_TYPE=Debug -DGENETIK_THREAD_SANITIZER=ON -DGENETIK_SANITIZERS=OFF -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_BUILD_EXAMPLES=OFF -DGENETIK_RECORDED_REFERENCE=OFF
../.tools/bin/cmake --build out/build/tsan --target genetik_concurrency_tests genetik_oop_consumer genetik_typed_tests genetik_typed_regression_tests genetik_scientific_tests genetik_parsing_tests -j2
TSAN_OPTIONS=halt_on_error=1 ../.tools/bin/ctest --test-dir out/build/tsan -R 'genetik_concurrency_contracts|genetik_oop_parallel|genetik_typed_overlap|genetik_typed_preparation_overlap|genetik_scientific_contracts|genetik_parsing_overlap' --repeat until-fail:3 --output-on-failure
python3 scripts/check_ci_manifest.py out/logs/parsing/portable-manifest.json
python3 scripts/check_ci_manifest.py out/logs/parsing/tsan-manifest.json --tsan
python3 scripts/check_hygiene.py --workspace --self-test
```

Workflow actionlint and git diff --check pass. Character --threads2 demo, formula
quickstart and the documented file-fit command execute without root clutter.
Logs/manifests/flags, output examples and source/evidence checksums are retained in
[parsing-results/](parsing-results/); complete temporary build/consumer commands
remain under out/logs/parsing/ and out/build/. Retained flag summaries trim trailing
whitespace; original generated flags remain in the build trees. Initial app compilation exposed a
misleading-indentation Werror in new option parsing; splitting the declaration onto
its own line fixed it, with original diagnostic retained. An environment-helper
invocation with g++ (instead of its required matrix identifier g++-13) was corrected;
its original output/note are retained under out/logs/parsing/. No numerical defect,
sanitizer finding or test expectation was suppressed. No research campaign repeated.

## Preservation and remaining scope

All127 frozen hashes, prior typed/scientific campaigns including oscillator seed1,
main and v0.1.0 are unchanged. Authoritative published archive SHA256 is still
7f632489e45abb97385d98c9c2f703664386e358b22f3f73baa8a72c99b6b992;
SHA256SUMS identity a12d576eedc38b80af11f118d82d086a992f3d8930b89bda743b91191663353b.
Existing runtime source and default search behavior are byte-identical, so no
long historical replay/campaign rerun is warranted by this additive input boundary.
Within-build repeatability and serial/parallel contracts remain in every suite.

No existing public API migration, new mandatory dependency or package-version change.
Hosted qualification of Phase5 is pending; previous hosted evidence remains scoped
to its exact older SHA. Windows/MSVC/AppleClang/libc++ remain unqualified. No general
unary negation, aliases, parser callbacks, real powers, typed CSV, model archive,
formula population initialization, shared parameters, templates, calculus or external
adapter is added. Phase6 constraints/templates and separately justified optional
adapters remain future work. Engineering acceptance concerns owned bounded import,
not mathematical-equivalence proofs or search recovery.
