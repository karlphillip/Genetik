#!/usr/bin/env python3
"""Run one frozen campaign/final coordinator phase in a fresh supervised worker."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

SCRIPT = Path(__file__).resolve()
WORKER_MARKER = "GENETIK_COORDINATOR_WORKER_TOKEN"
COORDINATOR_MEMORY_LIMIT_BYTES = 512 * 1024 * 1024
_WORKER_ENTRY_CPU_SECONDS = time.process_time()
_WORKER_ENTRY_MONOTONIC_NS = time.monotonic_ns()


def _worker_main(argv: list[str] | None = None) -> int:
    # Defense in depth: the supervisor sets this before exec, and the worker
    # repeats it before importing the coordinator implementation. This keeps a
    # forged marker/handshake from moving project imports outside the cap.
    import resource
    soft, hard = resource.getrlimit(resource.RLIMIT_AS)
    cap = min(COORDINATOR_MEMORY_LIMIT_BYTES, hard) if hard != resource.RLIM_INFINITY else COORDINATOR_MEMORY_LIMIT_BYTES
    resource.setrlimit(resource.RLIMIT_AS, (cap, cap))
    if resource.getrlimit(resource.RLIMIT_AS) != (cap, cap):
        sys.stderr.write("coordinator worker could not enforce address-space limit\n")
        return 78
    if not _consume_worker_nonce():
        sys.stderr.write("coordinator worker requires a verified inherited nonce\n")
        return 78
    from research_execution import (GateError, execute_continuation_phase, read_frozen_program,
                                    register_coordinator_entry_baseline, sha256_file)
    register_coordinator_entry_baseline(_WORKER_ENTRY_CPU_SECONDS,
                                        _WORKER_ENTRY_MONOTONIC_NS)

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
        _verify_supervisor_source(plan.get("coordinator_supervisor_entrypoint"))
        pin = plan.get("coordinator_entrypoint")
        if not isinstance(pin, dict) or not isinstance(pin.get("path"), str):
            raise GateError("frozen protocol does not pin this coordinator entrypoint")
        if Path(pin["path"]).resolve() != SCRIPT or sha256_file(SCRIPT) != pin.get("sha256"):
            raise GateError("continuation coordinator entrypoint hash/path mismatch")
        result = execute_continuation_phase(args.protocol_dir, args.phase_id)
        sys.stdout.write(json.dumps(result, sort_keys=True, separators=(",", ":")) + "\n")
        return 0
    except (GateError, OSError, ValueError) as error:
        sys.stderr.write(f"continuation phase failed: {type(error).__name__}: {error}\n")
        return 2


def _supervised_main(argv: list[str] | None = None) -> int:
    supervisor_started_mono = time.monotonic()
    supervisor_started_cpu = time.process_time()
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--protocol-dir", required=True, type=Path)
    parser.add_argument("--phase-id", required=True,
                        choices=("campaign_orchestration", "final_scoring_orchestration"))
    args, _ = parser.parse_known_args(argv)
    phase_ready = False
    reconciliation_attempted = False
    launcher_limit_error = None
    import resource
    try:
        _, hard = resource.getrlimit(resource.RLIMIT_AS)
        cap = (min(COORDINATOR_MEMORY_LIMIT_BYTES, hard)
               if hard != resource.RLIM_INFINITY else COORDINATOR_MEMORY_LIMIT_BYTES)
        resource.setrlimit(resource.RLIMIT_AS, (cap, cap))
        if resource.getrlimit(resource.RLIMIT_AS) != (cap, cap):
            raise RuntimeError("coordinator launcher could not enforce address-space limit")
    except (OSError, ValueError, RuntimeError) as error:
        launcher_limit_error = error
    sys.path.insert(0, str(SCRIPT.parent))
    from research_execution import (CONTINUATION_MEMORY_BYTES, GateError,
                                    read_frozen_program, record_supervised_coordinator_worker)

    try:
        _, config, _ = read_frozen_program(args.protocol_dir)
        plan = config.get("continuation_plan")
        if not isinstance(plan, dict) or plan.get("overall_deadline_seconds", 0) <= 0:
            raise GateError("frozen coordinator wall/memory budget is unavailable")
        phase_ready = True
        if launcher_limit_error is not None:
            raise GateError(f"coordinator launcher limit admission failed: {launcher_limit_error}")
        _verify_supervisor_source(plan.get("coordinator_supervisor_entrypoint"))
        from coordinator_supervisor import (CoordinatorSupervisorError,
                                            account_supervisor_exception,
                                            attach_supervisor_accounting,
                                            limit_current_process, supervise)
        limit_current_process(COORDINATOR_MEMORY_LIMIT_BYTES)
        state = json.loads((config["ledger_dir"] / "process-budget.json").read_text())
        remaining = (plan["overall_deadline_seconds"] if state.get("overall_deadline_monotonic_ns") is None
                     else (state["overall_deadline_monotonic_ns"] - time.monotonic_ns()) / 1_000_000_000)
        if remaining <= 0:
            reconciliation_attempted = True
            record_supervised_coordinator_worker(args.protocol_dir, args.phase_id, None, SCRIPT)
            raise GateError("continuation wall allowance is exhausted before coordinator launch")
        env = os.environ.copy()
        env[WORKER_MARKER] = os.urandom(24).hex()
        try:
            result = supervise(
                [sys.executable, str(SCRIPT), "--protocol-dir", str(args.protocol_dir),
                 "--phase-id", args.phase_id],
                memory_limit_bytes=CONTINUATION_MEMORY_BYTES,
                wall_limit_seconds=remaining,
                output_limit_bytes=8 * 1024 * 1024,
                env=env,
                worker_handshake=True,
            )
        except BaseException as error:
            raw_measurement = getattr(error, "coordinator_measurement", None)
            worker_wall = raw_measurement.get("wall_seconds", 0.0) if isinstance(raw_measurement, dict) else 0.0
            failed_measurement = account_supervisor_exception(
                error, cpu_seconds=time.process_time() - supervisor_started_cpu,
                exclusive_wall_seconds=max(
                    0.0, time.monotonic() - supervisor_started_mono
                    - worker_wall),
                address_space_limit_bytes=COORDINATOR_MEMORY_LIMIT_BYTES,
            )
            reconciliation_attempted = True
            record_supervised_coordinator_worker(args.protocol_dir, args.phase_id,
                                                failed_measurement, SCRIPT)
            if not isinstance(error, (CoordinatorSupervisorError, OSError, subprocess.SubprocessError)):
                raise
            sys.stderr.write(f"coordinator supervisor failed: {error}\n")
            return 3
        measurement = dict(result)
        attach_supervisor_accounting(
            measurement,
            cpu_seconds=time.process_time() - supervisor_started_cpu,
            exclusive_wall_seconds=max(0.0, time.monotonic() - supervisor_started_mono
                                       - result["wall_seconds"]),
            address_space_limit_bytes=COORDINATOR_MEMORY_LIMIT_BYTES,
        )
        reconciliation_attempted = True
        receipt = record_supervised_coordinator_worker(args.protocol_dir, args.phase_id, measurement, SCRIPT)
        if receipt.get("status") != "verified":
            return 3
        sys.stdout.buffer.write(result["stdout"])
        sys.stderr.buffer.write(result["stderr"])
        if result["timed_out"] or result["output_limit_exceeded"]:
            return 3
        if result["returncode"] != 0:
            return result["returncode"] if result["returncode"] > 0 else 3
        return 0
    except (GateError, OSError, ValueError, RuntimeError, KeyError) as error:
        if phase_ready and not reconciliation_attempted:
            try:
                record_supervised_coordinator_worker(args.protocol_dir, args.phase_id, None, SCRIPT)
            except (GateError, OSError, ValueError, RuntimeError):
                pass
        sys.stderr.write(f"coordinator supervisor refused: {type(error).__name__}: {error}\n")
        return 3


def _consume_worker_nonce() -> bool:
    """Validate the inherited one-shot handshake without importing project code."""
    import hmac
    import select
    raw_fd = os.environ.get("GENETIK_COORDINATOR_WORKER_FD")
    expected_hex = os.environ.get("GENETIK_COORDINATOR_WORKER_NONCE")
    if not raw_fd or not raw_fd.isdecimal() or not expected_hex or len(expected_hex) != 64:
        return False
    fd = int(raw_fd)
    try:
        os.set_blocking(fd, False)
        payload = bytearray()
        deadline = time.monotonic() + 2.0
        while len(payload) <= 32 and time.monotonic() < deadline:
            ready, _, _ = select.select([fd], [], [], max(0.0, deadline - time.monotonic()))
            if not ready:
                break
            block = os.read(fd, 33 - len(payload))
            if not block:
                break
            payload.extend(block)
        os.close(fd)
        expected = bytes.fromhex(expected_hex)
        return len(payload) == 32 and hmac.compare_digest(bytes(payload), expected)
    except (OSError, ValueError):
        return False


def _verify_supervisor_source(pin: object) -> None:
    """Check the frozen helper digest using this already-pinned entrypoint."""
    import hashlib
    if not isinstance(pin, dict) or set(pin) != {"path", "sha256"}:
        raise RuntimeError("frozen protocol lacks coordinator supervisor source pin")
    helper = SCRIPT.with_name("coordinator_supervisor.py").resolve()
    if Path(pin["path"]).resolve() != helper:
        raise RuntimeError("coordinator supervisor path differs from frozen pin")
    if hashlib.sha256(helper.read_bytes()).hexdigest() != pin["sha256"]:
        raise RuntimeError("coordinator supervisor hash differs from frozen pin")


if __name__ == "__main__":
    if os.environ.get(WORKER_MARKER):
        raise SystemExit(_worker_main())
    raise SystemExit(_supervised_main())
