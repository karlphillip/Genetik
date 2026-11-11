# Research execution gate

Use the repository's `genetik-research-execution` skill for every solver or
scientific campaign. A campaign may not begin until a complete protocol and
all immutable input identities have been frozen by
`scripts/research_execution.py` after a passing
`scripts/scientific_pipeline_preflight.py` receipt exercised the full synthetic
runner, receipt verifier, scorer, and aggregator pipeline.

- Never invoke an external solver directly, including for `--version`, parser
  probes, debugging, retries, or feasibility checks. All solver processes run
  through the harness and consume the same persistent cumulative budget.
- Run the harness's offline tests before freezing an execution protocol. A
  frozen protocol is write-once; use a new protocol ID and evidence directory
  for a genuine amendment. Never reset the ledger to regain budget.
- The end-to-end preflight is a mandatory admission gate. It must use multiple
  controls with distinct synthetic training and assessment artifacts, bind the
  exact protocol control/run matrix, and pin stage executables, adapters,
  dependencies, and runtime. Its immutable receipt/specification must be
  hash-pinned in the protocol; freeze and run fail closed on missing or stale
  evidence. Real or sealed assessment data never enters the synthetic preflight.
- Treat an unresolved reservation, protocol checksum mismatch, binary/library
  hash mismatch, evidence collision, output cap, process limit, or missing tool
  as a stop. Do not edit state to continue; preserve the incomplete evidence.
- Solver outcomes remain distinct (`successful`, `unsatisfiable`, `unknown`,
  `timed_out`, `invalid`, `resource_exhausted`). Timeouts and exhaustion are
  incomplete results, not UNSAT or mathematical failures.
- Assessment data is frozen and hash-pinned separately from training. It must
  not enter query construction, candidate selection, or refinement.

See [the full research execution skill](../../genetik-research-execution/SKILL.md)
for protocol fields, commands, staged attestation, and recovery rules. The
runner is optional and requires no cvc5 installation; tests use bounded fake
processes. It is not a mathematical verifier or a secure sandbox.
