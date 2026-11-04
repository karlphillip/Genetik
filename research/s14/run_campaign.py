#!/usr/bin/env python3
"""Sequential, resource-supervised S14 runner with a strict frozen protocol."""
import argparse
import hashlib
import json
import os
import pathlib
import signal
import subprocess
import sys
import time

EXPECTED_OPERATORS = ["+", "-", "*", "/", "powi", "sin"]
EXPECTED_NOISE_SEEDS = [14011, 14017, 14023]
EXPECTED_GA_SEEDS = [14101, 14103, 14107]
EXPECTED_RUNS = 192


def sha256(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def protocol_sha(path):
    return sha256(path)


def validate_protocol(path, source_sha=None):
    manifest_path = pathlib.Path(path).resolve()
    manifest = json.loads(manifest_path.read_text())
    campaign = manifest.get("campaign", {})
    if campaign.get("protocol_id") != "s14-multivariate-rational-confirmation-v1":
        raise ValueError("unexpected S14 protocol identity")
    if source_sha and campaign.get("source_sha") != source_sha:
        raise ValueError("manifest source SHA does not match the checked-out campaign source")
    expected_campaign = {
        "algorithmic_conditions": ["ga_coordinate", "ga_lm", "constructive_qr", "constructive_lm"],
        "ga_seeds": EXPECTED_GA_SEEDS,
        "constructive_runs_per_training_dataset": 2,
        "population_size": 32,
        "candidate_evaluation_limit": 512,
        "global_training_loss_attempt_limit": 12000,
        "generation_attempt_limit": 16,
        "worker_count": 1,
        "cpu_affinity": [0],
        "wall_seconds_per_run": 120,
        "process_tree_rss_cap_bytes": 6442450944,
        "virtual_address_space_cap_bytes": 6442450944,
    }
    for key, value in expected_campaign.items():
        if campaign.get(key) != value:
            raise ValueError(f"effective campaign configuration mismatch for {key}")
    if campaign.get("noise", {}).get("independent_python_random_seeds") != EXPECTED_NOISE_SEEDS:
        raise ValueError("noise seed declaration mismatch")
    tasks = manifest.get("tasks")
    if not isinstance(tasks, list) or len(tasks) != 24:
        raise ValueError("S14 protocol requires exactly 24 frozen task/noise configurations")
    ids = set()
    for task in tasks:
        if not isinstance(task, dict) or not isinstance(task.get("id"), str) or not task["id"] or task["id"] in ids:
            raise ValueError("task identifiers must be nonempty and unique")
        ids.add(task["id"])
        grammar = task.get("grammar", {})
        if grammar.get("variables") != 2 or grammar.get("operators") != EXPECTED_OPERATORS:
            raise ValueError(f"effective grammar mismatch for {task['id']}")
        for key, expected in (("max_nodes", 127), ("max_depth", 16),
                              ("max_arithmetic_operations", 2048)):
            if grammar.get(key) != expected:
                raise ValueError(f"effective grammar mismatch for {task['id']}: {key}")
        resources = task.get("resources", {})
        resource_map = {"population_size": 32, "candidate_evaluations": 512,
                        "training_loss_attempts": 12000, "generation_attempts": 16,
                        "worker_count": 1, "wall_seconds": 120,
                        "process_tree_rss_cap_bytes": 6442450944,
                        "virtual_address_space_cap_bytes": 6442450944,
                        "cpu_affinity": [0]}
        if resources != resource_map:
            raise ValueError(f"effective resource configuration mismatch for {task['id']}")
        fit = task.get("fitting", {})
        if fit.get("method") != "LevenbergMarquardt" or fit.get("lower") != -8 or fit.get("upper") != 8:
            raise ValueError(f"fitting configuration mismatch for {task['id']}")
        hashes = task.get("dataset_sha256", {})
        splits = task.get("domain_observations", [])
        if len(splits) != 3 or set(hashes) != {"training", "heldout", "extrapolation"}:
            raise ValueError(f"dataset declarations incomplete for {task['id']}")
        for split, relative in zip(("training", "heldout", "extrapolation"), splits):
            data = (manifest_path.parent / relative).resolve()
            if not data.is_file() or sha256(data) != hashes[split]:
                raise ValueError(f"dataset hash mismatch for {task['id']}:{split}")
    return manifest


def descendants(pid):
    result = [pid]
    todo = [pid]
    while todo:
        parent = todo.pop()
        try:
            children = pathlib.Path(f"/proc/{parent}/task/{parent}/children").read_text().split()
        except (OSError, FileNotFoundError):
            children = []
        for child in children:
            value = int(child)
            result.append(value)
            todo.append(value)
    return result


def resident_bytes(pid):
    total = 0
    for process in descendants(pid):
        try:
            for line in pathlib.Path(f"/proc/{process}/status").read_text().splitlines():
                if line.startswith("VmRSS:"):
                    total += int(line.split()[1]) * 1024
                    break
        except (OSError, ValueError, FileNotFoundError):
            pass
    return total


def set_limits(vas_bytes, cpus):
    import resource
    resource.setrlimit(resource.RLIMIT_AS, (vas_bytes, vas_bytes))
    os.sched_setaffinity(0, set(cpus))


def execute(command, wall_limit, rss_limit, vas_limit, cpus, stdout_path, stderr_path):
    start = time.monotonic()
    peak_sampled = 0
    timed_out = False
    rss_exceeded = False
    with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
        pid = os.fork()
        if pid == 0:
            try:
                os.setsid()
                set_limits(vas_limit, cpus)
                os.dup2(stdout.fileno(), 1)
                os.dup2(stderr.fileno(), 2)
                os.execv(command[0], command)
            except BaseException as error:
                os.write(2, f"runner child setup/exec failed: {error}\n".encode())
                os._exit(127)
        wait_status = None
        usage = None
        while wait_status is None:
            peak_sampled = max(peak_sampled, resident_bytes(pid))
            if peak_sampled > rss_limit:
                rss_exceeded = True
                try:
                    os.killpg(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            if time.monotonic() - start >= wall_limit:
                timed_out = True
                try:
                    os.killpg(pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                deadline = time.monotonic() + 2
                while time.monotonic() < deadline:
                    waited_pid, status, child_usage = os.wait4(pid, os.WNOHANG)
                    if waited_pid:
                        wait_status, usage = status, child_usage
                        break
                    time.sleep(.02)
                if wait_status is None:
                    try:
                        os.killpg(pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
            if wait_status is None:
                waited_pid, status, child_usage = os.wait4(pid, os.WNOHANG)
                if waited_pid:
                    wait_status, usage = status, child_usage
                else:
                    time.sleep(.02)
    return_code = os.waitstatus_to_exitcode(wait_status)
    elapsed = time.monotonic() - start
    maxrss = int(usage.ru_maxrss) * 1024 if usage is not None else None
    return {"return_code": return_code, "wall_seconds": elapsed,
            "cpu_user_seconds": usage.ru_utime if usage is not None else None,
            "cpu_system_seconds": usage.ru_stime if usage is not None else None,
            "peak_rss_bytes": maxrss if maxrss is not None else peak_sampled,
            "peak_rss_source": "isolated_wait4_child_maxrss" if maxrss is not None else "20ms_process_tree_sample",
            "peak_process_tree_sample_bytes": peak_sampled,
            "timed_out": timed_out, "rss_limit_exceeded": rss_exceeded}


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--genetik-protocol-identity":
        try:
            validate_protocol(sys.argv[2])
            print(protocol_sha(sys.argv[2]))
            return 0
        except (OSError, ValueError, json.JSONDecodeError) as error:
            print(f"protocol rejected: {error}", file=sys.stderr)
            return 2
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--preflight-output", required=True)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()
    try:
        manifest = validate_protocol(args.manifest, args.source_sha)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        raise SystemExit(f"runner refused invalid frozen protocol: {error}")
    manifest_path = pathlib.Path(args.manifest).resolve()
    manifest_hash = protocol_sha(manifest_path)
    preflight = json.loads(pathlib.Path(args.preflight_output).read_text())
    if preflight.get("status") != "domain_validated" or preflight.get("manifest_sha256") != manifest_hash:
        raise SystemExit("runner refused: manifest does not match successful domain preflight")
    binary = pathlib.Path(args.binary).resolve()
    if not binary.is_file():
        raise SystemExit("campaign binary missing")
    binary_hash = sha256(binary)
    out_root = pathlib.Path(args.output_dir).resolve()
    out_root.mkdir(parents=True, exist_ok=False)
    all_rows = []
    started = time.monotonic()
    for task in manifest["tasks"]:
        resources = task["resources"]
        cpus = resources["cpu_affinity"]
        limits = (float(resources["wall_seconds"]), resources["process_tree_rss_cap_bytes"],
                  resources["virtual_address_space_cap_bytes"], cpus)
        datasets = [str((manifest_path.parent / p).resolve()) for p in task["domain_observations"]]
        methods = [("constructive_qr", [0]), ("constructive_lm", [0]),
                   ("ga_coordinate", EXPECTED_GA_SEEDS), ("ga_lm", EXPECTED_GA_SEEDS)]
        for method, seeds in methods:
            for seed in seeds:
                identity = f"{task['id']}__{method}__seed{seed}"
                run_dir = out_root / identity
                run_dir.mkdir()
                command = [str(binary),
                           method, task["family"], task["track"], str(seed), *datasets]
                resources_run = execute(command, *limits, run_dir / "stdout.json", run_dir / "stderr.log")
                record = {"protocol_sha256": manifest_hash, "source_sha": args.source_sha,
                          "binary_sha256": binary_hash, "task_id": task["id"],
                          "family": task["family"], "track": task["track"], "noise_seed": task["noise_seed"],
                          "method": method, "seed": seed, "datasets": task["domain_observations"], **resources_run}
                output_text = (run_dir / "stdout.json").read_text().strip()
                try:
                    scientific = json.loads(output_text) if output_text else None
                except json.JSONDecodeError as error:
                    scientific = None
                    record["parse_error"] = str(error)
                record["result"] = scientific
                record["stopping_reason"] = (scientific or {}).get("stop", "incomplete_or_invalid_output")
                record["run_status"] = "completed" if resources_run["return_code"] == 0 and scientific else "failed"
                (run_dir / "record.json").write_text(json.dumps(record, indent=2, sort_keys=True)+"\n")
                all_rows.append(record)
    summary = {"protocol_sha256": manifest_hash, "source_sha": args.source_sha,
               "binary_sha256": binary_hash, "expected_runs": EXPECTED_RUNS,
               "recorded_runs": len(all_rows),
               "completed_runs": sum(r["run_status"] == "completed" for r in all_rows),
               "elapsed_wall_seconds": time.monotonic()-started, "runs": all_rows}
    (out_root / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True)+"\n")
    return 0 if len(all_rows) == EXPECTED_RUNS else 2


if __name__ == "__main__":
    raise SystemExit(main())
