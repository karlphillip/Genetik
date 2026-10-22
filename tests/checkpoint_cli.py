"""Bounded actual-prime human/raw/JSONL/checkpoint/resume acceptance, no recovery gate."""
from pathlib import Path
import json
import shutil
import subprocess
import sys
exe=Path(sys.argv[1]).resolve();root=Path(sys.argv[2]).resolve()
assert root.name=='checkpoint-cli'
if root.exists():shutil.rmtree(root)
root.mkdir(parents=True)
def run(name,args,ok=True):
    p=subprocess.run([str(exe),*args],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
    (root/(name+'.txt')).write_text(p.stdout)
    if (p.returncode==0)!=ok:raise RuntimeError(p.stdout)
    return p.stdout
for i,args in enumerate([['--report-format','json'],['--checkpoint-seconds','0'],['--checkpoint-keep','0'],['--checkpoint-keep','10001'],['--resume','absent.gtkc'],['--formula-precision','0'],['--formula-precision','18'],['--formula-precision','-1'],['--formula-precision','abc'],['--formula-precision','4294967296']]):
    run('invalid'+str(i),['--search-exact',*args],False)
checkpoint=root/'saved';metrics=root/'metrics.jsonl'
out=run('pretty',['--search-exact','--seed','42','--threads','2','--loss-budget','64','--no-fit','--report-seconds','.01','--checkpoint',str(checkpoint),'--metrics-file',str(metrics)])
assert 'Prime Search • Generation 0' in out and 'Exact primes' in out and ' / 1,000' in out
assert '≈' in out and '64 / 64' in out
assert 'rank=' not in out and '#2' not in out and 'Canonical g(x0)' in out and 'LaTeX' in out and 'evaluation_budget' in out
assert (checkpoint/'latest.gtkc').is_file()
records=[json.loads(s) for s in metrics.read_text().splitlines()]
assert len(records)==2 and all(r['schema']=='genetik.prime-progress/1' and len(r['candidates'])==1 for r in records)
assert records[-1]['progress']['actual_loss_attempts']=='64'
assert records[-1]['progress']['reporting_loss_attempts']=='0'
assert records[-1]['candidates'][0]['presentation_fitness']==1-float(records[-1]['candidates'][0]['metrics']['exact_prefix_loss'])/1001
out=run('resumed',['--resume',str(checkpoint),'--loss-budget','128','--report-seconds','.01','--top','2','--formula-precision','3','--metrics-file',str(metrics)])
assert 'Resuming checkpoint' in out and 'RNG state         restored' in out and 'Continuing with generation 1' in out
assert 'Prime Search • Generation 1' in out and '#2 fitness' in out
records=[json.loads(s) for s in metrics.read_text().splitlines()]
assert records[-1]['progress']['actual_loss_attempts']=='128'
assert records[-1]['progress']['committed_generation']=='1'
assert len(records[-1]['candidates'])==2
run('worker-mismatch',['--resume',str(checkpoint),'--threads','4'],False)
run('fit-mismatch',['--resume',str(checkpoint),'--fit-every','4'],False)
run('total-budget-low',['--resume',str(checkpoint),'--loss-budget','127'],False)
raw=run('raw',['--resume',str(checkpoint),'--report-format','raw','--loss-budget','128','--top','1'])
assert 'snapshot=resumed' in raw and 'snapshot=final' in raw and 'rank=1' in raw and 'exact_matches=' in raw
assert 'actual_loss_attempts=128' in raw
print('Human default, explicit top/raw, JSONL, validation and total-budget process resume passed')

# Raw canonical formula and JSONL coefficients do not inherit display precision.
assert records[-1]['candidates'][0]['canonical'] in raw
raw_low=run('raw-precision1',['--resume',str(checkpoint),'--report-format','raw','--formula-precision','1','--loss-budget','128','--top','1'])
assert [line for line in raw.splitlines() if line.startswith('g(x0) = ')]==[line for line in raw_low.splitlines() if line.startswith('g(x0) = ')]
