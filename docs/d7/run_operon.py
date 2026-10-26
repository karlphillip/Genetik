#!/usr/bin/env python3
"""PyOperon 0.6.2 adapter for the frozen D7 scalar benchmark."""
import argparse
import json
from pathlib import Path
import re
import time

import numpy as np
import pandas as pd


def load_csv(path):
    frame = pd.read_csv(path)
    return frame.drop(columns=["y"]), frame["y"].to_numpy(dtype=np.float64)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, required=True)
    parser.add_argument("--task", required=True)
    parser.add_argument("--track", choices=["clean", "noise01"], required=True)
    parser.add_argument("--train", type=Path)
    parser.add_argument("--preflight", action="store_true")
    args = parser.parse_args()
    if args.preflight:
        X = pd.DataFrame({"x0": np.linspace(-1.0, 1.0, 64, dtype=np.float64)})
        y = X["x0"].to_numpy() + 0.25
    else:
        if args.train is None:
            parser.error("--train is required outside preflight")
        X, y = load_csv(args.train)

    before_import = time.monotonic()
    from pyoperon.sklearn import SymbolicRegressor
    import_seconds = time.monotonic() - before_import
    model = SymbolicRegressor(
        allowed_symbols="add,sub,mul,div,sin,exp,log,constant,variable",
        population_size=128,
        pool_size=128,
        generations=1_000_000,
        max_evaluations=1_000_000_000,
        max_time=60 if args.preflight else 120,
        max_length=25,
        max_depth=8,
        initialization_max_length=10,
        initialization_max_depth=5,
        objectives=["c2"],  # monotonic with MSE for this fixed target vector
        model_selection_criterion="mean_squared_error",
        add_model_scale_term=False,
        add_model_intercept_term=False,
        optimizer="lm",
        optimizer_iterations=10,
        local_search_probability=1.0,
        lamarckian_probability=1.0,
        n_threads=1,
        random_state=args.seed,
    )
    start_fit = time.monotonic()
    model.fit(X, y)
    fit_seconds = time.monotonic() - start_fit
    if not model.pareto_front_:
        raise RuntimeError("PyOperon returned no model")
    best = min(model.pareto_front_, key=lambda item: (item["mean_squared_error"], item["length"]))
    raw_expression = model.get_model_string(best["tree"], precision=17)
    # PyOperon prints variables as X1..Xn regardless of the input DataFrame labels.
    expression = re.sub(r"\bX([1-9][0-9]*)\b", lambda m: f"x{int(m.group(1)) - 1}", raw_expression)
    print(json.dumps({
        "framework": "PyOperon",
        "version": "0.6.2",
        "core_commit": "fad10f087c592812b1a9ecfabce669f37bf459f7",
        "task": args.task,
        "track": args.track,
        "seed": args.seed,
        "expression": expression,
        "raw_expression": raw_expression,
        "native_training_mse": float(best["mean_squared_error"]),
        "native_complexity": int(best["length"]),
        "native_evaluation_count": int(model.stats_["evaluation_count"]),
        "native_generations": int(model.stats_["generations"]),
        "import_seconds": import_seconds,
        "fit_seconds": fit_seconds,
        "preflight": args.preflight,
    }, sort_keys=True, allow_nan=False))


if __name__ == "__main__":
    main()
