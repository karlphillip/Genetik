"""Independent-parser contracts; this suite does not invoke the selector or oracle runner."""
import importlib.util
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

TOOL = Path(__file__).parents[1] / "tools" / "score_transcripts.py"
spec = importlib.util.spec_from_file_location("fdi_scorer", TOOL)
scorer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scorer)

class ScorerContract(unittest.TestCase):
    @staticmethod
    def replay_fixture():
        records = [
            {"run_id": "primary", "control": "threshold", "reference_member": 0,
             "reference_candidate_id": "c0", "policy": "ascending", "seed_metadata": 0,
             "training_artifact": "training-a", "training_sha256": "a" * 64, "assessment_artifact": "assessment-a",
             "assessment_sha256": "b" * 64, "assessment_observation_count": 18},
            {"run_id": "replay", "control": "threshold", "reference_member": 0,
             "reference_candidate_id": "c0", "policy": "ascending", "seed_metadata": 1,
             "training_artifact": "training-a", "training_sha256": "a" * 64, "assessment_artifact": "assessment-a",
             "assessment_sha256": "b" * 64, "assessment_observation_count": 18,
             "replay_of": "primary"},
            {"run_id": "primary2", "control": "threshold", "reference_member": 0,
             "reference_candidate_id": "c0", "policy": "disagreement", "seed_metadata": 0,
             "training_artifact": "training-a", "training_sha256": "a" * 64, "assessment_artifact": "assessment-a",
             "assessment_sha256": "b" * 64, "assessment_observation_count": 18},
            {"run_id": "replay2", "control": "threshold", "reference_member": 0,
             "reference_candidate_id": "c0", "policy": "disagreement", "seed_metadata": 1,
             "training_artifact": "training-a", "training_sha256": "a" * 64, "assessment_artifact": "assessment-a",
             "assessment_sha256": "b" * 64, "assessment_observation_count": 18,
             "replay_of": "primary2"},
        ]
        transcript = {"run_id": "primary", "control": "threshold", "policy": "ascending",
            "seed_metadata": 0, "stop_reason": "one_finite_behavior_class",
            "steps": [{"input": -1, "response": 1, "eliminated_candidate_ids": ["c1"]}],
            "specification_identity": "spec-hash", "work": {"candidate_row_evaluations": 17,
                "charged_work": 43}}
        replay_transcript = {**transcript, "run_id": "replay", "seed_metadata": 1}
        transcript2 = {**transcript, "run_id": "primary2", "policy": "disagreement"}
        replay_transcript2 = {**transcript2, "run_id": "replay2", "seed_metadata": 1}
        score = {"run_id": "primary", "control": "threshold", "policy": "ascending",
            "assessment_artifact": "assessment-a", "assessment_sha256": "b" * 64,
            "assessment_observation_count": 18,
            "runner_stop_reason": "one_finite_behavior_class", "training_exact_candidate_ids": ["c0"],
            "assessment_exact_candidate_ids": ["c0"], "candidate_finite_assessment": {"c0": {"exact": True}},
            "finite_identifiability_only": True, "finite_certificate_scope": "query-domain[-8,8]",
            "outside_catalogue_empty_set": False, "reference_catalogue_member_present": True,
            "candidate_count": 4, "queried_inputs": [-1], "status": "scored", "exact": True,
            "work": {"selector_charged_work": 43, "oracle_operations": 2,
                     "combined_charged_work": 45, "cpu_seconds": 0.01, "wall_seconds": 0.02}}
        replay_score = {**score, "run_id": "replay", "work": {**score["work"],
            "cpu_seconds": 0.03, "wall_seconds": 0.04}}
        score2 = {**score, "run_id": "primary2", "policy": "disagreement"}
        replay_score2 = {**score2, "run_id": "replay2", "work": {**score2["work"],
            "cpu_seconds": 0.05, "wall_seconds": 0.06}}
        return records, [transcript, replay_transcript, transcript2, replay_transcript2], \
            [score, replay_score, score2, replay_score2]

    def test_deterministic_replay_allows_only_run_and_seed_metadata_differences(self):
        records, transcripts, scores = self.replay_fixture()
        result = scorer.deterministic_replay_assessment(records, transcripts, scores)
        self.assertEqual(result, {"passed": True, "replay_count": 2, "failures": []})

    def test_deterministic_replay_rejects_mutated_decision_or_work(self):
        for field, value in (("stop_reason", "query_cap"), ("work", {"charged_work": 44})):
            with self.subTest(field=field):
                records, transcripts, scores = self.replay_fixture()
                if field == "work":
                    transcripts[1][field] = {**transcripts[1][field], **value}
                else:
                    transcripts[1][field] = value
                result = scorer.deterministic_replay_assessment(records, transcripts, scores)
                self.assertFalse(result["passed"])
                expected = "replay:unresolved_outcome" if field == "stop_reason" else "replay:transcript_mismatch"
                self.assertIn(expected, result["failures"])
        records, transcripts, scores = self.replay_fixture()
        scores[1]["work"]["combined_charged_work"] += 1
        result = scorer.deterministic_replay_assessment(records, transcripts, scores)
        self.assertFalse(result["passed"])
        self.assertIn("replay:decision_or_work_mismatch", result["failures"])
        records, transcripts, scores = self.replay_fixture()
        scores[1]["assessment_sha256"] = "c" * 64
        result = scorer.deterministic_replay_assessment(records, transcripts, scores)
        self.assertFalse(result["passed"])
        self.assertIn("replay:decision_or_work_mismatch", result["failures"])

    def test_unresolved_replay_cannot_qualify(self):
        records, transcripts, scores = self.replay_fixture()
        scores[1]["status"] = "inconclusive"
        result = scorer.deterministic_replay_assessment(records, transcripts, scores)
        self.assertFalse(result["passed"])
        self.assertIn("replay:unresolved_outcome", result["failures"])
        records, transcripts, scores = self.replay_fixture()
        scores[1]["runner_stop_reason"] = "query_cap"
        result = scorer.deterministic_replay_assessment(records, transcripts, scores)
        self.assertFalse(result["passed"])
        self.assertIn("replay:unresolved_outcome", result["failures"])

    def test_missing_or_misbound_replay_fails_closed(self):
        records, transcripts, scores = self.replay_fixture()
        self.assertFalse(scorer.deterministic_replay_assessment(records[:1], transcripts[:1], scores[:1])["passed"])
        records[1]["replay_of"] = "absent"
        result = scorer.deterministic_replay_assessment(records, transcripts, scores)
        self.assertFalse(result["passed"])
        self.assertTrue(any("missing_predecessor" in failure for failure in result["failures"]))

    def test_failed_replay_is_rejected_before_assessment_loader_runs(self):
        for field, value in (("training_artifact", "other-training"),
                             ("assessment_artifact", "other-assessment"),
                             ("assessment_sha256", "c" * 64),
                             ("assessment_observation_count", 17)):
            with self.subTest(field=field):
                records, transcripts, _ = self.replay_fixture()
                records[1][field] = value
                loaded = [(record, transcript, None) for record, transcript in zip(records, transcripts)]
                called = []
                with self.assertRaises(scorer.Invalid):
                    scorer.load_assessment_data_after_replay_gate(loaded, lambda rows: called.append(rows))
                self.assertEqual(called, [])

    def test_current_campaign_method_seed_schema_passes_prelabel_replay_gate(self):
        records, transcripts, _ = self.replay_fixture()
        campaign_records = []
        for record in records:
            current = dict(record)
            current["method"] = current.pop("policy")
            current["seed"] = current.pop("seed_metadata")
            campaign_records.append(current)
        loaded = [(record, transcript, None) for record, transcript in zip(campaign_records, transcripts)]
        called = []
        self.assertEqual(scorer.load_assessment_data_after_replay_gate(
            loaded, lambda rows: called.append(rows) or "loaded"), "loaded")
        self.assertEqual(len(called), 1)
        campaign_records[1]["seed_metadata"] = 9
        with self.assertRaises(scorer.Invalid):
            scorer.load_assessment_data_after_replay_gate(
                [(record, transcript, None) for record, transcript in zip(campaign_records, transcripts)],
                lambda rows: self.fail("assessment loader ran after conflicting replay identity"))

    def test_replay_criterion_is_part_of_the_scientific_acceptance_gate(self):
        records=[]; transcripts=[]; scores=[]; index=0
        for control in ("arithmetic", "modular", "threshold", "conditional-modular"):
            for member in range(4):
                for policy in ("ascending", "disagreement"):
                    run_id=f"r{index}"; index+=1
                    steps=([1, 1] if control in {"arithmetic", "conditional-modular"} or
                           policy == "disagreement" else [1, 2, 3])
                    transcript={"run_id":run_id,"control":control,"policy":policy,"seed_metadata":0,
                        "stop_reason":"one_finite_behavior_class","steps":[{"input":n} for n in range(len(steps))],
                        "work":{"candidate_row_evaluations":4,"charged_work":10}}
                    score={"run_id":run_id,"control":control,"policy":policy,
                        "assessment_artifact":f"assessment-{control}-{member}",
                        "assessment_sha256":f"{member+1:064x}","assessment_observation_count":18,
                        "runner_stop_reason":"one_finite_behavior_class",
                        "training_exact_candidate_ids":["c0"],"assessment_exact_candidate_ids":["c0"],
                        "candidate_finite_assessment":{"c0":{"exact":True}},
                        "finite_identifiability_only":True,"finite_certificate_scope":"query-domain[-8,8]",
                        "outside_catalogue_empty_set":False,"reference_catalogue_member_present":True,
                        "candidate_count":4,"queried_inputs":list(range(len(steps))),
                        "work":{"selector_charged_work":10,"oracle_operations":0,"combined_charged_work":10},
                        "status":"scored","exact":True}
                    records.append({"run_id":run_id,"control":control,"reference_member":member,
                        "reference_candidate_id":f"c{member}","policy":policy,"seed_metadata":0,
                        "training_artifact":f"training-{control}-{member}",
                        "training_sha256":f"{member:064x}","assessment_artifact":f"assessment-{control}-{member}",
                        "assessment_sha256":f"{member+1:064x}","assessment_observation_count":18})
                    transcripts.append(transcript); scores.append(score)
        for control in ("ambiguity", "outside"):
            for policy in ("ascending", "disagreement"):
                run_id=f"r{index}"; index+=1
                stop="one_finite_behavior_class" if control=="ambiguity" else "empty_compatible_set"
                transcripts.append({"run_id":run_id,"control":control,"policy":policy,"seed_metadata":0,
                    "stop_reason":stop,"steps":[],"work":{"candidate_row_evaluations":1,"charged_work":1}})
                scores.append({"run_id":run_id,"control":control,"policy":policy,"runner_stop_reason":stop,
                    "assessment_artifact":f"assessment-{control}-0","assessment_sha256":f"{index+1:064x}",
                    "assessment_observation_count":18,
                    "training_exact_candidate_ids":["c0","c1"] if control=="ambiguity" else [],
                    "assessment_exact_candidate_ids":["c0"] if control=="ambiguity" else [],
                    "candidate_finite_assessment":{"c0":{"exact":True}} if control=="ambiguity" else {},
                    "finite_identifiability_only":control=="ambiguity",
                    "finite_certificate_scope":"query-domain[-8,8]" if control=="ambiguity" else None,
                    "outside_catalogue_empty_set":control=="outside",
                    "reference_catalogue_member_present":control=="ambiguity","candidate_count":3,
                    "queried_inputs":[],"work":{"selector_charged_work":1,"oracle_operations":0,
                        "combined_charged_work":1},"status":"scored","exact":control=="ambiguity"})
                records.append({"run_id":run_id,"control":control,"reference_member":0,
                    "policy":policy,"seed_metadata":0,"training_artifact":f"training-{control}-0",
                    "training_sha256":f"{index:064x}"})
        for policy in ("ascending", "disagreement"):
            primary_index=next(i for i,row in enumerate(records) if row["control"]=="threshold" and
                row["reference_member"]==0 and row["policy"]==policy)
            primary_id=records[primary_index]["run_id"]
            run_id=f"r{index}"; index+=1
            records.append({**records[primary_index],"run_id":run_id,"seed_metadata":1,"replay_of":primary_id})
            transcripts.append({**transcripts[primary_index],"run_id":run_id,"seed_metadata":1})
            scores.append({**scores[primary_index],"run_id":run_id,
                "work":dict(scores[primary_index]["work"])})
        summary=scorer.summarize(records,transcripts,scores)
        self.assertTrue(summary["deterministic_replays_passed"])
        self.assertTrue(summary["acceptance_gate_passed"])
        replay_index=next(i for i,row in enumerate(records) if row.get("replay_of"))
        scores[replay_index]["work"]["combined_charged_work"]+=1
        failed=scorer.summarize(records,transcripts,scores)
        self.assertFalse(failed["deterministic_replays_passed"])
        self.assertFalse(failed["acceptance_gate_passed"])

    def test_sealed_assessment_manifest_binds_paths_and_exact_hashes(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); data=root/"assessment.tsv"
            payload=b"genetik-fdi-assessment-v1\n9\t11\n"; data.write_bytes(payload)
            digest=hashlib.sha256(payload).hexdigest()
            manifest=root/"sealed-identity.json"
            manifest.write_text(json.dumps({"schema":"genetik-fdi-sealed-assessment-manifest-v1",
                "artifacts":[{"id":"assessment-a","path":str(data),"sha256":digest}]}))
            loaded=[({"run_id":"r1","assessment_artifact":"assessment-a","assessment_sha256":digest},)]
            self.assertEqual(scorer.load_sealed_assessments(manifest,loaded),{"r1":[(9,11)]})
            wrong=[({"run_id":"r1","assessment_artifact":"assessment-b","assessment_sha256":digest},)]
            with self.assertRaises(scorer.Invalid): scorer.load_sealed_assessments(manifest,wrong)
            altered=payload+b"0\t0\n"; data.write_bytes(altered)
            with self.assertRaises(scorer.Invalid): scorer.load_sealed_assessments(manifest,loaded)

    def test_final_scoring_candidate_can_be_replayed_from_verified_receipt_index(self):
        receipt={"schema":"genetik-fdi-verified-receipt-v2","run_id":"r1","verified":True,
            "candidate_output_sha256":"a"*64,"run_manifest_sha256":"b"*64,"training_sha256":"c"*64,
            "completion_status":"completed","report_validation_status":"valid_report",
            "transcript":{"stop_reason":"one_finite_behavior_class","steps":[]},
            "execution_receipt":{"cpu_seconds":1},
            "candidate_identity":{"schema":"genetik-fdi-candidate-output-v2","protocol_id":"p",
                "run_id":"r1","control_id":"c1","method":"disagreement","seed":3,
                "training_artifact":"t1","training_sha256":"c"*64,
                "completion_status":"completed","report_validation_status":"valid_report"}}
        candidate=scorer.candidates_from_verified_receipts([receipt])[0]
        self.assertEqual(candidate["run_id"],"r1")
        self.assertEqual(candidate["transcript"],receipt["transcript"])
        self.assertEqual(candidate["candidate_output_sha256"],"a"*64)
        with self.assertRaises(scorer.Invalid):
            scorer.candidates_from_verified_receipts([{**receipt,"verified":False}])

    def test_negative_constant_and_checked_int64(self):
        expression = scorer.Parser("(x0 < -1)").parse()
        self.assertTrue(scorer.evaluate(expression, -2))
        self.assertFalse(scorer.evaluate(expression, -1))
        with self.assertRaises(scorer.Invalid):
            scorer.evaluate(scorer.Parser("(9223372036854775807 + 1)").parse(), 0)

    def test_conditional_is_lazy_and_boolean_typed(self):
        expression = scorer.Parser("if((x0 < 0), (0 - x0), x0)").parse()
        self.assertEqual(scorer.evaluate(expression, -3), 3)
        self.assertEqual(scorer.evaluate(expression, 4), 4)

    def test_finite_equivalence_is_not_global_identity(self):
        identity = scorer.Parser("x0").parse()
        overfit = scorer.Parser("if((x0 > 8), (x0 + 1), x0)").parse()
        self.assertEqual([scorer.evaluate(identity, n) for n in range(-8, 9)],
                         [scorer.evaluate(overfit, n) for n in range(-8, 9)])
        self.assertNotEqual(scorer.evaluate(identity, 9), scorer.evaluate(overfit, 9))

    def test_training_exactness_does_not_imply_assessment_exactness(self):
        target = scorer.Parser("if((x0 > 8), (x0 + 1), x0)").parse()
        candidate = scorer.Parser("x0").parse()
        self.assertEqual(scorer.evaluate(candidate, 0), scorer.evaluate(target, 0))
        self.assertNotEqual(scorer.evaluate(candidate, 9), scorer.evaluate(target, 9))

    def test_complexity_is_structural(self):
        nodes, depth = scorer.complexity(scorer.Parser("(x0 + 0)").parse())
        self.assertEqual((nodes, depth), (3, 2))

    def test_revision_chain_recomputes_predecessor_and_identity(self):
        observed=[(0,0),(1,1)]
        initial_identity=scorer.specification_identity(observed[:1])
        final_identity=scorer.specification_identity(observed)
        transcript={"initial_observations":[{"input":0,"expected":{"type":"int64","value":0}}],
            "specification_identity":final_identity,
            "steps":[{"revision":1,"revision_id":"revision_1","query_id":"q1",
                "predecessor_specification_identity":initial_identity,"input":1,
                "counterexample":{"query_id":"q1","observation":{"inputs":[1],
                    "expected":{"type":"int64","value":1}}},
                "response":{"type":"int64","value":1},"specification_identity":final_identity,
                "validations":[{"candidate_id":"c0","status":"ValidatedFiniteDomain"}]}]}
        self.assertEqual(scorer.verify_revision_chain({"run_id":"run_001"},transcript,["c0"],observed),final_identity)
        transcript["steps"][0]["predecessor_specification_identity"]="forged"
        with self.assertRaises(scorer.Invalid):
            scorer.verify_revision_chain({"run_id":"run_001"},transcript,["c0"],observed)
        transcript["steps"][0]["predecessor_specification_identity"]=initial_identity
        transcript["steps"][0]["revision"]=True
        with self.assertRaises(scorer.Invalid):
            scorer.verify_revision_chain({"run_id":"run_001"},transcript,["c0"],observed)

    def test_unresolved_query_cap_cannot_pass_positive_outcome_gate(self):
        records=[{"control":"modular"},{"control":"ambiguity"},{"control":"outside"}]
        scores=[{"runner_stop_reason":"query_cap"},
                {"runner_stop_reason":"one_finite_behavior_class"},
                {"runner_stop_reason":"empty_compatible_set"}]
        self.assertFalse(scorer.complete_stop_outcomes(records,scores))
        scores[0]["runner_stop_reason"]="one_finite_behavior_class"
        self.assertTrue(scorer.complete_stop_outcomes(records,scores))

    def test_peer_responses_are_query_scoped_and_checked(self):
        from importlib.util import spec_from_file_location, module_from_spec
        peer_path=Path(__file__).parents[1]/"tools"/"oracle_peer.py"
        peer_spec=spec_from_file_location("fdi_oracle_peer",peer_path)
        peer=module_from_spec(peer_spec); peer_spec.loader.exec_module(peer)
        query_id,n=peer.parse_request('{"type":"query","id":"q1","payload":{"input":-1}}')
        self.assertEqual((query_id,n),("q1",-1))
        self.assertEqual(peer.oracle("modular",0,n)[0],2)
        with self.assertRaises(peer.PeerError):
            peer.parse_request('{"type":"query","id":"q1","payload":{"input":-9}}')

if __name__ == "__main__": unittest.main()
