# Discrete mathematics hosted qualification — 0.4.0 candidate

The audited clean D1–D3 release candidate `c7ffaee0ef9763f789464d4863fca86db3df4ea5` passed the complete matrix
[40210704207](https://github.com/karlphillip/Genetik/actions/runs/40210704207).
All test manifests, per-name results, compile/link flags, runtime/package versions
and installed-consumer evidence were reviewed, not inferred from green badges.
Selected immutable evidence is in [discrete-d4-results](discrete-d4-results/README.md).
Full local artifacts/logs are retained under out/runs/discrete-d4/hosted.

This report commit must receive the same full matrix. Its actual final SHA/run/job
links, source-asset hashes and completion checks are recorded only in the ignored
hosted/COMPLETION.json after success, avoiding another untested documentation tip.
The initial candidate results below do not alone qualify a later report tip.

## Measured platforms

| Job | Architecture/compiler | CMake | Headers/runtime | Results |
|---|---|---|---|---|
| [clang18-x64-sanitizers](https://github.com/karlphillip/Genetik/actions/runs/40210704207/job/121221102199) | x86_64, Clang18.1.3 (Ubuntu1ubuntu1) | 3.31.6 | GNU14, __GLIBCXX__20240908; GNU runtime14.2.0 | 71/71, ASan+UBSan |
| [gcc13-arm64](https://github.com/karlphillip/Genetik/actions/runs/40210704207/job/121221102472) | AArch64, GCC13.3.0 (Ubuntu13.3.0-6ubuntu2~24.04.1) | 3.31.6 | GNU13, __GLIBCXX__20240904; GNU runtime14.2.0 | 71/71; separate TSan45+5 complete |
| [gcc13-x64-minimum](https://github.com/karlphillip/Genetik/actions/runs/40210704207/job/121221102474) | x86_64, GCC13.3.0 (Ubuntu13.3.0-6ubuntu2~24.04.1) | 3.16.0 exactly | GNU13, __GLIBCXX__20240904; GNU runtime14.2.0 | 71/71 Release |

All runners: Ubuntu24.04.5 LTS, kernel6.17.0-1022-azure, glibc/libm from
libc6 2.39-0ubuntu8.9. GNU runtime/libtsan packages14.2.0-4ubuntu2~24.04.1.
X64 imageubuntu24/20261023.334.3; ARM imageubuntu24-arm64/20261023.149.3.
C++20, -fno-fast-math, -ffp-contract=off and warnings-as-errors were verified.
Clang compilation/linking includes -fsanitize=address,undefined; separate ARM
Debug TSan includes -fsanitize=thread (never combined). Runtime linkage and actual
consumer compile commands are retained. Clang uses GNU headers/runtime, not libc++.
Windows/MSVC, macOS/AppleClang and libc++ remain unqualified. This is bounded
Linux/POSIX qualification, not universal portability or binary ABI compatibility.

## Exact manifests

Each actual portable manifest has71tests. The sole platform exclusion is
`genetik_evaluation_reference`, the captured Jetson/GCC/libm snapshot. Portable
arithmetic, domain/resource, same-build replay and concurrency tests are retained.
The historical D3 count80 included unrelated research features; those are excluded
by the clean integration audit, not weakened portable coverage. Full names are in
[portable-manifest.txt](discrete-d4-results/portable-manifest.txt).

Separate TSan selected exactly the following15names. Each ran three successful
executions using supported --repeat until-fail:3, halt_on_error=1:

- `genetik_checkpoint_overlap`
- `genetik_concurrency_contracts`
- `genetik_derivative_overlap`
- `genetik_derivative_regression_overlap`
- `genetik_discrete_iteration_overlap`
- `genetik_discrete_overlap`
- `genetik_discrete_prime_overlap`
- `genetik_integral_overlap`
- `genetik_oop_parallel`
- `genetik_parsing_overlap`
- `genetik_prime_exact_overlap`
- `genetik_scientific_contracts`
- `genetik_template_overlap`
- `genetik_typed_overlap`
- `genetik_typed_preparation_overlap`

Additionally genetik_integral_contracts, genetik_discrete_contracts,
genetik_discrete_iteration_contracts, genetik_discrete_prime_contracts and
genetik_checkpoint_compatibility each ran once under TSan. Per-name counts are
recorded in tsan-repetitions.json. No unexplained retries or sanitizer suppressions.

## Mathematical and compatibility findings

Complete D1 verifies checked Int64/Bool, Euclidean positive-divisor modulo, lazy
conditionals, type/ownership/parser/node-codec invariants and typed variation.
D2 verifies independent series/squares/factorial references (20! exact,21!overflow),
empty identities after both bounds, nested scopes/capture restrictions, strict
zero-product invalidity, full-width interval admission, iteration/work/cancellation,
legacy D1 payload bytes and40-save-destroy-resume100 complete-state equivalence
at1/2/4workers. D3 verifies1200references/historical corpus/independent sieve,
divisor/primality and prime-count relationships, first10bounded nth identity,
resource/cancellation and both-task continuation/cross-task identity rejection.

Separate frozen unmodified0.3 producer headers compile in a separate process,
proving whole-state scalar forward continuation under each matching environment.
Original two-argument loader signatures/default strictness remain. Explicit first-party
forward readers check version/codec/checksum and exact runtime fingerprint suffix,
then complete problem/config identity. Format1/node IDs unchanged. Generic/custom
profiles are not implicitly migrated. Local actual published scalar CLI checkpoint
and all20retained D2/D3 native files were imported read-only;980cached entries
independently checked, no training attempts added and RNG/work unchanged.
Recompile dependent TUs consistently; source compatibility does not imply ABI.

The original ten D3 campaigns were not rerun or tuned. Best direct recovery remains
20/1000training primes; none of the five selected direct models matched any of200
later primes. Classifiers did not consistently exceed the trivial composite-only
baseline. Selected models had no iterator-reference nodes (some still had iteration
operators whose bodies did not reference the binder). Representability is distinct
from discovery. Naive B=7919 nested identity costs82,830,198,001iterations/query
under the documented expansion. Finite observations do not prove universal formulas.
Int64 overflow, positive-divisor domains, parser nesting, conservative capture-safe
variation, finite work/storage and cooperative cancellation remain explicit limits.

## Local evidence and retained repairs

At39499178ce7206775ee1311e730ad7f4809340a0, clean tracked-source Release/Debug/
exactCMake3.16 suites each passed72/72 including native snapshot; full ASan+UBSan
passed71/71 in3220.79s, complete D3 fixture1377.84s. Separate15x3TSan plus5complete,
headers/OOP/install/relocate/multiTU/strictFP/warnings/hygiene passed. Actual20native
imports checked980cached entries. The historical D3 ASan12/13timeout plus unchanged
focused success remains its original sequence, never relabeled as a full suite.

Initial30minute-cap run40210685228 was deliberately cancelled as superseded,
before qualification, by workflow repairc7ffaee. The90minute cap accounts for
measured53minute local full ASan cost plus compilation. Per-test900/3600allowances,
workloads/assertions and instrumentation unchanged. Additional discrete/compatibility
runtime linkage artifacts retained. No mathematical/source/GA repair was needed.
Initial HTTPS attempts failed due absent credential helper then OAuth workflow-scope
restriction; existing SSH key succeeded without authentication reset or credential
exposure. No PR/main/tag/release action was performed.

## Release and preservation

Clean release scope is D1–D3 and required infrastructure from published0.3.0;
all67original research commits/branches remain intact. Experimental objectives,
optimizers and mixed mutation were not imported. Generic engine, published scalar
variation/fitter/numerical headers/defaults are unchanged. All127frozen files,
three discrete evidence packet trees and five independent packets on original
branches, main/tags and published0.1/0.2/0.3asset identities remain unchanged.

PackageGenetik/Genetik::Genetik, source alias/include spelling, C++20/CMake3.16,
Threads::Threads, strictFP and SameMinorVersion remain. Explicit0.3requests reject
0.4; see [migration](releases/0.4.0/MIGRATION.md). Source assets must be regenerated
from the exact final report commit, all blobs compared, extracted without.git and
built/installed/relocated with representative OOP/discrete consumers. Final hashes
and verification records accompany the exact final-tip hosted run in ignored output.

Hosted upload/focused repairs/report commits are authorized. PR creation, main
integration, annotatedv0.4.0 and publication require separate final delivery approval.
No additional mathematical features or D5–D7 work is begun.
