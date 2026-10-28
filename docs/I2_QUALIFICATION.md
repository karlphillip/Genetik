# I2 local qualification record

This report applies only to the exact I2 candidate SHA named in the final
completion report. It separates inherited 0.5.0/I1 results from gates executed
against the final 0.6.0 source tree.

## Candidate scope

Version 0.6.0 is based on I1 `83c07793b3410b01101003d9626d954f4db961ab`, a
direct child of the published 0.5.0 baseline
`f95a204e25a6de36a54230be0cba750bde5e2ff7`. The I2 native source change is the
version update and explicit scalar/discrete checkpoint forward compatibility
from the 0.5.0 codec. Python remains optional. No search algorithm, fitness,
optimizer, mathematical grammar, or default has been changed.

## Final-source gates

| Gate | Toolchain / scope | Outcome | Evidence |
|---|---|---|---|
| CMake 3.16.0 Release portable suite | GCC 13.3, Linux AArch64, 92-test manifest | Passed 92/92; 329.88 s. Initial run 91/92 exposed the installed fixture's stale 0.5 request; correction and complete rerun passed. | `docs/i2-results/logs/release-configure-cmake316.log`, `release-build-cmake316.log`, `release-ctest-cmake316.log`, `release-ctest-cmake316-after-package-fix.log` |
| Debug portable suite | GCC 13.3, Linux AArch64, 92-test manifest | Passed 92/92; 381.69 s. | `docs/i2-results/logs/debug-configure-cmake316.log`, `debug-build-cmake316.log`, `debug-ctest-cmake316.log` |
| ASan+UBSan portable suite | GCC 13.3, Linux AArch64, 92-test manifest | Passed 92/92; 4402.42 s. | `docs/i2-results/logs/asan-configure-cmake316.log`, `asan-build-cmake316.log`, `asan-ctest-cmake316.log` |
| TSan selections and complete contracts | GCC 13, Linux AArch64, CMake 4.4.4, 21-test required manifest plus repeated contracts | Required 21 passed three times (63 executions); additional discrete/checkpoint 4/4, D5 6/6, D6 2/2, integral 1/1. | `docs/i2-results/tsan-manifest.json`, `docs/i2-results/logs/tsan-contracts-repeat3.log`, `tsan-discrete-complete.log`, `tsan-d5-complete.log`, `tsan-d6-complete.log`, `tsan-integral-complete.log` |
| CPython 3.12 AArch64 wheel, sdist and tests | Ubuntu 24.04 AArch64, CPython 3.12.3, GCC 13.3 | Platform-specific `cp312-cp312-linux_aarch64` wheel; 20/20 Python tests passed on installed wheel and installed sdist. SRBench pinned layout/import and bounded evaluator smoke passed after timer lifetime repair. | `docs/i2-results/logs/python-wheel-build-i2-final.log`, `python-wheel-tests-i2-final.log`, `python-sdist-build-i2-final.log`, `python-sdist-tests-i2-final.log`, `srbench-layout-i2.log`, `srbench-import-i2-final.log`, `srbench-evaluator-i2-repaired.log`, `srbench-evaluator-i2-repaired.json` |
| CPython 3.12 x86-64 wheel, sdist and tests | Hosted qualification required | Not locally available | `docs/i2-results/logs/x86-emulation-preflight.log` |
| SRBench pinned adapter/layout and bounded smoke | SRBench `2aabbb60dad41d6e5d52779ce134191a874b756c` | Layout 31 algorithms/45 methods; import test passed; bounded official smoke returned `x0` and finite test MSE `6.708655972948872e-33`. Initial failures exposed a dangling deadline capture and are retained; repaired extension and regression test pass. | `docs/i2-results/logs/srbench-layout-i2.log`, `srbench-import-i2-final.log`, `srbench-evaluator-i2-final.log`, `srbench-evaluator-i2-repaired.log`, `srbench-evaluator-i2-repaired.json` |
| Repository-scoped hygiene | Release, Debug and ASan+UBSan CTest; dedicated workspace audit | Repository hygiene passed in each full suite and a final standalone audit. Workspace audit identifies parent-workspace entries `docs`, `examples`, and user-owned `session.md`; none were deleted or modified. | Full CTest logs; `docs/i2-results/logs/repository-hygiene-final.log`, `workspace-hygiene.log` |
| Python extension ASan+UBSan smoke | GCC 13.3, CPython 3.12.3 AArch64, Debug optional module | Instrumented extension built and completed finite fit/predict. LeakSanitizer was disabled only for Python shutdown allocations; initial runtime staging failures are retained. | `docs/i2-results/logs/python-asan-configure.log`, `python-asan-build.log`, `python-asan-smoke-passed.log`, `python-asan-smoke.log.gz`, `python-asan-smoke-retry.log` |
| Workflow lint | Local pinned `actionlint` binary | Passed after correcting Python matrix artifact naming. | `docs/i2-results/logs/actionlint-i2.log` |

Historical I1 minimum-CMake evidence was 59/59 non-hygiene plus a separate
hygiene pass; it is not combined into the I2 92/92 final-source result.

## Platform limits

The local host is Linux AArch64, Ubuntu 24.04, GCC 13.3, CPython 3.12.3. A
Linux x86-64 container preflight failed because host emulation is unavailable;
no privileged system change was attempted. x86-64 must pass the hosted matrix
before release qualification. No Windows, macOS, CPython 3.11, or CPython 3.13
support is claimed.

Wheel compatibility claims will be limited to actual wheel tags and dependency
audits. Host-specific wheels will not be relabeled as manylinux artifacts.

## Native/Python contracts

The installed Python wheel and sdist each passed 20 tests, including native
facade parity at workers 1 and 2, estimator validation and repeat-fit tests,
expression-export references including `x1` versus `x10`, and cooperative
deadline behavior. The pinned SRBench adapter passed its layout/import checks
and official bounded smoke after fixing a dangling reference capture in the
native timer lambda. Earlier failing smoke logs remain retained. A finite-input
numerical comparison is not a proof of global symbolic equivalence. Python
`max_time` is cooperative, not a hard deadline.

The Python sdist explicitly excludes generated `docs/i2-results` artifacts and
logs while retaining the I2 guides, license, example and SRBench adapter. Two
independent builds from the packaging-fix source produced byte-identical
archives; the final-candidate SHA and archive checksum are in the artifact
manifest.

The scikit-learn `check_estimator` suite was executed in addition to the focused
tests: 31 checks passed and 11 did not. The unsupported or intentionally
restricted contracts include complex/object input (the API accepts finite
real numeric arrays), sparse matrices, pickling the native fitted object, and
the check suite's generic training-target accuracy on a small bounded search.
Non-string DataFrame column labels are also rejected. This is not full
scikit-learn estimator-check compliance; see
`docs/i2-results/logs/sklearn-checks-i2.log` for every check outcome.

## Evidence integrity

Raw logs and structured manifests under `docs/i2-results/` are retained,
including failures. Release archive and wheel hashes are recorded in
`docs/i2-results/artifact-manifest.json` and `SHA256SUMS` after the exact
candidate source commit is established.
