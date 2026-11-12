#!/usr/bin/env python3
"""Bounded deterministic aggregation for campaign and synthetic preflight scores."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from run_matrix import Failure, create_json, sha256_file


def load(path: Path, label: str):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise Failure(f"cannot read {label}: {error}") from error


def aggregate(manifest: dict, scores_doc: dict, scores_path: Path) -> dict:
    if not isinstance(manifest, dict) or manifest.get("schema") not in {
            "genetik-fdi-run-manifest-v2", "genetik-fdi-scorer-manifest-v2",
            "genetik-fdi-aggregator-manifest-v2", "genetik-fdi-aggregate-manifest-v2"}:
        raise Failure("run manifest schema mismatch")
    if not isinstance(scores_doc, dict) or scores_doc.get("schema") != "genetik-fdi-scores-v2":
        raise Failure("score document schema mismatch")
    if scores_doc.get("protocol_id") != manifest.get("protocol_id"):
        raise Failure("score protocol does not match frozen run manifest")
    expected_scorer_manifest_sha=manifest.get("scorer_manifest_sha256")
    if (expected_scorer_manifest_sha is not None and
            scores_doc.get("run_manifest_sha256") != expected_scorer_manifest_sha):
        raise Failure("scores are not bound to the frozen scorer manifest")
    runs = manifest.get("runs")
    scores = scores_doc.get("scores")
    if not isinstance(runs, list) or not runs or not isinstance(scores, list) or len(scores) != len(runs):
        raise Failure("score rows do not cover the frozen run set")
    run_ids = [row.get("run_id") for row in runs]
    score_ids = [row.get("run_id") for row in scores if isinstance(row, dict)]
    if (any(not isinstance(value, str) or not value for value in run_ids) or
            len(set(run_ids)) != len(run_ids) or score_ids != run_ids):
        raise Failure("run/score identity is missing, duplicated, or reordered")
    counts: dict[str, dict[str, int]] = {}
    for run, score in zip(runs, scores):
        for key in ("run_id", "control_id", "method", "seed"):
            if score.get(key) != run.get(key):
                raise Failure(f"score binding mismatch for {run.get('run_id')}:{key}")
        if type(run.get("seed")) is not int or run["seed"] < 0:
            raise Failure("seed must be a nonnegative exact integer")
        if score.get("status") != "scored" or type(score.get("exact")) is not bool:
            raise Failure("unresolved score cannot be aggregated as success")
        entry = counts.setdefault(run["control_id"], {"run_count": 0, "exact_run_count": 0})
        entry["run_count"] += 1
        entry["exact_run_count"] += int(score["exact"] is True)
    summary = scores_doc.get("summary")
    if not isinstance(summary, dict):
        raise Failure("score summary is missing")
    all_scored = summary.get("all_scored") is True and len(scores) == len(runs)
    acceptance = summary.get("acceptance_gate_passed") is True
    report = {
        "schema": "genetik-fdi-report-v2",
        "protocol_id": manifest["protocol_id"],
        "run_manifest_sha256": (manifest.get("protocol_manifest_sha256") or hashlib.sha256(
            json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()).hexdigest()),
        "scores_sha256": sha256_file(scores_path),
        "expected_run_count": len(runs),
        "scored_run_count": len(scores),
        "all_scored": all_scored,
        "acceptance_gate_passed": bool(all_scored and acceptance),
        "per_control": counts,
        "run_ids": run_ids,
    }
    if manifest.get("mode") == "synthetic-preflight":
        report["assessment_source"] = "synthetic"
    return report


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-manifest", required=True)
    parser.add_argument("--scores-input", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args(argv)
    manifest_path = Path(args.run_manifest).resolve()
    scores_path = Path(args.scores_input).resolve()
    report = aggregate(load(manifest_path, "run manifest"), load(scores_path, "scores"), scores_path)
    create_json(Path(args.output), report)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (Failure, KeyError, TypeError, ValueError, OSError) as error:
        raise SystemExit(f"aggregate report failed: {error}")
