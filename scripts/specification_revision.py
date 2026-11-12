"""Canonical, fail-closed verification for append-only exact specifications.

This research helper authenticates finite observation revisions. It makes no
claim about equivalence outside the explicitly supplied rows.
"""
from __future__ import annotations

from dataclasses import dataclass
import re
from typing import Any, Iterable

I64_MIN = -(1 << 63)
I64_MAX = (1 << 63) - 1
SAFE_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_-]{0,127}\Z")


class SpecificationRevisionError(ValueError):
    """A revision chain is malformed or fails authentication."""


class IncompleteSpecificationRevision(SpecificationRevisionError):
    """The supplied chain includes unresolved validation work."""


@dataclass(frozen=True)
class Observation:
    inputs: tuple[int, ...]
    output_type: str
    output: int | bool

    @classmethod
    def from_mapping(cls, raw: object) -> "Observation":
        if not isinstance(raw, dict) or set(raw) != {"inputs", "expected"}:
            raise SpecificationRevisionError("observation must contain exactly inputs and expected")
        inputs = raw["inputs"]
        expected = raw["expected"]
        if not isinstance(inputs, list) or not inputs:
            raise SpecificationRevisionError("observation inputs must be a nonempty integer array")
        if any(type(value) is not int or not I64_MIN <= value <= I64_MAX for value in inputs):
            raise SpecificationRevisionError("observation input is not a checked Int64")
        if not isinstance(expected, dict) or set(expected) != {"type", "value"}:
            raise SpecificationRevisionError("expected value must have an explicit type and value")
        value_type = expected["type"]
        value = expected["value"]
        if value_type == "int64":
            if type(value) is not int or not I64_MIN <= value <= I64_MAX:
                raise SpecificationRevisionError("expected int64 is outside the checked domain")
        elif value_type == "bool":
            if type(value) is not bool:
                raise SpecificationRevisionError("expected bool has a non-Boolean payload")
        else:
            raise SpecificationRevisionError("unsupported expected output type")
        return cls(tuple(inputs), value_type, value)

    def canonical(self) -> str:
        # Match DiscreteSpecification::identity() byte-for-byte: each input
        # is followed by a comma, and Boolean tags are b1/b0.
        inputs = "".join(f"{value}," for value in self.inputs)
        output = f"i{self.output}" if self.output_type == "int64" else ("b1" if self.output else "b0")
        return f"[{inputs}]={output};"


def specification_identity(observations: Iterable[Observation]) -> str:
    rows = tuple(observations)
    if not rows:
        raise SpecificationRevisionError("a finite specification needs at least one observation")
    dimensions = len(rows[0].inputs)
    if any(len(row.inputs) != dimensions for row in rows):
        raise SpecificationRevisionError("observation input dimensions differ")
    if len({row.inputs for row in rows}) != len(rows):
        raise SpecificationRevisionError("duplicate input tuple in specification")
    return f"discrete-spec-v1;d={dimensions};rows={len(rows)};" + "".join(row.canonical() for row in rows)


def _require_id(value: object, field: str) -> str:
    if not isinstance(value, str) or SAFE_ID.fullmatch(value) is None:
        raise SpecificationRevisionError(f"{field} must be a safe stable identifier")
    return value


def verify_revision_chain(
    initial_observations: list[dict[str, Any]],
    revisions: list[dict[str, Any]],
    *,
    expected_candidate_ids: list[str],
    final_specification_identity: str,
    final_candidate_outcomes: list[dict[str, Any]],
    identified_behavior_class_count: int | None = None,
) -> dict[str, Any]:
    """Authenticate an ordered, append-only observation refinement chain.

    Each revision binds a unique counterexample/query ID and new input to the
    exact prior identity. The next identity is recomputed from observations;
    caller assertions are never accepted as certificates by themselves.
    Candidate check sets must be complete for every revision. Allowed outcomes
    are ``consistent`` and ``eliminated``; pending, invalid, resource-stopped,
    or unknown checks produce an explicit incomplete failure. This validates
    authenticated record structure only; it does not run candidate evaluation
    or independently establish finite-behavior class counts.
    """
    if not isinstance(initial_observations, list) or not initial_observations:
        raise SpecificationRevisionError("initial observations must be a nonempty ordered list")
    if not isinstance(revisions, list):
        raise SpecificationRevisionError("revisions must be an ordered list")
    if not isinstance(expected_candidate_ids, list) or not expected_candidate_ids:
        raise SpecificationRevisionError("candidate identity list must be nonempty")
    candidates = [_require_id(item, "candidate_id") for item in expected_candidate_ids]
    if len(set(candidates)) != len(candidates):
        raise SpecificationRevisionError("candidate IDs must be unique")
    observations = [Observation.from_mapping(item) for item in initial_observations]
    if len({row.inputs for row in observations}) != len(observations):
        raise SpecificationRevisionError("initial observations contain duplicate inputs")
    current = specification_identity(observations)
    initial_identity = current
    seen_revision_ids: set[str] = set()
    seen_query_ids: set[str] = set()
    query_order: list[str] = []
    candidate_states: dict[str, str] = {candidate: "consistent" for candidate in candidates}
    last_outcomes: list[dict[str, str]] | None = None
    for index, revision in enumerate(revisions, start=1):
        if not isinstance(revision, dict):
            raise SpecificationRevisionError(f"revision {index} is not an object")
        required = {"revision", "revision_id", "predecessor_identity", "counterexample", "specification_identity", "candidate_checks"}
        if set(revision) != required:
            raise SpecificationRevisionRevisionShapeError(index, required - set(revision), set(revision) - required)
        if type(revision["revision"]) is not int or revision["revision"] != index:
            raise SpecificationRevisionError("revision ordinal is missing, duplicated, or reordered")
        revision_id = _require_id(revision["revision_id"], "revision_id")
        if revision_id in seen_revision_ids:
            raise SpecificationRevisionError("duplicate revision identity")
        seen_revision_ids.add(revision_id)
        if revision["predecessor_identity"] != current:
            raise SpecificationRevisionError(f"revision {index} predecessor identity mismatch")
        counterexample = revision["counterexample"]
        if not isinstance(counterexample, dict) or set(counterexample) != {"query_id", "observation"}:
            raise SpecificationRevisionError(f"revision {index} counterexample is malformed")
        query_id = _require_id(counterexample["query_id"], "query_id")
        if query_id in seen_query_ids:
            raise SpecificationRevisionError("duplicate counterexample query identity")
        seen_query_ids.add(query_id)
        query_order.append(query_id)
        observation = Observation.from_mapping(counterexample["observation"])
        if len(observation.inputs) != len(observations[0].inputs):
            raise SpecificationRevisionError("counterexample input dimension mismatch")
        if observation.inputs in {row.inputs for row in observations}:
            raise SpecificationRevisionError("counterexample does not add a new input")
        next_observations = [*observations, observation]
        next_identity = specification_identity(next_observations)
        if revision["specification_identity"] != next_identity:
            raise SpecificationRevisionError(f"revision {index} specification identity mismatch")
        checks = revision["candidate_checks"]
        if not isinstance(checks, list) or [item.get("candidate_id") if isinstance(item, dict) else None for item in checks] != candidates:
            raise SpecificationRevisionError(f"revision {index} candidate checks are missing or reordered")
        for check in checks:
            if set(check) != {"candidate_id", "outcome"}:
                raise SpecificationRevisionError(f"revision {index} candidate check is malformed")
            if check["outcome"] not in {"consistent", "eliminated"}:
                raise IncompleteSpecificationRevision(f"revision {index} contains unresolved candidate validation")
            candidate_id = check["candidate_id"]
            if candidate_states[candidate_id] == "eliminated" and check["outcome"] != "eliminated":
                raise SpecificationRevisionError("an eliminated candidate cannot be resurrected")
            candidate_states[candidate_id] = check["outcome"]
        last_outcomes = checks
        observations = next_observations
        current = next_identity

    if final_specification_identity != current:
        raise SpecificationRevisionError("final specification identity does not match authenticated chain")
    if not isinstance(final_candidate_outcomes, list):
        raise SpecificationRevisionError("final candidate outcomes must be an ordered list")
    if [item.get("candidate_id") if isinstance(item, dict) else None for item in final_candidate_outcomes] != candidates:
        raise SpecificationRevisionError("final candidate outcomes are missing or reordered")
    for item in final_candidate_outcomes:
        if set(item) != {"candidate_id", "outcome"}:
            raise SpecificationRevisionError("final candidate outcome is malformed")
        if item["outcome"] not in {"consistent", "eliminated"}:
            raise IncompleteSpecificationRevision("final candidate validation is unresolved")
    if last_outcomes is not None and final_candidate_outcomes != last_outcomes:
        raise SpecificationRevisionError("final candidate outcomes differ from the last authenticated revision")
    if not any(item["outcome"] == "consistent" for item in final_candidate_outcomes):
        raise IncompleteSpecificationRevision("all candidates were eliminated")
    if identified_behavior_class_count is not None:
        if type(identified_behavior_class_count) is not int or identified_behavior_class_count < 0:
            raise SpecificationRevisionError("behavior class count must be a nonnegative integer")
        raise IncompleteSpecificationRevision(
            "behavior class counts require independently evaluated signatures; this helper only validates chain structure")
    return {"initial_identity": initial_identity, "final_identity": current,
            "observation_count": len(observations), "revision_count": len(revisions),
            "query_ids": tuple(query_order), "candidate_ids": tuple(candidates),
            "scope": "finite observations only",
            "structural_validation_only": True,
            "mathematical_candidate_evaluation_performed": False}


class SpecificationRevisionRevisionShapeError(SpecificationRevisionError):
    def __init__(self, ordinal: int, missing: set[str], extra: set[str]):
        super().__init__(f"revision {ordinal} fields invalid; missing={sorted(missing)}, extra={sorted(extra)}")
