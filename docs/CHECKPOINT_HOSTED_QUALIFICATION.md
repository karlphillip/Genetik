# Genetik 0.3.0 hosted operational qualification

## Candidate and delivery scope

The first complete hosted matrix for the operational stack tests
**18e370e693e80d65bfd958ffea6942e7aea850e8**, seven commits after published
v0.2.0 **f9f300ed376370b2f476722e4a92888004d8358d**. This delivery adds exact-prefix
prime search, human/Unicode/LaTeX reporting, generic explicit candidate codecs,
format-1 durable committed-state checkpoints, cooperative cancellation and resume.
The generic OOP engine and mathematical specializations remain supported. There
are no new mathematical operators or evolutionary strategies in this delivery.

This report and release/support updates form a later commit. Its complete matrix
must pass before final qualification is declared. The exact final SHA, run/job links
and regenerated source-asset identities are recorded under ignored
`out/runs/checkpoint-preflight/hosted/COMPLETION.md`; no further tracked commit is
needed merely to record its own run. A passing earlier SHA is not relabeled as a
passing later tip. PR, main integration, tags and publication remain separate.

## Executed matrix and actual runtime

Workflow [39873935680](https://github.com/karlphillip/Genetik/actions/runs/39873935680)
passed for **18e370e693e80d65bfd958ffea6942e7aea850e8** on 2026-10-22 UTC.

| Job | Actual compiler / standard headers | CMake / CTest | Result |
|---|---|---|---|
| [GCC13 x64 minimum](https://github.com/karlphillip/Genetik/actions/runs/39873935680/job/120154799071) | GCC13.3.0, GNU13 headers, `__GLIBCXX__=20240904` | exact3.16.0, official checksum-verified bootstrap | Release, 61/61,81.28s |
| [Clang18 x64](https://github.com/karlphillip/Genetik/actions/runs/39873935680/job/120154798868) | Ubuntu Clang18.1.3, **GNU14 headers**, `__GLIBCXX__=20240908` |3.31.6| Debug ASan+UBSan,61/61,190.30s |
| [GCC13 ARM64](https://github.com/karlphillip/Genetik/actions/runs/39873935680/job/120154798737) | GCC13.3.0, GNU13 headers, `__GLIBCXX__=20240904` |3.31.6| Release,61/61,165.78s; separate Debug TSan |

All are Ubuntu24.04.5 LTS, kernel6.17.0-1022-azure. The x64 image is
`ubuntu24/20261012.334.1`, ARM64 `ubuntu24-arm64/20261012.149.1`.
GCC is Ubuntu13.3.0-6ubuntu2~24.04.1; glibc/libm2.39 comes from libc6
2.39-0ubuntu8.9. GNU runtime libstdc++6 and libtsan2 are14.2.0-4ubuntu2~24.04.1;
Python3.12.3. These versions are read from this run's environment logs, not assumed
from the previous qualification. Timings include correctness and package compilation,
not performance measurements.

[Selected retained evidence](checkpoint-hosted-results/) contains environments,
configured manifests, named result logs, package commands, compile/link excerpts,
run metadata and the artifact hash ledger. Full logs, compile/link commands and
artifacts remain in `out/runs/checkpoint-preflight/hosted/39873935680/`.

No hosted job failed. No production, test, workflow, tolerance or instrumentation
change was needed. The local post-download audit initially applied C++ strict-FP
requirements to the deliberately mixed-language consumer's C translation unit;
inspection corrected the audit to check C++ requirements on C++ files, retain no
C-only flag leakage and check no sanitizer leakage in either language. This was
an audit-scoping error, not a hosted test or package failure.

## Named TSan execution

The separate native ARM64 Debug TSan build uses `TSAN_OPTIONS=halt_on_error=1`.
Every configured selection below passed **three times**, with actual per-name
counts retained in [audit.json](checkpoint-hosted-results/audit.json):

- `genetik_checkpoint_overlap`: 3/3.
- `genetik_concurrency_contracts`: 3/3.
- `genetik_derivative_overlap`: 3/3.
- `genetik_derivative_regression_overlap`: 3/3.
- `genetik_integral_overlap`: 3/3.
- `genetik_oop_parallel`: 3/3.
- `genetik_parsing_overlap`: 3/3.
- `genetik_prime_exact_overlap`: 3/3.
- `genetik_scientific_contracts`: 3/3.
- `genetik_template_overlap`: 3/3.
- `genetik_typed_overlap`: 3/3.
- `genetik_typed_preparation_overlap`: 3/3.

That is36 successful selection executions. The complete
`genetik_integral_contracts` passed once more under TSan, covering transactional
split and failure cases beyond the overlap selection. No race report or sanitizer
suppression occurred. Runner CTest3.31.6 supports `--repeat until-fail:3`; the
minimum3.16 job does not use that newer option.

## Manifest, instrumentation and package review

The configured portable manifest is 61 distinct tests, verified by exact name against
`scripts/check_ci_manifest.py`. Local qualification has 62: the only exclusion is
`genetik_evaluation_reference`, the Jetson/GCC13/libm captured numerical snapshot.
Portable strict arithmetic, independent scientific references, typed mathematics,
parsing, templates, derivatives, weighted observations, quadrature, ownership,
resource/failure/publication, generic OOP and concurrency contracts remain enabled.
Same-environment replay and nonbinding worker-count equivalence are distinct from
cross-runtime captured-history claims.

Nine tests extend the previously hosted 52-test calculus suite:
`genetik_prime_exact_contracts`, `genetik_prime_exact_overlap`,
`genetik_prime_exact_cli`, `genetik_checkpoint_contracts`,
`genetik_checkpoint_overlap`, `genetik_checkpoint_process`,
`genetik_checkpoint_cli`, `genetik_checkpoint_quickstart` and
`genetik_formatting_contracts`.

Compilation uses C++20, warnings-as-errors, `-fno-fast-math -ffp-contract=off`.
ASan+UBSan and the separate TSan configuration have verified instrumentation at both
compile and link. Installed ordinary/relocated consumers receive public C++20,
strict-FP and threading requirements without private sanitizer flags. Package
coverage includes source-free relocation, multiple translation units, generic
nonmathematical OOP, all compiled quickstarts, version matching and source embedding.
Exact CMake 3.16.0 uses supported commands; modern runner CTest alone uses repetition.

## Checkpoint and reporting scope

The hosted checkpoint contracts compare generation 0→100 against 0→40/save/destroy/
restore→100, including full population/coefficients/fitness, archive, RNG, histories
and work/generation counters under nonbinding qualified worker settings. A private
cancelled generation is discarded, while consumed RNG and work are retained in the
checkpoint. Formatting, reporting and precision changes do not change training.

Atomic storage uses a same-directory temporary file, checked writes, file fsync,
checksum read-back, rename and directory fsync. Newly created entries sync through
parents. Retention follows successful numbered/latest replacement. Fault fixtures
reload the old valid checkpoint after pre-rename and failed-replacement errors.
After-rename fsync errors report uncertain durability rather than inventing rollback.
The checksum detects accidental corruption, not malicious changes. Custom candidates
and derived services need explicit codecs; arbitrary C++ objects are not serialized.

[Local operational evidence](CHECKPOINT_PREFLIGHT_QUALIFICATION.md) remains unchanged,
including actual injected ENOSPC, failed replacement, syscall audit and process-level
SIGKILL/restart. SIGKILL wrote no final save; durable generation 3 resumed in a fresh
process and published generations 4 and 5 before cooperative SIGTERM. These are local
observations, not invented hosted power-loss tests. Crash recovery may repeat work
since the last durable save; clean continuation and crash recovery differ.

## Limitations and delivery boundary

Qualification is limited to the executed Linux/GNU-runtime combinations. Clang with
GNU headers/runtime does not qualify libc++. No Windows/MSVC or macOS/AppleClang
qualification is claimed. Checkpoint continuation requires the documented matching
code/configuration/runtime/FP and worker count. Filesystem/hardware fsync behavior
qualifies durability; no hardware power-loss guarantee was tested. Mathematical,
search and coordinate-fitting limitations from v0.2.0 remain unchanged.

All 127 frozen evidence files, prior campaign records, published 0.1.0/0.2.0 assets,
main and release tags are preserved. No discovery campaign was rerun or tuned.
Genetik 0.3.0 remains unreleased. After exact-final-tip qualification and extracted
source verification, the prepared development-release proposal is ready for review,
not automatic publication.
