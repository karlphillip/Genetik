# Finite-Domain Identifiability Audit v1

## Status and hypothesis

This is a frozen diagnostic study, not a synthesis or search-algorithm
benchmark. The falsifiable hypothesis is: among explicit finite catalogues of
typed `DiscreteExpression` candidates, deterministic disagreement queries can
distinguish modular and threshold behavior using fewer exact oracle queries
than querying the remaining integer inputs in ascending order, without making
false claims of structural or universal identification.

Each of the four base candidates in every positive catalogue is separately
used as the reference (16 positive runs). The `E+0` forms are structural
representatives, not additional reference controls. A finite-catalogue ambiguity case and an
outside-catalogue case add two controls. Both query policies run on all 18
controls, for 36 executions. One fixed replay per policy of the threshold
family's first base candidate (`if n<0 then 0-n else n`) makes 38 total
executions. Primary runs have seed metadata 0 and replays seed metadata 1; no
RNG is used, and each run is deterministic.

## Candidate catalogues and training observations

The input is one signed Int64 integer `n`; the output is a signed Int64
integer. All operators use ordinary checked `DiscreteExpression` semantics.
The listed four candidates and their structurally distinct `E+0` forms are
retained separately (eight syntax trees per positive catalogue). Finite
signature equality never deletes a structural representative.

| Family | Four base candidates | Initial observations |
|---|---|---|
| Arithmetic | `n+2`; `2*n+2`; `n*n+2`; `2` | `{0}` |
| Modular | `n mod 3`; `n mod 5`; `(n mod 3) mod 2`; `n mod 2` | `{0,1}` |
| Threshold | `if n<0 then 0-n else n`; `n`; `if n<-1 then 0-n else n`; `if n<0 then (0-n)+1 else n` | `{0,1,2}` |
| Modular conditional | Let `q=n*n-2`: `if n mod 3==1 then n+2 else q`; corresponding true branch `n+3`; corresponding true branch `2*n`; and `q` | `{0,2,3}` |

The ambiguity catalogue is `n`, `n+0`, and `if n>8 then n+1 else n`, with
initial observation `{0}` and reference `n`. These are finite-domain-equivalent
on the query domain, although the third differs outside it.

The outside-catalogue reference is `U(0)=2^40`, and `U(n)=n` otherwise. It is
paired with the arithmetic catalogue and initial observation `{0}`. The
required outcome is an empty compatible set, not a synthesis success.

The selector receives candidate predictions over the finite query domain but
no reference labels except initial observations and answers to its chosen
queries. The independent oracle is a separately implemented exact reference
evaluator. Reference identities and assessment labels are kept out of the
selector input.

## Query domain and policies

The finite query domain is the ordered set `D={-8,-7,...,8}`. Initial inputs
are removed. Each policy may request at most eight additional observations.
For each candidate, a full typed output signature over `D` is computed from
candidate-only calls to `DiscreteExpression::evaluateWithCancellation` with
fixed per-row limits. No reference values are used to build signatures. Each
candidate-row evaluation and its exact status and work counters are retained.
Invalid candidate evaluation is not a signature value; resource exhaustion or
cancellation makes that run incomplete. Observed-input compatibility and
every elimination are separately checked with
`validateDiscreteSpecification`; its specification is extended immutably by
appending one oracle response at a time.

At every step candidates inconsistent with known observations are removed
using the existing validator. Candidate behavior classes are grouped only by
their exact typed signatures on `D`; all structural representatives remain
available for reporting and assessment.

1. **Disagreement policy:** for every unobserved input, partition currently
   viable finite behavior classes by their exact predicted value. Choose the
   input minimizing the largest resulting class. Ties go to the smallest
   integer. Request that input's reference value, append it to the immutable
   specification version, and eliminate incompatible candidates.
2. **Ascending baseline:** request the smallest unobserved input in `D`, append
   its reference value to the specification version, and eliminate
   incompatible candidates.

Stop when one finite behavior class remains, the compatible set is empty, or
eight queries have been consumed. If multiple classes remain at the cap, the
run is unresolved. A surviving class is not a proof of global equivalence.

## Independent assessment

After all query transcripts, candidate partitions, and stopping decisions are
immutable, independently assess every surviving structural candidate on the
new held-out set `{-17,-16,...,-9,9,10,...,17}`. This set is disjoint from
`D`. Assessment labels are stored separately, are unavailable to the selector,
and are read only by the independent scorer after transcripts are sealed. No
historical sealed assessment artifact is used. Assessment agreement is finite
evidence only; it does not establish universal equivalence.

The independent assessor also recomputes all candidate signatures and
partitions on `D` using an independent exact arithmetic implementation, checks
each chosen input and elimination, and verifies the two replays. No assessment
label participates in candidate construction, query selection, or stopping.

## Frozen acceptance criteria

All criteria are required for a positive result:

1. Zero incorrect finite partitions, eliminations, oracle responses, or
   counterexample records, and zero false `ValidatedFiniteDomain` certificates
   on their explicitly declared finite input sets.
2. Exact deterministic replay of transcripts, decisions, identities, and work
   counters (timing/process measurements are excluded from byte-for-byte
   identity).
3. Correct finite-ambiguity and outside-catalogue outcomes, with no global
   equivalence claim.
4. All four generating members of each positive catalogue are reported; a
   catalogue member being present is never called a synthesis recovery.
5. Every positive-control execution identifies one finite behavior class
   within the query cap. Any unresolved positive control makes advancement
   inconclusive and cannot improve the query-count comparison. The ambiguity
   and outside-catalogue controls must retain their separately specified
   outcomes.
6. Mean oracle-query count for disagreement is strictly lower than ascending
   on both the modular and threshold families (averaged over their four
   reference members), and no higher on arithmetic or modular-conditional
   families.
7. All 38 executions complete with a defined stop reason; no resource-
   exhausted, cancelled, malformed, or otherwise incomplete run is counted as
   a successful identification.

Failure of any criterion is a negative or inconclusive result. There will be
no retuning or rerun against these controls.

## Resource limits

Per execution: at most 8 candidates, 21 nodes and depth 6 per expression, 8
oracle queries, 1,024 candidate-row evaluations, 100,000 charged work units, a
1 MiB cumulative logical-storage charge, 512 MiB address space, 2 CPU seconds,
3 wall seconds, and 1 MiB combined output. The logical-storage charge sums
admitted logical allocations across the run, including temporary validator
results, rather than claiming to estimate peak process memory. Charge candidate
construction and identity handling, validator work, candidate-signature
comparisons, partition/bucket operations, and oracle work. The enforced process
address-space cap and any sampled RSS are reported separately.

Cumulative scientific limits: 38 executions; 38,912 candidate-row
evaluations; 3.8 million charged work units; 76 CPU seconds; 114 wall seconds.
The one synthetic four-stage pipeline preflight has per-stage ceilings of 10
CPU seconds, 15 wall seconds, 512 MiB address space, and 1 MiB combined output.
Final independent verifier, scorer, and aggregator stages each have the same
10 CPU-second, 15 wall-second, 512 MiB address-space, and 1 MiB output ceilings.
Total top-level ceiling is 45 process invocations, 146 CPU seconds, and 219
wall seconds. No retries or unbudgeted child processes are permitted. Any
resource exhaustion is incomplete evidence and fails the acceptance gate.

## Scope and interpretation

This study measures finite identifiability for supplied candidate catalogues.
It does not measure candidate generation, evolutionary discovery, or the
ability to invent an unknown mathematical rule. It may justify a later optional
finite-candidate analysis utility only; it does not by itself justify changing
Genetik's search algorithms or claiming mathematical identity outside the
declared finite domain.
