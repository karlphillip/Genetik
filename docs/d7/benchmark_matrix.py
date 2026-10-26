#!/usr/bin/env python3
"""Serial, resumable D7 engineering preflight and frozen target-run driver."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "docs/d7/data"
LOGS = ROOT / "out/logs/d7"
PROTOCOL = ROOT / "docs/d7/BENCHMARK_PROTOCOL.md"
TASKS = [
    "polynomial_x3", "rational_x2", "trig_sine", "exponential_decay",
    "fractional_power_positive", "kinetic_energy_mv2",
]
TRACKS = ["clean", "noise01"]
SEEDS = [0, 1, 42]
FRAMEWORKS = ["Genetik", "PySR", "PyOperon"]


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def parse_adapter_output(framework, path):
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    if framework in ("PySR", "PyOperon"):
        for line in reversed(lines):
            try:
                result = json.loads(line)
                if isinstance(result, dict) and "expression" in result:
                    return result
            except json.JSONDecodeError:
                continue
        return None
    fields = {}
    for line in lines:
        key, sep, value = line.partition("=")
        if sep:
            fields[key] = value
    if "expression" not in fields:
        return None
    return {
        "framework": "Genetik",
        "seed": int(fields["seed"]),
        "expression": fields["expression"],
        "native_training_mse": fields.get("training_mse"),
        "native_candidate_evaluations": fields.get("candidate_evaluations"),
        "native_training_loss_attempts": fields.get("training_loss_attempts"),
        "native_training_row_attempts": fields.get("training_row_attempts"),
        "native_stop": fields.get("stop"),
        "fit_seconds": float(fields.get("wall_seconds", "0")),
    }


def adapter_command(framework, mode, task, track, seed, train, run_dir, genetik_binary):
    if framework == "Genetik":
        return [str(genetik_binary), str(train), str(seed), "60" if mode == "preflight" else "120"], []
    if framework == "PySR":
        cmd = [
            str(ROOT / "out/tmp/d7/venvs/pysr/bin/python"),
            str(ROOT / "docs/d7/run_pysr.py"), "--seed", str(seed),
            "--task", task, "--track", track,
            "--output-directory", str(run_dir / "pysr-artifacts"),
        ]
        if mode == "preflight":
            cmd.append("--preflight")
        else:
            cmd += ["--train", str(train)]
        env = [
            f"PYTHON_JULIAPKG_EXE={ROOT}/out/tmp/d7/runtimes/julia-1.10.3/bin/julia",
            f"PYTHON_JULIAPKG_PROJECT={ROOT}/out/tmp/d7/julia-project-1103",
            f"JULIA_DEPOT_PATH={ROOT}/out/tmp/d7/julia-depot",
            "PYTHON_JULIACALL_THREADS=auto",
        ]
        return cmd, env
    cmd = [
        str(ROOT / "out/tmp/d7/venvs/operon/bin/python"),
        str(ROOT / "docs/d7/run_operon.py"), "--seed", str(seed),
        "--task", task, "--track", track,
    ]
    if mode == "preflight":
        cmd.append("--preflight")
    else:
        cmd += ["--train", str(train)]
    return cmd, []


def run_one(framework, mode, task, track, seed, train, genetik_binary, protocol_sha, attempt):
    run_id = f"{framework}/{task}/{track}/seed-{seed}/attempt-{attempt:02d}"
    run_dir = LOGS / ("preflight" if mode == "preflight" else "benchmark_runs") / run_id
    run_dir.mkdir(parents=True, exist_ok=False)
    command, extra_env = adapter_command(framework, mode, task, track, seed, train, run_dir, genetik_binary)
    output_cap = 180.0 if mode == "preflight" else 120.0
    result_path = run_dir / "supervisor.json"
    supervisor = [
        sys.executable, str(ROOT / "docs/d7/supervise_run.py"),
        "--wall-cap", str(output_cap), "--memory-cap-gib", "6", "--cpu", "0",
        "--stdout", str(run_dir / "stdout.log"),
        "--stderr", str(run_dir / "stderr.log"),
        "--result", str(result_path), "--cwd", str(ROOT),
    ]
    for env in extra_env:
        supervisor += ["--env", env]
    supervisor += ["--", *command]
    started = time.monotonic()
    subprocess.run(supervisor, cwd=ROOT, check=False, stdout=subprocess.DEVNULL)
    elapsed = time.monotonic() - started
    resource = json.loads(result_path.read_text(encoding="utf-8"))
    adapter = parse_adapter_output(framework, run_dir / "stdout.log")
    success = (
        resource["outcome"] == "completed"
        and resource["exit_code"] == 0
        and adapter is not None
        and bool(str(adapter.get("expression", "")).strip())
    )
    record = {
        "protocol_sha256": protocol_sha,
        "candidate_head": git("rev-parse", "HEAD"),
        "framework": framework,
        "task": task,
        "track": track,
        "seed": seed,
        "attempt": attempt,
        "status": "completed" if success else "failed_or_timeout",
        "adapter": adapter,
        "supervisor": resource,
        "driver_wall_seconds": elapsed,
        "run_directory": str(run_dir.relative_to(ROOT)),
    }
    return record


def append_record(path, record):
    with path.open("a", encoding="utf-8") as f:
        f.write(json.dumps(record, sort_keys=True, allow_nan=False) + "\n")
        f.flush()
        os.fsync(f.fileno())


def fixed_protocol_check(sha):
    actual = hashlib.sha256(PROTOCOL.read_bytes()).hexdigest()
    if actual != sha:
        raise SystemExit(f"protocol SHA mismatch: requested {sha}, actual {actual}")
    committed = subprocess.run(
        ["git", "show", f"HEAD:{PROTOCOL.relative_to(ROOT)}"],
        cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=False,
    )
    if committed.returncode != 0 or committed.stdout != PROTOCOL.read_bytes():
        raise SystemExit("protocol must be committed and identical to HEAD before running")


def read_jsonl(path):
    if not path.exists():
        return []
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["preflight", "targets"], required=True)
    parser.add_argument("--protocol-sha256", required=True)
    parser.add_argument("--genetik-binary", type=Path, required=True)
    parser.add_argument("--framework", choices=FRAMEWORKS, action="append")
    parser.add_argument("--retry-failed", action="store_true")
    args = parser.parse_args()
    fixed_protocol_check(args.protocol_sha256)
    frameworks = args.framework or FRAMEWORKS
    if not args.genetik_binary.is_file():
        parser.error("Genetik adapter binary does not exist")

    LOGS.mkdir(parents=True, exist_ok=True)
    preflight_file = LOGS / "preflight-results.jsonl"
    target_file = LOGS / "benchmark-candidates.jsonl"
    output = preflight_file if args.mode == "preflight" else target_file
    existing = read_jsonl(output)
    if args.mode == "targets":
        preflights = read_jsonl(preflight_file)
        successful = {r["framework"] for r in preflights if r.get("status") == "completed" and r.get("protocol_sha256") == args.protocol_sha256}
        missing = set(frameworks) - successful
        if missing:
            raise SystemExit(f"targets blocked: no successful frozen preflight for {sorted(missing)}")

    smoke = LOGS / "preflight" / "neutral-linear-smoke.csv"
    if args.mode == "preflight" and not smoke.exists():
        smoke.parent.mkdir(parents=True, exist_ok=True)
        with smoke.open("w", encoding="utf-8") as f:
            f.write("x0,y\n")
            for i in range(64):
                x = -1.0 + 2.0 * i / 63
                f.write(f"{x:.17g},{x + 0.25:.17g}\n")

    done = {
        (r["framework"], r.get("task"), r.get("track"), int(r["seed"]))
        for r in existing if not (args.retry_failed and r.get("status") != "completed")
    }
    protocol_sha = args.protocol_sha256
    for framework in frameworks:
        if args.mode == "preflight":
            cells = [("engineering_smoke", "clean", 42, smoke)]
        else:
            cells = [
                (task, track, seed, DATA / f"{task}-train-{('clean' if track == 'clean' else 'noise01')}.csv")
                for task in TASKS for track in TRACKS for seed in SEEDS
            ]
        for task, track, seed, train in cells:
            key = (framework, task, track, seed)
            if key in done:
                continue
            prior = [r.get("attempt", 0) for r in existing if (r["framework"], r.get("task"), r.get("track"), int(r["seed"])) == key]
            attempt = max(prior, default=-1) + 1
            record = run_one(framework, args.mode, task, track, seed, train, args.genetik_binary,
                             protocol_sha, attempt)
            append_record(output, record)
            existing.append(record)
            print(json.dumps({k: record[k] for k in ("framework", "task", "track", "seed", "attempt", "status")}, sort_keys=True), flush=True)


if __name__ == "__main__":
    main()
