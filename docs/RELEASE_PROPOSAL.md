# Genetik 0.2.0 development-delivery proposal

The complete agreed implementation scope has passed the implementation hosted matrix;
see [CALCULUS_HOSTED_QUALIFICATION.md](CALCULUS_HOSTED_QUALIFICATION.md). The final
report tip and its exact source assets must also pass before delivery review. Proposed version
**0.2.0**, annotated tag **v0.2.0**, title **Genetik 0.2.0 — development pre-release**,
GitHub pre-release **true**, latest **false**. Version is unreleased. Main, v0.1.0
and its published assets remain unchanged. The historical first-release proposal
is preserved in [releases/0.1.0/PROPOSAL.md](releases/0.1.0/PROPOSAL.md).

Review [release notes](releases/0.2.0/NOTES.md),
[PR title/body](releases/0.2.0/PULL_REQUEST.md), [migration](MIGRATION.md),
[support](SUPPORT.md) and [calculus preflight](CALCULUS_PREFLIGHT.md).
The ignored `out/runs/hosted-calculus/` packet supplies exact candidate SHAs,
both commit ranges, manifests, qualification links and asset hashes. A prior passing
SHA does not qualify a later documentation tip. Complete the matrix on the final
report commit, then record links in ignored output without another tracked commit.

## Proposed integration and publication (not authorized by preparation)

Remote main was f9299ea2cb5136d06e2de8f68bb83ecf460ca369 during preflight and is
an ancestor of this stack. Open the prepared PR from qualification/calculus into
main only after delivery approval. Immediately recheck remote main, require its
current tip to be an ancestor, and use an ordinary non-forced fast-forward push of
the approved SHA. Do not squash, rebase or create an extra merge commit. If main
moves incompatibly, return to review; qualify any resulting new commit before tags.
Require the main-branch matrix to pass at the approved SHA before tagging.

Prepare from the final committed source:

```sh
mkdir -p out/runs/hosted-calculus/review/assets
git archive --format=tar --prefix=Genetik-0.2.0/ "$CANDIDATE" | gzip -n > out/runs/hosted-calculus/review/assets/Genetik-0.2.0-source.tar.gz
(cd out/runs/hosted-calculus/review/assets && sha256sum Genetik-0.2.0-source.tar.gz > SHA256SUMS)
python3 scripts/check_source_archive.py --archive out/runs/hosted-calculus/review/assets/Genetik-0.2.0-source.tar.gz --commit "$CANDIDATE" --workspace out/runs/hosted-calculus/review/archive-check
```

The archive checker compares every file and executable mode to committed Git blobs, excludes
output/build trees and unsafe paths, extracts without .git, and executes isolated
ordinary/relocated installed multi-TU consumers, actual new quickstarts, generic
OOP consumption, versions/components, public requirements and source inclusion.
No Git/Python is required for ordinary library-only installation from the archive.
Regenerate assets if the candidate changes; never attach an older archive to a new SHA.

Exact proposed later remote operations (resolve CANDIDATE in the reviewed packet):

```sh
gh pr create --repo karlphillip/Genetik --base main --head qualification/calculus --title 'Prepare Genetik 0.2.0 development pre-release' --body-file out/runs/hosted-calculus/review/PR_BODY.md
git fetch origin main
git merge-base --is-ancestor FETCH_HEAD "$CANDIDATE"
git push git@github.com:karlphillip/Genetik.git "$CANDIDATE":refs/heads/main
# Wait for all required main jobs at exactly CANDIDATE, then:
git tag -a v0.2.0 "$CANDIDATE" -m 'Genetik 0.2.0 development pre-release'
git push git@github.com:karlphillip/Genetik.git refs/tags/v0.2.0
gh release create v0.2.0 out/runs/hosted-calculus/review/assets/Genetik-0.2.0-source.tar.gz out/runs/hosted-calculus/review/assets/SHA256SUMS --repo karlphillip/Genetik --verify-tag --prerelease --latest=false --title 'Genetik 0.2.0 — development pre-release' --notes-file docs/releases/0.2.0/NOTES.md
```

Verify remote annotated tag resolution and downloaded asset hashes. A temporary
draft for asset verification may be proposed at final review. None of these remote
actions is executed during local preflight; qualification-branch upload and targeted fixes are already authorized. PR/main/tag/
release actions still require the separate final delivery approval. No additional mathematical feature
phase precedes the combined qualification and delivery checkpoint.
