# End-to-end research pipeline preflight

Scientific runs require a successful synthetic end-to-end preflight before a
protocol can be frozen or executed. This is offline admission tooling; it does
not modify Genetik's library or run a scientific campaign.

The preflight exercises the same declared runner, receipt verifier, scorer,
and report aggregator commands that a campaign will use. Its versioned JSON
specification includes at least two controls, each with distinct synthetic
training and assessment artifacts, plus a complete run matrix keyed by run,
control, method, seed, and the control's artifact IDs. Production artifact IDs
and SHA256 values are bound by identity only: the preflight never opens their
files. The child runner receives synthetic training paths only; synthetic
assessment paths are made available to the scorer stage.

Each stage's explicit version, executable, adapter/dependency files, and runtime
identity must be pinned. The preflight validates candidate-output and receipt associations,
assessment-artifact selection, exact synthetic assessment predictions, and
per-control aggregate counts in the final report. A zero exit status or generic `passed` marker is not
enough. On success it writes a write-once receipt and preserves generated
synthetic artifacts, stage outputs, receipts, and report under a unique evidence
directory. Failed attempts leave partial evidence for diagnosis.

Stage processes receive a fixed minimal environment; caller environment
variables are not inherited. They run in a fresh process group. Per-stage
wall-time, CPU-time, address-space, and combined output caps are enforced, and
the group is terminated when a stage ends. CPU and address-space limits are
per process (inherited by children), not an aggregate process-group CPU or RSS
budget. `RLIMIT_CPU` has one-second granularity on the supported Linux runner,
so `cpu_seconds` must be an integer of at least one second and is enforced at
that exact integer limit; wall-time may be fractional. This preflight qualifies pipeline compatibility on small synthetic
fixtures; actual scientific run resources remain governed by the frozen
research-execution limits and receipt accounting.

The protocol must pin both the preflight specification and receipt in
`frozen_artifacts`, and include an `admission_preflight` reference. The receipt
binds a canonical hash of the entire protocol excluding only the receipt
reference and the two preflight artifact records, which avoids circular
hashing. `research_execution.py freeze` and every `run` revalidate the binding,
artifact mappings, report/status hashes, and pinned command/dependency
identities. Also include the actual gate source files as `admission-tool` and
`execution-gate` artifacts so the qualification procedure itself is pinned. A
changed run matrix, control association, tool, or protocol needs
a new synthetic preflight. Missing or stale evidence is a refusal.

Run the gate before protocol freeze:

```sh
python3 scripts/scientific_pipeline_preflight.py \
  --protocol path/to/protocol.json \
  --spec path/to/synthetic-pipeline-preflight.json \
  --receipt path/to/new-evidence/preflight-receipt.json
```

Then add the returned spec and receipt paths, hashes, preflight ID, and binding
hash to the protocol. Add both files to `frozen_artifacts` with roles
`admission_preflight_spec` and `admission_preflight_receipt`. See
`.agents/skills/genetik-research-execution/SKILL.md` for the full admission
workflow.

This is a reproducibility and contract gate, not a secure sandbox against the
same account owner. The synthetic examples qualify pipeline wiring; they do
not establish the scientific validity of real outcomes or replace independent
review of a frozen protocol.
