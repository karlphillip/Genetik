#!/usr/bin/env python3
"""Independent offline consistency checks for the frozen depth study."""
import ast
import csv
import pathlib
import re
import subprocess

import sympy as sp

ROOT = pathlib.Path(__file__).resolve().parent
OUT = ROOT / "results"
RAW = OUT / "raw"
EXPECTED_SEEDS = set(range(76001, 76009))
EXPECTED_SETTINGS = {
    "original": (2, 2),
    "deeper_initialization": (4, 2),
    "deeper_mutation": (2, 3),
    "library_defaults": (4, 3),
}


def parse_witness(s):
    data = ast.literal_eval(s)
    return {name: sp.Rational(value) for name, value in data.items()}


def verify_identity(expression, witness):
    tree = ast.parse(expression, mode="eval").body
    x = sp.Symbol("x")
    index = 0

    def visit(node):
        nonlocal index
        if isinstance(node, ast.Name) and node.id == "x0":
            return x
        if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
            value = witness[f"c{index}"]
            index += 1
            return value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            if isinstance(node.operand, ast.Constant):
                value = witness[f"c{index}"]
                index += 1
                return value
            return -visit(node.operand)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
            return visit(node.left) + visit(node.right)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Mult):
            return visit(node.left) * visit(node.right)
        raise ValueError(f"unsupported node: {ast.dump(node)}")

    result = sp.Poly(sp.expand(visit(tree) - (2 * x + 3)), x)
    return result.is_zero and index == len(witness)


def numeric(expression, x):
    tree = ast.parse(expression, mode="eval").body

    def visit(node):
        if isinstance(node, ast.Name) and node.id == "x0":
            return x
        if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
            return float(node.value)
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return -visit(node.operand)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
            return visit(node.left) + visit(node.right)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Mult):
            return visit(node.left) * visit(node.right)
        raise ValueError(f"unsupported selected expression: {ast.dump(node)}")

    return visit(tree)


def read(path):
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def main():
    results = read(OUT / "results.csv")
    caps = read(OUT / "capability_assessment.csv")
    details = read(OUT / "candidate_classifications.csv")
    normalized = read(OUT / "raw_summary_normalized.csv")
    assert len(results) == 32, len(results)
    assert len(caps) == 32, len(caps)
    assert len(normalized) == 32, len(normalized)
    assert len({(r["setting"], int(r["seed"])) for r in results}) == 32
    assert "search_elapsed_seconds" in normalized[0]
    assert "search_cpu_seconds" not in normalized[0]
    detail_by_run = {}
    snapshots_by_run = {}
    for r in details:
        detail_by_run.setdefault((r["setting"], int(r["seed"])), []).append(r)
    for setting in EXPECTED_SETTINGS:
        for seed in EXPECTED_SEEDS:
            stem = f"{setting}_seed{seed}"
            trace = (RAW / f"{stem}.trace").read_text()
            snapshots = {}
            for line in trace.splitlines():
                if line.startswith("TRACE_SURVIVE generation="):
                    parts = line.split(" ", 3)
                    assert len(parts) == 4, line
                    generation = int(parts[1].split("=", 1)[1])
                    snapshots.setdefault(generation, set()).add(parts[3])
            assert 1 in snapshots, f"missing first recorded population snapshot: {stem}"
            snapshots_by_run[(setting, seed)] = snapshots
    for setting, depths in EXPECTED_SETTINGS.items():
        rows = [r for r in results if r["setting"] == setting]
        assert {int(r["seed"]) for r in rows} == EXPECTED_SEEDS
        assert all((int(r["initial_depth"]), int(r["mutation_depth"])) == depths for r in rows)
        assert all(int(r["candidate_records"]) == 50 for r in rows)
        for r in rows:
            held = sum((numeric(r["selected_equation"], x) - (2 * x + 3)) ** 2
                       for x in (-0.5, 2.5, 4.0)) / 3
            saved = float(r["heldout_mse"])
            assert abs(held - saved) <= max(1e-12, abs(saved) * 1e-8), (r, held)
            assert int(r["numerical_recovery"]) == int(held <= 1e-12)
            stem = f"{setting}_seed{r['seed']}"
            trace = (RAW / f"{stem}.trace").read_text()
            assert len(re.findall(r"^TRACE_PREP ", trace, flags=re.M)) == 50, stem
            stdout = (RAW / f"{stem}.stdout").read_text()
            fields = next(csv.reader([stdout.strip()]))
            pairs = dict(zip(fields[::2], fields[1::2]))
            assert pairs["candidate_evaluations"] == "50"
            assert pairs["objective_calls"] == r["objective_calls"]
            normalized_row = next(x for x in normalized
                                  if x["setting"] == setting and int(x["seed"]) == int(r["seed"]))
            assert float(normalized_row["search_elapsed_seconds"]) == float(pairs["search_cpu_seconds"])
            assert "search_cpu_seconds" not in r

    # Candidate evaluation and selection population generations are distinct:
    # initialized generation-0 candidates first map to logged snapshot 1;
    # later offspring map to the snapshot with the same generation number.
    for row in details:
        run = (row["setting"], int(row["seed"]))
        evaluation_generation = int(row["candidate_evaluation_generation"])
        expected_snapshot = 1 if evaluation_generation == 0 else evaluation_generation
        snapshots = snapshots_by_run[run]
        assert int(row["population_snapshot_generation_inspected"]) == expected_snapshot, row
        assert int(row["population_snapshot_recorded"]) == 1, row
        assert expected_snapshot in snapshots, row
        expected_present = row["fit_output"] in snapshots[expected_snapshot]
        assert int(row["fitted_expression_present_in_population_snapshot"]) == int(expected_present), row
        assert row["candidate_evaluation_stage"] == ("initialization" if evaluation_generation == 0 else "offspring")

    for row in caps:
        if not row["first_capable_expression"]:
            continue
        run = (row["setting"], int(row["seed"]))
        generation = int(row["first_capable_candidate_evaluation_generation"])
        snapshot_generation = 1 if generation == 0 else generation
        matching = next(r for r in detail_by_run[run]
                        if r["prefit_expression_identity_includes_literal_values"] == row["first_capable_expression"]
                        and int(r["candidate_evaluation_generation"]) == generation)
        assert int(row["first_capable_population_snapshot_generation_inspected"]) == snapshot_generation
        assert int(row["first_capable_fitted_expression_present_in_snapshot"]) == int(
            matching["fitted_expression_present_in_population_snapshot"])

    for r in details:
        if r["capability"] == "capable":
            witness = parse_witness(r["coefficient_assignment_witness"])
            assert all(-8 <= value <= 8 for value in witness.values())
            assert verify_identity(r["prefit_expression_identity_includes_literal_values"], witness), r
        else:
            assert r["capability"] == "incapable", r
            assert "Groebner basis" in r["algebraic_reason"] or "fixed polynomial" in r["algebraic_reason"], r
    assert all(int(r["unresolved_distinct_trees"]) == 0 for r in caps)
    recovery_counts = {
        setting: sum(int(r["numerical_recovery"]) for r in results if r["setting"] == setting)
        for setting in EXPECTED_SETTINGS
    }
    assert recovery_counts == {
        "original": 2, "deeper_initialization": 6,
        "deeper_mutation": 8, "library_defaults": 7,
    }, recovery_counts
    baseline = {int(r["seed"]): int(r["numerical_recovery"])
                for r in results if r["setting"] == "original"}
    paired = {}
    for setting in EXPECTED_SETTINGS:
        current = {int(r["seed"]): int(r["numerical_recovery"])
                   for r in results if r["setting"] == setting}
        paired[setting] = (
            sum(current[k] == 1 and baseline[k] == 0 for k in baseline),
            sum(current[k] == 0 and baseline[k] == 1 for k in baseline),
        )
    assert paired == {
        "original": (0, 0), "deeper_initialization": (5, 1),
        "deeper_mutation": (6, 0), "library_defaults": (6, 1),
    }, paired
    capable_counts = {
        setting: sum(int(row["distinct_capable_prefit_trees_literal_sensitive"])
                     for row in caps if row["setting"] == setting)
        for setting in EXPECTED_SETTINGS
    }
    assert capable_counts == {
        "original": 7, "deeper_initialization": 49,
        "deeper_mutation": 37, "library_defaults": 67,
    }, capable_counts

    launcher = (ROOT / "run_comparison.sh").read_text()
    assert "search_elapsed_seconds" in launcher
    assert "results.csv" not in launcher
    assert "> \"$out/raw/" in launcher
    repo = ROOT.parents[1]
    subprocess.run(["sha256sum", "-c", str(ROOT / "IMMUTABLE_SEARCH_ARTIFACTS.sha256")],
                   cwd=repo, check=True, stdout=subprocess.DEVNULL)
    print(f"PASS runs=32 candidate_records=1600 unique_within_run_trees={len(details)} "
          f"capable={sum(r['capability']=='capable' for r in details)} "
          f"incapable={sum(r['capability']=='incapable' for r in details)} unresolved=0 "
          f"generation_snapshot_mapping=verified recovery={recovery_counts} paired={paired}")


if __name__ == "__main__":
    main()
