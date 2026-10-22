# Version, support and release policy

Phase7C bounded root integral models are locally qualified in
[INTEGRAL_QUALIFICATION.md](INTEGRAL_QUALIFICATION.md). Combined calculus hosted
qualification and final development-delivery review are next. Earlier hosted
records cover only their named SHAs, not this later local calculus stack.

Phase 7B supplied value/partial regression is locally qualified in
[DERIVATIVE_REGRESSION_QUALIFICATION.md](DERIVATIVE_REGRESSION_QUALIFICATION.md).
Hosted calculus qualification remains pending until the checkpoint after 7C; no
previous hosted result is extended to this new feature by these local checks.

Phase 7A derivative qualification is local and recorded in
[DERIVATIVE_QUALIFICATION.md](DERIVATIVE_QUALIFICATION.md). Hosted qualification
for the new derivative feature remains pending; earlier hosted claims below retain
their explicitly identified commit scopes.

The authoritative version is `project(GENETIK VERSION 0.1.0 ...)` in the root
CMakeLists.txt. Package metadata derives from it. The original checkout already
declared 0.1.0; there were no local release tags or documented published releases.
Version 0.1.0 was published as a development pre-release at
`f9299ea2cb5136d06e2de8f68bb83ecf460ca369`. Later local changes are not
retroactively qualified by that release. Historical preparation statements below
retain their checkpoint scope; see CLEANUP.md for current local verification.
See CHANGELOG.md for modernization/migration history.

Before 1.0, a minor change (0.1 to 0.2) may introduce documented public source API
incompatibilities. Patches within a minor series preserve the documented public
source API while correcting defects. Numerical or seeded-trajectory changes must
receive explicit release notes, including defect fixes in patches. CMake uses
SameMinorVersion (ordinary version requests, without EXACT): installed 0.1.x accepts an equal/older request in the 0.1 series,
not requests from 0.0 or 0.2 and not newer unavailable patches. EXACT requests must
match exactly. Header-only delivery does not provide a stable binary ABI; recompile
all dependent translation units consistently when changing headers/options.

The version file is ARCH_INDEPENDENT because the package installs no compiled
architecture-specific binaries. This controls package matching, **not** execution
qualification on arbitrary architectures. Compiler/runtime qualification is separate.

## Qualification status

| Configuration | Status |
| --- | --- |
| Jetson ARM64 Linux, GCC/libstdc++ 13.3.0, glibc/libm 2.39; Release, Debug, ASan+UBSan | Locally verified; delivery results in DELIVERY.md. |
| CMake 3.16.0 and 4.4.4 on the Jetson | Locally exercised minimum and current tool; install/relocation/consumption. |
| Ubuntu 24.04 x86-64 GCC 13 Release, CMake 3.16.0 | Hosted verified; see TYPED_SCIENTIFIC_HOSTED_QUALIFICATION.md for the current feature SHA, jobs and runtimes. |
| Ubuntu 24.04 x86-64 Clang 18 GNU frontend + libstdc++, Debug ASan+UBSan | Hosted verified; see TYPED_SCIENTIFIC_HOSTED_QUALIFICATION.md for the current feature SHA, jobs and runtimes. |
| Native Ubuntu 24.04 ARM64 GCC 13 Release | Hosted verified, including separate TSan; see TYPED_SCIENTIFIC_HOSTED_QUALIFICATION.md. |
| Windows/MSVC, clang-cl, AppleClang/macOS, libc++, other compilers/runtimes | Unqualified. Flag-selection logic alone does not qualify them. |

CMake 3.16 remains the minimum; a workspace tool at 4.4.4 does not establish a new
minimum. No CMake requirement was raised. Local minimum tooling was built from the
checksum-verified official 3.16.0 source, with OpenSSL disabled (only a build-tool
choice, not a Genetik library requirement). GCC 13 is the available local compiler. Clang 18 and x86-64 were pending at the
original delivery milestone; the subsequent hosted qualification is recorded below.

Same-seed repeatability is scoped to the **exact library revision/version**, compiler,
standard library, runtime math library/FP environment, input/order, configuration
and execution mode/concurrency and callback qualification (see CONCURRENCY.md). A compiler/libm update can change logarithm rounding,
scores, tie ordering and trajectories. Do not compare different builds as if a seed
were a portable model identifier. CI uses portable domain/math tolerances and
within-build deterministic checks. The captured evaluation-results bit reference
belongs specifically to accepted Jetson/GCC13/libm2.39 execution; it is opt-in via
GENETIK_RECORDED_REFERENCE and is retained unchanged. Qualification on another host
requires investigation/differential evidence, not blanket cross-platform snapshots.

## Release procedure (preparation only; no release performed)

1. Review API migrations and numerical/trajectory changes; update CHANGELOG and
   support evidence. Set the single root project version; review compatibility
   policy if moving to 1.x, and update version-request fixtures for the new package
   version. Do not imply stable ABI.
2. Run the documented Release/Debug/sanitizer contracts, standalone headers, installed
   quickstarts, relocation/multi-TU/version/source-parent checks and minimum CMake.
   Require successful hosted jobs for configurations listed as CI-verified.
3. Inspect a staged install for headers, relocatable CMake metadata and unchanged MIT
   license/attribution. Package no campaign/data/tool artifacts. Reproduce clean use
   without the checkout or producer build, with no development runtime dependencies.
4. Review documentation, versioned source archive contents and checksums. Record
   exact compiler/runtime/build qualification and remaining unsupported platforms.
5. Remote pushing, release tags and publication require a separately requested
   delivery action. None are performed here.

CI workflow uses read-only repository permissions, pinned official action revisions,
bounded jobs and no required stochastic campaigns. Runner labels/tool availability
were checked against [GitHub runner documentation](https://docs.github.com/en/actions/reference/runners/github-hosted-runners),
[Ubuntu x64 image](https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2404-Readme.md)
and [ARM64 image](https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2404-Arm64-Readme.md).
The initial checked-in workflow was configuration only. Executed hosted evidence
is now recorded in HOSTED_QUALIFICATION.md; workflow authorship alone still does
not qualify any additional combination.

## CPU execution qualification update

Opt-in generic worker execution and private built-in refinement are locally tested
with GCC 13.3.0/release-13 standard headers, installed libstdc++6/libtsan2 packages
14.2.0-4ubuntu2~24.04.1 and libc6 2.39-0ubuntu8.8 on the Jetson. Separate TSan
forced-overlap/OOP tests pass ten repetitions each; ASan+UBSan remains a separate
build. See CONCURRENCY_RESULTS.md for exact evidence and limitations. At that local checkpoint, hosted jobs (including ARM64 TSan) had not executed.
The subsequent hosted results below supersede that pending status. Parallel safety of
arbitrary callbacks is not inferred from constness or from this local qualification.

## Hosted qualification preparation

The workflow now asserts its 24-test portable manifest and the separate two-test
TSan selection, explicitly enables development coverage, and records checkout/image/
compiler/header/runtime package versions. No Jetson-specific captured trajectory is
asserted on hosted runners. A focused new contract verifies that requesting detailed
refinement diagnostics together with parallel execution fails clearly before work,
without changing the requested execution mode; serial diagnostics remain supported.
See DELIVERY.md and the local ignored out/hosted-preflight/PREFLIGHT.md for upload
scope, verification and authorization status. That preparation alone did not qualify hosted support; actual execution now
supports the updated table above. The recorded
approximately fourfold prime-refinement result remains a Jetson fixed-work/reused-
pool observation, not a hosted performance claim or general scaling guarantee.


## Executed hosted qualification

[HOSTED_QUALIFICATION.md](HOSTED_QUALIFICATION.md) records successful workflow
39477303495 at `23914b8502dfe4665bf65a2a6e7fa94c8d8e3654`: GCC13 x64 Release with
exact CMake/CTest 3.16.0; Clang18 x64 Debug ASan+UBSan with **libstdc++14 headers**;
and native GCC13 ARM64 Release plus separate Debug TSan. Each passes 24/24 portable
cases; TSan passes its two overlap/OOP cases. All include installed/relocated
multi-TU and generic/symbolic/OOP consumption. The sole local-suite exclusion is
the captured Jetson numerical snapshot, not arithmetic/domain or concurrency tests.

Runner images, exact header/runtime packages, job links, original failures and
repairs are retained in that record. These are selected tested combinations, not
all compiler/CMake/OS permutations. libc++/Windows/Apple platforms remain unqualified.
The final documentation-only tip is also submitted to fresh CI before completion.
0.1.0 remains unreleased; no default-branch or release action was performed.


## First development-release proposal

See [RELEASE_PROPOSAL.md](RELEASE_PROPOSAL.md) and the proposed
[0.1.0 notes](releases/0.1.0/NOTES.md). Version 0.1.0 remains unreleased during
preparation; no tag or GitHub release is created by this review task. The proposed
release is explicitly a GitHub **pre-release**, with a source archive and checksum.

Patch compatibility means documented public **source API** compatibility, not
identical numerical results or seeded trajectories. Compatible defect fixes may
change those results and require explicit notes. Breaking public API changes require
a new minor series before 1.0, matching SameMinorVersion's refusal to match another
minor. This first modernization is not promised source-compatible with the original
unreleased 2021 interfaces; consult MIGRATION.md. No stable binary ABI is promised.


## Phase 1 location/hygiene update

The Phase 1 suite added one repository-hygiene case: 25 portable cases, or 26 with
the qualified Jetson snapshot enabled. Separate TSan still selects only its two
forced-overlap/OOP cases. The previously executed hosted 24-case results remain
historical evidence for their recorded SHAs. The changed workflow uses out/build/
and the updated manifest; no hosted job for this local cleanup is claimed.


## Additive typed evaluation (Phase 2)

The new TypedValue/TypedExpression API is locally qualified on the Jetson/GCC13
configuration in [TYPED_EVALUATION_QUALIFICATION.md](TYPED_EVALUATION_QUALIFICATION.md):
29/29 with the captured snapshot in Release/Debug/ASan/minimum-CMake and three
separate TSan selections including gated typed overlap. The current portable
manifest has 28 cases; its separate TSan manifest has three. Modified hosted jobs
are configured, not executed for this local feature. Previously verified hosted
support applies to the recorded older revisions, not an automatic qualification
of these new headers on every compiler/stdlib. No existing source API or numerical
implementation changed, and the published v0.1.0 assets remain fixed.

## Phase 3 typed regression

Typed problems, aggregate loss, shape-aware structure search and coefficient fitting
are additive local development after the published v0.1.0. See
[TYPED_REGRESSION_QUALIFICATION.md](TYPED_REGRESSION_QUALIFICATION.md) for executed
Jetson gates and exact source/evidence identities. The portable manifest now has32
cases (33 with the qualified captured snapshot), separate TSan selection4. Hosted
jobs are updated but have not executed for this feature. Earlier Linux hosted
qualification does not automatically qualify these new APIs; other platforms remain
as documented above. No dependency, ABI promise or release/version policy changed.

## Phase 4 scientific scalar operators

Sin/Cos/Exp/Sqrt/Powi are locally qualified on the native Jetson/GCC13 configuration
in [SCIENTIFIC_QUALIFICATION.md](SCIENTIFIC_QUALIFICATION.md). The current suite has
33 portable tests (34 with the qualified captured snapshot), with five separate
TSan selections. Release/Debug/ASan+UBSan/exact-CMake3.16 and repeated TSan execution
passed; see retained source/log identities. New math references qualify selected
runtime results within documented tolerances, not cross-platform bitwise libm
behavior. Optional reference regeneration uses development-only mpmath1.2.1;
consumers and ordinary builds do not require it. Hosted jobs have not executed for
the accumulated cleanup/typed/scientific features. Earlier hosted Linux support
remains evidence for its recorded revisions; libc++ and other untested environments
remain unqualified. Published v0.1.0 assets/version policy are unchanged.

## Executed accumulated-feature qualification

The cleanup, typed evaluation/regression/fitting and scientific stack passed the
three-job hosted matrix at `778a30dd0c05fe74a0755dcadbe59bed7e4ebc32`. See
[TYPED_SCIENTIFIC_HOSTED_QUALIFICATION.md](TYPED_SCIENTIFIC_HOSTED_QUALIFICATION.md)
for exact jobs, runner/runtime versions, the33-test manifest and five TSan cases
repeated three times. This supersedes the feature-specific pending statements above
for that identified candidate; earlier reports remain historical. Clang18 uses
libstdc++14 headers/runtime, not libc++. No release/version change is implied.
The report update itself receives a final matrix; its tested SHA/links are kept in
the local completion record rather than creating a self-referential untested tip.

## Phase 5 bounded expression import (local)

ExpressionParsing.hpp/TypedExpressionParsing.hpp add owned scalar/typed text import,
with no new runtime/parser dependency or existing evaluator/search change. See
[PARSING_QUALIFICATION.md](PARSING_QUALIFICATION.md): native Jetson GCC13
Release/Debug/ASan+UBSan/exact-CMake3.16 pass38/38; six focused TSan cases each
pass three times. Portable manifest37 excludes only the captured Jetson snapshot.
The new hosted workflow includes parser coverage but has not executed for Phase5.
Earlier hosted qualification remains scoped to its recorded SHAs. No release,
version/ABI policy or additional compiler/standard-library qualification is implied.

Phase 6A scalar templates have additional local qualification recorded in
[SCALAR_TEMPLATE_QUALIFICATION.md](SCALAR_TEMPLATE_QUALIFICATION.md). This does not
extend hosted qualification to the combined parser/template branch. Existing Linux
hosted evidence and unqualified-platform limitations above retain their original scope.

## Executed Phase 5/6A hosted qualification

The parser/template implementation passed at
`c9080e2a0d1d3a00f10319379375284d6dd2bd28` in workflow
[39662713630](https://github.com/karlphillip/Genetik/actions/runs/39662713630).
[PARSER_TEMPLATES_HOSTED_QUALIFICATION.md](PARSER_TEMPLATES_HOSTED_QUALIFICATION.md)
records GCC13 x64 Release/exact CMake3.16.0, Clang18 x64 Debug ASan+UBSan with
libstdc++14 headers/runtime, and native GCC13 ARM64 Release plus separate Debug
TSan. Each ran41 portable tests; each of seven named TSan cases ran three times.
No qualification repair or numerical/instrumentation change was required.

This supersedes local-only/pending Phase 5/6A statements above for that precise
implementation candidate. The report commit itself receives a final complete matrix;
its exact tested SHA and links are recorded in the ignored completion record.
Clang's GNU headers/runtime do not qualify libc++; Windows/MSVC and macOS/AppleClang
also remain unqualified. Published version/assets and historical evidence stay fixed.
