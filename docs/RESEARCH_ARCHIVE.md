# Historical Genetik research archive

## Preservation status

Inventory base: `main` `8a321344e8f234616bee1b2d51902bfa82cfc1b0` (tree `2e4b5f45b967c35f3d8b417e47a2de22ec1784f1`). The local bundle contains this context ref plus all 29 local-only research branch tips. The branch tips contain 153 distinct commits beyond `main` by ancestry; each branch is unmerged/divergent from `main`. `git cherry` found no patch-equivalent commits for these tips. A branch may still contain selected capabilities separately curated into `main`; that does not make its full research history redundant.

The bundle does **not** authorize branch retirement. It is on the same device as the working repository, and no independently verified off-device copy is known. Preserve all 29 refs until off-device integrity verification and separate explicit retirement authorization. The ignored experiment outputs listed below also remain in place and are not in Git history.

## Archive identity

- Directory: `../archive/genetik/historical-branches-2026-10-31-main-8a321344/` (created without overwriting existing archives).
- Bundle: `genetik-historical-research.bundle` (300,576,479 bytes).
- SHA256: `46961582238ed6941024152296a6001fc5ca9f16d115cb9a6b7ca8b0328e5f60`.
- Ref coverage: `main` and all 29 exact local branch refs, preserving reachable histories.
- Validation: `git bundle verify` succeeded; a disposable mirror restored all 30 refs with exact commit and tree IDs. See `restoration-verify.json`, `bundle-verify.txt`, `content-scan.json`, and `SHA256SUMS` in the archive directory.
- Full machine-readable inventory is retained locally at `../archive/genetik/historical-branches-2026-10-31-main-8a321344/manifest.json`. It records exact tips/trees, unique commit IDs relative to `main`, merge bases, changed paths, tracked research evidence paths, and branch tip ancestry/identity relationships; it is not part of the public source change.

## Branch inventory

| Branch | Exact tip | Unique commits vs main | Changed paths | Production overlap and retained research |
|---|---|---:|---:|---|
| `experiment/prime-fit-budget` | `0b78f67cdf473f34d34b25cb55501435caabc425` | 37 | 980 | Prime coordinate-budget campaign; objective evidence remains unique. |
| `experiment/prime-fitness-diagnostics` | `8624482b20dbd9df50d9d5a9169f771651d4a5ca` | 10 | 235 | Prime fitness and stagnation diagnostics; study results remain unique. |
| `experiment/prime-fitter-evolution` | `62ffa989b747f6dd2580d0bef837e876758f4456` | 26 | 669 | Evolutionary fitter experiment; campaign results remain unique. |
| `experiment/prime-local-variation` | `e2204f95447a3898244de11793132dab56061f30` | 51 | 1346 | Mutation locality study; full results and replay evidence remain unique. |
| `experiment/prime-refinement-efficiency` | `46d4867d463641749b2f7527d75543370ca5c48d` | 18 | 385 | Refinement efficiency study; measured outcomes remain unique. |
| `feature/advanced-regression-s1` | `7e73bee18e0660383d1dfeef7ba7e97323a41fc2` | 19 | 505 | Selected S1 gradient-regression capability is in 0.7.0; branch also retains B1/B2 research. |
| `feature/compositional-discovery-m2` | `41fa10d036c8efa56d2c807c7899274474f4221f` | 6 | 106 | Some underlying APIs overlap; M1/M2 discovery implementation and outcomes are not represented by the current production surface. |
| `feature/constraint-aware-mathematics-s2` | `31c131b5b2e185ec82476e5b0b317af83bee3978` | 20 | 517 | Selected S1/S2/quantized-constraint functionality is in 0.7.0; B1/B2 research and complete branch history remain unique. |
| `feature/discrete-expressions` | `398c391e7aa3e187391c90747a71f4d382e7e3e0` | 57 | 1454 | D1–D3 implementation was selectively integrated; branch-specific experiments and follow-up evidence remain unique. |
| `feature/discrete-iteration` | `50d88668baa22ce3d9e8ed2d0203a31fa11b356d` | 62 | 1642 | D1–D3 implementation was selectively integrated; iteration experiments and evidence remain unique. |
| `feature/discrete-prime-discovery` | `c0e27e9bbe4a81930658c3b914228a0dde08c22e` | 67 | 1880 | D1–D3 implementation was selectively integrated; prime-discovery outcomes and evidence remain unique. |
| `feature/mixed-numerical-expressions-m3` | `6570ff456a16a2e4cd565560e5a2c8978f53b1fe` | 15 | 196 | MixedExpression capability is in 0.7.0; M3 research conclusions and campaign gaps remain unique. |
| `feature/number-theory-m1` | `860c09feb019bfd56b47994e2ed9bb0dc3ac9da1` | 2 | 27 | No full M1 discovery framework is in the current production surface; research outcomes remain unique. |
| `performance/scalar-throughput-p1` | `9c83a2804527344157c0c279fdaf0154489ff983` | 13 | 751 | Performance experiment only; no production implementation from this branch is claimed integrated. |
| `qualification/scientific-regression-s3-pre-curation` | `31c131b5b2e185ec82476e5b0b317af83bee3978` | 20 | 517 | Same exact tip as the S2 branch; selected 0.7.0 capabilities are integrated, while its pre-curation B1/B2 history remains unique. |
| `research/candidate-lifecycle-attribution` | `06b70395eab215cd20d09e82443c7522fee4d6d7` | 10 | 119 | R3 lifecycle attribution instruments and results remain research-only. |
| `research/casewise-selection-r7` | `be09551d1729f0efb75783ae28bf9050063f7bf7` | 42 | 797 | R7 case-wise selection implementation and results remain research-only. |
| `research/compositional-evolution-r8` | `572f5a698594d7d7170479370a34cb336464a5c0` | 50 | 1321 | R8 compositional evolution instruments and results remain research-only. |
| `research/direct-prime-discovery-m3` | `c8734fc8eef0088916eceb9586e4aec106a0f2c1` | 14 | 185 | M3 source capability overlaps 0.7.0; direct-prime discovery results remain research-only. |
| `research/evolutionary-search-effectiveness` | `f3b9dbaf7a29de6adf0fd236580b08bf81e56e97` | 3 | 14 | R1 effectiveness protocol and paired outcomes remain research-only. |
| `research/exact-gap-discovery-b2` | `e6046fea92d3c8f8ce0ac06dacc74f14824c74cb` | 18 | 474 | B2 exact-gap discovery remains unfinished research; do not treat similar APIs as integration. |
| `research/fitting-budget-validation` | `68872921a48bd78a83e94eccfec5d0a195cc48c8` | 31 | 754 | R5 fitting-budget validation and handoff remain research-only. |
| `research/mathematical-discovery-readiness` | `72fdb2b7eab0b5854d0095d22db6c8f683813030` | 12 | 180 | M2.1 readiness results remain research-only; no complete discovery framework is claimed in production. |
| `research/mixed-successor-discovery-b1` | `0cfa073edd0b2f631ed59bcfe49f23c4032e6dce` | 16 | 319 | B1 mixed-successor discovery remains research-only. |
| `research/mixed-successor-readiness-b1-1` | `333ec810648151d11f26248243cd9c57bb3f2c7a` | 17 | 380 | B1.1 reachability/readiness analysis remains research-only. |
| `research/per-candidate-fitting-budget` | `d42bd5e72f99429204bd147958b537458547bd7a` | 24 | 486 | R4 fitting-budget allocation evidence remains research-only. |
| `research/search-benchmark-synthesis-r9` | `c1e7d558be379d755498fa9555c9f1e5f3aa648f` | 53 | 1486 | R9 search/benchmark synthesis remains research-only. |
| `research/search-bottleneck-attribution` | `43b1d2b98dc6b26a2aaa1913c213a01cc7753df5` | 6 | 28 | R2 bottleneck attribution instruments and outcomes remain research-only. |
| `research/semantic-pareto-retention` | `d67b54b5a4398b48d965378adc7731187131a215` | 40 | 783 | R6 semantic-retention implementation and evidence remain research-only. |

The two branch names `feature/constraint-aware-mathematics-s2` and `qualification/scientific-regression-s3-pre-curation` point to the same commit and tree (`31c131b5b2e185ec82476e5b0b317af83bee3978`). They remain separate refs in the bundle and manifest. Branch-specific unique-commit counts overlap because branches share ancestry; the union is 153 distinct commits. The bundle preserves 30 refs total.

## Tracked evidence and production overlap

Every branch’s complete changed-path and tracked-evidence lists are in the machine-readable manifest. They include frozen protocols, results, qualification logs, source snapshots, failed runs, benchmark records, and research handoffs under `docs/`, plus campaign applications where present. The evidence is reachable from each archived commit. The selected D1–D3 implementation has a curated integration record in [DISCRETE_RELEASE_INTEGRATION.md](DISCRETE_RELEASE_INTEGRATION.md); the current 0.7.0 source contains selected optional S1/S2 and mixed-expression code. Those curated code portions do not include the branches’ complete 153-commit unique history or establish that all studies/results were integrated. M1/M2, B1/B1.1/B2, R1–R9, P1 and prime-fitter experiment conclusions remain distinct research.

No branch deletion is authorized by this archive. Of the 29, **zero are currently eligible for retirement**: the required independent off-device backup and explicit branch-specific retirement authorization are absent. The ongoing/unfinished B1/B1.1/B2, M1/M2 readiness, R-series, prime-fitting/variation, and discrete follow-up research should remain accessible. All branches remain accessible until the preservation gates are met.

## Ignored and untracked outputs

The repository worktree reports `out/` as ignored; these files are not represented in the bundle. The initial disk inventory found about 33 GB under `out/runs/` (including roughly 16 GB `prime-local-variation/`, 14 GB `prime-fit-budget/`, and 2.6 GB `r6/`), about 14 GB under `out/tmp/`, about 32 GB under `out/build/`, and additional consumer, log, release, install, cleanup, and source-archive-check data. Run-directory names include `r1`–`r6`, `prime-fitter-evolution`, `prime-refinement`, `prime-fit-budget`, `prime-local-variation`, `discrete-d1`–`d6`, `hosted-calculus`, parser/typed qualification, and checkpoint studies. These outputs were not copied, moved, or deleted. Preserve them; separately classify research evidence versus regenerable builds and caches before any later cleanup. Their contents were not checksum-inventoried in this Git archive.

## Content and confidentiality review

The reachable Git bundle is unpublished research material and must remain private unless separately approved for sharing. The archive includes historical campaign protocols/results and a compressed R6 trace/failure archive. A credential-pattern scan report is included; it scanned all reachable Git blobs and decompressed gzip and tar.zst evidence. No private-key, GitHub-token, AWS-key, or credential-assignment marker was confirmed. One `ghs_` match was the CMake `CMAKE_GHS_*` compiler-variable prefix in an existing hosted log, not a token. This scan is not legal or publication clearance. Off-device transfer should use a private, access-controlled destination; verify the supplied SHA256 and restored commit/tree IDs there before considering any retirement.

## Recovery

From a disposable directory, verify and restore without touching the active repository:

```sh
git bundle verify /path/to/genetik-historical-research.bundle
git clone --mirror /path/to/genetik-historical-research.bundle /path/to/disposable/genetik-research.git
git -C /path/to/disposable/genetik-research.git show-ref --heads
```

Compare every recovered `refs/heads/<branch>` commit and `^{tree}` with `manifest.json`; compare `main` as well. For a normal worktree rather than a mirror, clone the bundle and check out a chosen branch explicitly. Do not delete the original refs until exact restoration has been independently checked on another device and explicit retirement authorization is recorded.

## Lifecycle skill

The updated [Genetik branch-lifecycle skill](../.agents/skills/genetik-branch-lifecycle/SKILL.md) separates merged development cleanup, unmerged research archival, and ambiguous/active work. Its read-only policy check fails closed unless exact-tip coverage, bundle verification, restoration, off-device backup, explicit authorization, and clean/inactive worktree conditions are all established.
