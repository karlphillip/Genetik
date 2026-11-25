#!/usr/bin/env python3
"""Bounded outer runner for frozen autonomous complex symbolic comparisons."""
import argparse,hashlib,json,os,pathlib,resource,subprocess,time

def sha(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for block in iter(lambda:f.read(1<<20),b''):h.update(block)
    return h.hexdigest()

def main():
    p=argparse.ArgumentParser();p.add_argument('--spec',required=True);p.add_argument('--binary',required=True)
    p.add_argument('--source',required=True);p.add_argument('--api-source',required=True)
    p.add_argument('--mode',choices=['synthetic','target'],required=True);p.add_argument('--output',required=True)
    a=p.parse_args();spec_path=pathlib.Path(a.spec).resolve();binary=pathlib.Path(a.binary).resolve()
    source=pathlib.Path(a.source).resolve();api_source=pathlib.Path(a.api_source).resolve();spec=json.loads(spec_path.read_text())
    manifest_path=spec_path.parent/'manifest.json';frozen=None
    if manifest_path.exists():
        frozen=json.loads(manifest_path.read_text())
        for name,digest in frozen['artifacts'].items():
            if sha(spec_path.parent/name)!=digest:raise SystemExit(f'frozen artifact changed: {name}')
        if sha(binary)!=frozen['binary_sha256']:raise SystemExit('binary differs from frozen executable')
        if sha(source)!=frozen['artifacts'].get('complex_autonomous_comparison.cpp'):raise SystemExit('source differs from frozen artifact')
        if sha(api_source)!=frozen['artifacts'].get('ComplexSymbolicRegression.hpp'):raise SystemExit('API header differs from frozen artifact')
    limits=spec['resource_limits_per_invocation'];allowed=set(limits['cpu_affinity']);available=set(os.sched_getaffinity(0))
    if not allowed or not allowed<=available:raise SystemExit('declared CPU affinity exceeds effective affinity')
    seeds=[spec['synthetic_seed']] if a.mode=='synthetic' else spec['seeds']
    tasks=[(case,seed) for case in spec['systems'] for seed in seeds];records=[]
    def invoke(case,seed):
        command=[str(binary),case,str(seed),'synthetic' if a.mode=='synthetic' else 'target'];start=time.monotonic()
        def child_limits():
            os.sched_setaffinity(0,allowed)
            resource.setrlimit(resource.RLIMIT_CPU,(limits['cpu_seconds'],limits['cpu_seconds']+1))
            resource.setrlimit(resource.RLIMIT_AS,(limits['address_space_bytes'],limits['address_space_bytes']))
        try:
            run=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=limits['wall_seconds'],preexec_fn=child_limits)
            size=len(run.stdout)+len(run.stderr);status='completed' if run.returncode==0 else 'failed'
            if size>limits['max_captured_output_bytes']:status='output_limit'
            return {'case':case,'seed':seed,'command':command,'status':status,'returncode':run.returncode,
                    'wall_seconds':time.monotonic()-start,'captured_bytes':size,
                    'stdout':run.stdout.decode(errors='replace').strip(),'stderr':run.stderr.decode(errors='replace').strip()}
        except subprocess.TimeoutExpired as e:
            return {'case':case,'seed':seed,'command':command,'status':'timeout','returncode':None,
                    'wall_seconds':time.monotonic()-start,'captured_bytes':len(e.stdout or b'')+len(e.stderr or b''),
                    'stdout':(e.stdout or b'').decode(errors='replace'),'stderr':(e.stderr or b'').decode(errors='replace')}
    for case,seed in tasks:records.append(invoke(case,seed))
    payload={'mode':a.mode,'spec_sha256':sha(spec_path),'adapter_sha256':sha(pathlib.Path(__file__).resolve()),
             'binary_sha256':sha(binary),'source_sha256':sha(source),'api_source_sha256':sha(api_source),'records':records}
    target=pathlib.Path(a.output);target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(payload,sort_keys=True,indent=2)+'\n')
    print(json.dumps({'mode':a.mode,'spec_sha256':payload['spec_sha256'],'adapter_sha256':payload['adapter_sha256'],
                      'binary_sha256':payload['binary_sha256'],'records':len(records),
                      'source_sha256':payload['source_sha256'],'api_source_sha256':payload['api_source_sha256'],
                      'failures':sum(x['status']!='completed' for x in records),'output':str(target)},sort_keys=True))
    return 0 if all(x['status']=='completed' for x in records) else 1
if __name__=='__main__':raise SystemExit(main())
