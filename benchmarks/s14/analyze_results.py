#!/usr/bin/env python3
"""Independent Python scoring and descriptive analysis for frozen S14 outputs."""
import ast
import csv
import json
import math
import pathlib
import sys

ROOT = pathlib.Path(sys.argv[1]).resolve()
RUNS = ROOT / "runs/confirmation-01/summary.json"
MANIFEST = ROOT / "frozen-protocol/manifest.json"
OUT = ROOT / "analysis"
OUT.mkdir(parents=True, exist_ok=True)


def expr_eval(source, x, y):
    tree = ast.parse(source, mode="eval")
    def visit(node):
        if isinstance(node, ast.Expression): return visit(node.body)
        if isinstance(node, ast.Name):
            if node.id == "x0": return x
            if node.id == "x1": return y
            raise ValueError("unsupported identifier " + node.id)
        if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
            return float(node.value)
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.USub, ast.UAdd)):
            value = visit(node.operand)
            return -value if isinstance(node.op, ast.USub) else value
        if isinstance(node, ast.BinOp):
            left, right = visit(node.left), visit(node.right)
            if isinstance(node.op, ast.Add): return left + right
            if isinstance(node.op, ast.Sub): return left - right
            if isinstance(node.op, ast.Mult): return left * right
            if isinstance(node.op, ast.Div): return left / right
            if isinstance(node.op, ast.Pow): return left ** right
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
            args = [visit(a) for a in node.args]
            if node.func.id == "sin" and len(args) == 1: return math.sin(args[0])
            if node.func.id == "powi" and len(args) == 2: return args[0] ** args[1]
        raise ValueError("unsupported expression syntax: " + ast.dump(node))
    return float(visit(tree))


def read_rows(path):
    with pathlib.Path(path).open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    return [(float(r["x0"]), float(r["x1"]), float(r["y"])) for r in rows]


def score(expression, rows):
    errors = []
    invalid = 0
    max_abs = 0.0
    for x, y, target in rows:
        try:
            prediction = expr_eval(expression, x, y)
            error = prediction - target
            if not math.isfinite(error): raise ArithmeticError("nonfinite residual")
            errors.append(error)
            max_abs = max(max_abs, abs(error))
        except (ArithmeticError, OverflowError, ValueError, ZeroDivisionError):
            invalid += 1
    if invalid or len(errors) != len(rows):
        return {"rmse": None, "max_abs": max_abs if errors else None, "invalid": invalid}
    return {"rmse": math.sqrt(sum(e*e for e in errors)/len(errors)),
            "max_abs": max_abs, "invalid": 0}


def expression_shape(source):
    tree = ast.parse(source, mode="eval")
    def shape(node):
        if isinstance(node, ast.Expression): return shape(node.body)
        if isinstance(node, ast.UnaryOp):
            # Genetik prints a negative finite constant as a unary-minus token,
            # while the expression representation has one constant node.
            if isinstance(node.op, (ast.USub, ast.UAdd)) and isinstance(node.operand, ast.Constant):
                return 1, 1
            raise ValueError("unexpected unary operation in serialized expression")
        if isinstance(node, (ast.Name, ast.Constant)): return 1, 1
        if isinstance(node, ast.BinOp):
            left_nodes, left_depth = shape(node.left)
            right_nodes, right_depth = shape(node.right)
            return 1 + left_nodes + right_nodes, 1 + max(left_depth, right_depth)
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
            args = [shape(a) for a in node.args]
            return 1 + sum(a[0] for a in args), 1 + max((a[1] for a in args), default=0)
        raise ValueError("unexpected AST node in serialized expression")
    return shape(tree)

manifest = json.loads(MANIFEST.read_text())
summary = json.loads(RUNS.read_text())
assert summary["recorded_runs"] == summary["expected_runs"] == 192
by_task = {t["id"]: t for t in manifest["tasks"]}
rows = []
for run in summary["runs"]:
    if run["run_status"] != "completed" or not run.get("result"):
        rows.append({"task_id":run["task_id"],"method":run["method"],"seed":run["seed"],"failed":True,
                     "stop":run.get("stopping_reason"),"wall_seconds":run["wall_seconds"],
                     "peak_rss_bytes":run.get("peak_rss_bytes")})
        continue
    task = by_task[run["task_id"]]
    r = run["result"]
    expression_nodes, expression_depth = expression_shape(r["expression"])
    train, held, extrap = [read_rows(MANIFEST.parent / p) for p in task["domain_observations"]]
    scores = {"training":score(r["expression"],train),
              "heldout":score(r["expression"],held),
              "extrapolation":score(r["expression"],extrap)}
    held_rms = math.sqrt(sum(z*z for _,_,z in held)/len(held))
    ext_rms = math.sqrt(sum(z*z for _,_,z in extrap)/len(extrap))
    held_ratio = scores["heldout"]["rmse"]/held_rms if scores["heldout"]["rmse"] is not None else None
    ext_ratio = scores["extrapolation"]["rmse"]/ext_rms if scores["extrapolation"]["rmse"] is not None else None
    rational = task["family"].startswith("rational_")
    clean = task["track"] == "clean"
    ext_limit = .10 if task["family"] == "rational_near_pole" else .05
    if rational:
        threshold_pass = ((held_ratio is not None and ext_ratio is not None and held_ratio <= 1e-8 and ext_ratio <= 1e-8)
                     if clean else (held_ratio is not None and ext_ratio is not None and held_ratio <= .025 and ext_ratio <= ext_limit))
    else:
        threshold_pass = None
    rows.append({"task_id":run["task_id"],"family":task["family"],"track":task["track"],
        "noise_seed":task["noise_seed"],"method":run["method"],"seed":run["seed"],
        "predictive_threshold_pass_rational_only":threshold_pass,"expression":r["expression"],"scores_independent":scores,
        "expression_nodes":expression_nodes,"expression_depth":expression_depth,
        "heldout_target_rms":held_rms,"extrap_target_rms":ext_rms,"heldout_rmse_fraction":held_ratio,
        "extrap_rmse_fraction":ext_ratio,"numerator_degree":r.get("numerator_degree"),
        "denominator_degree":r.get("denominator_degree"),"parameters":r.get("parameters"),
        "coefficients_p":r.get("coefficients_p"),"coefficients_q":r.get("coefficients_q"),
        "domain_status":r.get("domain_status"),"domain_finding":r.get("domain_finding"),
        "pivot_ratio":r.get("pivot_ratio"),"training_loss_attempts":r.get("loss_evaluations",r.get("training_loss_attempts")),
        "candidate_evaluations":r.get("candidate_evaluations"),"fitting_loss_attempts":r.get("fitting_loss_attempts"),
        "jacobian_evaluations":r.get("jacobian_evaluations"),"mathematical_operations":r.get("mathematical_operations"),
        "linear_algebra_operations":r.get("linear_algebra_operations"),"lm_stop":r.get("lm_stop"),
        "invalid_losses":r.get("invalid_losses"),"stop":r.get("stop"),
        "wall_seconds":run["wall_seconds"],"cpu_user_seconds":run["cpu_user_seconds"],
        "cpu_system_seconds":run["cpu_system_seconds"],"peak_rss_bytes":run["peak_rss_bytes"],
        "peak_rss_source":run["peak_rss_source"],"training_rmse_runner":r.get("fit_train_rmse")})

(OUT/"independent-scores.json").write_text(json.dumps({"summary_source":str(RUNS),"scorer":"Python AST interpreter + Python CSV scorer, independent from Genetik evaluator","records":rows},indent=2,sort_keys=True)+"\n")
# Compact per-condition threshold and score summary, keeping all row-level results above.
agg={}
for row in rows:
    if row.get("family", "").startswith("rational_") and not row.get("failed"):
        key=(row["family"],row["track"],row["method"])
        agg.setdefault(key,[]).append(row)
compact=[]
for (family,track,method), group in sorted(agg.items()):
    vals=[r["heldout_rmse_fraction"] for r in group if r["heldout_rmse_fraction"] is not None]
    exts=[r["extrap_rmse_fraction"] for r in group if r["extrap_rmse_fraction"] is not None]
    compact.append({"family":family,"track":track,"method":method,"n":len(group),
        "predictive_threshold_passes":sum(r.get("predictive_threshold_pass_rational_only") is True for r in group),
        "heldout_fraction_median":sorted(vals)[len(vals)//2] if vals else None,
        "extrapolation_fraction_median":sorted(exts)[len(exts)//2] if exts else None,
        "invalid_heldout":sum(r["scores_independent"]["heldout"]["invalid"] for r in group),
        "invalid_extrapolation":sum(r["scores_independent"]["extrapolation"]["invalid"] for r in group)})
(OUT/"task-summary.json").write_text(json.dumps(compact,indent=2,sort_keys=True)+"\n")
print(json.dumps({"rows":len(rows),"rational_groups":len(compact),"independent_score_file":str(OUT/"independent-scores.json")},sort_keys=True))
