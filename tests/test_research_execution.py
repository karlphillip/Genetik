import json
import hashlib
import importlib.util
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest


ROOT = Path(__file__).resolve().parents[1]
HARNESS = ROOT / "scripts" / "research_execution.py"
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "tests"))
from test_scientific_pipeline_preflight import PIPELINE


class ResearchExecutionHarnessTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.solver = self.root / "fake_solver.py"
        self.solver.write_text(
            "#!/usr/bin/env python3\n"
            "import os, signal, sys, time\n"
            "query = sys.stdin.buffer.read().decode().strip()\n"
            "mode = query\n"
            "if mode == 'timeout': time.sleep(5)\n"
            "elif mode == 'unsat': print('unsat')\n"
            "elif mode == 'unsat29': sys.stdout.write('unsat\\n' * 29)\n"
            "elif mode == 'sat': print('sat')\n"
            "elif mode == 'unknown': print('unknown')\n"
            "elif mode == 'large': sys.stdout.write('x' * 100000)\n"
            "elif mode == 'both-large': sys.stdout.write('x' * 3000); sys.stderr.write('y' * 3000)\n"
            "elif mode == 'cpu':\n while True: pass\n"
            "elif mode == 'child':\n import subprocess, pathlib\n child = subprocess.Popen(['sleep', '10'])\n pathlib.Path(os.environ['CHILD_PID_FILE']).write_text(str(child.pid))\n time.sleep(5)\n"
            "elif mode == 'orphan':\n import subprocess, pathlib\n child = subprocess.Popen(['sleep', '10'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)\n pathlib.Path(os.environ['CHILD_PID_FILE']).write_text(str(child.pid))\n"
            "elif mode == 'bad': print('error', file=sys.stderr); sys.exit(2)\n"
            "elif mode == 'conflict': print('sat'); print('unsat')\n"
            "elif mode == 'mixed-unsat-unknown': print('unsat'); print('unknown (INCOMPLETE)')\n"
            "elif mode == 'mixed-unsat-fail': print('unsat'); print('fail')\n"
            "elif mode in ('parse-error', 'parse-error-alt'): print('Parse Error: broken grammar')\n"
            "elif mode == 'smt-error': print('(error \\\"bad query\\\")')\n"
            "elif mode == 'unknown-incomplete': print('unknown (INCOMPLETE)')\n"
            "elif mode == 'fail': print('fail')\n"
            "elif mode == 'cvc-timeout':\n sys.stderr.write('cvc5 interrupted by timeout.\\n'); sys.stderr.flush(); os.kill(os.getpid(), signal.SIGABRT)\n"
            "elif mode == 'sigkill': os.kill(os.getpid(), signal.SIGKILL)\n"
            "elif mode == 'oom-sigkill':\n sys.stderr.write('out of memory\\n'); sys.stderr.flush(); os.kill(os.getpid(), signal.SIGKILL)\n"
            "elif mode == 'malformed-timeout': print('Parse Error: parser timed out while reading token', file=sys.stderr); sys.exit(2)\n"
            "elif mode == 'external-term': os.kill(os.getpid(), signal.SIGTERM)\n"
            "elif mode == 'math-invalid': print('mathematical_invalid')\n"
            "else: print('model ' + str(len(query)))\n"
        )
        self.solver.chmod(0o755)
        self.checker = self.root / "fake_checker.py"
        self.checker.write_text(
            "#!/usr/bin/env python3\n"
            "import hashlib, json, pathlib, sys\n"
            "c = json.load(sys.stdin)\n"
            "assert hashlib.sha256(pathlib.Path(c['stdout_path']).read_bytes()).hexdigest() == c['stdout_sha256']\n"
            "assert hashlib.sha256(pathlib.Path(c['stderr_path']).read_bytes()).hexdigest() == c['stderr_sha256']\n"
            "sys.stdout.buffer.write(b'checker-ok\\n')\n"
        )
        self.checker.chmod(0o755)
        self.query = self.root / "query.sygus"
        self.query.write_text("(check-synth)\n")
        self.artifact = self.root / "frozen-input.json"
        self.artifact.write_text('{"fixture":"fixed"}\n')
        self.assessment_artifact = self.root / "assessment.json"
        self.assessment_artifact.write_text('{"assessment":"separate"}\n')
        self.artifact2 = self.root / "frozen-input-2.json"
        self.artifact2.write_text('{"fixture":"second-control"}\n')
        self.assessment_artifact2 = self.root / "assessment-2.json"
        self.assessment_artifact2.write_text('{"assessment":"second-control"}\n')
        self.pipeline_script = self.root / "synthetic_pipeline.py"
        self.pipeline_script.write_text(PIPELINE)
        self.pipeline_script.chmod(0o755)
        self.history = self.root / "prior-ledger.json"
        self.history.write_text(json.dumps({"query_invocations_counted": 11, "declared_query_ceiling": 10, "ceiling_overrun": 1}))
        self.program_root = self.root / "lifetime-ledgers"
        self.protocol_count = 0

    def tearDown(self):
        self.tmp.cleanup()

    def command(self, *args, env=None):
        merged = os.environ.copy()
        merged["GENETIK_RESEARCH_LEDGER_ROOT"] = str(self.program_root)
        args = list(args)
        if args and args[0] == "run" and "--query-id" not in args:
            args.extend(("--query-id", "q0"))
        if env:
            merged.update(env)
        return subprocess.run(
            [sys.executable, str(HARNESS), *map(str, args)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=merged,
        )

    def make_protocol(self, *, calls=4, query_bytes=4096, output_bytes=4096, wall=1, cpu=1, program_dir=None, program_id=None, new_cpu=1000, new_wall=1000, query_uses=None, query_args=None, group_id=None, group_calls=None, group_path=None, group_stages=None, max_syntax_retries=1, stage_id=None, retry_attempt=None, retry_of_run_id=None, query_content=None, attestation_required=False, checker_limits=None, expect_freeze_error=None):
        import hashlib

        self.protocol_count += 1
        program_id = program_id or f"adapter-continuation-test-{self.protocol_count}"
        query_path = self.query
        if query_content is not None:
            query_path = self.root / f"query-{self.protocol_count}.txt"
            query_path.write_text(query_content + "\n")
        source = self.root / "protocol-input.json"
        protocol = {
            "schema": 1,
            "protocol_id": f"unit-test-{self.protocol_count}",
            "scope": "fake executable only; harness enforcement test",
            "hypothesis": "the gate admits no more than the frozen number of fake calls",
            "grammar": "opaque test input bytes; not a mathematical campaign",
            "acceptance_criteria": ["every admitted invocation is recorded exactly once"],
            "source_commit": "ad52d4f39d3bf28941cf8eb4f901eccddbbc497a",
            "program_budget": {
                "ledger_dir": str(program_dir or (self.program_root / program_id)),
                "program_id": program_id,
                "historical_invocations": 11,
                "historical_limit": 10,
                "historical_overrun": 1,
                "max_lifetime_invocations": 11 + calls,
                "max_new_cpu_seconds": new_cpu,
                "max_new_wall_seconds": new_wall,
                "max_query_cpu_seconds": 10,
                "max_query_wall_seconds": 10,
                "max_address_space_bytes": 512 * 1024 * 1024,
                "max_query_bytes": 4096,
                "max_output_bytes": 4096,
                "historical_evidence": {"path": str(self.history), "sha256": hashlib.sha256(self.history.read_bytes()).hexdigest()},
            },
            "subprocess_policy": "forbidden",
            "controls": [
                {"id": "fixture", "training_artifact": "training-fixture",
                 "training_sha256": hashlib.sha256(self.artifact.read_bytes()).hexdigest(),
                 "assessment_artifact": "assessment-fixture",
                 "assessment_sha256": hashlib.sha256(self.assessment_artifact.read_bytes()).hexdigest()},
                {"id": "fixture-2", "training_artifact": "training-fixture-2",
                 "training_sha256": hashlib.sha256(self.artifact2.read_bytes()).hexdigest(),
                 "assessment_artifact": "assessment-fixture-2",
                 "assessment_sha256": hashlib.sha256(self.assessment_artifact2.read_bytes()).hexdigest()},
            ],
            "run_matrix": [
                {"run_id": "q0-fixture", "control_id": "fixture", "method": "test", "seed": 0,
                 "training_artifact": "training-fixture", "assessment_artifact": "assessment-fixture"},
                {"run_id": "q0-fixture-2", "control_id": "fixture-2", "method": "test", "seed": 0,
                 "training_artifact": "training-fixture-2", "assessment_artifact": "assessment-fixture-2"},
            ],
            "frozen_artifacts": [
                {"id": "training-fixture", "control_id": "fixture", "role": "training", "path": str(self.artifact), "sha256": hashlib.sha256(self.artifact.read_bytes()).hexdigest()},
                {"id": "assessment-fixture", "control_id": "fixture", "role": "assessment", "path": str(self.assessment_artifact), "sha256": hashlib.sha256(self.assessment_artifact.read_bytes()).hexdigest()},
                {"id": "training-fixture-2", "control_id": "fixture-2", "role": "training", "path": str(self.artifact2), "sha256": hashlib.sha256(self.artifact2.read_bytes()).hexdigest()},
                {"id": "assessment-fixture-2", "control_id": "fixture-2", "role": "assessment", "path": str(self.assessment_artifact2), "sha256": hashlib.sha256(self.assessment_artifact2.read_bytes()).hexdigest()},
                {"id": "query-fixture", "role": "query", "path": str(query_path), "sha256": hashlib.sha256(query_path.read_bytes()).hexdigest()},
                {"id": "checker-fixture", "role": "checker", "path": str(self.checker), "sha256": hashlib.sha256(self.checker.read_bytes()).hexdigest()},
                {"id": "pipeline-runtime", "role": "pipeline-executable", "path": str(Path(sys.executable).resolve()), "sha256": hashlib.sha256(Path(sys.executable).read_bytes()).hexdigest()},
                {"id": "pipeline-adapter", "role": "pipeline-dependency", "path": str(self.pipeline_script.resolve()), "sha256": hashlib.sha256(self.pipeline_script.read_bytes()).hexdigest()},
                {"id": "admission-tool", "role": "admission-tool", "path": str(ROOT / "scripts" / "scientific_pipeline_preflight.py"), "sha256": hashlib.sha256((ROOT / "scripts" / "scientific_pipeline_preflight.py").read_bytes()).hexdigest()},
                {"id": "execution-gate", "role": "execution-gate", "path": str(HARNESS), "sha256": hashlib.sha256(HARNESS.read_bytes()).hexdigest()},
            ],
            "queries": ([{"id": "q0", "path": str(query_path), "sha256": hashlib.sha256(query_path.read_bytes()).hexdigest(), "max_uses": query_uses if query_uses is not None else calls, "args": query_args or []}] if calls else []),
            "solver": {
                "executable": str(self.solver),
                "version": "test-fixture-1",
                "sha256": hashlib.sha256(self.solver.read_bytes()).hexdigest(),
                "fixed_args": [],
                "environment": {
                    "PATH": "/usr/bin:/bin",
                    "LANG": "C",
                    "LC_ALL": "C",
                    "CHILD_PID_FILE": str(self.root / "child.pid"),
                },
            },
            "limits": {
                "max_solver_invocations": calls,
                "max_query_wall_seconds": wall,
                "max_query_cpu_seconds": cpu,
                "max_total_wall_seconds": min(calls * wall, new_wall),
                "max_total_cpu_seconds": min(calls * cpu, new_cpu),
                "max_address_space_bytes": 512 * 1024 * 1024,
                "max_query_bytes": query_bytes,
                "max_output_bytes": output_bytes,
            },
        }
        if group_id is not None:
            canonical_group_path = (Path(program_dir) if program_dir else (self.program_root / program_id)) / "execution-groups" / group_id
            protocol["execution_group"] = {
                "group_id": group_id,
                "ledger_dir": str(group_path or canonical_group_path),
                "max_invocations": group_calls if group_calls is not None else calls,
            }
            if group_stages is not None:
                protocol["execution_group"]["stages"] = [
                    {**stage, **({"checker_sha256": hashlib.sha256(self.checker.read_bytes()).hexdigest()} if attestation_required else {})}
                    for stage in group_stages
                ]
                protocol["execution_group"]["max_syntax_retries"] = max_syntax_retries
                if attestation_required:
                    protocol["execution_group"]["attestation_required"] = True
                    protocol["execution_group"]["checker_limits"] = checker_limits or {
                        "max_attempts": 6,
                        "max_checker_wall_seconds": 3,
                        "max_checker_cpu_seconds": 2,
                        "max_total_wall_seconds": 18,
                        "max_total_cpu_seconds": 12,
                        "max_address_space_bytes": 512 * 1024 * 1024,
                        "max_process_group_rss_bytes": 512 * 1024 * 1024,
                        "max_output_bytes": 4096,
                    }
        if stage_id is not None:
            protocol["queries"][0]["stage_id"] = stage_id
            protocol["queries"][0]["retry_attempt"] = retry_attempt
            if retry_of_run_id is not None:
                protocol["queries"][0]["retry_of_run_id"] = retry_of_run_id
        from scientific_pipeline_preflight import canonical, digest, protocol_binding, run_preflight
        runtime = Path(sys.executable).resolve()
        runtime_hash = hashlib.sha256(runtime.read_bytes()).hexdigest()
        adapter_identity = [{"path": str(self.pipeline_script.resolve()), "sha256": hashlib.sha256(self.pipeline_script.read_bytes()).hexdigest()}]
        command_identity = {"version": "unit-synthetic-pipeline-v1", "executable": str(runtime), "sha256": runtime_hash,
                            "identity_files": adapter_identity, "runtime_identity": sys.version}
        protocol["pipeline_identity"] = {stage: command_identity for stage in ("runner", "verifier", "scorer", "aggregator")}
        spec_controls = []
        for control, value in zip(protocol["controls"], (3, -5)):
            cid = control["id"]
            spec_controls.append({
                "id": cid,
                "protocol_training_artifact": control["training_artifact"],
                "protocol_training_sha256": control["training_sha256"],
                "protocol_assessment_artifact": control["assessment_artifact"],
                "protocol_assessment_sha256": control["assessment_sha256"],
                "training": {"id": f"synthetic-{cid}-training", "synthetic_rows": [{"input": [0], "expected": value}, {"input": [1], "expected": value}]},
                "assessment": {"id": f"synthetic-{cid}-assessment", "synthetic_rows": [{"input": [2], "expected": value}]},
            })
        commands = {
            stage: {"argv": [str(runtime), str(self.pipeline_script.resolve()), "--stage", "{stage}",
                              "--manifest", "{manifest}", "--workdir", "{workdir}"], **command_identity}
            for stage in ("runner", "verifier", "scorer", "aggregator")
        }
        preflight_spec = {
            "schema": 1, "preflight_id": f"unit-preflight-{self.protocol_count}",
            "protocol_binding_sha256": protocol_binding(protocol), "controls": spec_controls,
            "runs": protocol["run_matrix"], "expected_run_count": len(protocol["run_matrix"]),
            "commands": commands,
            "limits": {"wall_seconds": 5, "cpu_seconds": 5, "address_space_bytes": 512 * 1024 * 1024, "output_bytes": 1024 * 1024},
        }
        spec_path = self.root / f"preflight-spec-{self.protocol_count}.json"
        spec_path.write_bytes(canonical(preflight_spec) + b"\n")
        source.write_text(json.dumps(protocol))
        receipt_path = self.root / f"preflight-receipt-{self.protocol_count}.json"
        preflight_receipt = run_preflight(source, spec_path, receipt_path)
        for path, identifier, role in ((spec_path, "admission-spec", "admission_preflight_spec"), (receipt_path, "admission-receipt", "admission_preflight_receipt")):
            protocol["frozen_artifacts"].append({"id": identifier, "role": role, "path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        protocol["admission_preflight"] = {
            "preflight_id": preflight_spec["preflight_id"], "spec_path": str(spec_path),
            "spec_sha256": hashlib.sha256(spec_path.read_bytes()).hexdigest(),
            "receipt_path": str(receipt_path), "receipt_sha256": hashlib.sha256(receipt_path.read_bytes()).hexdigest(),
            "protocol_binding_sha256": preflight_receipt["protocol_binding_sha256"],
        }
        source.write_text(json.dumps(protocol))
        destination = self.root / f"frozen-{self.protocol_count}"
        frozen = self.command("freeze", "--protocol", source, "--destination", destination)
        if expect_freeze_error is not None:
            self.assertNotEqual(frozen.returncode, 0)
            self.assertIn(expect_freeze_error, frozen.stderr)
            self.assertFalse(destination.exists())
            return frozen
        self.assertEqual(frozen.returncode, 0, frozen.stderr)
        return destination

    def set_query(self, content):
        self.query.write_text(content + "\n")

    def test_process_group_sample_includes_reaped_descendant_cpu(self):
        module_spec = importlib.util.spec_from_file_location("research_execution_group_sample_test", HARNESS)
        module = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)
        proc = self.root / "proc"
        for pid, group, rss, utime, stime, cutime, cstime in (
            (101, 77, 100, 10, 5, 4, 1),
            (102, 77, 200, 20, 5, 0, 0),
            (103, 88, 900, 100, 0, 0, 0),
        ):
            directory = proc / str(pid)
            directory.mkdir(parents=True)
            fields = ["S", "1", str(group)] + ["0"] * 8 + [str(utime), str(stime), str(cutime), str(cstime)]
            (directory / "stat").write_text(f"{pid} (fake process) " + " ".join(fields))
            (directory / "status").write_text(f"Name:\tfake\nVmRSS:\t{rss} kB\n")
        self.assertEqual(module._read_process_group_sample(proc, 77, 10), (300 * 1024, 4.5))
        self.assertIsNone(module._read_process_group_sample(proc, 99, 10))

    @unittest.skipUnless(sys.platform.startswith("linux") and hasattr(os, "wait4") and hasattr(os, "fork"), "Linux wait4 contract")
    def test_wait4_child_cpu_includes_a_descendant_reaped_by_the_leader(self):
        module_spec = importlib.util.spec_from_file_location("research_execution_wait4_test", HARNESS)
        module = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)
        leader = os.fork()
        if leader == 0:
            child = os.fork()
            if child == 0:
                deadline = time.monotonic() + 0.15
                while time.monotonic() < deadline:
                    pass
                os._exit(0)
            os.waitpid(child, 0)
            os._exit(0)
        returncode, usage = module._wait4_until(leader, 2)
        self.assertEqual(returncode, 0)
        self.assertGreater(usage.ru_utime + usage.ru_stime, 0.05)

    def test_unavailable_process_group_metrics_fail_closed(self):
        module_spec = importlib.util.spec_from_file_location("research_execution_unavailable_metrics_test", HARNESS)
        module = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)
        outcome = module.classify_execution(0, b"sat\n", b"", False, process_group_rss_available=False)
        self.assertEqual(outcome["status"], "resource_accounting_unavailable")
        self.assertIn("process_group_rss_unavailable", outcome["classification_overrides"])
        timed_out = module.classify_execution(
            -signal.SIGKILL, b"", b"", True, process_group_rss_available=False,
        )
        self.assertEqual(timed_out["status"], "timed_out")
        self.assertIn("process_group_rss_unavailable", timed_out["classification_overrides"])
        malformed = module.classify_execution(
            2, b"", b"Parse Error: invalid", False, process_group_rss_available=False,
        )
        self.assertEqual(malformed["status"], "malformed_input")
        self.assertEqual(module.terminal_status_for("timed_out", True), "timed_out")
        self.assertEqual(module.terminal_status_for("malformed_input", True), "resource_accounting_unavailable")
        self.assertEqual(
            module.charged_resource_time("malformed_input", 0.1, 0.2, 1.0, 1.0, accounting_unavailable=True),
            (1.0, 1.0),
        )

    def run_attestable_stage(self, *, checker_passes=True):
        if not checker_passes:
            self.checker.write_text(
                "#!/usr/bin/env python3\n"
                "import sys\n"
                "sys.stdin.buffer.read()\n"
                "sys.stdout.buffer.write(b'checker-no\\n')\n"
                "sys.exit(1)\n"
            )
            self.checker.chmod(0o755)
        stages = [{"stage_id": "synth", "expected_status": "successful", "retryable": False}]
        frozen = self.make_protocol(
            calls=2, group_id="attested", group_calls=2, group_stages=stages,
            stage_id="synth", retry_attempt=0, attestation_required=True,
        )
        result = self.command("run", "--protocol-dir", frozen, "--query", self.query, "--query-id", "q0")
        self.assertEqual(result.returncode, 0, result.stderr)
        protocol = json.loads((frozen / "protocol.json").read_text())
        group_dir = Path(protocol["execution_group"]["ledger_dir"])
        state = json.loads((group_dir / "group-budget.json").read_text())
        pending = state["attestation_pending"]
        return frozen, group_dir

    def test_refuses_execution_without_frozen_protocol(self):
        refused = self.command("run", "--protocol-dir", self.root / "missing", "--query", self.query)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("refused", refused.stderr)
        self.assertFalse((self.root / "missing").exists())

    def test_protocol_freeze_is_write_once(self):
        frozen = self.make_protocol()
        source = self.root / "protocol-input.json"
        duplicate = self.command("freeze", "--protocol", source, "--destination", frozen)
        self.assertNotEqual(duplicate.returncode, 0)
        self.assertTrue((frozen / "PROTOCOL.sha256").is_file())

    def test_protocol_requires_end_to_end_preflight_receipt(self):
        frozen = self.make_protocol()
        protocol = json.loads((frozen / "protocol.json").read_text())
        protocol.pop("admission_preflight")
        os.environ["GENETIK_RESEARCH_LEDGER_ROOT"] = str(self.program_root)
        try:
            module_spec = importlib.util.spec_from_file_location("research_execution_preflight_required", HARNESS)
            module = importlib.util.module_from_spec(module_spec)
            module_spec.loader.exec_module(module)
            with self.assertRaises(module.GateError):
                module.validate_protocol(protocol)
        finally:
            os.environ.pop("GENETIK_RESEARCH_LEDGER_ROOT", None)

    def test_protocol_rejects_swapped_control_artifact_ids(self):
        frozen = self.make_protocol()
        protocol = json.loads((frozen / "protocol.json").read_text())
        protocol["controls"][0]["training_artifact"] = "assessment-fixture"
        os.environ["GENETIK_RESEARCH_LEDGER_ROOT"] = str(self.program_root)
        try:
            module_spec = importlib.util.spec_from_file_location("research_execution_artifact_mapping", HARNESS)
            module = importlib.util.module_from_spec(module_spec)
            module_spec.loader.exec_module(module)
            with self.assertRaisesRegex(module.GateError, "mapping is inconsistent"):
                module.validate_protocol(protocol)
        finally:
            os.environ.pop("GENETIK_RESEARCH_LEDGER_ROOT", None)

    def test_protocol_requires_two_controls_for_preflight(self):
        frozen = self.make_protocol()
        protocol = json.loads((frozen / "protocol.json").read_text())
        protocol["controls"] = protocol["controls"][:1]
        os.environ["GENETIK_RESEARCH_LEDGER_ROOT"] = str(self.program_root)
        try:
            module_spec = importlib.util.spec_from_file_location("research_execution_minimum_controls", HARNESS)
            module = importlib.util.module_from_spec(module_spec)
            module_spec.loader.exec_module(module)
            with self.assertRaisesRegex(module.GateError, "at least two independent controls"):
                module.validate_protocol(protocol)
        finally:
            os.environ.pop("GENETIK_RESEARCH_LEDGER_ROOT", None)

    def test_protocol_requires_runs_for_every_declared_control(self):
        frozen = self.make_protocol()
        protocol = json.loads((frozen / "protocol.json").read_text())
        protocol["run_matrix"] = protocol["run_matrix"][:1]
        os.environ["GENETIK_RESEARCH_LEDGER_ROOT"] = str(self.program_root)
        try:
            module_spec = importlib.util.spec_from_file_location("research_execution_control_coverage", HARNESS)
            module = importlib.util.module_from_spec(module_spec)
            module_spec.loader.exec_module(module)
            with self.assertRaisesRegex(module.GateError, "exercise every declared control"):
                module.validate_protocol(protocol)
        finally:
            os.environ.pop("GENETIK_RESEARCH_LEDGER_ROOT", None)

    def test_query_over_limit_is_rejected_without_consuming_call(self):
        frozen = self.make_protocol(query_bytes=2)
        refused = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(refused.returncode, 0)
        budget = json.loads((frozen / "budget.json").read_text())
        self.assertEqual(budget["invocations_reserved"], 0)

    def test_frozen_query_overwrite_is_rejected(self):
        frozen = self.make_protocol(calls=1)
        self.query.write_text("altered after freeze\n")
        refused = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("hash", refused.stderr)
        self.assertEqual(json.loads((frozen / "budget.json").read_text())["invocations_reserved"], 0)

    def test_frozen_query_use_allowance_is_enforced(self):
        frozen = self.make_protocol(calls=2, query_uses=1)
        self.assertEqual(self.command("run", "--protocol-dir", frozen, "--query", self.query).returncode, 0)
        refused = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("query use allowance", refused.stderr)
        self.assertEqual(json.loads((frozen / "budget.json").read_text())["invocations_reserved"], 1)

    def test_query_specific_arguments_are_frozen_and_recorded(self):
        frozen = self.make_protocol(calls=1, query_args=["--fixed-query-mode"])
        result = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        record = json.loads(result.stdout)
        self.assertEqual(record["command"][-1], "--fixed-query-mode")

    def test_zero_invocation_budget_refuses_before_solver_launch(self):
        frozen = self.make_protocol(calls=0)
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("budget exhausted", run.stderr)
        self.assertEqual(json.loads((frozen / "budget.json").read_text())["invocations_reserved"], 0)
        self.assertEqual(len(list((frozen / "calls").glob("*"))), 0)

    def test_cumulative_budget_and_unique_evidence(self):
        frozen = self.make_protocol(calls=2)
        for expected in ("call-0001-", "call-0002-"):
            result = self.command("run", "--protocol-dir", frozen, "--query", self.query)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
            evidence = sorted((frozen / "calls").iterdir())
            self.assertEqual(len(evidence), 1 if expected.endswith("0001-") else 2)
            self.assertTrue(any(item.name.startswith(expected) for item in evidence))
        refused = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("budget exhausted", refused.stderr)
        budget = json.loads((frozen / "budget.json").read_text())
        self.assertEqual(budget["invocations_reserved"], 2)
        self.assertEqual(len(list((frozen / "calls").iterdir())), 2)

    def test_lifetime_budget_cannot_be_reset_by_another_protocol(self):
        first = self.make_protocol(calls=1)
        frozen_protocol = json.loads((first / "protocol.json").read_text())
        program_dir = Path(frozen_protocol["program_budget"]["ledger_dir"])
        run = self.command("run", "--protocol-dir", first, "--query", self.query)
        self.assertEqual(run.returncode, 0, run.stderr + run.stdout)
        second = self.make_protocol(calls=1, program_dir=program_dir, program_id=frozen_protocol["program_budget"]["program_id"])
        refused = self.command("run", "--protocol-dir", second, "--query", self.query)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("program-wide", refused.stderr)
        state = json.loads((program_dir / "program-budget.json").read_text())
        self.assertEqual(state["historical_invocations"], 11)
        self.assertEqual(state["invocations_reserved"], 12)
        self.assertEqual(state["max_lifetime_invocations"], 12)
        self.assertEqual(len((program_dir / "program-events.jsonl").read_text().splitlines()), 3)

    def test_concurrent_protocols_share_one_atomic_lifetime_reservation(self):
        first = self.make_protocol(calls=1)
        first_protocol = json.loads((first / "protocol.json").read_text())
        program_dir = Path(first_protocol["program_budget"]["ledger_dir"])
        second = self.make_protocol(calls=1, program_dir=program_dir, program_id=first_protocol["program_budget"]["program_id"])
        outputs = []

        def invoke(protocol_dir):
            outputs.append(self.command("run", "--protocol-dir", protocol_dir, "--query", self.query))

        threads = [threading.Thread(target=invoke, args=(item,)) for item in (first, second)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(sorted(result.returncode for result in outputs), [0, 3])
        state = json.loads((program_dir / "program-budget.json").read_text())
        self.assertEqual(state["invocations_reserved"], 12)
        self.assertEqual(state["pending"], None)

    def test_concurrent_runs_keep_child_and_group_metrics_per_call(self):
        first = self.make_protocol(calls=2)
        protocol = json.loads((first / "protocol.json").read_text())
        program_dir = Path(protocol["program_budget"]["ledger_dir"])
        second = self.make_protocol(
            calls=2, program_dir=program_dir, program_id=protocol["program_budget"]["program_id"],
        )
        outputs = []

        def invoke(protocol_dir):
            outputs.append(self.command("run", "--protocol-dir", protocol_dir, "--query", self.query))

        threads = [threading.Thread(target=invoke, args=(item,)) for item in (first, second)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(len(outputs), 2)
        for output in outputs:
            self.assertIn(output.returncode, (0, 2), output.stderr)
            record = json.loads(output.stdout)
            self.assertIsInstance(record["waited_child_cpu_seconds"], float)
            self.assertIsInstance(record["waited_child_peak_rss_bytes"], int)
            self.assertIn(record["process_group_rss_available"], (True, False))
        state = json.loads((program_dir / "program-budget.json").read_text())
        self.assertEqual(state["invocations_reserved"], 13)
        self.assertIsNone(state["pending"])

    def test_execution_group_cap_is_shared_across_protocol_versions(self):
        first = self.make_protocol(calls=2, group_id="syntax-retry", group_calls=1)
        first_protocol = json.loads((first / "protocol.json").read_text())
        program_id = first_protocol["program_budget"]["program_id"]
        program_dir = Path(first_protocol["program_budget"]["ledger_dir"])
        second = self.make_protocol(
            calls=2,
            program_dir=program_dir,
            program_id=program_id,
            group_id="syntax-retry",
            group_calls=1,
        )

        accepted = self.command("run", "--protocol-dir", first, "--query", self.query)
        self.assertEqual(accepted.returncode, 0, accepted.stderr + accepted.stdout)
        refused = self.command("run", "--protocol-dir", second, "--query", self.query)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("execution-group invocation budget exhausted", refused.stderr)

        group_dir = program_dir / "execution-groups" / "syntax-retry"
        state = json.loads((group_dir / "group-budget.json").read_text())
        self.assertEqual(state["invocations_reserved"], 1)
        self.assertIsNone(state["pending"])
        events = [json.loads(line) for line in (group_dir / "group-events.jsonl").read_text().splitlines()]
        self.assertEqual([event["event"] for event in events], ["created", "reserved", "completed"])

    def test_execution_group_cap_is_atomic_across_concurrent_protocols(self):
        first = self.make_protocol(calls=2, group_id="syntax-race", group_calls=1)
        first_protocol = json.loads((first / "protocol.json").read_text())
        program_id = first_protocol["program_budget"]["program_id"]
        program_dir = Path(first_protocol["program_budget"]["ledger_dir"])
        second = self.make_protocol(
            calls=2,
            program_dir=program_dir,
            program_id=program_id,
            group_id="syntax-race",
            group_calls=1,
        )
        outputs = []

        def invoke(protocol_dir):
            outputs.append(self.command("run", "--protocol-dir", protocol_dir, "--query", self.query))

        threads = [threading.Thread(target=invoke, args=(item,)) for item in (first, second)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(sorted(result.returncode for result in outputs), [0, 3])
        group_state = json.loads(
            (program_dir / "execution-groups" / "syntax-race" / "group-budget.json").read_text()
        )
        self.assertEqual(group_state["invocations_reserved"], 1)
        self.assertIsNone(group_state["pending"])

    def test_unresolved_execution_group_reservation_fails_closed(self):
        first = self.make_protocol(calls=2, group_id="syntax-pending", group_calls=2)
        first_protocol = json.loads((first / "protocol.json").read_text())
        second = self.make_protocol(
            calls=2,
            program_dir=Path(first_protocol["program_budget"]["ledger_dir"]),
            program_id=first_protocol["program_budget"]["program_id"],
            group_id="syntax-pending",
            group_calls=2,
        )
        group_dir = Path(first_protocol["execution_group"]["ledger_dir"])
        state_path = group_dir / "group-budget.json"
        events_path = group_dir / "group-events.jsonl"
        state = json.loads(state_path.read_text())
        event = {
            "event": "reserved",
            "run_id": "call-interrupted-before-completion",
            "protocol_sha256": hashlib.sha256((first / "protocol.json").read_bytes()).hexdigest(),
            "protocol_dir": str(first.resolve()),
            "call_dir": str((first / "calls" / "interrupted").resolve()),
            "query_id": "q0",
            "query_sha256": hashlib.sha256(self.query.read_bytes()).hexdigest(),
            "reserved_at_unix_ns": 1,
            "previous_hash": state["ledger_head"],
        }
        event_hash = hashlib.sha256(
            json.dumps(event, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        event["event_sha256"] = event_hash
        with events_path.open("ab") as stream:
            stream.write(json.dumps(event, sort_keys=True, separators=(",", ":")).encode() + b"\n")
        state["invocations_reserved"] += 1
        state["pending"] = event["run_id"]
        state["event_count"] += 1
        state["ledger_head"] = event_hash
        state_path.write_text(json.dumps(state, sort_keys=True, indent=2) + "\n")

        refused = self.command("run", "--protocol-dir", second, "--query", self.query)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("execution-group prior invocation is unresolved", refused.stderr)
        self.assertFalse((second / "calls").exists())

    def test_execution_group_identity_path_and_cap_are_immutable(self):
        first = self.make_protocol(calls=4, group_id="syntax-fixed", group_calls=2)
        protocol = json.loads((first / "protocol.json").read_text())
        program_dir = Path(protocol["program_budget"]["ledger_dir"])

        refused_cap = self.make_protocol(
            calls=4, program_dir=program_dir,
            program_id=json.loads((first / "protocol.json").read_text())["program_budget"]["program_id"],
            group_id="syntax-fixed", group_calls=3,
            expect_freeze_error="execution-group identity/ceiling differs",
        )

        wrong_path = program_dir / "alternate-execution-group"
        changed_path_protocol = json.loads((first / "protocol.json").read_text())
        changed_path_protocol["protocol_id"] = "unit-test-changed-group-path"
        changed_path_protocol["execution_group"]["ledger_dir"] = str(wrong_path)
        changed_path_source = self.root / "changed-path.json"
        changed_path_source.write_text(json.dumps(changed_path_protocol))
        refused_path = self.command("freeze", "--protocol", changed_path_source, "--destination", self.root / "changed-path")
        self.assertNotEqual(refused_path.returncode, 0)
        self.assertIn("canonical under its program ledger", refused_path.stderr)
        self.assertFalse((self.root / "changed-path").exists())

    def test_execution_group_cap_cannot_exceed_program_lifetime_remaining(self):
        frozen = self.make_protocol(calls=1)
        protocol = json.loads((frozen / "protocol.json").read_text())
        program_dir = Path(protocol["program_budget"]["ledger_dir"])
        protocol["protocol_id"] = "unit-test-group-over-program-cap"
        protocol["execution_group"] = {
            "group_id": "too-large",
            "ledger_dir": str(program_dir / "execution-groups" / "too-large"),
            "max_invocations": 2,
        }
        source = self.root / "group-over-program-cap.json"
        source.write_text(json.dumps(protocol))
        destination = self.root / "group-over-program-cap"
        refused = self.command("freeze", "--protocol", source, "--destination", destination)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("exceeds program lifetime remaining allowance", refused.stderr)
        self.assertFalse(destination.exists())
        self.assertFalse((program_dir / "execution-groups" / "too-large").exists())

    def test_staged_group_rejects_out_of_order_and_retryable_replay(self):
        stages = [
            {"stage_id": "syntax", "expected_status": "unsatisfiable", "retryable": True},
            {"stage_id": "replay-assessment", "expected_status": "successful", "retryable": False},
        ]
        frozen = self.make_protocol(
            calls=3,
            group_id="staged-order",
            group_calls=3,
            group_stages=stages,
            stage_id="replay-assessment",
            retry_attempt=0,
            query_content="unsat",
        )
        refused = self.command("run", "--protocol-dir", frozen, "--query", self.root / "query-1.txt")
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("execution stage out of order", refused.stderr)
        self.assertFalse((frozen / "calls").exists())

        replay_retry = self.root / "replay-retry.json"
        protocol = json.loads((frozen / "protocol.json").read_text())
        protocol["protocol_id"] = "unit-test-replay-retry"
        protocol["execution_group"]["stages"][1]["retryable"] = True
        replay_retry.write_text(json.dumps(protocol))
        invalid = self.command("freeze", "--protocol", replay_retry, "--destination", self.root / "bad-replay")
        self.assertNotEqual(invalid.returncode, 0)
        self.assertIn("replay stages cannot be retryable", invalid.stderr)

        unknown_status = json.loads((frozen / "protocol.json").read_text())
        unknown_status["protocol_id"] = "unit-test-unknown-stage-status"
        unknown_status["execution_group"]["stages"][0]["expected_status"] = "unknown"
        unknown_source = self.root / "unknown-stage-status.json"
        unknown_source.write_text(json.dumps(unknown_status))
        invalid_status = self.command("freeze", "--protocol", unknown_source, "--destination", self.root / "bad-stage-status")
        self.assertNotEqual(invalid_status.returncode, 0)
        self.assertIn("expected_status must be successful or unsatisfiable", invalid_status.stderr)

    def test_parser_retry_is_single_and_shared_across_protocol_versions(self):
        stages = [
            {"stage_id": "syntax", "expected_status": "unsatisfiable", "retryable": True},
            {"stage_id": "replay-assessment", "expected_status": "successful", "retryable": False},
        ]
        first = self.make_protocol(
            calls=4,
            group_id="staged-retry-success",
            group_calls=3,
            group_stages=stages,
            stage_id="syntax",
            retry_attempt=0,
            query_content="parse-error",
        )
        first_protocol = json.loads((first / "protocol.json").read_text())
        program_id = first_protocol["program_budget"]["program_id"]
        program_dir = Path(first_protocol["program_budget"]["ledger_dir"])
        initial = self.command("run", "--protocol-dir", first, "--query", self.root / "query-1.txt")
        self.assertEqual(initial.returncode, 2)
        initial_record = json.loads(initial.stdout)
        self.assertTrue(initial_record["parser_diagnostic"])
        group_path = Path(first_protocol["execution_group"]["ledger_dir"])
        state = json.loads((group_path / "group-budget.json").read_text())
        self.assertEqual(state["stage_index"], 0)
        self.assertEqual(state["syntax_retries_used"], 0)
        self.assertEqual(state["retry_pending"], {
            "stage_id": "syntax",
            "run_id": initial_record["run_id"],
            "query_sha256": initial_record["query_sha256"],
        })

        wrong_parent = self.make_protocol(
            calls=4,
            program_dir=program_dir,
            program_id=program_id,
            group_id="staged-retry-success",
            group_calls=3,
            group_stages=stages,
            stage_id="syntax",
            retry_attempt=1,
            retry_of_run_id="call-9999-" + "0" * 32,
            query_content="unsat",
        )
        wrong_lineage = self.command("run", "--protocol-dir", wrong_parent, "--query", self.root / "query-2.txt")
        self.assertNotEqual(wrong_lineage.returncode, 0)
        self.assertIn("must identify the failed invocation", wrong_lineage.stderr)

        retry = self.make_protocol(
            calls=4,
            program_dir=program_dir,
            program_id=program_id,
            group_id="staged-retry-success",
            group_calls=3,
            group_stages=stages,
            stage_id="syntax",
            retry_attempt=1,
            retry_of_run_id=initial_record["run_id"],
            query_content="unsat",
        )
        retried = self.command("run", "--protocol-dir", retry, "--query", self.root / "query-3.txt")
        self.assertEqual(retried.returncode, 2, retried.stderr)
        retry_record = json.loads(retried.stdout)
        self.assertEqual(retry_record["status"], "unsatisfiable")
        self.assertNotEqual(retry_record["query_sha256"], initial_record["query_sha256"])
        self.assertEqual(retry_record["retry_of_run_id"], initial_record["run_id"])
        state = json.loads((group_path / "group-budget.json").read_text())
        self.assertEqual((state["stage_index"], state["retry_pending"], state["syntax_retries_used"]), (1, None, 1))
        group_events = [json.loads(line) for line in (group_path / "group-events.jsonl").read_text().splitlines()]
        retry_events = [
            event for event in group_events
            if event.get("stage_id") == "syntax" and event.get("retry_attempt") == 1
        ]
        self.assertEqual([event["event"] for event in retry_events], ["reserved", "completed"])
        self.assertTrue(all(event["retry_of_run_id"] == initial_record["run_id"] for event in retry_events))
        self.assertTrue(all(event["query_sha256"] == retry_record["query_sha256"] for event in retry_events))

        replay = self.make_protocol(
            calls=4,
            program_dir=program_dir,
            program_id=program_id,
            group_id="staged-retry-success",
            group_calls=3,
            group_stages=stages,
            stage_id="replay-assessment",
            retry_attempt=0,
            query_content="replay-output",
        )
        replayed = self.command("run", "--protocol-dir", replay, "--query", self.root / "query-4.txt")
        self.assertEqual(replayed.returncode, 0, replayed.stderr + replayed.stdout)
        state = json.loads((group_path / "group-budget.json").read_text())
        self.assertEqual(state["stage_index"], 2)
        self.assertEqual(state["terminal_status"], "all_stages_completed")

        extra = self.make_protocol(
            calls=4,
            program_dir=program_dir,
            program_id=program_id,
            group_id="staged-retry-success",
            group_calls=3,
            group_stages=stages,
            stage_id="syntax",
            retry_attempt=1,
            retry_of_run_id=retry_record["run_id"],
            query_content="unsat",
        )
        refused = self.command("run", "--protocol-dir", extra, "--query", self.root / "query-5.txt")
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("halted after all_stages_completed", refused.stderr)

    def test_failed_syntax_retry_and_nonparse_invalid_are_terminal(self):
        stages = [{"stage_id": "syntax", "expected_status": "unsatisfiable", "retryable": True}]
        first = self.make_protocol(
            calls=3,
            group_id="staged-retry-fails",
            group_calls=2,
            group_stages=stages,
            stage_id="syntax",
            retry_attempt=0,
            query_content="parse-error",
        )
        protocol = json.loads((first / "protocol.json").read_text())
        program_id = protocol["program_budget"]["program_id"]
        program_dir = Path(protocol["program_budget"]["ledger_dir"])
        initial = self.command("run", "--protocol-dir", first, "--query", self.root / "query-1.txt")
        self.assertEqual(initial.returncode, 2)
        initial_run_id = json.loads(initial.stdout)["run_id"]

        identical_retry = self.make_protocol(
            calls=3,
            program_dir=program_dir,
            program_id=program_id,
            group_id="staged-retry-fails",
            group_calls=2,
            group_stages=stages,
            stage_id="syntax",
            retry_attempt=1,
            retry_of_run_id=initial_run_id,
            query_content="parse-error",
        )
        same_bytes = self.command("run", "--protocol-dir", identical_retry, "--query", self.root / "query-2.txt")
        self.assertNotEqual(same_bytes.returncode, 0)
        self.assertIn("must differ from the failed query bytes", same_bytes.stderr)

        retry = self.make_protocol(
            calls=3,
            program_dir=program_dir,
            program_id=program_id,
            group_id="staged-retry-fails",
            group_calls=2,
            group_stages=stages,
            stage_id="syntax",
            retry_attempt=1,
            retry_of_run_id=initial_run_id,
            query_content="parse-error-alt",
        )
        failed_retry = self.command("run", "--protocol-dir", retry, "--query", self.root / "query-3.txt")
        self.assertEqual(failed_retry.returncode, 2)
        group_path = Path(protocol["execution_group"]["ledger_dir"])
        state = json.loads((group_path / "group-budget.json").read_text())
        self.assertEqual(state["terminal_status"], "unexpected_malformed_input")

        repeat = self.make_protocol(
            calls=3,
            program_dir=program_dir,
            program_id=program_id,
            group_id="staged-retry-fails",
            group_calls=2,
            group_stages=stages,
            stage_id="syntax",
            retry_attempt=1,
            retry_of_run_id=json.loads(failed_retry.stdout)["run_id"],
            query_content="unsat",
        )
        refused = self.command("run", "--protocol-dir", repeat, "--query", self.root / "query-4.txt")
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("halted after unexpected_malformed_input", refused.stderr)

        nonparse = self.make_protocol(
            calls=3,
            group_id="staged-nonparse",
            group_calls=2,
            group_stages=stages,
            stage_id="syntax",
            retry_attempt=0,
            query_content="bad",
        )
        invalid = self.command("run", "--protocol-dir", nonparse, "--query", self.root / "query-5.txt")
        self.assertEqual(invalid.returncode, 2)
        self.assertFalse(json.loads(invalid.stdout)["parser_diagnostic"])
        nonparse_protocol = json.loads((nonparse / "protocol.json").read_text())
        nonparse_group = Path(nonparse_protocol["execution_group"]["ledger_dir"])
        self.assertEqual(json.loads((nonparse_group / "group-budget.json").read_text())["terminal_status"], "unexpected_malformed_input")

    def test_expected_stage_status_mismatch_is_terminal(self):
        stages = [{"stage_id": "syntax", "expected_status": "unsatisfiable", "retryable": True}]
        frozen = self.make_protocol(
            calls=2,
            group_id="staged-mismatch",
            group_calls=2,
            group_stages=stages,
            stage_id="syntax",
            retry_attempt=0,
            query_content="sat",
        )
        result = self.command("run", "--protocol-dir", frozen, "--query", self.root / "query-1.txt")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(json.loads(result.stdout)["status"], "successful")
        protocol = json.loads((frozen / "protocol.json").read_text())
        group_dir = Path(protocol["execution_group"]["ledger_dir"])
        self.assertEqual(json.loads((group_dir / "group-budget.json").read_text())["terminal_status"], "unexpected_successful")

    def test_program_wide_cpu_budget_cannot_reset_between_protocols(self):
        first = self.make_protocol(calls=2, cpu=1, new_cpu=1)
        first_protocol = json.loads((first / "protocol.json").read_text())
        program_dir = Path(first_protocol["program_budget"]["ledger_dir"])
        second = self.make_protocol(
            calls=2,
            cpu=1,
            new_cpu=1,
            program_dir=program_dir,
            program_id=first_protocol["program_budget"]["program_id"],
        )
        accepted = self.command("run", "--protocol-dir", first, "--query", self.query)
        self.assertEqual(accepted.returncode, 0, accepted.stderr + accepted.stdout)
        refused = self.command("run", "--protocol-dir", second, "--query", self.query)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("program-wide prospective solver CPU", refused.stderr)

    def test_same_program_id_cannot_select_a_new_lifetime_ledger(self):
        self.make_protocol(calls=1)
        source = self.root / "protocol-input.json"
        protocol = json.loads(source.read_text())
        protocol["program_budget"]["ledger_dir"] = str(self.root / "alternate-ledger")
        source.write_text(json.dumps(protocol))
        refused = self.command("freeze", "--protocol", source, "--destination", self.root / "bad-ledger")
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("canonical lifetime path", refused.stderr)
        self.assertFalse((self.root / "alternate-ledger").exists())

    def test_history_mismatch_cannot_seed_program_ledger(self):
        frozen = self.make_protocol(calls=1)
        self.history.write_text(json.dumps({"query_invocations_counted": 10}))
        refused = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("hash mismatch", refused.stderr)
        self.assertEqual(len(list((frozen / "calls").glob("*"))), 0)

    def test_statuses_are_distinguished(self):
        for mode, expected in (("unsat", "unsatisfiable"), ("unknown", "unknown"), ("bad", "malformed_input"), ("large", "resource_exhausted"), ("conflict", "invalid_output"), ("mixed-unsat-unknown", "invalid_output"), ("mixed-unsat-fail", "invalid_output"), ("parse-error", "malformed_input"), ("smt-error", "malformed_input"), ("malformed-timeout", "malformed_input"), ("unknown-incomplete", "unknown"), ("fail", "unknown"), ("math-invalid", "mathematical_invalid"), ("external-term", "externally_terminated"), ("sigkill", "externally_terminated"), ("oom-sigkill", "resource_exhausted")):
            self.set_query(mode)
            frozen = self.make_protocol(calls=1, wall=1, cpu=1)
            run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
            self.assertEqual(run.returncode, 2, run.stderr)
            result = json.loads(run.stdout)
            self.assertEqual(result["status"], expected)

    def test_retained_cvc5_timeout_diagnostic_is_not_misclassified_from_sigabrt(self):
        # Exact diagnostic retained from C2.2 call-0002 (29-byte stderr).
        retained_stderr = b"cvc5 interrupted by timeout.\n"
        self.assertEqual(len(retained_stderr), 29)
        self.set_query("cvc-timeout")
        frozen = self.make_protocol(calls=1, wall=1, cpu=1)
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertEqual(run.returncode, 2, run.stderr)
        result = json.loads(run.stdout)
        self.assertEqual(result["returncode"], -signal.SIGABRT)
        self.assertEqual(result["stderr_sha256"], hashlib.sha256(retained_stderr).hexdigest())
        self.assertEqual(result["status"], "timed_out")
        self.assertEqual(result["raw_classification"], "timed_out")
        self.assertEqual(result["timeout_source"], "solver_reported")
        self.assertTrue(result["solver_reported_timeout"])
        self.assertTrue(result["timed_out"])
        self.assertFalse(result["process_timed_out"])
        self.assertEqual(result["terminal_status"], "timed_out")
        self.assertFalse(result["cpu_limit_reached"])
        self.assertEqual(result["charged_cpu_seconds"], 1)
        self.assertEqual(result["charged_wall_seconds"], 1)

    def test_solver_timeout_diagnostic_survives_explicit_cpu_cap_override(self):
        retained_stderr = b"cvc5 interrupted by timeout.\n"
        self.assertEqual(len(retained_stderr), 29)
        module_spec = importlib.util.spec_from_file_location("research_execution_cpu_timeout_test", HARNESS)
        module = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)

        outcome = module.classify_execution(
            -signal.SIGABRT, b"", retained_stderr, False,
            cpu_limit_reached=True,
        )
        self.assertEqual(outcome["raw_classification"], "timed_out")
        self.assertEqual(outcome["status"], "resource_exhausted")
        self.assertEqual(outcome["classification_overrides"], ["cpu_limit"])
        self.assertTrue(outcome["solver_reported_timeout"])
        self.assertTrue(outcome["timed_out"])
        self.assertFalse(outcome["process_timed_out"])
        self.assertEqual(outcome["timeout_source"], "solver_reported")
        charged_cpu, charged_wall = module.charged_resource_time(
            outcome["status"], 21.0, 22.0, 60.0, 65.0
        )
        self.assertEqual(charged_cpu, 60.0)
        self.assertEqual(charged_wall, 65.0)

    def test_timeout_consumes_query_and_time_budget(self):
        self.set_query("timeout")
        frozen = self.make_protocol(calls=2, wall=0.1, cpu=1)
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertEqual(run.returncode, 2, run.stderr)
        self.assertEqual(json.loads(run.stdout)["status"], "timed_out")
        budget = json.loads((frozen / "budget.json").read_text())
        self.assertEqual(budget["invocations_reserved"], 1)
        self.assertGreaterEqual(budget["wall_seconds_charged"], 0.1)
        refused = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("halted", refused.stderr)

    def test_fast_solver_records_distinct_child_and_process_group_metrics(self):
        self.set_query("unsat")
        frozen = self.make_protocol(calls=1, wall=1, cpu=1)
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertIn(run.returncode, (0, 2), run.stderr)
        result = json.loads(run.stdout)
        self.assertIsInstance(result["cpu_seconds"], float)
        self.assertIsInstance(result["waited_child_cpu_seconds"], float)
        self.assertIsInstance(result["waited_child_peak_rss_bytes"], int)
        self.assertGreater(result["waited_child_peak_rss_bytes"], 0)
        self.assertEqual(result["status"] == "resource_accounting_unavailable", not result["process_group_rss_available"])
        if result["status"] == "resource_accounting_unavailable":
            self.assertEqual(result["terminal_status"], "resource_accounting_unavailable")
            self.assertEqual(result["charged_cpu_seconds"], 1)

    def test_tampered_protocol_fails_closed(self):
        frozen = self.make_protocol()
        (frozen / "protocol.json").chmod(0o644)
        with (frozen / "protocol.json").open("ab") as stream:
            stream.write(b" ")
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("checksum", run.stderr)
        budget = json.loads((frozen / "budget.json").read_text())
        self.assertEqual(budget["invocations_reserved"], 0)

    def test_tampered_budget_or_interrupted_reservation_fails_closed(self):
        frozen = self.make_protocol()
        budget_path = frozen / "budget.json"
        budget = json.loads(budget_path.read_text())
        budget["pending"] = "call-0001-interrupted"
        budget_path.chmod(0o600)
        budget_path.write_text(json.dumps(budget))
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("state", run.stderr)
        self.assertEqual(len(list((frozen / "calls").glob("*"))), 0)

    def test_tampered_cumulative_charge_fails_closed(self):
        frozen = self.make_protocol(calls=2)
        self.assertEqual(self.command("run", "--protocol-dir", frozen, "--query", self.query).returncode, 0)
        path = frozen / "budget.json"
        budget = json.loads(path.read_text())
        budget["cpu_seconds_charged"] = 0.0
        path.chmod(0o600)
        path.write_text(json.dumps(budget))
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("CPU charge", run.stderr)
        self.assertEqual(len(list((frozen / "calls").iterdir())), 1)

    def test_concurrent_calls_cannot_exceed_atomic_budget(self):
        frozen = self.make_protocol(calls=1)
        outputs = []

        def invoke():
            outputs.append(self.command("run", "--protocol-dir", frozen, "--query", self.query))

        threads = [threading.Thread(target=invoke) for _ in range(2)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(sorted(result.returncode for result in outputs), [0, 3])
        budget = json.loads((frozen / "budget.json").read_text())
        self.assertEqual(budget["invocations_reserved"], 1)
        self.assertEqual(len(list((frozen / "calls").iterdir())), 1)

    def test_combined_output_limit_is_enforced(self):
        self.set_query("both-large")
        frozen = self.make_protocol(calls=1, output_bytes=4096)
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertEqual(run.returncode, 2)
        self.assertEqual(json.loads(run.stdout)["status"], "resource_exhausted")
        call = next((frozen / "calls").iterdir())
        total = (call / "stdout.bin").stat().st_size + (call / "stderr.bin").stat().st_size
        self.assertLessEqual(total, 4096)

    def test_solver_identity_is_hash_pinned(self):
        frozen = self.make_protocol()
        with self.solver.open("a") as stream:
            stream.write("# altered after freeze\n")
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("hash", run.stderr)

    def test_frozen_input_artifact_is_rechecked(self):
        frozen = self.make_protocol()
        with self.artifact.open("a") as stream:
            stream.write("changed after freeze")
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("artifact", run.stderr)
        self.assertEqual(json.loads((frozen / "budget.json").read_text())["invocations_reserved"], 0)

    def test_cpu_limit_is_resource_exhaustion(self):
        self.set_query("cpu")
        frozen = self.make_protocol(calls=1, wall=2, cpu=0.1)
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertEqual(run.returncode, 2, run.stderr)
        self.assertEqual(json.loads(run.stdout)["status"], "resource_exhausted")

    def test_long_running_process_group_cpu_is_measured_separately(self):
        self.set_query("cpu")
        frozen = self.make_protocol(calls=1, wall=2, cpu=1)
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertEqual(run.returncode, 2, run.stderr)
        result = json.loads(run.stdout)
        self.assertEqual(result["status"], "resource_exhausted")
        self.assertTrue(result["process_group_rss_available"])
        self.assertTrue(result["process_group_cpu_is_lower_bound"])
        self.assertGreater(result["process_group_cpu_seconds"], 0)

    def test_wall_timeout_kills_process_group(self):
        self.set_query("child")
        frozen = self.make_protocol(calls=1, wall=0.2, cpu=1)
        run = self.command(
            "run", "--protocol-dir", frozen, "--query", self.query,
        )
        self.assertEqual(run.returncode, 2, run.stderr)
        result = json.loads(run.stdout)
        self.assertEqual(result["status"], "timed_out")
        self.assertTrue(result["process_group_rss_available"])
        self.assertIsInstance(result["waited_child_peak_rss_bytes"], int)
        child_pid = int((self.root / "child.pid").read_text())
        try:
            stat = Path(f"/proc/{child_pid}/stat").read_text()
            self.assertEqual(stat[stat.rfind(")") + 2 :].split()[0], "Z")
        except FileNotFoundError:
            pass

    def test_completed_leader_cannot_leave_background_process(self):
        self.set_query("orphan")
        frozen = self.make_protocol(calls=1, wall=1, cpu=1)
        run = self.command("run", "--protocol-dir", frozen, "--query", self.query)
        self.assertEqual(run.returncode, 2, run.stderr)
        result = json.loads(run.stdout)
        self.assertEqual(result["status"], "invalid_output")
        self.assertTrue(result["orphaned_process_group"])
        self.assertTrue(result["process_group_rss_available"])
        child_pid = int((self.root / "child.pid").read_text())
        try:
            state = Path(f"/proc/{child_pid}/stat").read_text()
            self.assertEqual(state[state.rfind(")") + 2 :].split()[0], "Z")
        except FileNotFoundError:
            pass

    def test_attestation_executes_pinned_checker_and_advances_once(self):
        frozen, group_dir = self.run_attestable_stage()
        blocked_protocol = self.make_protocol(
            calls=2, program_id="adapter-continuation-test-1", group_id="attested", group_calls=2,
            group_stages=[{"stage_id": "synth", "expected_status": "successful", "retryable": False}],
            stage_id="synth", retry_attempt=0, attestation_required=True,
        )
        blocked = self.command("run", "--protocol-dir", blocked_protocol, "--query", self.query)
        self.assertNotEqual(blocked.returncode, 0)
        self.assertIn("attestation", blocked.stderr)
        attested = self.command("attest", "--protocol-dir", frozen)
        self.assertEqual(attested.returncode, 0, attested.stderr)
        state = json.loads((group_dir / "group-budget.json").read_text())
        self.assertEqual(state["stage_index"], 1)
        self.assertEqual(state["terminal_status"], "all_stages_completed")
        receipt_files = list((group_dir / "attestations").glob("*.receipt.json"))
        self.assertEqual(len(receipt_files), 1)
        receipt = json.loads(receipt_files[0].read_text())
        self.assertTrue(receipt["passed"])
        repeated = self.command("attest", "--protocol-dir", frozen)
        self.assertNotEqual(repeated.returncode, 0)
        events = [json.loads(line) for line in (group_dir / "group-events.jsonl").read_text().splitlines()]
        self.assertEqual(sum(event.get("event") == "attested" for event in events), 1)

    def test_pinned_checker_can_attest_expected_unsatisfiable_stage(self):
        stages = [{"stage_id": "semantic", "expected_status": "unsatisfiable", "retryable": False}]
        frozen = self.make_protocol(
            calls=1, group_id="attested-unsat", group_calls=1, group_stages=stages,
            stage_id="semantic", retry_attempt=0, query_content="unsat29", attestation_required=True,
        )
        run = self.command("run", "--protocol-dir", frozen, "--query", self.root / "query-1.txt")
        self.assertEqual(run.returncode, 2, run.stderr)
        self.assertEqual(json.loads(run.stdout)["status"], "unsatisfiable")
        protocol = json.loads((frozen / "protocol.json").read_text())
        group_dir = Path(protocol["execution_group"]["ledger_dir"])
        state = json.loads((group_dir / "group-budget.json").read_text())
        self.assertIsNotNone(state["attestation_pending"])
        attested = self.command("attest", "--protocol-dir", frozen)
        self.assertEqual(attested.returncode, 0, attested.stderr + attested.stdout)
        state = json.loads((group_dir / "group-budget.json").read_text())
        self.assertEqual(state["terminal_status"], "all_stages_completed")
        self.assertEqual(state["checker_attempts"], 1)

    def test_attestation_rejects_modified_checker_and_caller_receipts(self):
        frozen, group_dir = self.run_attestable_stage()
        checker_bytes = self.checker.read_bytes()
        self.checker.write_bytes(checker_bytes + b"\n# changed after freeze\n")
        wrong_checker = self.command("attest", "--protocol-dir", frozen)
        self.assertNotEqual(wrong_checker.returncode, 0)
        self.checker.write_bytes(checker_bytes)
        fabricated = self.command("attest", "--protocol-dir", frozen, "--receipt", self.root / "fake.json")
        self.assertNotEqual(fabricated.returncode, 0)
        self.assertIn("unrecognized arguments", fabricated.stderr)
        state = json.loads((group_dir / "group-budget.json").read_text())
        self.assertIsNotNone(state["attestation_pending"])
        self.assertIsNone(state["attestation_attempt"])
        self.assertFalse((group_dir / "attestations").exists())

    def test_failed_checker_attestation_is_terminal(self):
        frozen, group_dir = self.run_attestable_stage(checker_passes=False)
        attested = self.command("attest", "--protocol-dir", frozen)
        self.assertEqual(attested.returncode, 2, attested.stderr)
        state = json.loads((group_dir / "group-budget.json").read_text())
        self.assertEqual(state["terminal_status"], "attestation_failed")
        self.assertEqual(state["stage_index"], 0)
        receipt_files = list((group_dir / "attestations").glob("*.receipt.json"))
        self.assertFalse(json.loads(receipt_files[0].read_text())["passed"])
        events = [json.loads(line) for line in (group_dir / "group-events.jsonl").read_text().splitlines()]
        completed = next(event for event in events if event.get("event") == "completed")
        failure = next(event for event in events if event.get("event") == "attested")
        self.assertEqual(failure["stdout_sha256"], completed["stdout_sha256"])
        self.assertEqual(failure["stderr_sha256"], completed["stderr_sha256"])
        self.assertNotEqual(failure["checker_stdout_sha256"], failure["stdout_sha256"])
        for key in ("checker_stdout_path", "checker_stderr_path"):
            artifact = group_dir / failure[key]
            self.assertTrue(artifact.is_file())
            self.assertEqual(hashlib.sha256(artifact.read_bytes()).hexdigest(), failure[key.replace("path", "sha256")])

    def test_checker_combined_output_cap_is_enforced(self):
        self.checker.write_text(
            "#!/usr/bin/env python3\nimport sys\nsys.stdin.buffer.read()\n"
            "sys.stdout.write('x' * 3000)\nsys.stderr.write('y' * 3000)\n"
        )
        self.checker.chmod(0o755)
        frozen = self.make_protocol(
            group_id="output-attested", group_calls=2,
            group_stages=[{"stage_id": "synth", "expected_status": "successful", "retryable": False}],
            stage_id="synth", retry_attempt=0, attestation_required=True,
        )
        self.assertEqual(self.command("run", "--protocol-dir", frozen, "--query", self.query).returncode, 0)
        result = self.command("attest", "--protocol-dir", frozen)
        self.assertEqual(result.returncode, 2)
        status = json.loads(result.stdout)
        self.assertEqual(status["status"], "resource_exhausted")

    def test_postlaunch_checker_harness_exception_is_measured_and_reaped(self):
        module_spec = importlib.util.spec_from_file_location("research_execution_under_test", HARNESS)
        module = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)
        script = self.root / "long_checker.py"
        script.write_text("#!/usr/bin/env python3\nimport time\nprint('partial', flush=True)\ntime.sleep(20)\n")
        script.chmod(0o755)
        limits = {
            "max_address_space_bytes": 512 * 1024 * 1024,
            "max_process_group_rss_bytes": 512 * 1024 * 1024,
            "max_checker_cpu_seconds": 2,
            "max_checker_wall_seconds": 2,
            "max_output_bytes": 4096,
        }
        real_popen = subprocess.Popen
        child_group = []

        class FailFirstWait:
            def __init__(self, *args, **kwargs):
                self.process = real_popen(*args, **kwargs)
                self.inject = True
                child_group.append(self.process.pid)

            def __getattr__(self, name):
                return getattr(self.process, name)

            def wait(self, *args, **kwargs):
                if self.inject:
                    self.inject = False
                    raise RuntimeError("injected post-launch accounting error")
                return self.process.wait(*args, **kwargs)

        subprocess.Popen = FailFirstWait
        try:
            result = module._run_checker([str(script)], b"{}", {"PATH": "/usr/bin:/bin"}, limits)
        finally:
            subprocess.Popen = real_popen
        self.assertEqual(result["status"], "harness_error")
        self.assertIn(b"injected post-launch accounting error", result["stderr"])
        self.assertGreaterEqual(result["charged_wall_seconds"], limits["max_checker_wall_seconds"])
        self.assertGreaterEqual(result["charged_cpu_seconds"], limits["max_checker_cpu_seconds"])
        with self.assertRaises(ProcessLookupError):
            os.killpg(child_group[0], 0)

    def test_checker_thread_start_failure_cleans_up_only_started_threads(self):
        module_spec = importlib.util.spec_from_file_location("research_execution_thread_failure_test", HARNESS)
        module = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)
        script = self.root / "thread_failure_checker.py"
        script.write_text("#!/usr/bin/env python3\nimport time\nprint('partial', flush=True)\ntime.sleep(20)\n")
        script.chmod(0o755)
        limits = {
            "max_address_space_bytes": 512 * 1024 * 1024,
            "max_process_group_rss_bytes": 512 * 1024 * 1024,
            "max_checker_cpu_seconds": 2,
            "max_checker_wall_seconds": 2,
            "max_output_bytes": 4096,
        }
        original_start = threading.Thread.start
        child_group = []
        real_popen = subprocess.Popen

        class RecordPopen:
            def __init__(self, *args, **kwargs):
                self.process = real_popen(*args, **kwargs)
                child_group.append(self.process.pid)

            def __getattr__(self, name):
                return getattr(self.process, name)

        calls = [0]

        def fail_second_thread(thread):
            calls[0] += 1
            if calls[0] == 2:
                raise RuntimeError("injected reader thread start failure")
            return original_start(thread)

        subprocess.Popen = RecordPopen
        threading.Thread.start = fail_second_thread
        try:
            result = module._run_checker([str(script)], b"{}", {"PATH": "/usr/bin:/bin"}, limits)
        finally:
            subprocess.Popen = real_popen
            threading.Thread.start = original_start
        self.assertEqual(result["status"], "harness_error")
        with self.assertRaises(ProcessLookupError):
            os.killpg(child_group[0], 0)

    def test_checker_timeout_consumes_one_shot_and_is_terminal(self):
        self.checker.write_text(
            "#!/usr/bin/env python3\nimport time\ntime.sleep(5)\nprint('late')\n"
        )
        self.checker.chmod(0o755)
        frozen = self.make_protocol(
            group_id="timeout-attested", group_calls=2,
            group_stages=[{"stage_id": "synth", "expected_status": "successful", "retryable": False}],
            stage_id="synth", retry_attempt=0, attestation_required=True,
            checker_limits={"max_attempts": 1, "max_checker_wall_seconds": 0.2, "max_checker_cpu_seconds": 1,
                            "max_total_wall_seconds": 0.2, "max_total_cpu_seconds": 1,
                            "max_address_space_bytes": 512 * 1024 * 1024, "max_process_group_rss_bytes": 512 * 1024 * 1024,
                            "max_output_bytes": 4096},
        )
        self.assertEqual(self.command("run", "--protocol-dir", frozen, "--query", self.query).returncode, 0)
        timed = self.command("attest", "--protocol-dir", frozen)
        self.assertEqual(timed.returncode, 2)
        protocol = json.loads((frozen / "protocol.json").read_text())
        group_dir = Path(protocol["execution_group"]["ledger_dir"])
        state = json.loads((group_dir / "group-budget.json").read_text())
        self.assertEqual(state["terminal_status"], "attestation_failed")
        self.assertEqual(state["checker_attempts"], 1)
        self.assertGreaterEqual(state["checker_wall_seconds_charged"], 0.2)
        second = self.command("attest", "--protocol-dir", frozen)
        self.assertNotEqual(second.returncode, 0)

    def test_crashed_checker_harness_leaves_one_shot_pending_and_fails_closed(self):
        marker = self.root / "checker.pid"
        self.checker.write_text(
            "#!/usr/bin/env python3\nimport os, pathlib, time\n"
            f"pathlib.Path({str(marker)!r}).write_text(str(os.getpid()))\n"
            "time.sleep(8)\nprint('late')\n"
        )
        self.checker.chmod(0o755)
        frozen = self.make_protocol(
            group_id="crashed-attested", group_calls=2,
            group_stages=[{"stage_id": "synth", "expected_status": "successful", "retryable": False}],
            stage_id="synth", retry_attempt=0, attestation_required=True,
        )
        self.assertEqual(self.command("run", "--protocol-dir", frozen, "--query", self.query).returncode, 0)
        env = os.environ.copy()
        env["GENETIK_RESEARCH_LEDGER_ROOT"] = str(self.program_root)
        harness = subprocess.Popen([sys.executable, str(HARNESS), "attest", "--protocol-dir", str(frozen)], env=env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        deadline = time.monotonic() + 5
        while not marker.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        self.assertTrue(marker.exists(), "checker did not launch")
        checker_pid = int(marker.read_text())
        harness.kill()
        harness.wait(timeout=3)
        if harness.stdout:
            harness.stdout.close()
        if harness.stderr:
            harness.stderr.close()
        try:
            os.killpg(checker_pid, 9)
        except ProcessLookupError:
            pass
        protocol = json.loads((frozen / "protocol.json").read_text())
        group_dir = Path(protocol["execution_group"]["ledger_dir"])
        state = json.loads((group_dir / "group-budget.json").read_text())
        self.assertIsNotNone(state["attestation_attempt"])
        second = self.command("attest", "--protocol-dir", frozen)
        self.assertNotEqual(second.returncode, 0)


if __name__ == "__main__":
    unittest.main()
