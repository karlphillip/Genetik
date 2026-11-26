#!/usr/bin/env python3
"""Bounded outer command for the frozen conservation discovery protocol."""
import argparse, hashlib, json, os, pathlib, resource, subprocess, sys, time

def sha(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for block in iter(lambda:f.read(1<<20),b''): h.update(block)
    return h.hexdigest()

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--spec',required=True); p.add_argument('--binary',required=True)
    p.add_argument('--mode',choices=['synthetic','target'],required=True)
    p.add_argument('--output',required=True)
    a=p.parse_args(); spec_path=pathlib.Path(a.spec).resolve(); binary=pathlib.Path(a.binary).resolve()
    spec=json.loads(spec_path.read_text()); target=pathlib.Path(a.output)
    package_manifest=spec_path.parent/'manifest.json'
    frozen=None
    if package_manifest.exists():
        frozen=json.loads(package_manifest.read_text())
        expected=frozen['artifacts']
        for name,digest in expected.items():
            if sha(spec_path.parent/name)!=digest: raise SystemExit(f'frozen artifact changed: {name}')
        if frozen['binary_sha256']!=sha(binary): raise SystemExit('campaign binary differs from frozen binary')
        if frozen['artifacts'].get('study.json')!=sha(spec_path): raise SystemExit('study specification differs from frozen package')
    ceiling=spec['resource_limits_per_invocation']
    allowed=set(ceiling['cpu_affinity']); available=set(os.sched_getaffinity(0))
    if not allowed or not allowed <= available: raise SystemExit('declared affinity exceeds effective process affinity')
    if a.mode=='synthetic': tasks=[(s,spec['synthetic_seed']) for s in spec['systems']]
    else: tasks=[(s,seed) for s in spec['systems'] for seed in spec['seeds']]
    records=[]
    def invoke(command):
        start=time.monotonic()
        def limits():
            os.sched_setaffinity(0,allowed)
            resource.setrlimit(resource.RLIMIT_CPU,(ceiling['cpu_seconds'],ceiling['cpu_seconds']+1))
            resource.setrlimit(resource.RLIMIT_AS,(ceiling['address_space_bytes'],ceiling['address_space_bytes']))
        try:
            proc=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=ceiling['wall_seconds'],preexec_fn=limits)
            out=proc.stdout.decode(errors='replace').strip(); err=proc.stderr.decode(errors='replace').strip()
            if len(proc.stdout)+len(proc.stderr)>ceiling['stdout_bytes']:
                return {'command':command,'returncode':proc.returncode,'wall_seconds':time.monotonic()-start,
                        'stdout':out,'stderr':err,'status':'output_limit'}
            return {'command':command,'returncode':proc.returncode,'wall_seconds':time.monotonic()-start,
                    'stdout':out,'stderr':err,'status':'completed' if proc.returncode==0 else 'failed'}
        except subprocess.TimeoutExpired as e:
            return {'command':command,'returncode':None,'wall_seconds':time.monotonic()-start,
                    'stdout':(e.stdout or b'').decode(errors='replace'),
                    'stderr':(e.stderr or b'').decode(errors='replace'),'status':'timeout'}
    for system,seed in tasks:
        command=[str(binary),system,str(seed),'synthetic' if a.mode=='synthetic' else 'target']
        record=invoke(command)
        record.update({'system':system,'seed':seed})
        records.append(record)
    target.parent.mkdir(parents=True,exist_ok=True)
    selected=[]
    for r in records:
        if r['status']=='completed':
            try:
                item=json.loads(r['stdout']); item['system']=r['system']; item['seed']=r['seed']; selected.append(item)
            except json.JSONDecodeError: r['status']='invalid_search_output'
    frozen={'mode':a.mode,'spec_sha256':sha(spec_path),'adapter_sha256':sha(pathlib.Path(__file__).resolve()),
            'binary_sha256':sha(binary),'selected':selected}
    frozen_path=target.with_name(target.stem+'.selected.json')
    frozen_path.write_text(json.dumps(frozen,sort_keys=True,indent=2)+'\n')
    verifications=[]
    for item in selected:
        if not item.get('valid'):
            verifications.append({'system':item['system'],'seed':item['seed'],'status':'missing_model'}); continue
        v=invoke([str(binary),'verify',item['system'],item['expression']])
        v.update({'system':item['system'],'seed':item['seed']})
        if v['status']=='completed': v['verification']=json.loads(v['stdout'])
        verifications.append(v)
    if a.mode=='target':
        d=invoke([str(binary),'damped','0','target']); d.update({'system':'damped','seed':0})
        if d['status']=='completed': d['control']=json.loads(d['stdout'])
        verifications.append(d)
    manifest={'mode':a.mode,'spec_sha256':sha(spec_path),'adapter_sha256':sha(pathlib.Path(__file__).resolve()),
              'binary_sha256':sha(binary),'selection_artifact':str(frozen_path),
              'selection_artifact_sha256':sha(frozen_path),'records':records,'verifications':verifications}
    target.write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')
    print(json.dumps({'mode':a.mode,'spec_sha256':manifest['spec_sha256'],'adapter_sha256':manifest['adapter_sha256'],
                      'binary_sha256':manifest['binary_sha256'],'records':len(records),
                      'failures':sum(r['status']!='completed' for r in records),'output':str(target),
                      'selection_artifact':str(frozen_path)},sort_keys=True))
    return 0 if all(r['status']=='completed' for r in records) and all(v.get('status')=='completed' or v.get('status')=='missing_model' for v in verifications) else 1
if __name__=='__main__': sys.exit(main())
