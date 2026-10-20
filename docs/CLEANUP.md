# Phase 1: workspace and output cleanup

Started from clean `main` at published `f9299ea2cb5136d06e2de8f68bb83ecf460ca369`;
work performed on local `cleanup/workspace-phase-1`. One worktree, no active
Genetik compilation/search and no typed-math edits were present. No reset, remote
push, default-branch update, tag or release operation was performed. Public headers,
generic OOP interfaces and moved campaign C++ bodies are byte-identical to the
checkpoint. Code/layout/hygiene changes are committed as `b444bdd`; this report and
the phased roadmap are a documentation-only follow-up.

## Actions and retained material

Before acting, `out/cleanup/inventory-before.json` measured each parent entry and
existing generated subtree. `manifest.json`/`manifest.tsv` recorded 194 concrete
source/action/destination/reason/provenance/verification entries. All are resolved.
Copies and per-file hash indexes live at `../archive/genetik/`; compaction has its
own member-by-member manifest. No broad cleanup glob or `git clean` was used, and
symlinks were not traversed. Uncertain material was archived, not declared unused.

- 177 legacy entries moved to `archive/genetik/legacy-workspace/`: loose arithmetic,
  concurrency, constant-fitting, multivariate/OOP/prime logs and campaign text;
  data directories; delivery/evaluation evidence and OOP replays; old external
  consumer/install fixtures; built probes and their unique source. The original
  measured timing records remain intact, including unsuccessful results.
- Historical `baseline-source/` and `evaluation-reference/` are compact snapshots.
  Across those and the three inspected preflight/release extracted sources, all
  1,023 source files matched blobs in Git history. The loose baseline reproduction
  differs from `docs/reproductions/baseline.cpp` only by that file's initial-revision
  comment. Timing/detail/negative-contract probes have no byte-identical current
  source; they and their paired binaries are deliberately retained as legacy data.
- `out/{concurrency,hosted,hosted-preflight,publication,release-review}` records moved
  to compact `archive/genetik/records/*.tar.gz`. After original per-file verification,
  1,335 preflight, 1,464 review and four publication duplicate/generated members were
  retired, retaining 109, 91 and 29 files respectively. Logs, original measurements,
  workflow metadata, manifests and failure evidence remain. Unchanged generated
  source copies can be recovered from Git, rather than maintained as checkouts.
- All seven old parent build trees were deleted **after** fresh replacement gates:
  `build-baseline`, `build-cmake-min`, `build-debug`, `build-min-contracts`,
  `build-release`, `build-sanitize`, `build-tsan`. Their 6,305 files occupied
  812,083,580 logical bytes. Compressed `archive/genetik/build-records/` retains
  2,233 log/cache/data/source/configuration files, verified before deletion.
- Authoritative published bytes are separately accessible at
  `archive/genetik/published/v0.1.0/`: source archive SHA256
  `7f632489e45abb97385d98c9c2f703664386e358b22f3f73baa8a72c99b6b992`, and
  `SHA256SUMS` SHA256
  `a12d576eedc38b80af11f118d82d086a992f3d8930b89bda743b91191663353b`.
  The existing annotated tag still resolves to the published checkpoint.
- All **127** frozen evidence files and paths match
  `docs/qualification-results/historical-artifact-sha256.json`. Historical transcripts
  were not rewritten. Intentional relocation paths with spaces remain in evidence
  and are recreated by the fresh integration fixtures.

Retained exceptions: `.tools/` and the entire `delivery-tools/` remain stable.
The latter contains required minimum-CMake source/build/install material, not an
obsolete Genetik build. Unique legacy downstream fixtures/probe binaries remain
indexed archive records; they are not active source checkouts or current tools.
Future additional sources should use managed worktrees.

## Measured workspace change

The inventory covers the whole parent workspace, including tools, Git metadata,
archives and freshly qualified output. Counts are regular files (not directories);
logical bytes sum file sizes, allocated bytes sum filesystem blocks. These are
recorded filesystem snapshots, not estimates from `git status` or free-disk space.

| Measure | Before | After qualification/retirement |
| --- | ---: | ---: |
| Parent entries, including hidden tools | 189 | 4 |
| Regular files | 35,789 | 32,421 |
| Logical bytes | 1,481,751,812 | 1,277,801,685 |
| Allocated bytes | 1,576,148,992 | 1,368,670,208 |

Net reduction: **194.50 MiB logical / 197.87 MiB allocated**, even retaining new
builds. At measurement, fresh `out/build/` held 5,894 files / 671,418,210 logical
bytes; the archive held 984 files / 54,238,438 bytes. Later report/commit bookkeeping
adds small metadata; exact measurement time and per-entry counts are retained in
`out/cleanup/inventory-after.json` and archive copies. The four parent entries are
`Genetik/`, `.tools/`, `delivery-tools/`, `archive/`.

## Canonical workflow and prevention

See [ORGANIZATION.md](ORGANIZATION.md) for the full role map. Builds are configured
fresh under `out/build/`; manual installs/consumers use `out/install/` and
`out/consumers/`; generated runs/data/logs/scratch use `out/runs/`, `out/data/`,
`out/logs/`, `out/tmp/`. `out/cleanup/` is the deliberate action-record exception.
Caches were not moved. The two compiled campaigns now live in `apps/campaigns/`,
with unchanged executable names. Scripts, README, current guide commands, CI paths
and AGENTS instructions use the new conventions; historical command records retain
old paths. A root in-source configure now fails with a command hint.

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON
cmake --build out/build/release -j2
cmake -E chdir out/build/release ctest --output-on-failure
cmake --install out/build/release --prefix "$PWD/out/install/local"
cmake -S examples/quickstarts -B out/consumers/quickstarts -DCMAKE_PREFIX_PATH="$PWD/out/install/local"
cmake --build out/consumers/quickstarts -j2
out/consumers/quickstarts/scalar_quickstart --threads 2
out/consumers/quickstarts/symbolic_quickstart --threads 2
cmake -S tests/oop_consumer -B out/consumers/oop -DCMAKE_PREFIX_PATH="$PWD/out/install/local"
cmake --build out/consumers/oop -j2
out/consumers/oop/permutation_consumer --threads 3
python3 scripts/check_hygiene.py --workspace --self-test
```

`check_hygiene.py` audits location/role: unknown root entries, stray source caches
or objects, C++ applications in scripts/, and unclassified output entries fail.
Intentional evidence/fixture text and tables remain allowed. It never deletes or
follows links. CTest/CI run repository mode; optional workspace mode audits this
local parent. Self-tests cover permitted data, misplaced output and symlinks.

On this Jetson `cmake`/`ctest` above are `../.tools/bin/{cmake,ctest}` (4.4.4).
Exact minimum tools are `../delivery-tools/cmake-min/bin/{cmake,ctest}` (3.16.0).
For `cmake -E chdir`, use an absolute tool path or put its bin directory on PATH.
The library still supports ordinary CMake 3.16 commands and parent compiler choices.

## Executed verification

Jetson ARM64 Linux 6.8.12-1021-tegra; GCC 13.3.0; glibc 2.39-0ubuntu8.8,
libstdc++6/libtsan2 14.2.0-4ubuntu2~24.04.1. Logs: `out/logs/cleanup/`.
All full builds used `-DGENETIK_WARNINGS_AS_ERRORS=ON` and
`-DGENETIK_RECORDED_REFERENCE=ON`, with `cmake --build ... -j2`.

| Fresh directory / settings | CTest | Suite seconds |
| --- | --- | ---: |
| out/build/release / Release, CMake 4.4.4 | 26/26 | 112.25 |
| out/build/debug / Debug, CMake 4.4.4 | 26/26 | 120.08 |
| out/build/sanitize / Debug, GENETIK_SANITIZERS=ON | 26/26 | 270.52 |
| out/build/minimum / Release, exact CMake/CTest 3.16.0 | 26/26 | 60.27 |
| out/build/tsan / Debug, GENETIK_THREAD_SANITIZER=ON, examples OFF | 2/2 | 0.74 |

For each directory configure with the corresponding settings above; run
`cmake -E chdir out/build/NAME /absolute/path/to/ctest --output-on-failure`.
TSan built only `genetik_concurrency_tests genetik_oop_consumer`, then ran
`TSAN_OPTIONS=halt_on_error=1 ... ctest -R 'genetik_concurrency_contracts|genetik_oop_parallel' --output-on-failure`.
Sanitizer configurations remained separate. These wall times include varying build
load and are not performance comparisons.

Full builds compiled standalone public/support headers without warnings. Full suites
cover strict-FP negative compilation, ordinary/relocated installed multi-TU
scalar/symbolic/OOP consumers after disposable producer removal, source inclusion,
private development flags, versions, quickstarts, concurrency and file workflows.
The new hygiene case makes 25 portable cases / 26 with the existing Jetson snapshot;
TSan still selects two. The portable manifest checker and archived actionlint passed.
The current CI paths/manifest are configured and locally checked, **not hosted-run**
by this local-only task; published hosted results retain their original SHA/scope.

The first minimum test invocation started before its final fitting binary linked;
it reported that case Not Run. `minimum-premature-ctest.log` is preserved; the full
rerun after build completion passed. A 128-loss fitting-enabled CLI smoke correctly
exhausted its budget before initialization publication (exit 2); that log is retained.
The completed 128-loss no-fit file smoke published a generation and scored separate
interior data. Character (20 generations, two workers), manual expression, data
creation and installed-consumer runs succeeded. Root entries before/after those
workflows matched; final repository/workspace hygiene passed.

No search replay or scientific campaign was repeated: no engine, arithmetic, RNG,
fitness, fitting or public-header behavior changed. Captured numerical checks and
existing deterministic contracts passed. Broader platform qualification remains as
recorded in SUPPORT.md. Next is separately scoped typed values/evaluation, not a
mathematical API change hidden in cleanup.
