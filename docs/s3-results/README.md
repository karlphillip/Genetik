# S3 retained evidence

The native build/test logs and test-name manifests describe the integrated
implementation whose product-code revision is `c52f026eb87c886a20f0d2d669f660d500504c5c`;
the only later changes before those runs were benchmark protocol/report
commits. The external target matrix was frozen before execution at protocol
commit `ee9d82f61bee26380b443698cd80a0e7f785d376`. Its machine-readable run
records, independent assessments, preflight outputs, PySR Hall-of-Fame files,
and all per-attempt stdout/stderr/supervisor records are under
`external-comparison/`.

`test-manifests/manifest.json` is derived from `ctest -N` for each named build
configuration. Logs ending in `.gz` are lossless gzip copies of the original
CTest/CMake diagnostic output; the bytes were round-trip verified. The
independent candidate extractor records Genetik best-so-far expressions and
PySR persisted Hall-of-Fame snapshots separately from process completion.

`SHA256SUMS` covers every other regular file under this directory at the
commit where the manifest was generated. It does not hash itself or release
artifacts. The native source archive, Python sdist, AArch64 wheel and their
artifact checksums are retained in the local `out/release/s3/` directory,
which is intentionally outside Git source history.
