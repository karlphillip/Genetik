# Discrete mathematics D1 — local qualification

Starting point was clean `e2204f95447a3898244de11793132dab56061f30` on
`experiment/prime-local-variation`. Work is local on `feature/discrete-expressions`.
No remote actions, versions, main or tags changed.

## Source and evidence scope

- `bab46ac29305486a14e6099b78e1cad393fcddff`: additive language, evaluator,
  parsing/formatting, typed variation, exact-mismatch objective, shared-engine
  adapter, candidate codec, quickstart, independent fixtures and consumers.
- `dc9dd619be57c0f11e6cf004bfaf1a67f050189f`: transactional copy assignment,
  moved-from access guards and noncopyable objective. Found by ownership review.
- `6022260de577fa8d086f766c01bf6ec08e752921`: storage checks before subtree
  replacement/codec allocation. Found by resource review.
- `a20a63909cba7ddfcaef799e57149e835a8928fe`: deep node-codec fixture and precise
  parser-ceiling documentation, without changing evaluator mathematics.
- `b994f80dc2a8ad2924bfb7d2254b072bb740525c`: measured installed-fixture timeout
  correction, with all tests/instrumentation retained.

The first gate driver began at bab46ac and its `source_sha` records that launch
revision, not an automatic per-binary guarantee while review continued. Original
records are preserved. Its first Release suite passed 74/74 before the hardening
rebuild. Its Debug full suite passed 73/74; the sole failure was package timeout.
The continuation driver starts at b994f80 and rebuilds/tests the final source.
Final report/evidence commits contain no executable-source changes.

## Retained failures and corrective work

The scratch build caught an inferred-return helper declared after use; an explicit
return type repaired it (original tool transcript). Warnings-as-errors then caught
size_t-to-worker-count narrowing in fixtures; the original diagnostic is retained.
An added cancellation fixture initially requested stop on an idle executor and
expected persistence into the next batch. Existing executors reset stop on batch
entry; the fixture now cancels active preparation and verifies unchanged committed
state. These were fixture/API-integration corrections, not numerical relaxations.

Review hardened value assignment and bounded allocation as listed above. The
arithmetic kernels, operation set, finite grammar strategy and objective remained
unchanged. No accepted historical assertion or numerical expectation was weakened.

The original installed fixture allowance was 300 seconds. The expanded D1 fixture
measured 290.93s in Release and timed out at 300.13s in Debug during its final
C++23 consumer build. Ordinary/relocated consumer steps and their execution had
completed. The original commands, timeout output, test manifest and retained
executable index are archived. Completing the retained C++23 build and executing
its consumer succeeded. A clean fixture rerun uses 600s, approximately twice the
observed work, with all producer/removal/relocation/quickstart/multi-TU/generic-OOP/
C++23/negative-package checks intact. No sanitizer suppression or instrumentation
change was introduced; no unexplained failure was rerun until green.

The first evidence-packaging helper hit a Path/string precedence TypeError.
Adding filename parentheses repaired the ignored helper; the failure record is
retained. No C++ source, test result or mathematical value changed.

## Numerical contract qualification

4,000 frozen Python exact-integer references cover a 144-pair boundary grid and
256 independently generated full-width pairs (seed 1729), ten binary operations
per pair. The oracle uses Python integers and explicitly rejects nonpositive
modulo divisors and out-of-Int64 arithmetic results. Production code does not
create its own oracle. Additional fixtures cover Negate, lazy invalid branches,
Bool conditional results, resource boundaries, ownership, original variable
indices, node/depth/grammar restrictions, variation and full-objective invalidity.

A 201-depth direct expression intentionally exceeds the parser nesting ceiling;
its node codec reconstructs structure and accepted fitness exactly. Parser round
trips are claimed only within supported textual limits. Floating-point terminal
construction is an expected compile rejection; oversized unsigned literals are
runtime API rejections. No integer operand passes through binary64.

Forced four-worker overlap includes independent parse/evaluation work and private
candidate preparation sharing immutable data. A failing worker retains its original
bad_alloc cause, drains admitted work and preserves the committed archive and
generation. Nonbinding search results/RNG/work match at 1/2/4 workers. Cancellation,
resource and global-loss failures retain distinct causes and consumed work.

## Results

| Gate | Actual result and scope |
|---|---|
| Final Release | Full **74/74**, final source b994f80, 370.55 seconds |
| Debug | Original **73/74**, sole package timeout; rebuilt final source and **4/4** affected checks passed, 297.80 seconds |
| Exact CMake 3.16.0 | Full **74/74**, final source, 282.04 seconds |
| Final ASan+UBSan | **3/3** D1 contracts/overlap/quickstart, 58.19 seconds; all seven new standalone headers rebuilt; no findings |
| Separate TSan | `genetik_discrete_overlap`, `genetik_concurrency_contracts`, `genetik_oop_parallel`, **three executions each**; complete `genetik_discrete_contracts` once; no findings |
| Portable manifest | Actual **73 names**, checked against the explicit expected set; only captured snapshot excluded |
| Consumers and build contracts | Installed/relocated, multi-TU, generic OOP, C++23 when available, standalone headers, transitive Threads/strict FP, negative FP/configuration and accepted replays passed within full suites |
| Diagnostics and hygiene | Compiler warnings treated as errors; workflow lint and hygiene/diff checks passed; expected floating-point-factory compile rejection verified |
| Bounded four-worker workflow | 504 loss attempts, 20 committed generations, 4 mismatches/9 observations, selected `(x0 * x0)`; generation-limit stop, no target injected |

No mathematical recovery is required by the smoke test. It demonstrates evaluation,
round trips, diagnostic rejection and real shared-engine execution. The example's
hand-supplied conditional evaluates −3→−8, 0→0, 2→4 and 3→10. Its selected learned
model is reported faithfully without tuning or retry.

Local environment: Jetson AArch64, GCC 13.3.0, GNU standard-library headers release
13 (`__GLIBCXX__=20240904`), libstdc++.so.6.0.33, glibc/libm 2.39, Linux
6.8.12-1021-tegra. Exact compiler/runtime paths and hashes, CMake versions and
compile/link instrumentation are recorded in the packet. Both sanitizer compile
and link flags and `-fno-fast-math -ffp-contract=off` were checked explicitly.


ASan+UBSan scope is the rebuilt D1 contracts, overlap, quickstart and standalone
new headers; this is not a claim that the complete legacy 74-test suite ran under
ASan in D1. Unchanged prior baseline evidence is retained. Full Release/Debug/
minimum manifests and final selected sanitizer/TSan manifests are in the evidence
packet. The portable manifest excludes only `genetik_evaluation_reference`, the
existing Jetson-specific captured snapshot. No portable numerical, package,
publication or concurrency contract is removed.

## Compatibility and limitations

The existing generic/scalar/typed headers, continuous/prime objectives, default
RNG paths, fitting settings and native checkpoint envelope/old codecs are unchanged.
All new mathematics is opt-in through separately includable headers. Installation
uses Genetik::Genetik, C++20, CMake >=3.16 and existing Threads/strict-FP propagation.
No new dependency or configure-time download exists. There is no binary-compatibility
promise for C++ header-defined types; recompile consumers normally.

D1 has Int64 inputs and regression targets, computed/literal Bool subtrees, one
exact mismatch objective, no fitting, no full discrete CLI resume and no large
prime-discovery campaign. It does not include integer division, factorial, GCD,
primality predicates or summations. New hosted, libc++, Windows/MSVC or
macOS/AppleClang qualification is not claimed.

Preservation checks cover 127 frozen files, all locally recorded published-asset
identities, five previous research packets, main and tags. No prior campaign
outcome was replaced or rerun.
