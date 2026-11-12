#!/usr/bin/env python3
"""Run one frozen campaign/final coordinator phase from a pinned protocol."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from research_execution import GateError, execute_continuation_phase, read_frozen_program, sha256_file


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--protocol-dir", required=True, type=Path)
    parser.add_argument("--phase-id", required=True,
                        choices=("campaign_orchestration", "final_scoring_orchestration"))
    args = parser.parse_args(argv)
    try:
        _, config, _ = read_frozen_program(args.protocol_dir)
        plan = config.get("continuation_plan")
        if plan is None:
            raise GateError("protocol does not declare a bounded continuation")
        pin = plan.get("coordinator_entrypoint")
        if not isinstance(pin, dict) or not isinstance(pin.get("path"), str):
            raise GateError("frozen protocol does not pin this coordinator entrypoint")
        actual_path = Path(__file__).resolve()
        if Path(pin["path"]).resolve() != actual_path or sha256_file(actual_path) != pin.get("sha256"):
            raise GateError("continuation coordinator entrypoint hash/path mismatch")
        result = execute_continuation_phase(args.protocol_dir, args.phase_id)
        sys.stdout.write(json.dumps(result, sort_keys=True, separators=(",", ":")) + "\n")
        return 0
    except (GateError, OSError, ValueError) as error:
        sys.stderr.write(f"continuation phase failed: {type(error).__name__}: {error}\n")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
