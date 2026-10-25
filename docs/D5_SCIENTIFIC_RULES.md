# D5 scalar scientific rules

This supplements [the public D5 guide](ADVANCED_MATHEMATICS_D5.md) and
[the numerical partial contract](SCALAR_DERIVATIVES.md). These are scalar
binary64 operations under strict FP, not real-number accuracy guarantees.
All operands are evaluated in original postfix order, including operands of
min/max/clamp. Every required primal/derivative intermediate must be finite;
finite underflow, subnormal and signed-zero results remain valid. Native calls
use the qualified standard-library/libm implementation, with no errno/flag-only
invalidity and no protected arithmetic.

Let `q` denote the checked primal result, `da`/`db` the propagated derivatives.
Rules below are invoked only for structurally target-active subtrees. Failure
in any required target-active child remains unavailable even if a later branch
or multiplication would discard it. Primal evaluation continues after derivative
failure where the remaining work allowance permits. Later primal invalidity
has priority over derivative unavailability.

| Canonical node | Primal/domain | Numerical derivative rule |
|---|---|---|
| `powi(a,-m)` | m=1..8; nonzero a; reciprocal then sequential inverse multiplication | `((-m*q)/a)*da` |
| `pow(a,b)` | positive a for general real b; negative a only if b exactly integral; a=0 only b>0 | active terms `((b*(q/a))*da)` and `((q*ln(a))*db)`, then checked addition |
| `cbrt(a)` | native real cube root, negative arguments permitted | `da/((3*q)*q)`; active a=0 unavailable |
| `log1p(a)` | a>-1 | `da/(1+a)` |
| `log2(a)` | a>0 | `(da/a)/ln(2)` |
| `log10(a)` | a>0 | `(da/a)/ln(10)` |
| `expm1(a)` | native result finite | `exp(a)*da` |
| `tan(a)` | radians; native finite result | `(1+q*q)*da` |
| `asin(a)` | -1<=a<=1 | `da/sqrt((1-a)*(1+a))`; active endpoints unavailable |
| `acos(a)` | -1<=a<=1 | negated asin rule; active endpoints unavailable |
| `atan(a)` | every finite a | small/moderate a: `da/(1+a*a)`; for abs(a)>1, r=1/a and `((r/(1+r*r))*r)*da` |
| `atan2(a,b)` | y=a, x=b; (0,0) invalid | scaled rule below; active a at a=0,b<0 unavailable |
| `sinh(a)` | native finite result | `cosh(a)*da` |
| `cosh(a)` | native finite result | `sinh(a)*da` |
| `tanh(a)` | native finite result | `((1-q)*(1+q))*da` |
| `abs(a)` | native absolute value | select da or -da by sign; active a=0 unavailable |
| `min(a,b)` | left operand wins equal-value/signed-zero ties | selected child derivative; target-active ties unavailable |
| `max(a,b)` | left operand wins equal-value/signed-zero ties | selected child derivative; target-active ties unavailable |
| `clamp(a,l,u)` | l<=u, three strict operands | derivative of selected a/l/u; active boundary equality unavailable |

For atan2, `s=max(abs(a),abs(b))`, `u=a/s`, `v=b/s`, and
`D=u*u+v*v`. The active terms are `((v/D)/s)*da` and
`((-u/D)/s)*db`, accumulated in that order. Scaling avoids unnecessary
squared-magnitude overflow; all scaled intermediates are still checked.

For general Pow at a negative base, an inactive integral exponent permits the
base rule. An active exponent is unavailable. At a zero base, a fixed integral
exponent>=1 yields da for exponent1 or zero for higher exponents. Fractional
active-base cases are conservatively unavailable. An inactive zero base with
positive active exponent is locally constant zero. These rules do not prove
global differentiability. Conservative unavailability does not prove that an
entire composite function lacks a derivative.

Positive metadata Powi0..8 keeps its previous sequential primal multiplication
and derivative recurrence. It is not rewritten into general Pow. The new scalar
parser accepts negative metadata exponents -1..-8; -9 remains rejected. Typed
vector/matrix Powi retains its previous nonnegative contract. The malformed-text
fixture was updated to reflect this explicit syntax extension, with positive
scalar and unchanged typed rejection checks.

Each native scalar node costs one primal operation; NegativePowi costs m.
Derivative arithmetic and additional native calls are separately counted by
`DerivativeWork`; allocation/node visits/binding validation retain independent
limits. Native argument classification is part of the node operation. Arithmetic
counts are not CPU-cycle bounds. Intermediate underflow and poor conditioning
can lose accuracy even when the computed derivative is finite. No universal
error bound follows from a successful result.

The standard canonical export retains all coefficient bits and explicit node
identities. Unicode and LaTeX are presentation only; nested signed/general
powers are grouped before an enclosing power, preserving the exponent tree.
There is no algebraic reassociation or silent executable simplification.

The frozen70 analytic value/derivative references use mpmath1.2.1 at100 decimal
digits and exact binary64 input identities. Absolute1e-14 plus relative1e-13 is
a deliberately conservative fixture tolerance for these few native/composite
operations, including reference conversion and ordinary rounding. It is not a
bound for arbitrary trees or extreme inputs. No finite difference or production
AD helper generates the oracle. Regenerate explicitly with
`scripts/references/d5_scientific.py`; this optional reference tool requires
mpmath, while the C++ library has no numerical dependency.
