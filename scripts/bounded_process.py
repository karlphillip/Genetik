"""Shared, research-only supervisor for pinned external processes.

This module has no knowledge of solver output or scientific status. Callers
provide an exact argv, environment, input bytes, and resource limits; this
module enforces the process boundary and returns captured evidence.
"""

from __future__ import annotations

import os
import ctypes
from pathlib import Path
import resource
import selectors
import signal
import subprocess
import threading
import time
import pickle
import tempfile
import sys
from typing import Any


_INVOCATION_LOCK = threading.Lock()


def _set_child_limits(limits: dict[str, Any]) -> None:
    memory = int(limits["max_address_space_bytes"])
    resource.setrlimit(resource.RLIMIT_AS, (memory, memory))
    cpu_seconds = max(1, int(float(limits["max_cpu_seconds"]) + 0.999999))
    resource.setrlimit(resource.RLIMIT_CPU, (cpu_seconds, cpu_seconds + 1))
    file_size = int(limits.get("max_file_bytes", limits["max_output_bytes"]))
    resource.setrlimit(resource.RLIMIT_FSIZE, (file_size, file_size))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def _process_stat(proc_root: Path, pid: int) -> list[str] | None:
    """Read one process stat record, tolerating a process that exited meanwhile."""
    try:
        text = (proc_root / str(pid) / "stat").read_text(encoding="ascii")
    except FileNotFoundError:
        return None
    except OSError as error:
        if not (proc_root / str(pid)).exists():
            return None
        raise RuntimeError(f"cannot read process {pid} stat: {error}") from error
    tail = text[text.rfind(")") + 2 :].split()
    if len(tail) < 15:
        raise RuntimeError(f"malformed process {pid} stat")
    return tail


def _children(proc_root: Path, pid: int) -> set[int] | None:
    """Read task-local child lists rather than scanning unrelated system PIDs."""
    task_dir = proc_root / str(pid) / "task"
    try:
        tasks = list(os.scandir(task_dir))
    except OSError as error:
        if not (proc_root / str(pid)).exists():
            return set()
        raise RuntimeError(f"cannot enumerate process {pid} tasks: {error}") from error
    found: set[int] = set()
    supported = False
    for task in tasks:
        if not task.name.isdecimal():
            continue
        try:
            raw = (Path(task.path) / "children").read_text(encoding="ascii")
        except OSError as error:
            if not (proc_root / str(pid)).exists():
                return found
            if isinstance(error, FileNotFoundError):
                continue
            raise RuntimeError(f"cannot read process {pid} child list: {error}") from error
        try:
            supported = True
            found.update(int(value) for value in raw.split())
        except ValueError as error:
            raise RuntimeError(f"malformed process {pid} child list") from error
    return found if supported else None


def _proc_parent_map(proc_root: Path) -> dict[int, int]:
    """Read process ancestry with low-allocation procfs I/O."""
    parent_by_pid: dict[int, int] = {}
    try:
        entries = os.scandir(proc_root)
    except OSError as error:
        raise RuntimeError(f"cannot enumerate process table: {error}") from error
    with entries:
        for entry in entries:
            if not entry.name.isdecimal():
                continue
            try:
                fd = os.open(f"{entry.path}/stat", os.O_RDONLY | os.O_CLOEXEC)
                try:
                    raw = os.read(fd, 4096)
                finally:
                    os.close(fd)
                tail = raw.rsplit(b")", 1)[1].split()
                if len(tail) >= 2:
                    parent_by_pid[int(entry.name)] = int(tail[1])
            except (OSError, ValueError, IndexError):
                continue
    return parent_by_pid


def _scan_proc_direct_children(proc_root: Path, parent_pid: int) -> set[int]:
    """Fallback for procfs implementations without task-local child lists."""
    return {pid for pid, parent in _proc_parent_map(proc_root).items() if parent == parent_pid}


def _scan_proc_descendants(proc_root: Path, root_pid: int) -> set[int]:
    """Fallback for procfs implementations without task-local child lists."""
    parent_by_pid = _proc_parent_map(proc_root)
    descendants: set[int] = set()
    frontier = {root_pid}
    while frontier:
        children = {pid for pid, parent in parent_by_pid.items() if parent in frontier}
        children -= descendants
        descendants.update(children)
        frontier = children
    return descendants


def _process_tree(proc_root: Path, root_pid: int, *, supervisor_pid: int | None = None,
                  baseline_children: set[int] | None = None) -> set[int]:
    """Enumerate a supervised process tree using per-process child lists."""
    pending = [root_pid]
    if supervisor_pid is not None:
        direct = _children(proc_root, supervisor_pid)
        if direct is None:
            tree = _scan_proc_descendants(proc_root, supervisor_pid)
            tree.add(root_pid)
            return tree - (baseline_children or set()) - {supervisor_pid}
        pending.extend(direct - (baseline_children or set()) - {root_pid})
    seen: set[int] = set()
    while pending:
        pid = pending.pop()
        if pid in seen:
            continue
        seen.add(pid)
        children = _children(proc_root, pid)
        if children is None:
            tree = _scan_proc_descendants(proc_root, supervisor_pid or root_pid) | seen
            if supervisor_pid is not None:
                tree -= (baseline_children or set())
                tree.discard(supervisor_pid)
            return tree
        pending.extend(children - seen)
    return seen


def _sample_process_group(proc_root: Path, pgid: int, ticks_per_second: int,
                          supervisor_pid: int | None = None,
                          baseline_children: set[int] | None = None,
                          known_pids: set[int] | None = None) -> tuple[int, float] | None:
    """Sample the invocation tree; unreadable live members make accounting fail closed."""
    rss_total = 0
    cpu_ticks = 0
    readable = 0
    pids = known_pids if known_pids is not None else _process_tree(
        proc_root, pgid, supervisor_pid=supervisor_pid, baseline_children=baseline_children)
    for pid in pids:
        tail = _process_stat(proc_root, pid)
        if tail is None or int(tail[2]) != pgid:
            continue
        if tail[0] == "Z":
            rss = 0
        else:
            # Linux stat field 24 is resident pages. Reading it with CPU fields
            # avoids a second status/statm file lookup on every sample.
            try:
                resident_pages = int(tail[21])
            except (IndexError, ValueError) as error:
                raise RuntimeError(f"malformed process {pid} resident-page count") from error
            if resident_pages < 0:
                raise RuntimeError(f"invalid process {pid} resident-page count")
            rss = resident_pages * os.sysconf("SC_PAGE_SIZE")
        # Include child CPU already reaped by a live process-group member.
        cpu_ticks += sum(int(tail[index]) for index in (11, 12, 13, 14))
        rss_total += rss
        readable += 1
    return (rss_total, cpu_ticks / ticks_per_second) if readable else None


def _resource_sampler(pid: int, limits: dict[str, Any], stop: threading.Event,
                      result: dict[str, Any], baseline_children: set[int]) -> None:
    tick_rate = os.sysconf("SC_CLK_TCK")
    root = Path("/proc")
    refresh_interval = 0.005
    last_refresh = 0.0
    known_pids: set[int] = {pid}
    while not stop.is_set():
        try:
            now = time.monotonic()
            if now - last_refresh >= refresh_interval:
                known_pids = _process_tree(root, pid, supervisor_pid=os.getpid(),
                                           baseline_children=baseline_children)
                last_refresh = now
            sample = _sample_process_group(root, pid, tick_rate,
                                           supervisor_pid=os.getpid(),
                                           baseline_children=baseline_children,
                                           known_pids=known_pids)
        except (OSError, RuntimeError, ValueError) as error:
            result["resource_measurement_error"] = f"{type(error).__name__}: {error}"
            try:
                os.killpg(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            break
        if sample is not None:
            rss, cpu = sample
            result["process_group_rss_available"] = True
            result["peak_process_group_rss_bytes"] = max(result.get("peak_process_group_rss_bytes", 0), rss)
            result["peak_process_group_cpu_seconds"] = max(result.get("peak_process_group_cpu_seconds", 0.0), cpu)
            if rss >= int(limits["max_address_space_bytes"]):
                result["memory_limit_reached"] = True
                try:
                    os.killpg(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                break
            if cpu >= float(limits["max_cpu_seconds"]):
                result["cpu_limit_reached"] = True
                try:
                    os.killpg(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                break
        stop.wait(0.005)


def _kill_group(pgid: int, sig: int) -> bool:
    try:
        os.killpg(pgid, sig)
        return True
    except ProcessLookupError:
        return False


def _signal_pids(pids: set[int], sig: int) -> None:
    for pid in sorted(pids, reverse=True):
        try:
            os.kill(pid, sig)
        except ProcessLookupError:
            pass


def _enable_child_subreaper() -> None:
    """Adopt orphaned descendants inside the isolated invocation worker."""
    if not (os.path.exists("/proc/self/stat") and os.uname().sysname == "Linux"):
        raise RuntimeError("bounded descendant cleanup requires Linux child-subreaper support")
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(36, 1, 0, 0, 0) != 0:  # PR_SET_CHILD_SUBREAPER
        error = ctypes.get_errno()
        raise OSError(error, os.strerror(error))
    # The parent reserves this exact CPU envelope and resets child limits to
    # their separately declared target allowance before exec.
    resource.setrlimit(resource.RLIMIT_CPU, (3, resource.RLIM_INFINITY))


def _direct_children(parent_pid: int) -> set[int]:
    children = _children(Path("/proc"), parent_pid)
    if children is not None:
        return children
    return _scan_proc_direct_children(Path("/proc"), parent_pid)


def _reap_known_children(pids: set[int]) -> None:
    for pid in pids:
        try:
            os.waitpid(pid, os.WNOHANG)
        except (ChildProcessError, ProcessLookupError):
            pass


def _adopted_invocation_descendants(baseline: set[int], leader_pid: int) -> set[int]:
    descendants = _process_tree(Path("/proc"), leader_pid) - {leader_pid}
    descendants.update(_direct_children(os.getpid()) - baseline - {leader_pid})
    frontier = set(descendants)
    while frontier:
        children: set[int] = set()
        for parent in frontier:
            children.update(_direct_children(parent))
        children -= descendants
        descendants.update(children)
        frontier = children
    return descendants


def _live_pids(pids: set[int]) -> set[int]:
    live: set[int] = set()
    for pid in pids:
        try:
            Path(f"/proc/{pid}/stat").read_text()
            live.add(pid)
        except OSError:
            pass
    return live


def _process_descendants(root_pid: int) -> set[int]:
    return _process_tree(Path("/proc"), root_pid)


def _wait4_nonblocking(pid: int):
    try:
        waited, status, usage = os.wait4(pid, os.WNOHANG)
    except InterruptedError:
        return None
    if waited:
        return os.waitstatus_to_exitcode(status), usage
    return None


def _run_bounded_process_locked(
    argv: list[str],
    stdin_bytes: bytes,
    environment: dict[str, str],
    cwd: Path,
    limits: dict[str, Any],
    target_pid_path: Path | None = None,
) -> dict[str, Any]:
    """Run one exact command with nonblocking pipe I/O and process-group limits.

    ``stdout`` and ``stderr`` in the result are byte strings capped together by
    ``max_output_bytes``. The returned measurements distinguish leader rusage
    from sampled process-group lower bounds. A successful status is not issued
    when process-group RSS accounting is unavailable.
    """
    if not argv or not all(isinstance(arg, str) for arg in argv):
        raise ValueError("argv must be a nonempty string array")
    if not isinstance(stdin_bytes, bytes):
        raise TypeError("stdin_bytes must be bytes")
    if len(stdin_bytes) > int(limits["max_stdin_bytes"]):
        return {"status": "input_limit", "returncode": None, "wall_seconds": 0.0,
                "cpu_seconds": 0.0, "stdout": b"", "stderr": b"", "peak_rss_bytes": None,
                "process_group_rss_available": False, "process_group_cpu_seconds": None}

    started = time.monotonic()
    deadline = started + float(limits["max_wall_seconds"])
    proc: subprocess.Popen | None = None
    selector = selectors.DefaultSelector()
    sampled: dict[str, Any] = {"process_group_rss_available": False}
    stop_sampling = threading.Event()
    sampler: threading.Thread | None = None
    output = {"stdout": bytearray(), "stderr": bytearray()}
    output_total = 0
    input_offset = 0
    input_closed_early = False
    timed_out = False
    output_limit = False
    orphaned = False
    launch_error: str | None = None
    returncode: int | None = None
    usage = None
    process_group_killed = False
    kill_cleanup_deadline: float | None = None
    known_descendants: set[int] = set()
    baseline_children: set[int] = set()

    try:
        _enable_child_subreaper()
        baseline_children = _direct_children(os.getpid())
        proc = subprocess.Popen(
            argv,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=cwd,
            env=environment,
            close_fds=True,
            bufsize=0,
            start_new_session=True,
            preexec_fn=lambda: _set_child_limits(limits),
        )
        if target_pid_path is not None:
            pid_tmp = target_pid_path.with_suffix(".partial")
            pid_tmp.write_text(str(proc.pid))
            os.replace(pid_tmp, target_pid_path)
        assert proc.stdin is not None and proc.stdout is not None and proc.stderr is not None
        for stream in (proc.stdin, proc.stdout, proc.stderr):
            os.set_blocking(stream.fileno(), False)
        selector.register(proc.stdout, selectors.EVENT_READ, "stdout")
        selector.register(proc.stderr, selectors.EVENT_READ, "stderr")
        if stdin_bytes:
            selector.register(proc.stdin, selectors.EVENT_WRITE, "stdin")
        else:
            proc.stdin.close()

        initial = _sample_process_group(Path("/proc"), proc.pid, os.sysconf("SC_CLK_TCK"),
                                        supervisor_pid=os.getpid(),
                                        baseline_children=baseline_children)
        if initial is not None:
            sampled["process_group_rss_available"] = True
            sampled["peak_process_group_rss_bytes"] = initial[0]
            sampled["peak_process_group_cpu_seconds"] = initial[1]
        sampler = threading.Thread(
            target=_resource_sampler,
            args=(proc.pid, limits, stop_sampling, sampled, baseline_children), daemon=True,
        )
        sampler.start()

        while selector.get_map() or returncode is None:
            now = time.monotonic()
            if now >= deadline and returncode is None:
                timed_out = True
                _kill_group(proc.pid, signal.SIGTERM)
                _signal_pids(known_descendants, signal.SIGTERM)
                process_group_killed = True
                kill_cleanup_deadline = time.monotonic() + 0.25
                grace_end = min(deadline + 0.1, now + 0.1)
                while time.monotonic() < grace_end:
                    waited = _wait4_nonblocking(proc.pid)
                    if waited is not None:
                        returncode, usage = waited
                        break
                    time.sleep(0.002)
                if returncode is None:
                    _kill_group(proc.pid, signal.SIGKILL)
                    process_group_killed = True
                    waited = _wait4_nonblocking(proc.pid)
                    while waited is None:
                        time.sleep(0.002)
                        waited = _wait4_nonblocking(proc.pid)
                    returncode, usage = waited

            if returncode is None:
                waited = _wait4_nonblocking(proc.pid)
                if waited is not None:
                    returncode, usage = waited
                    proc.returncode = returncode
                    # A completed leader must not leave a helper running in its
                    # group. Kill it before joining pipe activity.
                    known_descendants.update(_adopted_invocation_descendants(baseline_children, proc.pid))
                    known_descendants.intersection_update(_live_pids(known_descendants))
                    if _kill_group(proc.pid, 0) or known_descendants:
                        orphaned = True
                        _kill_group(proc.pid, signal.SIGTERM)
                        _signal_pids(known_descendants, signal.SIGTERM)
                        process_group_killed = True
                        kill_cleanup_deadline = time.monotonic() + 0.25
                        time.sleep(0.01)
                        _kill_group(proc.pid, signal.SIGKILL)
                        _signal_pids(known_descendants, signal.SIGKILL)

            if now >= deadline and returncode is not None and selector.get_map():
                # A descendant can leave the original session and keep inherited
                # pipes open. Bound draining by the run deadline regardless.
                orphaned = True
                _kill_group(proc.pid, signal.SIGKILL)
                _signal_pids(known_descendants, signal.SIGKILL)
                process_group_killed = True
                if kill_cleanup_deadline is None:
                    kill_cleanup_deadline = time.monotonic() + 0.25

            if sampled.get("memory_limit_reached") or sampled.get("cpu_limit_reached"):
                _kill_group(proc.pid, signal.SIGKILL)
                _signal_pids(known_descendants, signal.SIGKILL)
                process_group_killed = True
                if returncode is None:
                    waited = _wait4_nonblocking(proc.pid)
                    while waited is None:
                        time.sleep(0.002)
                        waited = _wait4_nonblocking(proc.pid)
                    returncode, usage = waited
                    proc.returncode = returncode

            if returncode is not None and selector.get_map() and process_group_killed:
                # Do not let escaped inherited descriptors hold the supervisor
                # indefinitely after the process tree has been killed.
                if kill_cleanup_deadline is None:
                    kill_cleanup_deadline = time.monotonic() + 0.25
                close_deadline = kill_cleanup_deadline
            else:
                close_deadline = None

            timeout = min(0.01, max(0.0, deadline - time.monotonic())) if returncode is None else 0.01
            if close_deadline is not None:
                timeout = min(timeout, max(0.0, close_deadline - time.monotonic()))
                if time.monotonic() >= close_deadline:
                    for key in list(selector.get_map().values()):
                        try:
                            selector.unregister(key.fileobj)
                            key.fileobj.close()
                        except (KeyError, OSError):
                            pass
                    continue
            for key, _ in selector.select(timeout):
                stream = key.fileobj
                name = key.data
                try:
                    if name == "stdin":
                        written = os.write(stream.fileno(), stdin_bytes[input_offset:input_offset + 65536])
                        input_offset += written
                        if input_offset >= len(stdin_bytes):
                            selector.unregister(stream)
                            stream.close()
                    else:
                        chunk = os.read(stream.fileno(), min(65536, int(limits["max_output_bytes"]) + 1 - output_total))
                        if not chunk:
                            selector.unregister(stream)
                            stream.close()
                            continue
                        allowed = max(0, int(limits["max_output_bytes"]) - output_total)
                        output[name].extend(chunk[:allowed])
                        output_total += min(len(chunk), allowed)
                        if len(chunk) > allowed:
                            output_limit = True
                            _kill_group(proc.pid, signal.SIGKILL)
                            process_group_killed = True
                except (BrokenPipeError, OSError):
                    if name == "stdin" and input_offset < len(stdin_bytes):
                        input_closed_early = True
                    try:
                        selector.unregister(stream)
                    except KeyError:
                        pass
                    stream.close()

            if output_limit and returncode is None:
                _kill_group(proc.pid, signal.SIGKILL)
                _signal_pids(known_descendants, signal.SIGKILL)
                process_group_killed = True
                waited = _wait4_nonblocking(proc.pid)
                while waited is None:
                    time.sleep(0.002)
                    waited = _wait4_nonblocking(proc.pid)
                returncode, usage = waited
                proc.returncode = returncode

        if returncode is None:
            waited = _wait4_nonblocking(proc.pid)
            if waited is not None:
                returncode, usage = waited
                proc.returncode = returncode
        if proc.returncode is None:
            proc.returncode = returncode
    except OSError as error:
        launch_error = f"{type(error).__name__}: {error}"
        if proc is not None:
            _kill_group(proc.pid, signal.SIGKILL)
            process_group_killed = True
            waited = _wait4_nonblocking(proc.pid)
            while waited is None:
                time.sleep(0.002)
                waited = _wait4_nonblocking(proc.pid)
            returncode, usage = waited
            proc.returncode = returncode
    except BaseException as error:
        launch_error = f"supervisor_error: {type(error).__name__}: {error}"
        if proc is not None:
            _kill_group(proc.pid, signal.SIGKILL)
            process_group_killed = True
            if returncode is None:
                waited = _wait4_nonblocking(proc.pid)
                while waited is None:
                    time.sleep(0.002)
                    waited = _wait4_nonblocking(proc.pid)
                returncode, usage = waited
                proc.returncode = returncode
    finally:
        if proc is not None and process_group_killed:
            _kill_group(proc.pid, signal.SIGKILL)
            known_descendants.update(_adopted_invocation_descendants(baseline_children, proc.pid))
            _signal_pids(known_descendants, signal.SIGKILL)
        _reap_known_children(known_descendants)
        stop_sampling.set()
        if sampler is not None:
            sampler.join(timeout=1.0)
        selector.close()
        if proc is not None:
            for stream in (proc.stdin, proc.stdout, proc.stderr):
                if stream is not None and not stream.closed:
                    stream.close()

    wall = time.monotonic() - started
    child_cpu = float(usage.ru_utime + usage.ru_stime) if usage is not None else 0.0
    group_cpu = sampled.get("peak_process_group_cpu_seconds")
    cpu = max(child_cpu, float(group_cpu or 0.0))
    child_peak = int(usage.ru_maxrss * 1024) if usage is not None and os.name == "posix" else None
    if launch_error:
        status = "launch_error" if not launch_error.startswith("supervisor_error") else "harness_error"
    elif timed_out or wall >= float(limits["max_wall_seconds"]):
        status = "timed_out"
    elif output_limit:
        status = "output_limit"
    elif sampled.get("resource_measurement_error"):
        status = "resource_accounting_unavailable"
        launch_error = sampled["resource_measurement_error"]
    elif sampled.get("cpu_limit_reached") or returncode == -signal.SIGXCPU or cpu >= float(limits["max_cpu_seconds"]):
        status = "cpu_limit"
    elif sampled.get("memory_limit_reached"):
        status = "memory_limit"
    elif orphaned:
        status = "orphaned_process_group"
    elif input_closed_early:
        status = "stdin_closed_early"
    elif returncode is None:
        status = "harness_error"
    elif returncode < 0:
        status = "signaled"
    elif returncode != 0:
        status = "nonzero_exit"
    elif not sampled.get("process_group_rss_available"):
        status = "resource_accounting_unavailable"
    elif returncode == 0:
        status = "completed"
    else:
        status = "harness_error"
    return {
        "status": status,
        "returncode": returncode,
        "wall_seconds": wall,
        "cpu_seconds": cpu,
        "waited_child_cpu_seconds": child_cpu,
        "process_group_cpu_seconds": group_cpu,
        "peak_process_group_rss_bytes": sampled.get("peak_process_group_rss_bytes"),
        "process_group_rss_available": bool(sampled.get("process_group_rss_available")),
        "waited_child_peak_rss_bytes": child_peak,
        "timed_out": timed_out,
        "cpu_limit_reached": bool(sampled.get("cpu_limit_reached")),
        "memory_limit_reached": bool(sampled.get("memory_limit_reached")),
        "output_limit_reached": output_limit,
        "orphaned_process_group": orphaned,
        "harness_diagnostic": launch_error,
        "stdout": bytes(output["stdout"]),
        "stderr": bytes(output["stderr"]),
        "combined_output_bytes": output_total,
        "stdin_bytes_sent": input_offset,
        "stdin_bytes_total": len(stdin_bytes),
        "stdin_delivery_complete": input_offset == len(stdin_bytes),
    }


def _isolated_worker(result_path: Path, target_pid_path: Path, request_path: Path) -> None:
    """Run one invocation in a process that owns no unrelated children."""
    try:
        with request_path.open("rb") as stream:
            request = pickle.load(stream)
        result = _run_bounded_process_locked(
            request["argv"], request["stdin_bytes"], request["environment"],
            Path(request["cwd"]), request["limits"], target_pid_path)
    except BaseException as error:
        result = {"status": "harness_error", "returncode": None, "wall_seconds": 0.0,
                  "cpu_seconds": 0.0, "stdout": b"", "stderr": b"",
                  "peak_rss_bytes": None, "process_group_rss_available": False,
                  "harness_diagnostic": f"isolated supervisor: {type(error).__name__}: {error}"}
    supervisor_usage = resource.getrusage(resource.RUSAGE_SELF)
    result["supervisor_cpu_seconds"] = float(supervisor_usage.ru_utime + supervisor_usage.ru_stime)
    result["supervisor_cpu_is_lower_bound"] = True
    result["supervisor_peak_rss_bytes"] = int(supervisor_usage.ru_maxrss * 1024)
    temporary = Path(f"{result_path}.partial")
    with temporary.open("wb") as stream:
        pickle.dump(result, stream, protocol=pickle.HIGHEST_PROTOCOL)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, result_path)


def run_bounded_process(
    argv: list[str], stdin_bytes: bytes, environment: dict[str, str], cwd: Path,
    limits: dict[str, Any],
) -> dict[str, Any]:
    # The isolated worker is the invocation's subreaper and owns no unrelated
    # children. Result transfer uses an atomic file so a partial framed read
    # cannot outlive the hard deadline.
    with _INVOCATION_LOCK:
        with tempfile.TemporaryDirectory(prefix="genetik-bounded-worker-") as directory:
            temp_dir = Path(directory)
            result_path = temp_dir / "result.pickle"
            target_pid_path = temp_dir / "target.pid"
            request_path = temp_dir / "request.pickle"
            with request_path.open("wb") as stream:
                pickle.dump({"argv": argv, "stdin_bytes": stdin_bytes, "environment": environment,
                             "cwd": str(cwd), "limits": limits}, stream, protocol=pickle.HIGHEST_PROTOCOL)
            worker_environment = {"PATH": os.environ.get("PATH", "/usr/bin:/bin"),
                                  "LANG": "C", "LC_ALL": "C", "PYTHONHASHSEED": "0"}
            started = time.monotonic()
            worker = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "--internal-worker",
                                       str(result_path), str(target_pid_path), str(request_path)],
                                      stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                      stderr=subprocess.DEVNULL, cwd=temp_dir,
                                      env=worker_environment, close_fds=True,
                                      start_new_session=True)
            hard_deadline = started + float(limits["max_wall_seconds"]) + 3.0
            descendants: set[int] = set()
            next_snapshot = started
            worker_usage = None
            worker_returncode = None
            timed_out = False
            while time.monotonic() < hard_deadline:
                waited = _wait4_nonblocking(worker.pid)
                if waited is not None:
                    worker_returncode, worker_usage = waited
                    worker.returncode = worker_returncode
                    break
                now = time.monotonic()
                if now >= next_snapshot:
                    descendants.update(_process_descendants(worker.pid))
                    next_snapshot = now + 0.05
                time.sleep(0.01)
            if worker_returncode is None:
                timed_out = True
            if result_path.is_file():
                max_record_bytes = int(limits["max_output_bytes"]) + 1024 * 1024
                if result_path.stat().st_size > max_record_bytes:
                    result = {"status": "harness_error", "returncode": None,
                              "harness_diagnostic": "bounded supervisor result exceeded its transfer cap"}
                else:
                    with result_path.open("rb") as stream:
                        result = pickle.load(stream)
                if worker_returncode is None:
                    while time.monotonic() < hard_deadline:
                        waited = _wait4_nonblocking(worker.pid)
                        if waited is not None:
                            worker_returncode, worker_usage = waited
                            worker.returncode = worker_returncode
                            break
                        time.sleep(0.01)
                    if worker_returncode is None:
                        descendants.update(_process_descendants(worker.pid))
                        target_pid = 0
                        try:
                            target_pid = int(target_pid_path.read_text())
                        except (OSError, ValueError):
                            pass
                        if target_pid > 0:
                            _kill_group(target_pid, signal.SIGKILL)
                        _signal_pids(descendants, signal.SIGKILL)
                        _kill_group(worker.pid, signal.SIGKILL)
                        waited = _wait4_nonblocking(worker.pid)
                        while waited is None:
                            time.sleep(0.002)
                            waited = _wait4_nonblocking(worker.pid)
                        worker_returncode, worker_usage = waited
                        worker.returncode = worker_returncode
                        timed_out = True
                if worker_returncode != 0:
                    descendants.update(_process_descendants(worker.pid))
                    _signal_pids(descendants, signal.SIGKILL)
                    cpu_exhausted = worker_returncode == -signal.SIGXCPU
                    result["status"] = ("supervisor_cpu_limit" if cpu_exhausted else
                                         "supervisor_timeout" if timed_out else "harness_error")
                    result["harness_diagnostic"] = "worker failed after atomically publishing its result"
                total_wall = time.monotonic() - started
                target_wall = float(result.get("wall_seconds", 0.0))
                target_cpu = float(result.get("cpu_seconds", 0.0))
                supervisor_cpu = float(result.get("supervisor_cpu_seconds", 0.0))
                result["target_wall_seconds"] = target_wall
                result["target_cpu_seconds"] = target_cpu
                result["supervisor_wall_seconds"] = max(0.0, total_wall - target_wall)
                result["total_invocation_wall_seconds"] = total_wall
                result["wall_seconds"] = total_wall
                worker_tree_cpu = (float(worker_usage.ru_utime + worker_usage.ru_stime)
                                   if worker_usage is not None else None)
                result["worker_process_tree_cpu_seconds"] = worker_tree_cpu
                result["worker_process_tree_peak_rss_bytes"] = (
                    int(worker_usage.ru_maxrss * 1024) if worker_usage is not None else None)
                # wait4(worker) includes reaped descendants. Keep that
                # aggregate diagnostic separate from worker RUSAGE_SELF.
                result["supervisor_cpu_seconds"] = supervisor_cpu
                result["supervisor_cpu_is_lower_bound"] = True
                result["cpu_seconds"] = target_cpu + supervisor_cpu
                result["supervisor_peak_rss_bytes"] = result.get("supervisor_peak_rss_bytes")
                supervisor_rss = result.get("supervisor_peak_rss_bytes")
                target_rss = result.get("peak_process_group_rss_bytes")
                result["estimated_combined_peak_rss_bytes"] = (
                    int(supervisor_rss) + int(target_rss) if supervisor_rss is not None and target_rss is not None else None)
                return result

            # The worker failed or exceeded its hard deadline. Kill the target
            # session and any descendants still attributable to that worker.
            # Walk from the worker rather than a shared target-PID slot. This
            # also catches a target that died before publishing its PID and
            # escaped children already adopted by the isolated subreaper.
            descendants.update(_process_descendants(worker.pid))
            target_pid = 0
            try:
                target_pid = int(target_pid_path.read_text())
            except (OSError, ValueError):
                pass
            if target_pid > 0:
                _kill_group(target_pid, signal.SIGKILL)
            _signal_pids(descendants, signal.SIGKILL)
            try:
                os.killpg(worker.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            if worker_returncode is None:
                waited = _wait4_nonblocking(worker.pid)
                while waited is None:
                    time.sleep(0.002)
                    waited = _wait4_nonblocking(worker.pid)
                worker_returncode, worker_usage = waited
            worker.returncode = worker_returncode
            worker_tree_cpu = float(worker_usage.ru_utime + worker_usage.ru_stime)
            cpu_exhausted = worker.returncode == -signal.SIGXCPU
            status = ("supervisor_timeout" if timed_out else
                      "supervisor_cpu_limit" if cpu_exhausted else "harness_error")
            return {"status": status,
                    "returncode": worker.returncode, "wall_seconds": time.monotonic() - started,
                    "cpu_seconds": None,
                    "target_cpu_seconds": None, "supervisor_cpu_seconds": None,
                    "worker_process_tree_cpu_seconds": worker_tree_cpu,
                    "worker_process_tree_peak_rss_bytes": int(worker_usage.ru_maxrss * 1024),
                    "target_wall_seconds": None, "supervisor_wall_seconds": None,
                    "stdout": b"", "stderr": b"", "peak_rss_bytes": None,
                    "process_group_rss_available": False,
                    "harness_diagnostic": "isolated supervisor failed before writing a complete result"}


if __name__ == "__main__" and len(sys.argv) == 5 and sys.argv[1] == "--internal-worker":
    _isolated_worker(Path(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[4]))
