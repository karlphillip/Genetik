#!/usr/bin/env python3
"""Join complete run outcomes to independent model assessments and summarize."""
import argparse
import json
import statistics
from pathlib import Path


def read_jsonl(path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]


def median(values):
    return statistics.median(values) if values else None


def quartile(values, probability):
    if not values:
        return None
    ordered = sorted(values)
    position = (len(ordered) - 1) * probability
    lower = int(position)
    fraction = position - lower
    upper = min(lower + 1, len(ordered) - 1)
    return ordered[lower] + fraction * (ordered[upper] - ordered[lower])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=Path("out/logs/d7/benchmark-candidates.jsonl"))
    parser.add_argument("--assessments", type=Path, default=Path("out/logs/d7/assessments.jsonl"))
    parser.add_argument("--missing", type=Path, default=Path("out/logs/d7/benchmark-missing-candidates.jsonl"))
    parser.add_argument("--output-dir", type=Path, default=Path("docs/d7/results"))
    args = parser.parse_args()
    runs = read_jsonl(args.manifest)
    assessed = read_jsonl(args.assessments)
    missing = read_jsonl(args.missing)
    assessment_by_key = {(r["framework"], r["task"], r["track"], int(r["seed"])): r for r in assessed}
    missing_by_key = {(r["framework"], r["task"], r["track"], int(r["seed"])): r for r in missing}
    joined = []
    for run in runs:
        key = (run["framework"], run["task"], run["track"], int(run["seed"]))
        assessment = assessment_by_key.get(key)
        absent = missing_by_key.get(key)
        resource = run["supervisor"]
        row = {
            "framework": run["framework"], "task": run["task"], "track": run["track"], "seed": run["seed"],
            "candidate_head": run["candidate_head"], "protocol_sha256": run["protocol_sha256"],
            "process_status": run["status"], "supervisor_outcome": resource["outcome"],
            "wall_seconds": resource["wall_seconds"], "cpu_seconds": resource["cpu_seconds"],
            "peak_rss_gib": resource["peak_rss_gib"], "peak_process_threads": resource["peak_process_threads"],
            "cpu_affinity": resource["cpu_affinity"], "run_directory": run["run_directory"],
            "stdout": resource["stdout"], "stderr": resource["stderr"],
            "candidate_present": assessment is not None,
            "candidate_provenance": assessment.get("candidate_provenance") if assessment else None,
            "expression": assessment.get("expression") if assessment else None,
            "native_candidate_evaluations": assessment.get("native_candidate_evaluations") if assessment else None,
            "native_training_mse": assessment.get("native_training_mse") if assessment else None,
            "hof_loss": assessment.get("hof_loss") if assessment else None,
            "hof_complexity": assessment.get("hof_complexity") if assessment else None,
            "hof_rows": assessment.get("hof_rows") if assessment else None,
            "hof_path": assessment.get("hof_path") if assessment else None,
            "hof_sha256": assessment.get("hof_sha256") if assessment else None,
            "missing_candidate_reason": absent.get("reason") if absent else None,
            "assessment": assessment.get("assessment") if assessment else None,
            "assessment_error": assessment.get("assessment_error") if assessment else None,
        }
        joined.append(row)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "benchmark-seed-results.jsonl").write_text(
        "".join(json.dumps(r, sort_keys=True, allow_nan=False) + "\n" for r in joined), encoding="utf-8")
    groups = []
    for framework in ("Genetik", "PySR", "PyOperon"):
        for task in sorted({r["task"] for r in joined}):
            for track in ("clean", "noise01"):
                rows = [r for r in joined if r["framework"] == framework and r["task"] == task and r["track"] == track]
                metrics = [r["assessment"] for r in rows if r["assessment"]]
                heldout = [m["heldout_normalized_rmse"] for m in metrics if m["heldout_normalized_rmse"] is not None]
                absolute = [m["heldout_rmse"] for m in metrics if m["heldout_rmse"] is not None]
                group = {
                    "framework": framework, "task": task, "track": track, "seeds": [r["seed"] for r in rows],
                    "runs": len(rows), "process_completions": sum(r["process_status"] == "completed" for r in rows),
                    "timeouts_or_failures": sum(r["process_status"] != "completed" for r in rows),
                    "candidate_outputs": sum(r["candidate_present"] for r in rows),
                    "independent_heldout_recoveries": sum(m["operational_recovery"] for m in metrics),
                    "symbolically_equivalent": sum(m["symbolic_assessment"] == "symbolically_equivalent" for m in metrics),
                    "median_heldout_normalized_rmse": median(heldout),
                    "heldout_normalized_rmse_q1": quartile(heldout, 0.25),
                    "heldout_normalized_rmse_q3": quartile(heldout, 0.75),
                    "heldout_normalized_rmse_iqr": quartile(heldout, 0.75) - quartile(heldout, 0.25) if heldout else None,
                    "median_heldout_rmse": median(absolute),
                    "median_expression_nodes": median([m["expression_nodes"] for m in metrics]),
                    "median_expression_depth": median([m["expression_depth"] for m in metrics]),
                    "invalid_training_predictions": sum(m["invalid_training_predictions"] for m in metrics),
                    "invalid_heldout_predictions": sum(m["invalid_heldout_predictions"] for m in metrics),
                    "per_seed_normalized_rmse": [
                        next((r["assessment"]["heldout_normalized_rmse"] for r in rows
                              if r["seed"] == s and r["assessment"] is not None), None)
                        for s in [0, 1, 42]
                    ],
                    "median_wall_seconds": median([r["wall_seconds"] for r in rows]),
                    "median_cpu_seconds": median([r["cpu_seconds"] for r in rows]),
                    "max_peak_rss_gib": max(r["peak_rss_gib"] for r in rows),
                    "native_evaluation_counts": [r["native_candidate_evaluations"] for r in rows if r["native_candidate_evaluations"] is not None],
                    "note": "HOF timeout snapshots are independently assessed candidates but do not change process status. Evaluation counts are framework-native and not cross-framework comparable.",
                }
                groups.append(group)
    output = {
        "candidate_implementation_sha": joined[0]["candidate_head"] if joined else None,
        "protocol_sha256": joined[0]["protocol_sha256"] if joined else None,
        "total_runs": len(joined), "candidate_outputs": sum(r["candidate_present"] for r in joined),
        "missing_candidate_outputs": sum(not r["candidate_present"] for r in joined),
        "process_outcomes_by_framework": {
            f: {status: sum(r["framework"] == f and r["process_status"] == status for r in joined)
                for status in sorted({r["process_status"] for r in joined if r["framework"] == f})}
            for f in ("Genetik", "PySR", "PyOperon")
        },
        "groups": groups,
    }
    (args.output_dir / "benchmark-summary.json").write_text(json.dumps(output, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({k: output[k] for k in ("total_runs", "candidate_outputs", "missing_candidate_outputs", "process_outcomes_by_framework")}, sort_keys=True))


if __name__ == "__main__":
    main()
