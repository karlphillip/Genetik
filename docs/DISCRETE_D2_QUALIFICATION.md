# D2 — bounded discrete iteration qualification and discovery

D2 is locally implemented on `feature/discrete-iteration` from
`398c391e7aa3e187391c90747a71f4d382e7e3e0`. The public API and numerical/scope
contracts are in [DISCRETE_ITERATION.md](DISCRETE_ITERATION.md). This is not a
prime-discovery experiment, production release or hosted qualification.

## Frozen experiments: every outcome

The protocol/data/source/executable identities were committed before execution.
Both cases used seeds 0,1,2,3,42, sequential serial execution, population 64,
coordinate fitting disabled, at most 10,048 loss attempts/200 generation attempts,
25 nodes/depth 6, Add/Subtract/Multiply/Sum/Product, constants {0,1,2}, original
GA crossover .9/mutation .35 and two elites. Iteration bounds were terminal-only,
with two nested levels and the predeclared work/iteration policy. Only observations
and the case's independent reference sequence differ. No model was injected.

|Case|Seed|Initial→final loss|Discovery gen|Actual attempts|Assessment|Nodes/depth|Seconds|Stop|
|---|---|---|---|---|---|---|---|---|
|series|0|0→0|0|64|4/4|4/2|0.095881|exact_fit|
|series|1|7→0|43|2816|4/4|4/2|0.531716|exact_fit|
|series|2|0→0|0|64|4/4|4/2|0.093767|exact_fit|
|series|3|7→0|7|512|4/4|4/2|0.101422|exact_fit|
|series|42|7→0|2|192|4/4|6/3|0.095968|exact_fit|
|factorial|0|5→0|10|704|3/3|4/2|0.199218|exact_fit|
|factorial|1|5→0|8|576|3/3|4/2|0.101907|exact_fit|
|factorial|2|5→0|4|320|3/3|6/3|0.098141|exact_fit|
|factorial|3|5→0|4|320|3/3|4/2|0.097394|exact_fit|
|factorial|42|5→0|1|128|3/3|4/2|0.096485|exact_fit|

All ten models match their declared training and assessment rows exactly, with
zero integer error and no assessment invalidity. Series seeds 0 and 2 recovered
at generation zero; neither was discarded or restarted. All selected models
contain iteration, not just alternative non-iteration polynomials coinciding on
the finite training data. Their complete trees are retained below, including
redundant arithmetic and nonzero binder IDs. No uniqueness/global identity claim
follows from these finite samples. Product lower bounds of 2 legitimately omit
factor 1 for the nonnegative input domain; empty ranges cover n=0/1.

The ten runs consumed 5,696 objective attempts in total. Early exact success
stopped each run; unused allowances were not padded. Native checkpoints and a
selected canonical record were written before each run's assessment. Later cases
used the already committed configuration unchanged. No failure/retry/tuning
occurred in these ten scientific runs. Runtime includes initialization, candidate
preparation and checkpoint/selection writes, excludes assessment, and is descriptive
unlocked Jetson timing rather than an optimizer throughput comparison.

### series

|Seed|Faithful canonical model|
|---|---|
|0|`sum(k0, 0, x0, k0)`|
|1|`sum(k0, 1, x0, k0)`|
|2|`sum(k0, 0, x0, k0)`|
|3|`sum(k0, 0, x0, k0)`|
|42|`sum(k0, 0, x0, (k0 - 0))`|

### factorial

|Seed|Faithful canonical model|
|---|---|
|0|`product(k1, 2, x0, k1)`|
|1|`product(k0, 1, x0, k0)`|
|2|`product(k1, 2, x0, (0 + k1))`|
|3|`product(k0, 2, x0, k0)`|
|42|`product(k0, 1, x0, k0)`|

`RUNS.csv` and `analysis.json` retain full metrics, structural size/depth/constants,
variation requests/rejections/fallbacks, training rows, visits, primitive operations,
aggregate iterations and begun constructs. Per-run result files retain assessment
prediction vectors and separately measured work. Reporting never consumes training
loss attempts. Independent Python integer assessment verifies all retained vectors;
series n=9..12 yields 45, 55, 66, 78 and factorial n=7..9 yields 5040, 40320, 362880.

## Independent mathematical validation

Checked fixtures cover arithmetic-series and sum-of-squares closed forms at
n=0,1,2,3,8,12,100,10000, within explicitly sufficient work limits. Products verify
0!=1,1!=1,5!=120,20!=2432902008176640000 and checked overflow at 21. Nested sums
return 10; independent outer-body and diagonal fixtures return 24 and 3, detecting
confusion of inner/outer cursors. The divisor-count composition returns 4 for 6;
it is a representability fixture, not a primality predicate or discovery campaign.

Singleton sums at INT64_MIN/MAX remain exact, short near-MAX accumulation overflows
mathematically, and the full signed interval is rejected as ResourceExhausted
before entering a body. The near-maximum unary series bound 4294967295 is likewise
refused by its declared iteration cap, not misreported as arithmetic invalidity.
No billions-of-terms experiment was attempted. Empty ranges validate bounds, then
skip invalid bodies. The lazy modulo sum is 1; a zero product does not hide a later
invalid modulo. Per-operation/aggregate/nesting/node/operation/frame limits and
mid-loop cancellation leave no valid partial accumulator.

`independent-references.csv` uses Python exact integers/`math.factorial`; positive
divisor counts apply only to n>0. The production interpreter never generates its
own oracle. The campaign uses independent tiny native-integer references and is
cross-checked against the Python data committed before execution. Negative n has
the explicitly defined empty-range model semantics, not an asserted extension of
factorials or of the positive arithmetic-series polynomial.

## Scope, ownership, variation and continuation

Fixtures reject unbound/leaking/self-bound/duplicate IDs, wrong operand types and
open fragment execution. External inputs remain unchanged. Closed incoming loops
are alpha-renamed without capture; an inner subtree depending on an outer binder
cannot move into an unbound context. Complete generated/mutated/crossed expressions
are revalidated, and parents remain owned/unchanged. D1 operators are unchanged
unless Sum/Product are explicitly enabled.

Continuous generation 0→100 exactly matches generation 0→40/save/destroy/load→100
at 1, 2, 4 workers: entire committed population, integer nodes and binder identities,
fitness/selection state, archive, RNG, generation/request/training/variation counters.
An additional independent comparison reaches generation 100 with the same complete
state key at all three worker counts (1,212 loss attempts). This is nonbinding
same-environment reproducibility, not a promise across arbitrary runtimes or
binding-budget schedules. A resource-failed private generation preserves its
committed archive while retaining consumed work/RNG, and its checkpoint restores
that complete state. Grammar/iteration-policy mismatches reject before RNG/work;
corrupted payloads reject. No candidate is rescored during restore.

Actual D1 headers from the starting Git revision produced the retained legacy
candidate payload and baseline replay. Current source reproduces both byte-for-byte.
Old operator IDs 0–14/codec payloads remain unchanged; new IDs 15–17 are appended
and reject safely in old readers. Generic atomic persistence remains untouched;
the new complete-run profile has exact bounded configuration identity and native
format 1 envelope. The native discrete structures require consistent recompilation;
tight ABI-dependent workspace/storage allowances may need requalification.

## Local engineering qualification

The local environment is Linux AArch64/Jetson, GCC 13.3.0, GNU C++ headers 13, runtime
`libstdc++.so.6.0.33` and glibc/libm 2.39. Full commands, compiler/runtime output and actual compile/link
instrumentation are retained in the packet. Warnings are errors. Public strict-FP
requirements remain `-fno-fast-math -ffp-contract=off`.

| Gate | Actual result and scope |
|---|---|
| Release, recorded-reference configuration | Full 77/77; later strengthened D2 targeted rerun 3/3 |
| Debug, recorded-reference configuration | Full 77/77 including strengthened scope assertions |
| Exact CMake/CTest 3.16.0, Release | Full 77/77 including strengthened scope assertions |
| ASan+UBSan, affected discrete fixtures | Original 5/6, one timeout; unchanged diagnostic pass; repaired complete contract 1/1. All six selected contracts/quickstarts now pass, not a full sanitizer suite |
| ASan+UBSan standalone headers | All eight affected discrete headers compile with instrumentation and warnings-as-errors |
| TSan affected selections | `genetik_discrete_iteration_overlap`, `genetik_discrete_overlap`, `genetik_concurrency_contracts`, `genetik_oop_parallel`: each 3/3, 12 executions |
| TSan complete D2 contract | 1/1, including continuation at 1/2/4 workers |
| Clean portable configuration manifest | 76 actual names; checker pass |
| CI TSan manifest audit | 18 actual names; checker pass; execution scope above |
| Installed/relocated, multi-TU, generic OOP, standalone headers, strict FP, warnings, accepted replays | Included in the complete local suites; new installed discrete quickstart included |
| Workflow lint, hygiene, diff whitespace | Pass |
| Public quickstart with four workers | Pass; bounded search is demonstrative, exact stochastic recovery not asserted |

The first ASan selection passed five of six tests, but the new complete D2
continuation fixture exceeded its initial 180-second allowance. Checkpoints
written before the timeout showed forward progress. With no binary change and
no competing workload, one diagnostic execution completed in 307.6278 seconds,
exit zero, with all three worker cohorts intact. Its SHA256 was
`33b51332d4acbcbf312d95e83606e91d7020120755e4c42cce2c19912f6a73a8`.
Only this new complete fixture's ASan CTest timeout was changed to 900 seconds at
`e257423`; the ordinary and overlap allowances remain unchanged. CTest then passed
that exact unchanged binary in 306.65 seconds. Original timeout/LastTest logs,
progress/checkpoint identities, measured run and final CTest result are retained.
No sanitizer finding was suppressed; no generation, worker, row, mathematical
assertion, resource check or instrumentation was removed.

The recorded local manifest contains 77 named tests. The clean portable
configuration contains 76: only the existing Jetson-specific captured
`genetik_evaluation_reference` is excluded, not portable numerical or replay
contracts. Actual manifests are retained in full. The CI TSan manifest contains
18 selections and passes its name-by-name checker; local execution is the four
affected selections named in the table, not a claim that all 18 ran here.

The full suites include installed/relocated consumers, multi-TU, generic OOP,
strict-FP rejection/propagation, standalone headers, warnings/hygiene, accepted
scalar/typed/prime replays, previous parsing/template/calculus/checkpoint contracts
and D1. Installed consumers compile and run the new public quickstart and an
independent sum expression. The ASan build includes all eight affected standalone
discrete headers. No hosted or cross-platform qualification is inferred.

`gates.json` records the driver's launch source/protocol commit, not a blanket
per-binary guarantee while the test-only additions were made. Public implementation
source is 0c55ba603fc949b2ee8e10a9e1ebad6c2b4733fc; additional scope assertions
are 80ab378749d9c3c872a28f95b710cf7cb0c96d1c. The first Release full suite predates
those assertions; the final targeted Release rerun includes them. Subsequent
Debug/minimum/sanitizer/TSan builds include them. Original records and mapping are
retained rather than rewritten. Final documentation commits add no executable
source changes.

## Retained development issues

Warnings-as-errors caught misleading indentation, an early checkpoint-document
aggregate initializer and an invalid constexpr-before-class-completion use.
The application scratch build caught a mistaken persistence helper name and an
ADL collision with `std::quoted`; calls now use the actual existing
`saveCheckpointSeries` and an explicit JSON quoting helper. Original diagnostics
remain in the packet. No mathematical/resource assertion, sanitizer instrument,
accepted replay was weakened. The measured new-fixture timeout correction is
documented above. Other engineering repairs preceded the protocol freeze; no
scientific results drove them.

## Limits and completion boundary

Canonical parsing remains bounded (hard nesting 128); deeper supported trees use
node-codec reconstruction. Declarations are globally unique rather than shadowing;
crossover conservatively rejects open donor fragments. No fitting or global
integer-function optimizer exists. Complete-run identities are bounded 1 MiB and
require exact budget/settings/runtime/worker compatibility. Native physical
workspace/storage accounting includes new state and can differ from D1 for very
tight custom allowances; default D1 replay/codec compatibility is qualified.

Preservation checks cover all 127 frozen historical files, six prior research/D1
packets, recorded published assets, main and tags. No remote writes/release actions
occurred. Local qualification does not establish hosted, libc++, Windows/MSVC or
macOS/AppleClang support.

D2 ends here. Next is the consolidated **D3: Discrete Sequence Discovery and
Controlled Prime-Number Experiments**, then **D4: Hosted Qualification and
Development Release**. No D3 prime campaign, D5 mathematics or additional phase
was begun.
