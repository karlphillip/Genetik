# I2 hosted qualification proposal

No remote action has been taken. The release source candidate is
`93e89fa296511ffb4136158214d270becd2f2d77`. The final local branch tip will
contain only its artifact manifest/checksums and qualification evidence; the
hosted matrix must run on that exact final tip. No executable source changes
are included after the source candidate.

## Proposed source branch

`qualification/python-interoperability-i2`

## Native hosted matrix

Reuse the existing `.github/workflows/ci.yml` native jobs without relaxing
their numerical, sanitizer, manifest or concurrency contracts:

1. GCC 13, Linux x86-64, Release, exact CMake 3.16.0.
2. Clang 18, Linux x86-64, ASan+UBSan.
3. GCC 13, Linux ARM64, Release, with the existing separate TSan selections
   and repeated contracts.

The native matrix must pass on the uploaded final branch tip. Python checks do
not substitute for native qualification.

## Python hosted matrix

The `python-cp312` workflow matrix qualifies Linux x86-64 on `ubuntu-24.04` and
Linux AArch64 on `ubuntu-24.04-arm`, both CPython 3.12.3. It installs the pinned
requirements in `docs/i2-results/requirements-python-cp312.txt`, builds a
wheel, audits its metadata and shared dependencies, installs and tests it,
checks native/Python parity, runs the quickstart, validates the pinned SRBench
method layout/import and a short official evaluator smoke, then builds and
installs the source distribution. Artifacts and failure diagnostics are
retained by the workflow.

The pinned SRBench source revision is
`2aabbb60dad41d6e5d52779ce134191a874b756c`. The workflow uses no full SRBench
campaign and makes no ranking claim.

## Authorization requested after local preflight

Approval is requested only for these actions:

1. Push exact local candidate SHA to `qualification/python-interoperability-i2`.
2. Run the required hosted native and Python qualification matrix.
3. Make targeted repairs if a required gate exposes a defect, then requalify
   the exact repaired tip.
4. Push the resulting qualification evidence and reports.
5. Verify the exact final branch tip against all required hosted gates.

This does not request PyPI/TestPyPI uploads, PR creation, merging, tagging, or
publication. Those remain separate approval boundaries.
