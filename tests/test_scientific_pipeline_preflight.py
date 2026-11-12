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
sys.path.insert(0, str(ROOT / "scripts"))


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
    if "--duplicate-score" in sys.argv:
        scores.append(dict(scores[0]))
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

    def test_rejects_one_control_preflight(self):
        self.protocol["controls"] = self.protocol["controls"][:1]
        self.protocol["run_matrix"] = self.protocol["run_matrix"][:1]
        self.spec["controls"] = self.spec["controls"][:1]
        self.spec["runs"] = self.spec["runs"][:1]
        self.spec["expected_run_count"] = 1
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("at least two controls", result.stderr)

    def test_rejects_control_without_any_run(self):
        self.protocol["run_matrix"] = self.protocol["run_matrix"][:1]
        self.spec["runs"] = self.spec["runs"][:1]
        self.spec["expected_run_count"] = 1
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exercise every declared control", result.stderr)

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

    def test_rejects_duplicate_scorer_run_record(self):
        self.commands["scorer"]["argv"].append("--duplicate-score")
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("scorer output count/identity", result.stderr)

    def test_admission_verifier_rejects_duplicate_scorer_run_record(self):
        self.write_inputs()
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = json.loads(self.receipt_path.read_text())
        evidence = Path(receipt["evidence_directory"])
        scores_path = evidence / "pipeline" / "independent-scores.json"
        for path in sorted(evidence.rglob("*"), reverse=True):
            if path.is_file():
                path.chmod(0o644)
            elif path.is_dir():
                path.chmod(0o755)
        scores = json.loads(scores_path.read_text())
        scores.append(dict(scores[0]))
        scores_path.write_text(json.dumps(scores, sort_keys=True))
        receipt["evidence_hashes"]["pipeline/independent-scores.json"] = sha(scores_path.read_bytes())
        self.receipt_path.chmod(0o644)
        self.receipt_path.write_text(json.dumps(receipt, sort_keys=True))
        for artifact in (self.spec_path, self.receipt_path):
            self.protocol["frozen_artifacts"].append({
                "id": artifact.stem,
                "role": "admission_preflight_spec" if artifact == self.spec_path else "admission_preflight_receipt",
                "path": str(artifact), "sha256": sha(artifact.read_bytes()),
            })
        self.protocol["admission_preflight"] = {
            "preflight_id": receipt["preflight_id"],
            "spec_path": str(self.spec_path), "spec_sha256": sha(self.spec_path.read_bytes()),
            "receipt_path": str(self.receipt_path), "receipt_sha256": sha(self.receipt_path.read_bytes()),
            "protocol_binding_sha256": receipt["protocol_binding_sha256"],
        }
        module = importlib.util.spec_from_file_location("admission_duplicate_score_test", HARNESS)
        harness = importlib.util.module_from_spec(module)
        module.loader.exec_module(harness)
        with self.assertRaisesRegex(harness.PreflightError, "scorer output count/identity"):
            harness.verify_admission(self.protocol)


class FrozenDescriptorPreflightTests(unittest.TestCase):
    """Exercise distinct pinned stage CLIs, never a stage-dispatch adapter."""
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.python = Path(sys.executable).resolve()
        self.scripts = {}
        sources = {
            "runner": r'''import argparse, json, pathlib
p=argparse.ArgumentParser();p.add_argument('--manifest');p.add_argument('--candidate-output');a=p.parse_args()
m=json.loads(pathlib.Path(a.manifest).read_text());out=[]
for r in m['runs']:
 c=next(x for x in m['controls'] if x['id']==r['control_id'])
 rows=json.loads(pathlib.Path(c['training']['path']).read_text())['rows']
 (x0,y0),(x1,y1)=[(q['inputs'][0],q['expected']) for q in rows]
 d=(y1-y0)//(x1-x0)
 out.append({**{k:r[k] for k in ('run_id','control_id','method','seed')},'training_artifact':c['training']['id'],'training_sha256':c['training']['sha256'],'predictions':[y1+d,y1+2*d]})
pathlib.Path(a.candidate_output).write_text(json.dumps(out,sort_keys=True))
''',
            "verifier": r'''import argparse,json,pathlib,hashlib
p=argparse.ArgumentParser();p.add_argument('--manifest');p.add_argument('--candidate-output');p.add_argument('--verified-output');a=p.parse_args()
rows=json.loads(pathlib.Path(a.candidate_output).read_text()); canon=lambda x:json.dumps(x,sort_keys=True,separators=(',',':')).encode()
out=[{**{k:r[k] for k in ('run_id','control_id','method','seed')},'candidate_sha256':hashlib.sha256(canon(r)).hexdigest(),'status':'verified'} for r in rows]
pathlib.Path(a.verified_output).write_text(json.dumps(out,sort_keys=True))
''',
            "scorer": r'''import argparse,json,pathlib
p=argparse.ArgumentParser();p.add_argument('--manifest');p.add_argument('--candidate-output');p.add_argument('--verified-output');p.add_argument('--assessment-bundle');p.add_argument('--scores-output');a=p.parse_args()
m=json.loads(pathlib.Path(a.assessment_bundle).read_text());outs={r['run_id']:r for r in json.loads(pathlib.Path(a.candidate_output).read_text())}; rec={r['run_id']:r for r in json.loads(pathlib.Path(a.verified_output).read_text())}; scores=[]
for run in m['runs']:
 c=next(x for x in m['controls'] if x['id']==run['control_id']); expected=[x['expected'] for x in json.loads(pathlib.Path(c['assessment']['path']).read_text())['rows']]; out=outs[run['run_id']]
 scores.append({**{k:run[k] for k in ('run_id','control_id','method','seed')},'assessment_artifact':c['assessment']['id'],'assessment_sha256':c['assessment']['sha256'],'exact':out['predictions']==expected and rec[run['run_id']]['status']=='verified','checked_observations':len(expected)})
pathlib.Path(a.scores_output).write_text(json.dumps(scores,sort_keys=True))
''',
            "aggregator": r'''import argparse,json,pathlib
p=argparse.ArgumentParser();p.add_argument('--manifest');p.add_argument('--scores-input');p.add_argument('--report-output');a=p.parse_args()
m=json.loads(pathlib.Path(a.manifest).read_text()); scores=json.loads(pathlib.Path(a.scores_input).read_text()); pc={}
for r in scores:
 e=pc.setdefault(r['control_id'],{'run_count':0,'exact_run_count':0});e['run_count']+=1;e['exact_run_count']+=int(r['exact'])
b=[{k:r[k] for k in ('run_id','control_id','method','seed')} for r in m['runs']]
report={'status':'passed' if all(x['exact'] for x in scores) else 'failed','assessment_source':'synthetic','preflight_id':m['preflight_id'],'run_count':len(scores),'scored_run_count':len(scores),'exact_run_count':sum(v['exact_run_count'] for v in pc.values()),'per_control':pc,'run_ids':[r['run_id'] for r in m['runs']],'run_bindings':b}
pathlib.Path(a.report_output).write_text(json.dumps(report,sort_keys=True))
''',
        }
        for name, body in sources.items():
            path = self.root / f"{name}_cli.py"
            # Keep each synthetic stage alive long enough for the supervisor's
            # process-group resource sampler to take a measured sample.
            path.write_text("import time; time.sleep(0.04)\n" + body)
            self.scripts[name] = path
        self.training_meta = {"id": "training-real-a", "sha256": "1" * 64}
        self.assessment_meta = {"id": "assessment-real-a", "sha256": "2" * 64}
        self.controls = [
            {"id": "control-a", "training_artifact": "training-real-a", "training_sha256": "1" * 64,
             "assessment_artifact": "assessment-real-a", "assessment_sha256": "2" * 64},
            {"id": "control-b", "training_artifact": "training-real-b", "training_sha256": "3" * 64,
             "assessment_artifact": "assessment-real-b", "assessment_sha256": "4" * 64},
        ]
        self.runs = [
            {"run_id": "control-a-run", "control_id": "control-a", "method": "fixture", "seed": 0,
             "training_artifact": "training-real-a", "assessment_artifact": "assessment-real-a"},
            {"run_id": "control-b-run", "control_id": "control-b", "method": "fixture", "seed": 0,
             "training_artifact": "training-real-b", "assessment_artifact": "assessment-real-b"},
        ]
        training_a = self.root / "training-real-a.tsv"; training_a.write_text("synthetic placeholder\n")
        training_b = self.root / "training-real-b.tsv"; training_b.write_text("synthetic placeholder\n")
        self.artifacts = [
            {"id": "python", "path": str(self.python), "sha256": sha(self.python.read_bytes()), "role": "pipeline-executable"},
            {"id": "training-real-a", "path": str(training_a), "sha256": sha(training_a.read_bytes()), "role": "training", "control_id": "control-a"},
            {"id": "assessment-real-a", "path": str(self.root / "never-open-assessment.tsv"), "sha256": "2" * 64, "role": "assessment", "control_id": "control-a"},
            {"id": "training-real-b", "path": str(training_b), "sha256": sha(training_b.read_bytes()), "role": "training", "control_id": "control-b"},
            {"id": "assessment-real-b", "path": str(self.root / "never-open-assessment-b.tsv"), "sha256": "4" * 64, "role": "assessment", "control_id": "control-b"},
        ]
        # Separate synthetic artifacts drive a full, unrelated campaign
        # lifecycle after admission qualification. The historical assessment
        # placeholders above remain unopened by preflight.
        campaign_train_a = self.root / "campaign-training-a.json"
        campaign_train_b = self.root / "campaign-training-b.json"
        campaign_assess_a = self.root / "campaign-assessment-a.json"
        campaign_assess_b = self.root / "campaign-assessment-b.json"
        campaign_train_a.write_text(json.dumps({"rows": [{"inputs": [0], "expected": 4}, {"inputs": [1], "expected": 6}]}))
        campaign_train_b.write_text(json.dumps({"rows": [{"inputs": [0], "expected": 10}, {"inputs": [1], "expected": 13}]}))
        campaign_assess_a.write_text(json.dumps({"rows": [{"inputs": [2], "expected": 8}, {"inputs": [3], "expected": 10}]}))
        campaign_assess_b.write_text(json.dumps({"rows": [{"inputs": [2], "expected": 16}, {"inputs": [3], "expected": 19}]}))
        self.campaign_artifacts = {
            "training-a": {"id": "campaign-training-a", "sha256": sha(campaign_train_a.read_bytes()), "path": str(campaign_train_a)},
            "training-b": {"id": "campaign-training-b", "sha256": sha(campaign_train_b.read_bytes()), "path": str(campaign_train_b)},
            "assessment-a": {"id": "campaign-assessment-a", "sha256": sha(campaign_assess_a.read_bytes()), "path": str(campaign_assess_a)},
            "assessment-b": {"id": "campaign-assessment-b", "sha256": sha(campaign_assess_b.read_bytes()), "path": str(campaign_assess_b)},
        }
        campaign_manifest = self.root / "campaign-manifest.json"
        campaign_runs = [{**run,
            "training_artifact": self.campaign_artifacts["training-a"]["id"] if run["control_id"] == "control-a" else self.campaign_artifacts["training-b"]["id"],
            "assessment_artifact": self.campaign_artifacts["assessment-a"]["id"] if run["control_id"] == "control-a" else self.campaign_artifacts["assessment-b"]["id"]}
            for run in self.runs]
        campaign_manifest.write_text(json.dumps({"preflight_id": "synthetic-campaign", "controls": [
            {"id": "control-a", "training": self.campaign_artifacts["training-a"]},
            {"id": "control-b", "training": self.campaign_artifacts["training-b"]}], "runs": campaign_runs}, sort_keys=True))
        assessment_bundle = self.root / "campaign-assessment-bundle.json"
        assessment_bundle.write_text(json.dumps({"controls": [
            {"id": "control-a", "assessment": self.campaign_artifacts["assessment-a"]},
            {"id": "control-b", "assessment": self.campaign_artifacts["assessment-b"]}], "runs": campaign_runs}, sort_keys=True))
        self.campaign_manifest_artifact = {"id": "campaign-manifest", "path": str(campaign_manifest),
            "sha256": sha(campaign_manifest.read_bytes()), "role": "training"}
        self.campaign_assessment_bundle = {"id": "campaign-assessment-bundle", "path": str(assessment_bundle),
            "sha256": sha(assessment_bundle.read_bytes()), "role": "assessment"}
        for item in (*self.campaign_artifacts.values(), self.campaign_manifest_artifact,
                     self.campaign_assessment_bundle):
            self.artifacts.append({**item, "role": "assessment" if "assessment" in item["id"] else "training"})
        stages = {}
        args = {
            "runner": ["--manifest", "{run_manifest}", "--candidate-output", "{candidate_outputs}"],
            "verifier": ["--manifest", "{run_manifest}", "--candidate-output", "{candidate_outputs}", "--verified-output", "{verified_receipts}"],
            "scorer": ["--manifest", "{run_manifest}", "--candidate-output", "{candidate_outputs}", "--verified-output", "{verified_receipts}", "--assessment-bundle", "{assessment_bundle}", "--scores-output", "{scores_output}"],
            "aggregator": ["--manifest", "{run_manifest}", "--scores-input", "{scores_input}", "--report-output", "{report_output}"],
        }
        input_roles = {
            "runner": ["run_manifest"], "verifier": ["run_manifest", "candidate_outputs"],
            "scorer": ["run_manifest", "candidate_outputs", "verified_receipts", "assessment_bundle"],
            "aggregator": ["run_manifest", "scores_input"],
        }
        output_roles = {"runner": ["candidate_outputs"], "verifier": ["verified_receipts"],
                        "scorer": ["scores_output"], "aggregator": ["report_output"]}
        for name in ("runner", "verifier", "scorer", "aggregator"):
            script_id = f"{name}-entrypoint"
            self.artifacts.append({"id": script_id, "path": str(self.scripts[name]), "sha256": sha(self.scripts[name].read_bytes()), "role": "pipeline-dependency"})
            stages[name] = {"executable_artifact": "python", "entrypoint_artifact": script_id,
                "argv": [str(self.python), str(self.scripts[name]), *args[name]],
                "environment": {"PATH": "/usr/bin:/bin", "LANG": "C", "LC_ALL": "C", "PYTHONHASHSEED": "0"},
                "version": sys.version.split()[0], "runtime_identity": f"CPython {sys.version.split()[0]}",
                "dependencies": [script_id], "input_roles": input_roles[name], "output_roles": output_roles[name]}
        self.descriptor = {"schema": 1, "stages": stages}
        from research_pipeline import descriptor_digest, pipeline_identity
        self.descriptor["sha256"] = descriptor_digest(self.descriptor)
        self.protocol = {"schema": 1, "protocol_id": "descriptor-preflight-test", "source_commit": "a" * 40,
            "controls": self.controls, "run_matrix": self.runs, "pipeline_descriptor": self.descriptor,
            "pipeline_identity": pipeline_identity(self.descriptor), "frozen_artifacts": self.artifacts}
        self.spec = {"schema": 1, "preflight_id": "descriptor-test", "protocol_binding_sha256": None,
            "pipeline_descriptor_sha256": self.descriptor["sha256"], "controls": [
                {"id": "control-a", "protocol_training_artifact": "training-real-a", "protocol_training_sha256": "1"*64,
                 "protocol_assessment_artifact": "assessment-real-a", "protocol_assessment_sha256": "2"*64,
                 "training": {"id": "synthetic-train-a", "synthetic_rows": [{"inputs": [0], "expected": 4}, {"inputs": [1], "expected": 6}]},
                 "assessment": {"id": "synthetic-assess-a", "synthetic_rows": [{"inputs": [2], "expected": 8}, {"inputs": [3], "expected": 10}]}},
                {"id": "control-b", "protocol_training_artifact": "training-real-b", "protocol_training_sha256": "3"*64,
                 "protocol_assessment_artifact": "assessment-real-b", "protocol_assessment_sha256": "4"*64,
                 "training": {"id": "synthetic-train-b", "synthetic_rows": [{"inputs": [0], "expected": 10}, {"inputs": [1], "expected": 13}]},
                 "assessment": {"id": "synthetic-assess-b", "synthetic_rows": [{"inputs": [2], "expected": 16}, {"inputs": [3], "expected": 19}]}}
            ], "runs": json.loads(json.dumps(self.runs)), "expected_run_count": 2,
            "limits": {"wall_seconds": 5, "cpu_seconds": 5, "address_space_bytes": 512*1024*1024, "output_bytes": 1024*1024}}
        if self._testMethodName == "test_incorrect_synthetic_assessment_prediction_fails_preflight":
            self.spec["controls"][1]["assessment"]["synthetic_rows"][0]["expected"] += 1
        self.protocol_path = self.root / "protocol.json"
        self.spec_path = self.root / "preflight.json"
        self.receipt_path = self.root / "receipt.json"
        self.process_ledger = self.root / "process-ledgers"
        self.previous_process_ledger_root = os.environ.get("GENETIK_RESEARCH_PROCESS_LEDGER_ROOT")
        os.environ["GENETIK_RESEARCH_PROCESS_LEDGER_ROOT"] = str(self.process_ledger)
        self.execution_dir = self.root / "frozen-program"
        self.protocol["execution_program_dir"] = str(self.execution_dir)
        self.write_inputs()
        self.freeze_execution_program()

    def freeze_execution_program(self):
        runs = [
            {"run_id": "preflight-runner", "phase_id": "preflight", "stage_id": "runner", "stdin_artifact": None,
             "slot_bindings": {"run_manifest": {"kind": "controller_file", "path": "preflight-inputs/runner-manifest.json"}}},
            {"run_id": "preflight-verifier", "phase_id": "preflight", "stage_id": "verifier", "stdin_artifact": None,
             "slot_bindings": {"run_manifest": {"kind": "controller_file", "path": "preflight-inputs/runner-manifest.json"},
                               "candidate_outputs": {"kind": "run_output", "run_id": "preflight-runner", "slot": "candidate_outputs"}}},
            {"run_id": "preflight-scorer", "phase_id": "preflight", "stage_id": "scorer", "stdin_artifact": None,
             "slot_bindings": {"run_manifest": {"kind": "controller_file", "path": "preflight-inputs/scorer-manifest.json"},
                               "candidate_outputs": {"kind": "run_output", "run_id": "preflight-runner", "slot": "candidate_outputs"},
                               "verified_receipts": {"kind": "run_output", "run_id": "preflight-verifier", "slot": "verified_receipts"},
                               "assessment_bundle": {"kind": "controller_file", "path": "preflight-inputs/scorer-manifest.json"}}},
            {"run_id": "preflight-aggregator", "phase_id": "preflight", "stage_id": "aggregator", "stdin_artifact": None,
             "slot_bindings": {"run_manifest": {"kind": "controller_file", "path": "preflight-inputs/aggregate-manifest.json"},
                               "scores_input": {"kind": "run_output", "run_id": "preflight-scorer", "slot": "scores_output"}}},
        ]
        # Future phase entries are frozen now so their full per-invocation
        # allowances are included before any preflight stage launches.
        runs.extend([
            {"run_id": "campaign-runner", "phase_id": "campaign", "stage_id": "runner", "stdin_artifact": None,
             "slot_bindings": {"run_manifest": {"kind": "artifact", "artifact_id": "campaign-manifest"}}},
            {"run_id": "campaign-verifier", "phase_id": "verification", "stage_id": "verifier", "stdin_artifact": None,
             "slot_bindings": {"run_manifest": {"kind": "artifact", "artifact_id": "campaign-manifest"},
                               "candidate_outputs": {"kind": "run_output", "run_id": "campaign-runner", "slot": "candidate_outputs"}}},
            {"run_id": "campaign-scorer", "phase_id": "scoring", "stage_id": "scorer", "stdin_artifact": None,
             "slot_bindings": {"run_manifest": {"kind": "artifact", "artifact_id": "campaign-manifest"},
                               "candidate_outputs": {"kind": "run_output", "run_id": "campaign-runner", "slot": "candidate_outputs"},
                               "verified_receipts": {"kind": "run_output", "run_id": "campaign-verifier", "slot": "verified_receipts"},
                               "assessment_bundle": {"kind": "artifact", "artifact_id": "campaign-assessment-bundle"}}},
            {"run_id": "campaign-aggregator", "phase_id": "aggregation", "stage_id": "aggregator", "stdin_artifact": None,
             "slot_bindings": {"run_manifest": {"kind": "artifact", "artifact_id": "campaign-manifest"},
                               "scores_input": {"kind": "run_output", "run_id": "campaign-scorer", "slot": "scores_output"}}},
            {"run_id": "recovery-aggregator", "phase_id": "recovery", "stage_id": "aggregator", "stdin_artifact": None,
             "slot_bindings": {"run_manifest": {"kind": "artifact", "artifact_id": "campaign-manifest"},
                               "scores_input": {"kind": "run_output", "run_id": "campaign-scorer", "slot": "scores_output"}}},
        ])
        phases = ("preflight", "campaign", "verification", "scoring", "aggregation", "recovery")
        plan = [{"phase_id": phase, "run_ids": [row["run_id"] for row in runs if row["phase_id"] == phase]} for phase in phases]
        total_cpu = 4.0 * len(runs); total_wall = 5.0 * len(runs)
        process_protocol = {"schema": 1, "kind": "bounded_program", "protocol_id": "descriptor-test",
            "source_commit": "a" * 40,
            "program_budget": {"program_id": "descriptor-test", "ledger_dir": str((self.process_ledger / "descriptor-test").resolve()),
                "max_lifetime_invocations": len(runs), "max_lifetime_cpu_seconds": total_cpu, "max_lifetime_wall_seconds": total_wall},
            "pipeline_descriptor": self.descriptor, "artifacts": list(self.artifacts),
            "limits": {"max_invocations": len(runs), "max_total_cpu_seconds": total_cpu, "max_total_wall_seconds": total_wall,
                "max_cpu_seconds": 1.0, "max_wall_seconds": 2.0, "max_address_space_bytes": 512*1024*1024,
                "max_stdin_bytes": 0, "max_output_bytes": 1024*1024, "max_file_bytes": 1024*1024},
            "runs": runs, "phase_plan": plan,
            "preflight_qualification": {"stage_run_ids": {"runner":"preflight-runner","verifier":"preflight-verifier",
                "scorer":"preflight-scorer","aggregator":"preflight-aggregator"},
                "expected_score_run_ids": [row["run_id"] for row in self.runs], "expected_control_count": 2,
                "expected_score_bindings": [{key: row[key] for key in ("run_id", "control_id", "method", "seed")} for row in self.runs],
                "spec_artifact_id": "preflight-spec"}}
        process_protocol["artifacts"].append({"id": "preflight-spec", "path": str(self.spec_path),
            "sha256": sha(self.spec_path.read_bytes()), "role": "preflight-spec"})
        source = self.root / "bounded-program.json"; source.write_text(json.dumps(process_protocol, sort_keys=True))
        env = os.environ.copy(); env["GENETIK_RESEARCH_PROCESS_LEDGER_ROOT"] = str(self.process_ledger)
        result = subprocess.run([sys.executable, str(HARNESS), "freeze-program", "--protocol", str(source),
                                 "--destination", str(self.execution_dir)], env=env, capture_output=True, text=True, timeout=20)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def tearDown(self):
        if self.previous_process_ledger_root is None:
            os.environ.pop("GENETIK_RESEARCH_PROCESS_LEDGER_ROOT", None)
        else:
            os.environ["GENETIK_RESEARCH_PROCESS_LEDGER_ROOT"] = self.previous_process_ledger_root
        self.temp.cleanup()

    def write_inputs(self):
        self.spec["protocol_binding_sha256"] = self.load_module().protocol_binding(self.protocol)
        self.protocol_path.write_text(json.dumps(self.protocol, sort_keys=True))
        self.spec_path.write_text(json.dumps(self.spec, sort_keys=True))

    def load_module(self):
        spec = importlib.util.spec_from_file_location("descriptor_preflight_test_module", PREFLIGHT)
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module); return module

    def invoke(self):
        env = os.environ.copy()
        env["GENETIK_RESEARCH_PROCESS_LEDGER_ROOT"] = str(self.process_ledger)
        return subprocess.run([sys.executable, str(PREFLIGHT), "--protocol", str(self.protocol_path),
            "--spec", str(self.spec_path), "--receipt", str(self.receipt_path)], env=env, capture_output=True, text=True, timeout=30)

    def test_distinct_production_commands_use_descriptor_and_runner_sees_training_only(self):
        self.write_inputs()
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = json.loads(self.receipt_path.read_text())
        self.assertEqual(receipt["pipeline_descriptor_sha256"], self.descriptor["sha256"])
        self.assertTrue(all(stage["status"] == "completed" for stage in receipt["stages"]))
        report = json.loads((Path(receipt["evidence_directory"]) / "pipeline/scientific-report.json").read_text())
        self.assertEqual(report["per_control"], {
            "control-a": {"run_count": 1, "exact_run_count": 1},
            "control-b": {"run_count": 1, "exact_run_count": 1},
        })
        state = json.loads((self.process_ledger / "descriptor-test/process-budget.json").read_text())
        self.assertEqual(state["qualification_status"], "qualified")
        self.assertEqual(state["invocations_reserved"], 4)
        evidence = Path(receipt["evidence_directory"])
        runner_manifest = evidence / "pipeline/runner-manifest.json"
        raw = runner_manifest.read_bytes()
        self.assertNotIn(b"assessment", raw.lower())
        self.assertNotIn(b"never-open-assessment.tsv", raw)
        self.assertNotIn(b"assessment-real-a", raw)
        self.assertEqual(receipt["stages"][0]["argv"][1], str(self.scripts["runner"]))
        module = self.load_module()
        for file, role in ((self.spec_path, "admission_preflight_spec"), (self.receipt_path, "admission_preflight_receipt")):
            self.protocol["frozen_artifacts"].append({"id": role, "role": role, "path": str(file), "sha256": sha(file.read_bytes())})
        self.protocol["admission_preflight"] = {"preflight_id": receipt["preflight_id"],
            "spec_path": str(self.spec_path), "spec_sha256": sha(self.spec_path.read_bytes()),
            "receipt_path": str(self.receipt_path), "receipt_sha256": sha(self.receipt_path.read_bytes()),
            "protocol_binding_sha256": receipt["protocol_binding_sha256"]}
        self.assertEqual(module.verify_admission(self.protocol)["spec_path"], self.spec_path.resolve())

    def test_synthetic_pipeline_runs_complete_campaign_verification_scoring_and_aggregation(self):
        self.write_inputs()
        preflight = self.invoke()
        self.assertEqual(preflight.returncode, 0, preflight.stderr)
        self.assertEqual(json.loads(self.receipt_path.read_text())["pipeline_descriptor_sha256"],
                         self.descriptor["sha256"])
        expected_protocol_sha = json.loads(
            (self.execution_dir / "runs/preflight-runner/receipt.json").read_text())["protocol_sha256"]

        run_receipts = {}
        for run_id in ("campaign-runner", "campaign-verifier", "campaign-scorer",
                       "campaign-aggregator", "recovery-aggregator"):
            result = subprocess.run([sys.executable, str(HARNESS), "run-program", "--protocol-dir",
                str(self.execution_dir), "--run-id", run_id], env=os.environ.copy(),
                capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, f"{run_id}: {result.stderr}\n{result.stdout}")
            run_receipts[run_id] = json.loads(result.stdout)
            self.assertEqual(run_receipts[run_id]["status"], "completed")
            self.assertEqual(run_receipts[run_id]["protocol_sha256"], expected_protocol_sha)
            self.assertEqual(run_receipts[run_id]["executable_sha256"],
                             self.artifacts[0]["sha256"])

        # Only the admitted scoring stage receives the assessment bundle.
        for run_id, receipt in run_receipts.items():
            assessment_hashes = [key for key in receipt["input_slot_hashes"] if key == "assessment_bundle"]
            self.assertEqual(assessment_hashes, ["assessment_bundle"] if run_id == "campaign-scorer" else [])
        scorer = run_receipts["campaign-scorer"]
        score_output = scorer["output_slots"]["scores_output"]["path"]
        scores = json.loads(Path(score_output).read_text())
        self.assertEqual(len(scores), 2)
        self.assertTrue(all(score["exact"] for score in scores))
        report_path = run_receipts["campaign-aggregator"]["output_slots"]["report_output"]["path"]
        report = json.loads(Path(report_path).read_text())
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["exact_run_count"], 2)
        self.assertEqual(set(report["per_control"]), {"control-a", "control-b"})

        verification = subprocess.run([sys.executable, str(HARNESS), "verify-program", "--protocol-dir",
            str(self.execution_dir)], env=os.environ.copy(), capture_output=True, text=True, timeout=30)
        self.assertEqual(verification.returncode, 0, verification.stderr)
        ledger = json.loads((self.process_ledger / "descriptor-test/process-budget.json").read_text())
        self.assertEqual(ledger["invocations_reserved"], 9)
        self.assertEqual(ledger["qualification_status"], "qualified")
        self.assertAlmostEqual(ledger["wall_seconds_charged"],
                               sum(item["charged_wall_seconds"] for item in run_receipts.values()) +
                               json.loads((self.execution_dir / "runs/preflight-runner/receipt.json").read_text())["charged_wall_seconds"] +
                               json.loads((self.execution_dir / "runs/preflight-verifier/receipt.json").read_text())["charged_wall_seconds"] +
                               json.loads((self.execution_dir / "runs/preflight-scorer/receipt.json").read_text())["charged_wall_seconds"] +
                               json.loads((self.execution_dir / "runs/preflight-aggregator/receipt.json").read_text())["charged_wall_seconds"],
                               places=6)

    def test_campaign_stage_is_blocked_before_preflight_qualification(self):
        result = subprocess.run([sys.executable, str(HARNESS), "run-program", "--protocol-dir",
            str(self.execution_dir), "--run-id", "campaign-runner"], env=os.environ.copy(),
            capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("qualification", result.stderr)
        state = json.loads((self.process_ledger / "descriptor-test/process-budget.json").read_text())
        self.assertEqual(state["invocations_reserved"], 0)
        self.assertEqual(state["qualification_status"], "preflight_pending")

    def test_tampered_predecessor_receipt_and_output_are_rejected_before_reservation(self):
        self.write_inputs()
        preflight = self.invoke()
        self.assertEqual(preflight.returncode, 0, preflight.stderr)
        runner = subprocess.run([sys.executable, str(HARNESS), "run-program", "--protocol-dir",
            str(self.execution_dir), "--run-id", "campaign-runner"], env=os.environ.copy(),
            capture_output=True, text=True, timeout=30)
        self.assertEqual(runner.returncode, 0, runner.stderr)

        receipt_path = self.execution_dir / "runs/campaign-runner/receipt.json"
        receipt = json.loads(receipt_path.read_text())
        output = receipt["output_slots"]["candidate_outputs"]
        output_path = Path(output["path"])
        output_path.chmod(0o644)
        output_path.write_text('{"substituted":"data"}')
        output["sha256"] = sha(output_path.read_bytes())
        receipt_path.chmod(0o644)
        receipt_path.write_bytes(canonical(receipt))

        verifier = subprocess.run([sys.executable, str(HARNESS), "run-program", "--protocol-dir",
            str(self.execution_dir), "--run-id", "campaign-verifier"], env=os.environ.copy(),
            capture_output=True, text=True, timeout=10)
        self.assertNotEqual(verifier.returncode, 0)
        self.assertIn("receipt does not match its immutable ledger event", verifier.stderr)
        state = json.loads((self.process_ledger / "descriptor-test/process-budget.json").read_text())
        self.assertEqual(state["invocations_reserved"], 5)
        self.assertIsNone(state["pending"])
        self.assertFalse((self.execution_dir / "runs/campaign-verifier").exists())

    def test_out_of_order_scorer_is_rejected_before_assessment_is_opened(self):
        result = subprocess.run([sys.executable, str(HARNESS), "run-program", "--protocol-dir",
            str(self.execution_dir), "--run-id", "campaign-scorer"], env=os.environ.copy(),
            capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("qualification", result.stderr)
        self.assertNotIn("never-open-assessment.tsv", result.stderr)
        state = json.loads((self.process_ledger / "descriptor-test/process-budget.json").read_text())
        self.assertEqual(state["invocations_reserved"], 0)

    def test_spec_cannot_override_frozen_argv(self):
        self.write_inputs()
        self.spec["commands"] = {"runner": ["/tmp/adapter"]}
        self.spec["protocol_binding_sha256"] = self.load_module().protocol_binding(self.protocol)
        self.spec_path.write_text(json.dumps(self.spec, sort_keys=True)); self.protocol_path.write_text(json.dumps(self.protocol,sort_keys=True))
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue("preflight-spec" in result.stderr or "cannot override" in result.stderr, result.stderr)

    def test_adapter_swap_or_argv_tamper_is_rejected(self):
        adapter = self.root / "adapter.py"; adapter.write_text("print('adapter')\n")
        self.artifacts.append({"id":"adapter", "path":str(adapter), "sha256":sha(adapter.read_bytes()), "role":"pipeline-dependency"})
        stage=self.descriptor["stages"]["runner"]
        stage["entrypoint_artifact"]="adapter"
        stage["dependencies"].append("adapter")
        from research_pipeline import descriptor_digest
        self.descriptor["sha256"]=descriptor_digest(self.descriptor)
        self.protocol["pipeline_identity"]["descriptor_sha256"]=self.descriptor["sha256"]
        self.protocol["pipeline_identity"]["stage_entrypoints"]["runner"]="adapter"
        self.spec["pipeline_descriptor_sha256"]=self.descriptor["sha256"]
        self.write_inputs()
        result=self.invoke()
        self.assertNotEqual(result.returncode,0)
        self.assertTrue("preflight-spec" in result.stderr or "descriptor differs" in result.stderr or "entrypoint" in result.stderr, result.stderr)

    def test_assessment_content_is_not_opened_during_preflight_validation(self):
        self.write_inputs()
        self.assertFalse(Path(self.artifacts[2]["path"]).exists())
        result=self.invoke()
        self.assertEqual(result.returncode,0,result.stderr)

    def test_incorrect_synthetic_assessment_prediction_fails_preflight(self):
        self.write_inputs()
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue("exact" in result.stderr.lower() or "assessment" in result.stderr.lower(), result.stderr)


if __name__ == "__main__":
    unittest.main()
