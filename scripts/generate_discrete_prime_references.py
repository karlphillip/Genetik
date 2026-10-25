#!/usr/bin/env python3
"""Frozen D3 labels: exact trial division, independently checked by a sieve.
No import of the Genetik candidate evaluator. Run from repository root.
"""
from pathlib import Path
import hashlib,json

def prime(n):
    if n<2:return False
    d=2
    while d*d<=n:
        if n%d==0:return False
        d+=1
    return True

values=[];n=2
while len(values)<1200:
    if prime(n):values.append(n)
    n+=1
sieve=[True]*10001;sieve[0]=sieve[1]=False
for p in range(2,101):
    if sieve[p]:
        for m in range(p*p,10001,p):sieve[m]=False
assert values==[i for i in range(10001) if sieve[i]][:1200]
assert values[999]==7919 and values[1199]==9733
out=Path('tests/discrete-primes.tsv')
out.write_text(''.join(f'{i}\t{p}\n' for i,p in enumerate(values,1)))
labels=Path('docs/discrete-d3-results/data.json');labels.write_text(json.dumps(dict(primes=values,primality_training=[[n,int(prime(n))] for n in range(2,64)],primality_held_out=[[n,int(prime(n))] for n in range(64,256)],primality_exterior=[[n,int(prime(n))] for n in range(256,1025)],nth_training_indices=[1,1000],nth_assessment_indices=[1001,1200],prime_tsv_sha256=hashlib.sha256(out.read_bytes()).hexdigest(),generator='exact Python trial division; independently checked sieve to10000'),indent=2)+'\n')
print(hashlib.sha256(out.read_bytes()).hexdigest(),out)
