#!/usr/bin/env python3
"""Development-only independent analytic integral references; Python stdlib Decimal."""
import argparse
from decimal import Decimal, localcontext
import json
from pathlib import Path

def values(precision):
    with localcontext() as context:
        context.prec = precision
        def atan_inverse(n):
            x = Decimal(1) / n
            term = x
            total = x
            k = 1
            while True:
                term *= -x*x
                updated = total + term / (2*k+1)
                if updated == total:
                    return total
                total = updated
                k += 1
        pi = 16*atan_inverse(5) - 4*atan_inverse(239)
        return {'t_squared': Decimal(1)/3, 'exp': Decimal(1).exp()-1,
                'rational': pi/4, 'sqrt': Decimal(2)/3, 'log_1_2': 2*Decimal(2).ln()-1}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    low, high = values(80), values(120)
    with localcontext() as context:
        context.prec = 60
        assert {k: str(+v) for k,v in low.items()} == {k: str(+v) for k,v in high.items()}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({'method': 'Analytic antiderivatives; Decimal exp/ln, pi from Machin 16*atan(1/5)-4*atan(1/239)',
        'precision': [80,120], 'stable_digits': 60,
        'values': {k:str(v) for k,v in high.items()}, 'comparison_absolute_tolerance': 2e-10}, indent=2)+'\n')
