# Discrete specification validation: modulo-domain amendment

**Protocols:** validation protocol versions 1–3<br>
**Amendment:** version 4, frozen before additional qualification runs<br>
**Reason:** the independent modulo fixture includes the familiar `-7 mod 3 = 2`
reference in the validator's finite scope, not only in an evaluator-only
oracle check. Earlier protocol versions remain unchanged. This amendment adds
one row to the existing modulo control without changing any threshold or
acceptance criterion.

For `euclidean-modulo`, retain the ordered rows `x0 = -5,-4,…,5`, then append
`x0=-7` with expected Euclidean remainder `2`. Validation must inspect all 12
declared rows in that order. The separate mathematical assertion
`-7 mod 3 = 2` remains an independently specified reference and must not call
Genetik's evaluator to compute the expected value.
