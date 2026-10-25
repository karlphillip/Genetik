# D3: discrete sequence discovery and prime research

This application builds on D1/D2 without adding mathematical operators, fitters,
mutation strategies or prime oracles. Ordinary continuous and discrete defaults
remain unchanged. Generic GA execution/publication remains representation-independent.
D3 is local research, not a release or hosted support qualification.

## Three separate tasks

A primality classifier returns Int64 0/1 from n; other integers are mismatches,
not silently coerced booleans. Prime counting returns the number of primes <=n.
Direct n-th-prime regression binds x0 to the original integer index1..1000 and
predicts Int64 p_n without rounding/scaling. A classifier is not a direct n-th-prime
formula. No optional external enumeration is included in this study.

The application-only trial-division reference generates1,200 primes. Exact Python
trial division independently generates `tests/discrete-primes.tsv` and the frozen
training/assessment labels; a sieve independently cross-checks them. C++ fixtures
also compare all1,200 entries against the historical continuous-demo corpus.
The TSV SHA256 is `203afe12c00daab34efc15b8e8b546d392a3ace93c16b9543a1594ff1c127658`.
These references build observations and assess selected models; no candidate node
can call them or inspect target labels while predicting. Indices1001..1200 have
already appeared in historical research; this is a disclosed, fixed assessment,
not a claim of a previously untouched validation set.

## Representability, separately from discovery

D1/D2 can represent the following directly constructed fixtures:

```
D(n): sum(k0, 1, x0, if((x0 % k0) == 0, 1, 0))
P(n): if(x0 < 2, 0, if(D(n) == 2, 1, 0))
pi(n): sum(k0, 2, x0, P(k0))
p_n: 1 + sum(k0, 1, B, if(pi(k0) < x0, 1, 0))
```

`D`/`P`/`pi` above are explanatory abbreviations, not parser functions. The test
contains fully expanded ordinary expressions with distinct binder identities.
D(n)=0 for n<=0 follows the empty-range convention; it is not a claim about an
infinite set of divisors of zero. P(n)=0 for n<2 follows the lazy guard. pi(n)=0
below2 follows its empty range. Modulo still requires a positive divisor.

The n-th identity requires n>0 and B>=p_n: pi(m)<n exactly when m<p_n, so the
sum has p_n-1 terms. B=30 verifies the first ten primes, at5,395 total iterations
per query. It does not recover a general n-th-prime result for arbitrary n at
fixedB. For this exact expansion the cost is
`B + B*(B-1)/2 + B*(B+1)*(B+2)/6 - B` iterations per query, including all three
levels. At B=7919 that is82,830,198,001 iterations, or82,830,198,001,000 for a
naive1,000-query pass. Work and arithmetic operations are additional. This exceeds
the declared resource policy; that large experiment was not attempted.

Independent fixtures cover n=-8..256 for D/P, n=-4..100 for pi, all first ten
n-th references, checked modulo/overflow, lexical nesting, exact resource refusal,
mid-loop cancellation and serialization. Fixture limits permit three nested
iterations and deeper trees; they are explicitly distinct from search limits.
No fixture expression is supplied to search initialization.

## Application

```sh
cmake -S . -B out/build/discrete-d3-release -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/discrete-d3-release -j2
out/build/discrete-d3-release/genetik_discrete_prime_search \
  --task primality --seed 42 --threads 4 --loss-budget 20000 \
  --report-seconds 30 --checkpoint out/runs/discrete-prime/p42/checkpoints \
  --output out/runs/discrete-prime/p42
out/build/discrete-d3-release/genetik_discrete_prime_search \
  --task nth-prime --seed 42 --threads 4 --loss-budget 5000 \
  --output out/runs/discrete-prime/n42
```

`--help` lists options. `--checkpoint` names a run-owned directory; numbered
committed files plus `latest.gtkc` use existing atomic durable replacement and
retain three recoverable generations. Default checkpoint interval300seconds;
`--checkpoint-seconds` changes only output scheduling. A final save always occurs
on ordinary stops. `--resume FILE` requires identical task, seed, workers, total
budget, generation limit and mathematical policy. Report/output paths and intervals
may change. Existing results reject accidental overwriting without explicit resume.

The existing application signal flag/steady absolute-deadline schedule is shared
through `PrimeRunMonitoring.hpp`; historical scalar behavior is unchanged. SIGINT
and SIGTERM only set a lock-free atomic flag. The discrete coordinator observes it
at the next idle generation boundary after bounded current work drains, saves and
exits130. It does not asynchronously interrupt an admitted generation or serialize
inside a handler. SIGKILL cannot trigger a save. A failed private generation is
never published, but consumed work/post-attempt RNG remain in its committed-state
checkpoint. Operational/I/O errors remain errors; resource failures are not penalties.

The research executable currently uses POSIX signal/resource instrumentation and
is locally qualified on Linux; this adds no platform dependency to installed
Genetik mathematical/generic headers.

Initial, timed and final progress prints show committed generation, objective
attempts, exact matches, loss, complexity, actual canonical/Unicode model, elapsed
session time and work. JSONL version1 and `result.json` preserve raw diagnostics.
Reporting uses a separate bounded all-row evaluator, never candidate caches/fitting
or training admission, and asserts unchanged training counters/RNG. Failed rows
have separate invalid/resource/cancelled counts; MAE/RMSE are null if incomplete.
Full-width absolute differences use unsigned arithmetic; finite long-double
statistics are diagnostics, not the exact mismatch-count training objective.

Selected canonical expression/native checkpoint are persisted before held-out
assessment. Primality trains2..63, assesses64..255 and then256..1024. N-th search
trains1..1000 and assesses1001..1200. Session attainment fields on resume refer to
the resumed invocation; the native archive/population/counters remain fully restored.
No assessment influences training, selection, stopping or parameter selection.

## Frozen search policy and feasibility

Both cases use population32, two elites, existing crossover .9/mutation .35,
maximum1,000 generation attempts, initialization depth4/mutation depth3, eight
bounded variation proposals,49 nodes/depth7 and no constant fitting. Seeds are
0,1,2,3,42; workers4. Primality has a maximum20,000 objective attempts per seed;
n-th has5,000. Full-generation admission can leave a remainder below32 unspent.
Exact training mismatch count0 stops successfully; all other stops remain distinct.

The grammar enables all12 D1 operations plus Sum/Product, Bool constants false/true,
Int64 constants{-2,-1,0,1,2,4,8,16}, and original variable x0. Modulo divisors are
general Int64 expressions, not a fixed prime terminal list. Iteration bounds are
terminals; search nesting is restricted to one. This deliberately bounded subset
can express divisor tests but excludes nested prime-count/n-th reference trees.
It is not evidence that the complete D1/D2 language lacks those representations.
The direct guarded P fixture needs greater depth than the search limit; the
unguarded D(n)==2 form has the same nonpositive-input results through empty ranges.

Per observation:100,000 node visits/operations,1MiB frames,2,048 iterations per
operation,32,768 aggregate iterations, runtime depth2. Per objective:50million
visits/operations and20million iterations. Bounds are terminals in[-2,16] or x0;
for training and assessment x0<=1200 the longest nonempty range has1,203 terms.
One-level nesting and49nodes bound body visits by49*1203 plus small setup costs;
iteration-control overhead also fits100,000. For1..1000, the conservative loss
bounds fit the configured allowances: with at most12 disjoint loops, iterations
are at most `12*sum(n+3)=6,042,000`, and node visits/operations are each bounded
by `49*sum(n+3)=24,671,500` over n=1..1000. Loop setup/advance/accumulation
are covered because each loop consumes three structural nodes plus its body.
The retained48-node balanced-body stress
candidate runs all1,000 rows before the campaign. Arbitrary deeper D1/D2 programs
are still resource-exhaustible; they are not silently skipped or invalidated.
Actual costs, rather than objective counts alone, are reported.

## Checkpoint/API compatibility

`DiscreteSearchConfig::contextIdentity` is an optional immutable application label.
The complete discrete-run identity appends a tagged field only when nonempty.
D3 fixes task, exact-mismatch objective version, reference policy and integer binding
semantics there. D2 already stores complete ordered data, grammar, mathematical
limits, evolution settings, codec, seed, workers and runtime. Identity checks reject
cross-task resume, even for deliberately identical rows. Existing empty-context
D2 identities/candidate codec IDs/native format1 bytes are unchanged. Recompile
translation units consistently after the configuration struct gains its field.

Fixtures compare full checkpoint population/archive/fitness/RNG/work/counters for
continuous versus save/destroy/load continuation of both tasks at1/2/4 workers.
Forced-overlap tests share immutable expressions/problems and use private objective
and codec state. Reporting checks no work/RNG changes. D1/D2 default trajectories,
continuous search behavior and the generic engine remain supported.

Results, failed development checks, exact manifests and local support scope are
recorded in [DISCRETE_D3_RESULTS.md](DISCRETE_D3_RESULTS.md). Finite-prefix agreement
would not establish a universal prime formula. D4 combined hosted qualification,
development-release preparation and delivery review is next; no remote action or
additional mathematical implementation phase is part of D3.
