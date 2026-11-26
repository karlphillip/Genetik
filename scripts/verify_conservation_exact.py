#!/usr/bin/env python3
"""Recheck frozen conservation selections without rerunning search (SymPy exact rationals)."""
import json
import re
import sys
from pathlib import Path

import sympy as sp

X0, X1 = sp.symbols("x0 x1")


def parse(text):
    # Frozen expressions use only numeric literals, x0/x1, and +-*/ parentheses.
    if not re.fullmatch(r"[x0-9eE.+*/()\-\s]+", text):
        raise ValueError("unsupported expression syntax")
    return sp.sympify(text, locals={"x0": X0, "x1": X1}, rational=True)


def verify(expr_text, system):
    f = sp.Poly(sp.expand(parse(expr_text)), X0, X1)
    if system == "harmonic":
        energy, velocity, acceleration = X0**2 + X1**2, X1, -X0
    elif system == "quartic":
        energy, velocity, acceleration = X0**4 + X1**2, X1, -2 * X0**3
    elif system == "linear-shear":
        energy, velocity, acceleration = None, sp.Integer(1), sp.Integer(-3)
    elif system == "linear-unit-shear":
        energy, velocity, acceleration = None, sp.Integer(1), sp.Integer(-1)
    else:
        raise ValueError(f"unsupported dynamics: {system}")
    if f.is_zero or f.total_degree() == 0:
        nonconstant = False
    else:
        nonconstant = True
    dy, dv = sp.diff(f.as_expr(), X0), sp.diff(f.as_expr(), X1)
    lie = sp.Poly(sp.expand(dy * velocity + dv * acceleration), X0, X1)
    # Solve F=aE+b by exact coefficient comparison. The nonconstant energy
    # coefficients uniquely determine a; every other nonconstant coefficient
    # must then match, and the constant difference is unrestricted.
    energy_terms = ({m: c for m, c in sp.Poly(energy, X0, X1).terms() if m != (0, 0)}
                    if energy is not None else {})
    candidate_terms = {m: c for m, c in f.terms() if m != (0, 0)}
    if energy_terms:
        pivot = next(iter(energy_terms))
        a = candidate_terms.get(pivot, sp.S.Zero) / energy_terms[pivot]
        union = set(energy_terms) | set(candidate_terms)
        affine = bool(a != 0 and all(candidate_terms.get(m, 0) == a * energy_terms.get(m, 0)
                                      for m in union))
        numeric = sp.simplify(f.as_expr() - a * energy)
    else:
        a, numeric, affine = None, None, False
    return {
        "verification_status": "resolved",
        "supported_polynomial": True,
        "nonconstant": nonconstant,
        "exact_conservation": bool(nonconstant and lie.is_zero),
        "exact_lie_derivative": str(lie.as_expr()),
        "exact_affine_reference_recovery": bool(nonconstant and affine),
        "affine_scale": str(a) if affine else None,
        "affine_offset": str(numeric) if affine else None,
    }


def self_test():
    e = X0**2 + X1**2
    for expr in (e, 3 - 2 * e):
        result = verify(str(expr), "harmonic")
        assert result["exact_conservation"] and result["exact_affine_reference_recovery"]
    square = verify(str(e**2), "harmonic")
    assert square["exact_conservation"] and not square["exact_affine_reference_recovery"]
    bad = verify(str(X0**2), "harmonic")
    assert not bad["exact_conservation"] and not bad["exact_affine_reference_recovery"]
    shear = verify(str(3 * X0 + X1), "linear-shear")
    assert shear["exact_conservation"] and not shear["exact_affine_reference_recovery"]
    compact = verify(str(X0 + X1), "linear-unit-shear")
    assert compact["exact_conservation"] and compact["exact_lie_derivative"] == "0"


def main():
    if len(sys.argv) == 2 and sys.argv[1] == "--self-test":
        self_test()
        print("exact conservation verifier fixtures passed")
        return
    if len(sys.argv) == 4 and sys.argv[1] == "--expression":
        print(json.dumps(verify(sys.argv[3], sys.argv[2]), sort_keys=True))
        return
    root = Path(__file__).resolve().parents[1]
    source = root / "research/conservation_discovery/results/target-runs-v2.json"
    data = json.loads(source.read_text())
    records = []
    for item in data["records"]:
        out = item.get("stdout", "")
        if '"candidate_evaluations"' not in out:
            continue
        selection = json.loads(out)
        try:
            result = verify(selection["expression"], selection["system"])
        except Exception as error:  # unsupported algebra and exhausted exact arithmetic
            result = {"verification_status": "inconclusive", "reason": str(error),
                      "supported_polynomial": None, "nonconstant": None,
                      "exact_conservation": None,
                      "exact_affine_reference_recovery": None}
        records.append({"system": selection["system"], "seed": selection["seed"],
                        "expression": selection["expression"], **result})
    if len(records) != 6:
        raise RuntimeError(f"expected six saved selections, found {len(records)}")
    destination = root / "research/conservation_discovery/results/exact-verification-v3.json"
    destination.write_text(json.dumps({"source": "target-runs-v2.json",
                                      "search_rerun": False,
                                      "verifier": f"SymPy {sp.__version__} exact rational polynomial coefficients",
                                      "records": records}, indent=2) + "\n")
    print(destination)


if __name__ == "__main__":
    main()
