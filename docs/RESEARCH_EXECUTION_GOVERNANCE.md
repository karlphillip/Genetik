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
status, exit code, CPU and wall measurements, and sampled process-group RSS/CPU
when measurable. `cpu_seconds` and `waited_child_cpu_seconds` come from Linux
`wait4` for the launched leader and any descendants it reaped; they do not
represent arbitrary descendants that escaped or were reparented. The separate
`process_group_cpu_seconds` is a sample of visible group members. On Linux,
`waited_child_peak_rss_bytes` converts the individual waited child's
`ru_maxrss` to bytes; it is null on other platforms because `ru_maxrss` units
vary. This is never an aggregate group peak.
`process_group_rss_bytes` is the sampled sum of resident memory for visible
group members and is a lower bound because sampling can miss brief peaks and
short-lived descendants. When the process-group RSS sample is unavailable, the
result records `resource_accounting_terminal_status=resource_accounting_unavailable`,
charges the full per-query allowance, and terminally blocks the protocol.
Otherwise-successful results also use `resource_accounting_unavailable` as
their primary status. Existing timeout, malformed, unknown, and termination
statuses are preserved; the separate terminal field records the accounting
stop reason, while a timeout remains terminal under its primary timeout
status.
Address-space enforcement is separate. Any pending call or
mismatch between the budget state and event chain stops later calls.

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
group is killed. Linux `wait4` supplies the waited child's CPU and individual
peak RSS; visible process-group RSS and CPU are sampled from `/proc`. Group
samples are lower bounds and do not account for descendants that have already
escaped the group. If the group RSS cannot be sampled, the harness sets the
separate terminal accounting status to `resource_accounting_unavailable`. The
harness is not a hostile same-user sandbox and cannot detect direct solver
processes launched outside the approved repository workflow.

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

## Pinned bounded research programs

Non-solver C++ or Python stages use the separate `bounded_program` schema and
process ledger. Follow the [`genetik-research-execution` skill](../.agents/skills/genetik-research-execution/SKILL.md)
for the `freeze-program`, `run-program`, and `verify-program` commands and
their phase-plan requirements. A frozen pipeline descriptor binds stage argv
and entrypoints to pinned code artifacts; preflight may not substitute an
adapter command. The process supervisor uses bounded nonblocking input and
output, process-group cleanup, resource caps, and immutable receipts. These
facilities do not grant scientific-run approval and do not alter the solver
protocol or ledger. Assessment-role data is identity-bound during freeze and
preflight, but opened only by the separately admitted scoring phase. Runner
and verification stages must not receive it.

A bounded-program protocol is write-once. A reviewed software-only correction
to an unused study must use `successor-program`, which appends one hash-chained
activation record to the existing lifetime ledger and preserves its counters.
The operation rejects any changed controls, datasets, run or phase plan,
acceptance criteria, authorization, or resource ceilings. It also refuses a
ledger with reserved work, charges, a started deadline, active phase,
qualification, or terminal state. The old protocol becomes inactive; a partial
activation is either completed from the immutable record or refused. Never
re-freeze a new protocol against the same ID to bypass this transition.

Each external process runs under a dedicated Linux supervisor worker. This
isolates descendant adoption from unrelated caller children. Target and
supervisor CPU/wall/RSS measurements are recorded separately. Cumulative
charges reserve the full supervisor CPU/wall envelope for each invocation;
actual measurements remain separately available. A combined RSS figure is a
conservative sum of the observed peaks, not a claim that both peaks occurred
simultaneously. The worker writes its result atomically, so a partial result
transfer cannot block past the outer deadline. Protocol admission reserves
three additional CPU-seconds and three wall-seconds per invocation for the
isolated worker's hard-fallback envelope, and includes this in complete and
lifetime budget checks.

### Adaptive request/response programs

A bounded-program stage may use `scripts/bounded_interactive_session.py` when
the stage itself is the supervised process-group leader. Pin the request
producer, response peer, executable hashes, imported runtime files, explicit
environment and helper in the frozen stage dependencies. The helper uses
bounded JSON Lines; requests carry a unique typed `id`, and the response must
echo it. Only one request is outstanding. Frame size, request count, aggregate
protocol plus stdout/stderr bytes, and wall time are bounded. Each exchange's
request and response hashes are recorded in a deterministic hash chain, with
capped lossless per-stream bytes retained in the receipt. Malformed schemas,
correlation-mismatched responses, extra output after the terminal frame, EOF, timeout, and resource
exhaustion are incomplete outcomes. The outer `run-program` process-group
supervisor accounts and cleans both children.

For append-only finite specifications, use
`scripts/specification_revision.py` to recompute each identity from ordered
observations and authenticate each predecessor/counterexample link. A finite
validation result only concerns the exact finite observation set; it is not a
universal equivalence certificate. Unresolved candidate checks cannot count
as successful identification.

Run these offline engineering contracts with:

```sh
python3 tests/test_bounded_interactive_session.py
python3 tests/test_specification_revision.py
```
