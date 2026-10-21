# Hosted qualification: expression parsing and one-site scalar templates

## Candidate and authorized scope

Workflow [39662713630](https://github.com/karlphillip/Genetik/actions/runs/39662713630)
passed all three jobs for **c9080e2a0d1d3a00f10319379375284d6dd2bd28** on
2026-10-21. This qualifies the accumulated Phase 5 scalar/typed expression import
and Phase 6A one-site scalar template features, preserving the existing engine,
strict interpreter, fitting, execution and publication contracts.

Four commits were uploaded, after the already hosted-qualified/public reference
`ae2f819b78b544eb34d146f9e8155aef283e2b60`:

- `898571a22bde1438c30030b09b5b5fb83f13baef`: bounded scalar/typed import and formula-file workflow.
- `d612446ae5f2fb5e1ebd91165b6e01e304b8cab5`: parser local qualification/evidence.
- `cdd2553cab1000d709ebaa44cac6ea9ddc740881`: one-site templates, restrictions, full-model search and selected fitting.
- `c9080e2a0d1d3a00f10319379375284d6dd2bd28`: template local qualification and campaign evidence.

The previous public feature ancestry remains unchanged. Upload and subsequent
qualification/report work are confined to `qualification/parser-templates`.
No PR, default-branch change, tag, version change or release action is involved.
The package version and published v0.1.0 remain unchanged.

This report/support update follows that implementation run and receives its own
complete matrix. The exact final report SHA and its workflow/job links are recorded
in ignored `out/runs/hosted-parser-templates/COMPLETION.md` and the completion response,
without another tracked bookkeeping commit that would leave an untested tip.
Links and environments below explicitly describe the implementation run above.

## Actual executed jobs and runtime environment

| Job | Compiler / standard headers | CMake + CTest | Result |
|---|---|---|---|
|[GCC13 x64 minimum](https://github.com/karlphillip/Genetik/actions/runs/39662713630/job/119439628980)|GCC/G++13.3.0, Ubuntu13.3.0-6ubuntu2~24.04.1; libstdc++13, `__GLIBCXX__=20240904`|exact3.16.0, checksum-verified official source bootstrap|Release/Werror, **41/41**,64.47s|
|[Clang18 x64 sanitizers](https://github.com/karlphillip/Genetik/actions/runs/39662713630/job/119439629005)|Ubuntu Clang18.1.3 (1ubuntu1); libstdc++14, `__GLIBCXX__=20240908`|3.31.6|Debug/Werror/ASan+UBSan, **41/41**,120.48s|
|[GCC13 native ARM64](https://github.com/karlphillip/Genetik/actions/runs/39662713630/job/119439628861)|GCC/G++13.3.0, Ubuntu13.3.0-6ubuntu2~24.04.1; libstdc++13, `__GLIBCXX__=20240904`|3.31.6|Release/Werror, **41/41**,106.21s; separate Debug TSan **7 selections ×3**|

Suite times include correctness/package checks, not performance benchmarks. All
three environment logs begin with the identical full tested SHA. Both x64 jobs
use Ubuntu24.04.5 LTS image `ubuntu24/20261012.334.1`; ARM64 uses
`ubuntu24-arm64/20261012.149.1`. Actual kernels are6.17.0-1022-azure on x86_64 and
aarch64. These were read from this run, not copied from previous qualification.

All three report Python3.12.3, glibc/libm2.39 (`libc6=2.39-0ubuntu8.9`),
libstdc++6 and libtsan2 `14.2.0-4ubuntu2~24.04.1` for the corresponding architecture.
Clang uses GNU standard-library headers/runtime, **not libc++**. See each retained
[environment log](parser-templates-hosted-results/clang18-x64-sanitizers/environment.log).

Downloaded compilation commands confirm C++20, `-fno-fast-math -ffp-contract=off`
and warnings-as-errors on qualification sources. Clang adds
`-fsanitize=address,undefined -fno-omit-frame-pointer`. Separate ARM64 TSan uses
`-fsanitize=thread`, never ASan simultaneously. Raw passed-test output was also
examined for sanitizer diagnostics, including potentially nonfatal UBSan messages;
none were found. No sanitizer suppression or instrumentation exclusion was added.
Default rounding, gradual underflow, nontrapping arithmetic and qualified native
libm remain required. These results do not establish cross-platform bitwise libm
or stochastic search equivalence.

## Actual manifests and exclusion

Every downloaded job manifest matches the exact set in
[portable-tests.txt](parser-templates-hosted-results/portable-tests.txt), with
**41 unique names**, and every name has one successful execution in its raw contract
log. This is derived from actual configured manifests, not inferred from a total.
All33 previously hosted portable tests remain. The additions are:

| Area | Added test names |
|---|---|
|Parsing/import|`genetik_parsing_contracts`, `genetik_parsing_overlap`, `genetik_parsing_quickstart`, `genetik_expression_file`|
|Scalar templates|`genetik_template_contracts`, `genetik_template_overlap`, `genetik_templates_quickstart`, `genetik_template_campaign_smoke`|

The local42 configuration adds only `genetik_evaluation_reference`. Hosted jobs
explicitly set `GENETIK_RECORDED_REFERENCE=OFF`: that Jetson/GCC13/libm captured
snapshot remains an environment-specific local reference. Portable arithmetic,
scientific references, ownership, resource, fitting, budget, publication, OOP and
concurrency contracts are not excluded.

Within-build deterministic repetition, nonbinding serial/parallel equivalence and
portable mathematical/API expectations remain active. A host need not reproduce
the Jetson's discovered expressions, RNG trajectory from a different standard-library
implementation, or captured runtime-specific libm result. Independent scientific
reference fixtures/tolerances remain unchanged. Long discovery campaigns are not
CI gates and were not repeated or retuned.

## Parsing and template boundaries exercised

Parser fixtures compare directly constructed scalar and typed expressions with
imports: exact structure, coefficient bits (including signed zero, subnormals and
extreme finite values), variable/schema identity, shape/rank, independent constant
occurrences, Powi metadata, canonical re-export, evaluation status/results and work.
Locale-independent conversion and complete tokens, malformed corpus/generated
round trips, bounded nesting/tokens/payload/workspace, allocation failures and owned
results remain tested under instrumentation. Supported parser limits still bound
the round-trip promise; this does not add a general model archive or unlimited parser.
The formula-file workflow checks ordered column mapping, bounded reads, explicit
training-only fitting and complete invalid-prediction reporting without evolution.

Template fixtures preserve original variable index1, reject forbidden masked terms
and operators, preserve frozen structure/coefficient bits, check complete versus
local composition limits and context-specific fitness, and fit only inserted
constant occurrences. Full-model scoring uses original rows, never divided targets.
All three hosted environments observed the independent weighted fixture:

```text
weighted_constant=2.5999999940395355 full_model_mse=1.6000000000000001 loss_calls=71
```

The independent optimum is c=2.6, from minimizing
`((c-1)^2+(2*c-6)^2)/2`; fitting divided observations would incorrectly give c=2.
Original coefficient/loss tolerances are unchanged. The separate affine coefficient
fixture, invalid probes after accepted progress, template/problem identity, owned
clones, fitness invalidation, failed preparation, budget exhaustion, cancellation,
draining and frozen committed state remain covered. Candidate-local workers share
immutable scaffolds/problem data and independent mutable scratch/fitting state.
The generic engine has no template/parser-specific interface or control flow.

## Exact separate TSan executions

The ARM64 job uses runner CTest3.31.6, `TSAN_OPTIONS=halt_on_error=1` and
`--repeat until-fail:3`. That repeat option is not used in the exact3.16 minimum job.
Every name below was verified independently in the actual manifest and execution log:

| Selection | Successful repetitions |
|---|---:|
|`genetik_concurrency_contracts`|3|
|`genetik_oop_parallel`|3|
|`genetik_typed_overlap`|3|
|`genetik_typed_preparation_overlap`|3|
|`genetik_scientific_contracts`|3|
|`genetik_parsing_overlap`|3|
|`genetik_template_overlap`|3|

This is21 actual successful executions,1.36s total, not merely a total that could
hide missing selections. See [TSan log](parser-templates-hosted-results/gcc13-arm64/tsan-contracts.log)
and [audit counts/commands](parser-templates-hosted-results/audit.json).
Full ThreadSanitizer/strict-FP compilation was checked for all seven fixture families.
Parser overlap is forced inside import; template overlap includes fitting with
private accepted progress followed by a sibling failure or cancellation. Existing
backend/OOP, typed evaluator/preparation and scientific worker cases remain enabled.
This does not claim that all41 portable tests run under TSan, or qualify TSan on
x64/Clang or other unexecuted environments.

## Installed delivery and generic isolation

Each job builds the new standalone public headers and runs the package fixture:
ordinary installed use, relocation to prefix B with spaces, deleted disposable
producer sources/builds, fresh consumers, multi-TU overlapping headers, versions/
components and source-parent inclusion. Parsed/typed and template APIs execute in
installed quickstarts and consumers; parallel template search is included. The
external nonmathematical permutation consumer builds/runs without expression,
parser, template, typed or regression headers in its compiler dependency closure.
Scalar/symbolic/typed consumers and character/file workflows remain enabled.

Actual installed-consumer metadata confirms `Genetik::Genetik`, relative installed
include roots, C++20/strict-FP requirements, transitive Threads::Threads and no
producer paths or development sanitizer/Werror/runtime leakage. The unrelated C
translation unit receives no C++ FP options; the unrelated source-parent target
retains its own settings. C++23 consumption is an additional runner-CMake variant;
exact3.16 checks mandatory C++20, not a newer standard setting that CMake3.16 predates.

## Preflight, failures and retained evidence

Preflight verified the exact four-commit range, unused destination, working auth/SSH,
branch triggers, full delta and manifests before public-upload approval. A clean
Git archive contained480 matching tracked files and no .git/untracked dependencies.
Clean-source Release/Werror built the affected apps/tests/standalone headers;
10/10 selected tests passed, including isolated package/relocation/OOP consumers.
Seven local TSan cases also passed three times each. Workflow lint, hygiene and
historical/publication identities passed. Existing42/42 Release/Debug/ASan/exact3.16,
serial replay and typed compatibility evidence was reused proportionately.

There were **no hosted implementation, portability, numerical, sanitizer, workflow
or service failures** in this implementation matrix. Existing SSH credentials
completed the approved ordinary push on the first attempt. No code, test, workflow,
timeout, tolerance, selection, instrumentation or numerical-reference repair was
needed. No unexplained rerun, campaign adjustment or recovery-driven tuning occurred.
The only subsequent changes are this qualification/support documentation and
selected evidence; the report tip receives its own complete matrix.

Selected durable evidence: [parser-templates-hosted-results/](parser-templates-hosted-results/run.json),
including run/job metadata, environment and full per-test outputs, portable/TSan
logs, exact names, audit assertions and checksums. Complete downloaded artifacts,
compilation commands, package metadata/commands and workflow logs remain under
`out/runs/hosted-parser-templates/hosted/39662713630/`; hosted artifacts retain14 days.
`DOWNLOADED_SHA256.json` identifies the complete local download; `RECORD_SHA256.json`
covers the selected tracked record. Selected .log copies trim trailing ASCII spaces/
tabs per line for repository whitespace hygiene; original downloaded bytes remain
unchanged locally and their hashes remain in DOWNLOADED_SHA256.json. Final report-commit evidence is retained separately
under the same ignored run hierarchy, without a self-referential extra tracked commit.

All127 frozen historical files and all249 previously hosted evidence/reproduction
files checked against the base remain byte-identical (overlapping counts, not a
sum). Campaigns and published assets are unchanged, including unsuccessful scientific
oscillator seed1. The source archive SHA256 remains
`7f632489e45abb97385d98c9c2f703664386e358b22f3f73baa8a72c99b6b992`;
SHA256SUMS remains `a12d576eedc38b80af11f118d82d086a992f3d8930b89bda743b91191663353b`.
Main remains `f9299ea2cb5136d06e2de8f68bb83ecf460ca369`; annotated tag object
`986316039749aedcf20d1c1537bb4c91ded06151` is unchanged.

## Supported scope and next work

These selected Linux GCC13/x64+ARM64 and Clang18/libstdc++14/x64 configurations
qualify the recorded parser/template candidate. Windows/MSVC, macOS/AppleClang,
libc++ and other combinations remain unqualified. Captured snapshots remain locally
scoped. Heuristic discovery/sample recovery is not proved by passing engineering
contracts; prior campaign outcomes are frozen. The roughly fourfold fixed-work
prime-refinement gain remains the recorded Jetson reused-four-worker-pool measurement
in CONCURRENCY_RESULTS.md, not a new hosted performance claim.

No new mathematical feature, mandatory dependency, version or release is introduced.
Further template capabilities, general constraints, adapters and later roadmap phases
remain separately scoped. This checkpoint establishes hosted parsing/template
qualification, not a new release or broader platform promise.
