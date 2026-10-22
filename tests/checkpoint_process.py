"""Each phase is a fresh process: no surviving run objects or mutable services."""
from pathlib import Path
import shutil
import subprocess
import sys
exe=Path(sys.argv[1]).resolve();root=Path(sys.argv[2]).resolve()
assert root.name=='checkpoint-fixture'
if root.exists():shutil.rmtree(root)
root.mkdir(parents=True)
for mode in ['phase100','phase40','phase-resume']:
    r=subprocess.run([str(exe),mode,str(root)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
    (root/(mode+'.log')).write_text(r.stdout)
    if r.returncode:raise RuntimeError(r.stdout)
assert (root/'phase100.state').read_bytes()==(root/'phase-resume.state').read_bytes()
print('Fresh-process generation100 == generation40/save/destroy/load/continue100, all serialized deterministic state')
