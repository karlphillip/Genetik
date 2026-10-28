# Genetik 0.6.0 delivery proposal (approval required)

The release candidate is a development pre-release focused on optional native
Python interoperability. The authorized qualification branch is
`qualification/python-interoperability-i2`. The exact candidate SHA and final
five-job hosted workflow result must be taken from the final completion
packet after the evidence commit has passed all checks.

## Proposed integration

1. Open a pull request from `qualification/python-interoperability-i2` into
   `main`, summarizing the native regression facade, optional pybind11
   extension, `GenetikRegressor`, NumPy input, SymPy export, SRBench adapter,
   migration notes, qualified platforms and limitations.
2. After review/approval, fast-forward integrate the exact hosted-qualified
   candidate into `main` if ancestry permits. Do not squash or add an
   unqualified merge commit. Verify the required matrix on `main` at the exact
   candidate SHA.
3. After main-branch validation and separate approval, create an annotated
   `v0.6.0` tag at the qualified commit.
4. Prepare a GitHub development pre-release titled
   **Genetik 0.6.0 — Python Interoperability Development Pre-release** using
   reviewed notes and only assets whose source commit, wheel tags, contents
   and SHA256 values match the tag.

The release notes must describe Genetik as a general-purpose C++20 genetic
algorithm framework with optional symbolic-regression and Python interfaces.
They must preserve documented mathematical limitations, cooperative (not
hard real-time) time limits, lack of fitted-estimator pickle support, and
host-specific wheel compatibility. No search-performance superiority claim
is proposed.

## Artifact policy

Build `Genetik-0.6.0-source.tar.gz` from the exact release tag tree. Build the
canonical Python sdist as `genetik-0.6.0.tar.gz`. Keep the qualified wheels
host-tagged `cp312-cp312-linux_x86_64` and
`cp312-cp312-linux_aarch64`; do not relabel them manylinux. Recalculate and
independently verify every SHA256 after obtaining the final tagged-source
artifacts. The earlier artifact hashes in `I2_QUALIFICATION.md` are explicitly
historical and must not be reused for later source bytes.

## Not authorized by this proposal

This packet does not itself authorize PR creation, main integration, tag
creation/push, GitHub release publication, or PyPI/TestPyPI upload. Request
separate approval for PR/integration/main validation/tag/publication after
presenting the exact candidate SHA, hosted run, source archive, wheels, sdist,
checksums and release notes. PyPI and TestPyPI remain separate channels and
are not proposed here.
