from pathlib import Path
import math
out=[]
def add(text,value):out.append((text,str(value) if -(1<<63)<=value<(1<<63) else 'invalid'))
a_values=[-(1<<63),-(1<<63)+1,-100,-7,-1,0,1,7,100,(1<<63)-1]
for a in a_values:
 for b in a_values:
  add(f'gcd({a},{b})',math.gcd(a,b));add(f'lcm({a},{b})',math.lcm(a,b))
  if b:add(f'integer_divide({a},{b})',(a-a%abs(b))//b)
  else:out.append((f'integer_divide({a},0)','invalid'))
for a in [-3,-2,-1,0,1,2,3]:
 for k in list(range(0,65))+[-1]:
  if k<0:out.append((f'integer_power({a},{k})','invalid'))
  else:add(f'integer_power({a},{k})',a**k)
for a in a_values:
 for k in [0,1,2,7,63,9223372036854775807]:
  for m in [1,2,3,17,9223372036854775807]:add(f'modular_power({a},{k},{m})',pow(a,k,m))
for n in range(-1,23):
 if n<0:out.append((f'factorial({n})','invalid'))
 else:add(f'factorial({n})',math.factorial(n))
for n in range(71):
 for k in range(n+1):add(f'binomial({n},{k})',math.comb(n,k))
for n,k in [(9223372036854775807,0),(9223372036854775807,1),(9223372036854775807,2),(128,64)]:add(f'binomial({n},{k})',math.comb(n,k))
for text in ['binomial(-1,0)','binomial(2,-1)','binomial(2,3)','modular_power(2,-1,3)','modular_power(2,3,0)','modular_power(2,3,-1)']:out.append((text,'invalid'))
Path('tests/d5-integer-references.tsv').write_text('\n'.join('\t'.join(row) for row in out)+'\n');print(len(out))
