# Codex project handoff

## D7 state

- Phase: D7, checkpoint C — frozen protocol committed; neutral preflight evidence is in the resumable manifest; target matrix is next.
- Repository: `/home/karlphillip/workspaces/primeResearcher/Genetik`.
- Branch: `qualification/advanced-mathematics-d7`.
- Current committed SHA: `a33c77bac744afe4025441ab63df74b3a70356ff`; executable implementation remains `daf6c96a8bf437a552ae836c1f0e40edb5859f52` (qualified migration source `c72eaaa3f1ca817ba804d36321069e58ac288e6f`).
- Published 0.4.0 tag resolves to `c08859e462210a1ef378b0c6c47a2b4c520f7353`, an ancestor of D6. The qualified D6 implementation `c72eaaa3f1ca817ba804d36321069e58ac288e6f` is also an ancestor. The v0.4.0 tag/assets have not been changed.
- Version metadata is now `0.5.0` in the D7 working tree; release designation remains development pre-release.
- Checkpoint-A implementation is committed. Targeted GCC 13 Release tests pass 3/3: scalar v0.3, scalar v0.4, and discrete v0.4 checkpoint continuation/compatibility. Full D7 candidate suites remain outstanding.
- No active tests, benchmark jobs, or builds. No remote writes, PRs, merges, tags, uploads, or publications have occurred.

## Completed in checkpoint A

- Read root `AGENTS.md`; keep builds in `out/`, preserve existing evidence, use separate sanitizer/TSan builds, retain the 127 frozen historical hashes, and run repository/workspace hygiene checks.
- Confirmed D5 and D6 source/API integration in one tree. D5 qualification records identify final implementation SHA `5e44bcfaf83e908d0b79a5eb7b23031fffa00284`; D6 record identifies implementation SHA `c72eaaa3f1ca817ba804d36321069e58ac288e6f`. D6 docs/evidence tip is `a05b3974ec636f5921184a90a2a06c3d7dbc84d5`.
- Existing D5 complete local evidence: Release 84/84, Debug 84/84, CMake 3.16.0 84/84, ASan+UBSan 83/83, selected TSan 20 tests x3, complete TSan contracts 11/11. Known limitations and failed/intermediate records remain retained.
- Existing D6 complete individual suites: Release 90/90, Debug 90/90, CMake 3.16.0 90/90, ASan+UBSan 89/89, TSan selected 21 x3 and contracts 13/13. D6 qualification driver ended with a failed workspace hygiene gate due to `/home/karlphillip/workspaces/primeResearcher/session.md`.
- Hygiene distinction: repository cleanliness passes; all current `out/` entries are role directories; parent-workspace audit fails because its explicit allowlist permits only `Genetik`, `.tools`, `delivery-tools`, and `archive`. `scripts/check_hygiene.py --self-test` demonstrates that unexpected parent entries are contractually rejected. The 21 MB `session.md` is outside the repository, user-owned, and must not be modified/deleted. Preserve and report the failure; do not change checker scope to hide it.
- Current host: Ubuntu 24.04.4, Linux 6.8.12-tegra, aarch64 Cortex-A78AE, 8 online CPUs, Python 3.12.3, GCC 13.3, installed workspace CMake 4.4.4, exact minimum CMake 3.16.0. `julia`, `pysr`, and `operon` are not currently on PATH. No benchmark experiment has run in D7.
- Official source pins inspected but not yet frozen into a protocol: PySR v2.7.2 and SymbolicRegression.jl v2.7.2; PyOperon v0.6.2, whose pinned Operon core dependency is commit `fad10f087c592812b1a9ecfabce669f37bf459f7`. The core's v0.6.2 tag is not the dependency pinned by PyOperon. Source licenses were checked (Apache-2.0 for PySR/backend; MIT for PyOperon).
- Resolved the checkpoint migration issue: generic checkpoint loading is still exact-version; additive named multi-policy helpers allow the v0.5 reader to import only v0.4.0/v0.3.0 scalar-expression and discrete-evolution/2 contracts. Actual v0.4.0 writer headers are vendored test-only with tag/blob/SHA provenance. Scalar v0.4, discrete v0.4 and existing scalar v0.3 full-state continuation fixtures passed three targeted CTest cases (GCC 13 Release, warning-as-error). D6 experimental contexts remain outside the published-v0.4 compatibility claim.

## Open issues and next actions

1. Run the fixed 108-cell matrix serially from the committed protocol SHA (SHA256 to be read from the file), and independently assess every result. Preserve timeouts/invalid outputs. Do not alter the matrix after runs begin.
3. Analyze retained D5/D6 findings alongside D7 metrics without changing the search algorithm. Complete D5/D6 specialist numerical references and checkpoint compatibility.
4. Derive the actual final test manifest; run complete Release, Debug, exact CMake 3.16.0, ASan+UBSan, TSan x3 selections, consumers/packaging, strict FP, workflow and hygiene. Preserve the known parent workspace failure on untouched user-owned `session.md`.
5. Prepare the 0.5.0 documentation, source archive, checksums and final local candidate commit. Confirm no executable source changed after its exact qualified SHA, or rerun affected qualification if a real source defect is repaired.
6. Prepare the hosted qualification approval packet. Stop before remote writes. Hosted upload requires explicit authorization; main integration, tags and publication each require separate authorization.

## Evidence pointers

- D5 qualification: `docs/D5_QUALIFICATION.md`; detailed retained records: `docs/d5-results/qualification/` and `docs/d5-results/benchmarks/`.
- D6 qualification: `docs/D6_QUALIFICATION.md`; retained campaign and qualification evidence: `docs/d6-results/`.
- D5 implementation source `5e44bcf` and D6 implementation source `c72eaaa` are qualification pins, not the current release candidate SHA.
- Workspace preservation record: `out/runs/d6/preservation-final.json`; frozen checksum inventory: `docs/qualification-results/historical-artifact-sha256.json`.
- Hygiene implementation and self-test: `scripts/check_hygiene.py`.

Update this file after each D7 checkpoint and before handing off context. Keep detailed outputs in repository evidence files and summarize them here; do not paste full logs into context.

## Checkpoint B acquisition and preflight status (2026-10-26)

- Frozen protocol draft prepared at `docs/d7/BENCHMARK_PROTOCOL.md`; generator, fixed data and SHA manifest are `docs/d7/benchmark_data.py` and `docs/d7/data/`. No target run has started.
- PySR 2.7.2 runs under isolated Julia 1.10.3; archive SHA256 is pinned in `docs/d7/environments/pysr-runtime-sha256.txt`. PyOperon 0.6.2 and its native dependency source commits are locked under `docs/d7/environments/`; a local GCC/aarch64 adaptation patch is retained.
- Accepted neutral preflights are summarized in `docs/d7/preflight_results.json`; raw process logs are under `out/logs/d7/preflight/`. PySR first failed from an adapter loss-API mistake, then preflighted with corrected `elementwise_loss` and deterministic serial configuration. Its fit call required ~117.5 s for a 60 s native preflight setting because Julia/JIT overhead is inside that call; target runs use a hard external 120 s cap. Preserve all attempts.
- An initial PySR import without the pinned Julia environment bootstrapped a venv-managed Julia 1.11 and began compiling to the existing user `~/.julia` depot. That process was identified as this task's own setup process and stopped; its logs/failure remain. Do not clean or alter user Julia files. The accepted runs use the private D7 Julia depot/project/runtime.
- PyOperon setup first failed on the upstream x86-64 preset; the failure log remains. The isolated retry used GCC 13 and a retained aarch64-only build patch. Source checkouts remain unchanged.
- Protocol/tooling checkpoint `a33c77bac744afe4025441ab63df74b3a70356ff` contains the frozen benchmark specification, 18 dataset CSVs and manifest, exact environment pins, adapters, assessor and preflight summary. `out/logs/d7/preflight-results.jsonl` imports three already executed accepted smoke preflights; running `benchmark_matrix.py --mode preflight` was verified to skip them. No target result exists yet.
- No background work is running. The last process ended before handoff; resume by checking `ps` before commands.
