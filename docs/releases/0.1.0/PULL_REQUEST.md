Title: Prepare Genetik 0.1.0 development pre-release

Genetik's original prototype becomes an installable, header-only C++20 generic
GA library with supported OOP candidates/behavior hooks, explicit run-owned
randomness, finite fitness states, bounded work and safe generation publication.
Symbolic regression reuses that engine for typed bounded expressions, strict
arithmetic, optional constant fitting and opt-in candidate concurrency. Prime
regression remains an application example; all unsuccessful campaign results remain.

The canonical package target is Genetik::Genetik; source genetik_lib and established
header spelling are preserved. Public migrations, callback safety, numerical scope,
pre-1.0 compatibility and qualified Linux combinations are documented. No stable ABI
or stochastic recovery guarantee is introduced. Version 0.1.0 remains unreleased
until the separately approved tag and GitHub pre-release publication.

Validation: successful hosted GCC13 x64/exact CMake 3.16.0, Clang18 x64/ASan+UBSan,
and native GCC13 ARM64 plus separate TSan; 24 portable tests per normal job and two
TSan executions, with installed/relocated multi-TU generic/symbolic/OOP consumers.
The local suite's additional Jetson-only captured snapshot is intentionally excluded
from hosted comparisons. Final candidate SHA/run and source archive checksum are
supplied in the release review packet; no earlier run is represented as testing a
later commit. Archive-based consumption is additionally checked without .git.

Integration proposal: fast-forward main to the exact reviewed candidate, preserving
its qualified SHA and complete history. Do not use a merge/squash/rebase button that
creates a different target. If main changes or a different integration commit is
required, stop and qualify that new candidate through the full matrix/archive gate
before requesting/creating v0.1.0. This PR text is prepared locally only; opening or
merging the PR and publishing the release require the final approval.
