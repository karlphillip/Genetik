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

Executed results and crash recovery follow below; exact final candidate/archive
identities are recorded in the ignored hosted-preflight packet. Hosted qualification is pending;
existing hosted claims retain their recorded commit/runtime scope.


Frozen local protocol: unchanged exact-prime seed 42, four workers, population 64,
default grammar/fitter, total budget 1,000,000. Reports and periodic checkpoints every
0.05 seconds, keep 3. After at least four flushed snapshots and retained nonzero
checkpoint generations, SIGKILL the exact subprocess PID, wait/reap and preserve
its files. Verify all three numbered files and latest by checksum and full state
restoration without objective work. In a fresh process resume with total 2,000,000,
precision 4, unchanged execution settings; SIGTERM after at least two more committed
generations. No configuration choice depends on prime-recovery quality. The helper
is scripts/check_checkpoint_crash.py and its deadline is 45 seconds per process.


## Frozen implementation and executed checks

Behavior/package metadata frozen at **5a75d2ab04a27ef50022f060d372409f8fe908fe**.
All later tracked edits record documentation/evidence only. Fresh configurations
live in out/build/checkpoint-preflight-{release,debug,sanitize,minimum,tsan}.
The old checkpoint qualification remains scoped to its original0.2 development SHA.

Native environment: Jetson Linux ARM64, kernel6.8.12-1021-tegra, GCC13.3.0 Ubuntu
13.3.0-6ubuntu2~24.04.1, libstdc++13 headers (__GLIBCXX__20240904), GNU runtime
libstdc++.so.6.0.33, glibc/libm2.39 (Ubuntu2.39-0ubuntu8.8), CMake/CTest4.4.4 and exact3.16.0.
Warnings-as-errors and strict -fno-fast-math -ffp-contract=off were active. Separate
Debug qualification used address,undefined or thread sanitizer at compile and link,
with frame pointers; no instrumentation/suppression/numerical expectation was weakened.

Actual local manifests agree on 62 test names. Portable 61 excludes only
**genetik_evaluation_reference**, the captured Jetson-specific snapshot (also passed
locally). [Named portable manifest](checkpoint-preflight-results/qualification/portable-tests.txt)
retains all mathematical, OOP, parser/template/calculus, prime, checkpoint, header,
FP and delivery contracts. Precision/durability cases extend existing tests rather
than dropping or renaming them. Three separate-process phases preserve 100 vs
40/save/destroy/load/100 exact search state; nonbinding worker 1/2/4 comparisons also
change display precision on resume, preserving coefficient/fitness/cache/archive,
RNG, history and search-counter equality. Elapsed/reporting observations may differ.

All 12 named TSan selections passed three times each, verified per name (36 successful
executions), plus one complete integral-contract execution. See
[repetitions](checkpoint-preflight-results/qualification/tsan-repetitions.json),
[transcript](checkpoint-preflight-results/qualification/tsan-tests.log) and
[instrumentation](checkpoint-preflight-results/qualification/instrumentation.txt).

## Durability audit

Source inspection confirms same-directory mkstemp, checked unbuffered writes,
file fsync/close, read-back checksum validation, rename and directory fsync. Each
new Genetik-created directory syncs itself and its parent; retention cleanup follows
successful installation and syncs deletion. Existing caller-created ancestors must
already be established. A failure after rename/during directory fsync means uncertain
durability and is reported; no rollback guarantee is invented after that boundary.
Hard crashes in the cleanup window may leave extra old/temporary files; only complete
checksummed files are resumable. FNV-1a64 detects accidental corruption, not forgery.

The Debug contracts additionally passed under strace. The
[selected syscall audit](checkpoint-preflight-results/qualification/durability-syscalls-selected.txt)
shows new nested-directory parent syncs, temporary-file syncs and successful renames,
plus the injected ENOENT rename failure. The test then reloads the previous valid
checkpoint. Allocation/pre-rename failures, missing paths, bad/corrupt/truncated
files, configuration/counter mismatches and retention/rollback checks remain active.
The first audit invocation used a directory name rejected by the fixture's safety
guard. It failed before any checkpoint work; the corrected checkpoint-fixture path
passed without changing the guard/assertions. That setup diagnostic is retained.
This audit and process-crash experiment do not simulate hardware power failure.

Hosted qualification has not executed for this candidate. The existing workflow
triggers on push/pull_request without branch/path filtering, so the proposed branch
and later documentation-only report tip execute the complete matrix. Required jobs
remain GCC13 x64 Release/exact CMake3.16, Clang18 x64 ASan+UBSan, and GCC13 ARM64
Release plus separate TSan. No libc++, Windows/MSVC or AppleClang/macOS support is
newly claimed. Future hosted results must record their actual toolchain/runtime/SHA.


A second syscall audit injects ENOSPC into the replacement temporary-file write
(strace inject=write:error=ENOSPC:when=2). The trace confirms the error applies to
atomic.gtkc.tmp-* after the first known-good file has been saved. The same unmodified
contracts catch the failed replacement, reload the previous payload and complete
successfully. [Injected-write trace](checkpoint-preflight-results/qualification/durability-write-fault-selected.txt)
and [contract result](checkpoint-preflight-results/qualification/durability-write-fault-contracts.log)
are retained. This is actual syscall fault injection, not a sanitizer suppression.

The displayed generation limit is the existing maximum attempted-generation ceiling;
failed private attempts can consume it without advancing the committed generation.
The separate published/attempted counters and loss allowance remain authoritative.


## Full local results

| Configuration (frozen implementation SHA above) | Result | Elapsed |
|---|---|---:|
| GCC13 Release / CMake4.4.4 | 62/62 | 328.56s |
| GCC13 Debug / CMake4.4.4 | 62/62 | 356.72s |
| GCC13 Debug ASan+UBSan / CMake4.4.4 | 62/62 | 1248.99s |
| GCC13 Release / exact CMake3.16.0 | 62/62 | 255.37s |

These suites include generic OOP isolation, standalone headers, strict-FP/warnings,
source inclusion, installed/relocated/multi-TU consumers and actual compiled
quickstarts. The final documentation/evidence commit adds no executable change;
its exact archive is independently verified from extracted source without .git.
All three accepted serial replays (prime, arithmetic, cadence4) match historical
stdout byte for byte. No full discovery campaign was repeated. All 127 frozen hashes,
published archive/checksum identities, main and tags remain unchanged.

## Frozen real-process SIGKILL recovery

[Protocol](checkpoint-preflight-results/protocol.json),
[full state/file identities and process results](checkpoint-preflight-results/summary.json),
[killed process output](checkpoint-preflight-results/sigkill.txt),
[resumed output](checkpoint-preflight-results/resumed-sigterm.txt) and full-precision
JSONL are retained. This fresh Release experiment uses the exact frozen implementation
SHA above; preliminary identical-protocol development evidence remains separately
under out/runs/checkpoint-preflight/crash-smoke. No seed/configuration was tuned.

The process displayed generations 0,1,2,3 before controlled SIGKILL (exit status -9;
no final save/report). Retention left exactly:

```text
generation-00000000000000000001.gtkc
generation-00000000000000000002.gtkc
generation-00000000000000000003.gtkc
latest.gtkc
```

Each numbered file passed checksum, context and complete population/fitness/archive/
RNG/counter restoration in a separate inspector process, without objective work or
active workers. latest was a complete byte-identical copy of generation 3, with
17,044 saved objective attempts. A fresh process resumed G3, published G4 and G5,
then SIGTERM requested cancellation; the private G6 attempt drained and was discarded.
The final checkpoint preserves G5, six attempted generations and 30,539 actual saved
loss attempts. Exit status 0 and cancelled state are explicit. Retention again left
three recoverable numbered generations and latest. Formula precision changed from
six to four while canonical/JSONL/checkpoint coefficients remained complete.

Killed latest SHA256:
`a1a8d030238a771369bbd38531ec4900419f94f44f888eabf93cc709b6e6bfe0`.
Final latest SHA256:
`69a7da83aa018024dba2314715750cf4bc0a4835de6c3a3fdeab08a1d12b6def`.
Binary files remain under out/runs/checkpoint-preflight/crash-qualified; the retained
summary indexes every generation's size/hash and restored state. SIGKILL did not
create a final save. Work after the last durable checkpoint is unknown and may be
repeated; this is crash recovery, not a claim of exactly-once attempted work.
No exact 1,000-prime recovery or performance result is claimed.

## Preflight/release boundary

The proposal is 0.3.0, tag v0.3.0, development pre-release, latest=false, source-only
assets. New metadata is locally qualified; SameMinorVersion correctly rejects 0.2
requests against 0.3. Existing published releases/assets remain unchanged. The
archive verifier reads the committed Version.hpp authority and preserves its
historical literal-CMake fallback. Exact final archive identities and extracted-
source results belong in out/runs/checkpoint-preflight/preflight/PREFLIGHT.md.

GitHub CLI authentication and repository push permission were checked read-only;
existing SSH ls-remote works. No login reset, remote upload, PR, main update, tag or
release operation occurred. The next action is one explicit approval for uploading
the listed full stack since v0.2.0 and executing/repairing/requalifying the existing
matrix on qualification/checkpoint-preflight. PR/main/tag/publication remain separate.
