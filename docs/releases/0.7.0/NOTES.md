# Genetik 0.7.0 — Advanced Scientific Regression Development Pre-release

This is a local development candidate. It has not been published and must not
be treated as a stable release.

## Candidate scope

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

Coordinate fitting remains the default. The GA does not consult the equation
archive. Mathematical constraints are limited to the documented exact affine
and bounded shared-floor fragment. The S1 derivative evaluator recomputes the
scalar partial evaluator for each parameter. Mathematical symbolic equivalence
under assumptions does not imply bit-exact floating-point equivalence.

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
no general superiority claim. See the S3 benchmark report for every run and
assessment.

## Dependencies and platforms

Ordinary Genetik remains C++20 and standard-library-only. Boost.Multiprecision
1.83 is optional and required only by exact rational clients/tests. LP64
LAPACK is an independent optional scientific target. Python remains optional;
the locally built 0.7.0 wheel is tagged for CPython 3.12 on Linux AArch64 only.
Existing platform limits remain in force. See the qualification reports for
numerical behavior, source and artifact hashes, and the proposed hosted gate.

## Migration

Existing coordinate fitter configuration and defaults remain unchanged. LM is
explicitly opt-in and participates in optimizer configuration identity. Shared
parameter references preserve their serialized identity. Exact rational
feasibility is distinct from selecting and verifying an executable binary64
witness. Consumers of changed header-only APIs must recompile dependent
translation units.

Hosted qualification, remote writes, PR integration, tagging and publication
require separate authorization.

See [migration notes](../../S3_MIGRATION.md) before recompiling consumers.
