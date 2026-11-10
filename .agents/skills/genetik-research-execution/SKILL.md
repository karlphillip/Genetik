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
4. Run the harness unit tests with fake executables. Freeze the protocol into a
   new, unique, write-once directory using:

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

5. For retries or ordered syntax/semantic/replay phases spanning immutable
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

6. Do not issue any solver execution before the architect's semantic and
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
