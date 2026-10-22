#!/usr/bin/env python3
"""Bounded real-prime periodic durability/SIGKILL/restart demonstration.
Run without concurrent builds/qualification. Owns a new output directory only.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import signal
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--executable', type=Path, required=True)
p.add_argument('--inspector', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--source-commit', help='exact source SHA associated with the executable')
a = p.parse_args()
exe, inspector, root = a.executable.resolve(strict=True), a.inspector.resolve(strict=True), a.output.resolve()
if root.exists():
    p.error('output must be a new directory; existing evidence is never deleted')
root.mkdir(parents=True)
checkpoints = root / 'checkpoints'
protocol = dict(seed=42, threads=4, population=64, fitting='unchanged enabled default',
                initial_total_budget=1000000, resumed_total_budget=2000000,
                report_seconds=.05, checkpoint_seconds=.05, keep=3,
                stop='SIGKILL after >=4 snapshots and three retained nonzero generations; '
                     'fresh process SIGTERM after at least two further committed generations',
                recovery_goal='functionality only; no tuning or exact-prime recovery gate')
(root / 'protocol.json').write_text(json.dumps(protocol, indent=2)+'\n')

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def inspect(path):
    r = subprocess.run([str(inspector), 'inspect', str(path)], stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, text=True, timeout=30)
    if r.returncode:
        raise RuntimeError(r.stderr)
    return json.loads(r.stdout)

def run(name, args, ready, sig):
    log, err = root / (name+'.txt'), root / (name+'.stderr')
    with log.open('wb') as out, err.open('wb') as errors:
        child = subprocess.Popen([str(exe), *map(str, args)], stdout=out, stderr=errors)
        started = time.monotonic()
        try:
            while True:
                if child.poll() is not None:
                    raise RuntimeError(f'process exited before controlled signal: {child.returncode}')
                text = log.read_text()
                generations = [int(s.replace(',', '')) for s in
                               re.findall(r'Prime Search • Generation ([\d,]+)', text)]
                if ready(generations):
                    child.send_signal(sig)
                    status = child.wait(timeout=20)
                    return dict(pid=child.pid, args=list(map(str, args)), signal=sig.name,
                                exit_status=status, elapsed_seconds=time.monotonic()-started,
                                generations_displayed=generations)
                if time.monotonic()-started > 45:
                    raise TimeoutError('bounded demonstration did not reach its declared boundary')
                time.sleep(.01)
        finally:
            if child.poll() is None:
                child.kill()
                child.wait(timeout=10)

first = run('sigkill', ['--search-exact', '--seed', '42', '--threads', '4', '--loss-budget', '1000000',
            '--report-seconds', '.05', '--checkpoint', checkpoints, '--checkpoint-seconds', '.05',
            '--checkpoint-keep', '3', '--metrics-file', root/'initial.jsonl'],
            lambda gs: len(gs)>=4 and len(list(checkpoints.glob('generation-*.gtkc')))==3 and
            all(int(q.stem.split('-')[1])>0 for q in checkpoints.glob('generation-*.gtkc')), signal.SIGKILL)
assert first['exit_status'] == -signal.SIGKILL
assert 'Stopping state' not in (root/'sigkill.txt').read_text(), 'SIGKILL must not claim a final save'
frozen = root / 'killed-checkpoints'
shutil.copytree(checkpoints, frozen)
files = sorted(frozen.glob('generation-*.gtkc'))
assert len(files)==3, 'completed retention should contain three recoverable generations'
verified = {q.name: dict(sha256=digest(q), bytes=q.stat().st_size, restored=inspect(q)) for q in files}
latest = inspect(frozen/'latest.gtkc')
latest_hash = digest(frozen/'latest.gtkc')
assert any(v['sha256']==latest_hash and v['restored']['generation']==latest['generation'] for v in verified.values())
resume = run('resumed-sigterm', ['--resume', checkpoints/'latest.gtkc', '--loss-budget', '2000000',
             '--report-seconds', '.05', '--checkpoint-seconds', '.05', '--checkpoint-keep', '3',
             '--formula-precision', '4', '--metrics-file', root/'resumed.jsonl'],
             lambda gs: len(gs)>=3 and gs[-1]>=latest['generation']+2, signal.SIGTERM)
assert resume['exit_status']==0
final = inspect(checkpoints/'latest.gtkc')
assert final['generation']>=latest['generation']+2 and final['loss_attempts']>=latest['loss_attempts']
assert len(list(checkpoints.glob('generation-*.gtkc')))==3
assert 'Resuming checkpoint' in (root/'resumed-sigterm.txt').read_text()
assert 'Stopping state cancelled' in (root/'resumed-sigterm.txt').read_text()
record = dict(source_commit=a.source_commit, executable_sha256=digest(exe), inspector_sha256=digest(inspector), first_process=first, killed_numbered_checkpoints=verified,
              killed_latest_sha256=latest_hash, restored_durable_state=latest,
              resumed_process=resume, final_state=final, final_latest_sha256=digest(checkpoints/'latest.gtkc'),
              exact_recovery_claim=False, sigkill_final_save=False,
              note='Work after the durable checkpoint was killed; restored counters cannot include unknown discarded work.')
(root/'summary.json').write_text(json.dumps(record, indent=2)+'\n')
print(json.dumps(record, indent=2))
