# Genetik 0.5.0 development pre-release

Genetik remains a general-purpose C++20 genetic-algorithm framework. This
development pre-release adds optional scientific mathematics and discovery
APIs while retaining the established GA defaults and generic OOP use cases.
It is locally and hosted qualified as a development candidate on the D7
qualification branch. The hosted matrix runs automatically on branch push;
main integration, tagging and publication are separate actions and are not
claimed here. See the [current hosted workflow](https://github.com/karlphillip/Genetik/actions/workflows/ci.yml?query=branch%3Aqualification%2Fadvanced-mathematics-d7)
for its exact branch SHA and job results.

## Scope

The release integrates the D5 checked integer and recurrence models, extended
scientific scalar functions, conservative symbolic algebra, derivatives,
dimensions, mathematical constraints and iterator-aware generation with D6
coupled equation systems, supplied-observation ODE/PDE residuals, bounded
second partial derivatives, linear algebra, rank 0–3 tensors, restricted
complex expressions/regression, finite statistics, probability distributions,
special functions and scientific fitting/evolution. See the [D5 guide](../../ADVANCED_MATHEMATICS_D5.md),
[D6 guide](../../SCIENTIFIC_DISCOVERY_D6.md),
[mathematical qualification](../../D7_NUMERICAL_QUALIFICATION.md), and
[competitor report](../../d7/BENCHMARK_REPORT.md) for qualified subscopes and
limitations.

No claim is made that Genetik outperforms PySR, SymbolicRegression.jl,
Operon or another symbolic-regression implementation. Search recovery is
empirical; mathematical correctness of the supported evaluators is qualified
separately. The documented D5/D6 unsuccessful discovery outcomes remain part
of the release evidence.

## Compatibility and requirements

- C++20 and CMake >= 3.16.
- Link `Genetik::Genetik` using the documented installed-package flow.
- Default genetic-algorithm behavior remains established; mathematical
  headers and APIs are opt-in.
- The generic checkpoint loader keeps exact-version behavior. Explicit v0.5
  compatibility readers accept only the documented v0.4.0/v0.3.0 scalar and
  discrete checkpoint contracts. Recompile dependent translation units
  consistently; this is a header-only source release, not a stable binary ABI.
- Optional LP64 LAPACK integration keeps its dependency and qualification
  restrictions. Arbitrary-precision integer support is incomplete and is not
  included as a qualified capability.

## Release state

This source archive is a development pre-release candidate. Its exact branch,
commit, engineering gates, checksums and remaining qualification failures are
listed in the local D7 approval packet. Branch upload is for qualification
only. PR creation, main integration, tags and publication require separate
approval and have not occurred.
