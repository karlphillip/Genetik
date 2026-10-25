# Discrete iteration D2 — selected local evidence

See [API/contracts](../DISCRETE_ITERATION.md) and the complete
[qualification/results](../DISCRETE_D2_QUALIFICATION.md).

`protocol.json`, `data.json` and `source.json` were committed before any of the
ten sequential discovery runs. They identify the executable built from
`0c55ba603fc949b2ee8e10a9e1ebad6c2b4733fc`; no campaign configuration was tuned
or retried. `RUNS.csv` and `analysis.json` summarize every seed, including two
initialization discoveries. Per-run files retain the selected model before
assessment, independent references, complete metrics and native checkpoint files
compressed without modifying their bytes. Decompress into a fresh run directory
before using `--resume`; no scoring, RNG or state change is needed to load them.
Later test-only and CTest-timeout changes do not change these experimental results.

`gates.json` retains the original driver, including the ASan timeout. Its source
field identifies launch, not a blanket binary revision; read
`qualification-scope.json` and the report for the Release full-versus-targeted
mapping. `gates-continuation.json` records the repaired affected sequence.
The unchanged ASan binary's successful measured completion, original timeout,
progress checkpoints and subsequent CTest result are retained separately. Only
this new complete fixture's ASan timeout changed; no assertion was removed.

Manifest JSON is normalized for readability; corresponding `*.original.gz` files
retain original CTest bytes. The portable manifest has one justified exclusion:
the existing Jetson-specific captured `genetik_evaluation_reference`. Local
recorded suites include it. CI's complete TSan manifest is distinct from the
four affected selections actually run locally three times, plus the complete D2
contract once. `tsan-repetitions.json` records actual executions by name.

`helpers/` retains exact scratch qualification/audit sources as inert text. The
original campaign executor also performed the protocol-freezing local commit;
it is a historical record, not a recommended automatic rerun. Use the documented
application commands to reproduce in new output directories. The production
interpreter is not its own numerical oracle: `independent-references.csv` uses
Python integer arithmetic/factorial and the C++ fixtures use closed forms.

Legacy candidate payloads were produced by the actual D1/current headers and
compare byte-for-byte. The corresponding baseline evolution replay is retained
in logs. The candidate codec appends explicit operator IDs; native checkpoint
format1 is unchanged. Complete-run compatibility additionally fixes the problem,
grammar, scope/iteration policy, budget and qualified execution settings.

`SHA256SUMS` indexes selected files relative to this directory, excluding itself.
Verify with `sha256sum -c SHA256SUMS`. Original builds, uncompressed logs and
consumer fixtures remain under ignored `out/` locations. Final commit identity is
recorded in an ignored completion record to avoid a self-referential manifest.
Preservation covers 127 frozen files, six earlier research/D1 packets, fifteen
archived published files, main, tags and the starting D1 branch. No remote writes,
D3 prime campaign, release work or default grammar changes are included.
