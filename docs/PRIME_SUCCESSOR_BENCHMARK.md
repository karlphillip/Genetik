# Prime successor benchmark

This is a deterministic, application-only benchmark for a frozen expression that
predicts the immediate successor of an input prime. It is a benchmark harness; it
does not launch search, train a synthesizer, alter Genetik's engines, or reopen the
closed synthesis research. The prior mathematical-discovery architecture review
remains **NO-GO for additional synthesis research**. Its decision and evidence are
preserved separately.

## Frozen data and splits

Inputs are every prime `p` in `[1,000, 50,000)`. The exact output is the immediate
next prime; the alternative target is `successor - p`. The checked-in CSV has 4,965
rows. An Eratosthenes sieve creates the reference; a separately implemented trial
division routine verifies that the two prime sequences agree through 50,021 and
that each recorded gap contains no prime. The final row is `49,999, 50,021, 22`;
the successor is intentionally outside the input interval.

| Partition | Input range | Rows |
|---|---:|---:|
| Training | `[1,000, 30,000)` | 3,077 |
| Validation | `[30,000, 40,000)` | 958 |
| Test | `[40,000, 50,000)` | 930 |

Dataset identity is SHA-256
`b9d6565dc78ca52d4bf9c2a25860d934f8395fa338ba8d848cd1031e5e181018`, recorded
in `benchmarks/prime_successor/prime_successor_1000_50000.manifest.json`. Regenerate
with `python3 benchmarks/prime_successor/benchmark.py --generate`; it must reproduce
the committed CSV bytes and hash. The manifest is intentionally small and the data
is versioned with the evaluator.

## Tracks and scoring

**Predictive formula** accepts one compact expression in `p`, numeric constants,
arithmetic (`+ - * / // % **`), unary signs, and `log(x)`. It has a 32-AST-node
limit. Calls, names other than `p`/`log`, branches, loops, lookup tables, primality
oracles, and next-prime scans are rejected. Output conversion uses nearest integer,
ties away from zero. Rounded outputs outside `[0, 100,000]` are invalid, which
keeps independent trial-division validity checks bounded while covering this
dataset and the optional extrapolation interval. Expression complexity is the
number of arithmetic, unary, and log nodes. Per-query mathematical work is the
number of evaluated operator nodes. An arithmetic-domain failure receives the
full expression node count as a conservative work charge.

**Algorithmic formula** is a separate track for bounded iteration/divisibility
logic. The expression `bounded_trial_scan(max_gap=N,max_divisor=N)` checks each
successive integer up to the gap bound and trial-divides it up to the divisor
bound. Limits are explicit (`max_gap` 1–512; `max_divisor` 2–10,000). The report
charges each inspected candidate plus each tested divisor. `exact_next_prime` is
also available on this track as a computational reference, not a discovered
formula; it charges tested divisors. Do not compare their capabilities to the
predictive track as equivalent restrictions, or compare their work counts across
these algorithmic implementations as if they represented the same operations.

Each partition reports exact successor accuracy, exact gap accuracy, gap MAE,
prime-output validity (verified by trial division), elapsed query runtime, and
mathematical work. The evaluator also reports a recursive successor-generation
diagnostic: starting at the first data prime, it repeatedly feeds the prior output
back as input and counts the correct prefix. Scoring/verifier work is excluded from
candidate runtime/work. Runtime is informational and machine/interpreter dependent;
operator counts are the comparable work measure within a track.

The constant-gap baseline is the training mean gap rounded to nearest integer with
ties upward (`9` here). The evaluator freezes that value from training targets only.
The logarithmic baseline is `p + max(1, round(log(p)))`, where `log` is natural log.
The exact next-prime method is reported beside the formula baselines only as a
computational reference.

Run the deterministic baseline report:

```sh
python3 benchmarks/prime_successor/benchmark.py --output /tmp/prime-successor.json
```

Evaluate a frozen predictive expression (the report includes its SHA-256):

```sh
python3 benchmarks/prime_successor/benchmark.py \
  --track predictive --candidate 'p + 6' \
  --output /tmp/prime-successor-frozen.json

python3 benchmarks/prime_successor/benchmark.py \
  --track algorithmic --candidate 'bounded_trial_scan(max_gap=128,max_divisor=250)' \
  --output /tmp/prime-successor-algorithmic.json
```

The report includes both the expression SHA-256 and a candidate identity SHA-256
over the protocol version, track, exact expression text, and evaluation bounds/
rounding configuration. The evaluator
measures a supplied expression; it does not pick expressions or
configurations. Freeze candidate text, complexity limits, rounding, and any
selection/configuration choices before requesting a report. Training metrics may
be used for fitting. Validation is for predeclared configuration selection. Only
after these are frozen should final test metrics be inspected; test metrics must
not feed candidate construction, configuration selection, or fitting. The baseline
report's test columns are fixed reference results, not tuning recommendations.

## Baseline reference run

One local run used Python 3.12.3. Runtime is shown as approximate microseconds per
query and varies with host load; work is exact under the counters above. Exact
successor accuracy and prime-output validity are shown for train/validation/test.

| Predictor | Exact accuracy T/V/Test | Gap MAE T/V/Test | Valid prime outputs T/V/Test | Work/query |
|---|---|---|---|---:|
| `p + 2` | 14.04% / 12.94% / 12.26% | 7.425 / 8.436 / 8.766 | 14.04% / 12.94% / 12.26% | 1 |
| `p + 6` | 22.16% / 20.04% / 19.35% | 5.104 / 5.944 / 6.228 | 28.50% / 25.37% / 24.19% | 1 |
| Train-fitted constant gap `9` | 0% / 0% / 0% | 5.270 / 5.800 / 6.002 | 0% / 0% / 0% | 1 |
| `p + round(log(p))` | 5.75% / 6.26% / 0% | 5.258 / 5.966 / 6.256 | 10.95% / 10.65% / 0% | 2 |
| Exact next-prime computational reference | 100% / 100% / 100% | 0 / 0 / 0 | 100% / 100% / 100% | 86.41 / 134.86 / 152.06 |

The formula baseline timings in that run were roughly 2.3 microseconds/query for
the arithmetic expressions and 3.3 microseconds/query for the log expression; all
use the same AST evaluator as frozen predictive candidates. The exact algorithm
was roughly 8.7, 13.2, and 14.8 microseconds/query for the three splits. These are
Python evaluator timings, not a performance claim about compiled code. The exact
reference work increases with prime magnitude because trial division checks more
divisors.

As a separate algorithmic-track smoke case, the frozen bounded scan
`bounded_trial_scan(max_gap=128,max_divisor=250)` scored 100% exact successors and
100% prime validity on all three partitions. It used about 178, 275, and 310 counted
candidate/divisor operations per query on train/validation/test. This only checks
that the bounded evaluator can represent an exact algorithm on this finite table;
it is not a predictive-formula result or a new discovery claim.

## Optional future extrapolation

An optional extrapolation assessment may later score frozen candidates on inputs
`[50,000, 100,000)`. That range is not part of this dataset, current development,
configuration selection, or reported baseline run. If authorized, freeze candidate
and configuration hashes first, generate the new successor table independently
with the same sieve plus trial-division cross-check, save its SHA-256, and run it as
a separately labeled post-freeze assessment. Do not feed those results back into
the candidate or configuration that was assessed.

## Validation

`tests/test_prime_successor_benchmark.py` checks deterministic byte identity,
partition counts, the outside-range successor, independent primality/completeness,
train-only constant fitting, exact algorithm correctness, rejection of oracle,
branch, and iteration constructs in the predictive language, plus bounded
algorithmic work and bound validation. It is registered as
`genetik_prime_successor_benchmark` in CTest. No search runs or scientific budget
are used by generation, evaluation, or tests.
