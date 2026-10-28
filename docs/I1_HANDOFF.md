# I1 handoff

- Phase: I1 Python interoperability, local completion.
- Branch: `feature/python-interoperability-i1`.
- Source baseline: published 0.5.0 commit `f95a204e25a6de36a54230be0cba750bde5e2ff7`.
- Implementation: optional pybind11 extension and estimator call the existing C++ symbolic-regression facade; no new search policy.
- Experiments/checks: focused Python/native parity, package install from wheel and sdist, pinned SRBench layout/import/evaluator smoke, targeted Release/Debug/ASan+UBSan/TSan gates. See `I1_QUALIFICATION.md` and checksummed `i1-results/`.
- Active processes: none at handoff.
- Evidence: `docs/i1-results/`; artifact identities in `artifact-manifest.json`; all retained evidence files are checksummed by `SHA256SUMS`.
- Limits: Python 3.12.3/Linux AArch64 only; no full SRBench campaign or full sklearn estimator checks. Parent hygiene reports external `../session.md`; preserved unchanged.
- Next permitted action: finish the local I1 commit and report; after that, prepare a separate release integration/qualification proposal.
- Authorization boundary: no push, PR, merge, tag, publication, or Genetik 0.6.0 preparation is authorized by I1.
- Current final commit: resolve with `git rev-parse HEAD` after the local completion commit; verify a clean worktree.
