# Scientific search results

Frozen protocol: [PROTOCOL.md](scientific-results/PROTOCOL.md). All15 runs completed once, no configuration adjustments. Models/representatives were frozen before loading evaluation rows. This is sample recovery evidence, not proof of mathematical or domain equivalence.

Population128, no continuous fitting, nodes15/depth5, loss cap10000, seeds0/1/2/3/42. Independent high-precision targets and complete configuration/data identities are retained. Each case uses its declared opt-in grammar. All fit counters are zero. Runtime covers search including initialization only; data loading, model freezing, metric computation and output are separate.

| Case | Seed | Initial MSE | Final MSE | Best generation | Published | Loss calls | Actual rows | Seconds | Train max | Interior max | Exterior max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| decay | 0 | 0 | 0 | 0 | 0 | 128 | 4192 | 0.00139194 | 0 | 0 | 0 |
| decay | 1 | 0.0856149 | 0 | 1 | 1 | 256 | 8431 | 0.00264948 | 0 | 0 | 0 |
| decay | 2 | 0.0340702 | 0 | 1 | 1 | 256 | 8447 | 0.00260368 | 0 | 0 | 0 |
| decay | 3 | 0 | 0 | 0 | 0 | 128 | 4203 | 0.00100633 | 0 | 0 | 0 |
| decay | 42 | 0.0856149 | 0 | 2 | 2 | 384 | 12672 | 0.0038328 | 0 | 0 | 0 |
| oscillation | 0 | 0.443498 | 7.07198e-33 | 27 | 27 | 3584 | 118272 | 0.0389728 | 2.22045e-16 | 2.22045e-16 | 2.22045e-16 |
| oscillation | 1 | 0.313997 | 0.148168 | 69 | 77 | 9984 | 329472 | 0.100497 | 1.07329 | 1.07763 | 1.08082 |
| oscillation | 2 | 0.443498 | 7.07198e-33 | 67 | 67 | 8704 | 287232 | 0.0893372 | 2.22045e-16 | 2.22045e-16 | 2.22045e-16 |
| oscillation | 3 | 0.477505 | 7.07198e-33 | 39 | 39 | 5120 | 168960 | 0.0486212 | 2.22045e-16 | 2.22045e-16 | 2.22045e-16 |
| oscillation | 42 | 0.443498 | 7.07198e-33 | 2 | 2 | 384 | 12672 | 0.00275127 | 2.22045e-16 | 2.22045e-16 | 2.22045e-16 |
| root | 0 | 0.502332 | 0 | 34 | 34 | 4480 | 103420 | 0.0271634 | 0 | 0 | 0 |
| root | 1 | 0.502332 | 0 | 30 | 30 | 3968 | 93264 | 0.0231497 | 0 | 0 | 0 |
| root | 2 | 0.502332 | 0 | 38 | 38 | 4992 | 115627 | 0.0299784 | 0 | 0 | 0 |
| root | 3 | 0.0793283 | 0 | 10 | 10 | 1408 | 31935 | 0.00920786 | 0 | 0 | 0 |
| root | 42 | 0.191489 | 0 | 29 | 29 | 3840 | 85283 | 0.0278362 | 0 | 0 | 0 |

All selected models have zero invalid predictions on all declared datasets. Fourteen runs stop at the training-only MSE threshold1e-24; oscillation seed1 stops at the allowance boundary (9984 calls,16 unused,77 published generations). Max-error recovery<=1e-6 holds on all three datasets for decay5/5, oscillation4/5 and root5/5. No initial target was discarded or run restarted. Discovery generations include zero where observed.

| Case | Seed | Nodes/depth | Constants | Exponents | Faithful expression |
| --- | ---: | --- | --- | --- | --- |
| decay | 0 | 4/3 | -1, |  | `exp(((-1.0) * x0))` |
| decay | 1 | 4/3 | -1, |  | `exp((x0 * (-1.0)))` |
| decay | 2 | 7/4 | 0,0, |  | `exp(((0.0 - x0) * exp(0.0)))` |
| decay | 3 | 4/3 | 0, |  | `exp((0.0 - x0))` |
| decay | 42 | 8/4 | 1, |  | `exp(((x0 - x0) - (1.0 * x0)))` |
| oscillation | 0 | 12/5 | 0,1, |  | `(sin((sin(0.0) + (1.0 * x0))) + cos((x0 + x0)))` |
| oscillation | 1 | 9/5 | 1, |  | `sin(((sin(1.0) + sin(x0)) + sin(x0)))` |
| oscillation | 2 | 7/4 |  |  | `(sin(x0) + cos((x0 + x0)))` |
| oscillation | 3 | 7/4 |  |  | `(sin(x0) + cos((x0 + x0)))` |
| oscillation | 42 | 7/4 |  |  | `(cos((x0 + x0)) + sin(x0))` |
| root | 0 | 5/4 | 1, | 2, | `sqrt((powi(x0,2) + 1.0))` |
| root | 1 | 5/4 | -1, | 2, | `sqrt((powi(x0,2) - (-1.0)))` |
| root | 2 | 5/4 | 1, | 2, | `sqrt((1.0 + powi(x0,2)))` |
| root | 3 | 7/4 | -2,-1, | 2, | `sqrt((powi(x0,2) - ((-2.0) - (-1.0))))` |
| root | 42 | 6/4 | 1, |  | `sqrt((1.0 + (x0 * x0)))` |

Representatives selected from fitting information only (MSE, nodes, faithful text, then first declared seed): decay0, oscillation42, root2. They remain selected after assessment.

The training-mean baselines have MSE:

- decay: `training_mean_baseline=0.25372290682112475 mse=0.072583188477104932`.

- oscillation: `training_mean_baseline=0.11300426809994532 mse=0.90823300790911576`.

- root: `training_mean_baseline=1.9369170048845279 mse=0.49835251618914855`.

The unsuccessful oscillation seed1 has fitting MSE0.14816829414366453 and max error1.07329, versus its mean baseline recorded above. Its training-only trace records5 committed improvements; its final expression nests sine and omits a cosine term. Variation changed candidates (see mutation/crossover counters), and the run completed77 generations. This is finite-budget heuristic stagnation rather than evidence of stale fitness, disabled variation or a need to tune against evaluation rows. No adjustment was made.

Full per-seed work, unused allowances, stopping reasons, structural rejections, constants/exponents, invalidity counts and committed improvement traces: [campaign records](scientific-results/campaign/). The full valid primitive cost printed for a selected model is a per-prediction plan cost, not a measured search-wide primitive count. Actual loss and visited-row counters are measured; no primitive operation budget was added to the engine.

## Reproduction

```sh
out/build/release/genetik_scientific_campaign --data tests/fixtures/scientific --output out/runs/scientific-new --threads 1
python3 scripts/check_scientific_results.py out/runs/scientific-new --output out/runs/scientific-new/results.json
```

The preselected decay seed2 file command is in [SCIENTIFIC_OPERATORS.md](SCIENTIFIC_OPERATORS.md). It reproduces the direct API search expression, exact score and every deterministic printResult field, excluding elapsed time. The retained file output is in scientific-results/qualification/file-reproduction.txt. Held-out rows are supplied only to post-selection reporting.

The generator can be rerun with `python3 scripts/generate_scientific_references.py --output out/data/scientific-reference-check` when development-only mpmath is installed. Retained input/reference/data checksums are tests/fixtures/scientific/SHA256.json. Runtime and normal builds need no mpmath.
