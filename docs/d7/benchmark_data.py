#!/usr/bin/env python3
"""Generate the frozen D7 primary datasets and SHA256 manifest."""
import csv
import hashlib
import json
from pathlib import Path

import numpy as np


ROOT = Path(__file__).with_name("data")
DATASET_SEED = 20261026
NOISE_SEED = 20261027


def targets(task, x):
    if task == "polynomial_x3":
        return x[:, 0] ** 3 - 2 * x[:, 0] + 1
    if task == "rational_x2":
        return (x[:, 0] + 2) / (x[:, 0] ** 2 + 1)
    if task == "trig_sine":
        return 2 * np.sin(1.5 * x[:, 0] + 0.3)
    if task == "exponential_decay":
        return 3 * np.exp(-0.7 * x[:, 0])
    if task == "fractional_power_positive":
        return 3.5 * x[:, 0] ** 1.7
    if task == "kinetic_energy_mv2":
        return 0.5 * x[:, 0] * x[:, 1] ** 2
    raise ValueError(task)


def make_inputs(rng, task, count):
    domains = {
        "polynomial_x3": [(-2.0, 2.0)],
        "rational_x2": [(-2.0, 2.0)],
        "trig_sine": [(-np.pi, np.pi)],
        "exponential_decay": [(-2.0, 2.0)],
        "fractional_power_positive": [(0.1, 3.0)],
        "kinetic_energy_mv2": [(0.5, 10.0), (0.1, 8.0)],
    }[task]
    return np.column_stack([rng.uniform(low, high, count) for low, high in domains])


def write_csv(path, x, y):
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f, lineterminator="\n")
        writer.writerow([*(f"x{i}" for i in range(x.shape[1])), "y"])
        writer.writerows([*map(float, row), float(target)] for row, target in zip(x, y))


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    tasks = [
        "polynomial_x3", "rational_x2", "trig_sine", "exponential_decay",
        "fractional_power_positive", "kinetic_energy_mv2",
    ]
    split_seeds = np.random.SeedSequence(DATASET_SEED).spawn(2 * len(tasks))
    ns = np.random.Generator(np.random.PCG64(NOISE_SEED))
    entries = []
    for task_index, task in enumerate(tasks):
        ntrain, ntest = (256, 1024) if task == "kinetic_energy_mv2" else (160, 512)
        train_rng = np.random.Generator(np.random.PCG64(split_seeds[2 * task_index]))
        test_rng = np.random.Generator(np.random.PCG64(split_seeds[2 * task_index + 1]))
        xtrain = make_inputs(train_rng, task, ntrain)
        xtest = make_inputs(test_rng, task, ntest)
        ytrain = targets(task, xtrain)
        ytest = targets(task, xtest)
        sigma = 0.01 * float(np.sqrt(np.mean(ytrain * ytrain)))
        noise = ns.normal(0.0, sigma, size=ytrain.shape)
        for name, x, y in (
            ("train-clean", xtrain, ytrain),
            ("train-noise01", xtrain, ytrain + noise),
            ("test", xtest, ytest),
        ):
            path = ROOT / f"{task}-{name}.csv"
            write_csv(path, x, y)
            entries.append({
                "task": task, "split": name, "rows": int(len(y)),
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "path": str(path.relative_to(Path(__file__).parents[2])),
            })
    manifest = {
        "generator": "docs/d7/benchmark_data.py",
        "numpy": np.__version__,
        "bit_generator": "PCG64",
        "dataset_seed": DATASET_SEED,
        "split_seed_scheme": "SeedSequence(dataset_seed).spawn(12), paired train/test per task in task order",
        "noise_seed": NOISE_SEED,
        "noise_sd": "0.01 * RMS(noiseless training targets)",
        "heldout_noise": False,
        "files": entries,
    }
    (ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
