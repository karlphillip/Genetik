#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${CONDA_PREFIX:-}" ]]; then
  echo "CONDA_PREFIX is required by the SRBench installation contract" >&2
  exit 2
fi
if [[ -z "${GENETIK_SOURCE_DIR:-}" || ! -f "${GENETIK_SOURCE_DIR}/pyproject.toml" ]]; then
  echo "Set GENETIK_SOURCE_DIR to a checkout of the qualified Genetik I1 source" >&2
  exit 2
fi
python -m pip install --no-deps --no-build-isolation "${GENETIK_SOURCE_DIR}"
