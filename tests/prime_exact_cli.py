#!/usr/bin/env python3
"""Bounded application-only exact-prefix CLI/cancellation checks; no recovery gate."""
from pathlib import Path
import re
import selectors
import signal
import subprocess
import sys
import time
exe=Path(sys.argv[1]).resolve();workspace=Path(sys.argv[2]).resolve();workspace.mkdir(parents=True,exist_ok=True)
def run(args,ok=True):
    p=subprocess.run([str(exe),*args],cwd=workspace,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=25)
    if (p.returncode==0)!=ok:raise RuntimeError(p.stdout)
    return p.stdout
help_text=run(['--search-exact','--help'])
assert 'publication' in help_text and 'Ctrl+C' in help_text
for option,values in [('--report-seconds',['0','-1','nan','inf','1junk','1e100','1e-1000']),('--top',['0','-1','2junk','18446744073709551616']),('--loss-budget',['0','-1','1junk','18446744073709551616']),('--seed',['-1','4294967296']),('--threads',['0','-1','257'])]:
    for value in values:
        out=run(['--search-exact',option,value],False)
        assert 'snapshot=' not in out and 'mode=search-exact' not in out
for args in [['--top'],['--top','2','--top','3'],['--search-exact'],['--campaign']]:
    if args==['--search-exact']:args=['--search-exact','--search-exact']
    run(['--search-exact',*args],False)
out=run(['--search-exact','--seed','42','--threads','2','--loss-budget','256','--no-fit','--report-seconds','0.000000001','--top','5'])
assert 'snapshot=initial' in out and ('snapshot=final' in out or 'snapshot=winning' in out)
assert 'reporting_loss_attempts=0' in out and 'p_hat(n) = 10000 *' in out
assert 'snapshot=periodic' in out
loss=int(re.search(r'search_complete .*?training_loss_attempts=(\d+)',out).group(1));assert loss<=256
(workspace/'bounded.txt').write_text(out)
# Signal only after an actual committed initial snapshot. Raw pipe reads avoid
# TextIO read-ahead interacting with select; no sleep-based scheduling assumption.
p=subprocess.Popen([str(exe),'--search-exact','--seed','42','--threads','4','--loss-budget','10000000','--no-fit'],cwd=workspace,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
selector=selectors.DefaultSelector();selector.register(p.stdout,selectors.EVENT_READ);captured=b''
try:
    deadline=time.monotonic()+25
    while b'snapshot=initial' not in captured:
        remaining=deadline-time.monotonic()
        if remaining<=0 or not selector.select(remaining):raise RuntimeError('initial snapshot timeout')
        chunk=p.stdout.read(65536)
        if not chunk:raise RuntimeError('process ended before initial publication')
        captured+=chunk
    p.send_signal(signal.SIGINT);tail,err=p.communicate(timeout=20);captured+=tail
    assert p.returncode==0,(p.returncode,err)
    text=captured.decode();assert 'snapshot=final' in text and 'stopping_reason=cancelled' in text
    (workspace/'cancelled.txt').write_text(text)
finally:
    selector.close()
    if p.poll() is None:p.kill();p.communicate()
print('exact-prefix CLI, bounded reporting and safe Ctrl+C passed')
