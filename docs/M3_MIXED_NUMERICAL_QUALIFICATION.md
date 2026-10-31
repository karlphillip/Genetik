# M3 mixed numerical qualification

## Result summary

The mixed expression implementation is verified as a bounded typed
`Int64 -> Int64` representation with real-valued operations, exact integer
fitness, typed variation, and deterministic checkpoint continuation. It is
not a prime discovery result. The five-seed first-1,000-prime campaign was not
run.

The frozen synthetic smoke used seed 37, population 12, five generations,
mutation rate 0.8, crossover rate 0.75, one elite, grammar depth 5, and the
default expression limits (1,023 nodes, depth 64, 1,000,000 operations per
evaluation, and real magnitude `1e100`). Training indices were 1–16 and
held-out indices were 17–24 for
`floor(1.15*n*ln(n) + 0.3*n + 2)`, with labels generated independently using
long-double reference evaluation. The held-out labels were not supplied to
candidate fitness, variation, or selection.

The selected smoke candidate was `iadd(x0, x0)`: 3/16 exact training matches
and 0/8 held-out matches. This is a negative stochastic discovery outcome. The
GA committed all five generations. Continuous and split-at-generation-2
execution produced equal committed candidates, fitness, population generation,
candidate attempt count, and MT19937 state after resume. Candidate codec
round-trip also reconstructed and revalidated fitness.

Fixed-seed grammar reachability generated 1,000 candidates and observed real
constants, Int64-to-Real64 conversions, logarithms, powers, integer modulo,
and typed conditionals. A separate 32-candidate variation exercise performed
fitness evaluation, mutation, crossover, and reevaluation. These are
reachability checks, not a claim that the stochastic grammar is efficient.

## Mathematical references

- `floor(2.3*n + 1.7) + (n mod 3)` was evaluated for every integer from -30
  through 30 against a separately computed exact rational reference, while
  also checking the explicit binary64 operation sequence. At `n=11`, exact
  decimal arithmetic gives 29 after the modulo term, while binary64 evaluates
  the intermediate just below 27 and returns 28. This single mismatch is
  deliberately retained as evidence of binary64 floor-boundary sensitivity.
- `floor(1.25*n*ln(n) - 0.4*n + 2)` was checked for `n=1..100` against a
  long-double reference; nonpositive logarithm input was rejected.
- `floor(1.2*n) + (n mod 3)` was checked for `n=-20..20` against exact
  rational floor division and Euclidean modulo.
- Conditional evaluation confirmed that an invalid logarithm in the unchosen
  branch is not evaluated; selecting that branch reports mathematical
  invalidity.
- Conversion checks covered negative floor/ceil/truncate/nearest, halfway-away
  ties, `-2^63`, rejected `+2^63`, values below the minimum, rounded
  `Int64 -> Real64`, nonfinite/domain invalidity, zero division, and configured
  real magnitude limits.

These finite references qualify implementation behavior only. They do not
establish exact real-number semantics or universal mathematical correctness.

## Engineering checks

The Release, Debug, ASan+UBSan, TSan, CMake 3.16, standalone-header,
multi-translation-unit, installed-consumer, and full-suite outcomes are listed
in `docs/m3-mixed-results/IMPLEMENTATION.json`. The run-continuation test is
deterministic in its recorded environment. The worker-overlap test synchronizes
four independent candidate evaluations against one immutable problem object;
TSan reported no race.

The generic evaluator reports node visits separately from operator work.
Mathematical invalidity and resource exhaustion have distinct statuses and
metrics. Input and output typing are validated at construction and candidate
replacement boundaries. No generic GA selection, mutation, crossover,
publication, or fitting algorithm was modified.

The complete Release suite ran on the final implementation. After that run, the
synthetic run-identity fixture added an explicit `coefficient-fitting=disabled`
identity field; the three mixed Release tests were rerun successfully after
that fixture-only change.

## Campaign proposal

The expression and candidate APIs are suitable foundations for an opt-in
direct-prime campaign, but this change does not contain the campaign CLI,
reference corpus adapter, frozen global objective budget, or campaign result
writer. The next proposal is to build a small application adapter over
`MixedIntegerVariation`, `MixedIntegerProblem`, and Genetik's existing GA and
run-checkpoint contracts; freeze its training-only reference labels, complexity
limits, and work ceiling; then review that concrete protocol before any
five-seed run. The prime campaign remains unlaunched and requires separate
authorization.
