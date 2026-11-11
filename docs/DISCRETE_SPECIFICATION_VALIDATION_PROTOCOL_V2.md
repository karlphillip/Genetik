# Discrete specification validation: frozen control amendment

**Protocol:** `DISCRETE_SPECIFICATION_VALIDATION_PROTOCOL.md` version 1<br>
**Amendment:** version 2, frozen before implementation and any validation
execution<br>
**Reason:** independent architecture review found that the initial conditional
control exercised both selected branches but did not prove unselected-branch
laziness. Version 1 remains unchanged and retained; this amendment adds the
following required controls without changing its hypothesis, acceptance gate,
budgets, or existing controls.

## Additional frozen controls

| ID | Candidate and declared finite behavior | Frozen expected outcome |
|---|---|---|
| `lazy-true-branch` | `if(x0 < 0, x0 + 1, x0 % 0)` on the single row `x0=-1`, expected `0` | `ValidatedFiniteDomain`; the invalid modulo expression in the unselected branch must not be evaluated |
| `lazy-false-branch` | `if(x0 < 0, x0 % 0, x0 + 1)` on the single row `x0=0`, expected `1` | `ValidatedFiniteDomain`; the invalid modulo expression in the unselected branch must not be evaluated |

The independent expected values are direct integer arithmetic. The actual
Genetik evaluator remains the implementation under test, not the oracle for
expected values. These cases establish lazy branch behavior only at the stated
inputs and make no broader equivalence claim.
