# Genetik 0.5.0 hosted qualification proposal

The D7 candidate is a development pre-release of the general-purpose Genetik
GA library with opt-in D5/D6 mathematical APIs. Local preflight is recorded
separately; this file describes hosted qualification and does not authorize
main integration, a PR, tag, or publication.

The exact approval packet is prepared locally after the final commit and
source-archive hash are fixed. It records the candidate SHA, qualification
branch, file/commit summary, passing and failed local gates, the frozen
competitor protocol/results, checkpoint compatibility, source archive SHA256,
and outstanding limitations.

## Proposed hosted workflow

Use the repository's pinned `.github/workflows/ci.yml` against the exact
candidate SHA on `qualification/advanced-mathematics-d7`:

| Hosted job | Toolchain and scope |
|---|---|
| `gcc13-x64-minimum` | GCC 13, Ubuntu 24.04 x86_64, Release, exact CMake 3.16.0, full portable test manifest |
| `clang18-x64-sanitizers` | Clang 18, Ubuntu 24.04 x86_64, Debug, complete ASan+UBSan portable test manifest |
| `gcc13-arm64` | GCC 13, Ubuntu 24.04 ARM64, Release, full portable manifest and required TSan selection repeated three times plus complete integral, discrete, D5 and D6 contracts |

The actual per-run CTest JSON manifests are retained. Expected portable and
TSan manifests are checked by `scripts/check_ci_manifest.py`; exact names and
counts must be read from the final commit's workflow configuration, not copied
from historical D5/D6 counts. The D7 portable list includes the v0.4 scalar and
discrete checkpoint compatibility tests.

The current user instruction authorizes committing and pushing the
qualification branch after local completion. Push may trigger the repository's
normal `on: push` workflow. No manual workflow dispatch, hosted repair, report
push, PR, merge, tag, or publication is included in that authorization.

## Approval boundary

After the exact branch tip and local packet are available, hosted qualification
must use that SHA and retain job logs/artifacts. Any repair changes the SHA and
requires rerunning affected local gates and hosted qualification on the repaired
tip. Main integration, PR creation, tag creation and release publication each
remain separate approval decisions.
