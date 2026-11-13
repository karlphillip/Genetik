#!/usr/bin/env python3
"""Synthetic-only tests for isolated coordinator resource supervision."""

from __future__ import annotations

import importlib.util
import hashlib
import json
import mmap
import os
import subprocess
from pathlib import Path
import sys
import unittest
from unittest import mock
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
SPEC = importlib.util.spec_from_file_location(
    "coordinator_supervisor_test", ROOT / "scripts/coordinator_supervisor.py")
supervisor = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(supervisor)


class CoordinatorSupervisorTests(unittest.TestCase):
    def run_python(self, body: str, *, memory=256 * 1024 * 1024, wall=3.0):
        return supervisor.supervise(
            [sys.executable, "-c", body], memory_limit_bytes=memory,
            wall_limit_seconds=wall, output_limit_bytes=1024 * 1024,
        )

    def test_fresh_process_scope_has_independent_high_water_marks(self):
        high = self.run_python(
            "x=bytearray(32*1024*1024); x[::4096]=b'\\x01'*(len(x)//4096); "
            "import time; time.sleep(.05); print('ready')")
        low = self.run_python("print('ready')")
        self.assertEqual(high["returncode"], 0, high["stderr"])
        self.assertEqual(low["returncode"], 0, low["stderr"])
        self.assertEqual(high["measurement_scope"], "single_coordinator_worker_process")
        self.assertIsNotNone(high["worker_proc_vm_hwm_sampled_bytes"])
        self.assertIsNotNone(low["worker_proc_vm_hwm_sampled_bytes"])
        self.assertGreater(high["worker_proc_vm_hwm_sampled_bytes"],
                           low["worker_proc_vm_hwm_sampled_bytes"] + 16 * 1024 * 1024)
        self.assertEqual(high["stdout"], b"ready\n")

    def test_address_space_limit_is_applied_before_worker_exec(self):
        result = self.run_python(
            "import resource; print(resource.getrlimit(resource.RLIMIT_AS)[0])",
            memory=128 * 1024 * 1024)
        self.assertEqual(result["returncode"], 0, result["stderr"])
        self.assertEqual(int(result["stdout"].strip()), 128 * 1024 * 1024)
        self.assertLessEqual(result["worker_proc_vm_hwm_sampled_bytes"], result["address_space_limit_bytes"])

    def test_actual_address_space_violation_is_not_success(self):
        result = self.run_python(
            "try:\n x=bytearray(256*1024*1024)\n print('unexpected')\nexcept MemoryError:\n raise SystemExit(17)",
            memory=128 * 1024 * 1024)
        self.assertEqual(result["returncode"], 17)
        self.assertNotEqual(result["stdout"], b"unexpected\n")

    def test_peak_measurement_unavailable_fails_closed(self):
        class Usage:
            ru_maxrss = 0
        with self.assertRaisesRegex(supervisor.CoordinatorSupervisorError, "unavailable"):
            supervisor.rss_bytes_from_wait4(Usage())

    def test_exec_worker_rusage_can_inherit_launcher_high_water(self):
        worker = """import os,resource
from pathlib import Path
lines=Path('/proc/self/status').read_text().splitlines()
h=int(next(x for x in lines if x.startswith('VmHWM:')).split()[1])*1024
r=int(next(x for x in lines if x.startswith('VmRSS:')).split()[1])*1024
print(os.getpid(),resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*1024,h,r)
"""
        launcher = """import os,sys
buf=bytearray(96*1024*1024)
for i in range(0,len(buf),4096): buf[i]=1
os.execv(sys.executable,[sys.executable,'-c',sys.argv[1]])
"""
        result = subprocess.run([sys.executable, "-c", launcher, worker],
                                check=True, capture_output=True, text=True)
        pid, inherited_ru, image_hwm, image_rss = map(int, result.stdout.split())
        self.assertNotEqual(pid, os.getpid())
        self.assertGreater(inherited_ru, image_hwm + 32 * 1024 * 1024)
        self.assertLessEqual(image_rss, image_hwm)

    def test_proc_vm_hwm_survives_allocation_release_and_tracks_current_rss(self):
        script = """import mmap
from pathlib import Path
def vals():
 s=Path('/proc/self/status').read_text().splitlines()
 return tuple(int(next(x for x in s if x.startswith(k+':')).split()[1])*1024
              for k in ('VmHWM','VmRSS'))
before=vals()
m=mmap.mmap(-1,64*1024*1024)
for i in range(0,len(m),4096): m[i:i+1]=b'x'
peak=vals()
m.close()
after=vals()
print(*before,*peak,*after)
"""
        result = subprocess.run([sys.executable, "-c", script],
                                check=True, capture_output=True, text=True)
        before_hwm, before_rss, peak_hwm, peak_rss, after_hwm, after_rss = map(int, result.stdout.split())
        self.assertGreater(peak_hwm, before_hwm + 32 * 1024 * 1024)
        self.assertLessEqual(peak_rss, peak_hwm)
        self.assertLessEqual(abs(after_hwm - peak_hwm), 1024 * 1024)
        self.assertLess(after_rss, peak_rss)

    def test_postlaunch_measurement_exception_kills_and_reaps_worker(self):
        observed = []
        def fail_after_spawn(pid):
            observed.append(pid)
            raise supervisor.CoordinatorSupervisorError("injected identity read failure")
        with mock.patch.object(supervisor, "process_starttime", side_effect=fail_after_spawn):
            with self.assertRaisesRegex(supervisor.CoordinatorSupervisorError, "injected"):
                self.run_python("import time; time.sleep(10)")
        self.assertEqual(len(observed), 1)
        self.assertFalse(supervisor._group_exists(observed[0]))

    def test_bad_worker_limits_are_refused_before_launch(self):
        for kwargs in (
            {"memory_limit_bytes": 0, "wall_limit_seconds": 1.0, "output_limit_bytes": 1},
            {"memory_limit_bytes": 1024, "wall_limit_seconds": 0.0, "output_limit_bytes": 1},
            {"memory_limit_bytes": 1024, "wall_limit_seconds": 1.0, "output_limit_bytes": 0},
        ):
            with self.assertRaises(ValueError):
                supervisor.supervise([sys.executable, "-c", "pass"], **kwargs)

    def test_worker_pid_identity_is_process_scoped(self):
        result = self.run_python("print('ok')")
        self.assertGreater(result["pid"], 0)
        self.assertGreater(result["starttime_ticks"], 0)

    def test_supervisor_worker_nonce_is_consumed_and_verified(self):
        env = os.environ.copy()
        env["PYTHONPATH"] = str(ROOT / "scripts") + os.pathsep + env.get("PYTHONPATH", "")
        result = supervisor.supervise(
            [sys.executable, "-c",
             "from coordinator_supervisor import consume_worker_handshake; "
             "print('ok' if consume_worker_handshake() else 'bad')"],
            memory_limit_bytes=128 * 1024 * 1024,
            wall_limit_seconds=2, output_limit_bytes=4096,
            env=env, worker_handshake=True,
        )
        self.assertEqual(result["returncode"], 0, result["stderr"])
        self.assertEqual(result["stdout"], b"ok\n")

    def test_handshake_rejects_nonce_mismatch_and_extra_bytes(self):
        for payload, expected in ((b"x" * 32, b"y" * 32),
                                  (b"x" * 33, b"x" * 32)):
            read_fd, write_fd = os.pipe()
            os.write(write_fd, payload)
            os.close(write_fd)
            prior_fd = os.environ.get("GENETIK_COORDINATOR_WORKER_FD")
            prior_nonce = os.environ.get("GENETIK_COORDINATOR_WORKER_NONCE")
            try:
                os.environ["GENETIK_COORDINATOR_WORKER_FD"] = str(read_fd)
                os.environ["GENETIK_COORDINATOR_WORKER_NONCE"] = expected.hex()
                self.assertFalse(supervisor.consume_worker_handshake())
            finally:
                if prior_fd is None:
                    os.environ.pop("GENETIK_COORDINATOR_WORKER_FD", None)
                else:
                    os.environ["GENETIK_COORDINATOR_WORKER_FD"] = prior_fd
                if prior_nonce is None:
                    os.environ.pop("GENETIK_COORDINATOR_WORKER_NONCE", None)
                else:
                    os.environ["GENETIK_COORDINATOR_WORKER_NONCE"] = prior_nonce

    def test_supervisor_pin_binds_exact_file_and_digest(self):
        digest = hashlib.sha256((ROOT / "scripts/coordinator_supervisor.py").read_bytes()).hexdigest()
        supervisor.verify_supervisor_pin({
            "path": str(ROOT / "scripts/coordinator_supervisor.py"), "sha256": digest,
        })
        with self.assertRaisesRegex(supervisor.CoordinatorSupervisorError, "path/hash"):
            supervisor.verify_supervisor_pin({
                "path": str(ROOT / "scripts/coordinator_supervisor.py"), "sha256": "0" * 64,
            })

    def test_fake_worker_marker_cannot_bypass_supervised_entrypoints(self):
        for script, args in (
            (ROOT / "scripts/execute_continuation_phase.py",
             ["--protocol-dir", "/missing", "--phase-id", "campaign_orchestration"]),
            (ROOT / "scripts/scientific_pipeline_preflight.py", ["--help"]),
        ):
            env = os.environ.copy()
            env["GENETIK_COORDINATOR_WORKER_TOKEN"] = "forged"
            env.pop("GENETIK_COORDINATOR_WORKER_FD", None)
            env.pop("GENETIK_COORDINATOR_WORKER_NONCE", None)
            result = subprocess.run([sys.executable, str(script), *args],
                                    env=env, capture_output=True, text=True, timeout=3)
            self.assertNotEqual(result.returncode, 0, script.name)
            self.assertIn("verified inherited nonce", result.stderr + result.stdout)

    def test_parent_lifetime_high_water_does_not_leak_into_fresh_worker(self):
        parent = bytearray(48 * 1024 * 1024)
        for i in range(0, len(parent), 4096):
            parent[i] = 1
        result = self.run_python("import time; time.sleep(.05)")
        parent_hwm, _ = supervisor.proc_memory_status(os.getpid())
        self.assertEqual(result["returncode"], 0)
        self.assertLess(result["worker_proc_vm_hwm_sampled_bytes"], parent_hwm - 20 * 1024 * 1024)

    def test_concurrent_supervisors_have_distinct_worker_identities(self):
        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(lambda _: self.run_python("print('ready')"), range(2)))
        self.assertEqual([row["returncode"] for row in results], [0, 0])
        self.assertNotEqual(results[0]["pid"], results[1]["pid"])
        self.assertTrue(all(row["worker_proc_vm_hwm_sampled_bytes"] for row in results))

    def test_nonzero_worker_exit_is_preserved(self):
        result = self.run_python("raise SystemExit(23)")
        self.assertEqual(result["returncode"], 23)

    def test_timeout_terminates_worker_and_process_group(self):
        result = supervisor.supervise(
            [sys.executable, "-c", "import time; time.sleep(10)"],
            memory_limit_bytes=128 * 1024 * 1024,
            wall_limit_seconds=0.2, output_limit_bytes=4096,
        )
        self.assertTrue(result["timed_out"])
        self.assertNotEqual(result["returncode"], 0)
        self.assertFalse(supervisor._group_exists(result["pid"]))

    def test_output_cap_terminates_chatty_worker(self):
        result = supervisor.supervise(
            [sys.executable, "-c", "import sys; sys.stdout.write('x'*200000)"],
            memory_limit_bytes=128 * 1024 * 1024,
            wall_limit_seconds=2.0, output_limit_bytes=4096,
        )
        self.assertTrue(result["output_limit_exceeded"])
        self.assertLessEqual(len(result["stdout"]) + len(result["stderr"]), 4096)

    def test_parent_exit_kills_live_descendant(self):
        code = (
            "import subprocess,sys; "
            "p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)']); "
            "print(p.pid,flush=True)"
        )
        result = self.run_python(code)
        self.assertEqual(result["returncode"], 0, result["stderr"])
        pid = int(result["stdout"].strip())
        deadline = __import__("time").monotonic() + 1.0
        while __import__("time").monotonic() < deadline:
            stat = Path(f"/proc/{pid}/stat")
            if not stat.exists() or stat.read_text().split()[2] == "Z":
                break
            __import__("time").sleep(0.01)
        else:
            self.fail("coordinator descendant remained live after supervisor cleanup")


class CoordinatorWorkerReconciliationTests(unittest.TestCase):
    """Temporary synthetic-ledger smoke test; never reads the research ledger."""

    def test_prephase_launcher_failure_is_terminal_without_fabricated_charges(self):
        import json
        import shutil
        import tempfile
        from unittest import mock
        import research_execution as research

        root = Path(tempfile.mkdtemp())
        try:
            ledger = root / "ledger"
            ledger.mkdir()
            (ledger / "process-budget.lock").write_bytes(b"")
            (ledger / "process-events.jsonl").write_bytes(b"")
            state = {"ledger_head": "0" * 64, "event_count": 0,
                     "terminal_status": None, "active_coordinator_phase": None}
            (ledger / "process-budget.json").write_text(json.dumps(state))
            protocol_sha = "a" * 64
            script = ROOT / "scripts/scientific_pipeline_preflight.py"
            plan = {"amendment_id": "synthetic-amendment",
                    "coordinator_phase_ids": ["primary_preflight", "recovery_preflight"],
                    "slots": {"primary_preflight": ["p"], "recovery_preflight": ["r"],
                              "campaign": ["c"], "final_scoring": ["s"]}}
            config = {"ledger_dir": ledger, "continuation_plan": plan}
            with (mock.patch.object(research, "read_frozen_program",
                                    return_value=({}, config, protocol_sha)),
                  mock.patch.object(research, "_verify_process_ledger", return_value=[])):
                result = research.record_supervised_coordinator_worker(
                    root, "primary_preflight", None, script)
            self.assertEqual(result["status"], "terminal_failure")
            saved = json.loads((ledger / "process-budget.json").read_text())
            events = [json.loads(line) for line in
                      (ledger / "process-events.jsonl").read_text().splitlines()]
            self.assertEqual(saved["terminal_status"], "coordinator_admission_refused")
            self.assertIsNone(saved["active_coordinator_phase"])
            self.assertEqual([row["event"] for row in events],
                             ["admission_refused", "program_terminal"])
            self.assertNotIn("charged_cpu_seconds", events[0])
            self.assertNotIn("charged_wall_seconds", events[0])
            self.assertEqual(research.continuation_next_action(plan, events)["state"],
                             "terminal_failure")
        finally:
            shutil.rmtree(root)

    def test_parent_measurement_is_bound_to_worker_receipt_and_charged(self):
        import json
        import shutil
        import tempfile
        from unittest import mock
        import research_execution as research

        root = Path(tempfile.mkdtemp())
        try:
            ledger = root / "ledger"
            ledger.mkdir()
            (ledger / "process-budget.lock").write_bytes(b"")
            events_path = ledger / "process-events.jsonl"
            script = ROOT / "scripts/execute_continuation_phase.py"
            script_sha = hashlib.sha256(script.read_bytes()).hexdigest()
            protocol_sha = "a" * 64
            receipt = {
                "token": "phase-token", "phase_id": "campaign_orchestration", "pid": 234,
                "coordinator_pid_starttime_ticks": 456, "status": "completed",
                "evidence_sha256": "c" * 64, "coordinator_peak_rss_bytes": 8 * 1024 * 1024,
                "coordinator_proc_vm_hwm_bytes": 8 * 1024 * 1024,
                "coordinator_rusage_self_ru_maxrss_diagnostic_bytes": 10 * 1024 * 1024,
                "coordinator_proc_vm_rss_bytes": 6 * 1024 * 1024,
            }
            receipt_path = ledger / "phase.json"
            receipt_path.write_text(json.dumps(receipt, sort_keys=True))
            receipt_sha = hashlib.sha256(receipt_path.read_bytes()).hexdigest()
            phase_event = {
                "event": "coordinator_phase", "protocol_sha256": protocol_sha,
                "amendment_id": "synthetic-amendment", "phase_id": "campaign_orchestration",
                "token": "phase-token", "pid": 234, "pid_starttime_ticks": 456,
                "status": "completed", "receipt_path": str(receipt_path),
                "receipt_sha256": receipt_sha, "coordinator_cpu_seconds": 0.2,
                "coordinator_exclusive_wall_seconds": 0.3,
                "coordinator_peak_rss_bytes": receipt["coordinator_peak_rss_bytes"],
                "coordinator_proc_vm_hwm_bytes": receipt["coordinator_proc_vm_hwm_bytes"],
                "failure_category": None,
            }
            prior = "0" * 64
            encoded = {**phase_event, "previous_hash": prior}
            digest = hashlib.sha256(json.dumps(encoded, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
            encoded["event_sha256"] = digest
            events_path.write_text(json.dumps(encoded, sort_keys=True, separators=(",", ":")) + "\n")
            state = {
                "ledger_head": digest, "event_count": 1, "terminal_status": None,
                "active_coordinator_phase": None, "coordinator_cpu_seconds_charged": 0.2,
                "coordinator_exclusive_wall_seconds_charged": 0.3,
                "max_coordinator_cpu_seconds": 11.0,
                "max_coordinator_exclusive_wall_seconds": 11.0,
            }
            (ledger / "process-budget.json").write_text(json.dumps(state))
            config = {
                "ledger_dir": ledger,
                "continuation_plan": {
                    "amendment_id": "synthetic-amendment",
                    "coordinator_entrypoint": {"path": str(script), "sha256": script_sha},
                    "slots": {"recovery_preflight": []},
                },
            }
            measurement = {
                "pid": 234, "starttime_ticks": 456,
                "worker_proc_vm_hwm_sampled_bytes": 8 * 1024 * 1024,
                "wait4_ru_maxrss_including_waited_children_bytes": 10 * 1024 * 1024,
                "cpu_seconds": 0.4, "supervisor_cpu_seconds": 0.01,
                "supervisor_exclusive_wall_seconds": 0.02,
                "supervisor_ru_maxrss_diagnostic_bytes": 14 * 1024 * 1024,
                "supervisor_vm_hwm_bytes": 12 * 1024 * 1024,
                "supervisor_vm_rss_bytes": 8 * 1024 * 1024,
                "address_space_limit_bytes": 512 * 1024 * 1024,
                "timed_out": False, "output_limit_exceeded": False, "returncode": 0,
            }
            with (mock.patch.object(research, "read_frozen_program", return_value=({}, config, protocol_sha)),
                  mock.patch.object(research, "_verify_process_ledger", return_value=[phase_event])):
                result = research.record_supervised_coordinator_worker(
                    root, "campaign_orchestration", measurement, script)
            self.assertEqual(result["status"], "verified")
            saved = json.loads((ledger / "process-budget.json").read_text())
            self.assertAlmostEqual(saved["coordinator_cpu_seconds_charged"], 0.21)
            self.assertAlmostEqual(saved["coordinator_exclusive_wall_seconds_charged"], 0.32)
            appended = json.loads(events_path.read_text().splitlines()[-1])
            self.assertEqual(appended["event"], "coordinator_worker_supervised")
            self.assertEqual(appended["wait4_ru_maxrss_including_waited_children_bytes"], 10 * 1024 * 1024)
        finally:
            shutil.rmtree(root)

    def test_concurrent_admission_refusal_preserves_foreign_active_worker(self):
        import shutil
        import tempfile
        from unittest import mock
        import research_execution as research

        root = Path(tempfile.mkdtemp())
        try:
            ledger = root / "ledger"
            ledger.mkdir()
            (ledger / "process-budget.lock").write_bytes(b"")
            protocol_sha = "a" * 64
            start = {
                "event": "coordinator_phase_started", "protocol_sha256": protocol_sha,
                "amendment_id": "synthetic-amendment", "phase_id": "primary_preflight",
                "token": "owner-token", "pid": 41, "pid_starttime_ticks": 777,
            }
            initial = {**start, "previous_hash": "0" * 64}
            initial_hash = research.sha256_bytes(research.compact_json(initial))
            initial["event_sha256"] = initial_hash
            events_path = ledger / "process-events.jsonl"
            events_path.write_bytes(research.compact_json(initial) + b"\n")
            active = {key: start[key] for key in (
                "token", "phase_id", "pid", "pid_starttime_ticks")}
            state = {
                "ledger_head": initial_hash, "event_count": 1,
                "terminal_status": None, "active_coordinator_phase": active,
            }
            (ledger / "process-budget.json").write_text(json.dumps(state))
            config = {"ledger_dir": ledger, "continuation_plan": {
                "amendment_id": "synthetic-amendment",
                "coordinator_phase_ids": ["primary_preflight", "recovery_preflight"],
            }}
            with (mock.patch.object(research, "read_frozen_program",
                                    return_value=({}, config, protocol_sha)),
                  mock.patch.object(research, "_verify_process_ledger", return_value=[start])):
                research._record_coordinator_admission_failure(
                    root, "primary_preflight", "second coordinator was refused")
            saved = json.loads((ledger / "process-budget.json").read_text())
            rows = [json.loads(line) for line in events_path.read_text().splitlines()]
            self.assertIsNone(saved["terminal_status"])
            self.assertEqual(saved["active_coordinator_phase"], active)
            self.assertEqual(rows[-1]["event"], "coordinator_admission_attempt_refused")
            self.assertEqual(rows[-1]["active_phase_token"], "owner-token")
            self.assertNotEqual(rows[-1]["requester_pid"], 41)
        finally:
            shutil.rmtree(root)

    def test_missing_phase_receipt_closes_only_supervisor_identified_active_worker(self):
        import shutil
        import tempfile
        from unittest import mock
        import research_execution as research

        root = Path(tempfile.mkdtemp())
        try:
            ledger = root / "ledger"
            ledger.mkdir()
            (ledger / "process-budget.lock").write_bytes(b"")
            events_path = ledger / "process-events.jsonl"
            script = ROOT / "scripts/execute_continuation_phase.py"
            protocol_sha = "a" * 64
            start = {"event": "coordinator_phase_started", "protocol_sha256": protocol_sha,
                     "amendment_id": "synthetic-amendment", "phase_id": "campaign_orchestration",
                     "token": "owned-token", "pid": 41, "pid_starttime_ticks": 777}
            initial = {**start, "previous_hash": "0" * 64}
            digest = research.sha256_bytes(research.compact_json(initial))
            initial["event_sha256"] = digest
            events_path.write_bytes(research.compact_json(initial) + b"\n")
            active = {key: start[key] for key in (
                "token", "phase_id", "pid", "pid_starttime_ticks")}
            state = {"ledger_head": digest, "event_count": 1, "terminal_status": None,
                     "active_coordinator_phase": active}
            (ledger / "process-budget.json").write_text(json.dumps(state))
            config = {"ledger_dir": ledger, "continuation_plan": {
                "amendment_id": "synthetic-amendment",
                "coordinator_entrypoint": {"path": str(script), "sha256": hashlib.sha256(script.read_bytes()).hexdigest()},
            }}
            measurement = {"pid": 41, "starttime_ticks": 777, "returncode": 9}
            with (mock.patch.object(research, "read_frozen_program",
                                    return_value=({}, config, protocol_sha)),
                  mock.patch.object(research, "_verify_process_ledger", return_value=[start])):
                result = research.record_supervised_coordinator_worker(
                    root, "campaign_orchestration", measurement, script)
            self.assertEqual(result["status"], "terminal_failure")
            saved = json.loads((ledger / "process-budget.json").read_text())
            self.assertIsNone(saved["active_coordinator_phase"])
            self.assertEqual(saved["terminal_status"], "coordinator_worker_failed_without_phase_receipt")
            rows = [json.loads(line) for line in events_path.read_text().splitlines()]
            self.assertEqual([row["event"] for row in rows[-3:]], [
                "coordinator_phase", "coordinator_worker_supervised", "program_terminal"])
            self.assertTrue(all(row.get("token") == "owned-token" for row in rows[-3:-1]))
            self.assertEqual(rows[-1]["status"], "coordinator_worker_failed_without_phase_receipt")
        finally:
            shutil.rmtree(root)

    def test_begin_checks_both_memory_high_water_sources_against_cap(self):
        import research_execution as research
        from unittest import mock

        plan = {"coordinator_phase_ids": ["primary_preflight"]}
        with (mock.patch.object(research, "read_frozen_program",
                                return_value=({}, {"continuation_plan": plan}, "a" * 64)),
              mock.patch.object(research, "_enforce_coordinator_address_space"),
              mock.patch.object(research, "_coordinator_memory_measurements",
                                return_value=(research.CONTINUATION_MEMORY_BYTES * 2,
                                              research.CONTINUATION_MEMORY_BYTES + 1,
                                              research.CONTINUATION_MEMORY_BYTES))):
            with self.assertRaisesRegex(research.GateError, "VmHWM cap"):
                research._begin_coordinator_phase(Path("unused"), "primary_preflight")

    def test_inherited_ru_maxrss_diagnostic_does_not_drive_worker_cap(self):
        import research_execution as research
        with self.assertRaisesRegex(research.GateError, "VmHWM cap"):
            research._require_coordinator_vm_hwm_within_limit(
                research.CONTINUATION_MEMORY_BYTES + 1)
        self.assertIsNone(research._require_coordinator_vm_hwm_within_limit(
            research.CONTINUATION_MEMORY_BYTES - 1))


if __name__ == "__main__":
    unittest.main()
