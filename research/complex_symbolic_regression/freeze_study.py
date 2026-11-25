#!/usr/bin/env python3
"""Freeze the exact rehearsed complex comparison adapter and executable."""
import hashlib,json,pathlib,shutil,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parent
def sha(path):return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
def main():
    if len(sys.argv)!=4:raise SystemExit('usage: freeze_study.py SYNTHETIC_REHEARSAL.json BINARY DESTINATION')
    rehearsal=pathlib.Path(sys.argv[1]).resolve();binary=pathlib.Path(sys.argv[2]).resolve();destination=pathlib.Path(sys.argv[3]).resolve()
    r=json.loads(rehearsal.read_text());spec=ROOT/'study.json';adapter=ROOT/'run_study.py'
    source=pathlib.Path('benchmarks/complex_autonomous_comparison.cpp').resolve()
    api=pathlib.Path('genetik/ComplexSymbolicRegression.hpp').resolve()
    if r.get('mode')!='synthetic' or any(x.get('status')!='completed' for x in r.get('records',[])):
        raise SystemExit('all synthetic rehearsal tasks must complete before freeze')
    if (r.get('spec_sha256')!=sha(spec) or r.get('adapter_sha256')!=sha(adapter) or
        r.get('binary_sha256')!=sha(binary) or r.get('source_sha256')!=sha(source) or
        r.get('api_source_sha256')!=sha(api)):
        raise SystemExit('rehearsed spec, adapter, or executable differs from freeze inputs')
    destination.mkdir(parents=True,exist_ok=False)
    artifacts={'study.json':spec,'run_study.py':adapter,'complex_autonomous_comparison.cpp':source,
               'ComplexSymbolicRegression.hpp':api}
    for name,path in artifacts.items():shutil.copyfile(path,destination/name)
    patch=subprocess.run(['git','diff','HEAD','--binary'],check=True,stdout=subprocess.PIPE).stdout
    manifest={'schema':'genetik-frozen-complex-study-v1','base_sha':'1344982fe798108abf2993ba204cd399d93ab947',
              'source_head_sha':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
              'tracked_working_patch_sha256':hashlib.sha256(patch).hexdigest(),
              'rehearsal_sha256':sha(rehearsal),'binary_sha256':sha(binary),
              'artifacts':{name:sha(destination/name) for name in artifacts}}
    (destination/'manifest.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')
    print(json.dumps({'frozen':str(destination),'manifest_sha256':sha(destination/'manifest.json'),**manifest},sort_keys=True))
if __name__=='__main__':main()
