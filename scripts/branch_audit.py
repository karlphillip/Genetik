#!/usr/bin/env python3
"""Read-only inventory of Git branches and worktree integration state."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path
from typing import Any


def git(repo: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(repo), *args],
        check=check,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def integration_allowed(evidence: dict[str, bool]) -> bool:
    """Fail closed unless every routine-integration gate is positively verified."""
    required = (
        "routine_authorized",
        "tests_complete",
        "exact_ci_success",
        "source_head_unchanged",
        "conflict_free",
        "review_clear",
        "scope_verified",
        "compatible",
        "provenance_known",
    )
    return all(evidence.get(key) is True for key in required)


def remote_delete_allowed(
    *, expected_sha: str, observed_sha: str, integrated: bool,
    active_worktree: bool, dirty_worktree: bool,
) -> bool:
    """Fail closed on changed remote heads, unproven integration, or unsafe worktrees."""
    return bool(
        expected_sha
        and expected_sha == observed_sha
        and integrated
        and not active_worktree
        and not dirty_worktree
    )


def _worktrees(repo: Path) -> list[dict[str, Any]]:
    raw = git(repo, "worktree", "list", "--porcelain").stdout
    entries: list[dict[str, Any]] = []
    current: dict[str, Any] = {}
    for line in raw.splitlines() + [""]:
        if not line:
            if current:
                path = Path(current["path"])
                status = git(repo, "-C", str(path), "status", "--porcelain", "--untracked-files=all", check=False)
                current["exists"] = path.exists()
                current["dirty"] = bool(status.stdout.strip()) if status.returncode == 0 else None
                current["status_error"] = status.stderr.strip() if status.returncode else None
                entries.append(current)
                current = {}
            continue
        key, _, value = line.partition(" ")
        if key == "worktree":
            current["path"] = value
        elif key == "HEAD":
            current["head"] = value
        elif key == "branch":
            current["branch"] = value.removeprefix("refs/heads/")
        elif key == "detached":
            current["detached"] = True
        elif key == "bare":
            current["bare"] = True
        elif key == "locked":
            current["locked"] = value or True
        elif key == "prunable":
            current["prunable"] = value or True
    return entries


def audit(repo: Path, base_ref: str = "origin/main") -> dict[str, Any]:
    repo = repo.resolve()
    base_result = git(repo, "rev-parse", "--verify", base_ref, check=False)
    if base_result.returncode:
        raise ValueError(f"base ref {base_ref!r} is unavailable: {base_result.stderr.strip()}")
    base = base_result.stdout.strip()
    base_tree = git(repo, "rev-parse", f"{base}^{{tree}}").stdout.strip()
    worktrees = _worktrees(repo)
    by_branch: dict[str, list[dict[str, Any]]] = {}
    for worktree in worktrees:
        if "branch" in worktree:
            by_branch.setdefault(worktree["branch"], []).append(worktree)

    refs = git(repo, "for-each-ref", "--format=%(refname) %(objectname)", "refs/heads", "refs/remotes").stdout
    branches: list[dict[str, Any]] = []
    for line in refs.splitlines():
        ref, sha = line.split(" ", 1)
        if ref.endswith("/HEAD"):
            continue
        is_remote = ref.startswith("refs/remotes/")
        name = ref.removeprefix("refs/heads/").removeprefix("refs/remotes/")
        ancestry = git(repo, "merge-base", "--is-ancestor", sha, base, check=False)
        base_is_ancestor = git(repo, "merge-base", "--is-ancestor", base, sha, check=False)
        tree = git(repo, "rev-parse", f"{sha}^{{tree}}").stdout.strip()
        ahead = int(git(repo, "rev-list", "--count", f"{base}..{sha}").stdout.strip())
        behind = int(git(repo, "rev-list", "--count", f"{sha}..{base}").stdout.strip())
        equivalent = git(repo, "cherry", base, sha).stdout.splitlines()
        unique = git(repo, "log", "--format=%H %s", f"{base}..{sha}").stdout.splitlines()
        active = by_branch.get(name, []) if not is_remote else []
        if ancestry.returncode == 0:
            relation = "fully_merged"
        elif sha == base:
            relation = "identical_commit"
        elif tree == base_tree:
            relation = "identical_tree_possible_squash_or_equivalent"
        elif base_is_ancestor.returncode == 0:
            relation = "ahead_unmerged"
        else:
            relation = "divergent"
        branches.append({
            "name": name,
            "kind": "remote" if is_remote else "local",
            "ref": ref,
            "sha": sha,
            "relation_to_base": relation,
            "ahead_commits": ahead,
            "behind_commits": behind,
            "patch_equivalent_commits": [line[1:] for line in equivalent if line.startswith("+")],
            "patch_equivalent_in_base": [line[1:] for line in equivalent if line.startswith("-")],
            "unique_commits": unique,
            "active_worktrees": active,
            "remote_tracking_ref": None if is_remote else git(repo, "rev-parse", "--abbrev-ref", f"{name}@{{upstream}}", check=False).stdout.strip() or None,
        })
    return {
        "repository": str(repo),
        "base_ref": base_ref,
        "base_sha": base,
        "worktrees": worktrees,
        "branches": branches,
        "notes": [
            "Read-only inventory; this command does not fetch, switch, delete, merge, or modify refs/files.",
            "Git ancestry and tree equality are evidence only. Verify PR integration, exact CI SHA, cleanliness, and remote-head leases before cleanup.",
            "A matching tree can indicate squash/equivalent integration but does not prove review or qualification.",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path.cwd(), help="repository or worktree path")
    parser.add_argument("--base", default="origin/main", help="already-fetched base ref (default: origin/main)")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args()
    try:
        result = audit(args.repo, args.base)
    except (ValueError, subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(f"Repository: {result['repository']}\nBase: {result['base_ref']} ({result['base_sha']})")
        for wt in result["worktrees"]:
            print(f"WORKTREE {wt['path']} branch={wt.get('branch', '(detached)')} head={wt['head']} dirty={wt['dirty']}")
        for branch in result["branches"]:
            print(f"{branch['kind'].upper():6} {branch['name']} {branch['sha']} {branch['relation_to_base']} "
                  f"ahead={branch['ahead_commits']} behind={branch['behind_commits']} "
                  f"active_worktrees={len(branch['active_worktrees'])}")
            for commit in branch["unique_commits"]:
                print(f"  UNIQUE {commit}")
        for note in result["notes"]:
            print(f"NOTE: {note}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
