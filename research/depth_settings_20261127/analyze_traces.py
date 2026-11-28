#!/usr/bin/env python3
"""Offline exact-polynomial capability analysis of saved ordinary-search traces."""
import ast
import csv
import itertools
import math
import pathlib
import re
import sys
from functools import lru_cache

import sympy as sp


ROOT = pathlib.Path(__file__).resolve().parent
RAW = ROOT / "results" / "raw"
OUT = ROOT / "results"
SEEDS = range(76001, 76009)
SETTINGS = {
    "original": (2, 2),
    "deeper_initialization": (4, 2),
    "deeper_mutation": (2, 3),
    "library_defaults": (4, 3),
}
TARGET_X = sp.Symbol("x")


def key_value_line(s):
    cells = next(csv.reader([s]))
    if len(cells) % 2:
        raise ValueError(f"not alternating key/value fields: {s}")
    return dict(zip(cells[::2], cells[1::2]))


def ast_expression(text):
    tree = ast.parse(text.replace("x0", "x0"), mode="eval").body
    symbols = []
    constant_number = 0

    def visit(node):
        nonlocal constant_number
        if isinstance(node, ast.Name) and node.id == "x0":
            return TARGET_X
        if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
            symbol = sp.Symbol(f"c{constant_number}", real=True)
            symbols.append(symbol)
            constant_number += 1
            return symbol
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            if isinstance(node.operand, ast.Constant):
                symbol = sp.Symbol(f"c{constant_number}", real=True)
                symbols.append(symbol)
                constant_number += 1
                return symbol
            return -visit(node.operand)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
            return visit(node.left) + visit(node.right)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Mult):
            return visit(node.left) * visit(node.right)
        raise ValueError(f"unsupported expression AST: {ast.dump(node)}")

    return sp.expand(visit(tree)), symbols


def equations_for(expr):
    poly = sp.Poly(sp.expand(expr - (2 * TARGET_X + 3)), TARGET_X)
    degree = max(1, poly.degree())
    return [sp.expand(poly.nth(i)) for i in range(degree + 1)]


def in_bounds(value):
    value = sp.N(value, 30)
    return value.is_real is True and value.is_finite is True and -8 <= float(value) <= 8


@lru_cache(maxsize=None)
def classify(text):
    try:
        expr, variables = ast_expression(text)
        equations = equations_for(expr)
        equations = [sp.factor(e) for e in equations if e != 0]
        if not equations:
            values = {v: sp.Integer(0) for v in variables}
            return "capable", values, "identically the target for all literal assignments"
        if not variables:
            return "incapable", {}, f"fixed polynomial coefficient equations are nonzero: {equations}"
        solutions = sp.solve(equations, variables, dict=True, simplify=False)
        if solutions == []:
            basis = sp.groebner(equations, *variables, domain=sp.QQ)
            if any(poly.as_expr().is_number and poly.as_expr() != 0 for poly in basis.polys):
                return "incapable", {}, f"exact Groebner basis contains a nonzero constant; coefficient equations are inconsistent: {equations}"
            return "unresolved", {}, f"solver found no assignment but exact ideal is not certified inconsistent: {equations}"
        if solutions is None:
            return "unresolved", {}, "symbolic solver did not return a finite solution description"
        for solution in solutions:
            free = (set().union(*(value.free_symbols for value in solution.values())) |
                    (set(variables) - set(solution))) - set(solution)
            substitutions = {symbol: sp.Integer(0) for symbol in free}
            vals = {v: sp.simplify(sp.sympify(solution.get(v, v)).subs(substitutions)) for v in variables}
            if all(sp.simplify(e.subs(vals)) == 0 for e in equations) and all(in_bounds(v) for v in vals.values()):
                return "capable", vals, "coefficient equations verified exactly within [-8,8]"
        # An algebraic solution may require a free parameter choice. Try a small
        # deterministic grid and verify each candidate exactly.
        for solution in solutions:
            free = sorted((set().union(*(value.free_symbols for value in solution.values())) |
                           (set(variables) - set(solution))) - set(solution), key=str)
            if len(free) > 10:
                continue
            for free_values in itertools.product((0, 1, -1), repeat=len(free)):
                substitutions = dict(zip(free, map(sp.Integer, free_values)))
                vals = {v: sp.simplify(sp.sympify(solution.get(v, v)).subs(substitutions)) for v in variables}
                if all(sp.simplify(e.subs(vals)) == 0 for e in equations) and all(in_bounds(v) for v in vals.values()):
                    return "capable", vals, "coefficient equations verified exactly within [-8,8]"
        return "unresolved", {}, "solutions require untested parameters, exceed bounds, or are non-finite"
    except Exception as exc:
        return "unresolved", {}, f"offline symbolic check error: {type(exc).__name__}: {exc}"


def main():
    summary_rows = []
    run_rows = []
    capability_rows = []
    for setting, (initial, mutation) in SETTINGS.items():
        for seed in SEEDS:
            stem = f"{setting}_seed{seed}"
            raw_result = (RAW / f"{stem}.stdout").read_text().strip()
            d = key_value_line(raw_result)
            if int(d["seed"]) != seed or int(d["initial_depth"]) != initial or int(d["mutation_depth"]) != mutation:
                raise ValueError(f"protocol mismatch in {stem}: {d}")
            if "search_cpu_seconds" in d:
                d["search_elapsed_seconds"] = d.pop("search_cpu_seconds")
            summary_rows.append({"setting": setting, **d})

            records = []
            for line in (RAW / f"{stem}.trace").read_text().splitlines():
                m = re.fullmatch(r"TRACE_PREP generation=(\d+) origin=(\d+) before=(.*?) after=(.*?) loss=([^ ]+) stop=([^ ]+) calls=(\d+) selector=(\d+)", line)
                if m:
                    records.append({
                        "generation": int(m.group(1)), "origin_code": int(m.group(2)),
                        "before": m.group(3), "after": m.group(4), "train_mse": float(m.group(5)),
                        "fit_stop": m.group(6), "fit_objective_calls": int(m.group(7)),
                        "selector_evaluations": int(m.group(8)),
                    })
            if len(records) != int(d["candidate_evaluations"]):
                raise ValueError(f"trace has {len(records)} fit records for {d['candidate_evaluations']} candidates in {stem}")
            distinct = {}
            for r in records:
                distinct.setdefault(r["before"], r)
            classified = {text: classify(text) for text in distinct}
            snapshots = {}
            trace_lines = (RAW / f"{stem}.trace").read_text().splitlines()
            for line in trace_lines:
                if line.startswith("TRACE_SURVIVE generation="):
                    parts = line.split(" ", 3)
                    if len(parts) == 4:
                        generation = int(parts[1].split("=", 1)[1])
                        snapshots.setdefault(generation, set()).add(parts[3])
            for text, first_record in distinct.items():
                evaluation_generation = first_record["generation"]
                snapshot_generation = 1 if evaluation_generation == 0 else evaluation_generation
                snapshot_recorded = snapshot_generation in snapshots
                status, witness, reason = classified[text]
                capability_rows_detail = {
                    "setting": setting, "seed": seed,
                    "candidate_evaluation_generation": evaluation_generation,
                    "candidate_evaluation_stage": "initialization" if first_record["generation"] == 0 else "offspring",
                    "population_snapshot_generation_inspected": snapshot_generation,
                    "population_snapshot_recorded": int(snapshot_recorded),
                    "prefit_expression_identity_includes_literal_values": text,
                    "capability": status,
                    "coefficient_assignment_witness": repr({str(k): str(v) for k, v in witness.items()}),
                    "algebraic_reason": reason,
                    "fit_output": first_record["after"], "training_mse": first_record["train_mse"],
                    "fit_stop": first_record["fit_stop"],
                    "fit_objective_calls": first_record["fit_objective_calls"],
                    "fitted_expression_present_in_population_snapshot": int(
                        snapshot_recorded and first_record["after"] in snapshots[snapshot_generation]),
                }
                all_candidate_classifications.append(capability_rows_detail)
            capable = [r for r in records if classified[r["before"]][0] == "capable"]
            first = capable[0] if capable else None
            init_capable = {t for t, (status, _, _) in classified.items()
                            if status == "capable" and any(r["before"] == t and r["generation"] == 0 for r in records)}
            offspring_capable = {t for t, (status, _, _) in classified.items()
                                 if status == "capable" and any(r["before"] == t and r["generation"] > 0 for r in records)}
            unresolved = {t for t, (status, _, _) in classified.items() if status == "unresolved"}
            capable_distinct = {t for t, (status, _, _) in classified.items() if status == "capable"}
            capability_rows.append({
                "setting": setting, "seed": seed, "initial_depth": initial,
                "mutation_depth": mutation, "candidate_records": len(records),
                "distinct_prefit_trees_literal_sensitive": len(distinct),
                "distinct_capable_prefit_trees_literal_sensitive": len(capable_distinct),
                "initialization_capable_trees": len(init_capable),
                "offspring_capable_trees": len(offspring_capable),
                "unresolved_distinct_trees": len(unresolved),
                "first_capable_stage": "initialization" if first and first["generation"] == 0 else ("offspring" if first else "none evaluated"),
                "first_capable_candidate_evaluation_generation": first["generation"] if first else "",
                "first_capable_expression": first["before"] if first else "",
                "first_capable_assignment": repr({str(k): str(v) for k, v in classify(first["before"])[1].items()}) if first else "",
                "first_capable_fit_output": first["after"] if first else "",
                "first_capable_training_mse": first["train_mse"] if first else "",
                "first_capable_fit_stop": first["fit_stop"] if first else "",
                "first_capable_population_snapshot_generation_inspected":
                    (1 if first["generation"] == 0 else first["generation"]) if first else "",
                "first_capable_fitted_expression_present_in_snapshot":
                    int((1 if first["generation"] == 0 else first["generation"]) in snapshots and
                        first["after"] in snapshots[(1 if first["generation"] == 0 else first["generation"])]) if first else "",
            })
            run_rows.append({
                "setting": setting, "seed": seed, "initial_depth": initial,
                "mutation_depth": mutation, "candidate_records": len(records),
                "distinct_prefit_trees_literal_sensitive": len(distinct),
                "first_capable_stage": capability_rows[-1]["first_capable_stage"],
                "unresolved_distinct_trees": len(unresolved),
                "selected_equation": d["equation"], "train_mse": d["train_mse"],
                "heldout_mse": d["heldout_mse"], "numerical_recovery": d["numerical_recovery"],
                "objective_calls": d["objective_calls"], "lm_math_ops": d["lm_math_ops"],
                "lm_linear_ops": d["lm_linear_ops"], "lm_objective_evals": d["lm_objective_evals"],
                "lm_jacobian_evals": d["lm_jacobian_evals"],
                "search_elapsed_seconds": d.get("search_elapsed_seconds", ""),
                "stop": d["stop"],
            })

    def save(path, rows):
        with path.open("w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(rows[0]), lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)
    save(OUT / "per_seed.csv", run_rows)
    save(OUT / "results.csv", run_rows)
    save(OUT / "capability_assessment.csv", capability_rows)
    save(OUT / "candidate_classifications.csv", all_candidate_classifications)
    save(OUT / "raw_summary_normalized.csv", summary_rows)
    counts = {s: sum(int(row["numerical_recovery"]) for row in run_rows if row["setting"] == s) for s in SETTINGS}
    for s in SETTINGS:
        same = {int(row["seed"]): int(row["numerical_recovery"]) for row in run_rows if row["setting"] == s}
        base = {int(row["seed"]): int(row["numerical_recovery"]) for row in run_rows if row["setting"] == "original"}
        gains = sum(same[k] == 1 and base[k] == 0 for k in base)
        losses = sum(same[k] == 0 and base[k] == 1 for k in base)
        print(f"{s}: recovery={counts[s]}/8 paired_gains={gains} paired_losses={losses}")
    print(f"sympy={sp.__version__}; classified literal-sensitive trees={len(classify.cache_info().__str__()) if False else classify.cache_info().currsize}")


if __name__ == "__main__":
    all_candidate_classifications = []
    main()
