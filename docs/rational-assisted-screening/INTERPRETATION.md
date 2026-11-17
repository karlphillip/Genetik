# Screening interpretation addendum

This addendum corrects the interpretation of the frozen screen. The protocol
and raw result identities are retained; in particular, the historical CSV case
key `nonrational_cubic` is unchanged for traceability.

- Ordinary symbolic constant fitting was disabled in this screen. The
  symbolic arm searched structures using its configured discrete constants.
- The hybrid's reserved expressions received their fitted coefficients from
  the constructive rational provider before symbolic search began.
- `sin` was absent from the symbolic search grammar. The sine case therefore
  measures the rational fitter's approximation and its seeded integration, not
  sine discovery by symbolic search.
- The cubic control is an in-family polynomial case: the rational family
  permits a constant denominator, including `Q(x)=1`.
- The rational fitter alone matched the hybrid's held-out quality on every
  rational and cubic task, and it was faster end to end because the hybrid also
  paid for symbolic search.

These results demonstrate optional integration of the constructive fitter
with symbolic initialization. They do not demonstrate improved autonomous
discovery, an improvement over the fitter alone, or superiority over PySR,
Operon, gplearn, or another external framework.
