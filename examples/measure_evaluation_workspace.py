#!/usr/bin/env python3
"""Five paired batches, alternating order; no allocation instrumentation in binaries."""
import argparse
import csv
import pathlib
import statistics
import subprocess
p = argparse.ArgumentParser()
p.add_argument('baseline', type=pathlib.Path)
p.add_argument('candidate', type=pathlib.Path)
p.add_argument('output', type=pathlib.Path)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
# Freeze workload order and request counts in EvaluationCorpus.hpp before measurement.
cases = ['polynomial', 'prime', 'arithmetic', 'mixed', 'near_limit',
         'early_invalid', 'late_invalid', 'alternating_log',
         'positive_zero_division', 'negative_zero_division', 'signed_underflow',
         'subnormal_ratio', 'masked_overflow', 'underflow_domain',
         'residual_overflow', 'sum_overflow', 'higher_variable']
records = []
for case in cases:
    for pair in range(5):
        values = []
        for mode in (['baseline', 'candidate'] if pair % 2 == 0 else ['candidate', 'baseline']):
            binary = getattr(a, mode).resolve()
            row = next(csv.reader([subprocess.check_output([binary, '--timing', case], text=True).strip()]))
            name, requests, rows, checksum, seconds = row
            values.append((requests, rows, checksum))
            records.append([case, pair, mode, requests, rows, checksum, seconds])
        if values[0] != values[1]:
            raise RuntimeError(f'work/checksum mismatch: {case} pair {pair}')
    print(f'paired batches complete: {case}', flush=True)
with (a.output / 'timings.csv').open('w', newline='') as f:
    writer = csv.writer(f, lineterminator='\n')
    writer.writerow(['case', 'pair', 'mode', 'requests', 'rows', 'checksum', 'seconds'])
    writer.writerows(records)
with (a.output / 'timing-summary.csv').open('w', newline='') as f:
    writer = csv.writer(f, lineterminator='\n')
    writer.writerow(['case', 'mode', 'requests_per_batch', 'rows_per_batch', 'median_ns_per_loss', 'min_ns_per_loss', 'max_ns_per_loss'])
    for case in cases:
        for mode in ['baseline', 'candidate']:
            selected = [r for r in records if r[0] == case and r[2] == mode]
            times = [float(r[6]) * 1e9 / int(r[3]) for r in selected]
            writer.writerow([case, mode, selected[0][3], selected[0][4], statistics.median(times), min(times), max(times)])
