#!/usr/bin/env python3
"""Freeze deterministic polynomial selections, then assess against sealed-from-search data."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(binary, *args):
    completed = subprocess.run([str(binary), *args], check=True, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return json.loads(completed.stdout)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: compare_conservation_polynomial.py PATH_TO_BINARY")
    binary = Path(sys.argv[1]).resolve()
    # The selector process is given only a system's training-group generator; it
    # returns and serializes its selected expression before any assessment input
    # or saved PR candidate is read by this orchestrator.
    selected = []
    for system in ("harmonic", "quartic"):
        result = run(binary, "polynomial-select", system)
        if not result["found"]:
            raise RuntimeError(f"no polynomial candidate selected for {system}")
        expected = {
            "harmonic": (3822, 2730),
            "quartic": (16380, 7644),
        }[system]
        measured = (result["cumulative_feature_values"],
                    result["final_matrix_feature_values"])
        if measured != expected:
            raise RuntimeError(f"{system} feature work {measured} != declared {expected}")
        selected.append({"system": system, **result})
    frozen_path = ROOT / "research/conservation_discovery/results/polynomial-v3.selected.json"
    frozen_path.write_text(json.dumps({"base_sha": "44524894a295cbdd4656d1b26de273ffe543d3c1",
                                       "selection": selected,
                                       "binary_sha256": sha256(binary)}, indent=2) + "\n")
    # Do not load the frozen PR candidates until selected expressions have been
    # committed to a separate selection artifact.
    with (ROOT / "research/conservation_discovery/results/target-runs-v2.json").open() as f:
        prior = json.load(f)
    candidates = [{"system": x["system"], "source": "polynomial-v3",
                   "expression": x["expression"], "search_work": {
                       "cumulative_feature_values": x["cumulative_feature_values"],
                       "final_matrix_feature_values": x["final_matrix_feature_values"],
                       "jacobi_rotations": x["jacobi_rotations"],
                       "cpu_seconds": x["cpu_seconds"]}} for x in selected]
    for record in prior["records"]:
        payload = json.loads(record["stdout"])
        candidates.append({"system": payload["system"], "source": "PR72-v2-seed-" + str(payload["seed"]),
                           "expression": payload["expression"], "search_work": {
                               "candidate_evaluations": payload["candidate_evaluations"],
                               "training_rows": payload["training_rows"],
                               "search_wall_seconds": record["wall_seconds"]}})
    # The exact verifier is independent and sees expressions only after freeze.
    sys.path.insert(0, str(ROOT / "scripts"))
    from verify_conservation_exact import verify
    results = []
    for item in candidates:
        assessment = run(binary, "polynomial-assess", item["system"], item["expression"])
        try:
            exact = verify(item["expression"], item["system"])
        except Exception as error:
            exact = {"verification_status": "inconclusive", "reason": str(error),
                     "exact_conservation": None, "exact_affine_reference_recovery": None}
        results.append({**item, "heldout": assessment, "exact_verification": exact})
    expected_units = "dimensionless_normalized_by_max_abs_candidate_output"
    if any(x["heldout"].get("heldout_separation_units") != expected_units
           for x in results):
        raise RuntimeError("held-out separation unit contract changed")
    destination = ROOT / "research/conservation_discovery/results/polynomial-v3.comparison.json"
    destination.write_text(json.dumps({"selection_artifact": frozen_path.name,
                                       "selection_artifact_sha256": sha256(frozen_path),
                                       "assessment_initial_conditions": [[0, 2], [-0.5, 0], [1.5, -0.5]],
                                       "records": results}, indent=2) + "\n")
    print(destination)


if __name__ == "__main__":
    main()
