# Saved-evidence correction (2026-11-28)

The frozen comparison was run from base `5425e68726337c038d88112f9b91d817d9f7eed6`, which contains PR #76. At correction time, freshly fetched `origin/main` was `562eb629bcb22cd2cfe1b6becf92e53673e089c9` after the approved PR #77 merge. The saved comparison remains tied to its original base and protocol SHA; no search, fit, candidate evaluation, or held-out assessment was rerun.

Corrections derive solely from the saved stdout and traces:

- Candidate evaluation generation and the inspected population snapshot are now separate columns. Initialization evaluations at generation 0 are checked against the first recorded population snapshot, generation 1. Offspring evaluated at generation `g > 0` are checked against snapshot `g`.
- Snapshot membership means the fitted expression's exact rendered form is present in the inspected snapshot. It does not claim candidate lineage.
- `(4,2)`, seed 76001: the capable initialization candidate was evaluated at generation 0, its first recorded snapshot is generation 1, and the fitted expression is absent there. It remained at its starting literals, training MSE 66.738, stop `minimum_step`.
- `(2,3)`, seed 76005: the first capable offspring was evaluated at generation 4 and checked against snapshot 4. Its fitted expression is present there; it remained at its starting literals, training MSE 9.62193, stop `minimum_step`. It was not the population's selected best.
- Derived result tables consistently use `search_elapsed_seconds`. The original raw stdout's key `search_cpu_seconds` is preserved unchanged; it held a `steady_clock` elapsed measurement. Process CPU time was not collected.
- The working launcher now writes only per-run stdout and traces; the offline analyzer creates valid derived CSVs from the saved stdout. The executed launcher, executed runner, raw stdout, and raw traces are hash-locked in `IMMUTABLE_SEARCH_ARTIFACTS.sha256`.

`verify_results.py` checks the mapping for all initialization and offspring records, snapshot availability and expression membership, all 32 held-out decisions, paired gains/losses, capability counts, and exact positive coefficient witnesses. It also verifies that raw/executed artifacts still match their pre-correction hashes.
