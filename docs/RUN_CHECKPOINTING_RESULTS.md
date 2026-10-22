# Run observability/checkpoint qualification

Implementation starts from853e1875a67c19365e10a25daff6aa01ec50c6cc on the local
feature/run-checkpointing branch. Main, releases/tags/assets and accepted campaigns
are preserved. [API and numerical/persistence contracts](RUN_CHECKPOINTING.md).

## Frozen demonstration protocol

Seed42, four workers, unchanged prime population64, grammar/fitter/cadence and exact-
prefix objective. No target expression or prime approximation is injected. Default
single-best pretty display, report0.1s, checkpoint0.15s, retain three generations,
JSONL sink. Initial total allowance1,000,000; after at least three displayed snapshots,
request SIGINT, retain its final checkpoint, destroy the process. Resume the same
checkpoint with total allowance2,000,000; after continued evolution and at least
three snapshots, request SIGTERM. These limits/signals demonstrate observability
and recovery, not a stochastic recovery/performance claim. No tuning or success-
dependent retry is allowed. Selected observations and qualification follow below.

## Deterministic acceptance

The independent small three-observation problem cannot reach an exact fit with the
three-node/depth-two grammar, avoiding an optimum-dependent test gate. A fully
serialized final state compares all committed expressions/coefficient bits/caches,
selection data, archive and best generation, MT19937 state, objective/preparation/
variation counters and improvement history. Generation100 matches a clean split at40,
including separate processes and nonbinding worker1/2/4 runs. Gate-controlled parallel
initial/preparation work forces actual overlap; reports/checkpoints remain coordinator-
only after drain. A deterministic interrupted private fitting generation retains G40,
counts attempt41 and post-failure RNG/work, then resumes by constructing G41.

Envelope/codec tests cover malformed/truncated/corrupt files, bad checksum, old/new
format IDs, wrong runtime/codec/problem/configuration, invalid counters, input limits,
I/O errors, allocation/pre-rename injected failure, retention and rollback rejection.
Codec tests preserve every scalar operator, variable identity, power metadata,
signed zero, subnormal/extreme finite coefficients and valid/invalid cached fitness.
Unicode/LaTeX tests preserve grouping, operand order and executable canonical text;
winning output, normalized presentation endpoints, JSONL, default/top/raw modes and
total-budget resume are tested without stochastic recovery requirements.

An existing-objective boundary is deliberately retained: native binary64 secondary
ratios/sums can make valid loss1001. The new presentation accepts fitness0 there;
the objective is not clipped or otherwise changed. A31-node product fixture tests
this exact behavior. Full loss remains authoritative for all search decisions.

Early retained development diagnostics under out/logs/checkpoint/ include Werror
indentation/aggregate warnings and a test-fixture name lookup error: inherited
limits() was selected instead of the intended ::limits() during test subclass base
construction. The fixture was corrected before sanitizer qualification. An initial
expanded pretty equation omitted grouping around the complete scaled expression;
that was fixed and receives an explicit regression test. No numerical expectation,
instrumentation or existing contract was weakened.
