# Optional smooth scalar operators

Genetik provides six opt-in unary scalar operators: `asinh`, `acosh`, `atanh`,
`sigmoid`, `softplus`, and `sinc`. They are available through the existing
`Expression` representation and parser; no second expression language is added.

```cpp
#include <ExpressionDerivative.hpp>
#include <ExpressionParsing.hpp>

#include <array>

int main() {
    const auto model = parseExpression("sigmoid(x0) + sinc(x1)", 2);
    const auto point = std::array{0.0, 0.0};
    const auto value = model.evaluate(point);
    const auto slope = evaluatePartial(model, point, 0);
    return value.valid() && slope.derivativeAvailable() && value.value() == 1.5 ? 0 : 1;
}
```

Build external consumers by linking `Genetik::Genetik`, which supplies C++20 and
Genetik's strict floating-point options. A complete executable example is
[`smooth_operators.cpp`](../examples/quickstarts/smooth_operators.cpp).

## Definitions and domains

- `asinh(x)` is defined for every finite real input.
- `acosh(x)` requires `x >= 1`; its derivative is singular at `x = 1`.
- `atanh(x)` requires `-1 < x < 1`; its derivative grows without bound toward
  either endpoint.
- `sigmoid(x) = 1 / (1 + exp(-x))`, evaluated with sign-dependent forms to avoid
  overflow.
- `softplus(x) = log(1 + exp(x))`, evaluated using `max(x, 0) +
  log1p(exp(-abs(x)))` to retain finite tails.
- `sinc(x) = sin(x) / x` for nonzero `x`, and exactly `1` at either signed zero.

Inputs and outputs must be finite under the normal checked expression contract.
Each scalar unary node accounts for one mathematical operation, consistent with
existing scalar unary functions; derivative arithmetic and native math calls
are separately accounted by the derivative evaluator.

All six functions require dimensionless input and produce dimensionless output.
They are intentionally absent from existing default scalar and typed variation
grammars. Callers can opt in by adding the corresponding `ExpressionKind` or
`TypedExpressionKind` values to their configured grammar. Parsing an explicit
expression does not imply that the operator is enabled for evolutionary search.

Numeric partial derivatives support all six operators. At `acosh(1)`, the primal
value is valid but the derivative is reported unavailable as singular. `sinc`
has derivative zero at zero and uses a local series near zero to avoid
cancellation. Symbolic differentiation of `sinc` is conditional away from zero:
the symbolic result carries a nonzero-argument assumption because the symbolic
language has no piecewise operator for the removable point. Other derivatives
use the ordinary smooth-domain assumptions of their mathematical definitions.

The optional Python regressor also accepts these names in its explicit
`operators` configuration; its defaults are unchanged. Its SymPy exporter emits
the corresponding elementary forms, including the removable value at zero for
Genetik's unnormalized `sinc`.

These operators do not change generic GA behavior, RNG streams, checkpoints,
existing grammar defaults, or expression evaluation order for existing
operators. Their numerical results use the platform's standard-library
transcendental functions under Genetik's required IEEE binary64 and strict
floating-point policy; bit-identical transcendental results across different
standard libraries are not promised.
