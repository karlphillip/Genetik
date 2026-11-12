"""Engineering contract tests for the separate pinned-program execution path."""
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor
import unittest

ROOT = Path(__file__).resolve().parents[1]
HARNESS = ROOT / "scripts" / "research_execution.py"
sys.path.insert(0, str(ROOT / "scripts"))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class BoundedProgramExecutionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.ledger = self.root / "process-ledgers"
        self.solver_ledger = self.root / "solver-ledgers"
        self.input_file = self.root / "input.bin"
        self.input_file.write_bytes(b"payload-\x00-with-bytes")
        self.script = self.root / "program.py"
        self.python = Path(sys.executable).resolve()
        self._write_program("import sys\ndata=sys.stdin.buffer.read()\nsys.stdout.buffer.write(b'OUT:'+data)\nsys.stderr.buffer.write(b'ERR')\n")

    def tearDown(self):
        self.temp.cleanup()

    def _write_program(self, body):
        self.script.write_text("#!/usr/bin/env python3\n" + body)
        self.script.chmod(0o755)

    def protocol(self, *, max_output=4096, max_wall=2.0, max_cpu=2.0, max_total_wall=None, max_total_cpu=None):
        max_total_wall = max_wall + 3.0 if max_total_wall is None else max_total_wall
        max_total_cpu = max_cpu + 3.0 if max_total_cpu is None else max_total_cpu
        budget_dir = self.ledger / "sample-program"
        return {
            "schema": 1, "kind": "bounded_program", "protocol_id": "program-test",
            "source_commit": "a" * 40,
            "program_budget": {"program_id": "sample-program", "ledger_dir": str(budget_dir),
                "max_lifetime_invocations": 1, "max_lifetime_cpu_seconds": max_total_cpu,
                "max_lifetime_wall_seconds": max_total_wall},
            "command": {"executable": str(self.python), "sha256": sha(self.python),
                "version": sys.version.split()[0], "args": [str(self.script)],
                "environment": {"PATH": "/usr/bin:/bin", "LANG": "C", "LC_ALL": "C", "PYTHONHASHSEED": "0"},
                "runtime_files": [{"path": str(self.script), "sha256": sha(self.script)}]},
            "artifacts": [{"id": "training", "path": str(self.input_file), "sha256": sha(self.input_file), "role": "input"}],
            "limits": {"max_invocations": 1, "max_total_cpu_seconds": max_total_cpu,
                "max_total_wall_seconds": max_total_wall, "max_cpu_seconds": max_cpu,
                "max_wall_seconds": max_wall, "max_address_space_bytes": 512 * 1024 * 1024,
                "max_stdin_bytes": 4096, "max_output_bytes": max_output, "max_file_bytes": 1024 * 1024},
            "runs": [{"run_id": "preflight-unit", "phase_id": "preflight", "stdin_artifact": "training"}],
            "phase_plan": [
                {"phase_id": phase, "run_ids": ["preflight-unit"] if phase == "preflight" else []}
                for phase in ("preflight", "campaign", "verification", "scoring", "aggregation", "recovery")
            ],
        }

    def command(self, *args):
        env = os.environ.copy()
        env["GENETIK_RESEARCH_PROCESS_LEDGER_ROOT"] = str(self.ledger)
        env["GENETIK_RESEARCH_LEDGER_ROOT"] = str(self.solver_ledger)
        return subprocess.run([sys.executable, str(HARNESS), *map(str, args)],
            env=env, text=True, capture_output=True, timeout=10)

    def freeze(self, protocol=None, destination=None):
        protocol = protocol or self.protocol()
        source = self.root / "source-protocol.json"
        source.write_text(json.dumps(protocol, sort_keys=True))
        dest = destination or self.root / "frozen"
        result = self.command("freeze-program", "--protocol", source, "--destination", dest)
        self.assertEqual(result.returncode, 0, result.stderr)
        return dest

    def test_pinned_python_process_streams_binary_stdin_and_both_outputs(self):
        frozen = self.freeze()
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        receipt_path = frozen / "runs/preflight-unit/receipt.json"
        receipt_detail = receipt_path.read_text() if receipt_path.exists() else "missing receipt"
        self.assertEqual(result.returncode, 0, f"{result.stderr}\n{receipt_detail}")
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertEqual(receipt["status"], "completed")
        self.assertGreaterEqual(receipt["charged_cpu_seconds"], receipt["cpu_seconds"])
        self.assertGreaterEqual(receipt["charged_wall_seconds"], receipt["wall_seconds"])
        self.assertIsInstance(receipt["target_cpu_seconds"], (int, float))
        self.assertIsInstance(receipt["supervisor_cpu_seconds"], (int, float))
        self.assertIsInstance(receipt["supervisor_wall_seconds"], (int, float))
        self.assertIsInstance(receipt["estimated_combined_peak_rss_bytes"], int)
        self.assertAlmostEqual(receipt["cpu_seconds"],
                               receipt["target_cpu_seconds"] + receipt["supervisor_cpu_seconds"], places=6)
        self.assertTrue(receipt["supervisor_cpu_is_lower_bound"])
        self.assertEqual((frozen / "runs/preflight-unit/stdout.bin").read_bytes(), b"OUT:payload-\x00-with-bytes")
        self.assertEqual((frozen / "runs/preflight-unit/stderr.bin").read_bytes(), b"ERR")
        verified = self.command("verify-program", "--protocol-dir", frozen)
        self.assertEqual(verified.returncode, 0, verified.stderr)
        self.assertTrue(json.loads(verified.stdout)["status"] == "valid")
        self.assertFalse((self.solver_ledger / "sample-program").exists())

    def test_pinned_bounded_program_can_run_correlated_interactive_session(self):
        helper = ROOT / "scripts" / "bounded_interactive_session.py"
        request = self.root / "requester.py"
        response = self.root / "responder.py"
        request.write_text(
            "import sys\n"
            "print('{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"q1\\\",\\\"input\\\":7}',flush=True)\n"
            "answer=sys.stdin.buffer.readline()\n"
            "print('{\\\"type\\\":\\\"final\\\",\\\"payload\\\":'+answer.strip().decode()+'}')\n")
        response.write_text(
            "import sys,json\n"
            "request=json.loads(sys.stdin.buffer.readline())\n"
            "print(json.dumps({'id':request['id'],'value':request['input']*2}),flush=True)\n")
        self._write_program(
            "import json,pathlib,sys\n"
            f"sys.path.insert(0,{str(ROOT / 'scripts')!r})\n"
            "from bounded_interactive_session import run_session\n"
            "import hashlib\n"
            "def pin(path): return {'executable':sys.executable,'sha256':hashlib.sha256(open(sys.executable,'rb').read()).hexdigest(),'runtime_files':[{'path':path,'sha256':hashlib.sha256(open(path,'rb').read()).hexdigest()}]}\n"
            "r=run_session([sys.executable," + repr(str(request)) + "],"
            "[sys.executable," + repr(str(response)) + "],cwd=pathlib.Path.cwd(),"
            "environment={'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C'},"
            "wall_seconds=1.5,max_frame_bytes=1024,max_protocol_bytes=8192,max_requests=4,"
            "max_cpu_seconds=2.0,max_memory_bytes=512*1024*1024,"
            "request_identity=pin(" + repr(str(request)) + "),response_identity=pin(" + repr(str(response)) + "))\n"
            "print(json.dumps(r,sort_keys=True,separators=(',',':')))\n"
            "sys.exit(0 if r['status']=='completed' else 3)\n")
        protocol = self.protocol(max_wall=3.0, max_total_wall=6.0)
        protocol["command"]["runtime_files"] = [
            {"path": str(self.script), "sha256": sha(self.script)},
            {"path": str(helper), "sha256": sha(helper)},
            {"path": str(request), "sha256": sha(request)},
            {"path": str(response), "sha256": sha(response)},
        ]
        frozen = self.freeze(protocol, self.root / "interactive-frozen")
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertEqual(receipt["status"], "completed")
        captured = json.loads((frozen / "runs/preflight-unit/stdout.bin").read_text())
        self.assertEqual(captured["status"], "completed")
        self.assertEqual(captured["request_count"], 1)
        event = captured["events"][0]
        self.assertEqual(event["previous_sha256"], "0" * 64)
        self.assertEqual(len(event["sha256"]), 64)
        verified = self.command("verify-program", "--protocol-dir", frozen)
        self.assertEqual(verified.returncode, 0, verified.stderr)

    @unittest.skipUnless(os.environ.get("GENETIK_RESEARCH_CPP_FIXTURE"),
                         "native process fixture is supplied by CTest")
    def test_pinned_native_cpp_process_uses_the_same_bounded_contract(self):
        executable = Path(os.environ["GENETIK_RESEARCH_CPP_FIXTURE"]).resolve()
        protocol = self.protocol()
        protocol["program_budget"]["program_id"] = "native-cpp-program"
        protocol["program_budget"]["ledger_dir"] = str((self.ledger / "native-cpp-program").resolve())
        protocol["command"] = {
            "executable": str(executable), "sha256": sha(executable), "version": "fixture-v1",
            "args": [], "environment": {"PATH": "/usr/bin:/bin", "LANG": "C", "LC_ALL": "C"},
            "runtime_files": [],
        }
        protocol["program_budget"]["max_lifetime_invocations"] = 1
        if os.environ.get("GENETIK_RESEARCH_CPP_FIXTURE_SANITIZED", "").strip().lower() in {
            "1", "on", "true", "yes",
        }:
            # ASan reserves a very large virtual shadow mapping. Keep the
            # isolated test fixture's explicit cap above that reservation;
            # independent tests still exercise ordinary address-space limits.
            protocol["limits"]["max_address_space_bytes"] = 1 << 46
        destination = self.root / "frozen-cpp"
        frozen = self.freeze(protocol, destination)
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        receipt_path = frozen / "runs/preflight-unit/receipt.json"
        receipt_detail = receipt_path.read_text() if receipt_path.exists() else "missing receipt"
        self.assertEqual(result.returncode, 0, f"stdout={result.stdout}\nstderr={result.stderr}\n{receipt_detail}")
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertEqual(receipt["status"], "completed")
        self.assertEqual((frozen / "runs/preflight-unit/stdout.bin").read_bytes(),
                         b"CPP:" + self.input_file.read_bytes())
        self.assertEqual((frozen / "runs/preflight-unit/stderr.bin").read_bytes(), b"CPPERR")

    def test_protocol_requires_complete_phase_budget_before_freeze(self):
        protocol = self.protocol(max_total_cpu=2.0)
        protocol["program_budget"]["max_lifetime_invocations"] = 2
        protocol["program_budget"]["max_lifetime_cpu_seconds"] = 2.0
        protocol["limits"]["max_invocations"] = 2
        protocol["limits"]["max_cpu_seconds"] = 1.5
        protocol["limits"]["max_wall_seconds"] = 1.5
        protocol["limits"]["max_total_wall_seconds"] = 3.0
        protocol["program_budget"]["max_lifetime_wall_seconds"] = 3.0
        protocol["runs"].append({"run_id": "campaign-step", "phase_id": "campaign", "stdin_artifact": "training"})
        protocol["phase_plan"][0]["run_ids"] = ["preflight-unit"]
        protocol["phase_plan"][1]["run_ids"] = ["campaign-step"]
        result = self.command("freeze-program", "--protocol", self._write_source(protocol),
                              "--destination", self.root / "bad-frozen")
        self.assertEqual(result.returncode, 3)
        self.assertIn("phase plan", result.stderr)
        self.assertFalse((self.root / "bad-frozen").exists())

    def test_cumulative_charges_preserve_measured_overshoot_and_reserved_failure_envelope(self):
        from research_execution import charged_program_resources
        measured = {"target_cpu_seconds": 1.2, "target_wall_seconds": 1.5,
                    "cpu_seconds": 4.7, "worker_process_tree_cpu_seconds": 6.2,
                    "wall_seconds": 9.0, "process_group_rss_available": True}
        incomplete, cpu_charge, wall_charge = charged_program_resources(measured, "cpu_limit", 2.0, 3.0)
        self.assertTrue(incomplete)
        self.assertEqual(cpu_charge, 6.2)
        self.assertEqual(wall_charge, 9.0)

        measured_success = {**measured, "target_cpu_seconds": 5.2, "target_wall_seconds": 5.5,
                            "cpu_seconds": 8.6, "worker_process_tree_cpu_seconds": 8.7,
                            "wall_seconds": 9.0}
        incomplete, cpu_charge, wall_charge = charged_program_resources(
            measured_success, "completed", 2.0, 3.0)
        self.assertFalse(incomplete)
        self.assertEqual(cpu_charge, 8.7)
        self.assertEqual(wall_charge, 9.0)

    def test_lifetime_invocation_capacity_is_reserved_for_the_whole_remaining_plan(self):
        protocol = self.protocol(max_total_cpu=5.0, max_total_wall=5.0)
        protocol["program_budget"]["max_lifetime_invocations"] = 1
        first = self.freeze(protocol, self.root / "first-protocol")
        completed = self.command("run-program", "--protocol-dir", first, "--run-id", "preflight-unit")
        self.assertEqual(completed.returncode, 0, completed.stderr)

        second_protocol = json.loads(json.dumps(protocol))
        second_protocol["protocol_id"] = "second-program-protocol"
        second_protocol["runs"][0]["run_id"] = "second-run"
        second_protocol["phase_plan"][0]["run_ids"] = ["second-run"]
        second_protocol["program_budget"]["ledger_dir"] = protocol["program_budget"]["ledger_dir"]
        second_source = self._write_source(second_protocol)
        second = self.root / "second-protocol"
        result = self.command("freeze-program", "--protocol", second_source, "--destination", second)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("lifetime identity/limits changed", result.stderr)
        self.assertFalse(second.exists())
        state = json.loads((self.ledger / "sample-program/process-budget.json").read_text())
        self.assertEqual(state["invocations_reserved"], 1)

    def _write_source(self, protocol):
        path = self.root / "bad-protocol.json"
        path.write_text(json.dumps(protocol, sort_keys=True))
        return path

    def test_runtime_dependency_tamper_refused_without_invocation(self):
        frozen = self.freeze()
        with self.script.open("a") as stream:
            stream.write("# tampered\n")
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 3)
        state = json.loads((self.ledger / "sample-program/process-budget.json").read_text())
        self.assertEqual(state["invocations_reserved"], 0)

    def test_coordinated_frozen_protocol_and_marker_tamper_refused_without_invocation(self):
        frozen = self.freeze()
        program_path = frozen / "program.json"
        protocol = json.loads(program_path.read_text())
        protocol["protocol_id"] = "substituted-frozen-plan"
        encoded = json.dumps(protocol, sort_keys=True, separators=(",", ":")).encode()
        program_path.chmod(0o644)
        program_path.write_bytes(encoded)
        marker_path = frozen / "PROGRAM.sha256"
        marker_path.chmod(0o644)
        marker_path.write_text(f"{hashlib.sha256(encoded).hexdigest()}  program.json\n")

        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 3)
        self.assertIn("different frozen protocol", result.stderr)
        state = json.loads((self.ledger / "sample-program/process-budget.json").read_text())
        self.assertEqual(state["invocations_reserved"], 0)
        self.assertFalse((frozen / "runs/preflight-unit").exists())

    def test_assessment_identity_is_not_opened_before_scoring(self):
        protocol = self.protocol()
        protocol["artifacts"].append({"id": "assessment", "path": str(self.root / "sealed-assessment.tsv"),
                                      "sha256": "f" * 64, "role": "assessment"})
        protocol["runs"][0]["stdin_artifact"] = "assessment"
        frozen = self.freeze(protocol)
        self.assertFalse((self.root / "sealed-assessment.tsv").exists())
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("scoring", result.stderr)
        self.assertFalse((frozen / "runs/preflight-unit/reservation.json").exists())

    def test_wall_timeout_is_explicit_and_charged_at_full_allowance(self):
        self._write_program("import time\ntime.sleep(2)\n")
        frozen = self.freeze(self.protocol(max_wall=0.15, max_total_wall=3.15))
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 2)
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertEqual(receipt["status"], "timed_out")
        self.assertGreaterEqual(receipt["charged_wall_seconds"], 0.15)

    def test_combined_stdout_stderr_limit_is_enforced(self):
        self._write_program("import sys\nsys.stdout.write('x'*80)\nsys.stderr.write('y'*80)\n")
        frozen = self.freeze(self.protocol(max_output=100))
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 2)
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertEqual(receipt["status"], "output_limit")
        self.assertLessEqual(receipt["stdout_bytes"] + receipt["stderr_bytes"], 100)

    def test_fast_exit_preserves_wait4_measurements_when_group_sampling_misses(self):
        self._write_program("import os\nos.write(1,b'fast')\n")
        protocol = self.protocol()
        protocol["runs"][0]["stdin_artifact"] = None
        frozen = self.freeze(protocol)
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        # A fast process may disappear before the first /proc sample. Its
        # waited-child rusage remains available, while group RSS is explicit.
        self.assertIn(result.returncode, (0, 2), result.stderr)
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertIn(receipt["status"], {"completed", "resource_accounting_unavailable"})
        self.assertIsNotNone(receipt["waited_child_peak_rss_bytes"])
        self.assertGreaterEqual(receipt["waited_child_cpu_seconds"], 0.0)
        if not receipt["process_group_rss_available"]:
            self.assertEqual(receipt["status"], "resource_accounting_unavailable")

    def test_partial_output_and_interactive_stdin_are_not_line_buffered(self):
        self._write_program(
            "import os,sys\nos.write(1,b'prompt:')\ndata=sys.stdin.buffer.read(7)\n"
            "os.write(1,b'answer='+data)\n"
        )
        frozen = self.freeze(self.protocol())
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertEqual((frozen / "runs/preflight-unit/stdout.bin").read_bytes(), b"prompt:answer=payload")

    def test_simultaneous_stdout_and_stderr_larger_than_pipe_capacity(self):
        self._write_program(
            "import os,threading\ndef w(fd,b):\n  for _ in range(32): os.write(fd,b*32768)\n"
            "a=threading.Thread(target=w,args=(1,b'x')); b=threading.Thread(target=w,args=(2,b'y'))\n"
            "a.start();b.start();a.join();b.join()\n"
        )
        protocol = self.protocol(max_output=2 * 1024 * 1024, max_total_wall=5.0)
        protocol["runs"][0]["stdin_artifact"] = None
        protocol["limits"]["max_output_bytes"] = 2 * 1024 * 1024
        protocol["limits"]["max_total_wall_seconds"] = 5.0
        frozen = self.freeze(protocol)
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        receipt_path = frozen / "runs/preflight-unit/receipt.json"
        receipt_detail = receipt_path.read_text() if receipt_path.exists() else "missing receipt"
        self.assertEqual(result.returncode, 0, f"{result.stderr}\n{receipt_detail}")
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertEqual(receipt["status"], "completed")
        self.assertEqual(receipt["stdout_bytes"], 1024 * 1024)
        self.assertEqual(receipt["stderr_bytes"], 1024 * 1024)

    def test_abnormal_exit_is_distinct_from_timeout_and_resource_exhaustion(self):
        self._write_program("import os,signal\nos.kill(os.getpid(),signal.SIGTERM)\n")
        protocol = self.protocol()
        protocol["runs"][0]["stdin_artifact"] = None
        frozen = self.freeze(protocol)
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 2)
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertEqual(receipt["status"], "signaled")
        self.assertEqual(receipt["returncode"], -signal.SIGTERM)

    def test_leader_exit_cleans_up_a_surviving_descendant(self):
        pid_file = self.root / "descendant.pid"
        self._write_program(
            "import pathlib,subprocess,sys\np=subprocess.Popen([sys.executable,'-c','import time; time.sleep(10)'])\n"
            "pathlib.Path(sys.argv[1]).write_text(str(p.pid))\n",
        )
        protocol = self.protocol()
        protocol["runs"][0]["stdin_artifact"] = None
        protocol["command"]["args"].append(str(pid_file))
        protocol["command"]["runtime_files"][0]["sha256"] = sha(self.script)
        frozen = self.freeze(protocol)
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 2)
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertEqual(receipt["status"], "orphaned_process_group")
        descendant = int(pid_file.read_text())
        try:
            with open(f"/proc/{descendant}/stat") as stream:
                state = stream.read().rsplit(")", 1)[1].split()[0]
            self.assertEqual(state, "Z")
        except FileNotFoundError:
            pass

    def test_escaped_session_descendant_cannot_hold_output_pipe_past_deadline(self):
        pid_file = self.root / "escaped-descendant.pid"
        self._write_program(
            "import pathlib,subprocess,sys\n"
            "p=subprocess.Popen([sys.executable,'-c',\"import os,sys,time; os.setsid(); "
            "open(sys.argv[1],'w').write(str(os.getpid())); time.sleep(10)\",str(sys.argv[1])])\n"
            "pathlib.Path(sys.argv[1]+'.parent').write_text(str(p.pid))\n",
        )
        protocol = self.protocol(max_wall=0.25, max_total_wall=3.25)
        protocol["runs"][0]["stdin_artifact"] = None
        protocol["command"]["args"].append(str(pid_file))
        frozen = self.freeze(protocol)
        started = time.monotonic()
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        elapsed = time.monotonic() - started
        self.assertEqual(result.returncode, 2)
        self.assertLess(elapsed, 2.0)
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertIn(receipt["status"], {"orphaned_process_group", "timed_out"})
        child_pid = int(pid_file.read_text())
        deadline = time.monotonic() + 1
        while time.monotonic() < deadline:
            try:
                stat = Path(f"/proc/{child_pid}/stat")
                if not stat.exists() or stat.read_text().rsplit(")", 1)[1].split()[0] == "Z":
                    break
            except OSError:
                break
            time.sleep(0.01)
        else:
            self.fail("escaped descendant survived bounded process cleanup")

    def test_timeout_cleans_up_escaped_session_descendant(self):
        pid_file = self.root / "timeout-escaped.pid"
        self._write_program(
            "import pathlib,subprocess,sys,time\n"
            "p=subprocess.Popen([sys.executable,'-c',\"import os,sys,time; os.setsid(); "
            "open(sys.argv[1],'w').write(str(os.getpid())); time.sleep(10)\",str(sys.argv[1])])\n"
            "pathlib.Path(sys.argv[1]+'.parent').write_text(str(p.pid))\n"
            "time.sleep(10)\n",
        )
        protocol = self.protocol(max_wall=0.3, max_total_wall=3.3)
        protocol["runs"][0]["stdin_artifact"] = None
        protocol["command"]["args"].append(str(pid_file))
        frozen = self.freeze(protocol)
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 2)
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertIn(receipt["status"], {"timed_out", "orphaned_process_group"})
        child_pid = int(pid_file.read_text())
        deadline = time.monotonic() + 1
        while time.monotonic() < deadline:
            try:
                stat = Path(f"/proc/{child_pid}/stat")
                if not stat.exists() or stat.read_text().rsplit(")", 1)[1].split()[0] == "Z":
                    break
            except OSError:
                break
            time.sleep(0.01)
        else:
            self.fail("escaped descendant survived supervisor timeout")

    def test_preexisting_unrelated_child_is_not_killed(self):
        from bounded_process import run_bounded_process
        unrelated = subprocess.Popen([str(self.python), "-c", "import time; time.sleep(10)"])
        try:
            limits = {"max_cpu_seconds": 1, "max_wall_seconds": 2, "max_address_space_bytes": 256*1024*1024,
                      "max_stdin_bytes": 10, "max_output_bytes": 1000, "max_file_bytes": 1000}
            result = run_bounded_process([str(self.python), "-c", "import time; time.sleep(.1); print('ok')"], b"",
                {"PATH": "/usr/bin:/bin", "LANG": "C", "LC_ALL": "C"}, self.root, limits)
            self.assertEqual(result["status"], "completed")
            self.assertIsNone(unrelated.poll())
        finally:
            unrelated.terminate()
            unrelated.wait(timeout=2)

    def test_unrelated_child_started_during_supervision_is_not_killed(self):
        from bounded_process import run_bounded_process
        unrelated_processes = []
        limits = {"max_cpu_seconds": 1, "max_wall_seconds": 2, "max_address_space_bytes": 256*1024*1024,
                  "max_stdin_bytes": 10, "max_output_bytes": 1000, "max_file_bytes": 1000}
        env = {"PATH": "/usr/bin:/bin", "LANG": "C", "LC_ALL": "C"}

        def launch_unrelated():
            time.sleep(0.03)
            unrelated_processes.append(subprocess.Popen(
                [str(self.python), "-c", "import time; time.sleep(10)"]))

        thread = threading.Thread(target=launch_unrelated)
        thread.start()
        try:
            result = run_bounded_process([str(self.python), "-c", "import time; time.sleep(.15); print('ok')"],
                b"", env, self.root, limits)
            thread.join(timeout=2)
            self.assertFalse(thread.is_alive())
            self.assertEqual(result["status"], "completed")
            self.assertEqual(len(unrelated_processes), 1)
            self.assertIsNone(unrelated_processes[0].poll())
        finally:
            thread.join(timeout=2)
            for unrelated in unrelated_processes:
                if unrelated.poll() is None:
                    unrelated.terminate()
                unrelated.wait(timeout=2)

    def test_fast_and_delayed_process_resource_statuses_are_explicit(self):
        from bounded_process import run_bounded_process
        env = {"PATH": "/usr/bin:/bin", "LANG": "C", "LC_ALL": "C"}
        limits = {"max_cpu_seconds": 1, "max_wall_seconds": 2, "max_address_space_bytes": 256*1024*1024,
                  "max_stdin_bytes": 10, "max_output_bytes": 1000, "max_file_bytes": 1000}
        fast = run_bounded_process([str(self.python), "-c", "pass"], b"", env, self.root, limits)
        if fast["process_group_rss_available"]:
            self.assertEqual(fast["status"], "completed")
            self.assertIsInstance(fast["peak_process_group_rss_bytes"], int)
        else:
            self.assertEqual(fast["status"], "resource_accounting_unavailable")
        delayed = run_bounded_process([str(self.python), "-c", "import time; time.sleep(.15); print('done')"],
                                      b"", env, self.root, limits)
        self.assertEqual(delayed["status"], "completed")
        self.assertTrue(delayed["process_group_rss_available"])
        self.assertIn(b"done", delayed["stdout"])
        self.assertGreaterEqual(delayed["waited_child_cpu_seconds"], 0)

    def test_target_cpu_limit_can_exceed_isolated_supervisor_limit(self):
        from bounded_process import run_bounded_process
        limits = {"max_cpu_seconds": 5, "max_wall_seconds": 2, "max_address_space_bytes": 256*1024*1024,
                  "max_stdin_bytes": 10, "max_output_bytes": 1000, "max_file_bytes": 1000}
        result = run_bounded_process([str(self.python), "-c", "import time; time.sleep(.05); print('ok')"],
            b"", {"PATH": "/usr/bin:/bin", "LANG": "C", "LC_ALL": "C"}, self.root, limits)
        self.assertEqual(result["status"], "completed")
        self.assertGreaterEqual(result["target_cpu_seconds"], result["waited_child_cpu_seconds"])
        self.assertLess(result["supervisor_cpu_seconds"], 3.0)

    def test_nonzero_and_signal_exit_are_distinct(self):
        self._write_program("import sys\nsys.stdin.buffer.read()\nsys.exit(7)\n")
        frozen = self.freeze()
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())["status"], "nonzero_exit")

        self._write_program("import os, signal, sys\nsys.stdin.buffer.read()\nos.kill(os.getpid(), signal.SIGTERM)\n")
        protocol = self.protocol()
        protocol["program_budget"]["program_id"] = "sample-program-signal"
        protocol["program_budget"]["ledger_dir"] = str((self.ledger / "sample-program-signal").resolve())
        frozen = self.freeze(protocol, self.root / "frozen-signal")
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())["status"], "signaled")

    def test_large_simultaneous_stdout_stderr_are_drained_without_deadlock(self):
        self._write_program("import sys\nsys.stdout.buffer.write(b'o'*200000)\nsys.stdout.flush()\nsys.stderr.buffer.write(b'e'*200000)\nsys.stderr.flush()\n")
        frozen = self.freeze(self.protocol(max_output=500000))
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertEqual(receipt["stdout_bytes"], 200000)
        self.assertEqual(receipt["stderr_bytes"], 200000)

    def test_orphaned_descendant_is_terminated_after_parent_exits(self):
        pid_file = self.root / "child.pid"
        self._write_program(f"import subprocess, sys\np=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)'])\nopen({str(pid_file)!r},'w').write(str(p.pid))\n")
        frozen = self.freeze()
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 2)
        receipt = json.loads((frozen / "runs/preflight-unit/receipt.json").read_text())
        self.assertEqual(receipt["status"], "orphaned_process_group")
        child_pid = int(pid_file.read_text())
        deadline = time.monotonic() + 1
        while time.monotonic() < deadline:
            try:
                stat = Path(f"/proc/{child_pid}/stat")
                if not stat.exists() or stat.read_text().split()[2] == "Z":
                    break
            except OSError:
                break
            time.sleep(.01)
        else:
            self.fail("orphaned child remained running after process-group cleanup")

    def test_concurrent_duplicate_run_id_has_one_atomic_admission(self):
        self._write_program("import time, sys\ntime.sleep(.1)\nprint('finished')\n")
        protocol = self.protocol()
        protocol["runs"][0]["stdin_artifact"] = None
        frozen = self.freeze(protocol)
        def invoke():
            return self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(lambda _: invoke(), range(2)))
        details = [(result.returncode, result.stderr) for result in results]
        receipt_path = frozen / "runs/preflight-unit/receipt.json"
        receipt_detail = receipt_path.read_text() if receipt_path.exists() else "missing receipt"
        self.assertEqual(sorted(result.returncode for result in results), [0, 3],
                         f"{details}\n{receipt_detail}")
        state = json.loads((self.ledger / "sample-program/process-budget.json").read_text())
        self.assertEqual(state["invocations_reserved"], 1)

    def test_phase_order_and_repeated_run_are_rejected(self):
        frozen = self.freeze()
        result = self.command("run-program", "--protocol-dir", frozen, "--run-id", "campaign-step")
        self.assertEqual(result.returncode, 3)
        run = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(run.returncode, 0, run.stderr)
        again = self.command("run-program", "--protocol-dir", frozen, "--run-id", "preflight-unit")
        self.assertEqual(again.returncode, 3)


if __name__ == "__main__":
    unittest.main()
