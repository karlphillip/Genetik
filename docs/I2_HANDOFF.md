# I2 qualification handoff

Phase: I2 local 0.6.0 release-candidate qualification. No remote writes,
publication, PyPI/TestPyPI upload, merge, tag, or release is authorized.

Branch: `qualification/python-interoperability-i2`.
Source starting point: I1 `83c07793b3410b01101003d9626d954f4db961ab`, direct
child of published 0.5.0 `f95a204e25a6de36a54230be0cba750bde5e2ff7`.
Current branch tip before the final evidence commit is
`93e89fa296511ffb4136158214d270becd2f2d77`. The implementation and packaging
candidate are committed; pending local changes are the qualification packet,
artifact manifest, and retained logs/checksums. They contain no executable
source changes. The branch does not incorporate R1–R8 or the reverted P1
implementation.

Completed final-source gates: CMake 3.16.0 Release, Debug and ASan+UBSan each
passed the portable 92/92 suite. Required TSan selections passed three
repetitions (63 executions) and additional discrete/checkpoint, D5, D6 and
integral contracts passed. CPython 3.12.3 AArch64 installed wheel and sdist
each passed 20 tests. The pinned SRBench layout/import and bounded evaluator
smoke passed after repairing the timer's dangling deadline capture. Raw output
and original failures are retained under `docs/i2-results/`.

Source candidate: `93e89fa296511ffb4136158214d270becd2f2d77`. Its native source
archive, AArch64 wheel and Python sdist are in
`out/tmp/i2-artifacts/final-delivery/` with a separate SHA256SUMS; their manifest
and checksum sidecar are in `docs/i2-results/`. The sdist is reproducible
across two builds. The full
scikit-learn estimator check suite has documented unsupported contracts.
Outstanding hosted gate: x86-64 CPython 3.12 wheel/native matrix. Windows/macOS
and other Python minors remain unqualified. The wheel is host-tagged
`cp312-cp312-linux_aarch64`, not retagged manylinux. Run final audits, commit
the completed artifact packet, leave clean, and request only exact hosted
qualification upload/matrix authorization. Never touch external `session.md`.
