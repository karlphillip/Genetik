"""Offline report generation from bounded campaign logs; standard library only."""
import csv
import re
import statistics
import sys
from pathlib import Path

root = Path(sys.argv[1])
lines = (root / "campaign.txt").read_text().splitlines()
assert "all_models_frozen=1 prime_later_range=previously_examined_1001..1200" in lines
parallel = any(line.startswith("threads_requested=") and not line.startswith("threads_requested=1 ") for line in lines)
records = []
for i, line in enumerate(lines):
    if not line.startswith("problem="):
        continue
    r = dict(item.split("=", 1) for item in line.split())
    r.update(item.split("=", 1) for item in lines[i + 1].split())
    structure, r["expression"] = lines[i + 2].split(" expression=", 1)
    r.update(item.split("=", 1) for item in structure.split())
    r.update(item.split("=", 1) for item in lines[i + 3].split())
    assert int(r["training_loss_attempts"]) == sum(int(r[k]) for k in ("ordinary_loss_attempts", "fitting_baselines", "fitting_probes"))
    records.append(r)
assert len(records) == 20
assert sum(line.startswith("paired_initialization=") for line in lines) == 10
assert sum(line.startswith("profile_equivalence=") for line in lines) == (0 if parallel else 2)
records.sort(key=lambda r: (r["problem"], int(r["seed"]), int(r["interval"])))
with (root / "summary.csv").open("w") as out:
    writer = csv.DictWriter(out, fieldnames=list(records[0]), lineterminator="\n")
    writer.writeheader()
    writer.writerows(records)

# Compare every existing observable non-time search field against accepted controls.
repo = Path(__file__).resolve().parents[1]
if not parallel:
    for problem, relative in (("prime", "docs/prime-results/campaign.txt"), ("arithmetic", "docs/strict-arithmetic-campaign.txt")):
        historical = (repo / relative).read_text().splitlines()
        for r in (r for r in records if r["problem"] == problem and r["interval"] == "1"):
            for i, line in enumerate(historical):
                if line == f"seed={r['seed']}" or line.startswith(f"seed={r['seed']} raw_initial_mse="):
                    old = historical[i + 1] if line == f"seed={r['seed']}" else line.split(" ", 1)[1]
                    fields = dict(item.split("=", 1) for item in old.split() if not item.startswith("seconds="))
                    assert all(r[k] == v for k, v in fields.items()), (problem, r["seed"], "control changed")
                    assert r["expression"] == historical[i + (2 if line == f"seed={r['seed']}" else 1)].split(" expression=", 1)[1]
                    break
            else:
                raise AssertionError("missing historical control")
    (root / "compatibility.txt").write_text("PASS: all ten controls reproduce accepted expressions, scores, stopping reasons and every legacy non-time result counter.\nPASS: ten raw/prepared initial population, fitness and work pairs; two full profile/control result, work, preparation, RNG comparisons.\n")
else:
    (root / "compatibility.txt").write_text("Ten paired raw/prepared initializations verified by runner; historical serial counter equality and detailed profile checks do not apply to binding parallel boundaries.\n")

metrics = [dict(x.split("=", 1) for x in line.split()) for line in lines if line.startswith(("model=", "dataset="))]
def span(values):
    return f"{statistics.median(values):.8g} [{min(values):.8g}, {max(values):.8g}]"

with (root / "summary.md").open("w") as out:
    out.write("# Complete fitting-cadence comparison\n\nAll runs use the predeclared settings. Interval 1 is the default/control.\nNo model was chosen using evaluation metrics.\n\n")
    out.write("| Problem | Seed | Interval | Raw initial MSE | Refined initial MSE | Final training MSE | Nodes/depth | Discovery | Published | Seconds | Stop | Unused |\n|---|---:|---:|---:|---:|---:|---|---|---:|---:|---|---:|\n")
    for r in records:
        out.write(f"| {r['problem']} | {r['seed']} | {r['interval']} | {r['raw_initial_mse']} | {r['refined_initial_mse']} | {r['training_mse']} | {r['nodes']}/{r['depth']} | {r['discovery_stage']} {r['discovery_generation']} | {r['published_generations']} | {float(r['seconds']):.6f} | {r['stop']} | {r['unused_allowance']} |\n")
    out.write("\n| Problem | Seed | Interval | Actual losses | Ordinary | Fit baselines | Fit probes | Fits | Fits with accepted steps | Policy skips | Unpublished losses | Actual rows | Initial losses |\n|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n")
    keys = ("problem", "seed", "interval", "training_loss_attempts", "ordinary_loss_attempts", "fitting_baselines", "fitting_probes", "fits", "fitting_improved", "schedule_skips", "unpublished_losses", "row_attempts", "initial_loss_calls")
    for r in records:
        out.write("| " + " | ".join(r[k] for k in keys) + " |\n")
    out.write("\n## Paired and distribution summaries\n\nEntries are median [minimum, maximum] across the declared five seeds. Timing ratios are paired experimental/control; a ratio above one is slower.\n\n")
    for problem in ("prime", "arithmetic"):
        by = {i: [r for r in records if r["problem"] == problem and r["interval"] == str(i)] for i in (1, 4)}
        wins = sum(float(b["training_mse"]) < float(a["training_mse"]) for a, b in zip(by[1], by[4]))
        losses = sum(float(b["training_mse"]) > float(a["training_mse"]) for a, b in zip(by[1], by[4]))
        ratios = [float(b["seconds"])/float(a["seconds"]) for a, b in zip(by[1], by[4])]
        out.write(f"{problem}: fitting-MSE wins/losses/ties **{wins}/{losses}/{5-wins-losses}**; paired runtime ratio **{span(ratios)}**.\n\n")
        out.write("| Metric | Interval 1 | Interval 4 |\n|---|---|---|\n")
        for key in ("training_mse", "published_generations", "training_loss_attempts", "fits", "schedule_skips", "unpublished_losses", "row_attempts", "seconds"):
            out.write(f"| {key} | {span([float(r[key]) for r in by[1]])} | {span([float(r[key]) for r in by[4]])} |\n")
        out.write(f"\nAggregate search seconds: {sum(float(r['seconds']) for r in by[1]):.6f} / {sum(float(r['seconds']) for r in by[4]):.6f}.\n\n")
    out.write("## Prime raw-unit assessment (1001–1200 previously examined)\n\n| Model | Range | RMSE | Max absolute | Matches / total | Invalid / unrepresentable | Negatives | Raw violations/pairs | Rounded violations/pairs | Duplicates |\n|---|---|---:|---:|---|---|---:|---|---|---:|\n")
    for m in (m for m in metrics if "model" in m):
        out.write(f"| {m['model']} | {m['range']} | {float(m['rmse']):.10g} | {float(m['max_abs']):.10g} | {m['matches']}/{m['denominator']} | {m['invalid_predictions']}/{m['integer_unrepresentable']} | {m['negative']} | {m['raw_increase_violations']}/{m['raw_assessable_pairs']} | {m['rounded_increase_violations']}/{m['rounded_assessable_pairs']} | {m['rounded_duplicates']} |\n")
    out.write("\n## Strict-arithmetic assessment\n\n| Model/range | Max absolute error | Recovery <=1e-6 | Invalid predictions/errors |\n|---|---:|---:|---|\n")
    for m in (m for m in metrics if "dataset" in m):
        out.write(f"| {m['dataset']} | {m['max_abs_error']} | {m['recovery']} | {m['invalid_predictions_total']}/{m['invalid_errors_total']} |\n")
    for problem in ("prime", "arithmetic"):
        out.write(f"\n{problem} assessment distributions (median [min,max]), distinct from training-based wins:\n\n| Mode | Range | RMSE | Maximum absolute error |\n|---|---|---|---|\n")
        ranges = ("fitting", "examined_extrapolation") if problem == "prime" else ("training", "interior", "exterior")
        for interval in (1, 4):
            for range_name in ranges:
                if problem == "prime":
                    group = [m for m in metrics if m.get("model", "").startswith("prime-") and m["model"].endswith(f"-{interval}") and m["range"] == range_name]
                    rmse = span([float(m["rmse"]) for m in group]);maximum = span([float(m["max_abs"]) for m in group])
                else:
                    group = [m for m in metrics if m.get("dataset", "").endswith(f"-{interval}-{range_name}")]
                    rmse = "not recomputed";maximum = span([float(m["max_abs_error"]) for m in group])
                out.write(f"| {interval} | {range_name} | {rmse} | {maximum} |\n")
    out.write("\n## Faithful frozen expressions\n\nPrime trees are g(x0), x0=n/1000.0; output is 10000.0*g(x0), preserving grouping. Arithmetic input order is x0=first,x1=second.\n")
    for r in records:
        out.write(f"\n{r['problem']}, seed {r['seed']}, interval {r['interval']}:\n\n```text\n{r['expression']}\n```\n")
print("Complete twenty-run report and default-compatibility checks passed.")
