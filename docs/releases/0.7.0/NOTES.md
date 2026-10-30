# Genetik 0.7.0 — Advanced Scientific Regression Development Pre-release

This is a local development candidate. It has not been published and must not
be treated as a stable release.

## Candidate scope

- Shared Real64 parameter identities in mixed numerical expressions.
- Analytic parameter Jacobians and optional Levenberg–Marquardt fitting.
- Optional exact rational and bounded quantized-constraint reasoning.
- A narrow domain-aware symbolic cancellation that preserves its assumptions.
- An output-only bounded training-loss/complexity Pareto equation archive.

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
successor expression and is not autonomous formula discovery. External D7
results are inherited measurements from their recorded 0.5.0 candidate SHA;
they are not represented as runs on this S1/S2 candidate.

## Dependencies and platforms

Ordinary Genetik remains C++20 and standard-library-only. Boost.Multiprecision
is optional and required only by exact rational clients/tests. LP64 LAPACK is
an independent optional scientific target. Python remains optional. See the
qualification reports for platform limits, numerical behavior, incomplete
gates, and the exact source/artifact identities once they are finalized.

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
