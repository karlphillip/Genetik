#!/usr/bin/env python3
"""Development-only exact Python integer oracle; no runtime dependency.
Frozen seed 1729, boundary grid and 256 full-width independent signed inputs.
Each row is expression<TAB>integer/bool value or invalid. No production evaluator.
"""
import random
from pathlib import Path
lo, hi = -(1 << 63), (1 << 63) - 1
rng = random.Random(1729)
values = [lo, lo+1, -7, -3, -1, 0, 1, 2, 3, 7, hi-1, hi]
pairs = [(a, b) for a in values for b in values]
pairs += [(rng.randrange(lo, hi+1), rng.randrange(lo, hi+1)) for _ in range(256)]
rows = ['# Exact Python integer references, generator seed 1729; Int64 output range checked after each operation.']
for a, b in pairs:
    for op in ['+', '-', '*', '%', '==', '!=', '<', '<=', '>', '>=']:
        if op == '%':
            v = a % b if b > 0 else None
        elif op == '+': v = a+b
        elif op == '-': v = a-b
        elif op == '*': v = a*b
        elif op == '==': v = a == b
        elif op == '!=': v = a != b
        elif op == '<': v = a < b
        elif op == '<=': v = a <= b
        elif op == '>': v = a > b
        else: v = a >= b
        expected = ('true' if v else 'false') if isinstance(v, bool) else str(v) if v is not None and lo <= v <= hi else 'invalid'
        rows.append(f'({a} {op} {b})\t{expected}')
Path('tests/discrete-arithmetic.tsv').write_text('\n'.join(rows)+'\n')
print(len(rows)-1, 'exact fixtures')
