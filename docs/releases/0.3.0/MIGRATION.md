# Migrating from Genetik 0.2.x to proposed 0.3.0

Rebuild all dependent translation units consistently when updating this header-only
library. Source-level OOP and mathematical APIs remain supported; this is not a
binary-compatibility guarantee. No mathematical grammar, prime objective or default
historical command has been changed by the operational work.

Use `find_package(Genetik 0.3 CONFIG REQUIRED)` with `Genetik::Genetik`.
SameMinorVersion intentionally rejects an explicit 0.2 request against 0.3.0, even
where source APIs remain compatible. Patches within 0.3 preserve documented source
APIs; documented breaking changes before 1.0 require another minor series.

Exact-prime progress now defaults to one human-readable candidate and six significant
digits. `--formula-precision 1..17` changes display only; approximate equations are
not executable serialization. Use `--report-format raw` for the dense original
fields (default top5), or `--metrics-file PATH` for full-precision canonical JSONL.
Reporting and output settings can change on resume without changing search state.

Checkpoint format 1 is unchanged: explicit integers/IEEE coefficient bits, versioned
codec IDs and FNV-1a64 checksum. The package/runtime guard still rejects checkpoints
from a different library version or incompatible build/runtime. In particular,
unreleased 0.2.0 checkpoint-development files are not silently accepted by 0.3.0.
No automatic cache/trajectory migration is supplied. Preserve old files and use
that exact qualified build for old runs. Published v0.2.0 did not ship these new
checkpoint APIs; its release assets remain untouched.

The built-in codec handles ordinary ExpressionCandidate only. Custom candidates,
constrained/template candidates and derived mutable services need explicit codecs
and context identities. Capture/restore only while idle, never during evolve or
concurrent public access. Custom callbacks retain their concurrency obligations.

A clean committed-boundary restart restores the deterministic trajectory under the
qualified configuration. After SIGKILL/crash, restore the latest durable checkpoint;
unknown work since it may be repeated and is absent from saved counters. There is
no per-objective write-ahead journal or exactly-once external-callback guarantee.
`--loss-budget N` is total, not additional. Worker-count changes are rejected.
SIGINT/SIGTERM drain and save normally (exit0 with explicit cancelled state); I/O
failure exits1 and preserves earlier valid files where replacement did not commit.
