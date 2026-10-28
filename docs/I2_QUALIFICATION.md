# Genetik 0.6.0 I2 qualification

This report records the I2 source and hosted qualification. The release is an
interoperability release; Python support is optional and native C++ behavior
remains authoritative. The branch's current source candidate is the commit
`7411f2771973226317452533e533c43fee815ebb`; it descends from I1
`83c07793b3410b01101003d9626d954f4db961ab` and published 0.5.0
`f95a204e25a6de36a54230be0cba750bde5e2ff7`.

The earlier `93e89fa296511ffb4136158214d270becd2f2d77` package manifest in
`i2-results/artifact-manifest.json` is a retained *pre-repair snapshot*. It is
not the final source or artifact identity. After that snapshot, Python input
validation and estimator conventions were repaired, and the hosted workflow
was corrected to inspect wheel metadata and install test environments outside
the checkout. The repair-validation build at 7411f27 includes those changes.
The C++ engine, mathematical operators, search policies and native public API
were not changed by those repairs. The exact final evidence-commit tree must
also pass the workflow before this qualification is considered complete.

## Hosted repair-validation run

Run `40736957740` completed successfully at source SHA
`7411f2771973226317452533e533c43fee815ebb`:

| Job | Result | Verified scope |
|---|---|---|
| [GCC 13 x86-64, CMake 3.16.0](https://github.com/karlphillip/Genetik/actions/runs/40736957740/job/122987979718) | Passed | Release, verified 92-test manifest, 92/92 portable tests, installed/relocated consumers and quickstarts. |
| [Clang 18 x86-64 ASan+UBSan](https://github.com/karlphillip/Genetik/actions/runs/40736957740/job/122987979496) | Passed | Instrumented build and link; portable manifest; 92/92 tests. |
| [GCC 13 ARM64](https://github.com/karlphillip/Genetik/actions/runs/40736957740/job/122987979555) | Passed | Release 92/92; required 21 TSan selections each repeated three times (63 executions); full integral 1/1, discrete/checkpoint 4/4, D5 6/6 and D6 2/2 TSan contracts. |
| [CPython 3.12 x86-64](https://github.com/karlphillip/Genetik/actions/runs/40736957740/job/122987979105) | Passed | Host-tagged wheel and canonical sdist installed outside checkout; 21/21 tests per artifact; quickstart, parity and pinned SRBench checks. |
| [CPython 3.12 AArch64](https://github.com/karlphillip/Genetik/actions/runs/40736957740/job/122987979406) | Passed | Host-tagged wheel and canonical sdist installed outside checkout; 21/21 tests per artifact; quickstart, parity and pinned SRBench checks. |

The native job logs are retained as `logs/hosted-final-*.log.gz`; the Python job
logs include wheel/sdist tags and hashes, tests, outside-checkout imports and
SRBench smoke results. The qualified pinned SRBench revision is
`2aabbb60dad41d6e5d52779ce134191a874b756c`. This is adapter/layout and bounded
smoke qualification, not a scientific SRBench campaign or ranking.

The five-job result above validates the repaired source commit. A final full
matrix is required on the exact subsequent documentation/evidence branch tip.
The delivery report must identify that run and SHA; this repair-validation run
is not a substitute for it.

## Native and Python behavior

The native C++ library remains C++20, CMake >=3.16, and exports
`Genetik::Genetik`. The Python build is optional; a Python-disabled C++ build
and installed consumers are part of the native checks. Python estimator calls
the same C++ regression facade and engine for population evolution, fitting,
fitness, expression evaluation, RNG and work accounting. Python does not
implement a second evolutionary algorithm. `max_time` is cooperative; it is
not a hard real-time deadline. On cancellation, the facade only exposes a
valid committed candidate when one exists.

The hosted wheel/sdist tests cover native/Python deterministic parity at
workers 1 and 2, NumPy conversion and validation, repeated fit, prediction,
expression export (including `x1`/`x10` identity), model lifetime and
cooperative cancellation. The SRBench smoke verifies the pinned method layout,
estimator import and model exporter. A finite set of numerical comparisons
does not establish universal symbolic equivalence.

## scikit-learn estimator checks

Pinned scikit-learn 1.5.2 initially reported 31 passed and 11 failed. The
repaired rerun reported 40 passed and 2 failed. All eleven initial failures
were reviewed individually:

| Check | Final disposition |
|---|---|
| `check_complex_data` | Fixed input-contract defect. Genetik is real-valued; complex data now receives the expected `ValueError`. |
| `check_dtype_object` | Fixed. Convertible numeric object arrays are accepted; nonnumeric objects raise `TypeError`. |
| `check_estimator_sparse_array` | Passes after repair. Sparse input remains unsupported and is explicitly rejected. |
| `check_estimator_sparse_matrix` | Passes after repair. Sparse input remains unsupported and is explicitly rejected. |
| `check_estimators_pickle` | Remains unsupported: fitted estimator owns a native C++ facade with no serialization contract. |
| `check_estimators_pickle(readonly_memmap=True)` | Same documented fitted-native-object serialization limitation. |
| `check_regressors_train` | Initial score-threshold failure did not reproduce in the pinned rerun; it passed targeted repeats. Cause of the initial discrepancy is unresolved; search behavior was not changed. |
| `check_regressors_train(readonly_memmap=True)` | Same non-reproduced initial score-threshold discrepancy; targeted repeat passed. |
| `check_regressors_train(readonly_memmap=True, X_dtype=float32)` | Same non-reproduced initial score-threshold discrepancy; targeted repeat passed. |
| `check_regressor_data_not_an_array` | Fixed. Non-string DataFrame labels use stable `x0`, `x1`, … names; string labels are preserved. |
| `check_requires_y_none` | Fixed. Missing target raises the expected `ValueError`. |

Thus 40/42 checks pass; the two remaining failures are the same deliberate
pickling limitation. Genetik does not claim full scikit-learn estimator-check
compliance or fitted-model pickle support. Exact records:
`logs/sklearn-checks-i2.log`, `logs/sklearn-i2-repaired.log`,
`logs/sklearn-targets-repeat.log` and `sklearn-check-disposition.json`.

## Python distributions and artifact identity

The hosted repair-validation run built artifacts from source SHA 7411f27. Its
wheel tags are exactly `cp312-cp312-linux_x86_64` and
`cp312-cp312-linux_aarch64`; these host-specific Linux tags were not renamed
to manylinux. `auditwheel` reports a glibc 2.39 policy floor. The native module
uses system `libc`, `libgcc_s`, `libm` and `libstdc++`; those are not bundled.
This qualification supports Ubuntu 24.04 / CPython 3.12.3 on x86-64 and
AArch64 only. Windows, macOS, other Python minor versions, and broader Linux
compatibility are not claimed.

The standard Python sdist filename is `genetik-0.6.0.tar.gz`. It is distinct
from the native C++ source archive `Genetik-0.6.0-source.tar.gz`. The hosted
Python artifacts from run 40736957740 have these SHA256 values:

| Artifact | SHA256 |
|---|---|
| x86-64 CPython 3.12 wheel | `c1e2675388652577beca8d8cbca43392fdda543ec2651b84bbb916aa81b04bb7` |
| AArch64 CPython 3.12 wheel | `0fe265feddc8fc60d0d74bcc7ec04f0504161ece6f848feb81605d281f00b7ef` |
| Python sdist, `genetik-0.6.0.tar.gz` | `2bc4d553c5a698db279123f4679987cf2abeada9c58cc9d07ab283b69b43bcdd` |
| Native source archive for source SHA 7411f27 | `40a513bdd23040d8bd3087804f7097897d8062cade9247355361414790e35c91` |

The native source archive was verified byte-for-byte against all 1,782 Git
tree entries at 7411f27, extracted without `.git`, and built/tested with
CMake 3.16.0 using ordinary and relocated C++ multi-translation-unit, generic
OOP, symbolic and quickstart consumers. These artifact identities describe
7411f27, not a later evidence commit. Regenerate the release source archive
from the exact eventual tag commit; use final-tip hosted artifacts/hashes for
that same candidate. Do not reuse this table as final release asset metadata.

## Earlier local qualification and limitations

The local final-source matrix recorded earlier remains valid for its stated
source commits: Release, Debug and ASan+UBSan portable suites each passed
92/92; required TSan selections passed three repetitions, with additional D5,
D6, discrete/checkpoint and integral contracts. The local x86 emulation
preflight was unavailable; hosted x86 qualification above now supplies that
platform result. Historical I1 CMake 3.16 evidence was 59/59 non-hygiene plus
a separate hygiene pass, not one combined 60/60 run.

Python limitations include dense real numeric inputs only, no sparse or
complex-valued regression, no fitted-estimator serialization, cooperative
time limits, and no cross-platform wheel claim beyond the tested systems.
Core C++ mathematical and numerical limitations remain those documented for
0.5.0. No search algorithm or performance improvement is claimed.

The pre-repair `artifact-manifest.json` and its checksums are retained as
historical evidence. `preservation.txt` is updated for the authorized branch
pushes; main, release tags and published 0.1.0–0.5.0 assets remain unchanged.
