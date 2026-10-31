# Protecting `main`

The **Protect main** GitHub ruleset keeps reviewed, qualified changes on the
default branch and makes the normal pull request workflow the only integration
path. It is expected to target `main`, require a pull request, require zero
approvals while resolving all review conversations, require up-to-date status
checks, block force-pushes and deletion, and configure no bypass actors.

The required GitHub Actions checks are:

- `gcc13-x64-minimum`
- `clang18-x64-sanitizers`
- `gcc13-arm64`
- `gcc13-x64-optional-math`
- `python-cp312-x86_64`
- `python-cp312-aarch64`

## Normal development lifecycle

Codex fetches `origin/main`, audits refs and worktrees, and creates an isolated
task branch and worktree. It makes a scoped change, runs applicable local
qualification, reviews the diff, commits, and pushes. It opens a pull request
to `main`, verifies the exact tested head SHA, required CI, review state, and
mergeability, then uses a normal merge commit when every gate passes. After
fetching and verifying integration, it removes only that clean task-owned
branch and worktree.

If a rule, check, or merge fails, report the PR, exact commit and failing check
or GitHub error, and preserve the branch and PR for diagnosis. Do not bypass or
weaken protection to force integration.

Routine, compatible maintenance changes may follow this workflow after their
qualification gates pass. Research branches and unresolved experiments remain
isolated; integrating research or retiring research refs requires explicit
user authorization and preserved evidence. Mathematical or numerical semantic
changes, incompatible APIs or checkpoints, failed qualification, and ambiguous
provenance are not routine integrations.

Verify protection through GitHub's ruleset API or repository rules page and
inspect PR checks and mergeability. Compare the active ruleset's target,
configuration, and required check names with this document; never disable a
rule or use a bypass to validate the workflow.
