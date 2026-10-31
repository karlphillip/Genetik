---
name: genetik-branch-lifecycle
description: Manage the start, qualification, integration, and cleanup of Genetik source-changing tasks while preserving research branches and user work.
---

# Genetik branch lifecycle

Read and apply this skill at the **start and end of every source-changing task** in this repository. `AGENTS.md` invokes it automatically. Task-specific instructions and the exception rules below override routine automatic integration. This workflow does not authorize publication, tagging, or changes to GitHub protection settings.

## Safety rules

- Never reset, clean, stash, overwrite, or delete uncommitted user work. If provenance or ownership is uncertain, stop and report it.
- Never force-push or rewrite `main`, a research branch, or a task branch.
- Preserve research evidence, failed qualification branches, and unrelated worktrees. A branch being old or having an empty diff is not sufficient evidence to delete it.
- Never infer PR integration or CI success from ancestry alone. Verify the PR, exact tested head SHA, all required checks, review/blocking state, and mergeability with GitHub.
- Do not modify or move tags, publish releases/packages, or modify published assets unless the task explicitly authorizes that specific action.
- Use `scripts/branch_audit.py` for a read-only inventory. It does not fetch refs or contact GitHub; fetch first and separately verify remote PR/CI state.

## START: isolate the task before editing

1. From the repository, fetch current refs and verify the base:

   ```sh
   git fetch origin
   git rev-parse origin/main
   git status --short --branch
   python3 scripts/branch_audit.py --repo . --base origin/main
   git worktree list --porcelain
   git branch -avv
   ```

2. Inspect all tracked, untracked, and ignored output relevant to the task. Read the audit's unique commits, local/remote divergence, and active/dirty worktrees. Do not reuse another task's branch or worktree. Preserve any existing modifications in place; if they make safe isolation impossible, stop and ask how to proceed.
3. Select a unique branch name such as `fix/<slug>`, `feature/<slug>`, or `chore/<slug>`. If it already exists, choose another name; never reset it. Record the exact `origin/main` starting SHA and the user's constraints in the task notes.
4. Create a task-owned worktree from the fetched SHA and verify its branch and cleanliness before implementation:

   ```sh
   git worktree add out/worktrees/<unique-slug> -b <unique-branch> <starting-sha>
   git -C out/worktrees/<unique-slug> status --short --branch
   git -C out/worktrees/<unique-slug> rev-parse HEAD
   ```

No source edit begins until this isolated branch exists. If a worktree cannot be created safely, retain existing state and report the blocker.

## IMPLEMENT: qualify and preserve evidence

1. Implement only the requested scope. Follow `AGENTS.md`, preserve public and checkpoint compatibility, and do not begin a new mathematical/research milestone unless specifically requested.
2. Run the task's applicable deterministic tests, full required suite, and requested qualification. A routine task is not integration-ready while required validation is missing or failing.
3. Preserve useful failure reproductions and qualification evidence in the repository's established location. Do not replace historical evidence with a later result.
4. Review the diff for unrelated files, credentials, generated binaries, omitted dependencies, public API or checkpoint changes, and user modifications. Check `git diff --check`, worktree status, and staged content.
5. Commit the completed implementation on the task branch. Record its exact source SHA. Push that branch and verify GitHub's branch head is exactly that SHA before opening/updating a PR.

## INTEGRATE: routine, qualified changes only

Routine source tasks have standing authorization to integrate after every gate below passes. This standing authorization excludes research and the explicit exceptions that follow.

1. Open a PR targeting `main`; describe scope, compatibility, evidence, supported platforms, limitations, and migration impacts. Keep it Draft until implementation and required qualification are complete.
2. Verify the PR source ref still equals the tested source SHA, the target is current `main`, all required hosted checks passed for that exact SHA, no blocking review/requested changes remain, and GitHub reports no conflicts. Inspect the final diff again for scope/provenance.
3. Confirm no exception applies. If all gates pass, mark the PR ready and merge with a normal merge commit (`gh pr merge --merge`). Do not squash or rebase if doing so would lose the development history expected by the task.
4. Fetch `origin/main`; record the merge SHA and verify the source commit is an ancestor of it and the intended tree is present. Do not force-push `main`.

Automatic integration is **prohibited** if tests/qualification fail or are incomplete; work is exploratory research with unresolved results; a public API or checkpoint format is incompatible; mathematical/numerical semantics change significantly; conflicts or blocking reviews remain; the task forbids merging; publication/tag/release authorization is needed; or implementation/provenance is ambiguous. Keep the branch and report the blocker. Ask for direction when needed; do not delete the branch or silently resolve user changes.

## CLEANUP: only after verified integration

1. Verify integration first: fetched `origin/main` contains the source SHA as an ancestor, the merge commit is recorded, and the task tree is present.
2. Confirm the task branch and its task-owned worktree are clean. Never remove a dirty, locked, unknown, or shared worktree. Remove only the task-owned worktree after its path and branch are verified.
3. Delete the local branch with safe deletion (`git branch -d <branch>`). If it is not safely deletable, stop and investigate; do not use `-D`.
4. Before remote deletion, fetch the exact branch and capture its current SHA. Confirm it is the same source SHA already integrated, has no new commits, and has no active/dirty worktree. Use a SHA lease:

   ```sh
   git push origin --force-with-lease=refs/heads/<branch>:<expected-sha> :refs/heads/<branch>
   ```

   The lease must match the branch head observed immediately before deletion. If the remote head changed, the lease fails: preserve the branch and investigate. Do not use `gh pr merge --delete-branch` because this workflow requires a checked SHA lease.
5. Fetch refs and rerun the read-only audit. Verify `main` is clean and synchronized with `origin/main`, the completed task branch is gone, and no unrelated or research worktree/ref was removed. Report the final main SHA and any retained branches.

## Read-only branch audit

Run after fetching refs, from any repository worktree:

```sh
python3 scripts/branch_audit.py --repo . --base origin/main
python3 scripts/branch_audit.py --repo . --base origin/main --json
```

The helper reports local and fetched remote refs, SHA, ancestry/tree relation, ahead/behind counts, commits unique by ancestry, patch-equivalent commits, tracking ref, active worktrees, and dirty worktree status. `identical_tree_possible_squash_or_equivalent` is only a lead: it does not prove a reviewed/qualified PR. The helper deliberately cannot verify GitHub reviews, CI, task ownership, or branch protection. Inspect those separately before integration or deletion. If the base ref is absent, fetch it explicitly; the helper will not fetch for you.

Qualify helper/policy changes against disposable repositories with:

```sh
python3 scripts/test_branch_lifecycle.py
```

The scenarios include merged and identical branches, unmerged and divergent commits, dirty and active worktrees, squash/tree equivalence, incomplete/failed CI, changed remote heads, and confirmation that audit is read-only.
