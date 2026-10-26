#!/usr/bin/env python3
"""PySR 2.7.2 adapter for the frozen D7 scalar benchmark."""
import argparse
import json
from pathlib import Path
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
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--train", type=Path)
    parser.add_argument("--preflight", action="store_true")
    args = parser.parse_args()
    if args.preflight:
        x = np.linspace(-1.0, 1.0, 64, dtype=np.float64).reshape(-1, 1)
        y = x[:, 0] + 0.25
        X = pd.DataFrame(x, columns=["x0"])
        run_name = f"preflight-{args.seed}"
    else:
        if args.train is None:
            parser.error("--train is required outside preflight")
        X, y = load_csv(args.train)
        run_name = f"{args.task}-{args.track}-{args.seed}"

    before_import = time.monotonic()
    from pysr import PySRRegressor
    import_seconds = time.monotonic() - before_import

    model = PySRRegressor(
        binary_operators=["+", "-", "*", "/"],
        unary_operators=["sin", "exp", "log"],
        populations=1,
        population_size=128,
        niterations=1_000_000,
        maxsize=25,
        maxdepth=8,
        warmup_maxsize_by=0.0,
        timeout_in_seconds=60.0 if args.preflight else 120.0,
        elementwise_loss="L2DistLoss()",
        loss_scale="linear",
        should_optimize_constants=True,
        optimizer_algorithm="BFGS",
        optimizer_nrestarts=2,
        optimizer_f_calls_limit=10_000,
        optimize_probability=0.14,
        parsimony=0.0,
        precision=64,
        random_state=args.seed,
        parallelism="serial",
        deterministic=True,
        run_id=run_name,
        output_directory=str(args.output_directory.resolve()),
        temp_equation_file=False,
        delete_tempfiles=False,
        verbosity=0,
        progress=False,
    )
    start_fit = time.monotonic()
    model.fit(X, y)
    fit_seconds = time.monotonic() - start_fit
    equations = model.equations_.copy()
    if equations.empty:
        raise RuntimeError("PySR returned no equation candidates")
    equations["equation_text"] = equations["equation"].map(str)
    best = equations.sort_values(["loss", "complexity", "equation_text"], kind="mergesort").iloc[0]
    print(json.dumps({
        "framework": "PySR",
        "version": "2.7.2",
        "task": args.task,
        "track": args.track,
        "seed": args.seed,
        "expression": str(best["equation"]),
        "native_training_loss": float(best["loss"]),
        "native_complexity": int(best["complexity"]),
        "candidate_rows": int(len(equations)),
        "import_seconds": import_seconds,
        "fit_seconds": fit_seconds,
        "preflight": args.preflight,
    }, sort_keys=True, allow_nan=False))


if __name__ == "__main__":
    main()
