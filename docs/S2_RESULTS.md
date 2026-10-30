# S2 results

## Scope and outcome

S2 qualified a bounded exact constraint fragment for fixed affine-quantized structures. It did not change evolutionary methods, standard fitting defaults, production dependencies, or launch a prime-successor experiment.

The fixed floor fixture recovered `[1.6,1.65)` exactly as `[8/5,33/20)`. The fixed affine-offset observations recover `[0,1/2)` for beta when alpha is fixed. Shared floor differences are represented as a bounded union of exact intervals; the implementation explicitly declines unbounded or over-budget branching. The exact binary64 gamma dyadic lies in the tested one-transition Wythoff gap region. The manually constructed B2 Wythoff formula matched the independent integer reference over 1,200 transitions (1,000 training, 200 held-out); this is parameter/evaluator verification, not discovery.

The executable witness utility separately reports exact rational feasibility, exact representability of its selected double, interval membership, and exact integer-output verification. Tests establish one interval with no binary64 value by checking adjacent representable endpoints. The general bounded adjacent-float search reports `Unknown` when its step cap is reached; it does not claim absence in that case.

The symbolic simplifier adds one deliberately narrow quotient cancellation with a retained `NonZero` condition for the original denominator. It leaves executable evaluation unchanged. No general rational-function rewrite or arbitrary nested-floor solver is claimed.

## Frozen command and work policy

The constraint test is deterministic, uses exact fixture inputs, at most 1,200 Wythoff transitions, 10,000 observations, 50,000 interval branches, 4 MiB of interval workspace, and one million estimated arithmetic-work units by default. The low-branch fixture confirms the resource outcome. No held-out-driven tuning was performed; the split is only used to report the fixed formula's independent 1,000/200 match counts.

## Qualification record

Detailed command outcomes, compiler/CMake identities, test totals, and checksums are recorded in `s2-results/qualification.json`. `sha256sums.txt` covers the retained S2 evidence and fixed test source. A full-suite result is stated only when the exact command completed successfully; targeted tests are not described as full-suite passes.

## Limitations and next recommendation

The exact arithmetic layer is Boost-backed and opt-in. It is not an installed mandatory dependency. Rational operation work is conservatively estimated and accumulated; it is not an internal limb-operation trace. The supported solver handles one rational parameter in affine forms and finite shared-floor branching, not arbitrary nonlinear expressions, many-parameter polyhedra, transcendental coefficients, or universal binary64 error certification. Symbolic cancellation is a specific polynomial identity, not a complete domain-aware algebra system. Coordinate fitting remains the default and LM remains optional. These APIs are useful for fixed-topology exact mixed regression, but they do not establish autonomous structure discovery or justify a prime-successor campaign.
