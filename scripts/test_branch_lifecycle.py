#!/usr/bin/env python3
"""Disposable-repository qualification for the read-only branch audit and gates."""

from __future__ import annotations

import hashlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from branch_audit import audit, integration_allowed, remote_delete_allowed  # noqa: E402


def run(repo: Path, *args: str) -> str:
    import subprocess

    return subprocess.run(
        ["git", "-C", str(repo), *args], check=True, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    ).stdout.strip()


def commit(repo: Path, name: str, content: str) -> str:
    (repo / name).write_text(content, encoding="utf-8")
    run(repo, "add", name)
    run(repo, "-c", "user.name=Branch Test", "-c", "user.email=branch-test@example.invalid",
        "commit", "-m", f"change {name}")
    return run(repo, "rev-parse", "HEAD")


class BranchLifecycleTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="genetik-branch-audit-")
        self.root = Path(self.temp.name)
        self.repo = self.root / "repo"
        self.repo.mkdir()
        run(self.repo, "init", "-b", "main")
        self.base = commit(self.repo, "base.txt", "base\n")
        run(self.repo, "branch", "--set-upstream-to", "main", "main")
        run(self.repo, "update-ref", "refs/remotes/origin/main", self.base)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def inventory(self) -> dict:
        return audit(self.repo, "refs/remotes/origin/main")

    def relation(self, branch: str) -> str:
        return next(item["relation_to_base"] for item in self.inventory()["branches"]
                    if item["kind"] == "local" and item["name"] == branch)

    def test_fully_merged_and_identical_branches(self) -> None:
        run(self.repo, "branch", "identical")
        run(self.repo, "branch", "merged")
        run(self.repo, "checkout", "merged")
        commit(self.repo, "merged.txt", "included\n")
        merged_sha = run(self.repo, "rev-parse", "HEAD")
        run(self.repo, "checkout", "main")
        run(self.repo, "merge", "--no-ff", "merged", "-m", "integrate merged")
        run(self.repo, "update-ref", "refs/remotes/origin/main", "HEAD")
        self.assertEqual(self.relation("merged"), "fully_merged")
        self.assertEqual(self.relation("identical"), "fully_merged")
        self.assertNotEqual(merged_sha, self.base)

    def test_unmerged_and_divergent_branches(self) -> None:
        run(self.repo, "branch", "unmerged")
        run(self.repo, "branch", "divergent")
        run(self.repo, "checkout", "unmerged")
        commit(self.repo, "candidate.txt", "candidate\n")
        run(self.repo, "checkout", "main")
        commit(self.repo, "main.txt", "main advances\n")
        run(self.repo, "update-ref", "refs/remotes/origin/main", "HEAD")
        run(self.repo, "checkout", "divergent")
        commit(self.repo, "other.txt", "divergent change\n")
        self.assertEqual(self.relation("unmerged"), "divergent")
        self.assertEqual(self.relation("divergent"), "divergent")
        self.assertEqual(next(item["ahead_commits"] for item in self.inventory()["branches"]
                              if item["name"] == "unmerged" and item["kind"] == "local"), 1)

    def test_unmerged_ahead_branch(self) -> None:
        run(self.repo, "branch", "ahead")
        run(self.repo, "checkout", "ahead")
        commit(self.repo, "ahead.txt", "ahead of base\n")
        self.assertEqual(self.relation("ahead"), "ahead_unmerged")

    def test_squash_tree_match_is_reported_but_not_ancestry_proof(self) -> None:
        run(self.repo, "branch", "squashed")
        run(self.repo, "checkout", "squashed")
        commit(self.repo, "feature.txt", "feature\n")
        feature_tree = run(self.repo, "rev-parse", "HEAD^{tree}")
        feature_sha = run(self.repo, "rev-parse", "HEAD")
        run(self.repo, "checkout", "main")
        (self.repo / "feature.txt").write_text("feature\n", encoding="utf-8")
        run(self.repo, "add", "feature.txt")
        run(self.repo, "-c", "user.name=Branch Test", "-c", "user.email=branch-test@example.invalid",
            "commit", "-m", "squash feature")
        run(self.repo, "update-ref", "refs/remotes/origin/main", "HEAD")
        self.assertEqual(run(self.repo, "rev-parse", "HEAD^{tree}"), feature_tree)
        self.assertNotEqual(run(self.repo, "rev-parse", "HEAD"), feature_sha)
        self.assertEqual(self.relation("squashed"), "identical_tree_possible_squash_or_equivalent")
        self.assertFalse(remote_delete_allowed(expected_sha=feature_sha, observed_sha=feature_sha,
                                               integrated=False, active_worktree=False, dirty_worktree=False))

    def test_dirty_active_worktree_is_visible(self) -> None:
        run(self.repo, "branch", "task")
        task_path = self.root / "active-task"
        run(self.repo, "worktree", "add", str(task_path), "task")
        (task_path / "untracked.txt").write_text("keep me\n", encoding="utf-8")
        inv = self.inventory()
        task = next(item for item in inv["branches"] if item["name"] == "task" and item["kind"] == "local")
        self.assertEqual(len(task["active_worktrees"]), 1)
        self.assertTrue(task["active_worktrees"][0]["dirty"])
        self.assertFalse(remote_delete_allowed(expected_sha=self.base, observed_sha=self.base,
                                               integrated=True, active_worktree=True, dirty_worktree=True))

    def test_failed_ci_blocks_integration(self) -> None:
        evidence = {key: True for key in (
            "routine_authorized", "tests_complete", "exact_ci_success", "source_head_unchanged",
            "conflict_free", "review_clear", "scope_verified", "compatible", "provenance_known",
        )}
        self.assertTrue(integration_allowed(evidence))
        for gate in ("tests_complete", "exact_ci_success", "source_head_unchanged", "conflict_free",
                     "review_clear", "scope_verified", "compatible", "provenance_known",
                     "routine_authorized"):
            blocked = dict(evidence)
            blocked[gate] = False
            self.assertFalse(integration_allowed(blocked), gate)
        self.assertFalse(integration_allowed({}))

    def test_changed_remote_head_blocks_sha_leased_deletion(self) -> None:
        expected = "a" * 40
        observed = "b" * 40
        self.assertFalse(remote_delete_allowed(expected_sha=expected, observed_sha=observed,
                                               integrated=True, active_worktree=False, dirty_worktree=False))
        self.assertTrue(remote_delete_allowed(expected_sha=expected, observed_sha=expected,
                                              integrated=True, active_worktree=False, dirty_worktree=False))
        self.assertFalse(remote_delete_allowed(expected_sha=expected, observed_sha=expected,
                                               integrated=True, active_worktree=False, dirty_worktree=True))

    def test_actual_remote_head_change_is_detected_before_delete(self) -> None:
        bare = self.root / "remote.git"
        run(self.root, "init", "--bare", str(bare))
        first = self.root / "first"
        second = self.root / "second"
        run(self.root, "clone", str(bare), str(first))
        run(first, "checkout", "-b", "task")
        (first / "task.txt").write_text("first\n", encoding="utf-8")
        run(first, "add", "task.txt")
        run(first, "-c", "user.name=Branch Test", "-c", "user.email=branch-test@example.invalid",
            "commit", "-m", "initial task")
        run(first, "push", "-u", "origin", "task")
        expected = run(first, "rev-parse", "HEAD")
        run(self.root, "clone", "-b", "task", str(bare), str(second))
        with (second / "task.txt").open("a", encoding="utf-8") as stream:
            stream.write("remote update\n")
        run(second, "add", "task.txt")
        run(second, "-c", "user.name=Branch Test", "-c", "user.email=branch-test@example.invalid",
            "commit", "-m", "advance remote task")
        run(second, "push", "origin", "task")
        observed = run(self.root, "ls-remote", str(bare), "refs/heads/task").split()[0]
        self.assertNotEqual(expected, observed)
        self.assertFalse(remote_delete_allowed(expected_sha=expected, observed_sha=observed,
                                               integrated=True, active_worktree=False, dirty_worktree=False))
        rejected = subprocess.run(
            ["git", "-C", str(first), "push",
             f"--force-with-lease=refs/heads/task:{expected}",
             "origin", ":refs/heads/task"],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        self.assertNotEqual(rejected.returncode, 0, "stale SHA lease must reject remote deletion")
        self.assertEqual(run(self.root, "ls-remote", str(bare), "refs/heads/task").split()[0], observed)

    def test_audit_does_not_modify_repository_state(self) -> None:
        before = run(self.repo, "status", "--porcelain")
        refs_before = run(self.repo, "show-ref", "--heads", "--hash")
        index_path = Path(run(self.repo, "rev-parse", "--git-path", "index"))
        if not index_path.is_absolute():
            index_path = self.repo / index_path
        index_before = hashlib.sha256(index_path.read_bytes()).hexdigest()
        self.inventory()
        self.assertEqual(run(self.repo, "status", "--porcelain"), before)
        self.assertEqual(run(self.repo, "show-ref", "--heads", "--hash"), refs_before)
        self.assertEqual(hashlib.sha256(index_path.read_bytes()).hexdigest(), index_before)


if __name__ == "__main__":
    unittest.main(verbosity=2)
