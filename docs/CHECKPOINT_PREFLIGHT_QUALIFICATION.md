# Operational polish and 0.3.0 local preflight

Start: clean feature/run-checkpointing at 9eb53516627f43df11b22cd7e98618a75d84d307.
Dedicated local branch: qualification/checkpoint-preflight. No remote writes.
Prior [checkpoint qualification](RUN_CHECKPOINTING_RESULTS.md) remains unchanged.

Implementation adds presentation-only significant digits, approximate Unicode/LaTeX
equations, readable metrics and generation ceilings. Canonical expressions, binary64
coefficients and machine-readable records remain full precision. Search objective,
ordering, RNG/grammars and generic OOP behavior are preserved.

Durability review: mkstemp in destination directory, unbuffered checked writes,
file fsync/close, checksum read-back, rename and directory fsync were already present.
Newly created directory entries now sync through each parent; retention deletion is
also directory-synced after replacement. Interrupted pre-rename/allocation and actual
rename-failure tests preserve and reload the previous verified file. Format 1 and
its codec IDs are unchanged. A direct envelope decode now enforces the source-byte
limit including the header. No valid encoded file is reinterpreted.

Package preparation moves authoritative Version.hpp to 0.3.0 and updates version
fixtures. The archive verifier now reads that committed authority (with a historical
literal-CMake fallback); code inspection found its stale CMake-only regular expression.
No old archive/evidence is rewritten. Version/runtime guards intentionally reject
old-version checkpoint files; see [migration](releases/0.3.0/MIGRATION.md).

Executed results, crash-recovery experiment, preservation and exact candidate/archive
identities will be recorded after local qualification. Hosted qualification is pending;
existing hosted claims retain their recorded commit/runtime scope.


Frozen local protocol: unchanged exact-prime seed42, four workers, population64,
default grammar/fitter, total budget1,000,000. Reports and periodic checkpoints every
0.05 seconds, keep3. After at least four flushed snapshots and retained nonzero
checkpoint generations, SIGKILL the exact subprocess PID, wait/reap and preserve
its files. Verify all three numbered files and latest by checksum and full state
restoration without objective work. In a fresh process resume with total2,000,000,
precision4, unchanged execution settings; SIGTERM after at least two more committed
generations. No configuration choice depends on prime-recovery quality. The helper
is scripts/check_checkpoint_crash.py and its deadline is45 seconds per process.
