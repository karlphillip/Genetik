# P0 — Computational feasibility

## Scope and method

This is a bounded arithmetic audit, not a campaign. Calculations below use only small direct verification scripts and asymptotic/closed-form work estimates. No GP run, candidate enumeration, benchmark campaign, or production code change was made.

## Practical reference baseline: exact sieve plus count

A byte-array Eratosthenes sieve used the proven Rosser–Schoenfeld bound `p_n < n(log n + log log n)` for `n>=6` to choose a target-independent endpoint. At `n=1000`, the bound is below 8,841, so sieving through 8,841 returns 1,102 primes, including `p_1000=7919`. The Boolean array contains 8,842 bytes; the loop checks 93 potential bases through `floor(sqrt(8841))=94`, performs 14,897 composite-mark attempts, and changes 7,738 entries from candidate to composite. This is a tiny bounded computational task.

This is an ordinary prime-enumeration algorithm and is **not** evidence for a qualifying discovered formula. Its relevance is calibration: the goal is not hard to compute with a sieve; the hard goal is finding a compact mathematical rule that does not hide the sieve/primality test.

The rough PNT estimate `n log n` at `n=1000` is about 6,908, and the next asymptotic expression `n(log n + log log n - 1)` is about 7,840.4. These are estimates, not exact outputs or certified upper bounds at this index by themselves. They illustrate why direct approximation can land near 7,919 but cannot ensure the exact prime without an exact counting/primality mechanism.

## Family cost estimates

| Representation | Evaluation cost at 1,000 | Intermediate magnitude / precision | Scaling and conclusion |
|---|---|---|---|
| Sieve and count/invert | For a cutoff of 8,841 from the Rosser–Schoenfeld upper bound: `O(N log log N)` elementary marking, `O(N)` bits/bytes of storage. The concrete run used 8,842 bytes and changed 7,738 entries to composite. | Integers fit in 14 bits; no arbitrary precision needed. | Excellent practical baseline; explicit candidate enumeration means it is not the requested discovered law.
| Trial division / Wilson indicators over candidates | Even `O(N sqrt(N))` simple divisor trial steps to enumerate all primes below 8,000 is bounded at this scale (the crude bound `N floor(sqrt N)` is under 0.75 million trial divisors). Wilson factorial checks are far worse if computed directly. | `(k-1)!` exceeds signed 64-bit after `20!`; Wilson's congruence can be computed modulo `k` but requires an iterative modular product for each candidate. | Practical primality algorithms exist, but they are algorithms to classify candidates. The Willans nested sum has `2^n` outer terms and `2^n(2^n+1)/2` inner summands: about `5.74×10^601` at `n=1000` (602 decimal digits in the count of summands). No representation-size improvement rescues the stated formula.
| Prime-counting inverse | `min{x:pi(x)=n}` is exact if `pi` is exact. Sieve-backed `pi` has the same candidate-classification costs as above. Analytic methods require specialized algorithms and rigorous precision control. | Analytic formulas involve nontrivial precision and truncation; exact integer answer requires a proven error enclosure around the crossing. | Feasible as a conventional algorithm, but not independent of prime-counting implementation.
| Ruiz–Sondow exact floor/divisor-count formula | Their `p_n` sum is bounded by `floor(2n log n+2)` and they give optimized `O((n log n)^(3/2))` computation. At `n=1000`, the bound is 13,817. Evaluating the optimized divisor-count formula for all `j` through that bound takes `sum_{j=2}^{13817} floor(sqrt(j)) = 1,075,970` divisor checks. | Fixed-width integers suffice for these bounded divisor counters. | Feasible at this target size, but every candidate integer is explicitly tested for divisors and a prime-indicator sum is computed. This is enumeration in mathematical notation, not an acceptable new discovery.
| Mills | A term has approximate magnitude `A^(3^k)` and output bit length `Theta(3^k)`. Assuming the commonly quoted least Mills constant `A ≈ 1.3063778838630806904686144926` (whose value is conditional on the Riemann hypothesis), at `k=1000` the result has approximately `1.53e476` decimal digits, equivalently about `0.386*3^1000` bits (on the order of `10^477` bits): it is unmaterializable. | Computing a floor reliably needs enough information about `A` to resolve that enormous integer; the constant's expansion is tied to a prime chain. | Does not output the kth prime and becomes impossible to materialize long before the requested index.
| Jones–Sato–Wada–Wiens Diophantine polynomial | The theorem gives a degree-25, 26-variable polynomial with positive values exactly the primes. It provides no bounded witness-size or efficient inverse from prime index to polynomial arguments. | Large integer powers and cancellation can make intermediate values far larger than the final prime; no practical fixed-width bound follows from the representation theorem. | Enumerating integer tuples until the nth distinct prime-valued output is found is itself an unbounded search with no useful cost guarantee.
| Prime-gap recurrence | If a compact recurrence existed, cost per term could be low. But the transition must encode exact gaps; no such short general rule is established. | Could fit Int64 for first 1,000 outputs, but hypothetical later growth and overflow semantics require proof. | Search based on finite gaps risks interpolation/memorization; S15's direct-expression search did not generalize.

## Arithmetic and resource constraints in Genetik

The discrete evaluator uses checked `Int64` semantics. It reports overflow, invalid operations and exhausted iteration budgets explicitly; those are failures, not reasons to use floating-point rounding. Factorial and products are therefore limited by intermediate values, not just final outputs. Arbitrary precision would change the arithmetic cost model and is not part of this project’s standard-library API.

Current `DiscreteExpression` includes bounded aggregate constructs (`sum`, `product`, `first`) and iteration limits, but an aggregate expression is not a free loop: each iteration consumes work, nested iteration multiplies work, and storage/work ceilings apply. A finite formula with nested sums over `2^n` cannot be accepted based on its syntax-node count. Genetik's genetic search also has no discoverability evidence for the required algorithmic structures; bounded search over operator trees grows combinatorially and suffers exact finite-prefix ambiguity.

## Beyond the first 1,000

- A sieve through `p_n` uses storage proportional to the numeric endpoint, which grows roughly as `n log n`, and work at least proportional to the number of candidates inspected for a full sieve. Segmented sieves reduce memory but remain explicit enumeration.
- Willans' outer bound grows as `2^n`, and its nested sum term count as approximately `4^n/2`.
- Mills output size grows exponentially in `3^n`; computing or even writing the result dominates any compact exponentiation program.
- Diophantine witness search has no usable bound from the set-representation theorem.
- Asymptotic `p_n` formulas improve estimates but do not supply exactness; an exact correction mechanism again needs prime-distribution/classification information.

## Reproducibility record

`Genetik-data/benchmarks/p0-prime-representation-feasibility/calculations.json` records the deterministic small sieve counts, asymptotic arithmetic, formula term counts, and scale estimates. The calculation intentionally stores no prime table and does not produce a candidate program. Its SHA256 is recorded in the package manifest.

## References

- C. P. Willans, “On Formulae for the Nth Prime Number,” *The Mathematical Gazette* 48(366), 413–415 (1964), [DOI 10.2307/3611701](https://doi.org/10.2307/3611701).
- J. B. Rosser and L. Schoenfeld, “Approximate formulas for some functions of prime numbers,” *Illinois Journal of Mathematics* 6(1), 64–94 (1962), [DOI 10.1215/ijm/1255631807](https://doi.org/10.1215/ijm/1255631807). Theorem 3 gives `p_n < n(log n + log log n)` for `n >= 6`.
- J. C. Lagarias and A. M. Odlyzko, “Computing π(x): An Analytic Method,” *Journal of Algorithms* 8(2), 173–191 (1987), [DOI 10.1016/0196-6774(87)90037-X](https://doi.org/10.1016/0196-6774(87)90037-X), [author-hosted paper](https://cr.yp.to/bib/1987/lagarias.pdf).
- J. P. Jones, D. Sato, H. Wada and D. Wiens, “Diophantine Representation of the Set of Prime Numbers,” *The American Mathematical Monthly* 83(6), 449–464 (1976), [DOI 10.1080/00029890.1976.11994142](https://doi.org/10.1080/00029890.1976.11994142).
- W. H. Mills, “A prime-representing function,” *Bulletin of the American Mathematical Society* 53 (1947), 604, [DOI 10.1090/S0002-9904-1947-08849-2](https://doi.org/10.1090/S0002-9904-1947-08849-2).
- S. M. Ruiz and J. Sondow, “Formulas for π(x) and the nth Prime,” *International Journal of Mathematics and Computer Science* 9(2), 95–98 (2014), [journal PDF](https://future-in-tech.net/9.2/R-RuizSondow.pdf), [arXiv preprint](https://arxiv.org/abs/math/0210312).
