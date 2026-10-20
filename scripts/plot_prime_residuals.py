"""Optional offline plot, no Python dependency in the C++ benchmark or library."""
import csv
from pathlib import Path
import sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

root = Path(sys.argv[1])
representative = next(line.split("=")[1] for line in (root / "frozen-models.txt").read_text().splitlines()
                      if line.startswith("representative_seed="))
fig, axes = plt.subplots(2, 1, figsize=(11, 7), sharex=True, constrained_layout=True)
for seed in (0, 1, 2, 3, 42):
    with (root / f"seed-{seed}-predictions.csv").open() as source:
        rows = list(csv.DictReader(source))
    axes[0].plot([int(row["index"]) for row in rows],
                 [float(row["residual"]) if row["residual"] else float("nan") for row in rows],
                 label=f"seed {seed}", linewidth=1)
for name in (f"seed-{representative}", "cubic", "fitted_log"):
    with (root / f"{name}-predictions.csv").open() as source:
        rows = list(csv.DictReader(source))
    axes[1].plot([int(row["index"]) for row in rows],
                 [float(row["residual"]) if row["residual"] else float("nan") for row in rows],
                 label=name, linewidth=1)
for ax in axes:
    ax.axhline(0, color="black", linewidth=.5)
    ax.axvline(1000.5, color="black", linestyle="--", label="end of fitting range")
    ax.set_ylabel("Prediction − reference prime")
    ax.legend(ncol=3, fontsize=8)
    ax.grid(alpha=.2)
axes[0].set_title("Frozen prime-prefix predictors; later indices assessed after selection")
axes[1].set_xlabel("One-based index n")
fig.savefig(root / "residuals.png", dpi=150)
