#!/usr/bin/env python3
"""Collect returned models and PySR timeout HOF snapshots for independent scoring."""
import argparse
import csv
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
EXPECTED_PROTOCOL = "b0b07d2ae418ca71b024fbaa1374264348d92f2b8290606efda2969a432f1ffd"
EXPECTED_HEAD = "5a95ea47f983d4ba99704fb9ea955b14264d6741"


def read_jsonl(path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=ROOT / "out/logs/d7/benchmark-candidates.jsonl")
    parser.add_argument("--output", type=Path, default=ROOT / "out/logs/d7/assessment-input.jsonl")
    parser.add_argument("--expected-head", default=EXPECTED_HEAD)
    parser.add_argument("--protocol-sha256", default=EXPECTED_PROTOCOL)
    args = parser.parse_args()
    rows = read_jsonl(args.manifest)
    keys = [(r["framework"], r["task"], r["track"], int(r["seed"])) for r in rows]
    expected_keys = {
        (framework, task, track, seed)
        for framework in ("Genetik", "PySR", "PyOperon")
        for task in ("polynomial_x3", "rational_x2", "trig_sine", "exponential_decay", "fractional_power_positive", "kinetic_energy_mv2")
        for track in ("clean", "noise01")
        for seed in (0, 1, 42)
    }
    if len(rows) != 108 or set(keys) != expected_keys or len(keys) != len(set(keys)):
        raise SystemExit(f"manifest grid incomplete or duplicated: {len(rows)} rows")
    if any(r["candidate_head"] != args.expected_head or r["protocol_sha256"] != args.protocol_sha256 for r in rows):
        raise SystemExit("manifest implementation or protocol identity mismatch")

    candidates = []
    missing = []
    for record in rows:
        framework = record["framework"]
        expression = None
        candidate_fields = {}
        if framework == "Genetik":
            adapter = record.get("adapter") or {}
            expression = adapter.get("expression")
            if expression:
                candidate_fields["candidate_provenance"] = "adapter final/best-so-far; original process status retained"
                candidate_fields["native_candidate_evaluations"] = adapter.get("native_candidate_evaluations")
                candidate_fields["native_training_mse"] = adapter.get("native_training_mse")
        elif framework == "PySR":
            run_directory = ROOT / record["run_directory"]
            artifacts = sorted(run_directory.glob("pysr-artifacts/*/hall_of_fame.csv"))
            if artifacts:
                artifact = artifacts[0]
                with artifact.open(newline="", encoding="utf-8") as source:
                    formulas = list(csv.DictReader(source))
                if formulas:
                    best = min(formulas, key=lambda item: (float(item["Loss"]), int(item["Complexity"]), item["Equation"]))
                    expression = best["Equation"]
                    candidate_fields.update({
                        "candidate_provenance": "minimum native HOF loss at timeout; original process status retained",
                        "hof_loss": float(best["Loss"]),
                        "hof_complexity": int(best["Complexity"]),
                        "hof_path": str(artifact.relative_to(ROOT)),
                        "hof_sha256": hashlib.sha256(artifact.read_bytes()).hexdigest(),
                        "hof_rows": len(formulas),
                    })
        if expression:
            candidates.append({**record, **candidate_fields, "expression": expression})
        else:
            missing.append({
                "framework": framework, "task": record["task"], "track": record["track"], "seed": record["seed"],
                "status": record["status"], "run_directory": record["run_directory"],
                "reason": "no expression returned or persisted in the recognized HOF format",
            })
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("".join(json.dumps(r, sort_keys=True, allow_nan=False) + "\n" for r in candidates), encoding="utf-8")
    missing_path = args.output.with_name("benchmark-missing-candidates.jsonl")
    missing_path.write_text("".join(json.dumps(r, sort_keys=True, allow_nan=False) + "\n" for r in missing), encoding="utf-8")
    print(json.dumps({"manifest_rows": len(rows), "candidate_rows": len(candidates), "missing_candidate_rows": len(missing),
                      "candidate_output": str(args.output), "missing_output": str(missing_path)}, sort_keys=True))


if __name__ == "__main__":
    main()
