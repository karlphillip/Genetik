# P0 — Research decision

## Decision: C — Stop further prime-formula search under the existing constraints

The first 1,000 primes are easy to compute using ordinary exact prime-enumeration algorithms, but the requested discovery excludes presenting a sieve, primality test, or prime-counting oracle as a newly discovered expression. The known exact formulas either conceal that operation (Willans/Wilson), fail indexed nth-prime semantics (Mills and set-valued Diophantine representations), or are not computationally constructive (high-dimensional existential witnesses and analytic explicit formulas). The existing generic expression and recurrence search has no demonstrated path to discover a compact exact prime-gap law; S15's bounded exact expression run failed on the prime task and exhausted its storage cap.

Therefore P0 does **not** authorize another prime search campaign or an implementation. It recommends stopping nth-prime formula search with the current grammar and constraints. This is a feasibility conclusion, not a proof that no compact formula exists.

## Three strongest representations and falsifiers

| Candidate | Why it ranks highest | Main obstacle | Smallest falsification test (not run) |
|---|---|---|---|
| Exact prime-counting inverse `min{x:pi(x)=n}` | Clean exact semantics; practical through 1,000 with a conventional sieve or optimized `pi` algorithm. | Every practical implementation reviewed derives `pi` through prime classification/counting. It violates the no-hidden-prime-enumeration discovery criterion. | Provenance audit a bounded implementation: if it classifies candidates or imports an equivalent prime-counting operation, reject it as a discovered mathematical rule. It remains a baseline only.
| Willans' indexed finite-sum formula | Proven exact formula for `p_n`, no fitted target constants. | `~5.36e601` nested summands at `n=1000`, factorial/precision burden, and formula operationally encodes Wilson primality checks. | Analyze the exact summand count for the frozen index (done); this already falsifies practical feasibility. No implementation is needed.
| Compact recurrence for prime gaps | Would directly meet indexed semantics and could be cheap if a true short transition exists. | No known bounded low-order recurrence with proof; finite-prefix exact fits are underidentified, and direct testing/enumeration to compute gaps is prohibited as the claimed rule. | Freeze a grammar and split a small prefix from later and alternate-start assessments; require exact autonomous continuation and complexity advantage over explicit enumeration. Historical S15 evidence already gives a negative signal, so this is not recommended without a new independent mathematical hypothesis.

Mills and the Jones–Sato–Wada–Wiens polynomial are important representability results but fail this objective's indexing or computational criteria more directly. A new generic capability should not be added merely to encode one of them.

## What could change the decision

Reopen the topic only if an independently justified mathematical construction supplies all three: (1) indexed semantics for `p_n`, (2) a cost bound for the first 1,000 outputs under explicit integer/precision semantics, and (3) a discoverability argument that does not reduce to prime lookup, primality testing, candidate enumeration or a hidden `pi` operator. A generic mathematical capability may be studied separately only when it has clear uses beyond primes and an independent correctness/resource case.

No S19 or other follow-on work is started by this decision.

## S18 closeout metadata correction

The verified `origin/main` SHA is `d4f55dde8378878a6bef151705a38deed35fdf02`. One S18 closeout qualification field instead contains `d4f55dde8378878a6bef142b8416e9ef6d984c0a`, which is a mistyped identifier. The S18 closeout document itself already gives the correct SHA. To preserve the historical checksummed package, the incorrect JSON was not edited; the exact correction and verification steps are in `Genetik-data/benchmarks/p0-prime-representation-feasibility/corrections/s18-main-sha-correction.json`.
