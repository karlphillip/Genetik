# Genetik 0.7.0 — Advanced Scientific Regression

Genetik 0.7.0 is published on GitHub as a development pre-release, not a stable
release. The independently reviewed LM resource-limit and stop-reason fixes are
included in the integrated implementation commit
[`b0b84e5ba5975bf2397e325469e0f45645486228`](https://github.com/karlphillip/Genetik/commit/b0b84e5ba5975bf2397e325469e0f45645486228).

## Release scope

- Shared Real64 parameter identities in mixed numerical expressions.
- Analytic parameter Jacobians and optional Levenberg–Marquardt fitting.
- Optional exact rational and bounded quantized-constraint reasoning.
- A narrow domain-aware symbolic cancellation that preserves its assumptions.
- An output-only bounded training-loss/complexity Pareto equation archive.

## Mathematical examples

The qualified shared-parameter models include `f(x)=a sin(bx+c)`,
`f(x)=a x^b`, `f(x)=a exp(-bx)`, and `f(x)=a/(1+b x^2)`. In the mixed
expression API, repeated references to one parameter identity contribute to
the same Jacobian column. For example, the supplied topology
`a sin(a x)` differentiates both occurrences with respect to the same `a`.
The derivative evaluator reports invalid domains, unavailable derivatives and
resource limits explicitly; it does not switch to finite differences.

The equation archive can be used after fitting to retain a training-only
accuracy/complexity trade-off:

```cpp
genetik::EquationArchive archive({64, 4096, 65536});
archive.insert({1e-8, 5, true, 2, 24,
                "a*(x^2)+b", "a x² + b", "x is real"});
const auto jsonl = archive.toJsonLines();
```

Callers provide the authoritative loss, complexity, constants description,
canonical expression, display string and assumptions. A canonical string is
the structural duplicate key; matching sampled predictions do not establish
symbolic equivalence. Display text may contain Unicode or caller-generated
LaTeX, but the archive does not derive either representation.

Coordinate fitting remains the default; Levenberg–Marquardt is optional.
Parameter fitting optimizes numeric parameters for a supplied fixed expression
topology and does not perform autonomous mathematical discovery. The GA does not
consult the equation archive, which is output-only. Mathematical constraints are
limited to the documented exact affine and bounded shared-floor fragment.
Exact rational feasibility does not guarantee an executable binary64 witness.
The S1 derivative evaluator recomputes the scalar partial evaluator for each
parameter. Mathematical symbolic equivalence under assumptions does not imply
bit-exact floating-point equivalence.

## Research interpretation

Fixed-topology parameter recovery, mathematical correctness and autonomous
structure discovery are separate outcomes. The retained LM fixed-topology
results are strong for their small supplied models; the two-seed evolutionary
comparison was mixed. The fixed-topology Wythoff run verifies the supplied
successor expression and is not autonomous formula discovery.

The candidate-specific frozen external matrix completed all 108 runs. Genetik
emitted and independently assessed 32 equations with 20 held-out threshold
recoveries (17/36 processes completed). PySR timed out on all 36 processes;
its separately assessed persisted Hall-of-Fame snapshots recovered 30/36 held-
out cases. PyOperon timed out on all 36 and returned no model. These finite-seed
results are limited to the pinned grammar and 120-second allowance and support
no general superiority claim. See the [S3 benchmark report](../../S3_SCIENTIFIC_BENCHMARKS.md)
for the protocol, per-task results, and interpretation.

## Dependencies and platforms

Ordinary Genetik remains C++20 and standard-library-only. Boost.Multiprecision
1.83 is optional and required only by exact rational clients/tests. LP64
LAPACK is an independent optional scientific target. Python remains optional.
Hosted wheel and sdist qualification passed on Ubuntu 24.04 with CPython 3.12.3
for both x86-64 and AArch64. Python qualification is limited to CPython 3.12 on
Linux x86-64 and ARM64. Wheels are platform-tagged
`cp312-cp312-linux_x86_64` and `cp312-cp312-linux_aarch64`; they are not
manylinux wheels. Other Python minor versions and operating systems remain
unqualified. See the [S3 release preparation report](../../S3_RELEASE_PREPARATION.md)
for exact hosted jobs, artifact identities, and checksums.

Native hosted qualification also passed with GCC 13/Release and exact CMake
3.16.0 on Ubuntu 24.04 x86-64, Clang 18/Debug ASan+UBSan on Ubuntu 24.04
x86-64, and GCC 13/Release on Ubuntu 24.04 AArch64 with a separate TSan
qualification. These tested configurations do not qualify Windows, macOS,
AppleClang, libc++, other compiler/runtime versions, or arbitrary Linux images.
The ARM64 job ran its selected TSan qualification; full-suite TSan coverage was
not claimed. The external benchmark results do not establish general
superiority over PySR.

## Hosted qualification

The six-job workflow [41170361397](https://github.com/karlphillip/Genetik/actions/runs/41170361397)
passed at implementation SHA `b0b84e5ba5975bf2397e325469e0f45645486228`.
It includes GCC 13 x86-64 with CMake 3.16.0, Clang 18 x86-64 ASan+UBSan,
GCC 13 AArch64 and its TSan gate, optional mathematical dependencies, and
CPython 3.12 wheels/sdists on both architectures. The run retains source,
sdist, wheel, native diagnostic, and optional-math artifacts. Its source archive
was built from the exact tested commit; documentation-only release preparation
changes require fresh checks on the resulting final commit.

## Migration

Existing coordinate fitter configuration and defaults remain unchanged. LM is
explicitly opt-in and participates in optimizer configuration identity. Shared
parameter references preserve their serialized identity. Exact rational
feasibility is distinct from selecting and verifying an executable binary64
witness. Consumers of changed header-only APIs must recompile dependent
translation units.

The published `v0.7.0` tag identifies the qualified release source. Existing
v0.6.0 consumers and scalar/discrete checkpoints have explicit forward-reader
coverage; generic checkpoint decoding remains exact-version. Recompile
translation units that include changed header-only APIs. Python installation is
available from the attached source distribution and CPython 3.12 Linux wheels;
PyPI publication is not part of this release.

See [migration notes](../../S3_MIGRATION.md) before recompiling consumers.
