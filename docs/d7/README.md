# Genetik 0.5.0 D7 evidence

This directory contains the frozen competitor protocol, primary benchmark data,
repeatable adapters, shared independent assessment program, protocol preflight
record, historical D5/D6 discovery diagnostics and release evidence.

- [Competitor protocol](BENCHMARK_PROTOCOL.md): versions, environment, frozen
  tasks, grammar, resource budget and scoring rules.
- [Preflight results](preflight_results.json): accepted neutral runs and
  preserved setup failures. No target benchmark runs until the protocol is
  committed.
- [Primary data](data/manifest.json): exact SHA256 values for the generated
  training/noisy/held-out CSVs.
- [Prior discovery diagnostics](PRIOR_DISCOVERY_DIAGNOSTICS.md): D5/D6
  negative and positive scientific findings, without causal overstatement.
- [Integrated D5/D6 numerical qualification](../D7_NUMERICAL_QUALIFICATION.md).
- [Competitor benchmark report](BENCHMARK_REPORT.md), when the frozen matrix
  and independent assessment complete.

The setup output, compiler logs and per-run benchmark logs are retained under
`out/logs/d7/`; generated environments, source checkouts and binaries are under
`out/tmp/d7/` and `out/build/d7-benchmark/`. They are local qualification
artifacts and do not touch the published 0.4.0 tag/assets.
