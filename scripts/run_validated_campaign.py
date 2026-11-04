#!/usr/bin/env python3
"""Run a campaign only after its standalone benchmark manifest passes preflight."""
import argparse
import pathlib
import subprocess
import sys


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--validator", required=True)
    p.add_argument("--manifest", required=True)
    p.add_argument("--preflight", required=True, help="scientific_benchmark_preflight.py")
    p.add_argument("--preflight-output", required=True)
    p.add_argument("--runner", required=True, help="campaign runner executable or script")
    p.add_argument("runner_args", nargs=argparse.REMAINDER)
    a = p.parse_args()
    if not pathlib.Path(a.validator).is_file() or not pathlib.Path(a.runner).exists():
        p.error("validator or campaign runner is missing")
    check = subprocess.run([sys.executable, a.preflight, "--validator", a.validator,
                            "--manifest", a.manifest, "--output", a.preflight_output])
    if check.returncode:
        print("campaign not started: benchmark preflight rejected the manifest", file=sys.stderr)
        return check.returncode
    command = ([sys.executable, a.runner] if pathlib.Path(a.runner).suffix == ".py" else [a.runner])
    runner_args = a.runner_args[1:] if a.runner_args[:1] == ["--"] else a.runner_args
    return subprocess.run([*command, *runner_args]).returncode


if __name__ == "__main__":
    raise SystemExit(main())
