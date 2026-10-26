import csv,subprocess,sys,os
from pathlib import Path
exe,directory=sys.argv[1:];root=Path(directory);root.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['OPENBLAS_NUM_THREADS']='1';env['OMP_NUM_THREADS']='1'
subprocess.run([exe,'oscillator','0',str(root)],check=True,env=env,timeout=120)
row=next(csv.DictReader((root/'result.tsv').open(),delimiter='\t'))
assert row['stop']=='evaluation_budget',row
assert int(row['attempts'])<=2048 and int(row['generation'])>0,row
assert (root/'final.gtkc').is_file()
print('bounded fitting exhaustion retains final committed model and reports budget stop')
