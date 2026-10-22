# Genetik 0.2.0 — development pre-release (proposed, unreleased)

Genetik is a general-purpose C++20 genetic algorithm library with owned candidates,
explicit run randomness, supported runtime-polymorphic behavior, bounded optional
candidate concurrency, and staged generation publication. Mathematical regression
is a reusable specialization of that engine; prime regression is an example.

Since the published 0.1.0 checkpoint, this candidate adds:

- Canonical `out/` output paths, organized campaign sources and workspace hygiene.
- Owned scalar/vector/matrix expressions, bounded strict CPU evaluation, shape-safe
  typed evolution, aggregate regression loss and coordinate coefficient fitting.
- Opt-in Sin/Cos/Exp/Sqrt and integer Powi(0..8); defaults remain unchanged.
- Bounded scalar/typed expression parsing and supplied-formula file evaluation.
- One-site scalar templates with structural variable/operator restrictions,
  complete-model scoring and fitting of inserted constants only.
- First-order scalar input partials, structural activity tracking and explicit
  unavailable-derivative outcomes; supplied value/partial observation regression
  with weighted normalized channels and explicit value anchors.
- Owned root integral models with prescribed bounds and appended local binding,
  deterministic adaptive GK7/15 estimated convergence, integrand discovery and
  coordinate fitting against complete integral observations.

Install with ordinary CMake >=3.16 and link `Genetik::Genetik` through
`find_package(Genetik 0.2 CONFIG REQUIRED)`. C++20, strict FP and Threads requirements
propagate; development sanitizers and warnings do not. The package remains
header-only, standard-library/runtime only, with established include spelling and
source compatibility target `genetik_lib`. See [README](../../../README.md),
[compiled installed quickstarts](../../../examples/quickstarts),
[generic OOP example](../../../tests/oop_consumer), and
[migration guidance](../../MIGRATION.md).

This is a new pre-1.0 minor series. Explicit 0.1 version requests intentionally
reject a 0.2 installation under SameMinorVersion. Recompile all dependent translation
units consistently: the polymorphic regression measurement seam changed class
layout/vtables. No stable binary ABI or cross-version seeded trajectory is promised.

Prior hosted qualification covers cleanup/typed/scientific/parser/template work at
its recorded SHAs. Combined calculus qualification is pending: consult
[the current preparation record](../../CALCULUS_PREFLIGHT.md) and
[support policy](../../SUPPORT.md). Intended Linux matrix: GCC13 x64 with exact
CMake3.16, Clang18 x64 ASan+UBSan using GNU standard-library headers/runtime, and
GCC13 ARM64 with separate TSan. Do not infer libc++, Windows or Apple qualification.

Numerical derivatives implement rounded chain-rule propagation, not symbolic
proofs; unavailable does not prove nondifferentiability. Observation anchors do
not establish global uniqueness. Quadrature success means estimated convergence,
not a rigorous error bound: unsampled narrow features/singularities can be missed.
The approximately 3.55e-14 largest reference-fixture discrepancy is an observation,
not a universal accuracy bound. Root proper integrals only: no nested/improper
integration or differentiation through quadrature. Coordinate fitting and structural
search remain heuristic. Existing unsuccessful scientific/prime seeds and all frozen
campaign results are retained; no global prime formula or extrapolation guarantee
is claimed. Detailed refinement diagnostics remain serial-only. User callbacks
require explicit parallel qualification; `--threads N` does not set CPU affinity.

Proposed distribution: source archive plus SHA256SUMS, annotated v0.2.0 tag,
GitHub pre-release, latest=false. No publication has occurred during preparation.
