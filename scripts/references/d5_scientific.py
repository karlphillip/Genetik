from pathlib import Path
from fractions import Fraction
import mpmath as m,json
m.mp.dps=100
def exact(x):f=Fraction.from_float(x);return m.mpf(f.numerator)/f.denominator
functions=[('cbrt',lambda x:m.sign(x)*m.root(abs(x),3),lambda x:1/(3*m.root(abs(x),3)**2),[-2.5,-.5,.25,2.5]),('log1p',m.log1p,lambda x:1/(1+x),[-.5,.25,2.5]),('log2',lambda x:m.log(x,2),lambda x:1/(x*m.log(2)),[.25,1.25,2.5]),('log10',m.log10,lambda x:1/(x*m.log(10)),[.25,1.25,2.5]),('expm1',m.expm1,m.exp,[-2.5,.25,2.5]),('tan',m.tan,lambda x:1/m.cos(x)**2,[-.5,.25,1.25]),('asin',m.asin,lambda x:1/m.sqrt(1-x*x),[-.5,.25,.5]),('acos',m.acos,lambda x:-1/m.sqrt(1-x*x),[-.5,.25,.5]),('atan',m.atan,lambda x:1/(1+x*x),[-2.5,.25,2.5]),('sinh',m.sinh,m.cosh,[-2.5,.25,2.5]),('cosh',m.cosh,m.sinh,[-2.5,.25,2.5]),('tanh',m.tanh,lambda x:1/m.cosh(x)**2,[-2.5,.25,2.5]),('abs',abs,lambda x:m.sign(x),[-2.5,.25,2.5])]
rows=[]
for name,fun,derivative,inputs in functions:
 for value in inputs:
  x=exact(value);rows.append((name+'(x0)',value,float(fun(x)),float(derivative(x))))
for value in [.25,1.25,2.5]:
 x=exact(value);power=exact(1.7);rows.append(('pow(x0,1.7)',value,float(x**power),float(power*x**(power-1))));rows.append(('atan2(x0,2)',value,float(m.atan2(x,2)),float(2/(4+x*x))))
for n in range(1,9):
 for value in [-2.5,.25,2.5]:
  x=exact(value);rows.append((f'powi(x0,-{n})',value,float(x**(-n)),float(-n*x**(-n-1))))
Path('tests/d5-scientific-references.tsv').write_text('\n'.join('\t'.join([e,repr(x),repr(v),repr(d)]) for e,x,v,d in rows)+'\n');Path('docs/d5-results/references/scientific.json').write_text(json.dumps(dict(generator='mpmath '+m.__version__,precision_decimal_digits=100,input_identity='exact Fraction.from_float binary64 dyadic values; exponent1.7 exact binary64',primary_oracle='independent analytic formulas using arbitrary precision; no Genetik helper or finite difference',rows=len(rows),relative_tolerance=1e-13,absolute_tolerance=1e-14),indent=2)+'\n');print(len(rows))
