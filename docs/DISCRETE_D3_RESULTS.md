# D3 — discrete-prime discovery: complete local results

D3 uses the completed D2 baseline `50d88668baa22ce3d9e8ed2d0203a31fa11b356d`
on `feature/discrete-prime-discovery`. The application/reference/fixture source is
`f21920f`; protocol/data/executable identities were committed at `1daa8a6` before
all ten sequential runs. Public contracts, commands, scope/resource conventions
and mathematical identities are in [DISCRETE_PRIME_DISCOVERY.md](DISCRETE_PRIME_DISCOVERY.md).

## Every evolutionary outcome

All ten runs completed normally at their training-loss budget boundary. There
were no restarts, retuning, process failures, resource-abort stops or injected
reference expressions. Each primality run consumed20,000 objectives and committed
624generations; each direct n-th run consumed4,992 and committed155generations.
The latter retains8 unspent attempts because another complete generation needs32.
Both use four workers, population32, no fitting and the frozen D1/D2 grammar/policy.
There is no zero-loss recovery in either task. Initialization results remain
reported: primality initial loss18 (44/62 correct), direct initial loss999 (1/1000).

### primality

|Seed|Train matches|Loss|Training prefix|RMSE|MAE|Max error|Held-out|Larger range|First best generation/attempts|Seconds|
|---|---|---|---|---|---|---|---|---|---|---|
|0|48/62|14|7|0.475191|0.225806|1|132/192|503/769|71/2304|0.953343|
|1|56/62|6|0|0.311086|0.096774|1|164/192|631/769|426/13664|0.906210|
|2|54/62|8|0|0.359211|0.129032|1|146/192|585/769|459/14720|1.174023|
|3|48/62|14|7|0.475191|0.225806|1|132/192|503/769|94/3040|0.816952|
|42|48/62|14|7|0.475191|0.225806|1|132/192|503/769|274/8800|1.130621|

### nth-prime

|Seed|Train matches|Loss|Training prefix|RMSE|MAE|Max error|Held-out|Larger range|First best generation/attempts|Seconds|
|---|---|---|---|---|---|---|---|---|---|---|
|0|9/1000|991|0|926.910350|705.327000|1930|0/200|not applicable|77/2496|15.177442|
|1|20/1000|980|0|972.454491|751.111000|1990|0/200|not applicable|108/3488|6.570613|
|2|9/1000|991|1|923.932645|702.710000|1926|0/200|not applicable|51/1664|2.243723|
|3|8/1000|992|0|1508.854131|1208.433000|2947|0/200|not applicable|55/1792|13.142125|
|42|5/1000|995|0|2070.499207|1693.913000|3932|0/200|not applicable|149/4800|2.656723|

The primality training prefix starts at integer2, not at prime index1. Held-out
classification is64..255; the larger range is256..1024. Direct n-th training is
indices1..1000 and assessment1001..1200. All selected models evaluate successfully
on all declared assessment rows; no invalid/resource row was silently dropped.
Direct n-th prefix is0 except seed2, whose prefix is1. None matches a later prime.

### Actual selected trees

The canonical and Unicode forms below are actual archive candidates, preserved
without algebraic simplification. Product/sum declarations are genuine executable
nodes, even where their bodies happen not to use the iterator.

**primality seed0** — 8nodes, depth4, 3constants

```text
(if((4 > x0), -1, x0) % 2)
((if (4 > x0) then -1 else x0) mod 2)
```

**primality seed1** — 9nodes, depth4, 2constants

```text
((x0 * x0) % ((x0 % 2) + 2))
((x0 · x0) mod ((x0 mod 2) + 2))
```

**primality seed2** — 18nodes, depth6, 8constants

```text
(if((2 != (product(k1, 4, 8, 8) % x0)), (product(k0, 4, 8, 8) % x0), x0) % 2)
((if (2 ≠ (Π[k1=4..8] 8 mod x0)) then (Π[k0=4..8] 8 mod x0) else x0) mod 2)
```

**primality seed3** — 8nodes, depth4, 3constants

```text
(if((4 <= x0), x0, -1) % 2)
((if (4 ≤ x0) then x0 else -1) mod 2)
```

**primality seed42** — 6nodes, depth3, 2constants

```text
product(k0, 4, x0, (x0 % 2))
Π[k0=4..x0] (x0 mod 2)
```

**nth-prime seed0** — 10nodes, depth5, 3constants

```text
(((sum(k1, 4, x0, 4) + x0) + 1) + x0)
(((Σ[k1=4..x0] 4 + x0) + 1) + x0)
```

**nth-prime seed1** — 36nodes, depth7, 12constants

```text
((((x0 + neg(sum(k1, 0, 2, 16))) + x0) + -2) + ((-1 - (-2 + x0)) + (((-2 + x0) + ((-2 + x0) + -2)) + ((-2 + sum(k0, 8, x0, 2)) + x0))))
((((x0 + −(Σ[k1=0..2] 16)) + x0) + -2) + ((-1 - (-2 + x0)) + (((-2 + x0) + ((-2 + x0) + -2)) + ((-2 + Σ[k0=8..x0] 2) + x0))))
```

**nth-prime seed2** — 13nodes, depth6, 3constants

```text
((x0 + ((4 * (-2 % x0)) + x0)) + (1 % x0))
((x0 + ((4 · (-2 mod x0)) + x0)) + (1 mod x0))
```

**nth-prime seed3** — 6nodes, depth3, 2constants

```text
(x0 + sum(k0, 8, x0, 4))
(x0 + Σ[k0=8..x0] 4)
```

**nth-prime seed42** — 12nodes, depth6, 5constants

```text
(1 + sum(k0, -2, -1, (((1 + x0) + x0) - 8)))
(1 + Σ[k0=-2..-1] (((1 + x0) + x0) - 8))
```

## What the classification results mean

The independently generated class balance is44nonprimes/18primes in training,
156/36 held out, and651/118 in the larger range. Constant-zero accuracy is thus
44/62,156/192 and651/769. All five learned candidates improve training accuracy;
only seed1 improves on constant zero on held-out points (164/192). None improves
on it in the larger range (best631/769). Accuracy must be read with this imbalance,
not treated as evidence of complete primality discovery. Full false-positive,
missed-prime and nonbinary-output counts are in `analysis.json`; every prediction,
target/status/work is independently audited in the per-run records.

The best training classifier is `n*n mod((n mod2)+2)`. Its modular structure
rejects multiples of2/3 but is not a primality predicate. This interpretation
explains its finite-sample errors; it was not supplied to initialization. Selected
models are frozen by training mismatch count and established complexity ties,
never by these later assessments. No learned-predicate enumeration was implemented
or presented as a direct n-th-prime formula.

## Computational cost and feasibility

|Task/seed|Objectives|Row attempts|Node visits|Integer operations|Iterations|Invalid objectives|Peak RSS KiB|
|---|---|---|---|---|---|---|---|
|primality/0|20000|1156515|12014858|7257836|1272838|1410|15104|
|primality/1|20000|1103127|10235972|6196989|1106747|2282|15104|
|primality/2|20000|1128809|22430702|18888428|5824652|1998|15104|
|primality/3|20000|1142633|12419278|7758887|1439599|1628|15104|
|primality/42|20000|1116508|42465505|35888698|10211825|2365|15104|
|nth-prime/0|4992|4858011|1474263985|2449112398|1136116409|136|15104|
|nth-prime/1|4992|4845020|531685823|866128274|402883563|149|15104|
|nth-prime/2|4992|4700219|105906241|94407313|28657504|293|15104|
|nth-prime/3|4992|4832108|1293465632|1886762654|813978206|162|15104|
|nth-prime/42|4992|4672654|108803339|127035794|49427626|321|15104|

Full per-run diagnostics additionally record iteration constructs, invalid rows,
reporting and assessment evaluations, canonical trees, work, actual stopping
reasons and effective runtime/configuration. Mathematical invalidity is ordinary
invalid candidate fitness; exhausted computation aborts private preparation rather
than assigning a finite penalty. There were zero resource-abort outcomes in the
ten campaigns, while focused fixtures deliberately exercise resource exhaustion.
Rows may stop at the first mathematical failure during training; selected-model
reporting separately evaluates every declared row. These costs are not comparable
to an objective that evaluates a three-node arithmetic expression.

The engineering stress candidate has48nodes and runs a balanced23-leaf body for
every term of `sum(k0,-2,x0,body)` across all1,000 training rows. It consumed
22,660,500visits,12,084,000operations and503,500iterations in0.687933seconds on the
unlocked Jetson. The terminal-bound/one-level/49node grammar has a finite
conservative cost within the declared50million-visit/operation per-loss allowance:
`49*sum(n+3)=24,671,500` visits/operations, and at most12disjoint loops imply
`12*sum(n+3)=6,042,000` iterations for n=1..1000. This bound is structural and
does not depend on observed recovery.
This is a grammar-specific engineering bound, not permission for arbitrary nested
D2 trees. Timing is descriptive; no CPU-frequency or thermal normalization was
performed. Scientific runs were sequential and did not overlap compilation/tests.
Peak RSS is process high-water usage, not an exact allocation/workspace measurement.

## Representability and independent mathematical validation

Python trial division and an independent sieve produce all1,200 labels, with the
historical corpus cross-check and frozen TSV hash recorded in the API document.
C++ independent sieve fixtures verify divisor counts/primality over-8..256 and
prime counts over-4..100. Negative/zero divisor-count inputs use the documented
empty-range value0; the guarded predicate returns0 below2. These are separately
constructed mathematical fixtures, never evolutionary initialization data.

The expanded bounded n-th identity at B=30 exactly returns the first ten primes.
It costs5,395 iterations per query including all three nested levels. At B=7919
the same expansion would cost82,830,198,001 iterations per query, over82trillion
per1,000-query pass, before additional arithmetic. That experiment was refused
by design rather than attempted with relaxed limits. The direct search grammar
restricts nesting to one and therefore excludes this particular expensive
reference construction; full D1/D2 representability is broader than the frozen
search subspace. No bounded-selection or prime-specific operator was added.

## Reproducibility and checkpoint safety

Both task types exactly match uninterrupted versus3/save/destroy/load/8 evolution
at1/2/4 workers. The compared checkpoint state includes complete populations,
coefficient/binder identities, fitness, archive/best-generation state, RNG,
generation/request/variation and mathematical-work counters. Fixtures reject
cross-task/problem identity, including a changed context with identical rows,
before modifying caller RNG or starting workers. TSan forced overlap shares only
immutable expressions/problems; objectives and codec scratch remain private.

The optional `DiscreteSearchConfig::contextIdentity` appends a tagged identity
field only when nonempty. Candidate IDs/native format1/D2 empty-context bytes
remain unchanged. The current reader restored all ten historical D2 native
checkpoints and independently verified650cached population/archive entries,
without adding training attempts. The D3 audit likewise verified330cached entries
and168,328diagnostic rows, and checked every selected training/assessment vector
against independent integer labels. RNG/work/full payload remained unchanged.

Reporting and periodic/final checkpointing occur only with the executor idle.
SIGINT/SIGTERM request an orderly stop after the bounded active generation drains;
the handler only sets the shared lock-free flag. SIGKILL cannot cause a final save.
JSONL/reporting work is separate from training, and exact configured total budgets
and workers are required for continuation. The new task identity prevents a
classifier checkpoint from becoming n-th regression. Recompile dependent TUs
consistently after the optional configuration field is added.

## Local qualification and retained repairs

The actual recorded local suite contains80tests. The clean portable configuration
contains79named tests; its only exclusion is `genetik_evaluation_reference`, the
existing Jetson-specific captured snapshot. Both manifests and the exact name-set
difference are retained. The CI TSan manifest contains19names; only the affected
local subset below was executed here. No hosted qualification is claimed.

| Gate | Actual execution and scope |
|---|---|
| Release | Original79/80 exposes metadata-write regression; repaired full80/80,393.25s |
| Debug | Complete80/80,466.87s |
| Exact CMake/CTest3.16.0 Release | Complete80/80,283.95s |
| ASan+UBSan affected selection | Original12/13, one600.11s timeout; unchanged complete fixture subsequently passes direct. All13fixtures successful across recorded executions; no fresh whole-selection pass claimed |
| ASan timeout metadata | Reconfigured3600s for the complete D3 fixture only; exact successful binary SHA unchanged |
| TSan repeated subset | Seven selections, each3/3 (21successful executions), halt_on_error=1 |
| TSan complete D3/D2 contracts | Each1/1,7.97s and5.04s respectively |
| Standalone headers, installed/relocated, multi-TU, generic OOP, strict FP, warnings, accepted replays and existing continuous/D1/D2 contracts | Included in all three complete ordinary suites |
| Portable/CI TSan manifest checks | Pass by actual names (79/19), not guessed counts |
| Workflow lint, workspace hygiene, diff whitespace | Pass |

The repeated TSan names are `genetik_discrete_prime_overlap`,
`genetik_discrete_iteration_overlap`, `genetik_discrete_overlap`,
`genetik_prime_exact_overlap`, `genetik_checkpoint_overlap`,
`genetik_concurrency_contracts`, and `genetik_oop_parallel`.
`tsan-repetitions.json` verifies every name, not just a total.

Actual environment: Linux6.8.12-1021-tegra/AArch64 Jetson, GCC13.3.0, GNU C++
headers13/runtime `libstdc++.so.6.0.33`, glibc/libm2.39, CMake4.4.4 and exact3.16.0.
Compilation/link records verify full ASan+UBSan/TSan instrumentation and public
`-fno-fast-math -ffp-contract=off`. The research executable's process resource
instrumentation is POSIX/Linux-oriented. libc++, Windows/MSVC and macOS/AppleClang
are not qualified by these executions.

The first build caught misleading indentation under warnings-as-errors. The
initial overlap fixture incorrectly tried to prepare a depth8 guarded reference
under the depth7 search policy: its declared fixture problem now uses the same
explicitly deeper fixture limits as the independent mathematical test. Search
limits were not increased. Repeated CLI tests originally reused a protected
output directory; they now allocate distinct retained execution directories.
Original diagnostics and outputs remain under `out/` and in the selected packet.

A `/dev/full` metadata-write probe found buffered stream failure was not checked
on flush/close. The application could incorrectly continue after losing its
configuration record. The focused `b5bff33` repair checks both operations before
search; the regression fixture requires failure and no checkpoint/search startup.
This changes no objective, grammar, candidate proposal, successful-run data or
checkpoint format. Scientific results retain their original frozen executable
identity and are not rerun. Original failure and corrected evidence are preserved.
The driver launch revision is not a blanket per-binary SHA; source mapping and
full-versus-targeted scope are recorded explicitly below.

The full repaired Release, Debug and minimum-CMake suites use the final C++
implementation at `b5bff3370eb6bf4982ae4c5c8a56892c8b4717c9`. Scientific runs retain
`f21920f` and their committed protocol. The original driver's launch revision
`1daa8a6` is not a blanket per-binary claim: its first Release run used the earlier
compiled application with the newly added write-failure regression and passed
79/80. Its observed phase/pause record explicitly corrects the initial assumption
that it had not started testing. The app was rebuilt, the failure probe verified,
and the complete Release suite rerun successfully before Debug/minimum.

The complete D3 ASan fixture timed out at600.11seconds while CPU-active with no
sanitizer finding. Original logs, binary SHA and phase observations are retained.
One unchanged-binary diagnostic completed successfully in1484.531seconds wall
clock, including25.044 and82.032seconds of recorded diagnostic pauses. This is
1377.455seconds excluding those pauses, not a precision throughput measurement.
An attach/child debugger was unavailable; a separate progress-only copy advanced
normally through divisor checks to n=128 in its60second sample. That copy is not
counted as qualification. No OS ptrace policy or sanitizer options were changed.

Only this new complete fixture's ASan CTest allowance became3600seconds at
`566f8a5`; its ordinary600second allowance, CLI allowance, all assertions, intervals,
continuation cohorts, numerical/iteration budgets and instrumentation remain intact.
The successful unchanged execution is the focused rerun: the other12already-passing
ASan tests were not repeated, and no whole13-test CTest pass is claimed. The final
metadata is checked to select3600 with the exact successful binary SHA unchanged.
Subsequent documentation/evidence commits change no executable behavior.

Final staged whitespace validation caught CRLF produced by Python CSV defaults.
The summary generator now emits LF explicitly; every parsed CSV row is verified
unchanged. Original check diagnostics and both integrity hashes are retained.
This changes neither scientific results nor executable source.

## Conclusions and limits

Median direct recovery is9/1000 (range5..20), with median training RMSE972.454491.
The selected first-1,000 models produce no exact later-prime matches.
The highest-match model does not have the lowest RMSE: seed1 reaches20matches
with RMSE972.454491, while seed2 reaches9 with RMSE923.932645. This is consistent
with exact mismatch count being authoritative, not continuous residual error. Historical
continuous searches often recovered50..74, but had much larger attempt budgets,
a different language/objective and different per-attempt costs. This is not a
controlled representation-superiority comparison. Exact integers alone do not
make the discovery search successful or cheap.

The structural audit finds no iterator-reference node in any of the ten selected
models. Iteration constructs are used only as repeated constants/counts or repeated
functions of the external input, not to learn a varying divisor test. That is a
measured property of the selected endpoints, not proof that the whole population
collapsed or that no intermediate useful subtree existed. Full D1/D2 can express
the reference classifier/count/identity, but its nested n-th construction is
prohibitively costly and outside the declared search subspace. The evidence does
not isolate one universal failure cause or prove that the grammar is insufficient.

A concrete future research recommendation is to measure discovery/retention of
iterator-dependent divisor-test subexpressions, using matched integer-operation
budgets and independent simple discrete tasks, before another large direct-prime
campaign. This recommendation does not add a search strategy/operator here or
schedule another implementation phase. First complete the agreed D4 combined
D1/D2/D3 hosted qualification, development-release preparation and delivery review.

Finite-prefix agreement would not establish a universal formula; these partial
fits plainly do not. No default grammar, operator, fitter, genetic probability,
main, tag, release asset, historical campaign or frozen evidence is promoted or
changed. D3 ends here, with D4 next and separately approved publication required.
