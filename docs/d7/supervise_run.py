#!/usr/bin/env python3
"""Run one benchmark process with serial CPU affinity and measured resource caps."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time


def process_group_metrics(group):
    rss = cpu = threads = 0
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            stat = (entry / "stat").read_text()
            fields = stat[stat.rfind(")") + 2 :].split()
            if int(fields[2]) != group:  # pgrp is field 5, fields start at field 3
                continue
            cpu += int(fields[11]) + int(fields[12])  # utime + stime, clock ticks
            for line in (entry / "status").read_text().splitlines():
                if line.startswith("VmRSS:"):
                    rss += int(line.split()[1])
                elif line.startswith("Threads:"):
                    threads += int(line.split()[1])
        except (FileNotFoundError, ProcessLookupError, PermissionError, ValueError, IndexError):
            continue
    return rss, cpu / os.sysconf("SC_CLK_TCK"), threads


def stop_group(process, sig):
    try:
        os.killpg(process.pid, sig)
    except ProcessLookupError:
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--wall-cap", type=float, required=True)
    parser.add_argument("--memory-cap-gib", type=float, default=6.0)
    parser.add_argument("--cpu", type=int, default=0)
    parser.add_argument("--stdout", type=Path, required=True)
    parser.add_argument("--stderr", type=Path, required=True)
    parser.add_argument("--result", type=Path, required=True)
    parser.add_argument("--cwd", type=Path, default=Path.cwd())
    parser.add_argument("--env", action="append", default=[], metavar="KEY=VALUE")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command and args.command[0] == "--" else args.command
    if not command or args.wall_cap <= 0 or args.memory_cap_gib <= 0:
        parser.error("provide positive limits and a command after --")
    env = os.environ.copy()
    env.update({"OMP_NUM_THREADS": "1", "OPENBLAS_NUM_THREADS": "1", "MKL_NUM_THREADS": "1"})
    for item in args.env:
        key, sep, value = item.partition("=")
        if not sep or not key:
            parser.error(f"invalid --env value {item!r}")
        env[key] = value
    args.stdout.parent.mkdir(parents=True, exist_ok=True)
    args.stderr.parent.mkdir(parents=True, exist_ok=True)
    args.result.parent.mkdir(parents=True, exist_ok=True)
    actual_command = ["taskset", "-c", str(args.cpu), *command]
    start = time.monotonic()
    reason = "completed"
    peak_rss = cpu_seconds = 0.0
    peak_threads = 0
    with args.stdout.open("wb") as stdout, args.stderr.open("wb") as stderr:
        try:
            process = subprocess.Popen(actual_command, cwd=args.cwd, env=env, stdout=stdout,
                                       stderr=stderr, start_new_session=True)
        except OSError as exc:
            result = {
                "command": actual_command,
                "cwd": str(args.cwd.resolve()),
                "cpu_affinity": [args.cpu],
                "wall_cap_seconds": args.wall_cap,
                "memory_cap_gib": args.memory_cap_gib,
                "outcome": "launch_failure",
                "exit_code": 127,
                "wall_seconds": time.monotonic() - start,
                "cpu_seconds": 0.0,
                "peak_rss_gib": 0.0,
                "peak_process_threads": 0,
                "launch_error": f"{type(exc).__name__}: {exc}",
                "stdout": str(args.stdout),
                "stderr": str(args.stderr),
            }
            args.result.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
            print(json.dumps(result, sort_keys=True))
            return 1
        deadline = start + args.wall_cap
        while process.poll() is None:
            rss_kb, cpu, threads = process_group_metrics(process.pid)
            peak_rss = max(peak_rss, rss_kb / 1024.0 / 1024.0)
            cpu_seconds = max(cpu_seconds, cpu)
            peak_threads = max(peak_threads, threads)
            if rss_kb * 1024 > args.memory_cap_gib * (1024**3):
                reason = "memory_limit"
                stop_group(process, signal.SIGTERM)
                break
            if time.monotonic() >= deadline:
                reason = "wall_timeout"
                stop_group(process, signal.SIGTERM)
                break
            time.sleep(0.05)
        try:
            exit_code = process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            stop_group(process, signal.SIGKILL)
            exit_code = process.wait()
        elapsed = time.monotonic() - start
        rss_kb, cpu, threads = process_group_metrics(process.pid)
        peak_rss = max(peak_rss, rss_kb / 1024.0 / 1024.0)
        cpu_seconds = max(cpu_seconds, cpu)
        peak_threads = max(peak_threads, threads)
    result = {
        "command": actual_command,
        "cwd": str(args.cwd.resolve()),
        "cpu_affinity": [args.cpu],
        "worker_env": {"OMP_NUM_THREADS": env["OMP_NUM_THREADS"],
                       "OPENBLAS_NUM_THREADS": env["OPENBLAS_NUM_THREADS"],
                       "MKL_NUM_THREADS": env["MKL_NUM_THREADS"]},
        "wall_cap_seconds": args.wall_cap,
        "memory_cap_gib": args.memory_cap_gib,
        "outcome": reason,
        "exit_code": exit_code,
        "wall_seconds": elapsed,
        "cpu_seconds": cpu_seconds,
        "peak_rss_gib": peak_rss,
        "peak_process_threads": peak_threads,
        "stdout": str(args.stdout),
        "stderr": str(args.stderr),
    }
    args.result.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, sort_keys=True))
    return 0 if reason == "completed" and exit_code == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
