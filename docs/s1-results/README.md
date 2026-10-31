# S1 evidence files

`fixed-structure.tsv` contains the final scalar and D6 fixed-topology fitting
comparison, with both process CPU and wall seconds. `evolutionary.tsv` contains
the final two-seed coordinate/LM comparison and candidate/work statistics.

Earlier completed outputs are retained with `pre-*` names. In particular,
`evolutionary-pre-rank-diagnostic.tsv` records the initial LM implementation
before the undamped rank/conditioning guard; it is diagnostic history, not the
final comparison. The `pre-final-fitter-fix` files retain the run before a
correctness fix to loss bookkeeping when an otherwise valid point had no
available derivative. The final comparisons are the unqualified `fixed-structure.tsv`
and `evolutionary.tsv` files. The initial invalid-config launch is documented
separately and did not produce a valid run.

`qualification.txt` records completed engineering checks and limitations.
`SHA256SUMS` covers every regular evidence file in this directory except the
manifest itself.
