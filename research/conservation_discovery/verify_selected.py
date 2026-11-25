#!/usr/bin/env python3
"""Post-selection verifier; reads only the frozen expression artifact and dynamics."""
import argparse,hashlib,json,pathlib,subprocess
def sha(path): return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--selected',required=True);p.add_argument('--binary',required=True);p.add_argument('--output',required=True);a=p.parse_args()
    selected=pathlib.Path(a.selected).resolve();binary=pathlib.Path(a.binary).resolve();payload=json.loads(selected.read_text());checks=[]
    for model in payload['selected']:
        if not model.get('valid'):
            checks.append({'system':model['system'],'seed':model['seed'],'status':'missing_model'});continue
        run=subprocess.run([str(binary),'verify',model['system'],model['expression']],check=True,text=True,capture_output=True)
        checks.append({'system':model['system'],'seed':model['seed'],'status':'verified','result':json.loads(run.stdout)})
    result={'selected_artifact_sha256':sha(selected),'verifier_binary_sha256':sha(binary),'records':checks}
    pathlib.Path(a.output).write_text(json.dumps(result,sort_keys=True,indent=2)+'\n')
    print(json.dumps({'selected_artifact_sha256':result['selected_artifact_sha256'],'verifier_binary_sha256':result['verifier_binary_sha256'],'records':len(checks),'output':a.output},sort_keys=True))
if __name__=='__main__':main()
