#!/usr/bin/env python3
"""Create an immutable-by-convention study package after a successful rehearsal."""
import hashlib,json,pathlib,shutil,sys
ROOT=pathlib.Path(__file__).resolve().parent
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    if len(sys.argv) not in (3,4): raise SystemExit('usage: freeze_study.py SYNTHETIC_REHEARSAL.json BINARY [DESTINATION]')
    rehearsal=pathlib.Path(sys.argv[1]).resolve(); binary=pathlib.Path(sys.argv[2]).resolve()
    r=json.loads(rehearsal.read_text())
    source=ROOT/'conservation_study.cpp'; adapter=ROOT/'run_study.py'; spec=ROOT/'study.json'
    scorer=ROOT.parent.parent/'genetik'/'ConservationDiscovery.hpp'
    if r.get('mode')!='synthetic' or any(x.get('status')!='completed' for x in r.get('records',[])+r.get('verifications',[])):
        raise SystemExit('successful synthetic rehearsal required')
    if r.get('spec_sha256')!=sha(spec) or r.get('adapter_sha256')!=sha(adapter) or r.get('binary_sha256')!=sha(binary):
        raise SystemExit('rehearsed spec, adapter, or binary differs from freeze candidates')
    destination=pathlib.Path(sys.argv[3]).resolve() if len(sys.argv)==4 else ROOT/'frozen'
    destination.mkdir(exist_ok=False)
    artifacts={'study.json':spec,'run_study.py':adapter,'conservation_study.cpp':source,
               'ConservationDiscovery.hpp':scorer}
    for name,path in artifacts.items(): shutil.copyfile(path,destination/name)
    manifest={'schema':'genetik-frozen-study-v1','rehearsal_sha256':sha(rehearsal),
              'binary_sha256':sha(binary),'artifacts':{name:sha(destination/name) for name in artifacts}}
    (destination/'manifest.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')
    print(json.dumps({'frozen':str(destination),'manifest_sha256':sha(destination/'manifest.json'),**manifest},sort_keys=True))
if __name__=='__main__': main()
