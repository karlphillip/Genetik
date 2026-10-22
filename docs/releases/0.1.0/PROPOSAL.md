# Historical first development-release proposal

This preserves the earlier preparation record; its publication status and commands
are historical, not the current delivery proposal. Relative documentation links
are adjusted for this archival location. v0.1.0 was subsequently published.

Planned package **0.1.0**, annotated tag **v0.1.0**, GitHub **pre-release**, title
**Genetik 0.1.0 — development pre-release**. No default-branch update, PR, tag or
GitHub release is created during preparation. Package version remains the root
CMake project's existing 0.1.0; no published prior version/tag was found.

Review [the release-note body](NOTES.md),
[proposed PR title/description](PULL_REQUEST.md),
[migration](../../MIGRATION.md), [support policy](../../SUPPORT.md), and
[executed hosted qualification](../../HOSTED_QUALIFICATION.md).

## Candidate and integration

Preparation and authorized pushes stay on `qualification/hosted-0.1.0`.
The review/completion packet records the exact final SHA, workflow links, archive
identity and checksum **after** stable documents are committed. They are generated
under ignored out/release-review/; no follow-up bookkeeping commit is needed to
make a document contain its own SHA. Existing hosted results at e21c196 remain
valid for that revision, not automatic proof of a later revision.

Remote main was `b8059f08ececa9bda6b2c19f35a925c070052f56`, an ancestor of this
branch. Propose a review PR with the prepared text, followed by an ordinary
**fast-forward push of the exact approved candidate to main**, retaining its SHA.
Do not use a GitHub merge/squash/rebase operation that creates a different commit.
Check main is still that baseline before integrating; if it moved, stop/review the
new integration and qualify any resulting different commit before tagging. Never
force-push or rewrite history. Require the main push's full matrix to pass for that
same SHA before tag/release creation. Publication approval must cover these exact
operations; if a different candidate is necessary, return to review.

## Source asset

Prefer **Genetik-0.1.0-source.tar.gz**, top directory **Genetik-0.1.0/**, plus
**SHA256SUMS**. Generate from the final commit, not a dirty working tree:

```sh
mkdir -p out/release-review/assets
git archive --format=tar --prefix=Genetik-0.1.0/ "$CANDIDATE" | gzip -n > out/release-review/assets/Genetik-0.1.0-source.tar.gz
(cd out/release-review/assets && sha256sum Genetik-0.1.0-source.tar.gz > SHA256SUMS)
python3 scripts/check_source_archive.py --archive out/release-review/assets/Genetik-0.1.0-source.tar.gz --commit "$CANDIDATE" --workspace out/release-review/archive-check
```

`CANDIDATE` is the exact reviewed SHA in the completion packet. The inspection
validates Git archive identity, paths and required distribution files, extracts
without .git, writes a per-file manifest, then runs the existing isolated installed
consumer fixture **from the archive**. It configures/installs with extras off, runs
ordinary/relocated multi-TU consumers, scalar/symbolic quickstarts and the generic
OOP consumer, deletes disposable producer trees, and checks source inclusion,
version compatibility, strict FP and absence of producer/runtime leaks. Supply
--cmake/--cxx only to select a particular locally installed toolchain. The source
package itself does not require Python/Git to install with development extras off.
All tracked accepted reports remain in the source archive; none install by default.
No ignored builds/output or .git is packaged. Preserve the unchanged MIT license.

The checksum binds the exact proposed upload. GitHub's automatically generated tag
archives are additional service-generated downloads, not promised byte-identical
to this explicitly checked gzip asset. No platform binaries, installer or research
outputs are separately proposed as release assets.

## Proposed publication commands — only after specific approval

The final packet resolves CANDIDATE and paths below. GitHub CLI is optional
maintainer tooling, not a library dependency; release creation needs an authenticated
account with repository write access. SSH pushes alone do not establish REST/CLI
release authorization. Do not put credentials into source or the review packet.

```sh
# Require authenticated release-capable maintainer tooling before integration:
gh auth status
# Review PR, only if included in final approval:
gh pr create --repo karlphillip/Genetik --base main --head qualification/hosted-0.1.0 --title "Prepare Genetik 0.1.0 development pre-release" --body-file out/release-review/PR_BODY.md
# Integrate the exact approved SHA; never force:
git fetch origin main
test "$(git rev-parse FETCH_HEAD)" = b8059f08ececa9bda6b2c19f35a925c070052f56
git merge-base --is-ancestor FETCH_HEAD "$CANDIDATE"
git push git@github.com:karlphillip/Genetik.git "$CANDIDATE":refs/heads/main
# Wait for all three main-branch jobs, including ARM64 TSan, to pass at CANDIDATE.
# Only then create the annotated tag and GitHub pre-release:
test -z "$(git ls-remote --tags origin refs/tags/v0.1.0)"
git tag -a v0.1.0 "$CANDIDATE" -m "Genetik 0.1.0 development pre-release"
test "$(git rev-parse 'v0.1.0^{commit}')" = "$CANDIDATE"
git push git@github.com:karlphillip/Genetik.git refs/tags/v0.1.0
(cd out/release-review/assets && sha256sum -c SHA256SUMS)
gh release create v0.1.0 out/release-review/assets/Genetik-0.1.0-source.tar.gz out/release-review/assets/SHA256SUMS --repo karlphillip/Genetik --verify-tag --prerelease --latest=false --title "Genetik 0.1.0 — development pre-release" --notes-file docs/releases/0.1.0/NOTES.md
```

No command above is executed by this preparation task. Do not create a release if
the candidate/tag differs, checks fail, main moved, or the asset checksum changed.
Do not add an unreviewed status/version commit before tagging; that creates a new
candidate requiring new qualification. Preparation-time "unreleased" statements
are checkpoint status; actual publication state is the approved tag/release page.

## Remaining scope

This is a first development release for the recorded Linux combinations, not a
promise for Windows/MSVC, AppleClang or libc++. Parallel detailed diagnostics remain
unsupported; user callback concurrency remains explicit. Search/fitting are heuristic,
prime extrapolation is measured evidence rather than a theorem, and cross-runtime
bit identity is not promised. See the linked contracts for all ownership, worker,
numerical, budget and publication limitations. These documented scope limits are
not blockers for the proposed Linux development pre-release.
