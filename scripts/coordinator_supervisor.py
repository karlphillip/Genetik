#!/usr/bin/env python3
"""Run one coordinator in a fresh, address-space-limited process.

The worker starts only after RLIMIT_AS is installed by the parent before exec.
The measured resident high-water mark is for this one worker process; it is not
an aggregate process-tree RSS bound.
"""

from __future__ import annotations

import os
import secrets
import select
import selectors
import signal
import subprocess
import sys
import time
import resource
import hashlib
from typing import Sequence
from pathlib import Path


class CoordinatorSupervisorError(RuntimeError):
    pass


COORDINATOR_MEMORY_LIMIT_BYTES = 512 * 1024 * 1024


def process_starttime(pid: int) -> int:
    """Return Linux /proc starttime ticks, guarding PID reuse."""
    try:
        text = Path(f"/proc/{pid}/stat").read_text(encoding="ascii")
        tail = text[text.rfind(")") + 2 :].split()
        # tail begins at field 3 (state); starttime is field 22.
        return int(tail[19])
    except (OSError, ValueError, IndexError) as error:
        raise CoordinatorSupervisorError("worker identity measurement unavailable") from error


def rss_bytes_from_wait4(usage) -> int:
    if not sys.platform.startswith("linux"):
        raise CoordinatorSupervisorError("wait4 RSS units are not qualified on this platform")
    value = getattr(usage, "ru_maxrss", None)
    if isinstance(value, bool) or not isinstance(value, (int, float)) or value <= 0:
        raise CoordinatorSupervisorError("worker RSS high-water measurement unavailable")
    return int(value * 1024)


def proc_memory_status(pid: int) -> tuple[int, int]:
    """Read the worker's own VmHWM and VmRSS while its /proc entry exists."""
    values = {}
    try:
        for line in Path(f"/proc/{pid}/status").read_text(encoding="ascii").splitlines():
            if line.startswith(("VmHWM:", "VmRSS:")):
                key, raw, unit = line.split()
                if unit != "kB":
                    raise ValueError("unexpected /proc memory unit")
                values[key[:-1]] = int(raw) * 1024
    except (OSError, ValueError) as error:
        raise CoordinatorSupervisorError("live worker /proc memory measurement unavailable") from error
    if set(values) != {"VmHWM", "VmRSS"} or values["VmHWM"] <= 0 or values["VmRSS"] <= 0:
        raise CoordinatorSupervisorError("live worker /proc memory measurement incomplete")
    return values["VmHWM"], values["VmRSS"]


def _set_child_address_space(limit_bytes: int) -> None:
    soft, hard = resource.getrlimit(resource.RLIMIT_AS)
    if hard != resource.RLIM_INFINITY and hard < limit_bytes:
        raise OSError("inherited RLIMIT_AS hard limit is below the frozen worker limit")
    resource.setrlimit(resource.RLIMIT_AS, (limit_bytes, limit_bytes))
    applied = resource.getrlimit(resource.RLIMIT_AS)
    if applied != (limit_bytes, limit_bytes):
        raise OSError("worker RLIMIT_AS could not be verified")


def _retain_bounded_output(chunks: dict[str, bytearray], stream: str,
                           data: bytes, limit: int) -> bool:
    """Keep at most `limit` combined bytes and report any discarded output."""
    retained = sum(map(len, chunks.values()))
    remaining = max(0, limit - retained)
    chunks[stream].extend(data[:remaining])
    return len(data) > remaining


def limit_current_process(limit_bytes: int = COORDINATOR_MEMORY_LIMIT_BYTES) -> tuple[int, int]:
    """Bound the lightweight launcher separately from its worker."""
    _set_child_address_space(limit_bytes)
    hwm, rss = proc_memory_status(os.getpid())
    if hwm > limit_bytes or rss > limit_bytes:
        raise CoordinatorSupervisorError("launcher already exceeds its observed RSS cap")
    return hwm, rss


def current_process_peak_rss_bytes() -> tuple[int, int, int]:
    """Return inherited-prone ru_maxrss, this image's VmHWM, and current VmRSS."""
    usage_peak = int(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024)
    vm_hwm, vm_rss = proc_memory_status(os.getpid())
    if usage_peak <= 0:
        raise CoordinatorSupervisorError("supervisor inherited ru_maxrss diagnostic is unavailable")
    return usage_peak, vm_hwm, vm_rss


def consume_worker_handshake() -> bool:
    """Consume the inherited one-shot nonce pipe supplied to a supervised worker."""
    raw_fd = os.environ.get("GENETIK_COORDINATOR_WORKER_FD")
    expected_hex = os.environ.get("GENETIK_COORDINATOR_WORKER_NONCE")
    if (not raw_fd or not raw_fd.isdecimal() or not expected_hex
            or len(expected_hex) != 64):
        return False
    fd = int(raw_fd)
    try:
        os.set_blocking(fd, False)
        nonce = bytearray()
        deadline = time.monotonic() + 2.0
        while len(nonce) <= 32 and time.monotonic() < deadline:
            ready, _, _ = select.select([fd], [], [], max(0.0, deadline - time.monotonic()))
            if not ready:
                break
            block = os.read(fd, 33 - len(nonce))
            if not block:
                break
            nonce.extend(block)
        os.close(fd)
    except (OSError, ValueError):
        return False
    try:
        expected = bytes.fromhex(expected_hex)
    except ValueError:
        return False
    return len(nonce) == 32 and secrets.compare_digest(bytes(nonce), expected)


def verify_supervisor_pin(pin: object) -> None:
    """Require the helper executing this function to match the frozen protocol pin."""
    if not isinstance(pin, dict) or set(pin) != {"path", "sha256"}:
        raise CoordinatorSupervisorError("frozen protocol lacks a coordinator supervisor pin")
    actual_path = Path(__file__).resolve()
    try:
        expected_path = Path(pin["path"]).resolve()
        digest = hashlib.sha256(actual_path.read_bytes()).hexdigest()
    except (OSError, TypeError, ValueError) as error:
        raise CoordinatorSupervisorError("coordinator supervisor pin cannot be checked") from error
    if expected_path != actual_path or pin.get("sha256") != digest:
        raise CoordinatorSupervisorError("coordinator supervisor path/hash differs from frozen pin")


def supervise(argv: Sequence[str], *, memory_limit_bytes: int,
              wall_limit_seconds: float, output_limit_bytes: int,
              env: dict[str, str] | None = None,
              worker_handshake: bool = False) -> dict:
    """Run a fresh worker with pre-exec address-space and bounded IO/time."""
    if (not argv or type(memory_limit_bytes) is not int or memory_limit_bytes <= 0
            or not isinstance(wall_limit_seconds, (int, float)) or wall_limit_seconds <= 0
            or type(output_limit_bytes) is not int or output_limit_bytes <= 0):
        raise ValueError("invalid coordinator worker limits")
    started = time.monotonic()
    started_wall_ns = time.time_ns()
    child_env = dict(os.environ if env is None else env)
    pass_fds: tuple[int, ...] = ()
    read_fd = write_fd = None
    nonce = None
    if worker_handshake:
        read_fd, write_fd = os.pipe()
        os.set_inheritable(read_fd, True)
        nonce = secrets.token_bytes(32)
        child_env["GENETIK_COORDINATOR_WORKER_FD"] = str(read_fd)
        child_env["GENETIK_COORDINATOR_WORKER_NONCE"] = nonce.hex()
        child_env["GENETIK_COORDINATOR_WORKER_TOKEN"] = nonce.hex()
        pass_fds = (read_fd,)
    try:
        child = subprocess.Popen(list(argv), stdin=subprocess.DEVNULL,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 env=child_env, start_new_session=True, pass_fds=pass_fds,
                                 preexec_fn=lambda: _set_child_address_space(memory_limit_bytes))
    except BaseException:
        for fd in (read_fd, write_fd):
            if fd is not None:
                os.close(fd)
        raise
    if read_fd is not None and write_fd is not None and nonce is not None:
        os.close(read_fd)
        try:
            offset = 0
            while offset < len(nonce):
                offset += os.write(write_fd, nonce[offset:])
        finally:
            os.close(write_fd)
    with child:
        pid = child.pid
        start_ticks = None
        proc_hwm_peak = 0
        proc_rss_peak = 0
        chunks = {"stdout": bytearray(), "stderr": bytearray()}
        exceeded = False
        timed_out = False
        wait_status = None
        usage = None
        selector = None
        try:
            start_ticks = process_starttime(pid)
            selector = selectors.DefaultSelector()
            assert child.stdout is not None and child.stderr is not None
            os.set_blocking(child.stdout.fileno(), False)
            os.set_blocking(child.stderr.fileno(), False)
            selector.register(child.stdout, selectors.EVENT_READ, "stdout")
            selector.register(child.stderr, selectors.EVENT_READ, "stderr")
            while wait_status is None:
                try:
                    hwm, rss = proc_memory_status(pid)
                    proc_hwm_peak = max(proc_hwm_peak, hwm)
                    proc_rss_peak = max(proc_rss_peak, rss)
                except CoordinatorSupervisorError:
                    # A very short worker may exit between wait polling and
                    # /proc sampling; wait4 remains a separate measurement.
                    pass
                if time.monotonic() - started >= wall_limit_seconds:
                    timed_out = True
                    break
                try:
                    waited, status, rusage = os.wait4(pid, os.WNOHANG)
                except InterruptedError:
                    waited = 0
                if waited == pid:
                    wait_status, usage = status, rusage
                for key, _ in selector.select(timeout=0.02):
                    try:
                        data = os.read(key.fd, 65536)
                    except BlockingIOError:
                        continue
                    if not data:
                        selector.unregister(key.fileobj)
                    else:
                        if _retain_bounded_output(chunks, key.data, data, output_limit_bytes):
                            exceeded = True
                            break
                if exceeded:
                    break
            if timed_out or exceeded:
                _terminate_process_group(pid)
                if wait_status is None:
                    try:
                        wait_status, usage = os.wait4(pid, 0)[1:]
                    except ChildProcessError as error:
                        raise CoordinatorSupervisorError("worker was reaped outside its supervisor") from error
            # Parent exit does not imply the process group is empty.
            _terminate_process_group(pid, grace_seconds=0.05)
            deadline = time.monotonic() + 0.5
            while time.monotonic() < deadline and _group_exists(pid):
                _terminate_process_group(pid, sig=signal.SIGKILL, grace_seconds=0)
                time.sleep(0.01)
            if _group_exists(pid):
                raise CoordinatorSupervisorError("coordinator descendants could not be reaped")
            # Drain remaining bytes after wait4 so normal worker output is complete.
            drain_deadline = time.monotonic() + 0.5
            while selector.get_map() and time.monotonic() < drain_deadline:
                for key, _ in selector.select(timeout=0.02):
                    try:
                        data = os.read(key.fd, 65536)
                    except BlockingIOError:
                        continue
                    if not data:
                        selector.unregister(key.fileobj)
                        continue
                    if _retain_bounded_output(chunks, key.data, data, output_limit_bytes):
                        exceeded = True
                        break
                if exceeded:
                    break
        except BaseException as error:
            _terminate_process_group(pid)
            if wait_status is None:
                try:
                    _pid, wait_status, usage = os.wait4(pid, 0)
                except ChildProcessError:
                    wait_status = None
            try:
                error.coordinator_measurement = {
                    "pid": pid,
                    "starttime_ticks": start_ticks,
                    "worker_proc_vm_hwm_sampled_bytes": proc_hwm_peak or None,
                    "worker_proc_vm_rss_sampled_peak_bytes": proc_rss_peak or None,
                    "wait4_ru_maxrss_including_waited_children_bytes": (
                        rss_bytes_from_wait4(usage) if usage is not None else None),
                    "cpu_seconds": ((usage.ru_utime + usage.ru_stime) if usage is not None else None),
                    "wall_seconds": max(0.0, time.monotonic() - started),
                    "timed_out": timed_out,
                    "output_limit_exceeded": exceeded,
                    "returncode": (os.waitstatus_to_exitcode(wait_status)
                                   if wait_status is not None else None),
                    "address_space_limit_bytes": memory_limit_bytes,
                    "stdout": bytes(chunks["stdout"]),
                    "stderr": bytes(chunks["stderr"]),
                }
            except BaseException:
                # Preserve the original supervision exception; missing identity
                # then causes the caller to fail closed without closing a foreign phase.
                pass
            raise
        finally:
            if selector is not None:
                selector.close()
            if child.stdout is not None:
                child.stdout.close()
            if child.stderr is not None:
                child.stderr.close()
            # Popen must not attempt a second wait after wait4 reaped this PID.
            if wait_status is not None:
                child.returncode = os.waitstatus_to_exitcode(wait_status)
        if wait_status is None or usage is None:
            raise CoordinatorSupervisorError("worker exit/resource record unavailable")
        wait_rss = rss_bytes_from_wait4(usage)
        wait_cpu = float(usage.ru_utime + usage.ru_stime)
        if not (wait_cpu >= 0 and wait_cpu < float("inf")):
            raise CoordinatorSupervisorError("worker CPU measurement unavailable")
        returncode = os.waitstatus_to_exitcode(wait_status)
        return {
            "pid": pid,
            "starttime_ticks": start_ticks,
            "started_at_unix_ns": started_wall_ns,
            "wall_seconds": max(0.0, time.monotonic() - started),
            "cpu_seconds": wait_cpu,
            "wait4_ru_maxrss_including_waited_children_bytes": wait_rss,
            "worker_proc_vm_hwm_sampled_bytes": proc_hwm_peak or None,
            "worker_proc_vm_rss_sampled_peak_bytes": proc_rss_peak or None,
            "address_space_limit_bytes": memory_limit_bytes,
            "returncode": returncode,
            "timed_out": timed_out,
            "output_limit_exceeded": exceeded,
            "stdout": bytes(chunks["stdout"]),
            "stderr": bytes(chunks["stderr"]),
            "measurement_scope": "single_coordinator_worker_process",
        }


def _group_exists(pgid: int) -> bool:
    # Zombie-only groups cannot consume memory/CPU and may remain until init
    # reaps them. Treat only live members as cleanup failures.
    proc = Path("/proc")
    try:
        for entry in proc.iterdir():
            if not entry.name.isdigit():
                continue
            try:
                text = (entry / "stat").read_text(encoding="ascii")
                fields = text[text.rfind(")") + 2 :].split()
                state, process_group = fields[0], int(fields[2])
                if process_group == pgid and state != "Z":
                    return True
            except (OSError, ValueError, IndexError):
                continue
        return False
    except OSError:
        try:
            os.killpg(pgid, 0)
            return True
        except ProcessLookupError:
            return False
        except PermissionError:
            return True


def _terminate_process_group(pgid: int, *, sig: int = signal.SIGTERM,
                             grace_seconds: float = 0.05) -> None:
    try:
        os.killpg(pgid, sig)
    except ProcessLookupError:
        return
    if grace_seconds:
        time.sleep(grace_seconds)
        try:
            os.killpg(pgid, signal.SIGKILL)
        except ProcessLookupError:
            pass
