import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
PREFLIGHT = ROOT / "scripts" / "scientific_pipeline_preflight.py"
HARNESS = ROOT / "scripts" / "research_execution.py"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


PIPELINE = r'''#!/usr/bin/env python3
import hashlib, json, os, pathlib, sys, time
if "GENETIK_SEALED_ASSESSMENT_PATH" in os.environ:
    raise SystemExit("sensitive parent environment leaked into preflight stage")
if "--force-timeout" in sys.argv:
    time.sleep(2)
if "--large-output" in sys.argv:
    sys.stdout.write("x" * (2 * 1024 * 1024))
    sys.stdout.flush()
stage = sys.argv[sys.argv.index("--stage") + 1]
if "--force-failure" in sys.argv:
    raise SystemExit(11)
manifest = json.loads(pathlib.Path(sys.argv[sys.argv.index("--manifest") + 1]).read_text())
workdir = pathlib.Path(sys.argv[sys.argv.index("--workdir") + 1])
preflight_id = manifest["preflight_id"]
runs = manifest["runs"]
bindings = [{k: r[k] for k in ("run_id", "control_id", "method", "seed")} for r in runs]
canonical = lambda value: json.dumps(value, sort_keys=True, separators=(",", ":")).encode()
if stage == "runner":
    outputs = []
    for run in runs:
        control = next(c for c in manifest["controls"] if c["id"] == run["control_id"])
        train = json.loads(pathlib.Path(control["training"]["path"]).read_text())
        value = train["rows"][0]["expected"]
        outputs.append({**{k: run[k] for k in ("run_id", "control_id", "method", "seed")},
                        "training_artifact": control["training"]["id"],
                        "training_sha256": control["training"]["sha256"], "predictions": [value]})
    (workdir / "candidate-outputs.json").write_bytes(json.dumps(outputs, sort_keys=True).encode())
elif stage == "verifier":
    outputs = json.loads((workdir / "candidate-outputs.json").read_text())
    receipts = [{**{k: row[k] for k in ("run_id", "control_id", "method", "seed")},
                 "candidate_sha256": hashlib.sha256(canonical(row)).hexdigest(), "status": "verified"} for row in outputs]
    (workdir / "verified-receipts.json").write_bytes(json.dumps(receipts, sort_keys=True).encode())
elif stage == "scorer":
    outputs = {r["run_id"]: r for r in json.loads(pathlib.Path(manifest["candidate_outputs"]).read_text())}
    scores = []
    for run in runs:
        control = next(c for c in manifest["controls"] if c["id"] == run["control_id"])
        assessment = json.loads(pathlib.Path(control["assessment"]["path"]).read_text())
        expected = [r["expected"] for r in assessment["rows"]]
        output = outputs[run["run_id"]]
        score = {k: run[k] for k in ("run_id", "control_id", "method", "seed")}
        score.update({"assessment_artifact": control["assessment"]["id"],
                      "assessment_sha256": control["assessment"]["sha256"],
                      "exact": output["predictions"] == expected, "checked_observations": len(expected)})
        if "--wrong-assessment" in sys.argv:
            score["assessment_artifact"] = "another-control-assessment"
        scores.append(score)
    (workdir / "independent-scores.json").write_bytes(json.dumps(scores, sort_keys=True).encode())
elif stage == "aggregator":
    scores = json.loads(pathlib.Path(manifest["scores"]).read_text())
    per_control = {}
    for row in scores:
        summary = per_control.setdefault(row["control_id"], {"run_count": 0, "exact_run_count": 0})
        summary["run_count"] += 1
        summary["exact_run_count"] += int(row["exact"] is True)
    report = {"status": "passed" if all(s["exact"] for s in scores) else "failed",
              "assessment_source": "synthetic", "preflight_id": preflight_id,
              "run_count": len(scores), "scored_run_count": len(scores),
              "exact_run_count": sum(v["exact_run_count"] for v in per_control.values()),
              "per_control": per_control,
              "run_ids": [r["run_id"] for r in runs], "run_bindings": bindings}
    if "--wrong-report-count" in sys.argv:
        report["exact_run_count"] += 1
    pathlib.Path(manifest["report"]).write_text(json.dumps(report, sort_keys=True))
else:
    raise SystemExit(9)
(workdir / (stage + ".status.json")).write_text(json.dumps({"status": "passed", "preflight_id": preflight_id, "run_bindings": bindings}, sort_keys=True))
'''


class ScientificPipelinePreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.script = self.root / "synthetic_pipeline.py"
        self.script.write_text(PIPELINE)
        self.script.chmod(0o755)
        self.tool_sha = sha(self.script.read_bytes())
        self.python = Path(sys.executable).resolve()
        self.python_sha = sha(self.python.read_bytes())
        self.controls = []
        self.artifacts = []
        for cid, value in (("offset-a", 13), ("offset-b", -4)):
            tr = self.root / f"{cid}-training.json"
            ass = self.root / f"{cid}-assessment.json"
            tr.write_text(json.dumps({"train": cid}))
            ass.write_text(json.dumps({"assessment": cid}))
            self.artifacts.extend([
                {"id": f"{cid}-training", "role": "training", "control_id": cid, "path": str(tr), "sha256": sha(tr.read_bytes())},
                {"id": f"{cid}-assessment", "role": "assessment", "control_id": cid, "path": str(ass), "sha256": sha(ass.read_bytes())},
            ])
            self.controls.append({
                "id": cid, "training_artifact": f"{cid}-training", "training_sha256": sha(tr.read_bytes()),
                "assessment_artifact": f"{cid}-assessment", "assessment_sha256": sha(ass.read_bytes()),
                "protocol_training_artifact": f"{cid}-training", "protocol_training_sha256": sha(tr.read_bytes()),
                "protocol_assessment_artifact": f"{cid}-assessment", "protocol_assessment_sha256": sha(ass.read_bytes()),
                "training": {"id": f"synthetic-{cid}-training", "synthetic_rows": [{"input": [0], "expected": value}, {"input": [1], "expected": value}]},
                "assessment": {"id": f"synthetic-{cid}-assessment", "synthetic_rows": [{"input": [2], "expected": value}]},
            })
        self.runs = []
        for c in self.controls:
            self.runs.append({"run_id": f"{c['id']}-m1-s0", "control_id": c["id"], "method": "m1", "seed": 0, "training_artifact": c["training_artifact"], "assessment_artifact": c["assessment_artifact"]})
        self.commands = {}
        identity_files = [{"path": str(self.script.resolve()), "sha256": self.tool_sha}]
        for stage in ("runner", "verifier", "scorer", "aggregator"):
            self.commands[stage] = {
                "argv": [str(self.python), str(self.script), "--stage", "{stage}", "--manifest", "{manifest}", "--workdir", "{workdir}"],
                "executable": str(self.python),
                "version": "synthetic-pipeline-v1", "sha256": self.python_sha,
                "identity_files": identity_files, "runtime_identity": sys.version,
            }
        self.pipeline_identity = {
            stage: {"version": "synthetic-pipeline-v1", "executable": str(self.python), "sha256": self.python_sha,
                    "identity_files": identity_files, "runtime_identity": sys.version}
            for stage in self.commands
        }
        self.protocol = {
            "schema": 1, "protocol_id": "test-multi-control", "source_commit": "a" * 40,
            "controls": [{k: c[k] for k in ("id", "training_artifact", "training_sha256", "assessment_artifact", "assessment_sha256")} for c in self.controls],
            "run_matrix": self.runs, "pipeline_identity": self.pipeline_identity,
            "frozen_artifacts": self.artifacts + [
                {"id": "preflight-tool", "role": "pipeline-dependency", "path": str(self.script), "sha256": self.tool_sha},
                {"id": "python-runtime", "role": "pipeline-executable", "path": str(self.python), "sha256": self.python_sha},
                {"id": "admission-tool", "role": "admission-tool", "path": str(PREFLIGHT), "sha256": sha(PREFLIGHT.read_bytes())},
                {"id": "execution-gate", "role": "execution-gate", "path": str(HARNESS), "sha256": sha(HARNESS.read_bytes())},
            ],
        }
        self.protocol_path = self.root / "protocol.json"
        self.spec_path = self.root / "preflight-spec.json"
        self.receipt_path = self.root / "preflight-receipt.json"
        self.spec = {
            "schema": 1, "preflight_id": "multi-control-preflight",
            "protocol_binding_sha256": None, "controls": self.controls,
            "runs": json.loads(json.dumps(self.runs)),
            "expected_run_count": len(self.runs), "commands": self.commands,
            "limits": {"wall_seconds": 5, "cpu_seconds": 5, "address_space_bytes": 512 * 1024 * 1024, "output_bytes": 1024 * 1024},
        }

    def tearDown(self):
        self.temp.cleanup()

    def write_inputs(self):
        module = self.load_preflight()
        self.spec["protocol_binding_sha256"] = module.protocol_binding(self.protocol)
        self.protocol_path.write_text(json.dumps(self.protocol, sort_keys=True))
        self.spec_path.write_text(json.dumps(self.spec, sort_keys=True))

    def load_preflight(self):
        spec = importlib.util.spec_from_file_location("pipeline_preflight_test_module", PREFLIGHT)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def invoke(self):
        return subprocess.run([sys.executable, str(PREFLIGHT), "--protocol", str(self.protocol_path), "--spec", str(self.spec_path), "--receipt", str(self.receipt_path)], capture_output=True, text=True)

    def test_two_control_end_to_end_preflight_passes_and_is_bound(self):
        self.write_inputs()
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = json.loads(self.receipt_path.read_text())
        self.assertEqual(receipt["status"], "passed")
        self.assertEqual([r["control_id"] for r in receipt["run_bindings"]], ["offset-a", "offset-b"])
        self.assertEqual([s["stage"] for s in receipt["stages"]], ["runner", "verifier", "scorer", "aggregator"])
        self.assertTrue(Path(receipt["evidence_directory"], "pipeline", "scientific-report.json").is_file())
        self.assertNotEqual(receipt["control_artifacts"][0]["synthetic_training_sha256"], receipt["control_artifacts"][1]["synthetic_training_sha256"])
        harness = importlib.util.spec_from_file_location("research_execution_preflight_test", HARNESS)
        module = importlib.util.module_from_spec(harness)
        harness.loader.exec_module(module)
        for artifact in (self.spec_path, self.receipt_path):
            self.protocol["frozen_artifacts"].append({"id": artifact.stem, "role": "admission_preflight_spec" if artifact == self.spec_path else "admission_preflight_receipt", "path": str(artifact), "sha256": sha(artifact.read_bytes())})
        self.protocol["admission_preflight"] = {
            "preflight_id": receipt["preflight_id"], "spec_path": str(self.spec_path), "spec_sha256": sha(self.spec_path.read_bytes()),
            "receipt_path": str(self.receipt_path), "receipt_sha256": sha(self.receipt_path.read_bytes()),
            "protocol_binding_sha256": receipt["protocol_binding_sha256"],
        }
        self.assertEqual(module.verify_admission(self.protocol)["spec_path"], self.spec_path.resolve())
        report_path = Path(receipt["evidence_directory"], "pipeline", "scientific-report.json")
        report_path.chmod(0o644)
        report_path.write_text('{"status":"passed","assessment_source":"synthetic"}')
        with self.assertRaises(module.PreflightError):
            module.verify_admission(self.protocol)

    def test_parent_assessment_environment_is_not_inherited(self):
        self.write_inputs()
        name = "GENETIK_SEALED_ASSESSMENT_PATH"
        previous = os.environ.get(name)
        os.environ[name] = "/sealed/assessment/labels.json"
        try:
            result = self.invoke()
        finally:
            if previous is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = previous
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_fractional_cpu_limit_is_rejected_instead_of_rounded_up(self):
        self.spec["limits"]["cpu_seconds"] = 0.5
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("cpu_seconds must be an integer", result.stderr)

    def test_rejects_incorrect_aggregate_counts(self):
        self.commands["aggregator"]["argv"].append("--wrong-report-count")
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_mutated_pipeline_entrypoint(self):
        self.write_inputs()
        with self.script.open("a") as stream:
            stream.write("\n# mutated after protocol freeze\n")
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_stage_wall_limit_is_enforced(self):
        self.commands["runner"]["argv"].append("--force-timeout")
        self.spec["limits"]["wall_seconds"] = 0.2
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_stage_output_limit_is_enforced(self):
        self.commands["runner"]["argv"].append("--large-output")
        self.spec["limits"]["output_bytes"] = 1024 * 1024
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_swapped_training_artifact_association(self):
        self.spec["runs"][0]["training_artifact"] = self.spec["controls"][1]["protocol_training_artifact"]
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_duplicate_control_identity(self):
        self.spec["controls"].append(dict(self.spec["controls"][0]))
        self.spec["expected_run_count"] += 1
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_corrupted_artifact_hash(self):
        self.spec["controls"][0]["training"]["sha256"] = "0" * 64
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_incorrect_run_count(self):
        self.spec["expected_run_count"] += 1
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_missing_run_from_frozen_matrix(self):
        self.spec["runs"].pop()
        self.spec["expected_run_count"] = len(self.spec["runs"])
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_missing_control_artifact_mapping(self):
        self.spec["controls"][0]["protocol_training_artifact"] = "missing-training-id"
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_missing_synthetic_assessment_artifact(self):
        self.spec["controls"][0]["assessment"].pop("synthetic_rows")
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_synthetic_artifact_path_traversal(self):
        self.spec["controls"][0]["training"]["id"] = "../../escaped"
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_scorer_execution_failure(self):
        self.commands["scorer"]["argv"].append("--force-failure")
        self.protocol["pipeline_identity"]["scorer"]["runtime_identity"] = sys.version
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_scorer_that_claims_the_wrong_assessment_control(self):
        self.commands["scorer"]["argv"].append("--wrong-assessment")
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
