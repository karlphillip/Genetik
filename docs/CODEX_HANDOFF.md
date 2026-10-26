# Codex project handoff

## D7 state

- Phase: D7, checkpoint A — integration and qualification baseline.
- Repository: `/home/karlphillip/workspaces/primeResearcher/Genetik`.
- Branch: `qualification/advanced-mathematics-d7`.
- Current committed SHA: `b2930fa0fc126dbabfc17a01f05f49e3eb32471a`; checkpoint-A source/test repair is in the working tree pending its local commit.
- Published 0.4.0 tag resolves to `c08859e462210a1ef378b0c6c47a2b4c520f7353`, an ancestor of D6. The qualified D6 implementation `c72eaaa3f1ca817ba804d36321069e58ac288e6f` is also an ancestor. The v0.4.0 tag/assets have not been changed.
- Version metadata is now `0.5.0` in the D7 working tree; release designation remains development pre-release.
- Working tree was clean at phase start. No active test, qualification, or competitor processes were found.
- No remote writes, PRs, merges, tags, uploads, or publications are authorized or performed.

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

1. Finish checkpoint-A scope inventory and commit the 0.5.0 metadata/checkpoint migration repair. Verify CMake/package behavior for 0.5 while preserving 0.4 consumer requests.
2. Before any target benchmark, freeze `docs/d7/BENCHMARK_PROTOCOL.md`: versions/commits/licenses, acquisition isolation, datasets/splits/noise/seeds, grammars/complexity, budgets, CPU/memory controls, metrics, and unsupported-comparison rules. Preflight timing must use a non-benchmark task and be recorded separately.
3. Acquire/build competitor tools only in isolated `out/tmp/d7/` environments. PySR requires its Python/Julia backend; Operon is official C++23 and may require dependencies unavailable on this aarch64 host. If an upstream tool cannot be built/run without changing global packages, document that limitation.
4. Use a shared independent scorer, preserve every seed/failure/timeout, and do not start concurrent benchmark jobs. Do not tune to recovery outcomes or introduce new operators/search algorithms.
5. Verify D5/D6 and new migration tests with the final derived manifest, then full Release, Debug, CMake 3.16.0, complete ASan+UBSan, TSan three-repeat selections, packaging/consumers, strict FP, workflow and repository hygiene. Keep the parent hygiene exception as a failed gate unless the parent owner resolves it.
6. Prepare 0.5.0 metadata/release docs/source archive/checksums and the hosted-qualification approval packet. Stop before remote writes. Hosted-upload permission, main integration, tags and publication each require separate authorization.

## Evidence pointers

- D5 qualification: `docs/D5_QUALIFICATION.md`; detailed retained records: `docs/d5-results/qualification/` and `docs/d5-results/benchmarks/`.
- D6 qualification: `docs/D6_QUALIFICATION.md`; retained campaign and qualification evidence: `docs/d6-results/`.
- D5 implementation source `5e44bcf` and D6 implementation source `c72eaaa` are qualification pins, not the current release candidate SHA.
- Workspace preservation record: `out/runs/d6/preservation-final.json`; frozen checksum inventory: `docs/qualification-results/historical-artifact-sha256.json`.
- Hygiene implementation and self-test: `scripts/check_hygiene.py`.

Update this file after each D7 checkpoint and before handing off context. Keep detailed outputs in repository evidence files and summarize them here; do not paste full logs into context.
