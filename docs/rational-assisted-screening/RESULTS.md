# Rational-assisted screening results

The frozen protocol is [PROTOCOL.md](PROTOCOL.md), with an interpretation
correction in [INTERPRETATION.md](INTERPRETATION.md); full per-run rows, exact
expressions, coefficient literals, work counters and runtimes are in
[RESULTS.csv](RESULTS.csv). The build was Release with GCC 13.3 on the
aarch64 host. The old D7 train/test files were read without modification. The
other four tasks were generated from the independent functions specified by
the protocol. Validation and assessment were scored only after each training
run returned its model. No labels from either split entered a fit or search.

## Recovery and quality

Numerical recovery was fixed at assessment NRMSE `<= 1e-6`. `B` is one
deterministic fit per case; `A` and `C` have three seeds each.

| Case | A symbolic | B rational | C hybrid | C best already seeded |
|---|---:|---:|---:|---:|
| Historical D7 rational x² | 0/3 | 1/1 | 3/3 | 0/3 |
| Independent rational (1,1) | 0/3 | 1/1 | 3/3 | 3/3 |
| Independent rational (2,1) | 0/3 | 1/1 | 3/3 | 3/3 |
| Cubic polynomial control | 0/3 | 1/1 | 3/3 | 2/3 |
| Nonrational sine | 0/3 | 0/1 | 0/3 | 0/3 |

Across the three seeds, median assessment NRMSE for A/B/C respectively was:

| Case | A | B | C |
|---|---:|---:|---:|
| D7 rational x² | 0.5457 | 4.07e-17 | 0 |
| Rational (1,1) | 0.2515 | 1.09e-16 | 1.09e-16 |
| Rational (2,1) | 0.4901 | 1.32e-16 | 1.32e-16 |
| Cubic | 0.5064 | 1.18e-16 | 1.18e-16 |
| Sine | 0.2362 | 1.82e-5 | 1.82e-5 |

The rational fitter alone matched the hybrid's numerical recovery on every
rational and cubic polynomial task; it also supplied the hybrid's sine approximation.
Thus these results show a substantial quality gain over the bounded ordinary
symbolic configuration, but no accuracy improvement over the constructive
rational fitter. On the three independent rational/cubic cases, 8 of 9 hybrid
best expressions were already in the two reserved initial slots; those are
constructive-fit successes, not evolutionary discoveries. On historical D7,
the initial fit was already within roughly `6.4e-17` training RMSE and later
evolution produced zero measured training error. The sine final expressions
changed through evolution, but their training, validation, and assessment
errors stayed equal to the initial fit to reported precision; those changes did
not improve held-out quality.

Exact symbolic equivalence was not proved for any returned expression; the CSV
marks all rows `not_proven`. Numerical agreement is only reported on the finite
assessment sets. No global equivalence or pole-free-domain claim is made.
Rational cases are evaluated on `[-1,1]`; any equality claim would additionally
require that both represented denominators be nonzero throughout that interval.

## Cost, work, and failure outcomes

End-to-end milliseconds are shown as median [min, max] across stochastic seeds;
B is deterministic and shown once:

| Case | A search, median [range] | B fitter | C hybrid, median [range] |
|---|---:|---:|---:|
| D7 rational x² | 6.55 [6.04, 8.33] | 39.01 | 42.89 [42.40, 48.08] |
| Rational (1,1) | 4.82 [4.74, 5.45] | 10.25 | 16.57 [16.39, 17.36] |
| Rational (2,1) | 4.74 [4.40, 4.95] | 10.28 | 18.40 [17.84, 19.00] |
| Cubic | 5.72 [5.45, 5.99] | 10.26 | 18.79 [17.32, 20.16] |
| Sine | 3.79 [3.62, 4.34] | 10.28 | 20.37 [19.53, 21.08] |

The hybrid end-to-end cost includes the single measured rational fit for that
case and the seed's symbolic search. The fit ran once per case and its batch was
reused across the three stochastic seeds. Initializer time was 39.0 ms on D7
and 10.2 ms on each independent case. The hybrid was slower than the rational
fitter alone on every case because it also runs the full symbolic budget. It is
an integration path, not an efficiency improvement. Search arms
each had a 624 candidate-call and training-loss allowance. All hybrid runs used
the declared 50,000,000 initializer work cap; actual work units were 70,624 on
D7 and 23,856 on each independent cases. These units are the maximum of
two differently defined fitter counters, not a hardware operation count.

A median search used 624 candidate calls for A and 624 for C; exact D7 hybrid
stops used 240 or 528. Median training-loss attempts were 624 for both search
arms. The fitter made 16 training-model score evaluations per case; its QR
arithmetic and linear-algebra counters are recorded separately in the CSV and
must not be read as evaluator calls. A/C invalid-loss totals were 432 and 655
across the 15 runs each. The rational fitter reported zero invalid fitted models
in these five cases. All 35 runs returned; there were no timeouts, cancellations,
exhausted budgets, missing models, or operational failures. Noisy and near-pole
stresses were outside the frozen scope.

Campaign-process peak RSS was 4,736 KiB. This is the single process high-water
mark for the whole campaign, not an isolated per-run memory figure. Per-seed
runtimes and their min/max spread are retained in the CSV. The hybrid takes
roughly 3.3–6.6x A's end-to-end time. No wall-clock acceptance threshold was
used.

The CSV reports exact final expression text and all constant literals. Its
`constant_nodes` is a structural count. `fitted_parameter_count` is populated
for rational models and for hybrid winners that exactly retain a fitted seed; it
is `NA` when evolution changed the seed structure, and is zero for A because
continuous refinement was disabled. For B the free rational coefficient count
is `m+n+1`; normalized `Q(0)=1` is fixed. Hybrid initial parameters come from
that fit, while later evolution can alter structure and literals. Every final
expression and its constant values are preserved verbatim in the CSV.

## Conclusion

The generic initializer seam is useful for composing data-derived expressions
with the ordinary symbolic population and preserving the usual evaluator,
grammar and budget checks. The measured hybrid improves on A but does not beat B
and is slower end to end than B. The evidence supports the feature as optional
integration functionality, not as a better autonomous rational discovery
method. No external competitor is included; external environments remain
unqualified and unavailable for this internal screen.

## Qualification record

The final Release build completed. Its full CTest invocation passed 135 of 136
tests; repository hygiene flagged the task-created installed consumer directory
because it was initially placed directly under `out/`. I moved that directory
into the recognized `out/consumers/` role, and reran the hygiene test
successfully. Thus every Release test passed across the full run plus the
isolated hygiene rerun. Total full-suite CTest time was 1,042.95 seconds; the
relocated installed consumer test took 729.64 seconds. Focused Debug and
ASan/UBSan checks each passed the four symbolic/rational regression contracts.
After refreshing the final Release install, a separately configured consumer
including `RationalSymbolicRegression.hpp` compiled and ran successfully.
