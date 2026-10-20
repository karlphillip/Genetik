# Phase 2 typed-evaluation qualification

Starting point: clean cleanup branch at `e906cbdb68e2e7a72fde573103ef0cd7c2c311ab`,
including both Phase 1 commits. Implementation is
`d63f06cc2c9445716b99e1625f06ab2cf8a221ab`, on local `feature/typed-evaluation`.
The following documentation-only checkpoint records the result. No remote actions,
default-branch integration, tag or release changes were performed.

Contracts and usage: [TYPED_EXPRESSIONS.md](TYPED_EXPRESSIONS.md).
Complete installed example: [quickstarts/typed.cpp](../examples/quickstarts/typed.cpp).
No old public header or generic GA implementation changed; the two new headers are
additive. Existing scalar evaluation/search/fitting, virtual OOP hooks, explicit RNG,
fitness, global admission, generation publication and execution APIs remain intact.
No new numerical dependency, executor or global cache was introduced.

## Executed environment and gates

Jetson ARM64 Linux 6.8.12-1021-tegra, Ubuntu GCC 13.3.0, release-13 libstdc++ headers;
libc6/glibc/libm 2.39-0ubuntu8.8 and libstdc++6/libtsan2
14.2.0-4ubuntu2~24.04.1. CMake/CTest 4.4.4 at `../.tools/bin/` and exact 3.16.0 at
`../delivery-tools/cmake-min/bin/`. Full logs, input hashes, manifests, environment
and structured record: `out/logs/typed/`.

| Configuration | Full suite | Seconds | Final focused typed checks |
| --- | --- | ---: | --- |
| Release, CMake 4.4.4, warnings as errors | 29/29 | 117.93 | 3/3 |
| Debug, CMake 4.4.4, warnings as errors | 29/29 | 117.92 | 3/3 |
| Debug ASan+UBSan, CMake 4.4.4 | 29/29 | 298.17 | 3/3 |
| Release, exact CMake/CTest 3.16.0 | 29/29 | 77.01 | 3/3 |
| Separate Debug TSan, halt_on_error=1 | selected 3/3 | 0.57 | 3/3 again, 0.52 s |

Full suites tested the final production headers. The last change refined only the
fault test: measure actual allocations before injecting each failure, rather than
assume exactly six allocations. All affected typed contract/overlap/example cases
were rebuilt and rerun in every normal configuration; the TSan selection was also
rebuilt/rerun. Input SHA256 records identify the final files. Earlier qualification
iterations remain separately retained under iteration-1; they are not substituted
for final-header/focused results. Timings include varying concurrent build load,
not performance measurements.

The suite adds typed contracts, overlap and example to Phase 1: **28 portable** cases,
or **29** with the qualified Jetson captured numerical snapshot. The separate TSan
selection is existing engine concurrency, generic OOP parallel, and typed overlap.
This does not claim all 29 cases ran under TSan. Workflow/manifests were updated and
locally validated with actionlint and both manifest checks; no hosted jobs ran for
this candidate. Prior hosted results keep their recorded SHA and coverage scope.
The new typed API is locally qualified here, not newly qualified on libc++, Windows,
AppleClang, Clang18 or hosted x86/ARM runners by workflow authorship alone.

## Contracts exercised

- Distinct scalar/vector[1]/matrix[1,1], owned finite coefficients, exact schema order,
  higher-index variables, unused binding validation, incompatible kinds/shapes,
  malformed postfix arities/fragments and unknown operator requests.
- Independent rectangular fixture A*x+b=[14,8]; A*B=[[1,8],[-6,8]];
  transpose(A)=[[1,-1],[2,0],[3,4]]; dot([2,-1,3],[1,2,3])=9.
  Arithmetic costs are independently checked as 12, 20, 0 and 5 respectively.
- Addition/subtraction, both scaling orders, scalar division/log domains, grouping,
  no-FMA counterexample, first-product signed zero, subnormals/underflow, intermediate
  overflow before cancellation or zero masking, and later-output failure without
  partial publication. Log reference comparisons use justified tolerances.
- Inclusive node/depth/value/constant/binding/work/payload caps and immediately
  excessive requests, dimension/storage/aggregate/work overflow, complete-plan
  runtime admission separate from invalid mathematics. A 10,001-deep transpose chain
  exercises iterative validation, evaluation, export and flat destruction.
- Construction/rvalue/retained-token/schema alias safety, independent copies,
  results surviving scratch and expression destruction, failed derivation/copy
  preservation, and owned expression/fitness-independent values.
- Every observed allocation boundary of the fixed evaluator fixture, including
  work after private intermediate results; bad_alloc propagates unchanged, and a
  subsequent valid call succeeds. Strong assignment of values, expressions, tokens
  and results is checked across different kinds/statuses. This prevents a failed
  result/token copy from publishing a discriminator with no matching payload.
- Concurrent evaluation of one immutable expression with different inputs. A timed
  test-only allocation gate stops both calls inside workspace allocation, proving
  overlapping evaluator invocations; each then performs 100 independent checks,
  including invalid-then-valid calls and retained owned results. No production hook,
  shared scratch, suppression or alternative test interpreter is used.
- Faithful operator/grouping/index/type/shape/row-major export, round-trippable
  binary64 constant spelling (not a parser round trip), signed zero and locale
  independence.

## Public delivery and regression checks

Standalone headers compile as sole includes. Unsafe fast/finite-only compilation
is rejected independently for Expression, TypedValue and TypedExpression; C++
strict-FP requirements still propagate through Genetik::Genetik. Warning-as-error
builds passed. No development sanitizer flags/runtime leaked into installed usage.

Each full suite ran ordinary and relocated installed multi-TU consumers after
removing its disposable producer source/build. Both C++ translation units use the
new headers, and the actual typed quickstart is copied/built/run through the
installed target. Scalar/symbolic/permutation consumers, source inclusion, versions,
private development flags and intentional paths with spaces remain checked. The
OOP consumer's compiler dependency inspection now also excludes TypedValue and
TypedExpression, proving generic use requires no typed headers.

Manual installation to `out/install/typed` and quickstarts in `out/consumers/typed`
succeeded; stdout includes vector[2](14.0,8.0), all independent fixtures, arithmetic=12,
temporary_coefficients=11, and a distinct 11-operation ResourceLimit outcome.
No program writes new root files. Repository and parent workspace hygiene passed.
All 127 frozen evidence hashes, existing genetik files, published asset identities,
main and annotated v0.1.0 remain unchanged. Existing numerical reference tests pass;
no scientific campaign/search replay was needed because no existing implementation
or arithmetic code changed.

## Reproduce

Ordinary supported CMake commands (substitute the local tool paths above here):

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
cmake --build out/build/release -j2
cmake -E chdir out/build/release ctest --output-on-failure
out/build/release/genetik_typed_quickstart
```

Use out/build/debug with Debug; out/build/sanitize with Debug and
GENETIK_SANITIZERS=ON; out/build/minimum with the exact 3.16.0 cmake/ctest.
Use an absolute ctest path with `cmake -E chdir` when the executable is not on PATH.
For separate TSan:

```sh
cmake -S . -B out/build/tsan -DCMAKE_BUILD_TYPE=Debug -DGENETIK_THREAD_SANITIZER=ON -DGENETIK_SANITIZERS=OFF -DGENETIK_BUILD_EXAMPLES=OFF -DGENETIK_WARNINGS_AS_ERRORS=ON
cmake --build out/build/tsan --target genetik_concurrency_tests genetik_oop_consumer genetik_typed_tests -j2
TSAN_OPTIONS=halt_on_error=1 cmake -E chdir out/build/tsan ctest -R 'genetik_concurrency_contracts|genetik_oop_parallel|genetik_typed_overlap' --output-on-failure
python3 scripts/check_hygiene.py --workspace --self-test
```

GENETIK_RECORDED_REFERENCE is a qualified Jetson-only opt-in, not a requirement
for ordinary consumers or other hosts. No new global loss/work admission policy or
promise of cross-platform bit-identical logarithms was introduced.

## Completion and next scope

Typed mathematical expressions can be constructed, inspected, exported, and
evaluated correctly under the documented limits. They are manually constructed
mathematical values, not a typed regression/search engine. Next define immutable
typed datasets and checked aggregate objectives/resource handling, then type-correct
bounded initialization/variation through the existing generic publication engine.
Constant fitting, parsers, scientific extensions, calculus and matrix-equation
search remain separately scoped. Local exceptions return no partial result/work
record; later adapters must keep honest outer attempt accounting on thrown calls.
