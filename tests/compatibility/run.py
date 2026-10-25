from pathlib import Path
import subprocess,sys,tempfile
folder=Path(sys.argv[3]);folder.mkdir(parents=True,exist_ok=True);folder=Path(tempfile.mkdtemp(prefix='execution-',dir=folder))
for executable in sys.argv[1:3]:subprocess.run([executable,str(folder)],check=True)
