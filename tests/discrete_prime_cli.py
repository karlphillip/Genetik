"""Bounded D3 process workflows; assessment/reporting cannot train the model."""
from pathlib import Path
import json,selectors,signal,subprocess,sys,tempfile
exe=Path(sys.argv[1]).resolve();root=Path(sys.argv[2]).resolve();root.mkdir(parents=True,exist_ok=True);root=Path(tempfile.mkdtemp(prefix='execution-',dir=root))
def run(args,expected=0):
    p=subprocess.run([str(exe),*args],text=True,capture_output=True,timeout=240)
    assert p.returncode==expected,(p.returncode,p.stdout,p.stderr)
    return p
assert 'publication' in run(['--help']).stdout
for args in [['--task','bogus'],['--task','primality','--threads','0'],['--task','primality','--report-seconds','nan'],['--task','primality','--loss-budget','31']]:run(args,1)
if Path('/dev/full').exists():
    failed=root/'failed-write';failed.mkdir();(failed/'effective.json').symlink_to('/dev/full')
    run(['--task','primality','--loss-budget','32','--generation-limit','0','--output',str(failed)],1)
    assert not (failed/'checkpoints').exists(),'failed metadata write started search'
for task in ['primality','nth-prime']:
    folder=root/task;args=['--task',task,'--seed','42','--threads','4','--loss-budget','64','--generation-limit','1','--report-seconds','0.000000001','--checkpoint-seconds','0.000000001','--output',str(folder)]
    run(args);r=json.loads((folder/'result.json').read_text());assert r['work']['loss_attempts']==64 and r['generation']==1
    progress=[json.loads(s) for s in (folder/'progress.jsonl').read_text().splitlines()];assert [s['kind'] for s in progress]==['initial','periodic','final'];assert all(s['reporting_loss_attempts']==0 for s in progress)
    run([*args,'--resume',str(folder/'checkpoints/latest.gtkc')]);resumed=json.loads((folder/'result.json').read_text());assert resumed['canonical']==r['canonical'] and resumed['work']==r['work'] and resumed['initial_generation']==1
    other='primality' if task=='nth-prime' else 'nth-prime';run(['--task',other,'--seed','42','--threads','4','--loss-budget','64','--generation-limit','1','--output',str(root/(other+'-wrong')),'--resume',str(folder/'checkpoints/latest.gtkc')],1)
# Signal after an actual flushed committed snapshot, never guessing process startup time.
folder=root/'signal';p=subprocess.Popen([str(exe),'--task','primality','--threads','4','--seed','42','--output',str(folder)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
try:
    ready=selectors.DefaultSelector();ready.register(p.stdout,selectors.EVENT_READ);data=b''
    while b'generation=' not in data:
        assert ready.select(120),'no committed snapshot';chunk=p.stdout.read(1);assert chunk,'early exit';data+=chunk
    p.send_signal(signal.SIGTERM);out,err=p.communicate(timeout=240);assert p.returncode==130,(data+out,err)
    r=json.loads((folder/'result.json').read_text());assert r['stop']=='cancelled' and (folder/'checkpoints/latest.gtkc').is_file()
finally:
    if p.poll() is None:p.kill();p.wait()
print('both tasks: initial/periodic/final, checkpoint resume, identity rejection, signal final save; training unaffected')
