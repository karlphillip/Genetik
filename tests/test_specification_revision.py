"""Offline contracts for authenticated finite specification revisions."""
from __future__ import annotations

import copy
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from specification_revision import (  # noqa: E402
    IncompleteSpecificationRevision,
    Observation,
    SpecificationRevisionError,
    specification_identity,
    verify_revision_chain,
)


def obs(x: int, y: int) -> dict:
    return {"inputs": [x], "expected": {"type": "int64", "value": y}}


class SpecificationRevisionTests(unittest.TestCase):
    def test_canonical_identity_matches_independent_genetik_literal(self):
        rows = [
            Observation((0,), "int64", 0),
            Observation((1,), "bool", True),
            Observation((-2,), "bool", False),
        ]
        self.assertEqual(
            specification_identity(rows),
            "discrete-spec-v1;d=1;rows=3;[0,]=i0;[1,]=b1;[-2,]=b0;",
        )

    def chain(self):
        initial = [obs(0, 0), obs(1, 1)]
        first = [Observation.from_mapping(item) for item in initial]
        identity0 = specification_identity(first)
        after_first = [*first, Observation.from_mapping(obs(2, 2))]
        identity1 = specification_identity(after_first)
        after_second = [*after_first, Observation.from_mapping(obs(3, 3))]
        identity2 = specification_identity(after_second)
        candidates = ["candidate-a", "candidate-b"]
        revisions = [
            {"revision": 1, "revision_id": "revision-1", "predecessor_identity": identity0,
             "counterexample": {"query_id": "query-1", "observation": obs(2, 2)},
             "specification_identity": identity1,
             "candidate_checks": [{"candidate_id": cid, "outcome": "consistent"} for cid in candidates]},
            {"revision": 2, "revision_id": "revision-2", "predecessor_identity": identity1,
             "counterexample": {"query_id": "query-2", "observation": obs(3, 3)},
             "specification_identity": identity2,
             "candidate_checks": [{"candidate_id": candidates[0], "outcome": "consistent"},
                                  {"candidate_id": candidates[1], "outcome": "eliminated"}]},
        ]
        final = [{"candidate_id": candidates[0], "outcome": "consistent"},
                 {"candidate_id": candidates[1], "outcome": "eliminated"}]
        return initial, revisions, candidates, identity2, final

    def verify(self, args=None):
        data = self.chain()
        if args:
            data = args(data)
        return verify_revision_chain(data[0], data[1], expected_candidate_ids=data[2],
                                     final_specification_identity=data[3],
                                     final_candidate_outcomes=data[4],
                                     identified_behavior_class_count=None)

    def test_valid_ordered_chain_is_deterministic_and_finite_only(self):
        first = self.verify()
        second = self.verify()
        self.assertEqual(first, second)
        self.assertEqual(first["revision_count"], 2)
        self.assertEqual(first["observation_count"], 4)
        self.assertEqual(first["scope"], "finite observations only")

    def test_rejects_missing_revision(self):
        def remove(data):
            data[1].pop()
            return data
        with self.assertRaises(SpecificationRevisionError):
            self.verify(remove)

    def test_rejects_reordered_revisions(self):
        def reorder(data):
            data[1].reverse()
            return data
        with self.assertRaises(SpecificationRevisionError):
            self.verify(reorder)

    def test_rejects_mismatched_predecessor_identity(self):
        def tamper(data):
            data[1][1]["predecessor_identity"] = "0" * 64
            return data
        with self.assertRaises(SpecificationRevisionError):
            self.verify(tamper)

    def test_rejects_altered_counterexample_and_next_identity(self):
        for field, value in (("counterexample", {"query_id": "query-1", "observation": obs(2, 9)}),
                             ("specification_identity", "f" * 64)):
            def tamper(data, field=field, value=value):
                data[1][0][field] = value
                return data
            with self.subTest(field=field), self.assertRaises(SpecificationRevisionError):
                self.verify(tamper)

    def test_rejects_missing_duplicated_or_misordered_candidate_checks(self):
        def tamper(data):
            data[1][0]["candidate_checks"] = [{"candidate_id": "candidate-a", "outcome": "consistent"}]
            return data
        with self.assertRaises(SpecificationRevisionError):
            self.verify(tamper)

    def test_unresolved_validation_never_certifies_identification(self):
        def tamper(data):
            data[1][0]["candidate_checks"][1]["outcome"] = "inconclusive"
            return data
        with self.assertRaises(IncompleteSpecificationRevision):
            self.verify(tamper)

    def test_query_id_and_input_must_be_new(self):
        def duplicate_id(data):
            data[1][1]["counterexample"]["query_id"] = "query-1"
            return data
        with self.assertRaises(SpecificationRevisionError):
            self.verify(duplicate_id)

    def test_non_single_remaining_class_is_incomplete(self):
        data = self.chain()
        with self.assertRaises(IncompleteSpecificationRevision):
            verify_revision_chain(data[0], data[1], expected_candidate_ids=data[2],
                                  final_specification_identity=data[3],
                                  final_candidate_outcomes=data[4],
                                  identified_behavior_class_count=2)

    def test_unverified_singleton_class_claim_is_also_incomplete(self):
        data = self.chain()
        with self.assertRaises(IncompleteSpecificationRevision):
            verify_revision_chain(data[0], data[1], expected_candidate_ids=data[2],
                                  final_specification_identity=data[3],
                                  final_candidate_outcomes=data[4],
                                  identified_behavior_class_count=1)

    def test_eliminated_candidate_cannot_be_resurrected(self):
        data = self.chain()
        data[1][0]["candidate_checks"][1]["outcome"] = "eliminated"
        data[1][1]["candidate_checks"][1]["outcome"] = "consistent"
        data[4][1]["outcome"] = "consistent"
        with self.assertRaises(SpecificationRevisionError):
            self.verify(lambda _: data)

    def test_final_outcomes_must_match_last_revision(self):
        data = self.chain()
        data[4][1]["outcome"] = "consistent"
        with self.assertRaises(SpecificationRevisionError):
            self.verify(lambda _: data)

    def test_all_eliminated_is_not_successful_identification(self):
        data = self.chain()
        for revision in data[1]:
            for item in revision["candidate_checks"]:
                item["outcome"] = "eliminated"
        for item in data[4]:
            item["outcome"] = "eliminated"
        with self.assertRaises(IncompleteSpecificationRevision):
            self.verify(lambda _: data)

    def test_rejects_invalid_observation_domains_and_dimensions(self):
        data = self.chain()
        data[0][0]["expected"]["value"] = True
        with self.assertRaises(SpecificationRevisionError):
            self.verify(lambda _: data)
        data = self.chain()
        data[1][0]["counterexample"]["observation"]["inputs"] = [2, 3]
        with self.assertRaises(SpecificationRevisionError):
            self.verify(lambda _: data)


if __name__ == "__main__":
    unittest.main()
