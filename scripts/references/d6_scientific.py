"""Independent mpmath100-digit values at exact binary64 inputs, not Genetik."""
from pathlib import Path
from fractions import Fraction
import mpmath as m
m.mp.dps=100
rows=[]
for name,fun,values in [('erf',m.erf,[-3.,-.5,0.,.25,1.,3.]),('erfc',m.erfc,[-3.,-.5,0.,.25,1.,3.]),('gamma',m.gamma,[.125,.5,1.,1.5,5.,20.]),('log_gamma',m.loggamma,[.125,.5,1.,1.5,5.,20.]),('bessel_j0',lambda x:m.besselj(0,x),[0.,.125,.5,1.,2.,5.,10.,100.])]:
    for x in values:
        f=Fraction.from_float(x);exact=m.mpf(f.numerator)/f.denominator
        rows.append(f'{name}(x0)\t{x!r}\t{float(fun(exact))!r}')
Path('tests/d6-scientific-references.tsv').write_text('\n'.join(rows)+'\n')
print(len(rows))
