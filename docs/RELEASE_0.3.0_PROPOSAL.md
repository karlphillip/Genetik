# Genetik 0.3.0 development-release proposal

Status: unreleased; upload and hosted qualification on qualification/checkpoint-preflight
are approved and the implementation matrix passed. PR, default-branch integration,
tag and publication still require final delivery approval. The completed exact
candidate and archive identities belong in the ignored hosted-preflight packet,
so this document does not require a commit to record its own SHA.

Review [notes](releases/0.3.0/NOTES.md), [migration](releases/0.3.0/MIGRATION.md),
[operational qualification](CHECKPOINT_PREFLIGHT_QUALIFICATION.md) and
[checkpoint contract](RUN_CHECKPOINTING.md). The complete delivery range starts at
published v0.2.0, f9f300ed376370b2f476722e4a92888004d8358d: exact-prefix prime
search plus formatting, observability, codec/storage, cancellation and resume work.
The earlier calculus features remain the v0.2.0 baseline, not new0.3 mathematical work.

Proposed version/tag/title:0.3.0, annotated v0.3.0, "Genetik 0.3.0 — development
pre-release"; GitHub prerelease=true, latest=false. Prefer a source archive
Genetik-0.3.0-source.tar.gz and SHA256SUMS, generated with git archive/gzip -n from
the final committed candidate, compared with every committed blob and verified
without .git through exact CMake3.16 plus installed/relocated consumers.

The approved upload and implementation matrix are recorded in
[hosted qualification](CHECKPOINT_HOSTED_QUALIFICATION.md): GCC13 x64 exact-minimum,
Clang18 x64 ASan+UBSan and GCC13 ARM64 Release/separate TSan. The final report commit
must pass that complete matrix too. Regenerate and verify assets from that exact
commit, then retain its SHA/run/job/asset identities in the ignored completion packet.
Do not extend a passing earlier SHA or its archive to a later candidate.

Only after hosted success, seek separate delivery approval for a PR titled
"Prepare Genetik 0.3.0 operational development pre-release", then ordinary fast-forward
main to the exact reviewed candidate if remote main remains its ancestor. Review
main CI at that SHA before creating/pushing annotated v0.3.0. Preserve all prior
commits/tags/assets. Any divergence requires a revised proposal; do not force,
squash, rebase or silently introduce a different tag candidate.

Publish only after approval, from the existing verified tag using prepared notes,
a temporary draft, asset upload/download/hash verification and explicit pre-release/
not-latest settings. No such action is part of the hosted-qualification authorization.
