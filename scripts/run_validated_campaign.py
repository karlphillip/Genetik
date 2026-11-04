#!/usr/bin/env python3
"""Run a campaign only after its standalone benchmark manifest passes preflight."""
import argparse
import hashlib
import json
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
    p.add_argument("--require-runner-protocol-identity", action="store_true",
                   help="require runner handshake against the exact validated manifest before execution")
    p.add_argument("runner_args", nargs=argparse.REMAINDER)
    a = p.parse_args()
    if not pathlib.Path(a.validator).is_file() or not pathlib.Path(a.runner).exists():
        p.error("validator or campaign runner is missing")
    check = subprocess.run([sys.executable, a.preflight, "--validator", a.validator,
                            "--manifest", a.manifest, "--output", a.preflight_output])
    if check.returncode:
        print("campaign not started: benchmark preflight rejected the manifest", file=sys.stderr)
        return check.returncode
    try:
        result = json.loads(pathlib.Path(a.preflight_output).read_text())
    except (OSError, json.JSONDecodeError) as error:
        print(f"campaign not started: cannot read preflight result: {error}", file=sys.stderr)
        return 2
    if result.get("status") != "domain_validated":
        print("campaign not started: declared observations were not domain-validated", file=sys.stderr)
        return 2
    manifest_path = pathlib.Path(a.manifest).resolve()
    manifest_sha = hashlib.sha256(manifest_path.read_bytes()).hexdigest()
    if result.get("manifest_sha256") != manifest_sha:
        print("campaign not started: manifest changed after preflight", file=sys.stderr)
        return 2
    command = ([sys.executable, a.runner] if pathlib.Path(a.runner).suffix == ".py" else [a.runner])
    runner_args = a.runner_args[1:] if a.runner_args[:1] == ["--"] else a.runner_args
    if a.require_runner_protocol_identity:
        # The wrapper can only establish protocol consistency for a cooperating
        # runner that reports the identity of the configuration it will use.
        handshake = subprocess.run(
            [*command, "--genetik-protocol-identity", str(manifest_path)],
            text=True, capture_output=True)
        if handshake.returncode or handshake.stdout.strip() != manifest_sha:
            print("campaign not started: runner does not confirm the validated protocol identity",
                  file=sys.stderr)
            return 2
    return subprocess.run([*command, *runner_args]).returncode


if __name__ == "__main__":
    raise SystemExit(main())
