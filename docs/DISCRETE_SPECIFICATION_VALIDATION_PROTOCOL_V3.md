# Discrete specification validation: frozen coverage amendment

**Protocols:** `DISCRETE_SPECIFICATION_VALIDATION_PROTOCOL.md` v1 and
`DISCRETE_SPECIFICATION_VALIDATION_PROTOCOL_V2.md` v2<br>
**Amendment:** version 3, frozen before additional qualification runs<br>
**Reason:** an independent review identified two explicitly requested
contracts not directly exercised by the earlier frozen rows. The earlier
protocols remain immutable and retained. This amendment adds coverage only; it
does not change the hypothesis, scope, resource thresholds, or acceptance
criteria.

## Additional frozen controls

| ID | Candidate and declared behavior | Frozen expected outcome |
|---|---|---|
| `boolean-output` | Candidate `x0 == 0` on inputs `{-1,0,1}`; expected values `{false,true,false}` | `ValidatedFiniteDomain`, exact Bool comparison for true and false |
| `negative-divisor` | Candidate `x0 % -2` on inputs `{-1,0,1}` | `InvalidCandidate` on the first declared row, with evaluator status `MathematicalInvalid` |
| `reduction-iteration-exhaustion` | The nested-sum control from v1 with a cumulative iteration allowance below its five required iterator-body evaluations | `InconclusiveResourceExhausted`, retaining actual partial iteration work and no certificate |

These tests are deterministic and use the same exact status, work, and replay
requirements as the v1 protocol. No benchmark or sealed assessment data is
introduced.
