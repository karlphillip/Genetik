#!/usr/bin/env python3
"""Summarize every frozen scientific seed; verify predeclared file reproduction."""
import argparse,json,re
from pathlib import Path

def fields(line):return dict(re.findall(r'(\w+)=([^\s]+)',line))
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);p.add_argument('--file-output',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    results=[]
    for case in ('decay','oscillation','root'):
        runs=[];current=None
        for line in (a.directory/(case+'.txt')).read_text().splitlines():
            if line.startswith('seed='):
                current=dict(case=case,seed=int(fields(line)['seed']),metrics={},trace=[]);runs.append(current)
            elif current is not None:
                if line.startswith('raw_initial_mse='):current['search']=fields(line)
                elif line.startswith('nodes='):
                    prefix,expression=line.split(' expression=',1);current.update(fields(prefix));current['expression']=expression
                elif line.startswith('constant_values='):current['parameters']=fields(line)
                elif line.startswith('dataset='):r=fields(line);current['metrics'][r['dataset']]=r
                elif line.startswith('trace '):current['trace'].append(fields(line))
        if [r['seed'] for r in runs]!=[0,1,2,3,42]:raise RuntimeError('missing seed')
        results+=runs
    if a.file_output:
        reference=next(r for r in results if r['case']=='decay' and r['seed']==2)
        text=a.file_output.read_text().splitlines();search=next(fields(l) for l in text if l.startswith('raw_initial_mse='))
        prefix,expression=next(l for l in text if l.startswith('nodes=')).split(' expression=',1)
        if expression!=reference['expression'] or fields(prefix)['nodes']!=reference['nodes']:raise RuntimeError('file expression mismatch')
        expected={k:v for k,v in reference['search'].items() if k!='seconds'}
        actual={k:v for k,v in search.items() if k!='seconds'}
        if actual!=expected:raise RuntimeError('file counters/fitness mismatch: '+str([(k,v,actual.get(k)) for k,v in expected.items() if actual.get(k)!=v]))
        print('Predeclared decay seed2 file expression, fitness and every deterministic printResult field match.')
    a.output.write_text(json.dumps(results,indent=2)+'\n')
    for r in results:
        metrics=r['metrics'];print(r['case'],r['seed'],r.get('expression','none'),r['search']['training_mse'],*[metrics.get(k,{}).get('max_abs_error','invalid') for k in ('training','interior','exterior')])
if __name__=='__main__':main()
