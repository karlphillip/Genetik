# Research execution governance

Solver and scientific work can use `scripts/research_execution.py`; see the
mandatory `.agents/skills/genetik-research-execution/SKILL.md` before any run.
This optional repository tool uses standard-library Python and is outside
Genetik's installed library. It has no cvc5 or adapter dependency; solver
identity is supplied by the frozen protocol. Tests use fake executables, not a
real solver. The runner recognizes the documented SMT-style process responses,
so an executable with another output protocol needs an independently reviewed
runner extension or a separate output verifier.

## Freeze

Create a new protocol JSON with immutable source, tool, input, and protocol
identities before search. Required sections are:

- `schema: 1`, unique `protocol_id`, explicit `scope`, and falsifiable
  `hypothesis`;
- complete supported `grammar` and a nonempty `acceptance_criteria` list;
- `controls`, each with a unique ID and separate training/assessment SHA256;
- `frozen_artifacts`, with paths and SHA256s for source protocol/datasets;
- `queries`, an exact query ID/path/SHA256 allowlist with per-query use counts;
- `source_commit`, recording the exact Genetik source SHA used to prepare the
  query adapter and independent evaluator;
- `subprocess_policy: "forbidden"`; solvers that fork children are rejected;
- `solver`, with executable path, version, SHA256, fixed arguments, and SHA256s
  of every material runtime library;
- `program_budget`, with a stable study ID, canonical lifetime-ledger path,
  historical invocation count and original cap/overrun, cumulative prospective
  CPU/wall limits, per-query ceilings, memory, query, and output caps;
- `limits`: maximum invocation count, query wall/CPU, aggregate wall/CPU,
  address space, query bytes, and combined output bytes.
- optional `execution_group`: a canonical persistent sub-budget shared across
  immutable protocol versions. A staged group freezes ordered stage IDs,
  expected solver-process statuses, retry eligibility, one syntax-retry ceiling,
  and a total invocation cap. Each query declares its stage and attempt number.

The freeze command validates hashes and writes a new directory exclusively.
It also initializes (once) the canonical program-wide budget and hash-chained
event ledger under `~/.local/state/genetik/research-program-ledgers/<program_id>`.
Every protocol for that program uses the same lifetime ledger; a different
directory for the same ID is rejected. Historical CPU and total wall remain
unknown when the prior record did not measure them; they are not treated as
zero. Prospective CPU/wall charges are separately enforced. A new protocol ID
or evidence directory does not reset the program budget. The optional
`GENETIK_RESEARCH_LEDGER_ROOT` override is for isolated tests; research runs
use the stable default root.

Execution groups support bounded continuations across write-once protocol
versions. Their hash-chained ledger is stored under the canonical program
ledger and initialized once with an immutable identity and ceiling. Program,
group, and protocol reservations are locked and charged together. Staged groups
reject out-of-order queries and unexpected solver statuses. A syntax retry is
admitted only after an invalid parser diagnostic, for the pending stage, and
when the new frozen query identifies the failed run and changes its query
bytes. The group enforces its retry allowance across protocol versions. A
failed retry, non-parser invalid result, unexpected SAT/UNSAT, timeout, or
resource failure halts the group. Stage progress and retry use are replayed from
the event chain; inconsistent state fails closed.

For groups with `attestation_required`, a successful solver response leaves the
stage pending. `attest --protocol-dir ...` atomically consumes one checker
attempt before launching the stage's hash-pinned executable against the exact
retained solver stdout/stderr and frozen run metadata. The harness, rather than
the caller, derives pass/fail from the checker exit code and writes the result
artifact and canonical receipt exclusively before appending an attestation
event. A failed/timed-out/resource-limited checker is terminal; a harness crash
after reservation leaves the stage blocked with the one-shot reservation
consumed. Checker limits are a separate frozen engineering budget, not deducted
from the solver lifetime budget. The harness enforces per-process address
space, sampled aggregate process-group RSS and CPU, wall time, combined
stdout/stderr bytes, and kills remaining descendants after leader exit. Checker
source, decoder modules, bridge, datasets, and executable artifacts must be
hash-pinned; execution uses a private working directory and a minimal
environment. Checker artifacts are frozen protocol artifacts and their hashes
are fixed in each staged policy.

## Run and outcomes

`run` reads query bytes once, checks their size and the frozen source/tool/data
hashes, takes the lifetime, optional group, and protocol locks in stable path
order, reserves the applicable slots, persists them, and only then launches the
fixed solver command. Reservations remain pending until completion is durable.
A crash between reservations is terminal and fail-closed. The call folder
contains the exact query, command and solver identity, bounded stdout/stderr,
status, exit code, CPU and wall measurements, and a sampled resident-memory
high-water lower bound when measurable. Sampling can miss brief peaks; it is
not a precise process peak. Address-space enforcement is separate. Any pending
call or mismatch between the budget state and event chain stops later calls.

The runner records process outcome and recognized solver outcome separately.
It distinguishes solver-reported `unknown`, solver-reported timeout, harness
wall timeout, external termination, malformed solver input/response, and
resource exhaustion; raw classification and harness limit overrides remain in
the receipt. `successful` or `unsatisfiable` describes only a solver process
response. A mathematical verifier must independently parse/reconstruct the
returned model and assess it using Genetik semantics. Mathematical invalidity
is a verifier result, never inferred from a generic process exit code. A
timeout, signal, or cap is incomplete; it is neither UNSAT nor proof that no
solution exists. A process that starts but emits an error still consumes its
reserved query. A query rejected before reservation consumes no slot.

The gate enforces child address-space and CPU limits, a monotonic wall timeout,
process-group termination, and a combined output limit. A child process that
leaves descendants is an invalid solver execution and the remaining process
group is killed. Per-process CPU is
sampled from Linux `/proc` and backed by `RLIMIT_CPU`; sampled resident memory
is read from `/proc/<pid>/status`. These measurements describe the direct solver
process and are unavailable on unsupported hosts; the adapter must fail closed
when required containment cannot be established. The harness is not a hostile
same-user sandbox and cannot detect direct solver processes launched outside
the approved repository workflow.

## Offline qualification

Run:

```sh
python3 -m unittest discover -s tests -p 'test_research_execution.py' -v
```

The deterministic tests use fake executables and cover missing/tampered
protocols, zero and exhausted budget, cross-protocol and concurrent lifetime
admission, unique artifacts, alternate-ledger rejection, hash mismatch, all
solver status classes, the retained cvc5 timeout diagnostic, bounded output,
timeout, staged checker attestation, and unresolved reservations. They require
no cvc5 installation and make no claim about solver or adapter semantics.
Solver version must be established from a retained identity record or package
metadata without executing the solver outside this gate.

Each execution specifies the frozen query ID as well as its path:

```sh
python3 scripts/research_execution.py run \
  --protocol-dir path/to/frozen-evidence-id \
  --query-id exact-frozen-query-id \
  --query path/to/frozen-query.smt2
```

Queries not in the protocol allowlist, changed bytes, and exhausted per-query
use counts are refused before process launch.
