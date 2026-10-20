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

if fields(original) != fields(reproduced):
    raise SystemExit("FAIL: score, discovery, stop, or work counters differ")
if campaign[position + 2] != next(line for line in file_run if line.startswith("nodes=")):
    raise SystemExit("FAIL: faithful expression or complexity differ")
if next(line for line in campaign if line.startswith("execution=")) != next(line for line in file_run if line.startswith("execution=")):
    raise SystemExit("FAIL: effective configuration differs")
if not any(line.startswith("x0=u ") for line in file_run):
    raise SystemExit("FAIL: expected normalized index binding")
values = fields(reproduced)
print(f"PASS: seed {seed}; exact expression, configuration, scores and all {len(values)} non-time result fields match.")
print(f"Separate reproduction work: {values['training_loss_attempts']} actual loss calls; {values['row_attempts']} row attempts.")
