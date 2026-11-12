# Finite-domain identifiability prototype

This directory contains a research-only audit of finite candidate identifiability. It does not synthesize expressions. The supplied catalogues are fixed by the protocol, and the selector only distinguishes their exact output signatures over the declared finite domain.

## Process separation

`runner.cpp` owns candidate catalogue evaluation and query selection. It receives an opaque run ID, catalogue family, policy, seed metadata, and a content-addressed training-only TSV. It writes a query request and accepts only an exact integer response on stdin. It has no reference-member identifier, target equations, oracle implementation, or assessment path.

`tools/run_matrix.py` is the external oracle/controller. Before execution it requires the frozen machine protocol, matching admission-preflight receipt, exact source/data hashes, an opaque 38-run allowlist, a unique evidence directory, and a stable one-shot reservation. It sends only the requested integer value to the selector; oracle operation counts remain in the outer run receipt. A failure or partial allowlist remains incomplete and consumes the reservation.

`tools/verify_matrix.py` verifies the ordered event hash chain, immutable per-run receipt hashes, protocol transcript bytes, query-wire sequence, work counters, and resource totals. It does not read assessment values. `tools/score_transcripts.py` independently parses and evaluates the ordinary Genetik expressions, recomputes finite signatures and behavior classes, verifies query choices/partitions/eliminations/specification identities, and only then reads the separately hashed assessment TSVs. Assessment agreement is finite evidence only.

`tools/freeze_data.py` generates separate, content-addressed training and assessment artifacts with create-exclusive manifests. Training TSVs contain only `input` and `value`; no operation-cost or reference identity is passed to selection. Assessment paths are never passed to the selector or controller child process.

## Resource-accounting convention

Selector mathematical work uses `DiscreteExpression` validator/evaluator counters plus deterministic candidate, signature, specification, partition, and transcript bookkeeping charges. `logical_storage_bytes` is a conservative cumulative logical-allocation reservation, not process RSS. The selector cap is 99,936 work units and reserves 64 units for independently measured oracle work. The controller records that oracle work separately and enforces the combined 100,000 work-unit per-run ceiling. CPU, wall, child RSS, controller CPU/RSS, and conservative combined peak-RSS estimates are separate receipt fields.

The matrix wrapper has a hard 2-second CPU and 3-second wall limit per run, a 512 MiB address-space limit per child, and a 1 MiB combined stdout/stderr cap. The study total is 38 runs, 76 CPU seconds, 114 wall seconds, and 3.8 million combined work units. Resource exhaustion is never a successful identification.

## Build and review

Compile the runner and contract test without running the frozen study:

```sh
g++ -std=c++20 -O0 -Igenetik -Iresearch/finite_domain_identifiability \
  -c research/finite_domain_identifiability/runner.cpp -o /tmp/fdi-runner.o
g++ -std=c++20 -O0 -Igenetik \
  -c research/finite_domain_identifiability/tests/selector_contract.cpp -o /tmp/fdi-selector-contract.o
```

The Python tools use only the standard library. Machine-protocol freeze and the synthetic end-to-end preflight must pass before any run-matrix execution. A successful finite-catalogue result does not establish universal equivalence or expression discovery capability.
