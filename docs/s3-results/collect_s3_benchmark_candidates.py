#!/usr/bin/env python3
"""Extract candidate equations from frozen S3 run records for independent scoring."""
import argparse
import csv
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("candidates", type=Path)
    parser.add_argument("missing", type=Path)
    args = parser.parse_args()
    candidates, missing = [], []
    for line in args.input.read_text(encoding="utf-8").splitlines():
        run = json.loads(line)
        expression = None
        provenance = None
        extra = {}
        adapter = run.get("adapter") or {}
        if run["framework"] == "Genetik" and adapter.get("expression"):
            expression = adapter["expression"]
            provenance = "best-so-far Genetik adapter output, including cancellation snapshots"
            extra = {key: adapter.get(key) for key in (
                "native_training_mse", "native_candidate_evaluations",
                "native_training_loss_attempts", "native_training_row_attempts",
            )}
        elif run["framework"] == "PySR":
            hof_dir = Path(run["run_directory"]) / "pysr-artifacts" / (
                f"{run['task']}-{run['track']}-{run['seed']}"
            )
            hof_path = hof_dir / "hall_of_fame.csv"
            if hof_path.is_file():
                with hof_path.open(newline="", encoding="utf-8") as stream:
                    rows = list(csv.DictReader(stream))
                usable = [row for row in rows if row.get("Equation") and row.get("Loss")]
                if usable:
                    row = min(usable, key=lambda item: (
                        float(item["Loss"]), int(float(item["Complexity"])), item["Equation"]
                    ))
                    expression = row["Equation"]
                    provenance = "persisted PySR Hall-of-Fame snapshot from timed-out run"
                    extra = {
                        "hof_loss": float(row["Loss"]),
                        "hof_complexity": int(float(row["Complexity"])),
                        "hof_path": str(hof_path),
                    }
        if expression:
            candidates.append({
                "framework": run["framework"], "task": run["task"],
                "track": run["track"], "seed": run["seed"],
                "expression": expression, "candidate_provenance": provenance,
                **extra,
            })
        else:
            missing.append({
                "framework": run["framework"], "task": run["task"],
                "track": run["track"], "seed": run["seed"],
                "reason": "no emitted adapter model or recognized persisted HOF equation",
            })
    args.candidates.write_text(
        "".join(json.dumps(row, sort_keys=True, allow_nan=False) + "\n" for row in candidates),
        encoding="utf-8",
    )
    args.missing.write_text(
        "".join(json.dumps(row, sort_keys=True, allow_nan=False) + "\n" for row in missing),
        encoding="utf-8",
    )
    print(json.dumps({"assessable_candidates": len(candidates), "missing_candidates": len(missing)}, sort_keys=True))


if __name__ == "__main__":
    main()
