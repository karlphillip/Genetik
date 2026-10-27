#!/usr/bin/env python3
"""Independently score canonical infix model strings against frozen D7 CSVs."""
import argparse
import ast
import csv
import json
import math
from pathlib import Path

import numpy as np
import sympy as sp


OPS = {
    ast.Add: lambda a, b: a + b,
    ast.Sub: lambda a, b: a - b,
    ast.Mult: lambda a, b: a * b,
    ast.Div: lambda a, b: a / b,
}
FUNCS = {"sin": np.sin, "exp": np.exp, "log": np.log, "ln": np.log}
SYMBOLS = {"sin": sp.sin, "exp": sp.exp, "log": sp.log, "ln": sp.log}

DOMAINS = {
    "polynomial_x3": [(-2.0, 2.0)], "rational_x2": [(-2.0, 2.0)],
    "trig_sine": [(-math.pi, math.pi)], "exponential_decay": [(-2.0, 2.0)],
    "fractional_power_positive": [(0.1, 3.0)],
    "kinetic_energy_mv2": [(0.5, 10.0), (0.1, 8.0)],
}


def parse_model(source, nvars):
    tree = ast.parse(source, mode="eval")

    def walk(node):
        if isinstance(node, ast.Expression):
            return walk(node.body)
        if isinstance(node, ast.Constant) and type(node.value) in (int, float):
            value = float(node.value)
            if not math.isfinite(value):
                raise ValueError("non-finite literal")
            return lambda x: np.full(len(x), value), sp.Float(repr(value)), 1, 1
        if isinstance(node, ast.Name) and node.id.startswith("x") and node.id[1:].isdigit():
            index = int(node.id[1:])
            if index >= nvars:
                raise ValueError("model references an unavailable input")
            return lambda x: x[:, index], sp.Symbol(node.id, real=True), 1, 1
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
            fn, sym, nodes, depth = walk(node.operand)
            sign_fn = (lambda x: fn(x)) if isinstance(node.op, ast.UAdd) else (lambda x: -fn(x))
            sign_sym = sym if isinstance(node.op, ast.UAdd) else -sym
            is_signed_literal = isinstance(node.operand, ast.Constant) or (
                isinstance(node.operand, ast.UnaryOp) and isinstance(node.operand.op, (ast.UAdd, ast.USub))
            )
            return sign_fn, sign_sym, (1 if is_signed_literal else nodes + 1), (1 if is_signed_literal else depth + 1)
        if isinstance(node, ast.BinOp) and type(node.op) in OPS:
            left, sl, nl, dl = walk(node.left)
            right, sr, nr, dr = walk(node.right)
            op = OPS[type(node.op)]
            return lambda x: op(left(x), right(x)), op_symbol(type(node.op), sl, sr), 1 + nl + nr, 1 + max(dl, dr)
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in FUNCS and len(node.args) == 1 and not node.keywords:
            fn, sym, nodes, depth = walk(node.args[0])
            name = node.func.id
            return lambda x: FUNCS[name](fn(x)), SYMBOLS[name](sym), nodes + 1, depth + 1
        raise ValueError(f"expression contains unsupported syntax: {type(node).__name__}")

    return walk(tree)


def op_symbol(kind, left, right):
    if kind is ast.Add:
        return left + right
    if kind is ast.Sub:
        return left - right
    if kind is ast.Mult:
        return left * right
    if kind is ast.Div:
        return left / right
    raise ValueError(kind)


def load_csv(path):
    rows = list(csv.DictReader(path.open(newline="", encoding="utf-8")))
    names = [key for key in rows[0] if key != "y"]
    x = np.asarray([[float(row[name]) for name in names] for row in rows], dtype=np.float64)
    y = np.asarray([float(row["y"]) for row in rows], dtype=np.float64)
    return x, y


def target_symbolic(task):
    x0 = sp.Symbol("x0", real=True)
    if task == "polynomial_x3":
        return x0**3 - 2*x0 + 1
    if task == "rational_x2":
        return (x0 + 2) / (x0**2 + 1)
    if task == "trig_sine":
        return 2*sp.sin(sp.Rational(3, 2)*x0 + sp.Rational(3, 10))
    if task == "exponential_decay":
        return 3*sp.exp(-sp.Rational(7, 10)*x0)
    if task == "fractional_power_positive":
        return sp.Rational(7, 2)*x0**sp.Rational(17, 10)
    if task == "kinetic_energy_mv2":
        return sp.Rational(1, 2)*sp.Symbol("x0", real=True)*sp.Symbol("x1", real=True)**2
    raise ValueError(task)


def certify_expression_domain(source, task):
    """Conservatively reject equivalence when model syntax may be undefined."""
    tree = ast.parse(source, mode="eval")
    symbols = [sp.Symbol(f"x{i}", real=True) for i in range(len(DOMAINS[task]))]
    denominators = []
    has_log = False

    def symbolic(node):
        nonlocal has_log
        if isinstance(node, ast.Expression):
            return symbolic(node.body)
        if isinstance(node, ast.Constant) and type(node.value) in (int, float):
            return sp.Rational(str(node.value))
        if isinstance(node, ast.Name) and node.id.startswith("x") and node.id[1:].isdigit():
            return symbols[int(node.id[1:])]
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
            value = symbolic(node.operand)
            return value if isinstance(node.op, ast.UAdd) else -value
        if isinstance(node, ast.BinOp) and type(node.op) in OPS:
            left, right = symbolic(node.left), symbolic(node.right)
            if isinstance(node.op, ast.Div):
                denominators.append(right)
            return op_symbol(type(node.op), left, right)
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in FUNCS and len(node.args) == 1:
            has_log = has_log or node.func.id in ("log", "ln")
            value = symbolic(node.args[0])
            return SYMBOLS[node.func.id](value)
        raise ValueError("unsupported syntax in domain certificate")

    expression = symbolic(tree)
    if has_log:
        return False, "logarithm domain not certified over full task domain"
    for denominator in denominators:
        if denominator.is_zero is True:
            return False, "identically zero divisor"
        if denominator.is_number:
            if denominator == 0:
                return False, "identically zero divisor"
            continue
        if len(symbols) != 1 or denominator.free_symbols - {symbols[0]}:
            return False, "multivariate divisor domain not certified"
        lo, hi = DOMAINS[task][0]
        try:
            polynomial = sp.Poly(denominator, symbols[0])
            interval_roots = polynomial.count_roots(sp.Rational(str(lo)), sp.Rational(str(hi)))
        except (sp.PolynomialError, NotImplementedError, ValueError, TypeError):
            return False, "non-polynomial divisor domain not certified"
        if interval_roots:
            return False, f"divisor zeros not excluded on [{lo}, {hi}]"
    return True, "all explicit divisors are constants or certified root-free polynomials on declared domain"


def summarize(source, x, y):
    parsed = parse_model(source, x.shape[1])
    fn, symbolic, nodes, depth = parsed
    invalid = 0
    with np.errstate(all="ignore"):
        pred = np.asarray(fn(x), dtype=np.float64)
    if pred.shape != y.shape:
        raise ValueError("model returned wrong prediction shape")
    valid = np.isfinite(pred)
    invalid = int((~valid).sum())
    if invalid:
        return {"rmse": None, "max_abs_error": None, "normalized_rmse": None,
                "max_relative_error": None, "relative_error_samples": 0,
                "invalid_predictions": invalid, "nodes": nodes, "depth": depth}, symbolic
    errors = pred - y
    rmse = float(np.sqrt(np.mean(errors * errors)))
    rms = float(np.sqrt(np.mean(y * y)))
    relative_mask = np.abs(y) > 1e-12 * max(rms, np.finfo(np.float64).tiny)
    relative_error = float(np.max(np.abs(errors[relative_mask]) / np.abs(y[relative_mask]))) if relative_mask.any() else None
    return {"rmse": rmse, "max_abs_error": float(np.max(np.abs(errors))),
            "normalized_rmse": rmse / max(rms, np.finfo(np.float64).tiny),
            "max_relative_error": relative_error,
            "relative_error_samples": int(relative_mask.sum()),
            "invalid_predictions": 0, "nodes": nodes, "depth": depth}, symbolic


def assess(record, data_dir):
    task, track = record["task"], record["track"]
    xtrain, yclean = load_csv(data_dir / f"{task}-train-clean.csv")
    _, ytrain = load_csv(data_dir / f"{task}-train-{track}.csv")
    xtest, ytest = load_csv(data_dir / f"{task}-test.csv")
    train, sym = summarize(record["expression"], xtrain, ytrain)
    test, _ = summarize(record["expression"], xtest, ytest)
    symbols = {f"x{i}": sp.Symbol(f"x{i}", real=True) for i in range(xtrain.shape[1])}
    try:
        difference = sp.simplify(sym - target_symbolic(task))
        domain_valid, domain_reason = certify_expression_domain(record["expression"], task)
        equivalence = "symbolically_equivalent" if difference == 0 and domain_valid else "not_proved_equivalent"
    except Exception as exc:
        equivalence = "unknown"
        record["equivalence_error"] = f"{type(exc).__name__}: {exc}"
    target_rms = float(np.sqrt(np.mean(yclean * yclean)))
    threshold = 1e-6 if track == "clean" else 0.02
    scale = max(1.0, target_rms) if track == "clean" else max(target_rms, np.finfo(np.float64).tiny)
    return {
        **record,
        "assessment": {
            "training_rmse": train["rmse"],
            "training_max_absolute_error": train["max_abs_error"],
            "training_max_relative_error": train["max_relative_error"],
            "training_relative_error_samples": train["relative_error_samples"],
            "heldout_rmse": test["rmse"],
            "heldout_max_absolute_error": test["max_abs_error"],
            "heldout_max_relative_error": test["max_relative_error"],
            "heldout_relative_error_samples": test["relative_error_samples"],
            "training_normalized_rmse": train["normalized_rmse"],
            "heldout_normalized_rmse": test["normalized_rmse"],
            "relative_error_definition": "RMSE / noiseless target RMS",
            "invalid_training_predictions": train["invalid_predictions"],
            "invalid_heldout_predictions": test["invalid_predictions"],
            "expression_nodes": test["nodes"],
            "expression_depth": test["depth"],
            "symbolic_assessment": equivalence,
            "symbolic_domain_assessment": domain_reason if "domain_reason" in locals() else "unknown",
            "operational_recovery": bool(test["rmse"] is not None and test["rmse"] <= threshold * scale),
            "recovery_relative_threshold": threshold,
            "recovery_absolute_threshold": threshold * scale,
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="JSONL canonical candidate records")
    parser.add_argument("output", type=Path, help="JSONL independent assessments")
    parser.add_argument("--data", type=Path, default=Path(__file__).with_name("data"))
    args = parser.parse_args()
    with args.input.open(encoding="utf-8") as src, args.output.open("w", encoding="utf-8") as dst:
        for line in src:
            record = json.loads(line)
            try:
                assessment = assess(record, args.data)
            except Exception as exc:
                assessment = {**record, "assessment_error": f"{type(exc).__name__}: {exc}"}
            dst.write(json.dumps(assessment, sort_keys=True, allow_nan=False) + "\n")


if __name__ == "__main__":
    main()
