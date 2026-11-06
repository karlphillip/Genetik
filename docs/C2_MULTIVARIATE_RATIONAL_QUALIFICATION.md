# C2 — Multivariate rational qualification diagnostic

## Decision

C2 stops at Advancement Gate A. Do not expose or selectively integrate the S14
domain assessor as a public capability until its false pole-free certificate is
corrected and independently qualified. No production API, confirmation campaign,
or integration PR is prepared by this diagnostic milestone.

## PR #17 closeout

PR #17 is merged. Protected `origin/main` is
`73432014a381078fff23e09226210fd5ae144303`; merge commit `73432014…` contains
the S16 feature commit `f4c0d7bf6602fbc9bf1a328c3aeb4b11ac9b1479` as an
ancestor. GitHub reports all six unique required checks successful on the exact
PR head and all six post-merge checks successful on main SHA
`73432014a381078fff23e09226210fd5ae144303` (workflow run
`42260947814`). The task-owned feature worktree was clean and its local and remote
feature refs were removed only after exact-SHA verification and a remote SHA
lease. S16 and all other research refs were preserved. The S14 worktree was
read-only during this review; temporary builds and the counterexample executable
were kept under `/tmp`.

## S14 evidence audit and reproduction

The SHA256 manifest in
`Genetik-data/benchmarks/s14-multivariate-rational-discovery/` verified every
listed protocol, dataset, run record, independent score, and qualification log.
The frozen protocol hash is `7936503b69059d281e41bfef60ec22729290472dcd8620891ad96bafb0d24ce1`;
the campaign source identity is `62df550cc3eb2a9462ef211c49e1844949431c5b`.
The retained integrity record has 24/24 preflighted task/track configurations,
72 dataset files, and 192/192 completed and independently scored runs, with no
recorded timeout or RSS-cap violation.

Independent-score records reconstruct the published rational results:

| Method | Frozen rational cases | Predictive threshold passes |
|---|---:|---:|
| Constructive QR | 16 | 13 |
| Constructive QR + LM | 16 | 13 |
| GA coordinate | 48 | 0 |
| GA + LM | 48 | 2 |

All three noisy near-pole realizations fail the constructive held-out and
extrapolation thresholds for both QR and QR+LM. The two GA+LM passes are finite
prediction-threshold outcomes and do not establish rational equation recovery.
These are the historical results as independently scored; no failure was
reclassified.

The original S14 source at research tip
`bd26be819559e9338f0c57939bc0d6dd39155d88` built in Release and its focused
deterministic contract passed. The clean mixed-variable rational run reproduced
training/held-out/extrapolation RMSE values of approximately
`2.27e-16 / 2.13e-16 / 2.20e-16`; an independent Python AST evaluator over the
frozen CSV observations reproduced those values. The three noisy near-pole QR
runs reproduced the recorded held-out RMSE values `0.3386`, `0.1326`, and
`0.1931`, and extrapolation RMSE values `12.347`, `4.054`, and `4.769`.
Each returned an `uncertain` domain assessment. Their training RMSE values were
only `0.0082`–`0.0131`, confirming that low training error did not control
near-pole extrapolation error.

The S13 and S13.1 research evidence manifests also verified: 234 and 383 files,
respectively. Their retained results independently document noisy improper and
near-pole extrapolation failures, QR/LM disagreement, and the distinction
between predictive agreement and equation recovery.

## Gate A defect: false domain certificate

S14's `assessDomain()` calls `monomialRange()` to bound each denominator term.
That helper handles exponents 0, 1, and 2 only. The public research `Model`
contains mutable basis and expression fields. Assessment checks that the stored
expression matches the supplied basis, but it does not reject basis terms above
total degree two before applying the interval calculation.

The following valid C++20 construction using the original S14 helper produces
the defect:

```cpp
Model m;
m.numeratorBasis = basis(0);
m.numerator = {1.0};
m.denominatorBasis = {{0, 0}, {3, 0}};
m.denominator = {1.0, -3.0}; // Q(x,y) = 1 - 3x^3
m.denominatorExpression = detail::denominatorExpression(
    m.denominatorBasis, std::span<const double>(m.denominator).subspan(1),
    ExpressionLimits{});
m.expression = detail::buildExpression(
    m.numeratorBasis, m.numerator, m.denominatorBasis,
    std::span<const double>(m.denominator).subspan(1), false, 1,
    ExpressionLimits{});
const auto domain = assessDomain(m, {0.0, 1.0, 0.0, 1.0});
```

It returns `Valid / SeparatedFromZero` with reason
`denominator_interval_separated_from_zero`. But `Q(0)=1`, `Q(0.5)=0.625`,
and `Q(1)=-2`, so continuity proves that a denominator zero lies inside the
assessed rectangle. The zero is not one of S14's sampled corner/midpoint values;
the interval routine misses the cubic term entirely. The issue is not a claim
that interval arithmetic can certify model accuracy: it is an unsound domain
classification for a model the assessor accepts.

The fit routine itself generates only the degree-two basis. However, the
assessment accepts caller-constructed or caller-mutated `Model` values, and a
future public API must not issue a safe-domain result for an unsupported basis.
This finding is material to Gate A and must be fixed and tested before any
public API proposal. The required behavior is explicit invalid-model or
uncertain status for unsupported basis terms, and certified bounds must cover
every accepted term.

## Historical Debug ASan+UBSan timeout

The retained S14 CTest log shows the focused contract stopped at its configured
60-second timeout. A fresh Debug ASan+UBSan build reproduced that CTest timeout.
The process was CPU-bound at approximately one core rather than blocked; it
emitted no sanitizer diagnostic. Running the same instrumented test executable
directly under a separate 300-second supervisor completed successfully and
printed `S14 deterministic contracts passed`.

This classifies the recorded failure as a workload/runtime timeout under Debug
ASan+UBSan, not as a sanitizer pass and not as evidence of a memory error. The
current evidence does not isolate whether the duration comes primarily from
Debug instrumentation, sanitizer overhead, or the composition of the broad
contract test. The 60-second CTest allowance is insufficient for that
configuration on this environment. A follow-up should split lightweight memory
and correctness contracts from expensive numerical fixtures while preserving
the scientific budgets; changing budgets to make the sanitizer pass would
invalidate the comparison. S18's historical sanitizer limitation is separate
and unresolved.

## Advancement and compatibility

No new previously unseen confirmation suite was started because Gate A failed.
Consequently Gate B, a public API, and full integration qualification remain
unassessed. No generic GA, RNG, checkpoint, scalar rational-regression API,
default, dependency, or published v0.7.0 artifact was changed. The two-variable
degree-two family and S14 results remain research-only. The recommendation is
to defer integration pending a corrected denominator-domain validator and a
fresh Gate A review, then freeze and run Gate B independently.
