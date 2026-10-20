"""Compare recorded same-build prime and generic-file runs; elapsed time is excluded."""
from pathlib import Path
import sys

campaign = Path(sys.argv[1]).read_text().splitlines()
file_run = Path(sys.argv[2]).read_text().splitlines()
seed = int(sys.argv[3])
position = campaign.index(f"seed={seed}")
original = campaign[position + 1]
reproduced = next(line for line in file_run if line.startswith("raw_initial_mse="))

def fields(line):
    return {key: value for key, value in (item.split("=", 1) for item in line.split()) if key != "seconds"}

original_fields, reproduced_fields = fields(original), fields(reproduced)
# Later milestones can add counters; every recorded field must still match.
if any(reproduced_fields.get(key) != value for key, value in original_fields.items()):
    raise SystemExit("FAIL: score, discovery, stop, or work counters differ")
if campaign[position + 2] != next(line for line in file_run if line.startswith("nodes=")):
    raise SystemExit("FAIL: faithful expression or complexity differ")
original_config = fields(next(line for line in campaign if line.startswith("execution=")))
reproduced_config = fields(next(line for line in file_run if line.startswith("execution=")))
original_config.setdefault("fit_every", "1")  # accepted historical default
reproduced_config.setdefault("fit_every", "1")
if original_config != reproduced_config:
    raise SystemExit("FAIL: effective configuration differs")
if "ordinary_loss_attempts" in reproduced_fields:
    if int(reproduced_fields["training_loss_attempts"]) != sum(int(reproduced_fields[key]) for key in ("ordinary_loss_attempts", "fitting_baselines", "fitting_probes")):
        raise SystemExit("FAIL: additive work counters do not partition actual loss attempts")
if not any(line.startswith("x0=u ") for line in file_run):
    raise SystemExit("FAIL: expected normalized index binding")
values = fields(reproduced)
print(f"PASS: seed {seed}; exact expression, configuration, scores and all {len(original_fields)} recorded non-time result fields match; additive counters checked when present.")
print(f"Separate reproduction work: {values['training_loss_attempts']} actual loss calls; {values['row_attempts']} row attempts.")
