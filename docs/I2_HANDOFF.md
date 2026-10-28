# I2 qualification handoff

Phase: I2 local 0.6.0 release-candidate qualification. No remote writes,
publication, PyPI/TestPyPI upload, merge, tag, or release is authorized.

Branch: `qualification/python-interoperability-i2`.
Source starting point: I1 `83c07793b3410b01101003d9626d954f4db961ab`, direct
child of published 0.5.0 `f95a204e25a6de36a54230be0cba750bde5e2ff7`.
Current uncommitted tree has version/checkpoint compatibility changes, Python
tests, CI additions, release docs, and qualification evidence. It intentionally
does not incorporate R1–R8 or the reverted P1 implementation.

Completed final-source gates: CMake 3.16.0 Release, Debug and ASan+UBSan each
passed the portable 92/92 suite. Required TSan selections passed three
repetitions (63 executions) and additional discrete/checkpoint, D5, D6 and
integral contracts passed. CPython 3.12.3 AArch64 installed wheel and sdist
each passed 20 tests. The pinned SRBench layout/import and bounded evaluator
smoke passed after repairing the timer's dangling deadline capture. Raw output
and original failures are retained under `docs/i2-results/`.

Outstanding: x86-64 Python wheel qualification requires hosted execution;
Windows/macOS and other Python minors are unqualified. Wheel is host-tagged
`cp312-cp312-linux_aarch64`, not manylinux. The full scikit-learn estimator
check suite has documented unsupported contracts. Final artifact hashes and
source archive identities must be generated after committing candidate source
and release docs. Run workspace hygiene and report its findings without
touching the external `session.md`. Then commit locally, leave clean, and
request only the exact hosted qualification upload/matrix authorization.
