# 0.4.0 integration audit

The release branch is constructed from published0.3.0,
`ac2786e824198cead70b32ae44229be35baed1ff`, rather than uploading the entire
research ancestry ending at `c0e27e9bbe4a81930658c3b914228a0dde08c22e`.
That original range contains67commits and remains intact on
`feature/discrete-prime-discovery` and the earlier research branches. The exact
67-entry commit/file ledger and new upload range are in the ignored D4 preflight
packet. No research branch is rebased, reset, deleted or promoted to main.

| Original group | Commits | Release treatment |
|---|---:|---|
| Fitness shaping/diversity study through8624482 |10| Independent experimental objectiveB, diagnostics/plots/CLI changes excluded |
| Frozen coefficient refinement through46d4867 |8| Experimental simplex/surrogate optimizers and study application excluded |
| Evolutionary fitter comparison through62ffa98 |8| NM preparation selector and research journals excluded |
| Coordinate fitting budget study through0b78f67 |11| Research instrumentation/CLI journals excluded |
| Localized mutation study throughe2204f9 |14| Mixed-point/point mutation and lineage observers excluded |
| D1 through398c391 |6| Exact expression/grammar/regression/codec code, references, consumers, original qualification retained |
| D2 through50d8866 |5| Scoped iteration/run state, quickstart/campaign source and complete frozen results retained |
| D3 throughc0e27e9 |5| Independent references/application/identity, fixtures, all ten outcomes and original failures retained |

## Dependencies and mixed commits

The generic algorithm, Population, EvaluatedCandidate, Execution and RandomContext
are byte-identical between0.3.0 and the research tip. D1/D2/D3 depend on those
existing extension points, not on the experimental edits to ConstantFitting or
ExpressionVariation. Those two production headers are restored exactly to0.3.0.
No Nelder–Mead, objectiveB, mixed-point mutation, research eligibility/cadence or
observer abstraction is imported. Existing coordinate fitting/default controls
are unchanged. No stable0.3.0 public API is removed.

DiscreteExpression, parsing, formatting, regression, variation, candidate codec and
search configuration are integrated from the D3 tree. DiscreteRunCheckpoint receives
only the explicitly documented version-import loader. Source comparisons retain
original blob/hash identities. Mixed build/consumer/CI commits were reconstructed:
keep discrete quickstarts/fixtures; omit point-mutation consumer checks and unrelated
study targets/manifests. This is selective tree integration, not blind cherry-picking.

The existing application signal flag/steady report schedule is shared in
PrimeRunMonitoring without changing scalar behavior. The D3 metadata flush/close
repair and measured sanitizer timeout are retained. The600second installed-consumer
allowance retains D1's measured qualification correction. Generic mathematical
loss, scalar/typed APIs, fitting, strict arithmetic and numerical kernels are not
rewritten.

## Evidence scope

All D1/D2/D3 selected evidence packets are copied byte-for-byte, including the
original D3 sanitizer12/13 plus successful focused unchanged-binary execution.
Their historical manifests include research-only tests and retain their original
source scope. They are not rewritten as release manifests. Other five research
packets remain unchanged on the original branches; they are deliberately outside
the0.4 source archive. These exclusions are release-scope decisions, not excuses
to drop portable contracts from the final candidate.

The release manifest derives from0.3 plus the discrete contracts and an explicit
version-compatibility fixture. Only the existing Jetson captured snapshot is a
platform-specific exclusion. The final required sanitizer gate is a complete
portable suite under full instrumentation, not a relabeling of D3's partial run.
No scientific campaign is rerun or tuned for this integration.

## Version and checkpoint integration

Version0.4.0 follows the pre-1.0 minor policy for an additive mathematical module
and documented configuration/layout changes. Genetik/Genetik::Genetik, source
compatibility target genetik_lib, include spelling, C++20, CMake3.16,
Threads::Threads and strict-FP propagation remain unchanged.

Native checkpoint format1 and codec IDs remain unchanged. Generic loading is
still exact-version by default. Explicit first-party forward-read policies accept
only0.3.0 with the scalar-candidate codec or the unchanged discrete-evolution/2
profile, checking checksum and the complete compiler/standard-library/libc/libm/
optimization/rounding fingerprint suffix. Arbitrary custom/research codecs are not
implicitly supported. Original producer version is exposed as import provenance;
new saves identify0.4.0. Data/grammar/budget/worker/context validation still follows.
Three frozen unmodified published0.3 checkpoint headers build a separate producer;
the0.4 consumer verifies full scalar continuation without mixing library versions
inside a translation unit. Native old files are never edited in place.

Source compatibility is not binary compatibility. Recompile all dependent TUs.
Historical pre-release research scripts can require their original build; use the
new explicit loaders for supported forward imports. No broad cross-version,
cross-toolchain or worker-count continuation promise is made.
