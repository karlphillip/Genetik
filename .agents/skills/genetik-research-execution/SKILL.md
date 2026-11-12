---
name: genetik-research-execution
description: Freeze, budget, execute, and preserve Genetik solver and scientific research runs.
---

# Genetik research execution

This skill is mandatory for scientific campaigns, solver calls, solver probes,
debugging, retries, and feasibility checks. It complements the
`genetik-development` and `genetik-branch-lifecycle` skills; it does not grant
permission to change production code, merge research, or relax an established
scientific acceptance criterion.

The runner is optional repository research tooling, outside the installed
Genetik library. It uses only the Python standard library and does not install,
invoke, or require cvc5. Its protocol can pin another compatible executable;
the recorded solver-status parser accepts the documented SMT-style responses.
The experimental scoped-reduction adapter is a separate research artifact and
is not required by this workflow.

When the configured multi-agent workflow is used, the coordinator owns the
cumulative budget, protocol freeze, execution admission, and final evidence
review. The architect independently reviews mathematical semantics and the
frozen protocol; the implementer may prepare code, tests, or offline checks but
must not execute experiments outside the governed runner. Verify that each
delegated review actually completed before treating it as a gate result.

## Before execution

1. Work in a task-owned isolated worktree and preserve all historical research
   branches, protocols, failed runs, and user changes.
2. Read the relevant decision documents and retained evidence. Establish one
   falsifiable hypothesis, supported grammar, independent controls, assessment
   separation, exact acceptance criteria, and cumulative computational and
   memory budgets. Do not create new thresholds during the experiment.
3. Pin source SHA, solver version and executable SHA256, every runtime library
   SHA256, control/training/assessment data identities, grammar, limits, and
   output rules in the protocol. Do not invoke the solver to discover its
   version; use a previously identified binary or record identity out of band.
4. Run the harness unit tests with fake executables and the mandatory
   end-to-end synthetic admission preflight described below. Freeze the
   protocol into a new, unique, write-once directory using:

   ```sh
   python3 scripts/research_execution.py freeze \
     --protocol path/to/protocol.json \
     --destination path/to/new-evidence-id
   ```

   The `program_budget` must use the canonical lifetime ledger for its stable
   `program_id`. It imports exact historical counts and immutable evidence;
   new protocol IDs and evidence directories do not reset it. Do not create a
   new ID for a continuation of the same research program. Historical costs
   that were not measured remain `unknown`; only prospective budgets can be
   charged precisely.

5. **Mandatory end-to-end synthetic admission gate.** Before freezing a
   protocol or launching any scientific/solver execution, run
   `scripts/scientific_pipeline_preflight.py` against a synthetic pipeline
   specification. Bind the exact protocol control IDs, training/assessment
   artifact IDs and hashes, and complete run/control/method/seed matrix. Pin
   runner, receipt verifier, scorer, aggregator, runtime identities, and
   adapter/dependency file hashes. The preflight materializes distinct
   synthetic training and assessment files for at least two controls, then
   executes runner output generation, receipt verification, independent
   synthetic scoring, and final aggregation. It validates each stage's
   outputs and identities, not only exit status. Include the passing receipt
   and specification in `frozen_artifacts` and `admission_preflight`, and pin
   the preflight and execution-gate source files with roles `admission-tool`
   and `execution-gate`; protocol freeze and each run recheck their hashes and
   canonical protocol binding.
   Missing, stale, mutated, or mismatched admission evidence is a hard
   refusal. Never open or pass real/sealed assessment artifacts to preflight
   commands.

   ```sh
   python3 scripts/scientific_pipeline_preflight.py \
     --protocol path/to/protocol.json \
     --spec path/to/synthetic-pipeline-preflight.json \
     --receipt path/to/new-evidence/preflight-receipt.json
   ```

   Preserve the unique synthetic inputs, pipeline outputs, receipts, and
   report directory recorded in the receipt. Do not reuse the receipt after
   changing the protocol, mappings, run matrix, or pinned command chain.
   Stage commands use a fixed minimal environment that excludes inherited
   assessment paths and secrets. They have per-stage wall, CPU, address-space,
   and output caps, with process-group cleanup. These synthetic preflight caps
   are per process, not aggregate process-group CPU or RSS limits; actual
   campaign resources remain governed by the frozen execution harness.

6. For retries or ordered syntax/semantic/replay phases spanning immutable
   protocol versions, define one persistent `execution_group` beneath the
   program ledger. Freeze its ordered stages, expected process status, retry
   eligibility, shared invocation ceiling, and `max_syntax_retries: 1` in every
   version. Each query declares `stage_id` and `retry_attempt`; a retry also
   declares `retry_of_run_id`. Only an `invalid` result with a parser diagnostic
   can open the single retry. The retry must identify that failed run and use
   different query bytes. Expected stage outcomes are `successful` or
   `unsatisfiable`; `unknown`, non-parser invalidity, timeout, and resource
   exhaustion halt the group. Replay stages use `replay-` IDs and are
   nonretryable. Never alter group identity or stage order between versions.

   When a staged protocol requires checker attestation, freeze each successful
   stage's checker SHA256 as an executable artifact and freeze a separate
   checker resource budget. A successful solver process leaves that stage
   pending. Use `attest --protocol-dir ...` without a receipt argument: the
   harness reserves its one attempt before launch, derives pass/fail from actual
   exit status, and writes immutable result/receipt artifacts. Never accept
   caller-supplied receipt fields as proof. Checker failure, timeout, or
   resource exhaustion terminates the stage; harness crash leaves its one-shot
   reservation consumed and the group fail-closed. The harness uses a private
   working directory and minimal environment, caps combined output, samples
   process-group CPU/RSS, and kills remaining descendants. Pin the checker and
   every imported module, data and bridge artifact in the protocol.

7. Do not issue any solver execution before the architect's semantic and
   protocol review and successful offline qualification. The scientific
   protocol must be frozen before implementation tuning or solver execution.

## Gated execution

All solver processes, including parser/feasibility probes and retries, must be
launched only through:

```sh
python3 scripts/research_execution.py run \
  --protocol-dir path/to/frozen-evidence-id \
  --query-id frozen-query-id \
  --query path/to/exact-query
```

The harness verifies the frozen protocol and all pinned file identities,
checks query size and program-wide plus protocol budgets under interprocess
locks, reserves the lifetime, optional group, and protocol slots together,
and keeps them pending until completion,
then saves each query and result in a unique
immutable call directory. It bounds child address space, CPU, wall time and
combined output; records status, command, solver identity, wall/CPU time and a
sampled resident-memory high-water lower bound; and maintains a hash-chained
event ledger. Group stage order, retry lineage, changed-query requirement, and
terminal status are replay-verified from the group event chain. Sampling can
miss brief peaks. Fake-executable
tests must exercise no-protocol refusal, zero/exhausted budget, concurrent
admission, tampering, unique evidence, invalid/unsat/unknown, timeout, resource
exhaustion, and interrupted reservations.

Every launched or reserved process consumes its query slot. A protocol,
ledger, solver/runtime hash mismatch, output overflow, timeout, unresolved
reservation, or resource limit stops execution. Do not edit the ledger, retry
under the same protocol, or call the solver outside the harness. Preserve the
incomplete evidence and request a separately reviewed continuation protocol.

Reported statuses are distinct: successful, unsatisfiable, unknown,
timed-out, invalid, and resource-exhausted. A timeout or cap is incomplete,
never UNSAT. Keep solver work separate from Genetik evaluation work. Keep
training, counterexamples, and final assessment roles explicit; assessment
observations never guide construction, selection, or refinement.
`successful` records only a successful solver-process response. It is not
mathematical synthesis success until the adapter parses/reconstructs the model
and independently evaluates it through Genetik.
Solver subprocess creation is forbidden. A solver that leaves a descendant
process is invalid; the harness kills the remaining process group.

The harness is a local execution gate, not a secure sandbox against a hostile
owner of the same machine account. Do not claim it can detect unrecorded direct
process execution. Repository practice makes direct solver execution
prohibited and tests the approved gate; preserve shell history and process
evidence where the protocol requires it.

## Pinned bounded C++/Python research programs

Non-solver research executables and multi-stage scientific pipelines use the
separate `bounded_program` protocol path. This path has its own persistent
ledger and must not alter or reuse the solver ledger. Freeze the exact source
commit, executable and runtime hashes, explicit environment, immutable input
IDs/hashes, ordered run allowlist, complete phase plan, and per-invocation and
cumulative CPU/wall/output/memory limits before execution:

```sh
python3 scripts/research_execution.py freeze-program \
  --protocol bounded-program-protocol.json \
  --destination out/research/frozen-program
python3 scripts/research_execution.py run-program \
  --protocol-dir out/research/frozen-program --run-id frozen-run-id
python3 scripts/research_execution.py verify-program \
  --protocol-dir out/research/frozen-program
```

The run ID must be allowlisted and follow the frozen phase order. Every
reservation consumes lifetime budget, including an interrupted invocation.
Receipts are immutable and distinguish completion, timeout, resource
exhaustion, and harness failure; none implies scientific recovery. Preflight,
campaign, verification, scoring, aggregation, and recovery must fit the full
declared phase plan before the first invocation. A pipeline descriptor binds
the complete stage argv and entrypoint to pinned code artifacts; preflight and
production use that same descriptor and shared process supervisor. Assessment
artifacts are identity-bound by ID and SHA256 during freeze/preflight and are
not opened by runner or verifier stages. Their content is rehashed only when
the separately admitted scoring stage consumes them. Never place assessment
paths or labels in runner-visible manifests or environments.

The Linux supervisor runs each invocation in a dedicated worker so orphaned
descendants can be adopted and cleaned without treating unrelated caller
children as invocation-owned. Result transfer is atomic and the outer worker
deadline is enforced independently of the target deadline. Admission reserves
an additional three CPU-seconds and three wall-seconds per invocation for the
worker's hard-fallback envelope; include this allowance in both protocol and
persistent lifetime totals. Receipts report target and supervisor CPU/wall/RSS
separately. Cumulative charges reserve the full supervisor CPU/wall envelope
on every invocation, while measured CPU/wall remain separately available; the
combined RSS estimate is a conservative sum of
separately observed peaks, not a simultaneous-RSS measurement. A missing
measurement is explicit and cannot become a successful completion.

Engineering-only tests may use synthetic programs and data. They must not
invoke solvers or scientific campaign executables unless the coordinator has
frozen and admitted the exact research protocol.

Adaptive programs may use `scripts/bounded_interactive_session.py` from the
single bounded-program process that owns the interaction. The process must be
the invocation's process-group leader; do not create a nested unaccounted
supervisor. Pin the helper, request producer, response peer, executable, and
all runtime files in the frozen command dependencies. Use JSON Lines with a
unique typed `id` on each request and an identical id on its response; only one
request may be outstanding. Bound frame bytes, combined request/response and
stderr bytes, request count, and wall time. The helper records each request
and response byte hash in an ordered SHA256 chain and retains capped lossless
per-stream byte evidence. Malformed, correlation-mismatched,
incomplete, timed-out, or resource-exhausted sessions are not successful.
Keep the response peer separate from candidate generation and assessment
inputs. The outer `run-program` supervisor owns and cleans the producer/peer
process group and accounts their group CPU/RSS.

Append-only specification chains may be checked offline with
`scripts/specification_revision.py`. Each revision must bind its query and
counterexample to the exact prior identity; the verifier recomputes every
successor identity and rejects unresolved candidate validation. A complete
finite-domain result remains scoped to those observations and must never be
reported as universal equivalence.
