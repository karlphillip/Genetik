# Generic OOP qualification and source organization

Starting HEAD: 8b633264cd2d5ab464facfcceae45c7826832d2d, clean working tree.
Architecture/repair checkpoint: 774c6f9. All work remains local and unreleased.
See [responsibility/extension audit](ARCHITECTURE.md), [layout](ORGANIZATION.md)
and [next concurrency design](CONCURRENCY.md). Existing source API/include/target/
version policy is retained, with one intentional selection-dispatch correction.
No mandatory mathematical dependency was added.

## Qualification commands

From the checkout on the Jetson (ordinary cmake/ctest also work when installed):

```sh
../.tools/bin/cmake -S . -B ../build-release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake -S . -B ../build-debug -DCMAKE_BUILD_TYPE=Debug -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake -S . -B ../build-sanitize -DCMAKE_BUILD_TYPE=Debug -DGENETIK_SANITIZERS=ON -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure
../delivery-tools/cmake-min/bin/cmake -S . -B ../build-min-contracts -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON -DGENETIK_RECORDED_REFERENCE=ON
../delivery-tools/cmake-min/bin/cmake --build ../build-min-contracts -j2
../delivery-tools/cmake-min/bin/cmake -E chdir ../build-min-contracts ../delivery-tools/cmake-min/bin/ctest --output-on-failure
```

These retain the existing builds and exact-minimum tooling from DELIVERY.md.
CMake minimum remains 3.16, version 0.1.0 unreleased. Environment remains Ubuntu
24.04.4, Linux 6.8.12-1021-tegra, ARM64, GCC/libstdc++ 13.3.0, glibc/libm 2.39.
Captured bits remain an opt-in qualification for this environment only.

The full builds compile all standalone public/support headers with warnings as
errors. CTest includes strict-FP negative-build contracts, source-inclusion consumer,
parent unrelated-target isolation, installed scalar/symbolic quickstarts, ordinary
and source-independent relocated multi-TU consumers, version/component checks, and
the new external permutation application. The installed fixture inspects compiler-emitted
include dependencies to prove the permutation application includes no symbolic headers.
No Genetik development sanitizer/profiling runtime requirement leaks downstream.
Sanitizer contracts exercise the custom candidate in the instrumented source target;
the separately installed consumers intentionally use plain package usage requirements.

See qualification-results for logs and exact serial replay captures. Search stdout
is compared to a preserved pre-edit accepted executable; final checks repeat after
source movement. The comparison covers prime seed 2 and strict-arithmetic seed 42
at cadence 1/100,000 losses, plus a short cadence-4 case. No new stochastic campaign
or search-quality claim is introduced. Timing is excluded from equivalence.

## Limits and next implementation

Hosted CI remains configured but unexecuted; no extra compiler, architecture,
standard library or runtime has been qualified. Runtime population concurrency is
absent. The concrete next task is a generic bounded preparation executor with
serial default and real --threads controls, coordinator-owned RNG/publication,
exclusive worker state, explicit shared-budget admission and draining/fault tests.
See CONCURRENCY.md before designing callback copying or promising serial-identical
failure-boundary behavior. No worker backend, cache or optimizer is part of this
milestone.

## Final-tree results

| Gate | Result | Observed suite time |
| --- | --- | ---: |
| Release / CMake 4.4.4 | 22/22 | 85.79 s |
| Debug / CMake 4.4.4 | 22/22 | 82.91 s |
| Debug ASan+UBSan / CMake 4.4.4 | 22/22 | 238.66 s |
| Release / exact CMake+CTest 3.16.0 | 22/22 | 62.77 s |

All four complete builds passed warnings-as-errors and standalone-header targets.
The final portable installed-dependency probe was also repeated in Release (pass,
66.19 s). These suite times are execution evidence, not performance comparisons.
Direct installed permutation execution reports five committed generations and 59
candidate evaluation attempts, including three calls consumed before an injected
failure and eight calls in an abandoned all-invalid successor. Failed selection
and variation consume draws without evaluating candidates. Acceptance does not
require a best schedule or particular stochastic trajectory.

Final post-move prime2/arithmetic42/short-interval4 replay captures compare exactly
to the preserved baseline binary and to the accepted historical snapshot files.
127 accepted report/raw artifact checksums are unchanged; the inventory classifies
all 217 tracked checkpoint files. Routine tests/builds leave no generated files at
the source root. Logs, baseline/final captures, inventory and checksums are under
[qualification-results](qualification-results/README.md). Temporary producers,
consumer trees and command/compile metadata remain under each build's
`tests/genetik-delivery-fixture`; the fixture logs all checks and removed-producer
relocation steps. All user-visible executable names and explicit output options
are retained. No remote delivery or hosted CI run occurred.
