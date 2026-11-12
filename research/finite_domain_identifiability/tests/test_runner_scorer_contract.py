"""Offline study-pipeline contracts; no campaign process or assessment data is used."""
from __future__ import annotations

import hashlib
import base64
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[3]
TOOLS=Path(__file__).parents[1]/"tools"
sys.path.insert(0,str(TOOLS))
sys.path.insert(0,str(ROOT/"scripts"))
import run_matrix
import verify_matrix
import aggregate_report
import scientific_pipeline_preflight as preflight


def load_module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


scorer=load_module("fdi_contract_scorer",TOOLS/"score_transcripts.py")
peer=load_module("fdi_contract_peer",TOOLS/"oracle_peer.py")


class RunnerScorerContract(unittest.TestCase):
    def digest(self,path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()

    def session_config(self):
        helper=ROOT/"scripts"/"bounded_interactive_session.py"
        peer_path=TOOLS/"oracle_peer.py"
        return {"python_executable":sys.executable,"python_sha256":self.digest(sys.executable),
            "helper_path":str(helper),"helper_sha256":self.digest(helper),
            "peer_path":str(peer_path),"peer_sha256":self.digest(peer_path),
            "runner_runtime_files":[],"peer_runtime_files":[],"max_frame_bytes":4096,
            "max_terminal_frame_bytes":16384,"max_protocol_bytes":1024*1024,"max_requests":8,"max_cpu_seconds":2.0,
            "max_memory_bytes":512*1024*1024,"terminal_type":"final"}

    def test_pinned_session_descriptor_and_limits(self):
        run_matrix.validate_interactive_session(self.session_config())
        malformed=self.session_config(); malformed["max_protocol_bytes"]=1024*1024+1
        with self.assertRaises(run_matrix.Failure):
            run_matrix.validate_interactive_session(malformed)

    def test_v2_campaign_slot_is_exactly_one_run_and_private_peer_metadata_is_separate(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); runner=root/"selector"; runner.write_bytes(b"fixture binary"); runner.chmod(0o755)
            content=b"genetik-fdi-training-v1\n0\t2\n"
            bundle={"schema":"genetik-fdi-training-bundle-v1","artifacts":[{
                "id":"train-a","sha256":hashlib.sha256(content).hexdigest(),
                "content_base64":base64.b64encode(content).decode()}]}
            run={"run_id":"opaque-1","control_id":"arithmetic_m0","method":"ascending",
                 "seed":7,"training_artifact":"train-a","training_sha256":hashlib.sha256(content).hexdigest(),
                 "peer_private":{"control":"arithmetic","reference_member":0}}
            manifest={"schema":"genetik-fdi-run-manifest-v2","mode":"campaign","protocol_id":"frozen-x",
                "selector_runner":{"path":str(runner),"sha256":self.digest(runner)},
                "interactive_session":self.session_config(),"runs":[run]}
            actual_runner,indexed=run_matrix._validate_stage_inputs(manifest,bundle,False,runner)
            self.assertEqual(actual_runner,runner.resolve())
            command=run_matrix.producer_command(actual_runner,{"run_id":run["run_id"],"control":"arithmetic",
                "policy":"ascending","seed_metadata":7},root/"training.tsv")
            self.assertNotIn("reference_member",repr(command))
            self.assertNotIn("assessment",repr(command))
            peer_command=run_matrix.peer_command(self.session_config(),run)
            self.assertIn("--reference-member",peer_command)
            self.assertEqual(len(indexed),1)
            malformed={**manifest,"runs":[run,dict(run,run_id="opaque-2")]}
            with self.assertRaises(run_matrix.Failure):
                run_matrix._validate_stage_inputs(malformed,bundle,False,runner)
            leaked={**manifest,"assessment_bundle":{"path":"sealed.bin"}}
            with self.assertRaises(run_matrix.Failure):
                run_matrix._validate_stage_inputs(leaked,bundle,False,runner)
            corrupt={"schema":bundle["schema"],"artifacts":[dict(bundle["artifacts"][0],sha256="0"*64)]}
            with self.assertRaises(run_matrix.Failure):
                run_matrix._validate_stage_inputs(manifest,corrupt,False,runner)

    def test_synthetic_manifest_requires_explicit_fixture_flag(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); runner=root/"selector"; runner.write_bytes(b"fixture binary"); runner.chmod(0o755)
            content=b"genetik-fdi-training-v1\n0\t2\n"
            bundle={"schema":"genetik-fdi-training-bundle-v1","artifacts":[{
                "id":"train-a","sha256":hashlib.sha256(content).hexdigest(),
                "content_base64":base64.b64encode(content).decode()}]}
            run={"run_id":"fixture-1","control_id":"arithmetic_m0","method":"ascending","seed":0,
                 "training_artifact":"train-a","training_sha256":hashlib.sha256(content).hexdigest(),
                 "training_path":str(root/"training.tsv")}
            (root/"training.tsv").write_bytes(content)
            run["control_id"]="control-a"
            manifest={"schema":"genetik-fdi-run-manifest-v2","mode":"synthetic-preflight",
                "protocol_id":"synthetic-only","selector_runner":{"path":str(runner),"sha256":self.digest(runner)},
                "interactive_session":self.session_config(),"catalogue_map":[{"control_id":"control-a","catalogue":"ambiguity"}],
                "runs":[run,dict(run,run_id="fixture-2")]}
            with self.assertRaises(run_matrix.Failure):
                run_matrix._validate_stage_inputs(manifest,bundle,False,runner)
            self.assertEqual(len(run_matrix._validate_stage_inputs(manifest,bundle,True,runner)[1]),1)

    def test_preflight_manifest_materializes_training_only_fixture_bundle(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/"train.tsv"
            content=b"genetik-fdi-training-v1\n0\t0\n"
            path.write_bytes(content)
            source={"schema":1,"preflight_id":"synthetic-a",
                "controls":[{"id":"control-a","catalogue":"ambiguity","training":{
                    "id":"training-a","path":str(path),"sha256":hashlib.sha256(content).hexdigest()}}],
                "runs":[{"run_id":"fixture-a","control_id":"control-a","method":"ascending",
                    "seed":0,"training_artifact":"training-a"}]}
            manifest,bundle=run_matrix.normalize_synthetic_manifest(source)
            self.assertEqual(manifest["mode"],"synthetic-preflight")
            self.assertNotIn("assessment",repr(manifest).lower())
            self.assertEqual(manifest["catalogue_map"],[{"control_id":"control-a","catalogue":"ambiguity","reference_member":0}])
            self.assertNotIn("peer_private",manifest["runs"][0])
            self.assertEqual(base64.b64decode(bundle["artifacts"][0]["content_base64"]),content)

    def test_two_control_preflight_manifest_covers_no_query_and_refinement_catalogues(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); runner=root/"selector"; runner.write_bytes(b"pinned test fixture"); runner.chmod(0o755)
            contents={"train-ambiguity":b"genetik-fdi-training-v1\n0\t0\n",
                      "train-arithmetic":b"genetik-fdi-training-v1\n0\t2\n"}
            controls=[]
            for control_id,catalogue,artifact_id in (
                ("ambiguity-control","ambiguity","train-ambiguity"),
                ("arithmetic-control","arithmetic","train-arithmetic")):
                path=root/f"{artifact_id}.tsv"; path.write_bytes(contents[artifact_id])
                controls.append({"id":control_id,"catalogue":catalogue,"training":{
                    "id":artifact_id,"path":str(path),"sha256":self.digest(path)}})
            source={"schema":1,"preflight_id":"synthetic-two-control","controls":controls,
                "interactive_session":self.session_config(),
                "selector_runner":{"path":str(runner),"sha256":self.digest(runner)},
                "runs":[
                    {"run_id":"fixture-ambiguity","control_id":"ambiguity-control","method":"disagreement",
                     "seed":0,"training_artifact":"train-ambiguity"},
                    {"run_id":"fixture-arithmetic","control_id":"arithmetic-control","method":"disagreement",
                     "seed":0,"training_artifact":"train-arithmetic"}]}
            manifest,bundle=run_matrix.normalize_synthetic_manifest(source)
            self.assertEqual([entry["catalogue"] for entry in manifest["catalogue_map"]],
                             ["ambiguity","arithmetic"])
            self.assertTrue(all(entry["reference_member"]==0 for entry in manifest["catalogue_map"]))
            checked,indexed=run_matrix._validate_stage_inputs(manifest,bundle,True,runner)
            self.assertEqual(checked,runner.resolve())
            self.assertEqual(set(indexed),{"train-ambiguity","train-arithmetic"})
        malformed=self.session_config(); malformed["peer_sha256"]="0"*64
        with self.assertRaises(run_matrix.Failure):
            run_matrix.validate_interactive_session(malformed)

    def test_producer_cannot_receive_reference_or_assessment_identity(self):
        run={"run_id":"run_001","control":"modular","reference_member":2,
             "policy":"ascending","seed_metadata":17,
             "assessment_path":"sealed-labels.tsv","assessment_sha256":"secret"}
        argv=run_matrix.producer_command("runner",run,"training.tsv")
        self.assertNotIn("--reference-member",argv)
        self.assertNotIn("2",argv)
        self.assertFalse(any("assessment" in item or "secret" in item for item in argv))
        peer_argv=run_matrix.peer_command(self.session_config(),run)
        self.assertIn("--reference-member",peer_argv)
        self.assertFalse(any("assessment" in item or "secret" in item for item in peer_argv))

    def test_runner_validation_never_opens_assessment_content(self):
        with tempfile.TemporaryDirectory(prefix="fdi-safe-assessment-") as temp:
            training=Path(temp)/"train.tsv"
            training.write_text("genetik-fdi-training-v1\n0\t2\n")
            training_hash=self.digest(training)
            assessment=Path(temp)/"assessment-sealed"/"label.tsv"
            runs=[]; data_runs=[]; matrix=[]
            for ordinal in range(1,39):
                run_id=f"run_{ordinal:03d}"
                row={"run_id":run_id,"control":"arithmetic","reference_member":0,
                    "policy":"ascending","seed_metadata":0,
                    "replay_of":("run_001" if ordinal==37 else "run_002" if ordinal==38 else None),
                    "training_sha256":training_hash,"assessment_sha256":"a"*64}
                runs.append(row)
                data_runs.append({**row,"training_path":str(training),"assessment_path":str(assessment)})
                matrix.append({"run_id":run_id,"control_id":"arithmetic_m0","method":"ascending","seed":0})
            source_paths=[run_matrix.Path(path).resolve() for path in (
                run_matrix.__file__,TOOLS/"freeze_data.py",TOOLS/"score_transcripts.py",
                TOOLS/"verify_matrix.py",ROOT/"research/finite_domain_identifiability/runner.cpp",
                ROOT/"research/finite_domain_identifiability/FiniteDomainIdentifiability.hpp",
                ROOT/"scripts/bounded_interactive_session.py",TOOLS/"oracle_peer.py")]
            identities=[{"path":str(path),"sha256":self.digest(path)} for path in source_paths]
            limits={**run_matrix.PER_RUN,**run_matrix.TOTAL,"charged_work_per_run":100000,
                "cumulative_charged_work":3800000,"selector_work_limit":99936,"oracle_work_reserve":64,
                "candidate_row_evaluations_per_run":1024,"logical_storage_bytes_per_run":1048576}
            config={"protocol_id":"finite-domain-identifiability-v1","limits":limits,"runs":runs,
                "run_matrix":matrix,"runner_sha256":self.digest(ROOT/"research/finite_domain_identifiability/runner.cpp"),
                "implementation_files":identities,"interactive_session":self.session_config()}
            manifest={"runs":data_runs}
            original_open=Path.open
            def guarded_open(path,*args,**kwargs):
                if "assessment-sealed" in str(path):
                    raise AssertionError("sealed assessment content opened")
                return original_open(path,*args,**kwargs)
            with mock.patch.object(run_matrix,"verify_admission",return_value=None), \
                 mock.patch.object(Path,"open",autospec=True,side_effect=guarded_open):
                self.assertEqual(set(run_matrix.validate(config,ROOT/"research/finite_domain_identifiability/runner.cpp",manifest)),
                                 {row["run_id"] for row in data_runs})

    def test_oracle_peer_accepts_only_bounded_correlated_queries(self):
        query_id,value=peer.parse_request('{"type":"query","id":"q2","payload":{"input":-1}}')
        self.assertEqual((query_id,value),("q2",-1))
        self.assertEqual(peer.oracle("modular",0,value),(2,1))
        with self.assertRaises(peer.PeerError):
            peer.parse_request('{"type":"query","id":"q2","payload":{"input":-9}}')

    def test_session_event_hashes_bind_query_response_and_terminal_bytes(self):
        producer=(b'{"type":"query","id":"q1","payload":{"input":1}}\n'
                  b'{"type":"final","id":"final","transcript":{}}\n')
        response=b'{"type":"response","id":"q1","value":1}\n'
        def event(previous,fields):
            body={"previous_sha256":previous,**fields}
            digest=hashlib.sha256(json.dumps(body,sort_keys=True,separators=(",",":")).encode()).hexdigest()
            return {**body,"sha256":digest},digest
        first,head=event("0"*64,{"kind":"exchange","ordinal":1,
            "request_bytes":len(producer.splitlines(keepends=True)[0]),
            "request_sha256":hashlib.sha256(producer.splitlines(keepends=True)[0]).hexdigest(),
            "response_bytes":len(response),"response_sha256":hashlib.sha256(response).hexdigest()})
        terminal_line=producer.splitlines(keepends=True)[-1]
        last,_=event(head,{"kind":"terminal","request_bytes":len(terminal_line),
            "request_sha256":hashlib.sha256(terminal_line).hexdigest()})
        receipt={"session_events":[first,last],"wait4_usage_complete":True,
                 "stream_capture_complete":True,"observed_output_bytes":len(producer)+len(response)}
        self.assertTrue(verify_matrix.verify_session_events(receipt,producer,response))
        self.assertFalse(verify_matrix.verify_session_events(receipt,producer,response+b"x"))

    def test_raw_process_streams_are_persisted_before_malformed_frame_classification(self):
        with tempfile.TemporaryDirectory() as temp:
            raw=b'{"type":"query"\nnot-json\n'
            evidence={"producer_stdout":{"base64":base64.b64encode(raw).decode()},
                "peer_stdout":{"base64":base64.b64encode(b"partial").decode()},
                "producer_stderr":{"base64":base64.b64encode(b"diagnostic").decode()},
                "peer_stderr":{"base64":""}}
            streams,frames,responses,error=run_matrix.persist_then_parse_streams(evidence,Path(temp))
            self.assertEqual((Path(temp)/"wire.stdout").read_bytes(),raw)
            self.assertEqual((Path(temp)/"wire.responses").read_bytes(),b"partial")
            self.assertEqual((Path(temp)/"stderr.txt").read_bytes(),b"diagnostic")
            self.assertTrue(error)
            self.assertEqual(frames,[])
            self.assertEqual(responses,[])

    def test_aggregate_report_recomputes_ordered_per_control_counts_and_score_hash(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            manifest={"schema":"genetik-fdi-run-manifest-v2","mode":"synthetic-preflight",
                "protocol_id":"preflight-a","runs":[
                    {"run_id":"r1","control_id":"ambiguity","method":"a","seed":0},
                    {"run_id":"r2","control_id":"arithmetic","method":"b","seed":1}]}
            scores={"schema":"genetik-fdi-scores-v2","protocol_id":"preflight-a",
                "summary":{"all_scored":True,"acceptance_gate_passed":True},"scores":[
                    {"run_id":"r1","control_id":"ambiguity","method":"a","seed":0,"status":"scored","exact":True},
                    {"run_id":"r2","control_id":"arithmetic","method":"b","seed":1,"status":"scored","exact":True}]}
            scores_path=root/"scores.json"
            scores_path.write_text(json.dumps(scores,sort_keys=True,separators=(",",":"))+"\n")
            report=aggregate_report.aggregate(manifest,scores,scores_path)
            self.assertEqual(report["per_control"],{
                "ambiguity":{"run_count":1,"exact_run_count":1},
                "arithmetic":{"run_count":1,"exact_run_count":1}})
            self.assertEqual(report["scores_sha256"],self.digest(scores_path))
            reordered={**scores,"scores":list(reversed(scores["scores"]))}
            with self.assertRaises(run_matrix.Failure):
                aggregate_report.aggregate(manifest,reordered,scores_path)
            unresolved={**scores,"scores":[{**scores["scores"][0],"status":"incomplete"},scores["scores"][1]]}
            with self.assertRaises(run_matrix.Failure):
                aggregate_report.aggregate(manifest,unresolved,scores_path)

    def test_final_verifier_index_accepts_exactly_38_ordered_one_run_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); runs=[]; entries=[]
            index_path=root/"candidate-outputs.json"
            evidence=index_path.with_name(index_path.name+".evidence")
            for index in range(1,39):
                run_id=f"run_{index:03d}"
                runs.append({"run_id":run_id})
                slot=evidence/f"run-{index-1:02d}"; slot.mkdir(parents=True)
                output=slot/"candidate.json"
                output.write_text(json.dumps({"run_id":run_id},separators=(",",":"))+"\n")
                entries.append({"run_id":run_id,"path":str(output.relative_to(root)),"sha256":self.digest(output)})
            index_path.write_text(json.dumps({"schema":"genetik-fdi-candidate-output-index-v1",
                "protocol_id":"fixture","run_manifest_sha256":"a"*64,"runs":entries}))
            candidates,parents=verify_matrix.expand_candidate_index(
                {"schema":"genetik-fdi-candidate-output-index-v1","protocol_id":"fixture",
                 "run_manifest_sha256":"a"*64,"runs":entries},runs,index_path)
            self.assertEqual([row["run_id"] for row in candidates],[row["run_id"] for row in runs])
            self.assertEqual(parents,[evidence/f"run-{i:02d}" for i in range(38)])
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.expand_candidate_index(
                    {"schema":"genetik-fdi-candidate-output-index-v1","runs":entries[:-1]},runs,index_path)
            swapped=[entries[1],entries[0],*entries[2:]]
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.expand_candidate_index(
                    {"schema":"genetik-fdi-candidate-output-index-v1","runs":swapped},runs,index_path)
            traversal=[dict(row) for row in entries]
            traversal[0]["path"]="../outside.json"
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.expand_candidate_index(
                    {"schema":"genetik-fdi-candidate-output-index-v1","runs":traversal},runs,index_path)
            external=root/"external.json"; external.write_text('{"run_id":"run_001"}\n')
            link=evidence/"run-00"/"candidate.json"
            link.unlink(); link.symlink_to(external)
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.expand_candidate_index(
                    {"schema":"genetik-fdi-candidate-output-index-v1","runs":entries},runs,index_path)

    def test_one_verifier_option_loads_two_row_combined_index(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); manifest_path=root/"manifest.json"
            runs=[{"run_id":"run-a"},{"run_id":"run-b"}]
            manifest={"protocol_id":"study-fixture","runs":runs}
            manifest_path.write_text(json.dumps(manifest,separators=(",",":"))+"\n")
            source_sha=self.digest(manifest_path)
            index_path=root/"candidate-output-index.json"
            evidence=index_path.with_name(index_path.name+".evidence")
            entries=[]
            for ordinal,run in enumerate(runs):
                slot=evidence/f"run-{ordinal:02d}"; slot.mkdir(parents=True)
                output=slot/"candidate.json"
                output.write_text(json.dumps({"run_id":run["run_id"]},separators=(",",":"))+"\n")
                entries.append({"run_id":run["run_id"],"path":str(output.relative_to(root)),
                                "sha256":self.digest(output)})
            index={"schema":"genetik-fdi-candidate-output-index-v1","protocol_id":"study-fixture",
                   "run_manifest_sha256":source_sha,"runs":entries}
            index_path.write_text(json.dumps(index,separators=(",",":"))+"\n")
            candidates,_=verify_matrix.load_cli_candidate_outputs(manifest,source_sha,
                None,[index_path])
            self.assertEqual([item["run_id"] for item in candidates],["run-a","run-b"])
            bad={**index,"protocol_id":"wrong"}
            bad_path=root/"bad-index.json"
            bad_path.write_text(json.dumps(bad,separators=(",",":"))+"\n")
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.load_candidate_outputs(manifest,source_sha,combined_index_path=bad_path)
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.load_cli_candidate_outputs(manifest,source_sha,index_path,[index_path])
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.load_cli_candidate_outputs(manifest,source_sha,None,[])
            detached={**index,"run_manifest_sha256":"c"*64}
            bad_path.write_text(json.dumps(detached,separators=(",",":"))+"\n")
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.load_candidate_outputs(manifest,source_sha,combined_index_path=bad_path)

    def test_final_verifier_expands_38_explicit_run_output_slots(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); runs=[]; paths=[]
            for i in range(38):
                run_id=f"run-{i:02d}"; runner_sha=f"{i+1:064x}"
                runs.append({"run_id":run_id,"runner_manifest_sha256":runner_sha})
                index=root/f"output-{i:02d}.json"; evidence=Path(str(index)+".evidence")/"run-00"
                evidence.mkdir(parents=True)
                candidate=evidence/"candidate.json"; candidate.write_text(json.dumps({"run_id":run_id})+"\n")
                index.write_text(json.dumps({"schema":"genetik-fdi-candidate-output-index-v1",
                    "protocol_id":"study","run_manifest_sha256":runner_sha,"runs":[{
                        "run_id":run_id,"path":str(candidate.relative_to(root)),"sha256":self.digest(candidate)}]}))
                paths.append(index)
            expanded,parents=verify_matrix.load_cli_candidate_outputs(
                {"protocol_id":"study","runs":runs},"unused",None,paths)
            self.assertEqual([row["run_id"] for row in expanded],[row["run_id"] for row in runs])
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.load_candidate_outputs(
                    {"protocol_id":"study","runs":runs},"unused",per_run_index_paths=paths[::-1])
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.load_candidate_outputs(
                    {"protocol_id":"study","runs":runs},"unused",per_run_index_paths=paths[:-1])
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.load_candidate_outputs(
                    {"protocol_id":"study","runs":runs},"unused",per_run_index_paths=[paths[0]]*len(runs))

    def test_receipt_index_is_compact_ordered_and_confined(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); index_path=root/"verified-receipts.json"
            evidence=index_path.with_name(index_path.name+".evidence"); runs=[]; entries=[]
            for i in range(38):
                run_id=f"run-{i:02d}"; runs.append({"run_id":run_id})
                slot=evidence/f"run-{i:02d}"; slot.mkdir(parents=True)
                item=slot/"receipt.json"
                item.write_text(json.dumps({"schema":"genetik-fdi-verified-receipt-v2",
                    "run_id":run_id,"verified":True},separators=(",",":"))+"\n")
                entries.append({"run_id":run_id,"path":str(item.relative_to(root)),"sha256":self.digest(item)})
            receipts=verify_matrix.expand_verified_index(
                {"schema":"genetik-fdi-verified-receipt-index-v1","runs":entries},runs,index_path)
            self.assertEqual([row["run_id"] for row in receipts],[row["run_id"] for row in runs])
            swapped=[entries[1],entries[0],*entries[2:]]
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.expand_verified_index(
                    {"schema":"genetik-fdi-verified-receipt-index-v1","runs":swapped},runs,index_path)
            entries[0]={**entries[0],"sha256":"0"*64}
            with self.assertRaises(run_matrix.Failure):
                verify_matrix.expand_verified_index(
                    {"schema":"genetik-fdi-verified-receipt-index-v1","runs":entries},runs,index_path)

    def test_verifier_requires_exact_replay_decision_and_work(self):
        runs=[]; candidates=[]
        for policy in ("ascending", "disagreement"):
            primary_id=f"primary-{policy}"; replay_id=f"replay-{policy}"
            common={"control_id":"threshold_m0","method":policy,"training_artifact":"training-a",
                    "training_sha256":"a"*64}
            runs.extend([{**common,"run_id":primary_id,"seed":0},
                         {**common,"run_id":replay_id,"seed":1,"replay_of":primary_id}])
            transcript={"run_id":primary_id,"control":"threshold","policy":policy,"seed_metadata":0,
                "stop_reason":"one_finite_behavior_class","steps":[{"input":-1,"response":{"value":1}}],
                "specification_identity":"spec","work":{"charged_work":17,"candidate_row_evaluations":3}}
            candidates.extend([{"transcript":transcript}, {"transcript":{**transcript,
                "run_id":replay_id,"seed_metadata":1}}])
        verify_matrix.verify_deterministic_replays(runs,candidates)
        mutated=[dict(row) for row in candidates]
        mutated[1]={"transcript":{**candidates[1]["transcript"],"stop_reason":"query_cap"}}
        with self.assertRaises(run_matrix.Failure):
            verify_matrix.verify_deterministic_replays(runs,mutated)
        unresolved=[dict(row) for row in candidates]
        unresolved[1]={"transcript":{**candidates[1]["transcript"],"stop_reason":"query_cap"}}
        with self.assertRaises(run_matrix.Failure):
            verify_matrix.verify_deterministic_replays(runs,unresolved)
        mutated=[dict(row) for row in candidates]
        changed={**candidates[1]["transcript"],"work":{
            **candidates[1]["transcript"]["work"],"charged_work":18}}
        mutated[1]={"transcript":changed}
        with self.assertRaises(run_matrix.Failure):
            verify_matrix.verify_deterministic_replays(runs,mutated)

    def test_scorer_expands_candidate_and_receipt_indexes_with_canonical_paths(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); records=[]; c_entries=[]; r_entries=[]
            ci=root/"candidate.json"; ri=root/"receipts.json"
            cr=ci.with_name(ci.name+".evidence"); rr=ri.with_name(ri.name+".evidence")
            for i in range(2):
                run_id=f"r{i}"; records.append({"run_id":run_id})
                for index_path,evidence,filename,entries,obj in (
                    (ci,cr,"candidate.json",c_entries,{"run_id":run_id}),
                    (ri,rr,"receipt.json",r_entries,{"run_id":run_id})):
                    slot=evidence/f"run-{i:02d}"; slot.mkdir(parents=True,exist_ok=True)
                    item=slot/filename; item.write_text(json.dumps(obj,separators=(",",":"))+"\n")
                    entries.append({"run_id":run_id,"path":str(item.relative_to(root)),"sha256":self.digest(item)})
            expanded=scorer._expand_index({"schema":"genetik-fdi-candidate-output-index-v1","runs":c_entries},ci,records,"candidate")
            self.assertEqual([row["run_id"] for row in expanded],["r0","r1"])
            expanded=scorer._expand_index({"schema":"genetik-fdi-verified-receipt-index-v1","runs":r_entries},ri,records,"receipt")
            self.assertEqual([row["run_id"] for row in expanded],["r0","r1"])
            c_entries[0]={**c_entries[0],"path":"../escape.json"}
            with self.assertRaises(scorer.Invalid):
                scorer._expand_index({"schema":"genetik-fdi-candidate-output-index-v1","runs":c_entries},ci,records,"candidate")

    def test_independent_preflight_validator_checks_two_catalogues_and_indexed_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); controls=["a-control","b-control"]
            fixtures={"a-control":"ambiguity","b-control":"arithmetic"}
            training={"a-control":b"genetik-fdi-training-v1\n0\t0\n",
                      "b-control":b"genetik-fdi-training-v1\n0\t2\n"}
            assessment={"a-control":b"genetik-fdi-assessment-v1\n9\t9\n",
                        "b-control":b"genetik-fdi-assessment-v1\n9\t11\n"}
            paths={}
            runs=[]; training_bundle={"schema":"genetik-fdi-training-bundle-v1","artifacts":[]}
            assessment_bundle={"schema":"genetik-fdi-assessment-bundle-v1","artifacts":[]}
            scorer_controls=[]; bindings=[]; catalogue_map=[]; runner_runs=[]
            for ordinal,control_id in enumerate(controls):
                fixture=fixtures[control_id]; tid=f"synthetic-train-{control_id}"; aid=f"synthetic-assess-{control_id}"
                tp=root/f"{control_id}.training.tsv"; tp.write_bytes(training[control_id]); paths[control_id]=str(tp)
                ph=hashlib.sha256(training[control_id]).hexdigest(); ah=hashlib.sha256(assessment[control_id]).hexdigest()
                spath=root/f"{control_id}.assessment.tsv"; spath.write_bytes(assessment[control_id])
                protocol_train=f"protocol-training-{control_id}"; protocol_assess=f"protocol-assess-{control_id}"
                scorer_controls.append({"id":control_id,"catalogue":fixture,"reference_member":0,
                    "protocol_training_artifact":protocol_train,"protocol_training_sha256":"1"*64,
                    "protocol_assessment_artifact":protocol_assess,"protocol_assessment_sha256":"2"*64,
                    "training":{"id":tid,"sha256":ph},"assessment":{"id":aid,"sha256":ah}})
                bindings.append({"control_id":control_id,"protocol_training_artifact":protocol_train,
                    "protocol_training_sha256":"1"*64,"synthetic_training_artifact":tid,
                    "synthetic_training_sha256":ph,"protocol_assessment_artifact":protocol_assess,
                    "protocol_assessment_sha256":"2"*64,"synthetic_assessment_artifact":aid,
                    "synthetic_assessment_sha256":ah})
                catalogue_map.append({"control_id":control_id,"catalogue":fixture,"reference_member":0})
                training_bundle["artifacts"].append({"id":tid,"sha256":ph,
                    "content_base64":base64.b64encode(training[control_id]).decode()})
                assessment_bundle["artifacts"].append({"id":aid,"sha256":ah,
                    "content_base64":base64.b64encode(assessment[control_id]).decode()})
                run={"run_id":f"run-{ordinal}","control_id":control_id,"method":"disagreement",
                    "seed":ordinal,"training_artifact":tid,"training_sha256":ph,"training_path":str(tp)}
                runs.append(run); runner_runs.append(run)
            runner_manifest={"schema":"genetik-fdi-run-manifest-v2","mode":"synthetic-preflight",
                "protocol_id":"fixture-preflight","preflight_id":"fixture-preflight","runs":runner_runs,
                "catalogue_map":catalogue_map}
            runner_bytes=preflight.canonical(runner_manifest)+b"\n"; runner_sha=hashlib.sha256(runner_bytes).hexdigest()
            scorer_manifest={"schema":"genetik-fdi-scorer-manifest-v2","mode":"synthetic-preflight",
                "protocol_id":"fixture-preflight","preflight_id":"fixture-preflight",
                "runner_manifest_sha256":runner_sha,"runs":runner_runs,"controls":scorer_controls}
            aggregate_manifest={"schema":"genetik-fdi-aggregate-manifest-v2","mode":"synthetic-preflight",
                "protocol_id":"fixture-preflight","preflight_id":"fixture-preflight","runs":runner_runs}
            candidate_path=root/"candidate-outputs.json"; candidate_evidence=Path(str(candidate_path)+".evidence")
            receipt_path=root/"verified-receipts.json"; receipt_evidence=Path(str(receipt_path)+".evidence")
            candidate_entries=[]; receipt_entries=[]; candidates=[]; receipts=[]; scores=[]
            for ordinal,run in enumerate(runs):
                control=run["control_id"]; fixture=fixtures[control]; ph=run["training_sha256"]
                steps=[] if fixture=="ambiguity" else [{"input":-8,"response":{"value":-6},"revision":1}]
                candidate={"schema":"genetik-fdi-candidate-output-v2","run_id":run["run_id"],
                    "control_id":control,"method":run["method"],"seed":run["seed"],
                    "training_artifact":run["training_artifact"],"training_sha256":ph,
                    "protocol_id":"fixture-preflight","run_manifest_sha256":runner_sha,
                    "completion_status":"completed","report_validation_status":"valid_report",
                    "transcript":{"stop_reason":"one_finite_behavior_class","steps":steps},
                    "receipt":{"wait4_usage_complete":True},"evidence_directory":".",
                    "raw_stream_sha256":{key:"a"*64 for key in ("producer_stdout","peer_stdout","producer_stderr","peer_stderr")}}
                slot=candidate_evidence/f"run-{ordinal:02d}"; slot.mkdir(parents=True)
                cp=slot/"candidate.json"; cp.write_bytes(preflight.canonical(candidate)+b"\n")
                candidate_entries.append({"run_id":run["run_id"],"path":str(cp.relative_to(root)),"sha256":self.digest(cp)})
                candidates.append(candidate)
                verified={"schema":"genetik-fdi-verified-receipt-v2","run_id":run["run_id"],
                    "candidate_output_sha256":hashlib.sha256(preflight.canonical(candidate)).hexdigest(),
                    "run_manifest_sha256":runner_sha,"training_sha256":ph,"verified":True,
                    "transcript":candidate["transcript"],"candidate_identity":{
                        key:candidate[key] for key in ("schema","protocol_id","run_id","control_id","method",
                            "seed","training_artifact","training_sha256","completion_status","report_validation_status")},
                    "completion_status":"completed","report_validation_status":"valid_report",
                    "execution_receipt":candidate["receipt"]}
                rslot=receipt_evidence/f"run-{ordinal:02d}"; rslot.mkdir(parents=True)
                rp=rslot/"receipt.json"; rp.write_bytes(preflight.canonical(verified)+b"\n")
                receipt_entries.append({"run_id":run["run_id"],"path":str(rp.relative_to(root)),"sha256":self.digest(rp)})
                receipts.append(verified)
                if fixture=="ambiguity":
                    pred={"c0":{"training":[0],"assessment":[9]},"c1":{"training":[0],"assessment":[9]},"c2":{"training":[0],"assessment":[10]}}
                    train_ids,assess_ids=["c0","c1","c2"],["c0","c1"]
                else:
                    pred={"c0":{"training":[2,-6],"assessment":[11]},"c1":{"training":[2,-6],"assessment":[11]}}
                    train_ids,assess_ids=["c0","c1"],["c0","c1"]
                scores.append({"run_id":run["run_id"],"control_id":control,"method":run["method"],"seed":run["seed"],
                    "training_exact_candidate_ids":train_ids,"assessment_exact_candidate_ids":assess_ids,
                    "predictions":pred,"exact":True,"status":"scored"})
            candidate_index={"schema":"genetik-fdi-candidate-output-index-v1","protocol_id":"fixture-preflight",
                "run_manifest_sha256":runner_sha,"runs":candidate_entries}
            receipt_index={"schema":"genetik-fdi-verified-receipt-index-v1","runs":receipt_entries}
            score_output={"schema":"genetik-fdi-scores-v2","protocol_id":"fixture-preflight",
                "run_manifest_sha256":hashlib.sha256(preflight.canonical(scorer_manifest)+b"\n").hexdigest(),"scores":scores}
            report={"schema":"genetik-fdi-report-v2","protocol_id":"fixture-preflight",
                "scores_sha256":"b"*64,"expected_run_count":2,"scored_run_count":2,
                "all_scored":True,"acceptance_gate_passed":True,
                "per_control":{run["control_id"]:{"run_count":1,"exact_run_count":1} for run in runs}}
            result=preflight._validate_fdi_synthetic_pipeline(preflight_id="fixture-preflight",
                runner_manifest=runner_manifest,runner_manifest_bytes=runner_bytes,scorer_manifest=scorer_manifest,
                runs=runs,training_bytes_by_control=training,assessment_bytes_by_control=assessment,
                training_path_by_control=paths,training_bundle=training_bundle,assessment_bundle=assessment_bundle,
                aggregate_manifest=aggregate_manifest,candidate_records=candidate_index,verified_records=receipt_index,
                score_output=score_output,report_output=report,scores_sha256="b"*64,
                candidate_index_path=candidate_path,receipt_index_path=receipt_path,
                protocol_training_bindings=bindings)
            self.assertEqual(result["run_count"],2)

    def test_synthetic_scorer_receipt_only_mode_binds_hash_and_reads_step_shape(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); train=root/"training.tsv"; train_bytes=b"genetik-fdi-training-v1\n0\t2\n"
            train.write_bytes(train_bytes); train_sha=self.digest(train)
            assess_bytes=b"genetik-fdi-assessment-v1\n9\t11\n"; assess_sha=hashlib.sha256(assess_bytes).hexdigest()
            bundle=root/"assessment.json"; bundle.write_text(json.dumps({"schema":"genetik-fdi-assessment-bundle-v1",
                "artifacts":[{"id":"assess-a","sha256":assess_sha,
                    "content_base64":base64.b64encode(assess_bytes).decode()}]}))
            manifest_path=root/"scorer-manifest.json"; manifest_path.write_text("pinned synthetic scorer manifest\n")
            transcript={"stop_reason":"one_finite_behavior_class","steps":[{
                "input":-8,"query_id":"q1","revision":1,"response":{"type":"int64","value":-6}}]}
            record={"run_id":"fixture-arithmetic","control_id":"arithmetic-a","method":"disagreement","seed":0,
                "training_artifact":"train-a","training_sha256":train_sha,"training_path":str(train),
                "assessment_artifact":"assess-a","assessment_sha256":assess_sha}
            identity={"schema":"genetik-fdi-candidate-output-v2","protocol_id":"preflight-a",
                "run_id":record["run_id"],"control_id":record["control_id"],"method":record["method"],"seed":0,
                "training_artifact":"train-a","training_sha256":train_sha,
                "completion_status":"completed","report_validation_status":"valid_report"}
            output_sha="d"*64; runner_sha="b"*64
            candidate={**identity,"run_manifest_sha256":runner_sha,"transcript":transcript,
                "receipt":{"run_id":record["run_id"]},"candidate_output_sha256":output_sha}
            receipt={"schema":"genetik-fdi-verified-receipt-v2","run_id":record["run_id"],
                "candidate_output_sha256":output_sha,"run_manifest_sha256":runner_sha,
                "training_sha256":train_sha,"verified":True,"candidate_identity":identity,
                "transcript":transcript,"execution_receipt":{"wait4_usage_complete":True}}
            manifest={"schema":"genetik-fdi-scorer-manifest-v2","mode":"synthetic-preflight",
                "protocol_id":"preflight-a","runner_manifest_sha256":runner_sha,"runs":[record],
                "controls":[{"id":"arithmetic-a","catalogue":"arithmetic","reference_member":0,
                    "training":{"id":"train-a","sha256":train_sha},
                    "assessment":{"id":"assess-a","sha256":assess_sha}}]}
            fake_verified={"compatible":["c0","c1"],"ids":["c0","c1"],
                "trees":[scorer.Parser("(x0 + 2)").parse(),scorer.Parser("(x0 + 2)").parse()],
                "observed":[(0,2),(-8,-6)]}
            output=root/"scores.json"
            with mock.patch.object(scorer,"verify_transcript",return_value=fake_verified):
                scorer.score_synthetic_preflight(manifest,[candidate],[receipt],bundle,output,manifest_path,
                    derived_candidates=True)
            result=json.loads(output.read_text())
            self.assertTrue(result["summary"]["acceptance_gate_passed"])
            self.assertEqual(result["scores"][0]["assessment_exact_candidate_ids"],["c0","c1"])
            malformed={**candidate,"candidate_output_sha256":"e"*64}
            with self.assertRaises(scorer.Invalid):
                scorer.score_synthetic_preflight(manifest,[malformed],[receipt],bundle,root/"bad.json",manifest_path,
                    derived_candidates=True)

    def test_unresolved_positive_run_is_never_complete_or_exact(self):
        records=[{"control":"conditional-modular"},{"control":"ambiguity"},{"control":"outside"}]
        outcomes=[{"runner_stop_reason":"query_cap"},
                  {"runner_stop_reason":"one_finite_behavior_class"},
                  {"runner_stop_reason":"empty_compatible_set"}]
        self.assertFalse(scorer.complete_stop_outcomes(records,outcomes))
        outcomes[0]["runner_stop_reason"]="one_finite_behavior_class"
        self.assertTrue(scorer.complete_stop_outcomes(records,outcomes))

    def test_synthetic_scorer_requires_actual_adaptive_revision(self):
        transcript={"stop_reason":"one_finite_behavior_class","steps":[{
            "input":-8,"query_id":"q1","revision":1,
            "response":{"type":"int64","value":-6}}]}
        scorer.require_synthetic_fixture_transcript("arithmetic",transcript)
        for malformed in (
            {**transcript,"stop_reason":"query_cap"},
            {**transcript,"steps":[{**transcript["steps"][0],"input":-7}]},
            {**transcript,"steps":[{**transcript["steps"][0],"response":{"type":"int64","value":-5}}]},
            {**transcript,"steps":[{**transcript["steps"][0],"revision":True}]},
        ):
            with self.assertRaises(scorer.Invalid):
                scorer.require_synthetic_fixture_transcript("arithmetic",malformed)
        scorer.require_synthetic_fixture_transcript("ambiguity",{"stop_reason":"one_finite_behavior_class","steps":[]})
        with self.assertRaises(scorer.Invalid):
            scorer.require_synthetic_fixture_transcript("ambiguity",transcript)

    def test_revision_replay_rejects_wrong_predecessor_or_unresolved_checks(self):
        rows=[(0,0),(1,1)]
        initial_identity=scorer.specification_identity(rows[:1])
        final_identity=scorer.specification_identity(rows)
        transcript={"initial_observations":[{"input":0,"expected":{"type":"int64","value":0}}],
            "specification_identity":final_identity,
            "steps":[{"revision":1,"revision_id":"revision_1","query_id":"q1",
                "predecessor_specification_identity":initial_identity,"input":1,
                "counterexample":{"query_id":"q1","observation":{"inputs":[1],
                    "expected":{"type":"int64","value":1}}},
                "response":{"type":"int64","value":1},"specification_identity":final_identity,
                "validations":[{"candidate_id":"c0","status":"ValidatedFiniteDomain"}]}]}
        record={"run_id":"run_001"}
        scorer.verify_revision_chain(record,transcript,["c0"],rows)
        transcript["steps"][0]["predecessor_specification_identity"]="wrong"
        with self.assertRaises(scorer.Invalid):
            scorer.verify_revision_chain(record,transcript,["c0"],rows)
        transcript["steps"][0]["predecessor_specification_identity"]=initial_identity
        with self.assertRaises(scorer.Invalid):
            scorer.verify_revision_chain(record,{**transcript,"steps":[]},["c0"],rows)
        transcript["steps"][0]["validations"][0]["status"]="ResourceExhausted"
        with self.assertRaises(scorer.Invalid):
            scorer.verify_revision_chain(record,transcript,["c0"],rows)


if __name__=="__main__":
    unittest.main()
