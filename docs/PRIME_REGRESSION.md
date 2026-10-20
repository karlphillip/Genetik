# Prime-prefix approximation through generic regression

This example fits all indices 1–1000; none of that prefix is a holdout. The later
1001–1200 range is withheld from fitting and model selection. After this campaign
it is a previously examined benchmark, not a fresh holdout for informed tuning.
No claim about all primes follows from approximation or exact rounded matches.
The existing strict-arithmetic five-seed results (including two misses) are retained.

## Predeclared experiment

Recorded before the pilot and main campaign: deterministic serial Release CPU;
seeds 0,1,2,3,42; population 64; 2,000 generation attempts; 31 nodes; depth 6
(leaves count one); absolute stored constant limit 8. Uniform selection among
structurally feasible enabled add,sub,mul,div,log entries, each weight one.
Half-full/half-grow initialization, random root cap up to 4; grow probability .65;
mutation subtree depth cap 3; eight variation proposals then unchanged-parent
fallback; crossover .9, mutation .35; linear-rank selection, two elites.
Terminals use the accepted half-discrete {-2,-1,0,1,2}/half-uniform [-8,8) constant
source (when choosing a constant; variable/constant terminal choice remains half).
Constant fitting applies to every prepared slot, including initialization, elites
and unchanged clones. Bounds [-8,8], initial step 1, minimum step 1e-8,
256 actual loss calls including baseline per fit, 128 sweeps, 8,192 proposals.
Both fitting and run thresholds are exact zero, disabling the toy 1e-14 stop.

Only u = double(n)/1000.0 is an input. Target v = double(prime)/10000.0.
Search minimizes checked normalized MSE. The standalone decoded predictor is
10000.0 * g(double(n)/1000.0), evaluated with this grouping and strict per-node
semantics. These fixed transformations are not learned constants. There is no
prime oracle, target lookup, reference feature, residual formula, repair or injected
baseline/skeleton in expression initialization or variation. All prime-specific
code is application-only in examples/PrimeRegression.hpp and examples/prime_regression.cpp.
The shared library, dataset loss, fitter and publication engine are unchanged.

Order each run and the overall representative by fitting MSE, node count, faithful
expression text, then lowest seed as the stable final campaign tie. Freeze all five
committed results, representative and baseline parameters before computing any
extrapolation predictions. No accuracy-based configuration adjustment is planned.

A separate seed-2026 Release pilot allows at most 4,096 actual calls, fitting at
most 512 independently initialized random expressions through the public objective
and fitter. Require at least 128 valid full-1,000-row calls. Estimate six 100,000-call
runs by 6*100000*pilot_seconds/valid_full_calls, with a factor-two conservative
margin (this charges even invalid attempts as full calls and includes pilot fitting
and generation overhead). Choose 100,000 if <=1,800 seconds, otherwise 50,000.
Runtime alone determines that choice. Pilot values are discarded, never seeded
into the campaign. Even 50,000 exceeds three worst-case 64*256=16,384-call
preparations; initialization and multiple complete preparations are affordable.
The generation/candidate boundaries and actual-loss cap retain unpublished-work
preservation; consumed work/randomness is not refunded. Unused allowance is explicit.

## Data, provenance and input

The benchmark is offline. A bounded sieve through 10,000 generates 1,200 exact
uint32 values. Independent trial division verifies every integer through the last
prime, checking completeness as well as primality. Anchors 1=2,2=3,3=5,100=541,
1000=7919,1001=7927,1200=9733 agree with the
[OEIS A000040 indexed reference](https://oeis.org/A000040/b000040.txt), checked
2026-10-20. Network access is unnecessary to generate, validate or run.

An optional --prime-file accepts exactly the first 1,000 values, one unsigned
base-ten integer per nonblank line; ASCII spaces/tabs at line edges and blank lines
are ignored; LF/CRLF and a final unterminated line work. No header, signs, quoting,
floats, comments or embedded whitespace. Limits: 65,536 bytes/file, 32 bytes/line
(including terminal CR, excluding LF), 1,000 nonblank values and uint32 range.
Every value is checked against the complete reference prefix in order; omissions,
duplicates, reordering, wrong values, extra or incomplete data fail before search.

Generated fitting.csv/extrapolation.csv contain n,prime,u,v in ascending n order,
classic locale and 17 significant digits, sufficient for binary64 round trips.
Only --inputs u --target v is the declared file workflow. Raw columns are reporting
metadata, never search inputs. first-1000.txt and first-1200.txt retain exact integers.
SHA-256 manifests in the result directory record generated data and complete tables.

## Baselines and metrics

Before extrapolation, fit constant, affine and degree-three cubic models and
A*u*log(u)+B*u+C on the same 1,000 normalized targets. Example-local Householder QR
uses long-double working arithmetic, never normal equations or the GP fitter.
Coefficients are stored as doubles; predictions use the exported strict expression
with 10000 output conversion. Cubic grouping is explicitly shown in baselines.txt.
The fifth reference n*std::log(n) is evaluated directly in raw units, including zero
at n=1, with no fitted parameters. Its normalized mathematical family is expressible
within the configured grammar/limits as (0.1*u)*(log(u)+log(1000)), with log(1000)
a permissible finite constant; this identity is not a bitwise floating-point
rewrite of the actual baseline, nor of a learned model. Other baseline trees are
validated directly under the configured candidate limits. None enters the search.
QR uses four 1,000-row design matrices; all work is separate from GP accounting.

Assess every row, never just the valid prefix. Decode finiteness failures count as
arithmetic-invalid predictions. Full-range MSE/RMSE/MAE/max absolute/mean and max
absolute relative errors are absent if any prediction or checked error accumulation
is invalid; no partial aggregate replaces them. Relative denominators are positive
reference primes. Raw-unit squared/absolute/relative sums use sequential double
arithmetic and finiteness checks. Numerical finite negative predictions remain
outputs and contribute errors; integer unrepresentability alone does not invalidate
continuous metrics.

Round once with std::round (nearest, halfway away from zero). Convert only finite
rounded values in [-2^63,2^63); never compare against double(INT64_MAX) inclusively.
Invalid/unrepresentable predictions remain in the exact-match-rate denominator.
Exact matches compare each prediction with the reference at that same index.
Raw/rounded strict-increase violations count adjacent assessable pairs with
next<=previous, without bridging gaps. Duplicate rounded predictions count occurrences
after the first across the whole range, among representable integers. Both adjacent
pair denominators are reported; rounding never repairs predictions.

Each model has all 1,200 rows in its prediction CSV: index, range, exact reference,
continuous raw prediction, rounded integer if representable, residual and validity.
The representative-first-1000.csv is the complete fitted prefix, not a selection of
matches. Traces contain only committed-best changes against cumulative actual work.
Baseline coefficients, frozen expressions and per-run counters are separate artifacts.
Post-search scans use 1,200 predictions per available model and zero search loss calls;
these raw-unit metric calculations are reported separately from training work.

## Commands

From the repository after the standard Release build:

```sh
out/build/release/genetik_prime_regression --generate --output out/data/prime
out/build/release/genetik_prime_regression --pilot --output out/runs/prime-pilot
# Use the pilot-selected common allowance recorded in PRIME_CAMPAIGN.md:
out/build/release/genetik_prime_regression --campaign --output out/runs/prime-results --loss-budget 100000
out/build/release/genetik_prime_regression --campaign --output out/runs/prime-owned --loss-budget 100000 --prime-file out/data/prime/first-1000.txt
out/build/release/genetik_regression_search out/runs/prime-results/fitting.csv --inputs u --target v --seed 0 --loss-budget 100000 --loss-threshold 0 --operators add,sub,mul,div,log
```

The additive generic CLI --loss-threshold sets both run and fitting MSE thresholds;
its existing default stays 1e-14. Zero requests only exact zero. No file numerical
contract changed. Optional --evaluate remains post-selection only. Supported
repeatability includes the same library/compiler/standard library, libm, strict FP
flags, input doubles/configuration and serial execution; elapsed times are excluded.
See PRIME_CAMPAIGN.md for measured allowance, all outcomes and verification evidence.

To capture and check a reproduction and checksum the generated artifacts:

```sh
../build-release/genetik_regression_search docs/prime-results/fitting.csv --inputs u --target v --seed 0 --loss-budget 100000 --loss-threshold 0 --operators add,sub,mul,div,log > ../prime-file-seed-0.txt
python3 scripts/check_prime_reproduction.py docs/prime-results/campaign.txt ../prime-file-seed-0.txt 0
python3 scripts/plot_prime_residuals.py docs/prime-results
(cd docs/prime-results && sha256sum *.csv *.txt residuals.png > SHA256SUMS)
(cd docs/prime-results && sha256sum -c SHA256SUMS)
```

The comparison/plot helpers are optional application tools. The executable, reference
and search have no Python, plotting or network runtime dependency. The committed
result directory records the completed campaign; use a separate output directory
for a new experiment rather than overwriting that evidence. Exported expressions
are faithful text descriptions, not a promised C++ parser or serialization format.
