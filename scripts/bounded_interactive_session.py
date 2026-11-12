"""Bounded newline-framed request/response sessions for research programs.

The caller supplies a pinned request producer and a pinned response peer. Both
run beneath ``bounded_process``'s process-group supervisor. This module adds
protocol framing; it does not interpret scientific answers.
"""
from __future__ import annotations

import hashlib
import base64
import json
import os
from pathlib import Path
import resource
import selectors
import signal
import subprocess
import sys
import threading
import time
from typing import Any


class SessionFailure(RuntimeError):
    pass


class SessionTimeout(SessionFailure):
    pass


class SessionResourceLimit(SessionFailure):
    pass


def _write_all(fd: int, data: bytes, deadline: float) -> None:
    offset = 0
    selector = selectors.DefaultSelector()
    try:
        selector.register(fd, selectors.EVENT_WRITE)
        while offset < len(data):
            if time.monotonic() >= deadline:
                raise SessionTimeout("session deadline while writing response")
            try:
                count = os.write(fd, data[offset:])
                offset += count
            except BlockingIOError:
                selector.select(min(0.02, max(0.0, deadline - time.monotonic())))
            except BrokenPipeError as error:
                raise SessionFailure("peer closed before response was delivered") from error
    finally:
        selector.close()


def _read_line(stream, deadline: float, max_frame_bytes: int, overflow: threading.Event,
               capture=None, on_eof=None) -> bytes:
    fd = stream.fileno()
    os.set_blocking(fd, False)
    buffer = bytearray()
    selector = selectors.DefaultSelector()
    try:
        selector.register(fd, selectors.EVENT_READ)
        while time.monotonic() < deadline:
            if overflow.is_set():
                raise SessionResourceLimit("combined stdout/stderr output limit exceeded")
            for _, _ in selector.select(min(0.02, max(0.0, deadline - time.monotonic()))):
                try:
                    chunk = os.read(fd, min(4096, max_frame_bytes + 1 - len(buffer)))
                except BlockingIOError:
                    continue
                if not chunk:
                    if on_eof is not None:
                        on_eof()
                    if buffer:
                        raise SessionFailure("premature EOF in partial protocol frame")
                    raise EOFError
                if capture is not None:
                    capture(chunk)
                buffer.extend(chunk)
                if len(buffer) > max_frame_bytes:
                    raise SessionResourceLimit("protocol frame exceeds configured maximum")
                if b"\n" in buffer:
                    line, separator, trailing = bytes(buffer).partition(b"\n")
                    if trailing:
                        raise SessionFailure("multiple frames arrived before the prior response")
                    return line + separator
        raise SessionTimeout("session timed out waiting for protocol frame")
    finally:
        selector.close()


def _validated_json_frame(frame: bytes, label: str) -> dict[str, Any]:
    def object_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON key")
            result[key] = value
        return result
    try:
        value = json.loads(frame, object_pairs_hook=object_pairs,
                           parse_constant=lambda item: (_ for _ in ()).throw(ValueError(f"invalid {item}")))
    except (json.JSONDecodeError, UnicodeDecodeError, ValueError) as error:
        raise SessionFailure(f"malformed {label} JSON frame") from error
    if not isinstance(value, dict):
        raise SessionFailure(f"{label} frame must be a JSON object")
    return value


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _verify_command(argv: list[str], identity: dict[str, Any], label: str) -> None:
    if not isinstance(identity, dict) or set(identity) != {"executable", "sha256", "runtime_files"}:
        raise ValueError(f"{label} pinned command identity is incomplete")
    executable = Path(identity["executable"]).resolve()
    if Path(argv[0]).resolve() != executable or not executable.is_file() or not os.access(executable, os.X_OK):
        raise ValueError(f"{label} executable differs from its pinned identity")
    if _sha256_file(executable) != identity["sha256"]:
        raise ValueError(f"{label} executable hash mismatch")
    runtime_files = identity["runtime_files"]
    if not isinstance(runtime_files, list):
        raise ValueError(f"{label} runtime_files must be an array")
    seen: set[Path] = set()
    for item in runtime_files:
        if not isinstance(item, dict) or set(item) != {"path", "sha256"}:
            raise ValueError(f"{label} runtime file identity is malformed")
        path = Path(item["path"]).resolve()
        if path in seen or not path.is_file() or _sha256_file(path) != item["sha256"]:
            raise ValueError(f"{label} runtime file hash mismatch or duplicate")
        seen.add(path)


def _reject_pending_bytes(stream, capture=None, on_eof=None) -> None:
    selector = selectors.DefaultSelector()
    try:
        selector.register(stream.fileno(), selectors.EVENT_READ)
        if selector.select(0):
            try:
                chunk = os.read(stream.fileno(), 1)
            except BlockingIOError:
                return
            if chunk:
                if capture is not None:
                    capture(chunk)
                raise SessionFailure("unexpected post-terminal protocol output")
            if on_eof is not None:
                on_eof()
    finally:
        selector.close()


def _drain_to_eof(stream, deadline: float, capture, on_eof) -> None:
    selector = selectors.DefaultSelector()
    try:
        selector.register(stream.fileno(), selectors.EVENT_READ)
        while time.monotonic() < deadline:
            ready = selector.select(min(0.02, max(0.0, deadline - time.monotonic())))
            if not ready:
                continue
            try:
                chunk = os.read(stream.fileno(), 4096)
            except BlockingIOError:
                continue
            if not chunk:
                on_eof()
                return
            capture(chunk)
            raise SessionFailure("unexpected post-terminal protocol output")
        raise SessionTimeout("session timed out draining final process output")
    finally:
        selector.close()


def _wait_child(process: subprocess.Popen, *, blocking: bool = False):
    if process.returncode is not None:
        return None
    try:
        pid, status, usage = os.wait4(process.pid, 0 if blocking else os.WNOHANG)
    except ChildProcessError:
        return None
    if pid == 0:
        return None
    process.returncode = os.waitstatus_to_exitcode(status)
    return usage


def _terminate_children(processes: list[subprocess.Popen]) -> dict[int, Any]:
    # The external bounded supervisor owns process-group cleanup. This helper
    # terminates direct children without killing the caller before it can emit
    # a structured protocol failure.
    usage_by_pid: dict[int, Any] = {}
    for process in processes:
        usage = _wait_child(process)
        if usage is not None:
            usage_by_pid[process.pid] = usage
        if process.returncode is None:
            try:
                os.kill(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
    deadline = time.monotonic() + 0.15
    while time.monotonic() < deadline and any(p.returncode is None for p in processes):
        for process in processes:
            usage = _wait_child(process)
            if usage is not None:
                usage_by_pid[process.pid] = usage
        time.sleep(0.005)
    for process in processes:
        if process.returncode is None:
            try:
                os.kill(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
    for process in processes:
        if process.returncode is None:
            usage = _wait_child(process, blocking=True)
            if usage is not None:
                usage_by_pid[process.pid] = usage
    return usage_by_pid


def run_session(
    request_argv: list[str], response_argv: list[str], *, cwd: Path,
    environment: dict[str, str], wall_seconds: float, max_frame_bytes: int,
    max_protocol_bytes: int, max_requests: int,
    request_identity: dict[str, Any], response_identity: dict[str, Any],
    max_cpu_seconds: float, max_memory_bytes: int,
    terminal_type: str = "final", max_terminal_frame_bytes: int | None = None,
) -> dict[str, Any]:
    """Run a deterministic two-process session with one outstanding request.

    The request producer emits JSON-object frames on stdout. Frames whose
    ``type`` equals ``terminal_type`` are recorded and end requests. Every
    other frame is forwarded to the response peer; its next JSON-object line
    is forwarded to the producer. All pipes are nonblocking and deadline
    bounded. Both commands inherit the same supervised process group.
    """
    if not request_argv or not response_argv or not all(isinstance(x, str) for x in request_argv + response_argv):
        raise ValueError("both commands must be nonempty string arrays")
    if (type(max_frame_bytes) is not int or type(max_protocol_bytes) is not int or
            type(max_requests) is not int or not isinstance(wall_seconds, (int, float)) or
            isinstance(wall_seconds, bool) or not (0 < wall_seconds < float("inf"))):
        raise ValueError("session resource limits must be finite positive values")
    if (not isinstance(max_cpu_seconds, (int, float)) or isinstance(max_cpu_seconds, bool)
            or not 0 < max_cpu_seconds < float("inf") or type(max_memory_bytes) is not int
            or max_memory_bytes <= 0):
        raise ValueError("session CPU and memory limits must be finite and positive")
    if max_terminal_frame_bytes is None:
        max_terminal_frame_bytes = max_frame_bytes
    if (type(max_terminal_frame_bytes) is not int or max_frame_bytes <= 0
            or max_terminal_frame_bytes < max_frame_bytes
            or max_protocol_bytes < max_terminal_frame_bytes or max_requests <= 0):
        raise ValueError("invalid session limits")
    if not isinstance(environment, dict) or not environment or not all(
            isinstance(key, str) and isinstance(value, str) for key, value in environment.items()):
        raise ValueError("session environment must be an explicit string mapping")
    if not cwd.is_dir():
        raise ValueError("session working directory does not exist")
    if not terminal_type:
        raise ValueError("terminal_type is required")
    if os.getpgrp() != os.getpid():
        raise ValueError("interactive sessions must run as the process-group leader of a bounded invocation")
    _verify_command(request_argv, request_identity, "request producer")
    _verify_command(response_argv, response_identity, "response peer")

    deadline = time.monotonic() + wall_seconds
    events: list[dict[str, Any]] = []
    event_head = "0" * 64
    consumed = 0
    requests = 0
    seen_ids: set[tuple[type, int | str]] = set()
    terminal_seen = False
    output_lock = threading.Lock()
    byte_evidence = {
        name: {"bytes": 0, "chunks": [], "digest": hashlib.sha256(), "complete": False}
        for name in ("producer_stdout", "peer_stdout", "producer_stderr", "peer_stderr")
    }
    output_overflow = threading.Event()
    drain_stop = threading.Event()
    producer: subprocess.Popen | None = None
    peer: subprocess.Popen | None = None
    child_usage: dict[int, Any] = {}
    retained_total = 0
    self_before = resource.getrusage(resource.RUSAGE_SELF)

    def capture(name: str, chunk: bytes) -> None:
        nonlocal retained_total
        with output_lock:
            evidence = byte_evidence[name]
            evidence["bytes"] += len(chunk)
            evidence["digest"].update(chunk)
            total_observed = sum(item["bytes"] for item in byte_evidence.values())
            remaining = max_protocol_bytes - retained_total
            if remaining > 0:
                retained = chunk[:remaining]
                evidence["chunks"].append(retained)
                retained_total += len(retained)
            if total_observed > max_protocol_bytes:
                output_overflow.set()

    def byte_evidence_record() -> dict[str, Any]:
        record = {}
        with output_lock:
            for name, item in byte_evidence.items():
                raw = b"".join(item["chunks"])
                record[name] = {
                    "observed_bytes": item["bytes"],
                    "captured_bytes": len(raw),
                    "truncated": len(raw) != item["bytes"],
                    "eof_observed": item["complete"],
                    "complete": item["complete"] and len(raw) == item["bytes"],
                    "sha256": item["digest"].hexdigest(),
                    "captured_sha256": hashlib.sha256(raw).hexdigest(),
                    "base64": base64.b64encode(raw).decode("ascii"),
                    "text": raw.decode("utf-8", "replace"),
                }
        return record

    def failed_result(status: str, error: BaseException) -> dict[str, Any]:
        processes = [p for p in (producer, peer) if p is not None]
        child_usage.update(_terminate_children(processes))
        for thread in drain_threads:
            thread.join(timeout=0.2)
        usage_complete = all(p.pid in child_usage for p in processes)
        child_cpu = (sum(child_usage[p.pid].ru_utime + child_usage[p.pid].ru_stime for p in processes)
                     if usage_complete else None)
        return {"status": status, "diagnostic": str(error),
                "request_count": requests, "protocol_bytes": consumed, "events": events,
                "wait4_usage_complete": usage_complete,
                "waited_child_cpu_seconds": child_cpu,
                "waited_child_peak_rss_bytes": (
                    sum(int(child_usage[p.pid].ru_maxrss) * 1024 for p in processes)
                    if usage_complete else None),
                "child_exit_observed": all(p.returncode is not None for p in processes),
                "stderr_drained": not any(t.is_alive() for t in drain_threads),
                "stream_capture_complete": all(item["complete"] and
                                                sum(map(len, item["chunks"])) == item["bytes"]
                                                for item in byte_evidence.values()),
                "byte_evidence": byte_evidence_record()}

    def drain_diagnostic(stream) -> None:
        while not drain_stop.is_set():
            chunk = stream.read(4096)
            if not chunk:
                with output_lock:
                    byte_evidence["producer_stderr" if stream is producer.stderr else "peer_stderr"]["complete"] = True
                return
            capture("producer_stderr" if stream is producer.stderr else "peer_stderr", chunk)

    drain_threads: list[threading.Thread] = []

    def append_event(event: dict[str, Any]) -> None:
        nonlocal event_head
        body = {"previous_sha256": event_head, **event}
        digest = hashlib.sha256(json.dumps(body, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        events.append({**body, "sha256": digest})
        event_head = digest
    try:
        producer = subprocess.Popen(request_argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, cwd=cwd, env=environment,
                                    close_fds=True, bufsize=0)
        peer = subprocess.Popen(response_argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, cwd=cwd, env=environment,
                                 close_fds=True, bufsize=0)
        assert producer.stdout and producer.stdin and peer.stdout and peer.stdin
        assert producer.stderr and peer.stderr
        for stream in (producer.stdout, producer.stdin, peer.stdout, peer.stdin):
            os.set_blocking(stream.fileno(), False)
        for stream in (producer.stderr, peer.stderr):
            thread = threading.Thread(target=drain_diagnostic, args=(stream,), daemon=True)
            thread.start()
            drain_threads.append(thread)
        while not terminal_seen:
            # Drain buffered stdout before classifying an exited child. A
            # short-lived producer may have written a complete final frame.
            request = _read_line(producer.stdout, deadline, max_terminal_frame_bytes, output_overflow,
                                 lambda chunk: capture("producer_stdout", chunk),
                                 lambda: byte_evidence["producer_stdout"].__setitem__("complete", True))
            if output_overflow.is_set():
                raise SessionResourceLimit("combined stdout/stderr output limit exceeded")
            consumed += len(request)
            if consumed > max_protocol_bytes:
                raise SessionResourceLimit("cumulative protocol byte limit exceeded")
            request_object = _validated_json_frame(request[:-1], "request")
            if request_object.get("type") == terminal_type:
                terminal_seen = True
                append_event({"kind": "terminal", "request_bytes": len(request),
                              "request_sha256": hashlib.sha256(request).hexdigest()})
                if output_overflow.is_set():
                    raise SessionResourceLimit("combined stdout/stderr/protocol byte limit exceeded")
                break
            if len(request) > max_frame_bytes:
                raise SessionResourceLimit("nonterminal request frame exceeds configured maximum")
            request_id = request_object.get("id")
            if (not isinstance(request_object.get("type"), str) or not request_object["type"]
                    or isinstance(request_id, bool) or not isinstance(request_id, (int, str))
                    or (isinstance(request_id, str) and not request_id)):
                raise SessionFailure("request requires a nonempty type and correlated id")
            if not {"type", "id"}.issubset(request_object):
                raise SessionFailure("request schema is incomplete")
            if len(request_object) < 3:
                raise SessionFailure("request schema must include a payload field")
            request_key = (type(request_id), request_id)
            if request_key in seen_ids:
                raise SessionFailure("request correlation id was reused")
            seen_ids.add(request_key)
            requests += 1
            if requests > max_requests:
                raise SessionResourceLimit("request count limit exceeded")
            _write_all(peer.stdin.fileno(), request, deadline)
            response = _read_line(peer.stdout, deadline, max_frame_bytes, output_overflow,
                                  lambda chunk: capture("peer_stdout", chunk),
                                  lambda: byte_evidence["peer_stdout"].__setitem__("complete", True))
            if output_overflow.is_set():
                raise SessionResourceLimit("combined stdout/stderr output limit exceeded")
            consumed += len(response)
            if consumed > max_protocol_bytes:
                raise SessionResourceLimit("cumulative protocol byte limit exceeded")
            response_object = _validated_json_frame(response[:-1], "response")
            if isinstance(response_object.get("id"), bool) or not isinstance(response_object.get("id"), (int, str)):
                raise SessionFailure("response requires a typed correlation id")
            if response_object.get("id") != request_id or type(response_object.get("id")) is not type(request_id):
                raise SessionFailure("response correlation id does not match the pending request")
            if len(response_object) < 2 or not ({"value", "result", "payload"} & set(response_object)):
                raise SessionFailure("response schema must include a result payload")
            if output_overflow.is_set():
                raise SessionResourceLimit("combined stdout/stderr/protocol byte limit exceeded")
            _reject_pending_bytes(producer.stdout, lambda chunk: capture("producer_stdout", chunk),
                                  lambda: byte_evidence["producer_stdout"].__setitem__("complete", True))
            _reject_pending_bytes(peer.stdout, lambda chunk: capture("peer_stdout", chunk),
                                  lambda: byte_evidence["peer_stdout"].__setitem__("complete", True))
            _write_all(producer.stdin.fileno(), response, deadline)
            append_event({"kind": "exchange", "ordinal": requests,
                          "request_bytes": len(request), "request_sha256": hashlib.sha256(request).hexdigest(),
                          "response_bytes": len(response), "response_sha256": hashlib.sha256(response).hexdigest()})
        producer.stdin.close()
        peer.stdin.close()
        while time.monotonic() < deadline:
            if producer.returncode is None:
                usage = _wait_child(producer)
                if usage is not None:
                    child_usage[producer.pid] = usage
            if peer.returncode is None:
                usage = _wait_child(peer)
                if usage is not None:
                    child_usage[peer.pid] = usage
            producer_code = producer.returncode
            peer_code = peer.returncode
            if producer_code is not None and peer_code is not None:
                break
            time.sleep(0.002)
        if producer.returncode is None or peer.returncode is None:
            raise SessionTimeout("processes did not exit before the session deadline after terminal frame")
        if producer.returncode != 0 or peer.returncode != 0:
            raise SessionFailure(f"abnormal process exit ({producer.returncode}, {peer.returncode})")
        _drain_to_eof(producer.stdout, deadline, lambda chunk: capture("producer_stdout", chunk),
                      lambda: byte_evidence["producer_stdout"].__setitem__("complete", True))
        _drain_to_eof(peer.stdout, deadline, lambda chunk: capture("peer_stdout", chunk),
                      lambda: byte_evidence["peer_stdout"].__setitem__("complete", True))
        for thread in drain_threads:
            thread.join(timeout=max(0.0, deadline - time.monotonic()))
        if any(thread.is_alive() for thread in drain_threads):
            raise SessionTimeout("session deadline while draining child stderr")
        if output_overflow.is_set():
            raise SessionResourceLimit("combined stdout/stderr/protocol byte limit exceeded")
        self_after = resource.getrusage(resource.RUSAGE_SELF)
        self_cpu = ((self_after.ru_utime + self_after.ru_stime) -
                    (self_before.ru_utime + self_before.ru_stime))
        launched_children = (producer, peer)
        usage_complete = all(p.pid in child_usage for p in launched_children)
        if not usage_complete:
            return {"status": "resource_measurement_unavailable",
                    "diagnostic": "wait4 usage record missing for one or more launched children",
                    "request_count": requests, "protocol_bytes": consumed, "events": events,
                    "wait4_usage_complete": False, "session_cpu_seconds": None,
                    "waited_child_cpu_seconds": None, "waited_child_peak_rss_bytes": None,
                    "stream_capture_complete": all(item["complete"] and
                                                    sum(map(len, item["chunks"])) == item["bytes"]
                                                    for item in byte_evidence.values()),
                    "group_memory_measurement": "outer supervisor sampled separately",
                    "byte_evidence": byte_evidence_record()}
        child_cpu = sum(child_usage[p.pid].ru_utime + child_usage[p.pid].ru_stime
                        for p in launched_children)
        child_peak_rss = sum(int(child_usage[p.pid].ru_maxrss) * 1024 for p in launched_children)
        self_peak_rss = int(self_after.ru_maxrss) * 1024
        conservative_rss = self_peak_rss + child_peak_rss
        cpu_estimate = self_cpu + child_cpu
        if cpu_estimate > max_cpu_seconds:
            return {"status": "resource_exhausted", "diagnostic": "wait4 CPU accounting exceeded session limit",
                    "request_count": requests, "protocol_bytes": consumed, "events": events,
                    "session_cpu_seconds": cpu_estimate, "waited_child_cpu_seconds": child_cpu,
                    "wait4_usage_complete": True,
                    "stream_capture_complete": all(item["complete"] and
                                                    sum(map(len, item["chunks"])) == item["bytes"]
                                                    for item in byte_evidence.values()),
                    "conservative_rss_sum_bytes": conservative_rss,
                    "group_memory_measurement": "not_claimed",
                    "byte_evidence": byte_evidence_record()}
        if conservative_rss > max_memory_bytes:
            return {"status": "resource_exhausted", "diagnostic": "sum of individual wait4 RSS peaks exceeded session cap",
                    "request_count": requests, "protocol_bytes": consumed, "events": events,
                    "session_cpu_seconds": cpu_estimate, "waited_child_cpu_seconds": child_cpu,
                    "wait4_usage_complete": True,
                    "stream_capture_complete": all(item["complete"] and
                                                    sum(map(len, item["chunks"])) == item["bytes"]
                                                    for item in byte_evidence.values()),
                    "conservative_rss_sum_bytes": conservative_rss,
                    "group_memory_measurement": "not_claimed",
                    "byte_evidence": byte_evidence_record()}
        # The final message is kept as a final event. No raw protocol stdout is
        # hidden; callers persist this canonical transcript in their receipt.
        return {"status": "completed", "request_count": requests,
                "protocol_bytes": consumed, "events": events,
                "session_cpu_seconds": cpu_estimate, "waited_child_cpu_seconds": child_cpu,
                "waited_child_peak_rss_bytes": child_peak_rss,
                "wait4_usage_complete": True,
                "stream_capture_complete": all(item["complete"] and
                                                sum(map(len, item["chunks"])) == item["bytes"]
                                                for item in byte_evidence.values()),
                "conservative_rss_sum_bytes": conservative_rss,
                "group_memory_measurement": "outer supervisor sampled separately; conservative_rss_sum_bytes is not simultaneous group RSS",
                "byte_evidence": byte_evidence_record(),
                "producer_returncode": producer.returncode,
                "peer_returncode": peer.returncode}
    except EOFError as error:
        return failed_result("premature_eof", error)
    except SessionTimeout as error:
        return failed_result("timed_out", error)
    except SessionResourceLimit as error:
        return failed_result("resource_exhausted", error)
    except SessionFailure as error:
        return failed_result("protocol_error", error)
    finally:
        processes = [p for p in (producer, peer) if p is not None]
        if any(p.returncode is None for p in processes):
            child_usage.update(_terminate_children(processes))
        for process in processes:
            for stream in (process.stdin, process.stdout, process.stderr):
                if stream is not None:
                    try:
                        stream.close()
                    except OSError:
                        pass
        drain_stop.set()
        for thread in drain_threads:
            thread.join(timeout=0.5)


def _main(argv: list[str]) -> int:
    if len(argv) != 2:
        return 2
    try:
        config = json.loads(Path(argv[1]).read_text(encoding="utf-8"))
        result = run_session(
            config["request_argv"], config["response_argv"], cwd=Path(config["cwd"]),
            environment=config["environment"], wall_seconds=config["wall_seconds"],
            max_frame_bytes=config["max_frame_bytes"],
            max_protocol_bytes=config["max_protocol_bytes"],
            max_requests=config["max_requests"],
            request_identity=config["request_identity"], response_identity=config["response_identity"],
            max_cpu_seconds=config["max_cpu_seconds"], max_memory_bytes=config["max_memory_bytes"],
            terminal_type=config.get("terminal_type", "final"),
            max_terminal_frame_bytes=config.get("max_terminal_frame_bytes"))
        sys.stdout.write(json.dumps(result, sort_keys=True, separators=(",", ":")) + "\n")
        return 0 if result["status"] == "completed" else 3
    except BaseException as error:
        sys.stderr.write(f"interactive session harness failure: {type(error).__name__}: {error}\n")
        return 4


if __name__ == "__main__":
    raise SystemExit(_main(sys.argv))
