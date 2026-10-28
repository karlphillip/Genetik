# I2 qualification handoff

Phase: hosted qualification of the Genetik 0.6.0 development pre-release.
Current branch: `qualification/python-interoperability-i2`.

The approved source started from published 0.5.0
`f95a204e25a6de36a54230be0cba750bde5e2ff7` and I1
`83c07793b3410b01101003d9626d954f4db961ab`. The Python validation repairs are
in `021ada54d7b6717fc9264167bffe1b1c41bf14bc`; workflow corrections are in
`da0e2c1d4348ece612c194f18178006bf8c6fdbf` and
`7411f2771973226317452533e533c43fee815ebb`.

Repair-validation run 40736957740 passed all five jobs at 7411f27. Its full
logs are retained under `docs/i2-results/logs/hosted-final-*.log.gz`. It is not
the final branch tip because this qualification packet updates documentation
and retained evidence. Next action: commit only the reviewed report/proposal,
qualification logs and refreshed checksums; push that commit to the same
authorized qualification branch; wait for and review the complete workflow
triggered by that exact tip. If it passes, stop and submit the exact delivery
proposal for separate approval. Do not create a PR, change `main`, tag, publish
or upload to a package index.

The final source archive must be regenerated from the exact evidence-commit
SHA after it is known, verified using `scripts/check_source_archive.py`, then
hashed. The exact-tip Python workflow produces platform wheels and the
canonical sdist; download them and verify hashes/metadata. Store generated
delivery assets outside the source tree in ignored `out/tmp/`.

No local test/benchmark process is active at this checkpoint. Do not modify
external user-owned `session.md`. Preserve all 127 frozen historical files,
published releases/tags, `main`, and prior research branches. No release or
package publication is authorized.
