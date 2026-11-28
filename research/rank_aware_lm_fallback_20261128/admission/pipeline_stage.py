#!/usr/bin/env python3
"""Synthetic stage adapter for the bounded rank-aware-fallback pipeline preflight."""
import hashlib
import json
import pathlib
import sys


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(path):
    return json.loads(pathlib.Path(path).read_text(encoding="utf-8"))


def write(path, value):
    pathlib.Path(path).write_bytes(canonical(value) + b"\n")


def main():
    stage, manifest_path, workdir_path = sys.argv[1:]
    manifest = read(manifest_path)
    workdir = pathlib.Path(workdir_path)
    runs = manifest["runs"]
    bindings = [{key: row[key] for key in ("run_id", "control_id", "method", "seed")} for row in runs]
    preflight_id = manifest["preflight_id"]
    outputs_path = workdir / "candidate-outputs.json"
    receipts_path = workdir / "verified-receipts.json"
    scores_path = workdir / "independent-scores.json"
    report_path = workdir / "scientific-report.json"
    if stage == "runner":
        controls = {row["id"]: row for row in manifest["controls"]}
        outputs = []
        for run in runs:
            control = controls[run["control_id"]]
            training_path = pathlib.Path(control["training"]["path"])
            training = read(training_path)
            # The synthetic control is a constant-zero response. The runner
            # sees its training artifact only; the distinct assessment rows
            # remain unavailable until the scorer stage.
            prediction = training["rows"][0]["expected"]
            outputs.append({
                **{key: run[key] for key in ("run_id", "control_id", "method", "seed")},
                "training_artifact": control["training"]["id"],
                "training_sha256": sha(training_path.read_bytes()),
                "predictions": [prediction],
                "synthetic_expression": "0",
            })
        write(outputs_path, outputs)
    elif stage == "verifier":
        outputs = read(outputs_path)
        receipts = [{
            **{key: row[key] for key in ("run_id", "control_id", "method", "seed")},
            "candidate_sha256": sha(canonical(row)),
            "status": "verified",
        } for row in outputs]
        write(receipts_path, receipts)
    elif stage == "scorer":
        controls = {row["id"]: row for row in manifest["controls"]}
        outputs = {row["run_id"]: row for row in read(outputs_path)}
        receipts = {row["run_id"]: row for row in read(receipts_path)}
        scores = []
        for run in runs:
            control = controls[run["control_id"]]
            assessment_path = pathlib.Path(control["assessment"]["path"])
            assessment = read(assessment_path)
            output = outputs[run["run_id"]]
            receipt = receipts[run["run_id"]]
            score = output["predictions"] == [row["expected"] for row in assessment["rows"]]
            if receipt["candidate_sha256"] != sha(canonical(output)):
                raise RuntimeError("candidate receipt mismatch")
            scores.append({
                **{key: run[key] for key in ("run_id", "control_id", "method", "seed")},
                "assessment_artifact": control["assessment"]["id"],
                "assessment_sha256": sha(assessment_path.read_bytes()),
                "checked_observations": len(assessment["rows"]),
                "exact": score,
            })
        write(scores_path, scores)
    elif stage == "aggregator":
        scores = read(scores_path)
        per_control = {}
        for row in scores:
            summary = per_control.setdefault(row["control_id"], {"run_count": 0, "exact_run_count": 0})
            summary["run_count"] += 1
            summary["exact_run_count"] += int(row["exact"])
        write(report_path, {
            "preflight_id": preflight_id,
            "assessment_source": "synthetic",
            "run_count": len(runs),
            "scored_run_count": len(scores),
            "exact_run_count": sum(row["exact_run_count"] for row in per_control.values()),
            "per_control": per_control,
            "status": "passed",
            "run_ids": [row["run_id"] for row in runs],
            "run_bindings": bindings,
        })
    else:
        raise RuntimeError("unknown pipeline stage")
    write(workdir / f"{stage}.status.json", {
        "status": "passed", "preflight_id": preflight_id, "run_bindings": bindings,
    })


if __name__ == "__main__":
    main()
