import sympy as s

x, a, b, c = s.symbols("x a b c", real=True)
families = [
    (a * s.sin(b * x + c), (s.sin(b*x+c), a*x*s.cos(b*x+c), a*s.cos(b*x+c))),
    (a * x**b, (x**b, a*x**b*s.log(x))),
    (a * s.exp(-b*x), (s.exp(-b*x), -a*x*s.exp(-b*x))),
    (a / (1 + b*x**2), (1/(1+b*x**2), -a*x**2/(1+b*x**2)**2)),
]
for formula, expected in families:
    actual = tuple(s.diff(formula, p) for p in (a, b, c)[:len(expected)])
    assert all(s.simplify(left-right) == 0 for left, right in zip(actual, expected))
shared = a * s.sin(a*x)
assert s.simplify(s.diff(shared, a) - (s.sin(a*x) + a*x*s.cos(a*x))) == 0
print("independent symbolic Jacobian formulas verified: sine, power, exponential, rational, repeated shared parameter")
