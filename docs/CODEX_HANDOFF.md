# Codex D7 handoff

## Current state

- Milestone: D7, checkpoint D — frozen competitor matrix, Release, Debug, CMake 3.16.0, full ASan+UBSan, workflow-defined TSan and optional LP64 LAPACK consumer gates passed; final hygiene and packaging checks remain.
- Repository: `/home/karlphillip/workspaces/primeResearcher/Genetik`.
- Branch: `qualification/advanced-mathematics-d7`.
- Current HEAD: `20d2e3e` (`Update installed quickstarts to request version 0.5`); full SHA can be read with `git rev-parse HEAD`. This is a test/package metadata repair after the matrix; no executable library code changed after the benchmark implementation SHA.
- Qualified D6 implementation source: `c72eaaa3f1ca817ba804d36321069e58ac288e6f`.
- D7 checkpoint A implementation/source-migration commit: `daf6c96a8bf437a552ae836c1f0e40edb5859f52`.
- D7 protocol/data/tooling checkpoint: `a33c77bac744afe4025441ab63df74b3a70356ff`.
- Published 0.4.0 baseline/tag: `c08859e462210a1ef378b0c6c47a2b4c520f7353`, unchanged and ancestor. No remote writes, tags, merges, PRs, uploads or publication occurred.
- Executable implementation and frozen protocol have not changed since matrix start. Current uncommitted changes are D7 reports, benchmark results/tooling, release notes and README/changelog/roadmap reconciliation; no executable library code or test configuration changed after the tested candidate.

## Active benchmark process

- No benchmark or qualification process remains. ASan+UBSan passed all 90/90 in 4506.15 s (`final-sanitize-test-manifest.json`, `final-sanitize-build.log`, `final-sanitize-ctest.log`); TSan passed all 21 selections three times and complete integral 1/1, discrete/checkpoint 4/4, D5 6/6 and D6 2/2 contracts (`final-tsan-*.log`). Optional LP64 OpenBLAS/LAPACK installed-consumer test passed 1/1 (`final-lapack-build.log`, `final-lapack-optional-package.log`). Long ASan prime/dimension contracts passed in 1375.57 s and 890.45 s. No process was terminated.
- Frozen protocol SHA256: `b0b07d2ae418ca71b024fbaa1374264348d92f2b8290606efda2969a432f1ffd`.
- Target manifest: `out/logs/d7/benchmark-candidates.jsonl`, all 108 rows complete. Every row records protocol SHA256 `b0b07d2ae418ca71b024fbaa1374264348d92f2b8290606efda2969a432f1ffd` and implementation SHA `5a95ea47f983d4ba99704fb9ea955b14264d6741`. Genetik: 14 process completions, 22 timeouts; PySR: 36 timeouts; PyOperon: 36 timeouts. Raw evidence and per-run checkpoints are in `out/logs/d7/benchmark_runs/`.
- Run logs/artifacts: `out/logs/d7/benchmark_runs/`. PySR timed-out runs still have `hall_of_fame.csv`/checkpoint artifacts; each run remains classified as a timeout. Do not alter budgets or repeat a cell based on its score.
- Target data and independent assessment implementation: `docs/d7/data/`, `docs/d7/assess_results.py`. Competitor protocol/adapters are frozen in `docs/d7/BENCHMARK_PROTOCOL.md` and adjacent `docs/d7` scripts.

## Completed work

- Read `AGENTS.md`; builds and output stay under `out/`, frozen historical evidence remains intact.
- Confirmed D5/D6 implementation integration and checkpoint forward-reader policy. Targeted v0.3/v0.4 scalar/discrete checkpoint continuation cases passed 3/3 on D7 source.
- Existing D5 evidence: Release 84/84, Debug 84/84, CMake 3.16 84/84, ASan+UBSan 83/83, selected TSan 20 x3 and contracts 11/11.
- Existing D6 individual suite evidence: Release 90/90, Debug 90/90, CMake 3.16 90/90, ASan+UBSan 89/89, selected TSan 21 x3 and contracts 13/13. D6 wrapper overall failed its parent-workspace hygiene check because of user-owned `/home/karlphillip/workspaces/primeResearcher/session.md`; preserve that failure and file. Repository/output hygiene passed, but parent workspace check did not.
- Official PySR 2.7.2 / SymbolicRegression.jl 2.7.2 (Apache-2.0) and PyOperon 0.6.2 (MIT; Operon core pinned commit `fad10f087c592812b1a9ecfabce669f37bf459f7`) were acquired in isolated `out/tmp/d7` environments. PyOperon needed a separately retained GCC/aarch64 adaptation; upstream checkouts are unchanged.
- Three accepted neutral preflights are in `docs/d7/preflight_results.json`, with raw records in `out/logs/d7/preflight/`. PySR final deterministic preflight: 126.42 s wall, 104.09 CPU s, 0.908 GiB RSS; its fit call took 117.48 s. Target wall cap is 120 s for all systems, enforced externally.
- The full 108-cell frozen benchmark finished. Independent scoring yielded 30 Genetik emitted candidates and 36 PySR timeout-HOF snapshots; PyOperon returned/persisted no recognized candidate. The assessed models met held-out thresholds in 20/30 Genetik outputs (six additional Genetik cells had no expression) and 31/36 PySR HOF snapshots. PySR HOF scores are secondary diagnostics only; all primary PySR outcomes remain timeouts/missing model returns. Row-level scores are `docs/d7/results/benchmark-seed-results.jsonl`; aggregates are `docs/d7/results/benchmark-summary.json`; exact HOF artifact hashes and full raw logs remain under `out/logs/d7/`.
- Release CTest passed 90/90 in `out/logs/d7/final-release-ctest-rerun.log` after fixing stale current-package expectations; original failure logs remain. Debug passed 90/90 in `out/logs/d7/final-debug-ctest.log` (497.01 s). Exact CMake 3.16.0 built and passed 90/90 in `out/logs/d7/final-minimum-build.log` and `out/logs/d7/final-minimum-ctest.log` (342.06 s); its derived manifest is `out/logs/d7/final-minimum-test-manifest.json`. Workflow YAML lint passed in `out/logs/d7/workflow-lint.log`. Frozen inventory integrity passed all 127 entries in `out/logs/d7/historical-evidence-integrity.log`.

## Next actions

1. Run final hygiene/strict-FP and source archive checks.
2. Complete the approval packet with exact candidate SHA, results, source archive SHA256 and hosted workflow proposal; update this handoff.
3. Commit only intended D7 docs/results/tooling. Verify a clean tree and source identity; then push only `qualification/advanced-mathematics-d7` as explicitly authorized. Do not manually dispatch hosted CI, open a PR, merge, tag or publish.

D7 scientific campaigns are complete and must not be repeated. Pushing the qualification branch is authorized after local work is complete; hosted CI may trigger on push. Do not manually dispatch hosted CI without authorization. Keep detailed logs in files and update this handoff at each checkpoint.
