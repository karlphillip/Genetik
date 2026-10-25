# Migrating to proposed Genetik 0.4.0

Use find_package(Genetik 0.4 CONFIG REQUIRED) and Genetik::Genetik. Existing include
spelling and the source target genetik_lib remain. C++20, minimumCMake3.16,
Threads::Threads and strict floating-point requirements propagate as before.
SameMinorVersion intentionally rejects requests for0.3.x against0.4.0; change the
requested minor after reviewing/rebuilding. An exact0.4.0 request is supported;
newer unavailable0.4 patches and other minor series are rejected.

Rebuild every dependent translation unit consistently. Header-only/source compatibility
is not a binary ABI promise, including earlier polymorphic measurement/preparation
changes and the new discrete search configuration fields. Published scalar/typed/
OOP APIs and defaults remain supported. Unreleased research APIs for NM, objectiveB
and mixed-point mutation are outside0.4; keep their original research branch if needed.

DiscreteExpression is separate from floating-point Expression/TypedExpression.
External inputs are ordered Int64; computed Bool values are typed, not truthiness
of integers. Use explicit if(condition,1,0) for classification. Iterator kinds and
binding identities are distinct from external input indices. Configure expression,
parser, workspace, per-operation/aggregate iteration and objective-work allowances.
Resource exhaustion aborts private preparation; it is not a finite loss penalty.

Generic loadCheckpoint remains exact-version. loadScalarCheckpoint explicitly
supports the published0.3.0 scalar codec on the identical qualified compiler,
standard/runtime libraries, FP/optimization environment and unchanged problem/config.
The scalar prime application's --resume uses that loader. loadDiscreteRunCheckpoint
similarly supports the unchanged0.3 research discrete-evolution/2 profile; current
run data/grammar/limits/task/worker/budget identity must still match. The D3 application
and D2 campaign use this explicit loader. Other versions/codecs reject, and user
codecs require an explicit reviewed CheckpointReadCompatibility policy. Do not
resume untrusted files; checksums detect corruption, not hostile forgery.

CheckpointDocument.producerVersion exposes the original version on import;
runtime is normalized only after all other fingerprint fields match exactly.
Saving creates a0.4 document, without editing old files or changing format1/node
codec IDs. Unsupported legacy files should remain with their exact original build.
No automatic conversion of experimental optimizer/mutation journal profiles exists.

Resume restores population, fitness/archive, RNG, generation/work counters and
configuration; it does not recompute stored fitness or supply missing observations.
Changed datasets/tasks cannot reuse a checkpoint. CPU workers/settings remain part
of the supported reproducibility scope. Crash recovery restarts the last durable
committed state and may repeat later unknown work; it is not an exactly-once journal.

The discrete-prime application has a declared bounded Linux/POSIX research profile,
not a general prime solver. Its SIGINT/SIGTERM flag is observed after bounded active
generation work drains; reports/checkpoints are coordinator-owned. Existing scalar
signal/cancellation semantics remain unchanged. Preserve original research datasets
and assessments; do not interpret training matches as global mathematical identity.
