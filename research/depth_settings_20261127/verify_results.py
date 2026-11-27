#!/usr/bin/env python3
"""Independent offline consistency checks for the frozen depth study."""
import ast
import csv
import pathlib
import re

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
    assert len(results) == 32, len(results)
    assert len(caps) == 32, len(caps)
    assert len({(r["setting"], int(r["seed"])) for r in results}) == 32
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
    for r in details:
        if r["capability"] == "capable":
            witness = parse_witness(r["coefficient_assignment_witness"])
            assert all(-8 <= value <= 8 for value in witness.values())
            assert verify_identity(r["prefit_expression_identity_includes_literal_values"], witness), r
        else:
            assert r["capability"] == "incapable", r
            assert "Groebner basis" in r["algebraic_reason"] or "fixed polynomial" in r["algebraic_reason"], r
    assert all(int(r["unresolved_distinct_trees"]) == 0 for r in caps)
    print(f"PASS runs=32 candidate_records=1600 unique_within_run_trees={len(details)} "
          f"capable={sum(r['capability']=='capable' for r in details)} "
          f"incapable={sum(r['capability']=='incapable' for r in details)} unresolved=0")


if __name__ == "__main__":
    main()
