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
