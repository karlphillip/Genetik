# Finite-domain identifiability study: admission blocker

## Decision

The 38-run scientific matrix was **not executed**. The finite-domain
identifiability study is unqualified and inconclusive; do not interpret the
implementation tests or synthetic preflight outputs as scientific results.
No optional production integration candidate is justified by this evidence.

## Frozen study identity

- Research branch: `research/discrete-finite-identifiability-2026-11`
- Source commit: `9231a62384ed045cf44eacf5bf48d4c717a9f506`
- Study protocol: `docs/FINITE_DOMAIN_IDENTIFIABILITY_PROTOCOL.md`
- Machine protocol SHA256: `4a0dc33245faed53935d9b9aa76ed92a838960096be3dc32f45a801e1addc636`
- Protocol binding SHA256: `5b663d1998ef641a67fd216c56a4849d3c7f89b65cbe5bb72313f331c9df7f1f`
- Frozen input manifest SHA256: `b3d637f39390c807e4c58db605b80514b5e1dfb5b268a2d2c48644a698f1296f`
- Latest synthetic preflight receipt SHA256:
  `6d85cee6426bde045ffcd3dd88addb6493cbb6eac74fd77ea66d21f3f26fb540`

The frozen scientific criteria and run matrix remain unchanged. Historical
preflight attempts and the two pre-freeze selector unit executions remain
separate evidence. The unit executions have unknown CPU, wall-time, and RSS;
their outputs were not used to tune or score the study.

## Admission and cumulative-budget findings

The current research execution skill requires a frozen protocol and a
successful end-to-end synthetic preflight before scientific execution. Four
successful synthetic preflights (`protocol-preflight-v2` through `v5`) ran all
four pipeline stages, consuming **16 stage invocations**. An earlier `v1`
attempt is incomplete and has no receipt or stage evidence; its invocation
count is unknown and is not included in that lower bound. Preserve every
version and receipt unchanged.

For the v5 declared cap of 45 top-level invocations, finishing the matrix and
its final verifier, scorer, and aggregator requires 41 more invocations (38
scientific runs plus 3 final stages). The minimum complete total would thus be
57, already 12 above the declared cap, before counting the incomplete v1
attempt or unit-test executions. No remaining invocation allowance is
available under the frozen protocol. The v5 CPU and wall ceilings cannot be
treated as lifetime budgets independent of these already completed stages.

The four successful preflight receipts record these wall times:

| Version | Runner | Verifier | Scorer | Aggregator | Total recorded wall |
|---|---:|---:|---:|---:|---:|
| v2 | 0.2715 s | 0.2668 s | 0.2694 s | 0.2674 s | 1.0751 s |
| v3 | 0.2775 s | 0.2740 s | 0.2709 s | 0.2654 s | 1.0879 s |
| v4 | 0.2731 s | 0.3067 s | 0.2717 s | 0.2668 s | 1.1184 s |
| v5 | 0.2741 s | 0.2682 s | 0.2735 s | 0.2681 s | 1.0840 s |

CPU and RSS were not recorded by the preflight tool. Its per-stage 10-second
CPU, 15-second wall, 512 MiB address-space, and 1 MiB output limits were
enforced; they do not substitute for missing measurements. The measured sum
of four successful preflights is 4.3652 seconds of wall time. No extrapolated
CPU or memory total is claimed.

## Blocking defects

1. **Budget exhaustion:** the completed preflight stages plus remaining
   required work exceed the frozen invocation ceiling. Do not reset the
   reservation ledger or create a continuation protocol to recover the
   consumed budget.
2. **Preflight scope mismatch:** v2–v5 run the synthetic-only
   `preflight_adapter.py` for all four stages. They verify synthetic artifact,
   receipt, scoring, and aggregation contracts, but do not exercise the actual
   C++ selector runner, `verify_matrix.py`, or `score_transcripts.py` as the
   end-to-end stage chain. Pinning those sources as separate artifacts does
   not establish that the actual campaign path is compatible.
3. **Execution-path mismatch:** the custom matrix wrapper is not admitted via
   the repository's `research_execution.py` freeze/run protocol. Static review
   also found that `proc.stdout.read(65536)` may wait for a full buffered read
   while the interactive selector is waiting for an oracle response. This was
   not executed as a failure; the custom path is not qualified.

The custom wrapper's dry admission check passed for 38 allowlisted runs. That
check did not launch selector processes, consume the one-shot scientific
reservation, or produce scientific output. A first dry check with an output
path different from the frozen evidence directory was rejected; a second dry
check using the exact declared path passed. Neither used `--execute`.

## Other qualification performed

- Architect reviewed the frozen study design and identified the budget and
  execution-path blockers above.
- The synthetic preflight passed with 18 controls and the complete 38-run
  matrix bound into the receipt.
- The selector contract binary passed after protocol freeze; five independent
  scorer contract tests passed.
- The C++ runner and selector test compiled as C++20; research Python tools
  passed syntax checks.
- No selector run matrix, assessment scoring, hosted CI, or production API
  qualification was performed.

## Disposition and next boundary

Preserve this research branch, all preflight versions, training/assessment
artifacts, receipts, implementation-test incident note, and the immutable
evidence manifests. The candidate query-selection idea remains untested at the
frozen study level. A future study requires a separately authorized budget and
a qualified admission/execution chain that exercises the actual runner and
scorer; it cannot continue this exhausted protocol. This result does not show
that disagreement queries help or fail to help, and it does not establish
general expression-discovery value.

The next priority is engineering-only qualification of a reusable generic
non-solver execution/admission path, not a different mathematical experiment.
It should exercise interactive request/response handling, the actual
runner→verifier→scorer→aggregator chain on unrelated synthetic fixtures,
persistent budgets for all attempts, immutable identities, and fail-closed
timeout/resource/interruption outcomes. Acceptance requires deterministic
synthetic tests for each declared failure mode and independent reconciliation
of resource charges, while preserving the existing solver harness and
historical ledgers.
