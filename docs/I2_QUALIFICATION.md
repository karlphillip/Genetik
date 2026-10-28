# I2 local qualification record

The release source candidate is `93e89fa296511ffb4136158214d270becd2f2d77`.
The final local branch tip adds only the artifact manifest/checksums and
corresponding qualification evidence. This report separates inherited
0.5.0/I1 results from gates executed against the 0.6.0 source candidate.
The full native Release/Debug/ASan+UBSan/TSan matrix ran at
`f242d7f1153c4445e125d8521af8e61bc96a2582`; the only later source-tree
change before `93e89fa` was Python-sdist exclusion metadata plus documentation
and retained packaging evidence. The C++ and Python implementation files are
identical across these source SHAs.

## Candidate scope

Version 0.6.0 is based on I1 `83c07793b3410b01101003d9626d954f4db961ab`, a
direct child of the published 0.5.0 baseline
`f95a204e25a6de36a54230be0cba750bde5e2ff7`. The C++ library source updates
version metadata and explicitly enables scalar/discrete checkpoint reading
from compatible 0.5.0 codecs. The optional Python facade fixes a dangling
cooperative-timeout deadline capture. Python remains optional. No search
algorithm, fitness, optimizer, mathematical grammar, or production default has
changed.

## Final-source gates

| Gate | Toolchain / scope | Outcome | Evidence |
|---|---|---|---|
| CMake 3.16.0 Release portable suite | GCC 13.3, Linux AArch64, 92-test manifest | Passed 92/92; 329.88 s. Initial run 91/92 exposed the installed fixture's stale 0.5 request; correction and complete rerun passed. | `docs/i2-results/logs/release-configure-cmake316.log`, `release-build-cmake316.log`, `release-ctest-cmake316.log`, `release-ctest-cmake316-after-package-fix.log` |
| Debug portable suite | GCC 13.3, Linux AArch64, 92-test manifest | Passed 92/92; 381.69 s. | `docs/i2-results/logs/debug-configure-cmake316.log`, `debug-build-cmake316.log`, `debug-ctest-cmake316.log` |
| ASan+UBSan portable suite | GCC 13.3, Linux AArch64, 92-test manifest | Passed 92/92; 4402.42 s. | `docs/i2-results/logs/asan-configure-cmake316.log`, `asan-build-cmake316.log`, `asan-ctest-cmake316.log` |
| TSan selections and complete contracts | GCC 13, Linux AArch64, CMake 4.4.4, 21-test required manifest plus repeated contracts | Required 21 passed three times (63 executions); additional discrete/checkpoint 4/4, D5 6/6, D6 2/2, integral 1/1. | `docs/i2-results/tsan-manifest.json`, `docs/i2-results/logs/tsan-contracts-repeat3.log`, `tsan-discrete-complete.log`, `tsan-d5-complete.log`, `tsan-d6-complete.log`, `tsan-integral-complete.log` |
| CPython 3.12 AArch64 wheel, sdist and tests | Ubuntu 24.04 AArch64, CPython 3.12.3, GCC 13.3, CMake 3.16.0 | Platform-specific `cp312-cp312-linux_aarch64` wheel; 20/20 Python tests passed on installed wheel and installed sdist built from source SHA `93e89fa`. Two sdist builds were byte-identical. | `docs/i2-results/logs/python-wheel-build-final-candidate.log`, `python-wheel-tests-final-source.log`, `python-sdist-build-final-a.log`, `python-sdist-build-final-b.log`, `python-sdist-tests-final-source.log`, `python-wheel-quickstart-final-source.log`, `python-sdist-quickstart-final-source.log` |
| CPython 3.12 x86-64 wheel, sdist and tests | Hosted qualification required | Not locally available | `docs/i2-results/logs/x86-emulation-preflight.log` |
| SRBench pinned adapter/layout and bounded smoke | SRBench `2aabbb60dad41d6e5d52779ce134191a874b756c` | Layout 31 algorithms/45 methods; import passed; final installed-wheel smoke returned `x0` and finite test MSE `6.708655972948872e-33` in 0.0020 s. Initial failures exposed the dangling deadline capture and are retained. | `docs/i2-results/logs/srbench-layout-i2.log`, `srbench-import-i2-final.log`, `srbench-evaluator-i2-final.log`, `srbench-evaluator-i2-repaired.log`, `srbench-evaluator-final-source.log`, `srbench-evaluator-final-source.json` |
| Repository-scoped hygiene | Release, Debug and ASan+UBSan CTest; dedicated workspace audit | Repository hygiene passed in each full suite and a final standalone audit. Workspace audit identifies parent-workspace entries `docs`, `examples`, and user-owned `session.md`; none were deleted or modified. | Full CTest logs; `docs/i2-results/logs/repository-hygiene-final.log`, `workspace-hygiene-final.log` |
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

The initial scikit-learn 1.5.2 `check_estimator` run recorded 31 passes and 11
failures in `logs/sklearn-checks-i2.log`. Inspection identified input
convention defects and native-object serialization limitations. Input handling
was corrected without changing search behavior: complex arrays now raise the
expected `ValueError`, convertible numeric object arrays are accepted, sparse
inputs give an explicit unsupported-input message, non-string DataFrame column
labels fall back to stable `x0`, `x1`, ... names, and missing `y` raises the
documented `ValueError` form. Focused Python tests pass for these changes.

A full rerun against the corrected wrapper and pinned scikit-learn 1.5.2
reported 40 passed and 2 failed. The remaining failures are
`check_estimators_pickle` and its `readonly_memmap=True` variant: fitted
estimators hold a native C++ facade that has no serialization contract. This
is an unsupported persistence feature, not a prediction or fitting failure;
the package does not claim pickling compatibility. The three initial
`check_regressors_train` score-threshold failures passed in the rerun; their
initial discrepancy was not reproduced, and no search-policy change was made.
The exact rerun output is retained in
`docs/i2-results/logs/sklearn-i2-repaired.log`. Genetik is not fully
scikit-learn estimator-check compliant.

## Distribution artifacts

The exact source commit and all three artifact identities are recorded in
[`artifact-manifest.json`](i2-results/artifact-manifest.json). The local
assets are under `out/tmp/i2-artifacts/final-delivery/` with a separate
`SHA256SUMS`; its checksum is retained in
`docs/i2-results/release-assets-SHA256SUMS.sha256`. The evidence files under
`docs/i2-results/` have their own `SHA256SUMS` and sidecar. The native source
archive is verified against every Git tree entry in source commit `93e89fa`.
The Python sdist was
built twice from the same source/configuration and the archives were byte
identical. The wheel tag is `cp312-cp312-linux_aarch64`; `auditwheel` reports
the `manylinux_2_39_aarch64` policy floor, but the wheel remains host-tagged and
was not relabeled. It depends on the platform's glibc/libstdc++ system
libraries. The source/wheel build toolchain is CPython 3.12.3, GCC 13.3 and
CMake 3.16.0. No x86-64 artifact has been produced locally.

## Evidence integrity

Raw logs and structured manifests under `docs/i2-results/` are retained,
including failures. Artifact hashes are recorded in the artifact manifest and
the release-assets checksum file; the evidence checksum file independently
covers all retained records under `docs/i2-results/`.
