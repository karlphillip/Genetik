# Implementation contract-test execution note

Two local unit-binary executions occurred before the coordinator's instruction
that selector outcome tests must wait for the frozen protocol and end-to-end
admission preflight. These were implementation contract tests only; they were
not any of the 38 frozen campaign executions and were not used to change the
protocol or make an advancement decision.

## Commands and observed results

1. `g++ -std=c++20 -O0 -Igenetik research/finite_domain_identifiability/tests/selector_contract.cpp -o /tmp/fdi-selector-test && /tmp/fdi-selector-test`
   Output: `selector contract passed`; exit status 0.
2. The test source was adjusted to compute Euclidean modulo for negative inputs, then:
   `g++ -std=c++20 -O0 -Igenetik research/finite_domain_identifiability/tests/selector_contract.cpp -o /tmp/fdi-selector-test && /tmp/fdi-selector-test`
   Output: `selector contract passed`; exit status 0.

The tool invocation did not retain per-command timestamps, CPU time, wall time,
or RSS. The surviving binary's filesystem timestamp is 2026-11-12
00:01:19.695661776 UTC. Its current SHA256 is
`10d490c64a782ecc550b57203c4b65a12cf0a1c0f7de1921ccda4423f21b6683`.
The current test-source SHA256 is
`022465afa358c1600e05b9db593dd6919b9e2ff735d6c3ad8426d30a27df93f3`.

The tests exercised finite ambiguity, policy partition/transcript behavior,
an outside-catalogue observation, and explicit candidate-row resource
exhaustion. They used no held-out assessment labels and did not pass a target
reference ID to the selector. The modular test supplied an oracle answer to
exercise one selector transition; this is unit-test data, not campaign evidence.
No frozen run-matrix execution has been launched.

## Pre-freeze resource wording amendment

The first protocol draft described a 1 MiB “logical retained storage” limit.
Before any frozen campaign execution, implementation review established that
the prototype's counter conservatively accumulates logical allocation charges
across candidate/specification checks, including temporary validator results.
The frozen protocol was therefore corrected to call this a 1 MiB cumulative
logical-storage charge, explicitly separate from the enforced 512 MiB process
address-space limit and sampled RSS. This is a stricter accounting policy, not
a relabeling of measured peak memory. The initial wording and its replacement
are preserved here. The amendment was made before the synthetic preflight and
before any campaign run.
