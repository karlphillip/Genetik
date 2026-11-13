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
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
HARNESS = ROOT / "scripts" / "research_execution.py"
sys.path.insert(0, str(ROOT / "scripts"))
from research_execution import (GateError, continuation_next_action,
                                _resolve_authenticated_run_output,
                                _require_receipt_measurements,
                                _validate_controller_file_phase,
                                validate_continuation_events,
                                validate_continuation_history,
                                validate_continuation_phase_plan,
                                validate_run_output_list_binding)
from research_pipeline import PipelineDescriptorError, expand_argv


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

    def test_unused_ledger_accepts_one_hash_chained_software_successor(self):
        import research_execution
        from research_execution import read_frozen_program

        predecessor = self.freeze(destination=self.root / "predecessor")
        ledger_dir = self.ledger / "sample-program"
        original_event_bytes = (ledger_dir / "process-events.jsonl").read_bytes()
        original_events = [json.loads(line) for line in original_event_bytes.splitlines()]
        previous_sha = (predecessor / "PROGRAM.sha256").read_text().split()[0]
        successor_protocol = self.protocol()
        successor_protocol["protocol_id"] = "program-test-v2"
        successor_protocol["source_commit"] = "b" * 40
        source = self._write_source(successor_protocol)
        destination = self.root / "successor"
        result = self.command("successor-program", "--predecessor-dir", predecessor,
                              "--protocol", source, "--destination", destination)
        self.assertEqual(result.returncode, 0, result.stderr)

        with mock.patch.object(research_execution, "PROCESS_LEDGER_ROOT", self.ledger.resolve()):
            _, _, active_sha = read_frozen_program(destination)
        self.assertEqual(active_sha, (destination / "PROGRAM.sha256").read_text().split()[0])
        self.assertEqual((ledger_dir / "process-events.jsonl").read_bytes(), original_event_bytes)
        successor_event = json.loads((ledger_dir / "protocol-successor.json").read_text())
        self.assertEqual(successor_event["event"], "protocol_successor_authorized")
        self.assertEqual(successor_event["predecessor_protocol_sha256"], previous_sha)
        self.assertEqual(successor_event["successor_protocol_sha256"], active_sha)
        self.assertEqual(successor_event["event_index"], 1)
        self.assertEqual([event["event_sha256"] for event in original_events],
                         [json.loads(line)["event_sha256"] for line in
                          (ledger_dir / "process-events.jsonl").read_text().splitlines()])
        state = json.loads((ledger_dir / "process-budget.json").read_text())
        self.assertEqual(state["frozen_protocol_sha256"], active_sha)
        self.assertEqual(state["event_count"], 2)
        self.assertEqual(state["invocations_reserved"], 0)
        self.assertEqual(state["cpu_seconds_charged"], 0)
        self.assertEqual(state["wall_seconds_charged"], 0)

        with mock.patch.object(research_execution, "PROCESS_LEDGER_ROOT", self.ledger.resolve()):
            with self.assertRaisesRegex(GateError, "active|different|superseded"):
                read_frozen_program(predecessor)
        executed = self.command("run-program", "--protocol-dir", destination, "--run-id", "preflight-unit")
        self.assertEqual(executed.returncode, 0, executed.stderr)
        verified = self.command("verify-program", "--protocol-dir", destination)
        self.assertEqual(verified.returncode, 0, verified.stderr)
        self.assertEqual(json.loads(verified.stdout)["status"], "valid")
        another = json.loads(json.dumps(successor_protocol))
        another["source_commit"] = "c" * 40
        another["protocol_id"] = "program-test-v3"
        repeated = self.command("successor-program", "--predecessor-dir", predecessor,
                                "--protocol", self._write_source(another),
                                "--destination", self.root / "successor-2")
        self.assertNotEqual(repeated.returncode, 0)
        self.assertFalse((self.root / "successor-2").exists())

    def test_successor_state_recovers_after_atomic_record_before_budget_replace(self):
        import research_execution
        from research_execution import read_frozen_program

        predecessor = self.freeze(destination=self.root / "predecessor")
        protocol = self.protocol()
        protocol["protocol_id"] = "program-test-after-crash"
        protocol["source_commit"] = "d" * 40
        source = self._write_source(protocol)
        destination = self.root / "successor-after-crash"
        with (mock.patch.object(research_execution, "PROCESS_LEDGER_ROOT", self.ledger.resolve()),
              mock.patch.object(research_execution, "_replace_process_budget",
                                side_effect=OSError("simulated interruption after successor record"))):
            with self.assertRaisesRegex(OSError, "simulated interruption"):
                research_execution.successor_program_protocol(predecessor, source, destination)

        ledger_dir = self.ledger / "sample-program"
        interrupted_state = json.loads((ledger_dir / "process-budget.json").read_text())
        self.assertEqual(interrupted_state["frozen_protocol_sha256"],
                         (predecessor / "PROGRAM.sha256").read_text().split()[0])
        self.assertEqual(interrupted_state["event_count"], 1)
        self.assertTrue((ledger_dir / "protocol-successor.json").is_file())

        with mock.patch.object(research_execution, "PROCESS_LEDGER_ROOT", self.ledger.resolve()):
            _, _, active_sha = read_frozen_program(destination)
        recovered_state = json.loads((ledger_dir / "process-budget.json").read_text())
        self.assertEqual(recovered_state["frozen_protocol_sha256"], active_sha)
        self.assertEqual(recovered_state["event_count"], 2)
        self.assertEqual(recovered_state["invocations_reserved"], 0)

    def test_orphaned_successor_package_is_inert_and_retryable(self):
        import research_execution
        from research_execution import read_frozen_program

        predecessor = self.freeze(destination=self.root / "predecessor")
        protocol = self.protocol()
        protocol["protocol_id"] = "program-test-unpublished"
        protocol["source_commit"] = "e" * 40
        source = self._write_source(protocol)
        destination = self.root / "unpublished-successor"
        with (mock.patch.object(research_execution, "PROCESS_LEDGER_ROOT", self.ledger.resolve()),
              mock.patch.object(research_execution, "_atomic_write_successor_record",
                                side_effect=OSError("simulated interruption before activation"))):
            with self.assertRaisesRegex(OSError, "simulated interruption"):
                research_execution.successor_program_protocol(predecessor, source, destination)

        ledger_dir = self.ledger / "sample-program"
        original_state = json.loads((ledger_dir / "process-budget.json").read_text())
        self.assertEqual(original_state["event_count"], 1)
        self.assertFalse((ledger_dir / "protocol-successor.json").exists())
        with mock.patch.object(research_execution, "PROCESS_LEDGER_ROOT", self.ledger.resolve()):
            with self.assertRaises(GateError):
                read_frozen_program(destination)

        with mock.patch.object(research_execution, "PROCESS_LEDGER_ROOT", self.ledger.resolve()):
            repeated = research_execution.successor_program_protocol(predecessor, source, destination)
        self.assertEqual(repeated, (destination / "PROGRAM.sha256").read_text().split()[0])
        self.assertEqual(json.loads((ledger_dir / "process-budget.json").read_text())["event_count"], 2)

    def test_successor_rejects_science_budget_and_allowance_changes(self):
        import research_execution
        from research_execution import _validate_protocol_successor

        predecessor = self.protocol()
        with mock.patch.object(research_execution, "PROCESS_LEDGER_ROOT", self.ledger.resolve()):
            predecessor_config = research_execution._validate_bounded_program(predecessor)
        for field, mutate in (
            ("runs", lambda value: value[0].update({"stdin_artifact": None})),
            ("limits", lambda value: value.update({"max_address_space_bytes": value["max_address_space_bytes"] - 1024})),
        ):
            successor = json.loads(json.dumps(predecessor))
            successor["source_commit"] = "f" * 40
            successor["protocol_id"] = "program-test-v2"
            mutate(successor[field])
            with mock.patch.object(research_execution, "PROCESS_LEDGER_ROOT", self.ledger.resolve()):
                successor_config = research_execution._validate_bounded_program(successor)
                with self.assertRaisesRegex(GateError, "changes frozen science"):
                    _validate_protocol_successor(predecessor, successor, predecessor_config, successor_config)

    def test_successor_refuses_admission_after_any_invocation_reservation(self):
        import research_execution

        predecessor = self.freeze(destination=self.root / "predecessor")
        result = self.command("run-program", "--protocol-dir", predecessor, "--run-id", "preflight-unit")
        self.assertEqual(result.returncode, 0, result.stderr)
        successor = self.protocol()
        successor["source_commit"] = "9" * 40
        successor["protocol_id"] = "program-test-v2"
        with mock.patch.object(research_execution, "PROCESS_LEDGER_ROOT", self.ledger.resolve()):
            with self.assertRaisesRegex(GateError, "before any admission|unused ledger"):
                research_execution.successor_program_protocol(
                    predecessor, self._write_source(successor), self.root / "forbidden-successor")
        self.assertFalse((self.root / "forbidden-successor").exists())

    def test_successor_hash_chain_preserves_three_historical_event_hashes(self):
        from research_execution import (_active_protocol_sha256, _process_ledger_identity, _read_process_ledger_events,
                                        compact_json, sha256_bytes)

        ledger = self.root / "three-event-ledger"
        ledger.mkdir()
        old_protocol_sha = "1" * 64
        new_protocol_sha = "2" * 64
        allowance = {"invocations": 49, "cpu_seconds": 333.0, "wall_seconds": 426.0}
        plan = {"amendment_id": "continuation", "coordinator_allowance":
                {"cpu_seconds": 11.0, "exclusive_wall_seconds": 11.0},
                "overall_deadline_seconds": 426.0}
        budget = {"max_new_invocations": 49, "max_new_cpu_seconds": 333.0,
                  "max_new_wall_seconds": 426.0}
        identity = _process_ledger_identity({"budget": budget, "program_id": "study",
            "frozen_protocol_sha256": old_protocol_sha, "continuation_plan": plan})
        payloads = [
            {"event": "created", **identity},
            {"event": "historical_import", "import_id": "retained-v7"},
            {"event": "amendment_authorized", "authorization_sha256": "3" * 64,
             "allowance": allowance},
        ]
        previous = "0" * 64
        historical = []
        for payload in payloads:
            event = {**payload, "previous_hash": previous}
            previous = sha256_bytes(compact_json(event))
            event["event_sha256"] = previous
            historical.append(event)
        (ledger / "process-events.jsonl").write_bytes(
            b"".join(compact_json(event) + b"\n" for event in historical))
        event = {"schema": 1, "event": "protocol_successor_authorized", "program_id": "study",
                 "predecessor_protocol_sha256": old_protocol_sha,
                 "successor_protocol_sha256": new_protocol_sha,
                 "predecessor_program_dir": str(self.root / "v7"),
                 "successor_program_dir": str(self.root / "v8"), "event_index": 3,
                 "compatibility_sha256": "4" * 64, "authorization_sha256": "3" * 64,
                 "allowance": allowance, "previous_hash": previous}
        event["event_sha256"] = sha256_bytes(compact_json(event))
        (ledger / "protocol-successor.json").write_bytes(json.dumps(event, sort_keys=True).encode())

        events = _read_process_ledger_events(ledger)
        self.assertEqual(len(events), 4)
        self.assertEqual([item["event_sha256"] for item in events[:3]],
                         [item["event_sha256"] for item in historical])
        self.assertEqual(events[3]["event_sha256"], event["event_sha256"])
        self.assertEqual(_active_protocol_sha256(events), new_protocol_sha)

    def test_successor_allows_only_the_coordinator_supervisor_schema_upgrade(self):
        from research_execution import _validate_protocol_successor

        predecessor = {
            "schema": 1, "kind": "bounded_program", "protocol_id": "study-v7",
            "source_commit": "1" * 40,
            "program_budget": {"program_id": "study", "max_new_invocations": 49,
                               "max_new_cpu_seconds": 333, "max_new_wall_seconds": 426},
            "continuation_authorization": {"schema": 1, "amendment_id": "same-amendment",
                                            "prospective_allowance": {"invocations": 49}},
            "artifacts": [
                {"id": "source-execution", "role": "pipeline-dependency", "path": "/old/execution.py",
                 "sha256": "2" * 64},
                {"id": "authorization-amendment", "role": "authorization-amendment",
                 "path": "/data/authorization.json", "sha256": "3" * 64},
            ],
            "controls": ["control-a"], "run_matrix": ["run-a"],
            "acceptance_criteria": {"exact_recovery": 4}, "limits": {"memory": 512},
        }
        successor = json.loads(json.dumps(predecessor))
        successor["protocol_id"] = "study-v8"
        successor["source_commit"] = "4" * 40
        successor["continuation_authorization"]["schema"] = 2
        successor["continuation_authorization"]["coordinator_supervisor_artifact_id"] = "source-supervisor"
        successor["artifacts"][0]["path"] = "/new/execution.py"
        successor["artifacts"][0]["sha256"] = "5" * 64
        successor["artifacts"].append({"id": "source-supervisor", "role": "coordinator-entrypoint",
                                        "path": "/new/coordinator_supervisor.py", "sha256": "6" * 64})
        config = {"program_id": "study", "ledger_dir": self.ledger / "study"}
        compatibility = _validate_protocol_successor(predecessor, successor, config, config)
        self.assertEqual(len(compatibility), 64)

        changed_science = json.loads(json.dumps(successor))
        changed_science["controls"] = ["control-b"]
        with self.assertRaisesRegex(GateError, "changes frozen science"):
            _validate_protocol_successor(predecessor, changed_science, config, config)

    def test_successor_refuses_active_phase_and_started_deadline(self):
        from research_execution import _ensure_unstarted_ledger

        base_state = {"accounting_scope": "prospective_continuation_only",
            "invocations_reserved": 0, "pending": None,
            "qualification_status": "preflight_pending", "active_coordinator_phase": None,
            "terminal_status": None, "overall_start_monotonic_ns": None,
            "overall_deadline_monotonic_ns": None, "cpu_seconds_charged": 0.0,
            "wall_seconds_charged": 0.0, "coordinator_cpu_seconds_charged": 0.0,
            "coordinator_exclusive_wall_seconds_charged": 0.0}
        prefix = [{"event": "created"}, {"event": "historical_import"},
                  {"event": "amendment_authorized"}]
        for update in (
            {"active_coordinator_phase": {"phase_id": "primary_preflight"}},
            {"overall_start_monotonic_ns": 17, "overall_deadline_monotonic_ns": 443},
            {"terminal_status": "coordinator_memory_limit"},
        ):
            with self.assertRaisesRegex(GateError, "unused ledger"):
                _ensure_unstarted_ledger({**base_state, **update}, prefix)

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
            "assert sys.dont_write_bytecode\n"
            f"sys.path.insert(0,{str(ROOT / 'scripts')!r})\n"
            "from bounded_interactive_session import run_session\n"
            "import hashlib\n"
            "def pin(path): return {'executable':sys.executable,'sha256':hashlib.sha256(open(sys.executable,'rb').read()).hexdigest(),'runtime_files':[{'path':path,'sha256':hashlib.sha256(open(path,'rb').read()).hexdigest()}]}\n"
            "r=run_session([sys.executable," + repr(str(request)) + "],"
            "[sys.executable," + repr(str(response)) + "],cwd=pathlib.Path.cwd(),"
            "environment={'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','PYTHONDONTWRITEBYTECODE':'1'},"
            "wall_seconds=1.5,max_frame_bytes=1024,max_protocol_bytes=8192,max_requests=4,"
            "max_cpu_seconds=2.0,max_memory_bytes=512*1024*1024,"
            "request_identity=pin(" + repr(str(request)) + "),response_identity=pin(" + repr(str(response)) + "))\n"
            "print(json.dumps(r,sort_keys=True,separators=(',',':')))\n"
            "sys.exit(0 if r['status']=='completed' else 3)\n")
        protocol = self.protocol(max_wall=3.0, max_total_wall=6.0)
        protocol["command"]["environment"]["PYTHONDONTWRITEBYTECODE"] = "1"
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


class ContinuationLifecycleContractTests(unittest.TestCase):
    """Offline pure-state tests; these never create a process ledger or launch a program."""

    def setUp(self):
        self.plan = {
            "amendment_id": "amendment-test",
            "coordinator_phase_ids": ["primary_preflight", "recovery_preflight",
                                       "campaign_orchestration", "final_scoring_orchestration"],
            "slots": {"primary_preflight": ["p1", "p2"], "recovery_preflight": ["r1", "r2"],
                      "campaign": ["c1"], "final_scoring": ["s1"]},
        }
        self.events = [{"event": "program_started", "protocol_sha256": "a" * 64,
                        "amendment_id": self.plan["amendment_id"],
                        "start_monotonic_ns": 1, "deadline_monotonic_ns": 426_000_000_001,
                        "deadline_seconds": 426.0, "boot_id": "boot"}]

    def start_phase(self, phase, token):
        self.events.append({"event": "coordinator_phase_started", "protocol_sha256": "a" * 64,
            "amendment_id": self.plan["amendment_id"], "phase_id": phase, "token": token,
            "pid": 100, "pid_starttime_ticks": 1000,
            "boot_id": "boot", "start_monotonic_ns": 1000, "start_cpu_seconds": 1.0})

    def finish_phase(self, phase, token, status, category=None):
        self.events.append({"event": "coordinator_phase", "protocol_sha256": "a" * 64,
            "amendment_id": self.plan["amendment_id"], "phase_id": phase, "token": token,
            "pid": 100, "pid_starttime_ticks": 1000,
            "status": status, "receipt_sha256": "b" * 64,
            "failure_category": None if status == "completed" else (category or "stage_failure"),
            "coordinator_peak_rss_bytes": 4096, "coordinator_proc_vm_hwm_bytes": 4096,
            "coordinator_cpu_seconds": 0.1, "coordinator_exclusive_wall_seconds": 0.2})

    def supervise_phase(self, phase, token):
        self.events.append({"event": "coordinator_worker_supervised", "protocol_sha256": "a" * 64,
            "amendment_id": self.plan["amendment_id"], "phase_id": phase, "token": token,
            "script_path": "/pinned/coordinator.py", "script_sha256": "d" * 64,
            "pid": 100, "pid_starttime_ticks": 1000,
            "worker_self_peak_rss_bytes": 4096, "worker_self_vm_hwm_bytes": 4096,
            "wait4_ru_maxrss_including_waited_children_bytes": 8192,
            "supervisor_ru_maxrss_diagnostic_bytes": 8192,
            "supervisor_vm_hwm_bytes": 4096, "supervisor_vm_rss_bytes": 2048,
            "supervisor_memory_limit_bytes": 512 * 1024 * 1024,
            "supervisor_cpu_seconds": 0.01, "supervisor_exclusive_wall_seconds": 0.02,
            "status": "verified", "failure_category": None, "reason": None})

    def run_slots(self, ids, *, status="completed"):
        for run_id in ids:
            self.events.append({"event": "reserved", "run_id": run_id})
            self.events.append({"event": "completed", "run_id": run_id,
                                "status": status if run_id == ids[-1] else "completed"})

    def test_primary_success_retires_recovery_before_campaign(self):
        self.start_phase("primary_preflight", "primary-token")
        self.run_slots(self.plan["slots"]["primary_preflight"])
        self.assertEqual(continuation_next_action(self.plan, self.events)["state"],
                         "primary_qualification_required")
        self.events.append({"event": "qualification_prepared", "protocol_sha256": "a" * 64,
            "amendment_id": self.plan["amendment_id"], "qualification_path": "primary",
            "evidence_sha256": "c" * 64})
        self.finish_phase("primary_preflight", "primary-token", "completed")
        self.supervise_phase("primary_preflight", "primary-token")
        self.events.append({"event": "qualified", "protocol_sha256": "a" * 64,
            "amendment_id": self.plan["amendment_id"], "qualification_path": "primary",
            "preflight_evidence_sha256": "c" * 64, "coordinator_phase_receipt_sha256": "b" * 64})
        for run_id in self.plan["slots"]["recovery_preflight"]:
            self.events.append({"event": "recovery_retired", "run_id": run_id,
                "amendment_id": self.plan["amendment_id"], "charged_cpu_seconds": 0,
                "charged_wall_seconds": 0})
        action = validate_continuation_events(self.plan, self.events)
        self.assertEqual(action["state"], "campaign")
        self.assertEqual(action["next_run_id"], "c1")

    def test_no_primary_run_can_be_admitted_outside_measured_phase(self):
        self.events.append({"event": "reserved", "run_id": "p1"})
        with self.assertRaises(GateError):
            validate_continuation_events(self.plan, self.events)

    def test_primary_failure_opens_exactly_one_recovery_path(self):
        self.start_phase("primary_preflight", "primary-token")
        self.run_slots(["p1"], status="failed")
        self.finish_phase("primary_preflight", "primary-token", "failed")
        self.supervise_phase("primary_preflight", "primary-token")
        self.events.append({"event": "preflight_rejected", "protocol_sha256": "a" * 64,
            "amendment_id": self.plan["amendment_id"], "failure_category": "stage_failure",
            "reason": "synthetic failure"})
        self.assertEqual(continuation_next_action(self.plan, self.events)["state"], "recovery")
        self.assertEqual(continuation_next_action(self.plan, self.events)["next_run_id"], "r1")
        self.start_phase("recovery_preflight", "recovery-token")
        self.run_slots(self.plan["slots"]["recovery_preflight"], status="failed")
        self.finish_phase("recovery_preflight", "recovery-token", "failed")
        self.supervise_phase("recovery_preflight", "recovery-token")
        self.events.append({"event": "recovery_rejected", "protocol_sha256": "a" * 64,
            "amendment_id": self.plan["amendment_id"], "failure_category": "stage_failure",
            "reason": "recovery failed"})
        self.assertEqual(continuation_next_action(self.plan, self.events)["state"], "terminal_failure")

    def test_admission_refusal_is_terminal_and_never_enters_recovery(self):
        self.events.append({"event": "admission_refused", "protocol_sha256": "a" * 64,
            "amendment_id": self.plan["amendment_id"], "phase_id": "primary_preflight",
            "requested_phase_id": "primary_preflight", "failure_category": "admission_refused",
            "reason": "deadline expired before phase launch"})
        self.events.append({"event": "program_terminal", "status": "coordinator_admission_refused"})
        state = validate_continuation_events(self.plan, self.events)
        self.assertEqual(state["state"], "terminal_failure")

    def test_concurrent_admission_refusal_does_not_close_active_phase(self):
        self.start_phase("primary_preflight", "primary-token")
        self.events.append({"event": "coordinator_admission_attempt_refused",
            "protocol_sha256": "a" * 64, "amendment_id": self.plan["amendment_id"],
            "active_phase_token": "primary-token", "requested_phase_id": "primary_preflight",
            "requester_pid": 200, "requester_pid_starttime_ticks": 2000,
            "reason": "another coordinator phase is active"})
        action = validate_continuation_events(self.plan, self.events)
        self.assertEqual(action["state"], "primary_preflight")
        self.assertEqual(action["next_run_id"], "p1")
        self.finish_phase("primary_preflight", "primary-token", "completed")
        self.supervise_phase("primary_preflight", "primary-token")
        self.assertEqual(validate_continuation_events(self.plan, self.events)["state"],
                         "primary_preflight")

    def test_concurrent_admission_refusal_requires_current_active_token(self):
        self.start_phase("primary_preflight", "primary-token")
        self.events.append({"event": "coordinator_admission_attempt_refused",
            "protocol_sha256": "a" * 64, "amendment_id": self.plan["amendment_id"],
            "active_phase_token": "different-token", "requested_phase_id": "primary_preflight",
            "requester_pid": 200, "requester_pid_starttime_ticks": 2000,
            "reason": "forged contender"})
        with self.assertRaisesRegex(GateError, "malformed"):
            validate_continuation_events(self.plan, self.events)

    def test_coordinator_resource_failure_cannot_be_relabelled_as_recovery_eligible(self):
        self.start_phase("primary_preflight", "primary-token")
        self.run_slots(["p1"], status="failed")
        self.finish_phase("primary_preflight", "primary-token", "failed", "coordinator_phase_failure")
        self.supervise_phase("primary_preflight", "primary-token")
        self.events.append({"event": "preflight_rejected", "protocol_sha256": "a" * 64,
            "amendment_id": self.plan["amendment_id"],
            "failure_category": "coordinator_phase_failure", "reason": "RSS unavailable"})
        with self.assertRaisesRegex(GateError, "qualification boundary"):
            validate_continuation_events(self.plan, self.events)

    def test_recovery_phase_cannot_start_before_primary_failure_authorizes_it(self):
        self.start_phase("recovery_preflight", "recovery-token")
        with self.assertRaises(GateError):
            validate_continuation_events(self.plan, self.events)

    def test_campaign_cannot_begin_until_every_recovery_slot_is_retired(self):
        self.start_phase("primary_preflight", "primary-token")
        self.run_slots(self.plan["slots"]["primary_preflight"])
        self.events.append({"event": "qualification_prepared", "protocol_sha256": "a" * 64,
            "amendment_id": self.plan["amendment_id"], "qualification_path": "primary",
            "evidence_sha256": "c" * 64})
        self.finish_phase("primary_preflight", "primary-token", "completed")
        self.supervise_phase("primary_preflight", "primary-token")
        self.events.append({"event": "qualified", "protocol_sha256": "a" * 64,
            "amendment_id": self.plan["amendment_id"], "qualification_path": "primary",
            "preflight_evidence_sha256": "c" * 64, "coordinator_phase_receipt_sha256": "b" * 64})
        self.events.append({"event": "recovery_retired", "run_id": "r1",
            "amendment_id": self.plan["amendment_id"], "charged_cpu_seconds": 0,
            "charged_wall_seconds": 0})
        action = validate_continuation_events(self.plan, self.events)
        self.assertEqual(action["state"], "retire_recovery")

    def test_history_import_preserves_exact_measurements_and_unknowns(self):
        history = {
            "source_protocol_artifact_id": "historical-protocol",
            "evidence_manifest_artifact_id": "historical-manifest",
            "prior_declared_invocation_cap": 45,
            "known_governed_preflight": {
                "count": 16, "status": "successful", "cpu_seconds": None,
                "stage_wall_seconds": 4.365234316792339,
                "stage_wall_scope": "sum_of_16_bounded_stage_wall_records_only; coordinator_total_unknown",
                "receipt_sha256_by_version": {
                    "v2": "8d0f868433735f8188135b7bc7f493332622e66caa8d3ca1612921a2f0049263",
                    "v3": "6c2bb1b4106b2baa674c925f034a20f7740d44512ea07d1051bfbc258a2db581",
                    "v4": "ac8e835f2b9fdc1dcbf565737c8b1df83c4be3a8e9c9c70a73b9b5727f776074",
                    "v5": "6d85cee6426bde045ffcd3dd88addb6493cbb6eac74fd77ea66d21f3f26fb540",
                },
            },
            "unmeasured_pre_freeze_executions": {"count": 2, "cpu_seconds": None, "wall_seconds": None},
            "prior_v1_activity": {"invocations": None, "cpu_seconds": None, "wall_seconds": None},
        }
        self.assertEqual(validate_continuation_history(history), history)
        for mutate in (
            lambda value: value["known_governed_preflight"].update(cpu_seconds=0.0),
            lambda value: value["known_governed_preflight"].update(stage_wall_seconds=0.0),
            lambda value: value["unmeasured_pre_freeze_executions"].update(wall_seconds=0.0),
            lambda value: value["prior_v1_activity"].update(invocations=0),
        ):
            corrupted = json.loads(json.dumps(history))
            mutate(corrupted)
            with self.assertRaises(GateError):
                validate_continuation_history(corrupted)

    def test_amended_phase_plan_orders_recovery_before_science(self):
        slots = {"primary_preflight": ["p"], "recovery_preflight": ["r"],
                 "campaign": ["c"], "final_scoring": ["s"]}
        runs = [{"run_id": "p", "phase_id": "preflight"},
                {"run_id": "r", "phase_id": "recovery"},
                {"run_id": "c", "phase_id": "campaign"},
                {"run_id": "s", "phase_id": "scoring"}]
        plan = [{"phase_id": "primary_preflight", "run_ids": ["p"]},
                {"phase_id": "recovery_preflight", "run_ids": ["r"]},
                {"phase_id": "campaign", "run_ids": ["c"]},
                {"phase_id": "final_scoring", "run_ids": ["s"]}]
        self.assertEqual(validate_continuation_phase_plan(plan, runs, slots), ["p", "r", "c", "s"])
        late_recovery = [plan[0], plan[2], plan[3], plan[1]]
        with self.assertRaises(GateError):
            validate_continuation_phase_plan(late_recovery, runs, slots)
        wrong_slot = [*plan]
        wrong_slot[1] = {"phase_id": "recovery_preflight", "run_ids": ["c"]}
        with self.assertRaises(GateError):
            validate_continuation_phase_plan(wrong_slot, runs, slots)

    def test_recovery_preflight_controller_files_and_receipt_binding(self):
        _validate_controller_file_phase("preflight", "run_manifest")
        _validate_controller_file_phase("recovery", "run_manifest")
        _validate_controller_file_phase("recovery", "assessment_bundle")
        with self.assertRaises(GateError):
            _validate_controller_file_phase("campaign", "run_manifest")
        receipt = {"coordinator_cpu_seconds": 0.125,
                   "coordinator_exclusive_wall_seconds": 0.25,
                   "coordinator_peak_rss_bytes": 4096}
        _require_receipt_measurements(receipt, dict(receipt), tuple(receipt))
        altered = dict(receipt, coordinator_cpu_seconds=0.126)
        with self.assertRaises(GateError):
            _require_receipt_measurements(receipt, altered, tuple(receipt))

    def test_begin_phase_writes_start_events_to_canonical_ledger_directory(self):
        from research_execution import begin_coordinator_phase
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            frozen_dir, ledger = root / "frozen", root / "ledger"
            frozen_dir.mkdir()
            ledger.mkdir()
            (ledger / "process-budget.lock").write_bytes(b"")
            (ledger / "process-events.jsonl").write_bytes(b"")
            state = {"ledger_head": "0" * 64, "event_count": 0,
                     "active_coordinator_phase": None, "terminal_status": None,
                     "overall_start_monotonic_ns": None,
                     "overall_deadline_monotonic_ns": None, "kernel_boot_id": None}
            (ledger / "process-budget.json").write_text(json.dumps(state))
            plan = {"amendment_id": "test-amendment",
                    "coordinator_phase_ids": ["primary_preflight"],
                    "overall_deadline_seconds": 426.0,
                    "slots": {"primary_preflight": ["p1"], "recovery_preflight": ["r1"],
                              "campaign": ["c1"], "final_scoring": ["s1"]}}
            config = {"continuation_plan": plan, "ledger_dir": ledger}
            with (mock.patch("research_execution.read_frozen_program", return_value=({}, config, "a" * 64)),
                  mock.patch("research_execution._verify_process_ledger", return_value=[]),
                  mock.patch("research_execution.validate_continuation_events",
                             return_value={"state": "primary_preflight"}),
                  mock.patch("research_execution._enforce_coordinator_address_space"),
                  mock.patch("research_execution._self_peak_rss_bytes", return_value=1024),
                  mock.patch("research_execution._coordinator_memory_measurements", return_value=(1024, 1024, 512)),
                  mock.patch("research_execution._process_starttime_ticks", return_value=12345),
                  mock.patch("research_execution._kernel_boot_id", return_value="test-boot")):
                begin_coordinator_phase(frozen_dir, "primary_preflight")
            rows = [json.loads(line) for line in (ledger / "process-events.jsonl").read_text().splitlines()]
            self.assertEqual([row["event"] for row in rows], ["program_started", "coordinator_phase_started"])
            self.assertFalse((frozen_dir / "process-events.jsonl").exists())

    def test_coordinator_cpu_guard_uses_user_and_system_cpu_timer(self):
        from research_execution import coordinator_phase_guard
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ledger = root / "ledger"
            ledger.mkdir()
            state = {"active_coordinator_phase": {"token": "t", "phase_id": "campaign_orchestration",
                     "pid": os.getpid(), "start_monotonic_ns": time.monotonic_ns(),
                     "start_cpu_seconds": 0.0},
                     "max_coordinator_cpu_seconds": 11.0,
                     "max_coordinator_exclusive_wall_seconds": 11.0,
                     "coordinator_cpu_seconds_charged": 0.0,
                     "coordinator_exclusive_wall_seconds_charged": 0.0,
                     "overall_deadline_monotonic_ns": time.monotonic_ns() + 20_000_000_000}
            (ledger / "process-budget.json").write_text(json.dumps(state))
            (ledger / "process-events.jsonl").write_text("")
            plan = {"slots": {"primary_preflight": [], "recovery_preflight": [],
                              "campaign": ["c1"], "final_scoring": []}}
            config = {"continuation_plan": plan, "ledger_dir": ledger}
            timer_calls = []
            with (mock.patch("research_execution.read_frozen_program", return_value=({}, config, "a" * 64)),
                  mock.patch("research_execution._self_cpu_seconds", return_value=0.01),
                  mock.patch("research_execution.signal.getitimer", return_value=(0.0, 0.0)),
                  mock.patch("research_execution.signal.signal"),
                  mock.patch("research_execution.signal.setitimer",
                             side_effect=lambda which, seconds: timer_calls.append((which, seconds)))):
                with coordinator_phase_guard(root, "t"):
                    pass
            self.assertIn(signal.ITIMER_PROF, [which for which, _ in timer_calls])
            self.assertNotIn(signal.ITIMER_VIRTUAL, [which for which, _ in timer_calls])

    def test_ordered_candidate_output_fan_in_binding_is_strict(self):
        prior = {"run-a": {"stage_id": "runner"}, "run-b": {"stage_id": "runner"}}
        refs = [{"kind": "run_output", "run_id": "run-a", "slot": "candidate_outputs"},
                {"kind": "run_output", "run_id": "run-b", "slot": "candidate_outputs"}]
        self.assertEqual(validate_run_output_list_binding(refs, "candidate_outputs", "verify", prior), refs)
        with self.assertRaises(GateError):
            validate_run_output_list_binding(refs, "verified_receipts", "verify", prior)
        with self.assertRaises(GateError):
            validate_run_output_list_binding([refs[0], refs[0]], "candidate_outputs", "verify", prior)
        with self.assertRaises(GateError):
            validate_run_output_list_binding([*refs, {"kind": "run_output", "run_id": "missing",
                                                       "slot": "candidate_outputs"}],
                                             "candidate_outputs", "verify", prior)
        with self.assertRaises(GateError):
            validate_run_output_list_binding([{"kind": "artifact", "run_id": "run-a",
                                                "slot": "candidate_outputs"}],
                                             "candidate_outputs", "verify", prior)

    def test_candidate_output_argv_expansion_preserves_order_and_scalar_behavior(self):
        first, second = Path("/tmp/candidate-a.json"), Path("/tmp/candidate-b.json")
        descriptor = {"stages": {"verifier": {"argv": ["/usr/bin/python3", "verify.py",
                                                             "--candidate-output-index", "{candidate_outputs}"]}}}
        self.assertEqual(expand_argv(descriptor, "verifier", {
            "candidate_outputs": [str(first), str(second)]}),
            ["/usr/bin/python3", "verify.py", "--candidate-output-index",
             str(first), str(second)])
        scalar_descriptor = {"stages": {"runner": {"argv": ["/usr/bin/python3", "runner.py",
                                                                  "--candidate-output", "{candidate_outputs}"]}}}
        self.assertEqual(expand_argv(scalar_descriptor, "runner", {"candidate_outputs": str(first)}),
                         ["/usr/bin/python3", "runner.py", "--candidate-output", str(first)])
        with self.assertRaises(PipelineDescriptorError):
            expand_argv(descriptor, "verifier", {"candidate_outputs": [str(first), "relative.json"]})

    def test_candidate_output_resolution_authenticates_each_receipt_and_hash_in_order(self):
        from research_execution import _resolve_pipeline_inputs, verify_program
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            protocol_sha = "a" * 64
            events = []
            refs = []
            expected = []
            for run_id, payload in (("run-a", b"first"), ("run-b", b"second")):
                run_dir = root / "runs" / run_id
                output_dir = run_dir / "outputs"
                output_dir.mkdir(parents=True)
                output = output_dir / "candidate_outputs.json"
                output.write_bytes(payload)
                receipt_path = run_dir / "receipt.json"
                receipt = {"protocol_sha256": protocol_sha, "run_id": run_id,
                           "invocation_id": f"inv-{run_id}", "status": "completed",
                           "output_slots": {"candidate_outputs": {"path": str(output.resolve()),
                                                                    "sha256": sha(output)}}}
                receipt_path.write_text(json.dumps(receipt, sort_keys=True))
                events.append({"event": "completed", "protocol_sha256": protocol_sha,
                               "run_id": run_id, "invocation_id": f"inv-{run_id}",
                               "status": "completed", "receipt_path": str(receipt_path.resolve()),
                               "receipt_sha256": sha(receipt_path)})
                refs.append({"kind": "run_output", "run_id": run_id, "slot": "candidate_outputs"})
                expected.append((output.resolve(), sha(output)))
            resolved = [_resolve_authenticated_run_output(root, protocol_sha, events, ref) for ref in refs]
            self.assertEqual(resolved, expected)
            run_by_id = {run_id: {"stage_id": "runner", "run_id": run_id}
                         for run_id in ("run-a", "run-b")}
            bindings = [dict(reference) for reference in refs]
            config = {"run_by_id": run_by_id | {"verify": {"run_id": "verify", "stage_id": "verifier"}},
                "pipeline_descriptor": {"schema": 1, "stages": {"verifier": {
                    "argv": ["/usr/bin/python3", "verify.py", "--candidate-output-index", "{candidate_outputs}"],
                    "input_roles": ["candidate_outputs"], "output_roles": ["verified_receipts"]}}},
                "artifacts": {}, "pipeline_identity": None,
                "commands_by_stage": {}, "ledger_dir": root / "ledger", "continuation_plan": None}
            target_run = {"run_id": "verify", "stage_id": "verifier", "phase_id": "scoring",
                          "slot_bindings": {"candidate_outputs": bindings}}
            verify_run_dir = root / "runs" / "verify"
            verify_run_dir.mkdir(parents=True)
            argv, input_hashes, output_slots, input_paths = _resolve_pipeline_inputs(
                root, config, target_run, verify_run_dir, protocol_sha, events)
            self.assertEqual(argv[-2:], [str(path) for path, _ in expected])
            self.assertEqual(input_hashes["candidate_outputs"], [digest for _, digest in expected])
            self.assertEqual(input_paths["candidate_outputs"], [str(path) for path, _ in expected])

            # Round-trip the resolved list through a normal run receipt and the
            # same immutable-input verification used by verify_program().
            stdout_path, stderr_path = verify_run_dir / "stdout.bin", verify_run_dir / "stderr.bin"
            stdout_path.write_bytes(b"")
            stderr_path.write_bytes(b"")
            verified_output = verify_run_dir / "outputs" / "verified_receipts.json"
            verified_output.parent.mkdir()
            verified_output.write_bytes(b"[]")
            receipt = {"run_id": "verify", "invocation_id": "inv-verify", "status": "completed",
                       "stdout_sha256": sha(stdout_path), "stderr_sha256": sha(stderr_path),
                       "input_slot_paths": input_paths, "input_slot_hashes": input_hashes,
                       "output_slots": {"verified_receipts": {"path": str(verified_output.resolve()),
                                                                 "sha256": sha(verified_output)}}}
            receipt_path = verify_run_dir / "receipt.json"
            receipt_path.write_text(json.dumps(receipt, sort_keys=True))
            (root / "ledger").mkdir()
            (root / "ledger" / "process-budget.json").write_text(json.dumps({
                "pending": None, "invocations_reserved": 1,
                "cpu_seconds_charged": 1.0, "wall_seconds_charged": 1.0}))
            target_events = [{"event": "completed", "protocol_sha256": protocol_sha,
                "run_id": "verify", "invocation_id": "inv-verify", "status": "completed",
                "receipt_path": str(receipt_path.resolve()), "receipt_sha256": sha(receipt_path)},
                {"event": "qualified", "protocol_sha256": protocol_sha}]
            with (mock.patch("research_execution.read_frozen_program",
                             return_value=({}, config, protocol_sha)),
                  mock.patch("research_execution._verify_process_ledger", return_value=target_events)):
                self.assertEqual(verify_program(root)["status"], "valid")
                second_input = Path(input_paths["candidate_outputs"][1])
                second_input.write_bytes(b"mutated")
                with self.assertRaises(GateError):
                    verify_program(root)
            bad_slot = {"kind": "run_output", "run_id": "run-a", "slot": "missing_slot"}
            with self.assertRaises(GateError):
                _resolve_authenticated_run_output(root, protocol_sha, events, bad_slot)
            second_output = root / "runs" / "run-b" / "outputs" / "candidate_outputs.json"
            second_output.unlink()
            with self.assertRaises(GateError):
                _resolve_authenticated_run_output(root, protocol_sha, events, refs[1])
            second_output.write_bytes(b"altered")
            with self.assertRaises(GateError):
                _resolve_authenticated_run_output(root, protocol_sha, events, refs[1])

    def test_process_ledger_roundtrips_emitted_coordinator_receipt_measurements(self):
        from research_execution import (_process_ledger_identity, _verify_process_ledger,
                                        canonical_json, compact_json, sha256_bytes)

        def build_ledger(cpu_event):
            with tempfile.TemporaryDirectory() as temp:
                ledger = Path(temp)
                protocol_sha = "a" * 64
                history = {"prior": "unknown"}
                source_hashes = {"source_protocol": "b" * 64, "evidence_manifest": "c" * 64}
                allowance = {"invocations": 49, "cpu_seconds": 333.0, "wall_seconds": 426.0}
                plan = {"amendment_id": "amendment", "coordinator_allowance":
                        {"cpu_seconds": 11.0, "exclusive_wall_seconds": 11.0},
                        "overall_deadline_seconds": 426.0, "history": history,
                        "history_artifact_sha256": source_hashes, "allowance": allowance,
                        "authorization_sha256": "d" * 64}
                budget = {"max_new_invocations": 49, "max_new_cpu_seconds": 333.0,
                          "max_new_wall_seconds": 426.0}
                identity = _process_ledger_identity({"budget": budget, "program_id": "program",
                    "frozen_protocol_sha256": protocol_sha, "continuation_plan": plan})
                receipt_path = ledger / "coordinator-receipt.json"
                receipt = {"invocation_id": "inv-1", "run_id": "r1",
                           "measurement_method": "RUSAGE_SELF_AND_EXCLUSIVE_WALL",
                           "cpu_seconds": 0.125, "exclusive_wall_seconds": 0.25}
                receipt_path.write_text(json.dumps(receipt, sort_keys=True))
                payloads = [
                    {"event": "created", **identity},
                    {"event": "historical_import", "history": history,
                     "history_sha256": sha256_bytes(canonical_json(history)),
                     "source_artifact_sha256": source_hashes},
                    {"event": "amendment_authorized", "allowance": allowance,
                     "authorization_sha256": "d" * 64},
                    {"event": "reserved", "invocation_id": "inv-1", "run_id": "r1"},
                    {"event": "completed", "protocol_sha256": protocol_sha,
                     "invocation_id": "inv-1", "run_id": "r1", "status": "completed",
                     "charged_cpu_seconds": 1.0, "charged_wall_seconds": 2.0},
                    {"event": "coordinator_accounted", "protocol_sha256": protocol_sha,
                     "invocation_id": "inv-1", "run_id": "r1",
                     "receipt_path": str(receipt_path.resolve()),
                     "receipt_sha256": sha(receipt_path),
                     "coordinator_cpu_seconds": cpu_event,
                     "coordinator_exclusive_wall_seconds": 0.25},
                ]
                previous = "0" * 64
                encoded = []
                for payload in payloads:
                    event = {**payload, "previous_hash": previous}
                    previous = sha256_bytes(compact_json(event))
                    encoded.append(compact_json({**event, "event_sha256": previous}) + b"\n")
                events_path = ledger / "process-events.jsonl"
                events_path.write_bytes(b"".join(encoded))
                state = {**identity, "invocations_reserved": 1, "cpu_seconds_charged": 1.0,
                         "wall_seconds_charged": 2.0, "pending": None,
                         "qualification_status": "preflight_pending", "event_count": len(payloads),
                         "ledger_head": previous, "coordinator_cpu_seconds_charged": cpu_event,
                         "coordinator_exclusive_wall_seconds_charged": 0.25,
                         "overall_start_monotonic_ns": None, "overall_deadline_monotonic_ns": None,
                         "kernel_boot_id": None, "terminal_status": None,
                         "active_coordinator_phase": None}
                (ledger / "process-budget.json").write_text(json.dumps(state, sort_keys=True))
                with mock.patch("research_execution.validate_continuation_events"):
                    if cpu_event == 0.125:
                        verified = _verify_process_ledger(ledger, state, protocol_sha, plan)
                        self.assertEqual(len(verified), len(payloads))
                    else:
                        with self.assertRaisesRegex(GateError, "receipt disagrees"):
                            _verify_process_ledger(ledger, state, protocol_sha, plan)

        build_ledger(0.125)
        build_ledger(0.126)


if __name__ == "__main__":
    unittest.main()
