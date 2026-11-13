#!/usr/bin/env python3
"""Fail-closed, persistent execution gate for external research solvers.

This tool is research-only. A protocol directory is immutable after freeze;
each attempted solver process consumes one atomically reserved query slot.
"""

from __future__ import annotations

import argparse
import copy
from contextlib import ExitStack, contextmanager
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import resource
import re
import signal
import subprocess
import sys
import selectors
import threading
import tempfile
import time
import uuid

# The admission tool is sibling research tooling, not a library dependency.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from scientific_pipeline_preflight import PreflightError, verify_admission
from bounded_process import run_bounded_process
from research_pipeline import PipelineDescriptorError, validate_descriptor as validate_pipeline_descriptor, expand_argv as expand_pipeline_argv


REQUIRED_LIMITS = {
    "max_solver_invocations",
    "max_query_wall_seconds",
    "max_query_cpu_seconds",
    "max_total_wall_seconds",
    "max_total_cpu_seconds",
    "max_address_space_bytes",
    "max_query_bytes",
    "max_output_bytes",
}
MAX_ATTESTATION_RECEIPT_BYTES = 64 * 1024
MAX_ATTESTATION_RESULT_BYTES = 16 * 1024 * 1024


PROGRAM_LEDGER_ROOT = Path(
    os.environ.get(
        "GENETIK_RESEARCH_LEDGER_ROOT",
        "~/.local/state/genetik/research-program-ledgers",
    )
).expanduser().resolve()

# This ledger is deliberately separate from the historical solver-call ledger.
PROCESS_LEDGER_ROOT = Path(
    os.environ.get(
        "GENETIK_RESEARCH_PROCESS_LEDGER_ROOT",
        "~/.local/state/genetik/research-process-ledgers",
    )
).expanduser().resolve()

# The per-invocation isolated supervisor has a hard wall deadline of target
# allowance plus three seconds. Reserve the same worst-case CPU/wall envelope
# during protocol admission so orchestration overhead cannot overrun a budget.
BOUNDED_SUPERVISOR_CPU_ALLOWANCE_SECONDS = 3.0
BOUNDED_SUPERVISOR_WALL_ALLOWANCE_SECONDS = 3.0

_ACTIVE_COORDINATOR_GUARD: dict | None = None


def _validate_program_budget(value: object) -> dict:
    if not isinstance(value, dict):
        raise GateError("a persistent program-wide solver ledger is required")
    required = {
        "ledger_dir", "program_id", "historical_invocations", "historical_limit", "historical_overrun", "max_lifetime_invocations",
        "max_new_cpu_seconds", "max_new_wall_seconds", "max_query_cpu_seconds",
        "max_query_wall_seconds", "max_address_space_bytes", "max_query_bytes",
        "max_output_bytes", "historical_evidence",
    }
    if not required.issubset(value):
        raise GateError("program_budget is incomplete")
    for field in ("historical_invocations", "max_lifetime_invocations"):
        number = value[field]
        if isinstance(number, bool) or not isinstance(number, int) or number < 0:
            raise GateError(f"program_budget.{field} must be a nonnegative integer")
    if value["max_lifetime_invocations"] < value["historical_invocations"]:
        raise GateError("program-wide lifetime ceiling is below recorded historical use")
    if (
        isinstance(value["historical_limit"], bool)
        or not isinstance(value["historical_limit"], int)
        or isinstance(value["historical_overrun"], bool)
        or not isinstance(value["historical_overrun"], int)
        or value["historical_invocations"] != value["historical_limit"] + value["historical_overrun"]
    ):
        raise GateError("historical invocation ceiling and overrun must be preserved exactly")
    for field in ("max_new_cpu_seconds", "max_new_wall_seconds", "max_query_cpu_seconds", "max_query_wall_seconds"):
        if isinstance(value[field], bool) or not isinstance(value[field], (int, float)) or not math.isfinite(value[field]) or value[field] <= 0:
            raise GateError(f"program_budget.{field} must be finite and positive")
    for field in ("max_address_space_bytes", "max_query_bytes", "max_output_bytes"):
        if isinstance(value[field], bool) or not isinstance(value[field], int) or value[field] < 1:
            raise GateError(f"program_budget.{field} must be a positive integer")
    if not isinstance(value["program_id"], str) or re.fullmatch(r"[A-Za-z0-9_-]+", value["program_id"]) is None:
        raise GateError("program_budget.program_id must be a safe stable identifier")
    if not isinstance(value["ledger_dir"], str) or not Path(value["ledger_dir"]).is_absolute():
        raise GateError("program_budget.ledger_dir must be an absolute path")
    expected_directory = (PROGRAM_LEDGER_ROOT / value["program_id"]).resolve()
    if Path(value["ledger_dir"]).resolve() != expected_directory:
        raise GateError("program ledger path must be the canonical lifetime path for this program_id")
    evidence = value["historical_evidence"]
    if not isinstance(evidence, dict) or not isinstance(evidence.get("path"), str):
        raise GateError("historical query ledger evidence is required")
    path = Path(evidence["path"]).resolve()
    if not path.is_file() or evidence.get("sha256") != sha256_file(path):
        raise GateError("historical query ledger evidence hash mismatch")
    historical = json.loads(path.read_text())
    if historical.get("query_invocations_counted") != value["historical_invocations"]:
        raise GateError("program-wide seed does not match retained historical query count")
    if value["historical_limit"] != historical.get("declared_query_ceiling") or value["historical_overrun"] != historical.get("ceiling_overrun"):
        raise GateError("program-wide seed does not preserve the retained historical ceiling/overrun")
    partial_walls = [
        call.get("wall_seconds")
        for call in historical.get("invocations", [])
        if isinstance(call, dict) and isinstance(call.get("wall_seconds"), (int, float)) and not isinstance(call.get("wall_seconds"), bool)
    ]
    partial_wall = sum(partial_walls) if partial_walls else None
    return {
        **value,
        "ledger_dir": str(Path(value["ledger_dir"]).resolve()),
        "historical_evidence": {**evidence, "path": str(path)},
        "historical_partial_wall_seconds": partial_wall,
    }


def _validate_execution_group(value: object, program_budget: dict) -> dict:
    if not isinstance(value, dict):
        raise GateError("execution_group must be an object")
    required = {"group_id", "ledger_dir", "max_invocations"}
    if not required.issubset(value):
        raise GateError("execution_group is incomplete")
    group_id = value["group_id"]
    if not isinstance(group_id, str) or re.fullmatch(r"[A-Za-z0-9_-]+", group_id) is None:
        raise GateError("execution_group.group_id must be a safe stable identifier")
    maximum = value["max_invocations"]
    if isinstance(maximum, bool) or not isinstance(maximum, int) or maximum < 0:
        raise GateError("execution_group.max_invocations must be a nonnegative integer")
    remaining_program_invocations = (
        program_budget["max_lifetime_invocations"] - program_budget["historical_invocations"]
    )
    if maximum > remaining_program_invocations:
        raise GateError("execution-group ceiling exceeds program lifetime remaining allowance")
    ledger_dir = value["ledger_dir"]
    if not isinstance(ledger_dir, str) or not Path(ledger_dir).is_absolute():
        raise GateError("execution_group.ledger_dir must be an absolute path")
    expected = (
        Path(program_budget["ledger_dir"]) / "execution-groups" / group_id
    ).resolve()
    if Path(ledger_dir).resolve() != expected:
        raise GateError("execution group path must be canonical under its program ledger")
    result = {"group_id": group_id, "ledger_dir": str(expected), "max_invocations": maximum}
    attestation_required = value.get("attestation_required", False)
    if not isinstance(attestation_required, bool):
        raise GateError("execution_group.attestation_required must be Boolean")
    has_stages = "stages" in value or "max_syntax_retries" in value or "attestation_required" in value
    checker_limits = value.get("checker_limits")
    if attestation_required:
        required_checker_limits = {
            "max_attempts", "max_checker_wall_seconds", "max_checker_cpu_seconds",
            "max_total_wall_seconds", "max_total_cpu_seconds",
            "max_address_space_bytes", "max_process_group_rss_bytes", "max_output_bytes",
        }
        if not isinstance(checker_limits, dict) or not required_checker_limits.issubset(checker_limits):
            raise GateError("attested execution groups require complete checker_limits")
        for name in ("max_attempts", "max_address_space_bytes", "max_process_group_rss_bytes", "max_output_bytes"):
            number = checker_limits[name]
            if isinstance(number, bool) or not isinstance(number, int) or number <= 0:
                raise GateError(f"checker limit {name} must be a positive integer")
        for name in ("max_checker_wall_seconds", "max_checker_cpu_seconds", "max_total_wall_seconds", "max_total_cpu_seconds"):
            number = checker_limits[name]
            if isinstance(number, bool) or not isinstance(number, (int, float)) or not math.isfinite(number) or number <= 0:
                raise GateError(f"checker limit {name} must be positive and finite")
        if checker_limits["max_checker_wall_seconds"] > checker_limits["max_total_wall_seconds"] or checker_limits["max_checker_cpu_seconds"] > checker_limits["max_total_cpu_seconds"]:
            raise GateError("checker aggregate limits must admit one complete checker attempt")
        result["checker_limits"] = {name: checker_limits[name] for name in sorted(required_checker_limits)}
    elif checker_limits is not None:
        raise GateError("checker_limits require attestation_required")
    if has_stages:
        stages = value.get("stages")
        retry_limit = value.get("max_syntax_retries")
        if not isinstance(stages, list) or not stages:
            raise GateError("staged execution_group requires a nonempty ordered stages array")
        if isinstance(retry_limit, bool) or not isinstance(retry_limit, int) or retry_limit != 1:
            raise GateError("staged execution_group requires max_syntax_retries exactly 1")
        valid_statuses = {"successful", "unsatisfiable"}
        stage_ids = set()
        normalized_stages = []
        for stage in stages:
            if not isinstance(stage, dict) or not {"stage_id", "expected_status", "retryable"}.issubset(stage):
                raise GateError("each execution stage requires stage_id, expected_status and retryable")
            stage_id = stage["stage_id"]
            if not isinstance(stage_id, str) or re.fullmatch(r"[A-Za-z0-9_-]+", stage_id) is None or stage_id in stage_ids:
                raise GateError("execution stage IDs must be unique safe identifiers")
            if not isinstance(stage["expected_status"], str) or stage["expected_status"] not in valid_statuses:
                raise GateError("execution stage expected_status must be successful or unsatisfiable")
            if not isinstance(stage["retryable"], bool):
                raise GateError("execution stage retryable must be Boolean")
            if stage_id.startswith("replay-") and stage["retryable"]:
                raise GateError("replay stages cannot be retryable")
            checker_sha256 = stage.get("checker_sha256")
            if checker_sha256 is not None and (
                not isinstance(checker_sha256, str)
                or re.fullmatch(r"[0-9a-f]{64}", checker_sha256) is None
            ):
                raise GateError("stage checker_sha256 must be a lowercase SHA256")
            if attestation_required and stage["expected_status"] == "successful" and checker_sha256 is None:
                raise GateError("attested successful stages require a pinned checker_sha256")
            stage_ids.add(stage_id)
            normalized_stage = {
                "stage_id": stage_id,
                "expected_status": stage["expected_status"],
                "retryable": stage["retryable"],
            }
            if checker_sha256 is not None:
                normalized_stage["checker_sha256"] = checker_sha256
            normalized_stages.append(normalized_stage)
        minimum_calls = len(normalized_stages) + int(any(stage["retryable"] for stage in normalized_stages))
        if maximum < minimum_calls:
            raise GateError("execution-group cap cannot cover every stage and its single syntax retry")
        result["stages"] = normalized_stages
        result["max_syntax_retries"] = retry_limit
        result["attestation_required"] = attestation_required
    elif attestation_required:
        raise GateError("attestation_required needs an ordered staged execution group")
    return result


class GateError(RuntimeError):
    pass


class CoordinatorResourceExceeded(GateError):
    pass


def _require_sha256(value: object, field: str) -> None:
    if not isinstance(value, str) or re.fullmatch(r"[0-9a-f]{64}", value) is None:
        raise GateError(f"{field} must be a lowercase SHA256")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_exclusive(path: Path, data: bytes, mode: int = 0o444) -> None:
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, mode)
    with os.fdopen(fd, "wb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    _fsync_directory(path.parent)


def _fsync_directory(path: Path) -> None:
    fd = os.open(path, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def canonical_json(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, indent=2) + "\n").encode()


def compact_json(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


PROGRAM_SAFE_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_-]{0,127}\Z")
CONTINUATION_SLOT_COUNTS = {
    "primary_preflight": 4,
    "recovery_preflight": 4,
    "campaign": 38,
    "final_scoring": 3,
}
CONTINUATION_ALLOWANCE = {"invocations": 49, "cpu_seconds": 333.0, "wall_seconds": 426.0}
CONTINUATION_COORDINATOR_ALLOWANCE = {"cpu_seconds": 11.0, "exclusive_wall_seconds": 11.0}
CONTINUATION_PHASE_IDS = ("primary_preflight", "recovery_preflight", "campaign_orchestration", "final_scoring_orchestration")
CONTINUATION_MEMORY_BYTES = 512 * 1024 * 1024
CONTINUATION_OUTPUT_BYTES = 1024 * 1024
HISTORICAL_STAGE_WALL_SECONDS = {
    "v2": 1.075066692661494,
    "v3": 1.0878780307248235,
    "v4": 1.1183677860535681,
    "v5": 1.0839218073524535,
}
HISTORICAL_STAGE_RECEIPT_SHA256 = {
    "v2": "8d0f868433735f8188135b7bc7f493332622e66caa8d3ca1612921a2f0049263",
    "v3": "6c2bb1b4106b2baa674c925f034a20f7740d44512ea07d1051bfbc258a2db581",
    "v4": "ac8e835f2b9fdc1dcbf565737c8b1df83c4be3a8e9c9c70a73b9b5727f776074",
    "v5": "6d85cee6426bde045ffcd3dd88addb6493cbb6eac74fd77ea66d21f3f26fb540",
}
PROGRAM_LIMIT_FIELDS = {
    "max_invocations", "max_total_cpu_seconds", "max_total_wall_seconds",
    "max_cpu_seconds", "max_wall_seconds", "max_address_space_bytes",
    "max_stdin_bytes", "max_output_bytes", "max_file_bytes",
}


def _safe_program_id(value: object, field: str) -> str:
    if not isinstance(value, str) or PROGRAM_SAFE_ID.fullmatch(value) is None:
        raise GateError(f"{field} must be a safe identifier")
    return value


def _validate_controller_file_phase(phase_id: object, slot: str) -> None:
    if phase_id not in {"preflight", "recovery"}:
        raise GateError("controller files are limited to primary/recovery preflight inputs")
    if slot == "assessment_bundle" and phase_id not in {"preflight", "recovery"}:
        raise GateError("synthetic assessment input is limited to preflight qualification")


def validate_run_output_list_binding(binding: object, slot: str, target_run_id: str,
                                     prior_runs: dict[str, dict]) -> list[dict]:
    """Validate an ordered list fan-in of previously admitted output slots."""
    if slot != "candidate_outputs" or not isinstance(binding, list) or not binding:
        raise GateError("list-valued run_output binding is supported only for nonempty candidate_outputs")
    result = []
    seen = set()
    for reference in binding:
        if (not isinstance(reference, dict) or set(reference) != {"kind", "run_id", "slot"}
                or reference.get("kind") != "run_output"):
            raise GateError("candidate_outputs list entries must be exact run_output references")
        source_id, source_slot = reference["run_id"], reference["slot"]
        if (not isinstance(source_id, str) or PROGRAM_SAFE_ID.fullmatch(source_id) is None
                or not isinstance(source_slot, str) or not source_slot):
            raise GateError("candidate_outputs list source identity or slot is malformed")
        key = (source_id, source_slot)
        if key in seen:
            raise GateError("candidate_outputs list repeats a source run/slot")
        seen.add(key)
        source = prior_runs.get(source_id)
        if source is None or source_id == target_run_id:
            raise GateError("candidate_outputs list source must be an earlier admitted run")
        source_stage = source.get("stage_id")
        # The producer stage is checked against the frozen descriptor by the caller.
        if not isinstance(source_stage, str) or not isinstance(source_slot, str):
            raise GateError("candidate_outputs list source has an invalid stage/slot")
        result.append(reference)
    return result


def validate_continuation_phase_plan(phase_plan: object, runs: list[dict], slots: dict) -> list[str]:
    """Validate the amended allowlist order without imposing the legacy cursor."""
    group_bindings = (("primary_preflight", "primary_preflight", "preflight"),
                      ("recovery_preflight", "recovery_preflight", "recovery"),
                      ("campaign", "campaign", "campaign"),
                      ("final_scoring", "final_scoring", "scoring"))
    if (not isinstance(phase_plan, list)
            or [phase.get("phase_id") for phase in phase_plan if isinstance(phase, dict)]
            != [phase_id for phase_id, _, _ in group_bindings]
            or len(phase_plan) != len(group_bindings)):
        raise GateError("amended phase_plan must declare the four continuation slot groups")
    run_by_id = {run.get("run_id"): run for run in runs}
    flattened = []
    for phase, (phase_id, slot_name, run_phase_id) in zip(phase_plan, group_bindings):
        ids = phase.get("run_ids")
        if (not isinstance(ids, list) or not all(isinstance(value, str) for value in ids)
                or len(ids) != len(set(ids)) or ids != slots.get(slot_name)):
            raise GateError(f"amended phase {phase_id} must match its exact ordered authorization slots")
        if any(run_id not in run_by_id or run_by_id[run_id].get("phase_id") != run_phase_id for run_id in ids):
            raise GateError(f"amended phase {phase_id} contains a run with the wrong legacy phase label")
        flattened.extend(ids)
    if flattened != [run.get("run_id") for run in runs]:
        raise GateError("amended phase_plan must cover the exact allowlist once in frozen slot order")
    return flattened


def validate_continuation_history(history: object) -> dict:
    """Validate exact known/unknown imported history without opening its files."""
    history_keys = {"source_protocol_artifact_id", "evidence_manifest_artifact_id", "prior_declared_invocation_cap",
                    "known_governed_preflight", "unmeasured_pre_freeze_executions", "prior_v1_activity"}
    if not isinstance(history, dict) or set(history) != history_keys:
        raise GateError("continuation historical_import fields are incomplete or unexpected")
    if history["prior_declared_invocation_cap"] != 45 or isinstance(history["prior_declared_invocation_cap"], bool):
        raise GateError("historical 45-invocation authorization must be preserved exactly")
    known = history["known_governed_preflight"]
    known_keys = {"count", "status", "cpu_seconds", "stage_wall_seconds", "stage_wall_scope", "receipt_sha256_by_version"}
    if not isinstance(known, dict) or set(known) != known_keys:
        raise GateError("known governed history must distinguish measured stage wall from unknown CPU/coordinator work")
    stage_wall = known["stage_wall_seconds"]
    if (known["count"] != 16 or isinstance(known["count"], bool) or known["status"] != "successful"
            or known["cpu_seconds"] is not None
            or isinstance(stage_wall, bool) or not isinstance(stage_wall, (int, float))
            or not math.isfinite(stage_wall) or stage_wall != sum(HISTORICAL_STAGE_WALL_SECONDS.values())
            or known["stage_wall_scope"] != "sum_of_16_bounded_stage_wall_records_only; coordinator_total_unknown"
            or known["receipt_sha256_by_version"] != HISTORICAL_STAGE_RECEIPT_SHA256):
        raise GateError("historical preflight evidence must preserve exact stage receipts and explicitly unknown CPU/coordinator totals")
    unknown = history["unmeasured_pre_freeze_executions"]
    if (not isinstance(unknown, dict) or set(unknown) != {"count", "cpu_seconds", "wall_seconds"}
            or unknown["count"] != 2 or isinstance(unknown["count"], bool)
            or unknown["cpu_seconds"] is not None or unknown["wall_seconds"] is not None):
        raise GateError("two unmeasured pre-freeze executions must retain unknown resource values")
    v1 = history["prior_v1_activity"]
    if (not isinstance(v1, dict) or set(v1) != {"invocations", "cpu_seconds", "wall_seconds"}
            or any(v1[field] is not None for field in v1)):
        raise GateError("prior v1 count and resource totals must remain explicitly unknown")
    return history


def validate_bounded_program_continuation(
    amendment: object,
    runs: list[dict],
    artifacts_by_id: dict[str, dict],
    limits: dict,
    program_budget: dict,
    pipeline_descriptor: dict | None,
    *,
    allow_legacy_predecessor: bool = False,
) -> dict:
    """Validate the narrowly scoped, additive finite-identifiability authorization.

    This is an offline contract check: it reads no scientific rows and launches
    no process. Historical unknowns remain null and are not folded into the
    prospective counters.
    """
    required_v2 = {"schema", "amendment_id", "authorization_artifact_id", "coordinator_entrypoint_artifact_id",
                "preflight_coordinator_artifact_id", "coordinator_supervisor_artifact_id", "historical_import",
                "prospective_allowance", "coordinator_allowance", "overall_deadline_seconds",
                "coordinator_phase_ids", "slots", "interactive_session"}
    required_v1 = required_v2 - {"coordinator_supervisor_artifact_id"}
    legacy = (isinstance(amendment, dict) and allow_legacy_predecessor
              and amendment.get("schema") == 1 and set(amendment) == required_v1)
    if not isinstance(amendment, dict) or (not legacy and
            (set(amendment) != required_v2 or amendment.get("schema") != 2)):
        raise GateError("bounded-program continuation amendment schema is incomplete")
    _safe_program_id(amendment["amendment_id"], "continuation amendment_id")

    def require_artifact(artifact_id: object, role: str, field: str) -> dict:
        artifact_id = _safe_program_id(artifact_id, field)
        artifact = artifacts_by_id.get(artifact_id)
        if artifact is None or artifact.get("role") != role:
            raise GateError(f"continuation {field} must reference a pinned {role} artifact")
        return artifact

    authorization_artifact = require_artifact(amendment["authorization_artifact_id"], "authorization-amendment", "authorization_artifact_id")
    coordinator_entrypoint_artifact = require_artifact(amendment["coordinator_entrypoint_artifact_id"],
                                                       "coordinator-entrypoint", "coordinator_entrypoint_artifact_id")
    preflight_coordinator_artifact = require_artifact(amendment["preflight_coordinator_artifact_id"],
                                                      "coordinator-entrypoint", "preflight_coordinator_artifact_id")
    if (not allow_legacy_predecessor and
            Path(coordinator_entrypoint_artifact["path"]).resolve() != Path(__file__).resolve().with_name("execute_continuation_phase.py")):
        raise GateError("continuation coordinator entrypoint must pin execute_continuation_phase.py")
    if (not allow_legacy_predecessor and
            Path(preflight_coordinator_artifact["path"]).resolve() != Path(__file__).resolve().with_name("scientific_pipeline_preflight.py")):
        raise GateError("preflight coordinator entrypoint must pin scientific_pipeline_preflight.py")
    coordinator_supervisor_artifact = None
    if not legacy:
        coordinator_supervisor_artifact = require_artifact(amendment["coordinator_supervisor_artifact_id"],
                                                          "coordinator-entrypoint", "coordinator_supervisor_artifact_id")
    if (coordinator_supervisor_artifact is not None and not allow_legacy_predecessor and
            Path(coordinator_supervisor_artifact["path"]).resolve() != Path(__file__).resolve().with_name("coordinator_supervisor.py")):
        raise GateError("coordinator supervisor artifact must pin coordinator_supervisor.py")
    history = validate_continuation_history(amendment["historical_import"])
    source_protocol_artifact = require_artifact(history["source_protocol_artifact_id"], "historical-protocol", "source_protocol_artifact_id")
    history_manifest_artifact = require_artifact(history["evidence_manifest_artifact_id"], "historical-evidence-manifest", "evidence_manifest_artifact_id")

    allowance = amendment["prospective_allowance"]
    if not isinstance(allowance, dict) or set(allowance) != set(CONTINUATION_ALLOWANCE):
        raise GateError("prospective continuation allowance is incomplete")
    if allowance != CONTINUATION_ALLOWANCE:
        raise GateError("prospective continuation allowance exceeds or differs from authorization")
    if amendment["coordinator_allowance"] != CONTINUATION_COORDINATOR_ALLOWANCE:
        raise GateError("coordinator CPU/exclusive-wall allowance differs from the frozen authorization")
    if amendment["overall_deadline_seconds"] != CONTINUATION_ALLOWANCE["wall_seconds"]:
        raise GateError("overall continuation deadline differs from the frozen wall-time authorization")
    if amendment["coordinator_phase_ids"] != list(CONTINUATION_PHASE_IDS):
        raise GateError("coordinator phase receipt plan differs from the frozen continuation plan")
    if (program_budget.get("max_new_invocations") != allowance["invocations"]
            or program_budget.get("max_new_cpu_seconds") != allowance["cpu_seconds"]
            or program_budget.get("max_new_wall_seconds") != allowance["wall_seconds"]):
        raise GateError("program budget does not match additive prospective authorization")
    if (limits.get("max_invocations") != allowance["invocations"]
            or limits.get("max_total_cpu_seconds") != allowance["cpu_seconds"]
            or limits.get("max_total_wall_seconds") != allowance["wall_seconds"]
            or limits.get("max_address_space_bytes") != CONTINUATION_MEMORY_BYTES
            or limits.get("max_output_bytes") != CONTINUATION_OUTPUT_BYTES
            or limits.get("max_file_bytes") != CONTINUATION_OUTPUT_BYTES):
        raise GateError("continuation limits differ from the authorized invocation/resource caps")

    slots = amendment["slots"]
    if not isinstance(slots, dict) or set(slots) != set(CONTINUATION_SLOT_COUNTS):
        raise GateError("continuation slots must define primary, recovery, campaign and final-scoring arrays")
    flattened: list[str] = []
    expected_phase = {"primary_preflight": "preflight", "recovery_preflight": "recovery",
                      "campaign": "campaign", "final_scoring": "scoring"}
    run_by_id = {run["run_id"]: run for run in runs}
    if len(run_by_id) != len(runs):
        raise GateError("continuation run allowlist contains duplicate run IDs")
    for name, count in CONTINUATION_SLOT_COUNTS.items():
        values = slots[name]
        if not isinstance(values, list) or len(values) != count or not all(isinstance(item, str) for item in values):
            raise GateError(f"continuation {name} must contain exactly {count} ordered run IDs")
        if len(set(values)) != count:
            raise GateError(f"continuation {name} repeats a run ID")
        for run_id in values:
            run = run_by_id.get(run_id)
            if run is None or run.get("phase_id") != expected_phase[name]:
                raise GateError(f"continuation slot {run_id} is missing or has the wrong phase")
            expected_cpu, expected_wall = ((2, 3) if name == "campaign" else (9, 14))
            if run.get("max_cpu_seconds") != expected_cpu or run.get("max_wall_seconds") != expected_wall:
                raise GateError(f"continuation slot {run_id} per-run budget differs from its frozen phase allocation")
        flattened.extend(values)
    if len(set(flattened)) != len(flattened) or set(flattened) != set(run_by_id) or len(flattened) != 49:
        raise GateError("continuation slots must partition the complete 49-run allowlist exactly once")

    session = amendment["interactive_session"]
    session_keys = {"python_executable", "python_sha256", "helper_path", "helper_sha256", "peer_path", "peer_sha256",
                    "runner_runtime_files", "peer_runtime_files", "max_frame_bytes", "max_terminal_frame_bytes", "max_protocol_bytes",
                    "max_requests", "max_cpu_seconds", "max_memory_bytes", "terminal_type"}
    if not isinstance(session, dict) or set(session) != session_keys:
        raise GateError("interactive-session manifest is incomplete or contains unsupported fields")
    for path_field, hash_field in (("python_executable", "python_sha256"), ("helper_path", "helper_sha256"),
                                   ("peer_path", "peer_sha256")):
        path = session[path_field]
        _require_sha256(session[hash_field], f"interactive_session.{hash_field}")
        if not isinstance(path, str) or not Path(path).is_absolute():
            raise GateError(f"interactive_session.{path_field} must be an absolute pinned path")
        if not Path(path).is_file() or sha256_file(Path(path)) != session[hash_field]:
            raise GateError(f"interactive_session.{path_field} is missing or hash-mismatched")
    for file_list_name in ("runner_runtime_files", "peer_runtime_files"):
        files = session[file_list_name]
        if not isinstance(files, list):
            raise GateError(f"interactive_session.{file_list_name} must be an array")
        paths = set()
        for entry in files:
            if not isinstance(entry, dict) or set(entry) != {"path", "sha256"} or not isinstance(entry["path"], str):
                raise GateError(f"interactive_session.{file_list_name} entries require path and SHA256")
            _require_sha256(entry["sha256"], f"interactive_session.{file_list_name}.sha256")
            path = Path(entry["path"]).resolve()
            if path in paths:
                raise GateError(f"interactive_session.{file_list_name} contains duplicate paths")
            paths.add(path)
            if not path.is_file() or sha256_file(path) != entry["sha256"]:
                raise GateError(f"interactive_session.{file_list_name} contains a missing or hash-mismatched file")
    exact_session_limits = {"max_frame_bytes": 4096, "max_terminal_frame_bytes": CONTINUATION_OUTPUT_BYTES,
                            "max_protocol_bytes": CONTINUATION_OUTPUT_BYTES,
                            "max_requests": 8, "max_cpu_seconds": 2,
                            "max_memory_bytes": CONTINUATION_MEMORY_BYTES, "terminal_type": "final"}
    if any(session.get(key) != value for key, value in exact_session_limits.items()):
        raise GateError("interactive-session limits differ from the frozen continuation contract")
    if session["max_terminal_frame_bytes"] < session["max_frame_bytes"]:
        raise GateError("terminal frame limit cannot be below the ordinary frame limit")
    if pipeline_descriptor is None:
        raise GateError("continuation interactive session requires a pinned pipeline descriptor")
    artifacts_by_path = {str(Path(artifact["path"]).resolve()): artifact for artifact in artifacts_by_id.values()}
    pinned_paths = {str(Path(path).resolve()) for path in (
        session["python_executable"], session["helper_path"], session["peer_path"])}
    pinned_paths.update(str(Path(item["path"]).resolve()) for name in ("runner_runtime_files", "peer_runtime_files")
                        for item in session[name])
    for raw_path in pinned_paths:
        artifact = artifacts_by_path.get(raw_path)
        if artifact is None or artifact.get("role") not in {"pipeline-executable", "pipeline-dependency"}:
            raise GateError("interactive session files must all be bound as pipeline code/dependency artifacts")
    runner_dependencies = pipeline_descriptor["stages"]["runner"]["dependencies"]
    runner_dependency_paths = {str(Path(artifacts_by_id[artifact_id]["path"]).resolve())
                               for artifact_id in runner_dependencies}
    if not pinned_paths.issubset(runner_dependency_paths | {
            str(Path(artifacts_by_id[stage_artifact]["path"]).resolve())
            for stage_artifact in (pipeline_descriptor["stages"]["runner"]["entrypoint_artifact"],
                                   pipeline_descriptor["stages"]["runner"]["executable_artifact"])}):
        raise GateError("runner pipeline identity does not pin every interactive executable and runtime")
    return {"amendment_id": amendment["amendment_id"], "authorization_sha256": authorization_artifact["sha256"],
            "slots": slots, "run_by_id": run_by_id,
            "history": history, "allowance": allowance,
            "coordinator_allowance": amendment["coordinator_allowance"],
            "overall_deadline_seconds": amendment["overall_deadline_seconds"],
            "coordinator_phase_ids": amendment["coordinator_phase_ids"],
            "coordinator_entrypoint": {"path": coordinator_entrypoint_artifact["path"],
                                        "sha256": coordinator_entrypoint_artifact["sha256"]},
            "preflight_coordinator_entrypoint": {"path": preflight_coordinator_artifact["path"],
                                                   "sha256": preflight_coordinator_artifact["sha256"]},
            "coordinator_supervisor_entrypoint": (
                {"path": coordinator_supervisor_artifact["path"],
                 "sha256": coordinator_supervisor_artifact["sha256"]}
                if coordinator_supervisor_artifact is not None else None),
            "history_artifact_sha256": {
                "source_protocol": source_protocol_artifact["sha256"],
                "evidence_manifest": history_manifest_artifact["sha256"],
            },
            "interactive_session": session}


def continuation_next_action(plan: dict, events: list[dict]) -> dict:
    """Pure state transition used by run admission; no files or processes touched."""
    if any(event.get("event") in {"admission_refused", "program_terminal"} for event in events):
        return {"state": "terminal_failure", "next_run_id": None, "retire_run_ids": []}
    slots = plan["slots"]
    reserved = {event.get("run_id") for event in events if event.get("event") == "reserved"}
    completed = {event.get("run_id"): event for event in events if event.get("event") == "completed"}
    qualification_events = [event for event in events if event.get("event") == "qualified"]
    if len(qualification_events) > 1:
        raise GateError("continuation qualification is one-shot")
    qualified_path = qualification_events[-1].get("qualification_path", "primary") if qualification_events else None
    failed = any(event.get("event") == "preflight_rejected" for event in events)
    recovery_rejected = any(event.get("event") == "recovery_rejected" for event in events)
    retired = {event.get("run_id") for event in events if event.get("event") == "recovery_retired"}
    recovery_started = any(run_id in reserved for run_id in slots["recovery_preflight"])

    def next_in_order(ids: list[str]) -> str | None:
        for run_id in ids:
            if run_id not in reserved:
                return run_id
            result = completed.get(run_id)
            if result is None:
                raise GateError("continuation has an unresolved invocation")
            if result.get("status") != "completed":
                return None
        return None

    primary_process_failed = any(
        run_id in completed and completed[run_id].get("status") != "completed"
        for run_id in slots["primary_preflight"]
    )
    primary_failed = failed
    if primary_process_failed and not primary_failed:
        return {"state": "primary_rejection_required", "next_run_id": None, "retire_run_ids": []}
    if recovery_started or primary_failed:
        if qualified_path == "primary":
            raise GateError("qualified continuation cannot also enter recovery")
        if recovery_rejected:
            return {"state": "terminal_failure", "next_run_id": None, "retire_run_ids": []}
        recovery_process_failed = any(run_id in completed and completed[run_id].get("status") != "completed"
                                      for run_id in slots["recovery_preflight"])
        if recovery_process_failed and not recovery_rejected:
            return {"state": "recovery_rejection_required", "next_run_id": None, "retire_run_ids": []}
        if recovery_process_failed:
            return {"state": "terminal_failure", "next_run_id": None, "retire_run_ids": []}
        next_recovery = next_in_order(slots["recovery_preflight"])
        if next_recovery is not None:
            return {"state": "recovery", "next_run_id": next_recovery, "retire_run_ids": []}
        if all(run_id in completed and completed[run_id].get("status") == "completed"
               for run_id in slots["recovery_preflight"]):
            if qualified_path != "recovery":
                return {"state": "recovery_qualification_required", "next_run_id": None, "retire_run_ids": []}
        else:
            return {"state": "terminal_failure", "next_run_id": None, "retire_run_ids": []}
    else:
        next_primary = next_in_order(slots["primary_preflight"])
        if next_primary is not None:
            return {"state": "primary_preflight", "next_run_id": next_primary, "retire_run_ids": []}
        if qualified_path != "primary":
            return {"state": "primary_qualification_required", "next_run_id": None, "retire_run_ids": []}
        missing_retirements = [run_id for run_id in slots["recovery_preflight"] if run_id not in retired]
        if missing_retirements:
            return {"state": "retire_recovery", "next_run_id": None, "retire_run_ids": missing_retirements}

    for phase_name in ("campaign", "final_scoring"):
        next_run = next_in_order(slots[phase_name])
        if next_run is not None:
            return {"state": phase_name, "next_run_id": next_run, "retire_run_ids": []}
        if any(run_id not in completed or completed[run_id].get("status") != "completed"
               for run_id in slots[phase_name]):
            return {"state": "terminal_failure", "next_run_id": None, "retire_run_ids": []}
    return {"state": "complete", "next_run_id": None, "retire_run_ids": []}


def validate_continuation_events(plan: dict, events: list[dict]) -> dict:
    """Check ordered reservations, qualification and no-charge retirements."""
    prefix: list[dict] = []
    pending: str | None = None
    amendment_id = plan.get("amendment_id")
    program_starts = [event for event in events if event.get("event") == "program_started"]
    if len(program_starts) > 1:
        raise GateError("continuation overall deadline may be started only once")
    program_protocol_sha = program_starts[0].get("protocol_sha256") if program_starts else None
    program_starts = [event for event in events if event.get("event") == "program_started"]
    if len(program_starts) > 1:
        raise GateError("continuation overall deadline may be started only once")
    program_protocol_sha = program_starts[0].get("protocol_sha256") if program_starts else None
    active_phase: dict | None = None
    seen_phases: set[str] = set()
    closed_phase_status: dict[str, str] = {}
    closed_phase_category: dict[str, str | None] = {}
    terminal_seen = False
    for event in events:
        kind = event.get("event")
        if terminal_seen and kind != "program_terminal":
            raise GateError("continuation ledger contains events after terminal refusal/failure")
        if kind == "coordinator_phase_started":
            if active_phase is not None:
                raise GateError("coordinator phases may not overlap")
            phase_id = event.get("phase_id")
            action = continuation_next_action(plan, prefix)
            expected_phase = _phase_id_for_state(plan, action["state"])
            if (phase_id != expected_phase or phase_id in seen_phases
                    or event.get("amendment_id") != amendment_id
                    or event.get("protocol_sha256") is None
                    or program_protocol_sha is None
                    or event.get("protocol_sha256") != program_protocol_sha
                    or program_protocol_sha is None
                    or event.get("protocol_sha256") != program_protocol_sha
                    or not isinstance(event.get("token"), str)
                    or type(event.get("pid")) is not int
                    or type(event.get("pid_starttime_ticks")) is not int
                    or event.get("pid_starttime_ticks") <= 0
                    or type(event.get("start_monotonic_ns")) is not int
                    or not isinstance(event.get("start_cpu_seconds"), (int, float))
                    or not math.isfinite(float(event["start_cpu_seconds"]))):
                raise GateError("coordinator phase start is unauthorized, repeated, or malformed")
            active_phase = event
            seen_phases.add(phase_id)
        elif kind == "reserved":
            action = continuation_next_action(plan, prefix)
            if action["state"] not in {"primary_preflight", "recovery", "campaign", "final_scoring"}:
                raise GateError("continuation reservation occurred before required qualification/retirement")
            if event.get("run_id") != action["next_run_id"]:
                raise GateError("continuation run reservation is out of immutable slot order")
            if pending is not None:
                raise GateError("continuation reserved another process while a prior process was unresolved")
            expected_phase = _phase_id_for_state(plan, action["state"])
            if active_phase is None or active_phase.get("phase_id") != expected_phase:
                raise GateError("continuation process invocation is outside its measured coordinator phase")
            pending = event["run_id"]
        elif kind == "completed":
            if pending is None or event.get("run_id") != pending:
                raise GateError("continuation completion does not resolve its reservation")
            pending = None
        elif kind == "qualification_prepared":
            action = continuation_next_action(plan, prefix)
            path = event.get("qualification_path")
            expected = "primary_qualification_required" if path == "primary" else "recovery_qualification_required"
            phase_id = "primary_preflight" if path == "primary" else "recovery_preflight"
            if (active_phase is None or active_phase.get("phase_id") != phase_id
                    or action["state"] != expected or event.get("amendment_id") != amendment_id
                    or not isinstance(event.get("evidence_sha256"), str)):
                raise GateError("prepared qualification is not bound to the active preflight phase")
            _require_sha256(event["evidence_sha256"], "qualification_prepared.evidence_sha256")
        elif kind == "coordinator_phase":
            if (active_phase is None or event.get("token") != active_phase.get("token")
                    or event.get("phase_id") != active_phase.get("phase_id")
                    or event.get("amendment_id") != amendment_id
                    or event.get("status") not in {"completed", "failed"}):
                raise GateError("coordinator phase receipt does not close its active phase")
            cpu, wall = event.get("coordinator_cpu_seconds"), event.get("coordinator_exclusive_wall_seconds")
            measurements_available = (
                isinstance(cpu, (int, float)) and not isinstance(cpu, bool) and math.isfinite(cpu) and cpu >= 0
                and isinstance(wall, (int, float)) and not isinstance(wall, bool) and math.isfinite(wall) and wall >= 0
            )
            if not measurements_available and not (event.get("status") == "failed" and cpu is None and wall is None):
                raise GateError("coordinator phase receipt has invalid resource measurements")
            if not isinstance(event.get("receipt_sha256"), str):
                raise GateError("coordinator phase receipt lacks immutable evidence identity")
            _require_sha256(event["receipt_sha256"], "coordinator_phase.receipt_sha256")
            closed_phase_status[event["phase_id"]] = event["status"]
            category = event.get("failure_category")
            if event["status"] == "completed" and category is not None:
                raise GateError("successful coordinator phase has a failure category")
            if event["status"] == "failed" and category not in {
                    "stage_failure", "qualification_failure", "coordinator_phase_failure"}:
                raise GateError("failed coordinator phase lacks a valid failure category")
            closed_phase_category[event["phase_id"]] = category
            active_phase = None
        elif kind == "coordinator_worker_supervised":
            matching = [prior for prior in prefix if prior.get("event") == "coordinator_phase"
                        and prior.get("token") == event.get("token")]
            if len(matching) != 1:
                raise GateError("supervisor measurement does not bind exactly one closed phase receipt")
            phase_event = matching[0]
            if (event.get("phase_id") != phase_event.get("phase_id")
                    or event.get("amendment_id") != amendment_id
                    or event.get("status") not in {"verified", "failed"}
                    or not isinstance(event.get("script_path"), str)
                    or not isinstance(event.get("script_sha256"), str)):
                raise GateError("coordinator supervisor measurement identity is malformed")
            _require_sha256(event["script_sha256"], "coordinator_worker_supervised.script_sha256")
            if event.get("status") == "verified":
                for key in ("pid", "pid_starttime_ticks", "worker_self_peak_rss_bytes",
                            "worker_self_vm_hwm_bytes", "wait4_ru_maxrss_including_waited_children_bytes"):
                    if type(event.get(key)) is not int or event[key] <= 0:
                        raise GateError(f"coordinator supervisor measurement lacks {key}")
                if (event["pid"] != phase_event.get("pid")
                        or event["worker_self_peak_rss_bytes"] != phase_event.get("coordinator_peak_rss_bytes")
                        or event["worker_self_vm_hwm_bytes"] != phase_event.get("coordinator_proc_vm_hwm_bytes")
                        or event.get("worker_self_ru_maxrss_diagnostic_bytes") != phase_event.get("coordinator_rusage_self_ru_maxrss_diagnostic_bytes")
                        or event.get("worker_self_vm_rss_bytes") != phase_event.get("coordinator_proc_vm_rss_bytes")):
                    raise GateError("supervisor and coordinator worker self-memory measurements disagree")
                for key in ("supervisor_vm_hwm_bytes", "supervisor_vm_rss_bytes",
                            "supervisor_memory_limit_bytes"):
                    if type(event.get(key)) is not int or event[key] <= 0:
                        raise GateError(f"coordinator supervisor measurement lacks {key}")
                diagnostic = event.get("supervisor_ru_maxrss_diagnostic_bytes")
                if diagnostic is not None and (type(diagnostic) is not int or diagnostic <= 0):
                    raise GateError("coordinator supervisor ru_maxrss diagnostic is malformed")
                if (event["supervisor_memory_limit_bytes"] != CONTINUATION_MEMORY_BYTES
                        or event["supervisor_vm_hwm_bytes"] > CONTINUATION_MEMORY_BYTES
                        or event["supervisor_vm_rss_bytes"] > CONTINUATION_MEMORY_BYTES):
                    raise GateError("coordinator supervisor process memory cap/measurements are invalid")
                for key in ("supervisor_cpu_seconds", "supervisor_exclusive_wall_seconds"):
                    value = event.get(key)
                    if (isinstance(value, bool) or not isinstance(value, (int, float))
                            or not math.isfinite(value) or value < 0):
                        raise GateError(f"coordinator supervisor lacks valid {key}")
            else:
                if event.get("failure_category") != "coordinator_phase_failure" or not event.get("reason"):
                    raise GateError("failed supervisor measurement lacks terminal failure detail")
        elif kind == "admission_refused":
            if active_phase is not None or pending is not None:
                raise GateError("admission refusal cannot be recorded inside an open coordinator phase")
            action = continuation_next_action(plan, prefix)
            if (event.get("phase_id") != _phase_id_for_state(plan, action["state"])
                    or event.get("amendment_id") != amendment_id
                    or event.get("failure_category") != "admission_refused"
                    or not isinstance(event.get("reason"), str) or not event["reason"]):
                raise GateError("coordinator admission refusal is malformed or out of phase")
            terminal_seen = True
        elif kind == "coordinator_admission_attempt_refused":
            requester_pid = event.get("requester_pid")
            requester_start = event.get("requester_pid_starttime_ticks")
            if (active_phase is None
                    or event.get("active_phase_token") != active_phase.get("token")
                    or event.get("amendment_id") != amendment_id
                    or event.get("protocol_sha256") != active_phase.get("protocol_sha256")
                    or event.get("requested_phase_id") not in plan.get("coordinator_phase_ids", [])
                    or type(requester_pid) is not int or requester_pid <= 0
                    or type(requester_start) is not int or requester_start <= 0
                    or not isinstance(event.get("reason"), str) or not event["reason"]):
                raise GateError("concurrent coordinator admission refusal is malformed or owns the active phase")
        elif kind == "preflight_rejected":
            action = continuation_next_action(plan, prefix)
            if (action["state"] not in {"primary_qualification_required", "primary_rejection_required"}
                    or closed_phase_status.get("primary_preflight") != "failed"
                    or closed_phase_category.get("primary_preflight") not in {"stage_failure", "qualification_failure"}
                    or not any(event.get("event") == "coordinator_worker_supervised"
                               and event.get("phase_id") == "primary_preflight"
                               and event.get("status") == "verified" for event in prefix)):
                raise GateError("primary preflight rejection was recorded outside its qualification boundary")
            if event.get("amendment_id") != amendment_id or not event.get("reason"):
                raise GateError("primary rejection lacks amendment identity or reason")
            if event.get("failure_category") != closed_phase_category.get("primary_preflight"):
                raise GateError("primary rejection category differs from immutable failed phase receipt")
        elif kind == "recovery_rejected":
            action = continuation_next_action(plan, prefix)
            if (action["state"] not in {"recovery_qualification_required", "recovery_rejection_required"}
                    or closed_phase_status.get("recovery_preflight") != "failed"
                    or not any(event.get("event") == "coordinator_worker_supervised"
                               and event.get("phase_id") == "recovery_preflight"
                               and event.get("status") == "verified" for event in prefix)):
                raise GateError("recovery rejection was recorded outside its qualification boundary")
            if (event.get("amendment_id") != amendment_id or not event.get("reason")
                    or event.get("failure_category") not in {
                        "stage_failure", "qualification_failure", "coordinator_phase_failure"}):
                raise GateError("recovery rejection lacks amendment identity or reason")
            if event.get("failure_category") != closed_phase_category.get("recovery_preflight"):
                raise GateError("recovery rejection category differs from immutable failed phase receipt")
        elif kind == "qualified":
            action = continuation_next_action(plan, prefix)
            path = event.get("qualification_path")
            expected = "primary_qualification_required" if path == "primary" else "recovery_qualification_required"
            phase_id = "primary_preflight" if path == "primary" else "recovery_preflight"
            phase_event = next((row for row in reversed(prefix)
                                if row.get("event") == "coordinator_phase" and row.get("phase_id") == phase_id), None)
            prepared = [row for row in prefix if row.get("event") == "qualification_prepared"
                        and row.get("qualification_path") == path]
            if (action["state"] != expected or event.get("amendment_id") != amendment_id
                    or closed_phase_status.get(phase_id) != "completed"
                    or phase_event is None
                    or not any(row.get("event") == "coordinator_worker_supervised"
                               and row.get("token") == phase_event.get("token")
                               and row.get("status") == "verified" for row in prefix)
                    or len(prepared) != 1
                    or event.get("preflight_evidence_sha256") != prepared[0].get("evidence_sha256")
                    or event.get("coordinator_phase_receipt_sha256") != phase_event.get("receipt_sha256")):
                raise GateError("continuation qualification is inconsistent with its selected branch")
        elif kind == "recovery_retired":
            action = continuation_next_action(plan, prefix)
            if action["state"] != "retire_recovery" or event.get("run_id") not in action["retire_run_ids"]:
                raise GateError("recovery slot retirement is out of order or unauthorized")
            if (event.get("amendment_id") != amendment_id or event.get("charged_cpu_seconds") != 0
                    or event.get("charged_wall_seconds") != 0 or "invocation_id" in event):
                raise GateError("recovery retirement must be an immutable zero-charge non-invocation event")
        elif kind == "program_terminal":
            if any(previous.get("event") == "program_terminal" for previous in prefix):
                raise GateError("continuation program has more than one terminal event")
            if not isinstance(event.get("status"), str) or not event["status"]:
                raise GateError("program terminal event lacks a status")
            terminal_seen = True
        prefix.append(event)
    if pending is not None:
        raise GateError("continuation has an unresolved invocation")
    return continuation_next_action(plan, prefix)


def continuation_remaining_run_ids(plan: dict, events: list[dict]) -> list[str]:
    """Worst-case remaining calls along the currently eligible branch."""
    slots = plan["slots"]
    reserved = {event.get("run_id") for event in events if event.get("event") == "reserved"}
    retired = {event.get("run_id") for event in events if event.get("event") == "recovery_retired"}
    action = continuation_next_action(plan, events)
    if action["state"] in {"primary_preflight", "primary_qualification_required"}:
        phase_names = ("primary_preflight", "recovery_preflight", "campaign", "final_scoring")
    elif action["state"] == "primary_rejection_required":
        phase_names = ("recovery_preflight", "campaign", "final_scoring")
    elif action["state"] in {"recovery", "recovery_qualification_required"}:
        phase_names = ("recovery_preflight", "campaign", "final_scoring")
    elif action["state"] == "recovery_rejection_required":
        return []
    elif action["state"] == "retire_recovery":
        phase_names = ("campaign", "final_scoring")
    elif action["state"] in {"campaign", "final_scoring"}:
        phase_names = ("campaign", "final_scoring")
    else:
        return []
    return [run_id for phase in phase_names for run_id in slots[phase]
            if run_id not in reserved and run_id not in retired]


def _validate_bounded_program(protocol: object, *, allow_legacy_predecessor: bool = False) -> dict:
    """Validate the separate pinned-process protocol (not the solver schema)."""
    if not isinstance(protocol, dict) or protocol.get("schema") != 1 or protocol.get("kind") != "bounded_program":
        raise GateError("unsupported bounded-program protocol")
    source_commit = protocol.get("source_commit")
    if not isinstance(source_commit, str) or re.fullmatch(r"[0-9a-f]{40}", source_commit) is None:
        raise GateError("source_commit must be a full lowercase Git SHA1")
    _safe_program_id(protocol.get("protocol_id"), "protocol_id")
    budget = protocol.get("program_budget")
    if not isinstance(budget, dict):
        raise GateError("bounded program requires a persistent program_budget")
    program_id = _safe_program_id(budget.get("program_id"), "program_budget.program_id")
    expected_ledger = (PROCESS_LEDGER_ROOT / program_id).resolve()
    if not isinstance(budget.get("ledger_dir"), str) or Path(budget["ledger_dir"]).resolve() != expected_ledger:
        raise GateError("bounded-program ledger must use its canonical process-ledger path")
    amended = protocol.get("continuation_authorization") is not None
    invocation_cap_key = "max_new_invocations" if amended else "max_lifetime_invocations"
    cpu_cap_key = "max_new_cpu_seconds" if amended else "max_lifetime_cpu_seconds"
    wall_cap_key = "max_new_wall_seconds" if amended else "max_lifetime_wall_seconds"
    if amended:
        if program_id != "finite-domain-identifiability-v1":
            raise GateError("prospective continuation must retain the stable finite-identifiability program_id")
        if any(key in budget for key in ("max_lifetime_invocations", "max_lifetime_cpu_seconds", "max_lifetime_wall_seconds")):
            raise GateError("continuation budget must separate prospective caps from unknown historical totals")
        if not isinstance(budget.get(invocation_cap_key), int) or isinstance(budget.get(invocation_cap_key), bool) or budget[invocation_cap_key] < 0:
            raise GateError(f"program_budget.{invocation_cap_key} must be a nonnegative integer")
    else:
        if isinstance(budget.get(invocation_cap_key), bool) or not isinstance(budget.get(invocation_cap_key), int) or budget[invocation_cap_key] < 0:
            raise GateError(f"program_budget.{invocation_cap_key} must be a nonnegative integer")
    for key in (cpu_cap_key, wall_cap_key):
        value = budget.get(key)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0:
            raise GateError(f"program_budget.{key} must be finite and positive")

    limits = protocol.get("limits")
    if not isinstance(limits, dict) or not PROGRAM_LIMIT_FIELDS.issubset(limits):
        raise GateError("bounded-program limits are incomplete")
    for key in ("max_invocations", "max_address_space_bytes", "max_stdin_bytes", "max_output_bytes", "max_file_bytes"):
        if isinstance(limits.get(key), bool) or not isinstance(limits.get(key), int) or limits[key] < 0:
            raise GateError(f"limits.{key} must be a nonnegative integer")
    if limits["max_address_space_bytes"] < 1024 * 1024 or limits["max_output_bytes"] < 1 or limits["max_file_bytes"] < 1:
        raise GateError("address-space, output and file limits must be positive and practical")
    for key in ("max_total_cpu_seconds", "max_total_wall_seconds", "max_cpu_seconds", "max_wall_seconds"):
        value = limits.get(key)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0:
            raise GateError(f"limits.{key} must be finite and positive")
    if limits["max_invocations"] > budget[invocation_cap_key]:
        raise GateError("protocol invocation cap exceeds authorized program cap")
    if limits["max_total_cpu_seconds"] > budget[cpu_cap_key] or limits["max_total_wall_seconds"] > budget[wall_cap_key]:
        raise GateError("protocol time cap exceeds authorized program cap")
    if limits["max_cpu_seconds"] > limits["max_total_cpu_seconds"] or limits["max_wall_seconds"] > limits["max_total_wall_seconds"]:
        raise GateError("aggregate budget must admit one complete invocation")

    descriptor_raw = protocol.get("pipeline_descriptor")
    command = protocol.get("command")
    command_by_stage = {}
    if descriptor_raw is not None:
        if command is not None:
            raise GateError("descriptor-mode protocol must not also define a global command")
        try:
            pipeline_descriptor = validate_pipeline_descriptor(descriptor_raw, protocol.get("artifacts", []))
        except PipelineDescriptorError as error:
            raise GateError(str(error)) from error
        artifact_map = {a["id"]: a for a in protocol.get("artifacts", [])}
        for stage_name, stage in pipeline_descriptor["stages"].items():
            executable_record = artifact_map[stage["executable_artifact"]]
            command_by_stage[stage_name] = {
                "executable": str(Path(executable_record["path"]).resolve()),
                "sha256": executable_record["sha256"], "version": stage["version"],
                "args": [], "environment": stage["environment"],
                "runtime_files": [{"path": str(Path(artifact_map[dep]["path"]).resolve()),
                                   "sha256": artifact_map[dep]["sha256"]}
                                  for dep in stage["dependencies"]],
            }
    else:
        required_command = {"executable", "sha256", "version", "args", "environment", "runtime_files"}
        if not isinstance(command, dict) or not required_command.issubset(command):
            raise GateError("bounded-program command identity is incomplete")
        executable = Path(command["executable"]).resolve()
        _require_sha256(command["sha256"], "command.sha256")
        if not executable.is_file() or not os.access(executable, os.X_OK) or sha256_file(executable) != command["sha256"]:
            raise GateError("bounded-program executable is missing or hash-mismatched")
        if not isinstance(command["version"], str) or not command["version"].strip():
            raise GateError("command.version must be recorded")
        if not isinstance(command["args"], list) or not all(isinstance(arg, str) for arg in command["args"]):
            raise GateError("command.args must be a string array")
        env = command["environment"]
        if not isinstance(env, dict) or not env or not all(isinstance(k, str) and isinstance(v, str) for k, v in env.items()):
            raise GateError("command.environment must be an explicit string mapping")
        if not {"PATH", "LANG", "LC_ALL"}.issubset(env):
            raise GateError("command.environment must pin PATH, LANG and LC_ALL")
        runtime_files = command["runtime_files"]
        if not isinstance(runtime_files, list):
            raise GateError("command.runtime_files must be an array")
        seen_runtime = set()
        for item in runtime_files:
            if not isinstance(item, dict) or not isinstance(item.get("path"), str):
                raise GateError("runtime file identity requires path and SHA256")
            path = Path(item["path"]).resolve()
            _require_sha256(item.get("sha256"), "runtime_file.sha256")
            if path in seen_runtime or not path.is_file() or sha256_file(path) != item["sha256"]:
                raise GateError("runtime file is duplicate, missing or hash-mismatched")
            seen_runtime.add(path)

    artifacts = protocol.get("artifacts")
    if not isinstance(artifacts, list):
        raise GateError("bounded-program artifacts must be an array")
    artifacts_by_id = {}
    artifact_paths = set()
    for item in artifacts:
        if not isinstance(item, dict) or not {"id", "path", "sha256", "role"}.issubset(item):
            raise GateError("artifact requires id, path, SHA256 and role")
        artifact_id = _safe_program_id(item["id"], "artifact id")
        path = Path(item["path"]).resolve()
        _require_sha256(item["sha256"], f"artifact {artifact_id} SHA256")
        must_read = item.get("role") not in {"assessment", "preflight-input"}
        if artifact_id in artifacts_by_id or path in artifact_paths:
            raise GateError(f"artifact identity is duplicate: {artifact_id}")
        if must_read and (not path.is_file() or sha256_file(path) != item["sha256"]):
            raise GateError(f"artifact identity is missing or hash-mismatched: {artifact_id}")
        artifacts_by_id[artifact_id] = {**item, "path": str(path)}
        artifact_paths.add(path)
    runs = protocol.get("runs")
    if not isinstance(runs, list) or len(runs) != limits["max_invocations"]:
        raise GateError("bounded-program run allowlist must exactly match max_invocations")
    run_ids = set()
    run_by_id = {}
    planned_cpu = 0.0
    planned_wall = 0.0
    for item in runs:
        common_keys = {"run_id", "stdin_artifact", "phase_id"}
        if descriptor_raw is not None:
            common_keys |= {"stage_id", "slot_bindings"}
        if not isinstance(item, dict) or not common_keys.issubset(item):
            raise GateError("each bounded-program run is missing its required stage/input bindings")
        run_id = _safe_program_id(item["run_id"], "run_id")
        if run_id in run_ids:
            raise GateError("bounded-program run IDs must be unique")
        run_ids.add(run_id)
        run_by_id[run_id] = item
        if item["stdin_artifact"] is not None and item["stdin_artifact"] not in artifacts_by_id:
            raise GateError(f"run {run_id} refers to an unknown stdin artifact")
        if descriptor_raw is not None:
            stage_id = item["stage_id"]
            if stage_id not in pipeline_descriptor["stages"] or item["stdin_artifact"] is not None or item.get("args"):
                raise GateError(f"descriptor run {run_id} must select a pinned stage and cannot override argv/stdin")
            stage = pipeline_descriptor["stages"][stage_id]
            templates = {m.group(1) for arg in stage["argv"] if (m := re.fullmatch(r"\{([a-z][a-z0-9_]*)\}", arg))}
            outputs = set(stage["output_roles"])
            bindings = item["slot_bindings"]
            if not isinstance(bindings, dict) or set(bindings) != templates - outputs:
                raise GateError(f"descriptor run {run_id} slot bindings must exactly match stage inputs")
            for slot, binding in bindings.items():
                if isinstance(binding, list):
                    references = validate_run_output_list_binding(binding, slot, run_id, run_by_id)
                    for reference in references:
                        source = run_by_id[reference["run_id"]]
                        source_stage = pipeline_descriptor["stages"][source["stage_id"]]
                        if reference["slot"] not in source_stage["output_roles"]:
                            raise GateError("candidate_outputs list references an undeclared producer output slot")
                    continue
                if not isinstance(binding, dict) or binding.get("kind") not in {"artifact", "controller_file", "run_output"}:
                    raise GateError(f"descriptor run {run_id} has an invalid typed binding for {slot}")
                if binding["kind"] == "artifact":
                    artifact = artifacts_by_id.get(binding.get("artifact_id"))
                    if artifact is None or artifact.get("role") in {"pipeline-executable", "pipeline-dependency"}:
                        raise GateError(f"descriptor run {run_id} binds an unknown/data-ineligible artifact")
                    if artifact.get("role") == "assessment" and (item["phase_id"] != "scoring" or slot != "assessment_bundle"):
                        raise GateError("assessment artifacts are admitted only to assessment_bundle in scoring")
                elif binding["kind"] == "controller_file":
                    rel = Path(binding.get("path", ""))
                    _validate_controller_file_phase(item["phase_id"], slot)
                    if rel.is_absolute() or ".." in rel.parts or not rel.parts:
                        raise GateError("controller files must use safe relative typed input paths")
                else:
                    source_id, source_slot = binding.get("run_id"), binding.get("slot")
                    source = run_by_id.get(source_id)
                    if source is None or source_id == run_id:
                        raise GateError("run_output binding must refer to an earlier admitted run")
                    source_stage = pipeline_descriptor["stages"][source["stage_id"]]
                    if source_slot not in source_stage["output_roles"]:
                        raise GateError("run_output binding references an undeclared output slot")
            if set(stage["input_roles"]) != set(bindings):
                raise GateError(f"descriptor run {run_id} must bind every declared stage input exactly once")
        elif item.get("stage_id") is not None or item.get("slot_bindings") is not None:
            raise GateError("stage bindings require a pipeline_descriptor")
        extra_args = item.get("args", [])
        if not isinstance(extra_args, list) or not all(isinstance(arg, str) for arg in extra_args):
            raise GateError(f"run {run_id} args must be a string array")
        run_cpu = item.get("max_cpu_seconds", limits["max_cpu_seconds"])
        run_wall = item.get("max_wall_seconds", limits["max_wall_seconds"])
        for name, number, cap in (("max_cpu_seconds", run_cpu, limits["max_cpu_seconds"]),
                                  ("max_wall_seconds", run_wall, limits["max_wall_seconds"])):
            if isinstance(number, bool) or not isinstance(number, (int, float)) or not math.isfinite(number) or number <= 0 or number > cap:
                raise GateError(f"run {run_id} {name} must be positive and no greater than the protocol per-run cap")
        planned_cpu += float(run_cpu) + BOUNDED_SUPERVISOR_CPU_ALLOWANCE_SECONDS
        planned_wall += float(run_wall) + BOUNDED_SUPERVISOR_WALL_ALLOWANCE_SECONDS
    phase_plan = protocol.get("phase_plan")
    flattened = []
    if amended:
        flattened = validate_continuation_phase_plan(
            phase_plan, runs, protocol["continuation_authorization"]["slots"])
    else:
        required_phases = ["preflight", "campaign", "verification", "scoring", "aggregation", "recovery"]
        if not isinstance(phase_plan, list) or [phase.get("phase_id") for phase in phase_plan if isinstance(phase, dict)] != required_phases:
            raise GateError("phase_plan must name preflight, campaign, verification, scoring, aggregation and recovery in order")
        for phase in phase_plan:
            ids = phase.get("run_ids")
            if not isinstance(ids, list) or not all(isinstance(value, str) for value in ids):
                raise GateError(f"phase {phase['phase_id']} run_ids must be an ordered string array")
            if len(ids) != len(set(ids)):
                raise GateError(f"phase {phase['phase_id']} repeats a run ID")
            for run_id in ids:
                if run_id not in run_by_id or run_by_id[run_id]["phase_id"] != phase["phase_id"]:
                    raise GateError(f"phase plan/run phase mismatch for {run_id}")
                flattened.append(run_id)
    if not amended and flattened != [run["run_id"] for run in runs]:
        raise GateError("phase plan must cover the exact allowlist once in frozen slot order")
    continuation_plan = None
    if amended:
        if descriptor_raw is None:
            raise GateError("continuation authorization requires descriptor-mode pipeline stages")
        continuation_plan = validate_bounded_program_continuation(
            protocol["continuation_authorization"], runs, artifacts_by_id, limits, budget,
            pipeline_descriptor, allow_legacy_predecessor=allow_legacy_predecessor)
        if limits["max_invocations"] != len(runs):
            raise GateError("continuation invocation ceiling must cover the complete immutable run allowlist")
        campaign_outputs = continuation_plan["slots"]["campaign"]
        for run in runs:
            if run.get("stage_id") != "verifier" or run.get("phase_id") != "scoring":
                continue
            binding = run.get("slot_bindings", {}).get("candidate_outputs")
            if not isinstance(binding, list) or [row.get("run_id") for row in binding if isinstance(row, dict)] != campaign_outputs:
                raise GateError("final verifier candidate_outputs must bind every campaign output in frozen order")
    qualification = protocol.get("preflight_qualification") if descriptor_raw is not None else None
    recovery_qualification = protocol.get("recovery_qualification") if amended and descriptor_raw is not None else None
    if descriptor_raw is not None:
        if not isinstance(qualification, dict) or set(qualification) != {"stage_run_ids", "expected_score_run_ids", "expected_control_count", "expected_score_bindings", "spec_artifact_id"}:
            raise GateError("descriptor protocol requires a frozen preflight_qualification contract")
        stage_ids = qualification["stage_run_ids"]
        if not isinstance(stage_ids, dict) or set(stage_ids) != {"runner", "verifier", "scorer", "aggregator"}:
            raise GateError("preflight qualification must bind all four stage run IDs")
        for stage_name, run_id in stage_ids.items():
            stage_run = run_by_id.get(run_id)
            if stage_run is None or stage_run.get("phase_id") != "preflight" or stage_run.get("stage_id") != stage_name:
                raise GateError(f"preflight qualification run mismatch for {stage_name}")
        if amended:
            primary_slots = protocol["continuation_authorization"].get("slots", {}).get("primary_preflight")
            if primary_slots != [stage_ids[name] for name in ("runner", "verifier", "scorer", "aggregator")]:
                raise GateError("primary preflight qualification must bind the exact ordered primary slot quartet")
            if not isinstance(recovery_qualification, dict) or set(recovery_qualification) != set(qualification):
                raise GateError("continuation recovery qualification contract is incomplete")
            recovery_stage_ids = recovery_qualification.get("stage_run_ids")
            if not isinstance(recovery_stage_ids, dict) or set(recovery_stage_ids) != {"runner", "verifier", "scorer", "aggregator"}:
                raise GateError("recovery qualification must bind all four stage run IDs")
            for stage_name, run_id in recovery_stage_ids.items():
                stage_run = run_by_id.get(run_id)
                if stage_run is None or stage_run.get("phase_id") != "recovery" or stage_run.get("stage_id") != stage_name:
                    raise GateError(f"recovery qualification run mismatch for {stage_name}")
            recovery_slots = protocol["continuation_authorization"].get("slots", {}).get("recovery_preflight")
            if recovery_slots != [recovery_stage_ids[name] for name in ("runner", "verifier", "scorer", "aggregator")]:
                raise GateError("recovery qualification must bind the exact ordered recovery slot quartet")
        score_ids = qualification["expected_score_run_ids"]
        if not isinstance(score_ids, list) or not score_ids or len(score_ids) != len(set(score_ids)) or not all(isinstance(v, str) for v in score_ids):
            raise GateError("preflight qualification needs unique expected scorer run IDs")
        if type(qualification["expected_control_count"]) is not int or qualification["expected_control_count"] < 2:
            raise GateError("preflight qualification requires at least two controls")
        bindings = qualification["expected_score_bindings"]
        required_binding_keys = {"run_id", "control_id", "method", "seed"}
        if (not isinstance(bindings, list) or len(bindings) != len(score_ids)
                or any(not isinstance(binding, dict) or set(binding) != required_binding_keys for binding in bindings)
                or [binding["run_id"] for binding in bindings] != score_ids
                or len({binding["control_id"] for binding in bindings}) != qualification["expected_control_count"]):
            raise GateError("preflight qualification must freeze ordered run/control/method/seed mappings")
        spec_artifact = artifacts_by_id.get(qualification["spec_artifact_id"])
        if spec_artifact is None or spec_artifact.get("role") != "preflight-spec":
            raise GateError("preflight qualification must pin its synthetic specification artifact")
    if planned_cpu > limits["max_total_cpu_seconds"] or planned_wall > limits["max_total_wall_seconds"]:
        raise GateError("complete phase plan per-run CPU/wall ceilings exceed protocol aggregate limits")
    if planned_cpu > budget[cpu_cap_key] or planned_wall > budget[wall_cap_key]:
        raise GateError("complete phase plan exceeds persistent program-wide CPU/wall ceilings")
    if len(runs) > budget[invocation_cap_key]:
        raise GateError("complete phase plan exceeds persistent program-wide invocation ceiling")
    if amended and (
        not math.isclose(planned_cpu + CONTINUATION_COORDINATOR_ALLOWANCE["cpu_seconds"],
                         CONTINUATION_ALLOWANCE["cpu_seconds"], rel_tol=0, abs_tol=1e-9)
        or not math.isclose(planned_wall + CONTINUATION_COORDINATOR_ALLOWANCE["exclusive_wall_seconds"],
                            CONTINUATION_ALLOWANCE["wall_seconds"], rel_tol=0, abs_tol=1e-9)
    ):
        raise GateError("declared per-stage envelopes plus coordinator overhead do not reconcile with authorization")
    normalized_command = ({**command, "executable": str(executable)} if command is not None else None)
    return {"protocol": protocol, "program_id": program_id, "ledger_dir": expected_ledger,
            "command": normalized_command, "artifacts": artifacts_by_id,
            "commands_by_stage": command_by_stage, "pipeline_descriptor": pipeline_descriptor if descriptor_raw is not None else None,
            "preflight_qualification": qualification,
            "recovery_qualification": recovery_qualification,
            "runs": runs, "run_by_id": run_by_id, "phase_plan": phase_plan,
            "continuation_plan": continuation_plan,
            "planned_cpu_seconds": planned_cpu, "planned_wall_seconds": planned_wall,
            "limits": limits, "budget": budget}


def _process_ledger_identity(config: dict) -> dict:
    budget = config["budget"]
    protocol_sha256 = config.get("frozen_protocol_sha256")
    if protocol_sha256 is None:
        protocol_sha256 = sha256_bytes(canonical_json(config["protocol"]))
    identity = {"schema": 1, "kind": "bounded_program_ledger", "program_id": config["program_id"],
                "frozen_protocol_sha256": protocol_sha256}
    if config.get("continuation_plan") is not None:
        identity.update({
            "accounting_scope": "prospective_continuation_only",
            "max_new_invocations": budget["max_new_invocations"],
            "max_new_cpu_seconds": budget["max_new_cpu_seconds"],
            "max_new_wall_seconds": budget["max_new_wall_seconds"],
            "amendment_id": config["continuation_plan"]["amendment_id"],
            "max_coordinator_cpu_seconds": config["continuation_plan"]["coordinator_allowance"]["cpu_seconds"],
            "max_coordinator_exclusive_wall_seconds": config["continuation_plan"]["coordinator_allowance"]["exclusive_wall_seconds"],
            "overall_deadline_seconds": config["continuation_plan"]["overall_deadline_seconds"],
        })
    else:
        identity.update({
            "accounting_scope": "program_lifetime",
            "max_lifetime_invocations": budget["max_lifetime_invocations"],
            "max_lifetime_cpu_seconds": budget["max_lifetime_cpu_seconds"],
            "max_lifetime_wall_seconds": budget["max_lifetime_wall_seconds"],
        })
    return identity


def _process_budget_caps(state: dict) -> tuple[int, float, float]:
    if state.get("accounting_scope") == "prospective_continuation_only":
        return (int(state["max_new_invocations"]), float(state["max_new_cpu_seconds"]),
                float(state["max_new_wall_seconds"]))
    return (int(state["max_lifetime_invocations"]), float(state["max_lifetime_cpu_seconds"]),
            float(state["max_lifetime_wall_seconds"]))


def _self_cpu_seconds() -> float:
    usage = resource.getrusage(resource.RUSAGE_SELF)
    return float(usage.ru_utime + usage.ru_stime)


def _self_peak_rss_bytes() -> int:
    usage = resource.getrusage(resource.RUSAGE_SELF)
    # Linux reports ru_maxrss in KiB. Continuation is Linux-only because its
    # process-ledger deadline is bound to /proc boot identity.
    return int(usage.ru_maxrss * 1024)


def _proc_self_vm_hwm_bytes() -> int:
    """Read this live process's Linux VmHWM in bytes, independently of wait4."""
    try:
        for line in Path("/proc/self/status").read_text(encoding="ascii").splitlines():
            if line.startswith("VmHWM:"):
                fields = line.split()
                if len(fields) != 3 or fields[2] != "kB":
                    break
                value = int(fields[1]) * 1024
                if value > 0:
                    return value
                break
    except (OSError, ValueError):
        pass
    raise GateError("coordinator /proc VmHWM measurement unavailable")


def _proc_self_vm_rss_bytes() -> int:
    """Read this live process's current Linux VmRSS in bytes."""
    try:
        for line in Path("/proc/self/status").read_text(encoding="ascii").splitlines():
            if line.startswith("VmRSS:"):
                fields = line.split()
                if len(fields) != 3 or fields[2] != "kB":
                    break
                value = int(fields[1]) * 1024
                if value > 0:
                    return value
                break
    except (OSError, ValueError):
        pass
    raise GateError("coordinator /proc VmRSS measurement unavailable")


def _coordinator_memory_measurements() -> tuple[int | None, int, int]:
    """Return inherited-prone RUSAGE diagnostic, authoritative VmHWM, and current VmRSS."""
    try:
        usage_peak = _self_peak_rss_bytes()
    except GateError:
        usage_peak = None
    proc_peak = _proc_self_vm_hwm_bytes()
    proc_current = _proc_self_vm_rss_bytes()
    return usage_peak, proc_peak, proc_current


def _require_coordinator_vm_hwm_within_limit(vm_hwm_bytes: int) -> None:
    """Apply the resident-peak policy to post-exec VmHWM, never inherited ru_maxrss."""
    if type(vm_hwm_bytes) is not int or vm_hwm_bytes <= 0:
        raise GateError("coordinator /proc VmHWM measurement unavailable")
    if vm_hwm_bytes > CONTINUATION_MEMORY_BYTES:
        raise GateError("coordinator has exceeded its 512 MiB observed VmHWM cap")


def _process_starttime_ticks(pid: int) -> int:
    try:
        text = Path(f"/proc/{pid}/stat").read_text(encoding="ascii")
        fields_after_comm = text[text.rfind(")") + 2:].split()
        return int(fields_after_comm[19])
    except (OSError, ValueError, IndexError) as error:
        raise GateError("coordinator PID starttime measurement unavailable") from error


def _enforce_coordinator_address_space(limit_bytes: int) -> None:
    if not hasattr(resource, "RLIMIT_AS"):
        raise GateError("coordinator address-space enforcement is unavailable")
    soft, hard = resource.getrlimit(resource.RLIMIT_AS)
    if hard != resource.RLIM_INFINITY and hard < limit_bytes:
        limit_bytes = int(hard)
    if limit_bytes <= 0:
        raise GateError("coordinator address-space limit is invalid")
    try:
        resource.setrlimit(resource.RLIMIT_AS, (limit_bytes, limit_bytes))
    except (OSError, ValueError) as error:
        raise GateError("could not enforce coordinator address-space limit") from error
    applied_soft, applied_hard = resource.getrlimit(resource.RLIMIT_AS)
    if applied_soft != limit_bytes or applied_hard != limit_bytes:
        raise GateError("coordinator address-space limit could not be verified")


def _kernel_boot_id() -> str:
    try:
        value = Path("/proc/sys/kernel/random/boot_id").read_text(encoding="ascii").strip()
    except OSError as error:
        raise GateError("cannot establish Linux boot identity for the continuation deadline") from error
    if not value:
        raise GateError("Linux boot identity is empty")
    return value


def _process_ledger_event(path: Path, payload: dict, previous_hash: str) -> str:
    event = {**payload, "previous_hash": previous_hash}
    event_hash = sha256_bytes(compact_json(event))
    event["event_sha256"] = event_hash
    fd = os.open(path, os.O_WRONLY | os.O_APPEND)
    with os.fdopen(fd, "ab") as stream:
        stream.write(compact_json(event) + b"\n")
        stream.flush()
        os.fsync(stream.fileno())
    return event_hash


def _replace_process_budget(directory: Path, state: dict) -> None:
    temporary = directory / f"process-budget.{uuid.uuid4().hex}.tmp"
    write_exclusive(temporary, canonical_json(state), 0o600)
    os.replace(temporary, directory / "process-budget.json")
    _fsync_directory(directory)


def _append_program_terminal(directory: Path, state: dict, status: str) -> None:
    if state.get("terminal_status") is not None:
        return
    state["ledger_head"] = _process_ledger_event(
        directory / "process-events.jsonl",
        {"event": "program_terminal", "status": status, "at_unix_ns": time.time_ns()},
        state["ledger_head"])
    state["event_count"] += 1
    state["terminal_status"] = status
    _replace_process_budget(directory, state)


def _append_continuation_event(directory: Path, state: dict, payload: dict) -> dict:
    event_hash = _process_ledger_event(directory / "process-events.jsonl", payload, state["ledger_head"])
    state["ledger_head"] = event_hash
    state["event_count"] += 1
    return {**payload, "previous_hash": "", "event_sha256": event_hash}


def _pending_coordinator_supervision(events: list[dict]) -> list[str]:
    closed = {event.get("token") for event in events if event.get("event") == "coordinator_phase"}
    supervised = {event.get("token") for event in events if event.get("event") == "coordinator_worker_supervised"}
    return sorted(token for token in closed - supervised if isinstance(token, str))


def record_supervised_coordinator_worker(directory: Path, phase_id: str,
                                        measurement: dict | None,
                                        script_path: Path) -> dict:
    """Bind parent wait4/proc measurements to the worker's immutable phase receipt."""
    _, config, protocol_sha = read_frozen_program(directory)
    plan = config.get("continuation_plan")
    if not isinstance(plan, dict):
        raise GateError("coordinator worker measurement requires an amended protocol")
    ledger = config["ledger_dir"]
    lock_fd = os.open(ledger / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state = json.loads((ledger / "process-budget.json").read_text())
        events = _verify_process_ledger(ledger, state, protocol_sha, plan)
        scoped = [event for event in events if event.get("protocol_sha256") == protocol_sha]
        phase_events = [event for event in scoped if event.get("event") == "coordinator_phase"
                        and event.get("phase_id") == phase_id]
        if not phase_events:
            if state.get("terminal_status") is not None:
                return {"status": "terminal_failure", "terminal_status": state["terminal_status"]}
            active = state.get("active_coordinator_phase")
            if isinstance(active, dict):
                requester_pid = (measurement.get("pid") if isinstance(measurement, dict)
                                 else os.getpid())
                requester_start = (measurement.get("starttime_ticks") if isinstance(measurement, dict)
                                   else _process_starttime_ticks(os.getpid()))
                owns_process = (isinstance(measurement, dict)
                                and requester_pid == active.get("pid")
                                and requester_start == active.get("pid_starttime_ticks")
                                and phase_id == active.get("phase_id"))
                if owns_process:
                    entrypoint = (plan.get("preflight_coordinator_entrypoint")
                                  if phase_id.endswith("preflight") else plan.get("coordinator_entrypoint"))
                    actual_path = script_path.resolve()
                    actual_sha = sha256_file(actual_path)
                    if (not isinstance(entrypoint, dict)
                            or Path(entrypoint.get("path", "")).resolve() != actual_path
                            or entrypoint.get("sha256") != actual_sha):
                        _append_program_terminal(ledger, state, "coordinator_worker_identity_mismatch")
                        _replace_process_budget(ledger, state)
                        raise GateError("failed coordinator worker does not match the frozen entrypoint")
                    failure = "coordinator worker exited without an immutable phase receipt"
                    receipt = {
                        "schema": 1, "protocol_sha256": protocol_sha,
                        "amendment_id": plan["amendment_id"], "phase_id": active["phase_id"],
                        "token": active["token"], "pid": active["pid"],
                        "pid_starttime_ticks": active.get("pid_starttime_ticks"), "status": "failed",
                        "measurement_method": "FRESH_WORKER_RUSAGE_SELF_AND_PROC_VM_HWM_RESIDUAL_WALL",
                        "coordinator_cpu_seconds": None, "coordinator_exclusive_wall_seconds": None,
                        "coordinator_peak_rss_bytes": None, "coordinator_proc_vm_hwm_bytes": None,
                        "coordinator_rusage_self_ru_maxrss_diagnostic_bytes": None,
                        "coordinator_proc_vm_rss_bytes": None,
                        "coordinator_memory_scope": "single_fresh_worker_process",
                        "memory_measurement_status": "unavailable",
                        "failure_category": "coordinator_phase_failure", "reason": failure,
                    }
                    receipt_dir = ledger / "coordinator-phases"
                    receipt_dir.mkdir(exist_ok=True)
                    receipt_path = receipt_dir / f"{active['phase_id']}-{active['token']}.json"
                    write_exclusive(receipt_path, canonical_json(receipt))
                    _append_continuation_event(ledger, state, {
                        "event": "coordinator_phase", "protocol_sha256": protocol_sha,
                        "amendment_id": plan["amendment_id"], "phase_id": active["phase_id"],
                        "token": active["token"], "status": "failed", "pid": active["pid"],
                        "pid_starttime_ticks": active.get("pid_starttime_ticks"),
                        "receipt_path": str(receipt_path.resolve()),
                        "receipt_sha256": sha256_file(receipt_path),
                        "coordinator_cpu_seconds": None, "coordinator_exclusive_wall_seconds": None,
                        "coordinator_peak_rss_bytes": None, "coordinator_proc_vm_hwm_bytes": None,
                        "coordinator_rusage_self_ru_maxrss_diagnostic_bytes": None,
                        "coordinator_proc_vm_rss_bytes": None,
                        "coordinator_memory_scope": "single_fresh_worker_process",
                        "failure_category": "coordinator_phase_failure", "reason": failure,
                    })
                    entry_event = {
                        "event": "coordinator_worker_supervised", "protocol_sha256": protocol_sha,
                        "amendment_id": plan["amendment_id"], "phase_id": active["phase_id"],
                        "token": active["token"], "script_path": str(actual_path),
                        "script_sha256": actual_sha, "pid": requester_pid,
                        "pid_starttime_ticks": requester_start, "status": "failed",
                        "failure_category": "coordinator_phase_failure", "reason": failure,
                    }
                    _append_continuation_event(ledger, state, entry_event)
                    state["active_coordinator_phase"] = None
                    _append_program_terminal(ledger, state, "coordinator_worker_failed_without_phase_receipt")
                    _replace_process_budget(ledger, state)
                    return {"status": "terminal_failure", "failure_category": "coordinator_phase_failure"}
                else:
                    _append_continuation_event(ledger, state, {
                        "event": "coordinator_admission_attempt_refused",
                        "protocol_sha256": protocol_sha,
                        "amendment_id": plan["amendment_id"],
                        "active_phase_token": active["token"],
                        "requested_phase_id": phase_id,
                        "requester_pid": requester_pid,
                        "requester_pid_starttime_ticks": requester_start,
                        "reason": "supervised request has no exact ownership of the active coordinator phase",
                    })
                    _replace_process_budget(ledger, state)
                    return {"status": "admission_attempt_refused", "active_phase_token": active["token"]}
            try:
                action = continuation_next_action(plan, scoped)
                expected_phase = _phase_id_for_state(plan, action["state"])
            except GateError:
                expected_phase = phase_id
            _append_continuation_event(ledger, state, {
                "event": "admission_refused", "protocol_sha256": protocol_sha,
                "amendment_id": plan["amendment_id"], "phase_id": expected_phase,
                "requested_phase_id": phase_id, "failure_category": "admission_refused",
                "reason": "coordinator worker exited without an immutable phase receipt",
            })
            _append_program_terminal(ledger, state, "coordinator_admission_refused")
            state["active_coordinator_phase"] = None
            _replace_process_budget(ledger, state)
            return {"status": "terminal_failure", "failure_category": "coordinator_phase_failure"}
        phase_event = phase_events[-1]
        if any(event.get("event") == "coordinator_worker_supervised"
               and event.get("token") == phase_event.get("token") for event in scoped):
            raise GateError("coordinator worker measurement is already recorded")
        receipt_path = Path(phase_event["receipt_path"])
        receipt = json.loads(receipt_path.read_text())
        entrypoint = (plan.get("preflight_coordinator_entrypoint") if phase_id.endswith("preflight")
                      else plan.get("coordinator_entrypoint"))
        actual_path = script_path.resolve()
        actual_sha = sha256_file(actual_path)
        if (not isinstance(entrypoint, dict) or Path(entrypoint.get("path", "")).resolve() != actual_path
                or entrypoint.get("sha256") != actual_sha):
            _append_program_terminal(ledger, state, "coordinator_worker_identity_mismatch")
            _replace_process_budget(ledger, state)
            raise GateError("supervised coordinator source path/hash differs from frozen entrypoint")
        reason = None
        if not isinstance(measurement, dict):
            reason = "supervisor measurement unavailable"
        else:
            pid = measurement.get("pid")
            start_ticks = measurement.get("starttime_ticks")
            worker_rss = receipt.get("coordinator_peak_rss_bytes")
            worker_hwm = receipt.get("coordinator_proc_vm_hwm_bytes")
            sampled_hwm = measurement.get("worker_proc_vm_hwm_sampled_bytes")
            waited_rss = measurement.get("wait4_ru_maxrss_including_waited_children_bytes")
            if (pid != receipt.get("pid") or pid != measurement.get("pid")
                    or start_ticks != receipt.get("coordinator_pid_starttime_ticks")
                    or start_ticks != measurement.get("starttime_ticks")):
                reason = "coordinator worker PID/starttime identity mismatch"
            elif (type(worker_rss) is not int or type(worker_hwm) is not int
                  or worker_rss <= 0 or worker_hwm <= 0 or worker_rss != worker_hwm):
                reason = "worker self RSS/VmHWM receipt is unavailable or inconsistent"
            elif sampled_hwm is not None and sampled_hwm > worker_hwm + max(1024 * 1024, int(os.sysconf("SC_PAGE_SIZE"))):
                reason = "supervisor observed worker VmHWM above worker receipt"
            elif type(waited_rss) is not int or waited_rss <= 0:
                reason = "wait4 waited-process RSS observation is unavailable"
            elif worker_rss > CONTINUATION_MEMORY_BYTES:
                reason = "coordinator worker RSS cap exceeded"
            elif (measurement.get("address_space_limit_bytes") != CONTINUATION_MEMORY_BYTES
                  or type(measurement.get("supervisor_ru_maxrss_diagnostic_bytes")) is not int
                  or type(measurement.get("supervisor_vm_hwm_bytes")) is not int
                  or type(measurement.get("supervisor_vm_rss_bytes")) is not int
                  or measurement["supervisor_ru_maxrss_diagnostic_bytes"] <= 0
                  or measurement["supervisor_vm_hwm_bytes"] <= 0
                  or measurement["supervisor_vm_rss_bytes"] <= 0
                  or measurement["supervisor_vm_hwm_bytes"] > CONTINUATION_MEMORY_BYTES
                  or measurement["supervisor_vm_rss_bytes"] > CONTINUATION_MEMORY_BYTES):
                reason = "coordinator supervisor memory limit/measurement unavailable or exceeded"
            elif (isinstance(measurement.get("supervisor_cpu_seconds"), bool)
                  or not isinstance(measurement.get("supervisor_cpu_seconds"), (int, float))
                  or not math.isfinite(measurement["supervisor_cpu_seconds"])
                  or measurement["supervisor_cpu_seconds"] < 0
                  or isinstance(measurement.get("supervisor_exclusive_wall_seconds"), bool)
                  or not isinstance(measurement.get("supervisor_exclusive_wall_seconds"), (int, float))
                  or not math.isfinite(measurement["supervisor_exclusive_wall_seconds"])
                  or measurement["supervisor_exclusive_wall_seconds"] < 0):
                reason = "coordinator supervisor CPU/wall measurement unavailable"
            elif measurement.get("timed_out") or measurement.get("output_limit_exceeded"):
                reason = "coordinator supervisor wall/output limit reached"
            elif measurement.get("returncode") != 0 and phase_event.get("status") == "completed":
                reason = "worker reported successful phase but process returned failure"
        payload = {
            "event": "coordinator_worker_supervised", "protocol_sha256": protocol_sha,
            "amendment_id": plan["amendment_id"], "phase_id": phase_id,
            "token": phase_event["token"], "script_path": str(actual_path),
            "script_sha256": actual_sha,
            "pid": measurement.get("pid") if isinstance(measurement, dict) else None,
            "pid_starttime_ticks": measurement.get("starttime_ticks") if isinstance(measurement, dict) else None,
            "worker_self_peak_rss_bytes": receipt.get("coordinator_peak_rss_bytes"),
            "worker_self_vm_hwm_bytes": receipt.get("coordinator_proc_vm_hwm_bytes"),
            "worker_self_ru_maxrss_diagnostic_bytes": receipt.get("coordinator_rusage_self_ru_maxrss_diagnostic_bytes"),
            "worker_self_vm_rss_bytes": receipt.get("coordinator_proc_vm_rss_bytes"),
            "supervisor_sampled_vm_hwm_bytes": measurement.get("worker_proc_vm_hwm_sampled_bytes") if isinstance(measurement, dict) else None,
            "wait4_ru_maxrss_including_waited_children_bytes": measurement.get("wait4_ru_maxrss_including_waited_children_bytes") if isinstance(measurement, dict) else None,
            "wait4_cpu_seconds_including_waited_children": measurement.get("cpu_seconds") if isinstance(measurement, dict) else None,
            "supervisor_cpu_seconds": measurement.get("supervisor_cpu_seconds") if isinstance(measurement, dict) else None,
            "supervisor_exclusive_wall_seconds": measurement.get("supervisor_exclusive_wall_seconds") if isinstance(measurement, dict) else None,
            "supervisor_ru_maxrss_diagnostic_bytes": measurement.get("supervisor_ru_maxrss_diagnostic_bytes") if isinstance(measurement, dict) else None,
            "supervisor_vm_hwm_bytes": measurement.get("supervisor_vm_hwm_bytes") if isinstance(measurement, dict) else None,
            "supervisor_vm_rss_bytes": measurement.get("supervisor_vm_rss_bytes") if isinstance(measurement, dict) else None,
            "supervisor_memory_limit_bytes": measurement.get("address_space_limit_bytes") if isinstance(measurement, dict) else None,
            "status": "failed" if reason else "verified",
            "failure_category": "coordinator_phase_failure" if reason else None,
            "reason": reason,
        }
        if not reason and isinstance(measurement, dict):
            projected_cpu = state["coordinator_cpu_seconds_charged"] + float(measurement["supervisor_cpu_seconds"])
            projected_wall = state["coordinator_exclusive_wall_seconds_charged"] + float(measurement["supervisor_exclusive_wall_seconds"])
            state["coordinator_cpu_seconds_charged"] = projected_cpu
            state["coordinator_exclusive_wall_seconds_charged"] = projected_wall
            if (projected_cpu > state["max_coordinator_cpu_seconds"]
                    or projected_wall > state["max_coordinator_exclusive_wall_seconds"]):
                reason = "coordinator supervisor overhead allowance exceeded"
                payload["status"] = "failed"
                payload["failure_category"] = "coordinator_phase_failure"
                payload["reason"] = reason
        _append_continuation_event(ledger, state, payload)
        if reason:
            _append_program_terminal(ledger, state, "coordinator_worker_measurement_failed")
        elif phase_event.get("status") == "failed" and state.get("terminal_status") is None:
            failure_category = phase_event.get("failure_category")
            if phase_id == "primary_preflight" and failure_category in {"stage_failure", "qualification_failure"}:
                _append_continuation_event(ledger, state, {
                    "event": "preflight_rejected", "protocol_sha256": protocol_sha,
                    "amendment_id": plan["amendment_id"], "failure_category": failure_category,
                    "reason": phase_event.get("reason") or "primary preflight failed",
                })
            elif phase_id == "recovery_preflight":
                _append_continuation_event(ledger, state, {
                    "event": "recovery_rejected", "protocol_sha256": protocol_sha,
                    "amendment_id": plan["amendment_id"],
                    "failure_category": failure_category or "coordinator_phase_failure",
                    "reason": phase_event.get("reason") or "recovery preflight failed",
                })
                _append_program_terminal(ledger, state, "recovery_preflight_failed")
        elif phase_event.get("status") == "completed" and phase_id in {"primary_preflight", "recovery_preflight"}:
            path = "primary" if phase_id == "primary_preflight" else "recovery"
            prepared = [event for event in scoped if event.get("event") == "qualification_prepared"
                        and event.get("qualification_path") == path
                        and event.get("amendment_id") == plan["amendment_id"]]
            phase_evidence_sha = receipt.get("evidence_sha256")
            prepared_evidence_sha = prepared[0].get("evidence_sha256") if len(prepared) == 1 else None
            if (len(prepared) != 1 or not isinstance(phase_evidence_sha, str)
                    or not isinstance(prepared_evidence_sha, str)):
                _append_program_terminal(ledger, state, "qualification_evidence_invalid_after_supervision")
                _replace_process_budget(ledger, state)
                raise GateError("worker qualification evidence was not bound to its supervised phase receipt")
            _append_continuation_event(ledger, state, {
                "event": "qualified", "protocol_sha256": protocol_sha,
                "amendment_id": plan["amendment_id"], "qualification_path": path,
                "preflight_evidence_sha256": prepared_evidence_sha,
                "coordinator_phase_receipt_sha256": phase_event["receipt_sha256"],
                "coordinator_phase_input_evidence_sha256": phase_evidence_sha,
            })
            state["qualification_status"] = "qualified"
            if path == "primary":
                for run_id in plan["slots"]["recovery_preflight"]:
                    _append_continuation_event(ledger, state, {
                        "event": "recovery_retired", "protocol_sha256": protocol_sha,
                        "amendment_id": plan["amendment_id"], "run_id": run_id,
                        "charged_cpu_seconds": 0, "charged_wall_seconds": 0,
                        "reason": "primary_preflight_qualified_after_supervision",
                    })
        _replace_process_budget(ledger, state)
        if reason:
            raise GateError(reason)
        return payload


def _ensure_continuation_started(directory: Path, state: dict, plan: dict, protocol_sha: str,
                                *, start_monotonic_ns: int | None = None) -> None:
    if state.get("overall_start_monotonic_ns") is not None:
        return
    started = time.monotonic_ns() if start_monotonic_ns is None else start_monotonic_ns
    boot_id = _kernel_boot_id()
    deadline = started + int(plan["overall_deadline_seconds"] * 1_000_000_000)
    payload = {"event": "program_started", "protocol_sha256": protocol_sha,
               "amendment_id": plan["amendment_id"], "start_monotonic_ns": started,
               "deadline_monotonic_ns": deadline, "boot_id": boot_id,
               "deadline_seconds": plan["overall_deadline_seconds"]}
    _append_continuation_event(directory, state, payload)
    state.update({"overall_start_monotonic_ns": started,
                  "overall_deadline_monotonic_ns": deadline, "kernel_boot_id": boot_id})


def _phase_id_for_state(plan: dict, state_name: str) -> str:
    mapping = {"primary_preflight": "primary_preflight", "recovery": "recovery_preflight",
               "campaign": "campaign_orchestration", "final_scoring": "final_scoring_orchestration"}
    try:
        return mapping[state_name]
    except KeyError as error:
        raise GateError(f"no coordinator phase may begin while continuation is {state_name}") from error


def _record_coordinator_admission_failure(directory: Path, requested_phase: str,
                                         reason: str,
                                         worker_identity: dict | None = None) -> None:
    """Persist any failed phase admission/worker start as terminal, never recovery."""
    _, config, protocol_sha = read_frozen_program(directory)
    plan = config.get("continuation_plan")
    if not isinstance(plan, dict):
        return
    ledger = config["ledger_dir"]
    lock_fd = os.open(ledger / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state = json.loads((ledger / "process-budget.json").read_text())
        events = _verify_process_ledger(ledger, state, protocol_sha, plan)
        if state.get("terminal_status") is not None:
            return
        scoped = [event for event in events if event.get("protocol_sha256") == protocol_sha]
        starts = [event for event in scoped if event.get("event") == "coordinator_phase_started"]
        closes = {event.get("token") for event in scoped if event.get("event") == "coordinator_phase"}
        active = next((event for event in reversed(starts) if event.get("token") not in closes), None)
        if active is not None:
            owns_active = (isinstance(worker_identity, dict)
                           and worker_identity.get("pid") == active.get("pid")
                           and worker_identity.get("pid_starttime_ticks") == active.get("pid_starttime_ticks")
                           and worker_identity.get("token") == active.get("token"))
            if not owns_active:
                _append_continuation_event(ledger, state, {
                    "event": "coordinator_admission_attempt_refused",
                    "protocol_sha256": protocol_sha,
                    "amendment_id": plan["amendment_id"],
                    "active_phase_token": active["token"],
                    "requested_phase_id": requested_phase,
                    "requester_pid": os.getpid(),
                    "requester_pid_starttime_ticks": _process_starttime_ticks(os.getpid()),
                    "reason": reason,
                })
                _replace_process_budget(ledger, state)
                return
            receipt = {
                "schema": 1, "protocol_sha256": protocol_sha,
                "amendment_id": plan["amendment_id"], "phase_id": active["phase_id"],
                "token": active["token"], "pid": active["pid"],
                "pid_starttime_ticks": active.get("pid_starttime_ticks"),
                "status": "failed",
                "measurement_method": "FRESH_WORKER_RUSAGE_SELF_AND_PROC_VM_HWM_RESIDUAL_WALL",
                "coordinator_cpu_seconds": None, "coordinator_exclusive_wall_seconds": None,
                "coordinator_peak_rss_bytes": None, "coordinator_proc_vm_hwm_bytes": None,
                "coordinator_memory_scope": "single_fresh_worker_process",
                "memory_measurement_status": "unavailable_after_worker_failure",
                "failure_category": "coordinator_phase_failure", "reason": reason,
            }
            receipt_dir = ledger / "coordinator-phases"
            receipt_dir.mkdir(exist_ok=True)
            receipt_path = receipt_dir / f"{active['phase_id']}-{active['token']}.json"
            write_exclusive(receipt_path, canonical_json(receipt))
            _append_continuation_event(ledger, state, {
                "event": "coordinator_phase", "protocol_sha256": protocol_sha,
                "amendment_id": plan["amendment_id"], "phase_id": active["phase_id"],
                "token": active["token"], "status": "failed",
                "pid": active["pid"], "pid_starttime_ticks": active.get("pid_starttime_ticks"),
                "receipt_path": str(receipt_path.resolve()),
                "receipt_sha256": sha256_file(receipt_path),
                "coordinator_cpu_seconds": None, "coordinator_exclusive_wall_seconds": None,
                "coordinator_peak_rss_bytes": None, "coordinator_proc_vm_hwm_bytes": None,
                "coordinator_memory_scope": "single_fresh_worker_process",
                "failure_category": "coordinator_phase_failure", "reason": reason,
            })
        else:
            try:
                action = continuation_next_action(plan, scoped)
                expected_phase = _phase_id_for_state(plan, action["state"])
            except GateError:
                expected_phase = requested_phase
            _append_continuation_event(ledger, state, {
                "event": "admission_refused", "protocol_sha256": protocol_sha,
                "amendment_id": plan["amendment_id"], "phase_id": expected_phase,
                "requested_phase_id": requested_phase,
                "failure_category": "admission_refused", "reason": reason,
            })
        terminal = "coordinator_admission_refused" if active is None else "coordinator_worker_failed"
        _append_program_terminal(ledger, state, terminal)
        state["active_coordinator_phase"] = None
        _replace_process_budget(ledger, state)


def begin_coordinator_phase(directory: Path, phase_id: str) -> dict:
    """Admit a phase or immutably terminalize an admission refusal."""
    try:
        return _begin_coordinator_phase(directory, phase_id)
    except BaseException as error:
        try:
            _record_coordinator_admission_failure(
                directory, phase_id, f"{type(error).__name__}: {error}")
        except BaseException as record_error:
            raise GateError(f"coordinator admission failed ({error}); refusal accounting failed ({record_error})") from error
        raise


def _begin_coordinator_phase(directory: Path, phase_id: str) -> dict:
    """Open a measured coordinator phase; callers must finish it exactly once."""
    phase_cpu_start = _self_cpu_seconds()
    phase_mono_start = time.monotonic_ns()
    if (_COORDINATOR_ENTRY_BASELINE is not None
            and _COORDINATOR_ENTRY_BASELINE[0] == os.getpid()):
        _, phase_cpu_start, phase_mono_start = _COORDINATOR_ENTRY_BASELINE
    _, config, protocol_sha = read_frozen_program(directory)
    plan = config.get("continuation_plan")
    if plan is None or phase_id not in plan["coordinator_phase_ids"]:
        raise GateError("coordinator phase is not declared by this continuation")
    _enforce_coordinator_address_space(CONTINUATION_MEMORY_BYTES)
    start_ru_maxrss, start_vm_hwm, start_vm_rss = _coordinator_memory_measurements()
    _require_coordinator_vm_hwm_within_limit(start_vm_hwm)
    ledger = config["ledger_dir"]
    lock_fd = os.open(ledger / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state_path = ledger / "process-budget.json"
        state = json.loads(state_path.read_text())
        events = _verify_process_ledger(ledger, state, protocol_sha, plan)
        pevents = [event for event in events if event.get("protocol_sha256") == protocol_sha]
        if _pending_coordinator_supervision(pevents):
            raise GateError("prior coordinator worker is awaiting parent resource measurement")
        if state.get("terminal_status") is not None or state.get("active_coordinator_phase") is not None:
            raise GateError("continuation is terminal or already has an open coordinator phase")
        action = validate_continuation_events(plan, pevents)
        expected = _phase_id_for_state(plan, action["state"])
        if phase_id != expected:
            raise GateError(f"coordinator phase {phase_id} cannot start while continuation requires {expected}")
        _ensure_continuation_started(ledger, state, plan, protocol_sha,
                                     start_monotonic_ns=phase_mono_start)
        now_ns = phase_mono_start
        if now_ns >= state["overall_deadline_monotonic_ns"]:
            _append_program_terminal(ledger, state, "overall_wall_deadline")
            raise GateError("continuation overall hard wall deadline expired")
        token = uuid.uuid4().hex
        start_ru_maxrss, start_vm_hwm, start_vm_rss = _coordinator_memory_measurements()
        _require_coordinator_vm_hwm_within_limit(start_vm_hwm)
        start = {"event": "coordinator_phase_started", "protocol_sha256": protocol_sha,
                 "amendment_id": plan["amendment_id"], "phase_id": phase_id,
                 "token": token, "pid": os.getpid(), "boot_id": _kernel_boot_id(),
                 "start_monotonic_ns": now_ns, "start_cpu_seconds": phase_cpu_start,
                 "pid_starttime_ticks": _process_starttime_ticks(os.getpid()),
                 "start_rusage_self_ru_maxrss_diagnostic_bytes": start_ru_maxrss,
                 "start_vm_hwm_bytes": start_vm_hwm, "start_vm_rss_bytes": start_vm_rss,
                 "memory_measurement_scope": "fresh_coordinator_worker_process"}
        _append_continuation_event(ledger, state, start)
        state["active_coordinator_phase"] = {
            "token": token, "phase_id": phase_id, "pid": os.getpid(),
            "pid_starttime_ticks": start["pid_starttime_ticks"],
            "start_monotonic_ns": now_ns, "start_cpu_seconds": start["start_cpu_seconds"],
            "boot_id": start["boot_id"],
        }
        _replace_process_budget(ledger, state)
        return {key: start[key] for key in ("token", "phase_id", "pid", "boot_id", "start_monotonic_ns", "start_cpu_seconds")}


@contextmanager
def coordinator_phase_guard(directory: Path, token: str):
    """Enforce the remaining coordinator CPU and exclusive-wall allowance.

    The real-time timer is paused only while a separately bounded worker is
    running. Its wall span remains charged to the overall deadline and the
    worker's own frozen wall budget.
    """
    global _ACTIVE_COORDINATOR_GUARD
    if threading.current_thread() is not threading.main_thread():
        raise GateError("coordinator resource timers require the main thread")
    if _ACTIVE_COORDINATOR_GUARD is not None:
        raise GateError("nested coordinator phase guards are forbidden")
    _, config, protocol_sha = read_frozen_program(directory)
    plan = config.get("continuation_plan")
    if plan is None:
        raise GateError("coordinator phase guard requires an amended continuation")
    ledger = config["ledger_dir"]
    state = json.loads((ledger / "process-budget.json").read_text())
    active = state.get("active_coordinator_phase")
    if not isinstance(active, dict) or active.get("token") != token or active.get("pid") != os.getpid():
        raise GateError("coordinator guard token does not match the active phase")
    events = [json.loads(line) for line in (ledger / "process-events.jsonl").read_text().splitlines()]
    phase = active["phase_id"]
    phase_runs = {"primary_preflight": set(plan["slots"]["primary_preflight"]),
                  "recovery_preflight": set(plan["slots"]["recovery_preflight"]),
                  "campaign_orchestration": set(plan["slots"]["campaign"]),
                  "final_scoring_orchestration": set(plan["slots"]["final_scoring"])}[phase]
    run_cpu = sum(float(event["coordinator_cpu_seconds"]) for event in events
                  if event.get("event") == "coordinator_accounted" and event.get("run_id") in phase_runs)
    run_wall = sum(float(event["coordinator_exclusive_wall_seconds"]) for event in events
                   if event.get("event") == "coordinator_accounted" and event.get("run_id") in phase_runs)
    worker_wall = sum(float(event["worker_span_seconds"]) for event in events
                      if event.get("event") == "completed" and event.get("run_id") in phase_runs
                      and isinstance(event.get("worker_span_seconds"), (int, float)))
    elapsed = (time.monotonic_ns() - int(active["start_monotonic_ns"])) / 1e9
    phase_cpu_so_far = _self_cpu_seconds() - float(active["start_cpu_seconds"]) - run_cpu
    phase_wall_so_far = elapsed - run_wall - worker_wall
    cpu_remaining = float(state["max_coordinator_cpu_seconds"]) - float(state["coordinator_cpu_seconds_charged"]) - max(0.0, phase_cpu_so_far)
    wall_remaining = float(state["max_coordinator_exclusive_wall_seconds"]) - float(state["coordinator_exclusive_wall_seconds_charged"]) - max(0.0, phase_wall_so_far)
    overall_remaining = ((int(state["overall_deadline_monotonic_ns"]) - time.monotonic_ns()) / 1e9
                         if state.get("overall_deadline_monotonic_ns") is not None else 0.0)
    if cpu_remaining <= 0 or wall_remaining <= 0 or overall_remaining <= 0:
        raise GateError("coordinator resource or overall deadline allowance is exhausted")
    prior_real_timer = signal.getitimer(signal.ITIMER_REAL)
    prior_profile_timer = signal.getitimer(signal.ITIMER_PROF)
    if prior_real_timer != (0.0, 0.0) or prior_profile_timer != (0.0, 0.0):
        raise GateError("cannot safely install coordinator timers over an existing process timer")
    old_real_handler = signal.getsignal(signal.SIGALRM)
    old_profile_handler = signal.getsignal(signal.SIGPROF)

    def exceeded(signum, _frame):
        label = "exclusive wall" if signum == signal.SIGALRM else "CPU"
        raise CoordinatorResourceExceeded(f"coordinator {label} allowance exhausted")

    signal.signal(signal.SIGALRM, exceeded)
    signal.signal(signal.SIGPROF, exceeded)
    wall_timer = min(wall_remaining, overall_remaining)
    signal.setitimer(signal.ITIMER_REAL, wall_timer)
    signal.setitimer(signal.ITIMER_PROF, cpu_remaining)
    guard = {"token": token, "wall_remaining": wall_timer, "wall_timer_active": True,
             "overall_deadline_monotonic_ns": state["overall_deadline_monotonic_ns"]}
    _ACTIVE_COORDINATOR_GUARD = guard
    try:
        yield
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0.0)
        signal.setitimer(signal.ITIMER_PROF, 0.0)
        signal.signal(signal.SIGALRM, old_real_handler)
        signal.signal(signal.SIGPROF, old_profile_handler)
        _ACTIVE_COORDINATOR_GUARD = None
_COORDINATOR_ENTRY_BASELINE: tuple[int, float, int] | None = None


def register_coordinator_entry_baseline(cpu_seconds: float, monotonic_ns: int) -> None:
    """Include fresh-worker script startup/import work in its phase accounting."""
    global _COORDINATOR_ENTRY_BASELINE
    if (isinstance(cpu_seconds, bool) or not isinstance(cpu_seconds, (int, float))
            or not math.isfinite(cpu_seconds) or cpu_seconds < 0
            or type(monotonic_ns) is not int or monotonic_ns <= 0):
        raise GateError("coordinator entry baseline is invalid")
    _COORDINATOR_ENTRY_BASELINE = (os.getpid(), float(cpu_seconds), monotonic_ns)


@contextmanager
def _pause_coordinator_exclusive_wall_timer():
    guard = _ACTIVE_COORDINATOR_GUARD
    if guard is None:
        yield
        return
    if not guard["wall_timer_active"]:
        raise GateError("coordinator exclusive-wall timer is already paused")
    remaining, interval = signal.getitimer(signal.ITIMER_REAL)
    signal.setitimer(signal.ITIMER_REAL, 0.0)
    guard["wall_remaining"] = remaining
    guard["wall_timer_active"] = False
    try:
        yield
    finally:
        overall = (int(guard["overall_deadline_monotonic_ns"]) - time.monotonic_ns()) / 1e9
        if overall <= 0:
            raise CoordinatorResourceExceeded("continuation overall wall deadline expired during worker execution")
        resume = min(float(guard["wall_remaining"]), overall)
        if resume <= 0:
            raise CoordinatorResourceExceeded("coordinator exclusive-wall allowance exhausted")
        signal.setitimer(signal.ITIMER_REAL, resume)
        guard["wall_timer_active"] = True


def finish_coordinator_phase(directory: Path, token: str, *, status: str,
                            evidence_sha256: str | None = None, reason: str | None = None,
                            failure_category: str | None = None) -> dict:
    """Close a phase and charge only residual coordinator CPU/wall, once."""
    if status not in {"completed", "failed"}:
        raise GateError("coordinator phase status must be completed or failed")
    if evidence_sha256 is not None:
        _require_sha256(evidence_sha256, "coordinator phase evidence_sha256")
    _, config, protocol_sha = read_frozen_program(directory)
    plan = config.get("continuation_plan")
    if plan is None:
        raise GateError("coordinator phases require an amended continuation")
    ledger = config["ledger_dir"]
    lock_fd = os.open(ledger / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state_path = ledger / "process-budget.json"
        state = json.loads(state_path.read_text())
        events = _verify_process_ledger(ledger, state, protocol_sha, plan)
        active = state.get("active_coordinator_phase")
        if not isinstance(active, dict) or active.get("token") != token:
            raise GateError("coordinator phase token is missing, stale, or mismatched")
        if (active.get("pid") != os.getpid() or active.get("boot_id") != _kernel_boot_id()
                or active.get("pid_starttime_ticks") != _process_starttime_ticks(os.getpid())):
            _append_program_terminal(ledger, state, "coordinator_measurement_scope_changed")
            raise GateError("coordinator phase must finish in its originating process and kernel boot")
        now_ns = time.monotonic_ns()
        raw_cpu = _self_cpu_seconds() - float(active["start_cpu_seconds"])
        raw_wall = (now_ns - int(active["start_monotonic_ns"])) / 1_000_000_000.0
        failure_category = None if status == "completed" else (failure_category or "stage_failure")
        phase = active["phase_id"]
        phase_runs = {"primary_preflight": set(plan["slots"]["primary_preflight"]),
                      "recovery_preflight": set(plan["slots"]["recovery_preflight"]),
                      "campaign_orchestration": set(plan["slots"]["campaign"]),
                      "final_scoring_orchestration": set(plan["slots"]["final_scoring"])}[phase]
        completed = [event for event in events if event.get("event") == "completed"
                     and event.get("protocol_sha256") == protocol_sha and event.get("run_id") in phase_runs]
        run_ids = {event.get("run_id") for event in completed}
        worker_wall = sum(float(event["worker_span_seconds"]) for event in completed
                          if isinstance(event.get("worker_span_seconds"), (int, float))
                          and math.isfinite(float(event["worker_span_seconds"])))
        coordinator_runs = [event for event in events if event.get("event") == "coordinator_accounted"
                            and event.get("protocol_sha256") == protocol_sha
                            and event.get("run_id") in phase_runs]
        prior_run_cpu = sum(float(event["coordinator_cpu_seconds"]) for event in coordinator_runs)
        prior_run_wall = sum(float(event["coordinator_exclusive_wall_seconds"]) for event in coordinator_runs)
        phase_cpu = raw_cpu - prior_run_cpu
        phase_wall = raw_wall - worker_wall - prior_run_wall
        measurement_ok = all(math.isfinite(value) and value >= -1e-6
                             for value in (raw_cpu, raw_wall, phase_cpu, phase_wall))
        phase_cpu = max(0.0, phase_cpu) if measurement_ok else None
        phase_wall = max(0.0, phase_wall) if measurement_ok else None
        try:
            ru_maxrss_diagnostic, peak_vm_hwm, current_vm_rss = _coordinator_memory_measurements()
            peak_rss = peak_vm_hwm
            memory_measurement_ok = True
        except GateError as error:
            ru_maxrss_diagnostic = peak_rss = peak_vm_hwm = current_vm_rss = None
            memory_measurement_ok = False
            status, reason = "failed", str(error)
        if memory_measurement_ok and peak_vm_hwm > CONTINUATION_MEMORY_BYTES:
            status, reason = "failed", "coordinator worker observed VmHWM exceeded 512 MiB"
            failure_category = "coordinator_phase_failure"
        if phase in {"primary_preflight", "recovery_preflight"}:
            expected_runs = (plan["slots"]["primary_preflight"] if phase == "primary_preflight"
                             else plan["slots"]["recovery_preflight"])
            prepared = any(event.get("event") == "qualification_prepared"
                           and event.get("protocol_sha256") == protocol_sha
                           and event.get("qualification_path") == ("primary" if phase == "primary_preflight" else "recovery")
                           for event in events)
            if status == "completed" and run_ids != set(expected_runs):
                status, reason = "failed", "preflight stage set was incomplete"
                failure_category = "stage_failure"
            elif status == "completed" and any(event.get("status") != "completed" for event in completed):
                status, reason = "failed", "preflight stage failed"
                failure_category = "stage_failure"
            elif status == "completed" and not prepared:
                status, reason = "failed", "preflight outputs did not satisfy the frozen qualification contract"
                failure_category = "qualification_failure"
        if phase == "campaign_orchestration" and status == "completed" and run_ids != phase_runs:
            status, reason = "failed", "campaign stage outputs are incomplete"
            failure_category = "stage_failure"
        if phase == "final_scoring_orchestration" and status == "completed" and run_ids != phase_runs:
            status, reason = "failed", "final scoring stage outputs are incomplete"
            failure_category = "stage_failure"
        if not measurement_ok or phase_cpu is None or phase_wall is None or not memory_measurement_ok:
            status, reason = "failed", "coordinator phase resource measurement unavailable"
            failure_category = "coordinator_phase_failure"
        coordinator_cpu_total = float(state["coordinator_cpu_seconds_charged"]) + (phase_cpu or 0.0)
        coordinator_wall_total = float(state["coordinator_exclusive_wall_seconds_charged"]) + (phase_wall or 0.0)
        if (coordinator_cpu_total > state["max_coordinator_cpu_seconds"]
                or coordinator_wall_total > state["max_coordinator_exclusive_wall_seconds"]):
            status, reason = "failed", "coordinator phase allowance exceeded"
            failure_category = "coordinator_phase_failure"
        if status == "failed" and failure_category not in {
                "stage_failure", "qualification_failure", "coordinator_phase_failure"}:
            failure_category = "coordinator_phase_failure"
        receipt = {"schema": 1, "protocol_sha256": protocol_sha, "amendment_id": plan["amendment_id"],
                   "phase_id": phase, "token": token, "pid": os.getpid(), "status": status,
                   "measurement_method": "FRESH_WORKER_RUSAGE_SELF_AND_PROC_VM_HWM_RESIDUAL_WALL",
                   "raw_cpu_seconds": raw_cpu if measurement_ok else None,
                   "worker_cpu_already_charged": prior_run_cpu,
                   "coordinator_cpu_seconds": phase_cpu, "raw_wall_seconds": raw_wall if measurement_ok else None,
                   "worker_span_seconds_already_charged": worker_wall,
                   "per_run_coordinator_wall_already_charged": prior_run_wall,
                   "coordinator_exclusive_wall_seconds": phase_wall,
                   "coordinator_peak_rss_bytes": peak_rss,
                   "coordinator_proc_vm_hwm_bytes": peak_vm_hwm,
                   "coordinator_rusage_self_ru_maxrss_diagnostic_bytes": ru_maxrss_diagnostic,
                   "coordinator_proc_vm_rss_bytes": current_vm_rss,
                   "coordinator_memory_scope": "single_fresh_worker_process",
                   "coordinator_pid_starttime_ticks": active.get("pid_starttime_ticks"),
                   "memory_measurement_status": "measured" if memory_measurement_ok else "unavailable",
                   "failure_category": failure_category,
                   "completed_run_ids": sorted(run_ids), "evidence_sha256": evidence_sha256,
                   "reason": reason, "measured_at_monotonic_ns": now_ns}
        receipt_dir = ledger / "coordinator-phases"
        receipt_dir.mkdir(exist_ok=True)
        receipt_path = receipt_dir / f"{phase}-{token}.json"
        write_exclusive(receipt_path, canonical_json(receipt))
        phase_event = {"event": "coordinator_phase", "protocol_sha256": protocol_sha,
                       "amendment_id": plan["amendment_id"], "phase_id": phase, "token": token,
                       "pid": os.getpid(), "pid_starttime_ticks": active.get("pid_starttime_ticks"),
                       "status": status, "receipt_path": str(receipt_path.resolve()),
                       "receipt_sha256": sha256_file(receipt_path),
                       "coordinator_cpu_seconds": phase_cpu,
                       "coordinator_exclusive_wall_seconds": phase_wall,
                       "coordinator_peak_rss_bytes": peak_rss,
                       "coordinator_proc_vm_hwm_bytes": peak_vm_hwm,
                       "coordinator_rusage_self_ru_maxrss_diagnostic_bytes": ru_maxrss_diagnostic,
                       "coordinator_proc_vm_rss_bytes": current_vm_rss,
                       "coordinator_memory_scope": "single_fresh_worker_process",
                       "failure_category": failure_category,
                       "evidence_sha256": evidence_sha256, "reason": reason}
        _append_continuation_event(ledger, state, phase_event)
        state["active_coordinator_phase"] = None
        if phase_cpu is not None and phase_wall is not None:
            state["coordinator_cpu_seconds_charged"] = coordinator_cpu_total
            state["coordinator_exclusive_wall_seconds_charged"] = coordinator_wall_total
        if (status == "failed" and phase == "primary_preflight"
                and failure_category not in {"stage_failure", "qualification_failure"}):
            _append_program_terminal(ledger, state, "primary_preflight_phase_failed")
        elif (status == "failed" and phase == "recovery_preflight"
              and failure_category not in {"stage_failure", "qualification_failure"}):
            _append_program_terminal(ledger, state, "recovery_preflight_phase_failed")
        elif status == "failed" and phase not in {"primary_preflight", "recovery_preflight"}:
            _append_program_terminal(ledger, state, f"{phase}_failed")
        if now_ns >= state["overall_deadline_monotonic_ns"]:
            _append_program_terminal(ledger, state, "overall_wall_deadline")
        _replace_process_budget(ledger, state)
        if status != "completed":
            raise GateError(reason or f"coordinator phase {phase} failed")
        return receipt


@contextmanager
def measured_coordinator_phase(directory: Path, phase_id: str):
    """Convenience wrapper for pinned coordinators around campaign/final work."""
    token = begin_coordinator_phase(directory, phase_id)
    try:
        with coordinator_phase_guard(directory, token["token"]):
            yield token
        finish_coordinator_phase(directory, token["token"], status="completed")
    except BaseException as error:
        try:
            finish_coordinator_phase(directory, token["token"], status="failed",
                                     reason=f"{type(error).__name__}: {error}")
        except BaseException as accounting_error:
            raise GateError(f"phase failure and accounting failure: {error}; {accounting_error}") from error
        raise


def _require_receipt_measurements(receipt: dict, event: dict,
                                 fields: tuple[str | tuple[str, str], ...]) -> None:
    """Bind ledgered resource values to their immutable receipt payload."""
    for mapping in fields:
        receipt_field, event_field = mapping if isinstance(mapping, tuple) else (mapping, mapping)
        receipt_value, event_value = receipt.get(receipt_field), event.get(event_field)
        if receipt_field.endswith("_bytes") or event_field.endswith("_bytes"):
            valid = (type(receipt_value) is int and receipt_value >= 0
                     and type(event_value) is int and event_value >= 0)
        else:
            valid = (not isinstance(receipt_value, bool) and isinstance(receipt_value, (int, float))
                     and math.isfinite(float(receipt_value)) and float(receipt_value) >= 0
                     and not isinstance(event_value, bool) and isinstance(event_value, (int, float))
                     and math.isfinite(float(event_value)) and float(event_value) >= 0)
        if not valid or receipt_value != event_value:
            raise GateError(f"immutable coordinator receipt disagrees with ledger measurement {event_field}")


def execute_continuation_phase(directory: Path, phase_id: str) -> dict:
    """Execute only the frozen campaign or final-stage slots under one guard."""
    _, config, protocol_sha = read_frozen_program(directory)
    plan = config.get("continuation_plan")
    if plan is None:
        raise GateError("continuation phase execution requires an amended protocol")
    slots_by_phase = {"campaign_orchestration": "campaign",
                      "final_scoring_orchestration": "final_scoring"}
    if phase_id not in slots_by_phase:
        raise GateError("execute_continuation_phase handles campaign/final phases only")
    run_ids = plan["slots"][slots_by_phase[phase_id]]
    token = begin_coordinator_phase(directory, phase_id)
    receipts = []
    try:
        with coordinator_phase_guard(directory, token["token"]):
            for run_id in run_ids:
                result = run_program(directory, run_id)
                if result.get("status") != "completed":
                    raise GateError(f"continuation stage {run_id} ended with {result.get('status')}")
                receipt_path = directory / "runs" / run_id / "receipt.json"
                receipts.append({"run_id": run_id, "receipt_sha256": sha256_file(receipt_path),
                                 "output_slots": result.get("output_slots", {})})
        evidence = sha256_bytes(canonical_json({"protocol_sha256": protocol_sha,
                                                "phase_id": phase_id, "receipts": receipts}))
        phase_receipt = finish_coordinator_phase(directory, token["token"], status="completed",
                                                 evidence_sha256=evidence)
        return {"status": "completed", "phase_id": phase_id,
                "completed_stage_count": len(receipts), "evidence_sha256": evidence,
                "coordinator_receipt_sha256": sha256_bytes(canonical_json(phase_receipt)),
                "coordinator_phase": {
                    "token": phase_receipt["token"], "phase_id": phase_receipt["phase_id"],
                    "pid": phase_receipt["pid"],
                    "pid_starttime_ticks": phase_receipt["coordinator_pid_starttime_ticks"],
                    "worker_self_peak_rss_bytes": phase_receipt["coordinator_peak_rss_bytes"],
                    "worker_self_vm_hwm_bytes": phase_receipt["coordinator_proc_vm_hwm_bytes"],
                    "status": phase_receipt["status"],
                }}
    except BaseException as error:
        try:
            finish_coordinator_phase(directory, token["token"], status="failed",
                                     reason=f"{type(error).__name__}: {error}")
        except BaseException as accounting_error:
            raise GateError(f"phase failure and accounting failure: {error}; {accounting_error}") from error
        raise


def _read_process_ledger_events(directory: Path) -> list[dict]:
    """Read the append-only event chain, inserting its atomic successor record."""
    raw_events = [json.loads(line) for line in (directory / "process-events.jsonl").read_bytes().splitlines()]
    successor_path = directory / "protocol-successor.json"
    successor = json.loads(successor_path.read_text()) if successor_path.exists() else None
    if successor is not None:
        claimed = successor.pop("event_sha256", None)
        if (successor.get("event") != "protocol_successor_authorized"
                or claimed != sha256_bytes(compact_json(successor))):
            raise GateError("protocol-successor record checksum/type is invalid")
        successor["event_sha256"] = claimed
        event_index = successor.get("event_index")
        if type(event_index) is not int or event_index < 1 or event_index > len(raw_events):
            raise GateError("protocol-successor record has an invalid event position")

    events: list[dict] = []
    previous = "0" * 64
    inserted = False
    for index, raw in enumerate(raw_events):
        if successor is not None and not inserted and index == successor["event_index"]:
            if successor.get("previous_hash") != previous:
                raise GateError("protocol-successor record does not extend the ledger head")
            events.append(successor)
            previous = successor["event_sha256"]
            inserted = True
        event = dict(raw)
        claimed = event.pop("event_sha256", None)
        if event.get("previous_hash") != previous or claimed != sha256_bytes(compact_json(event)):
            raise GateError("bounded-process ledger hash-chain mismatch")
        event["event_sha256"] = claimed
        previous = claimed
        events.append(event)
    if successor is not None and not inserted:
        if successor["event_index"] != len(raw_events) or successor.get("previous_hash") != previous:
            raise GateError("protocol-successor record cannot be placed in the ledger chain")
        events.append(successor)
    return events


def _active_protocol_sha256(events: list[dict]) -> str:
    if not events or events[0].get("event") != "created":
        raise GateError("bounded-process ledger has no creation event")
    genesis = events[0].get("frozen_protocol_sha256")
    _require_sha256(genesis, "ledger creation protocol SHA256")
    successors = [event for event in events if event.get("event") == "protocol_successor_authorized"]
    if len(successors) > 1:
        raise GateError("bounded-process ledger permits only one pre-execution protocol successor")
    if not successors:
        return genesis
    successor = successors[0]
    successor_index = events.index(successor)
    prefix_names = [event.get("event") for event in events[:successor_index]]
    allowed_prefixes = (["created"], ["created", "historical_import", "amendment_authorized"])
    if (successor.get("event_index") != successor_index or prefix_names not in allowed_prefixes
            or successor.get("predecessor_protocol_sha256") != genesis
            or successor.get("previous_hash") != events[successor_index - 1].get("event_sha256")
            or successor.get("program_id") != events[0].get("program_id")):
        raise GateError("protocol successor is not the authorized pre-execution extension")
    if any(event.get("event") in {"reserved", "completed", "qualified", "program_started",
                                  "coordinator_phase_started", "coordinator_phase"}
           for event in events[:successor_index]):
        raise GateError("protocol successor cannot follow execution or phase admission")
    _require_sha256(successor.get("successor_protocol_sha256"), "successor protocol SHA256")
    return successor["successor_protocol_sha256"]


def _verify_process_ledger(directory: Path, state: dict, expected_protocol_sha256: str,
                           continuation_plan: dict | None = None) -> list[dict]:
    events = _read_process_ledger_events(directory)
    if (not events or len(events) != state.get("event_count")
            or events[-1].get("event_sha256") != state.get("ledger_head")):
        raise GateError("bounded-process ledger state/head mismatch")
    genesis_sha = events[0].get("frozen_protocol_sha256") if events else None
    active_sha = _active_protocol_sha256(events)
    if (active_sha != expected_protocol_sha256
            or state.get("frozen_protocol_sha256") != active_sha):
        raise GateError("bounded-process ledger is bound to a different frozen protocol or inactive successor")
    cap_keys = (("max_new_invocations", "max_new_cpu_seconds", "max_new_wall_seconds")
                if state.get("accounting_scope") == "prospective_continuation_only"
                else ("max_lifetime_invocations", "max_lifetime_cpu_seconds", "max_lifetime_wall_seconds"))
    identity_args = {"budget": {key: state.get(key) for key in cap_keys},
        "program_id": state.get("program_id"),
        **({"continuation_plan": {
                "amendment_id": state.get("amendment_id"),
                "coordinator_allowance": {
                    "cpu_seconds": state.get("max_coordinator_cpu_seconds"),
                    "exclusive_wall_seconds": state.get("max_coordinator_exclusive_wall_seconds"),
                },
                "overall_deadline_seconds": state.get("overall_deadline_seconds"),
            }}
           if state.get("accounting_scope") == "prospective_continuation_only" else {})}
    identity = _process_ledger_identity({**identity_args, "frozen_protocol_sha256": active_sha})
    creation_identity = _process_ledger_identity({**identity_args, "frozen_protocol_sha256": genesis_sha})
    if not events or any(state.get(key) != value for key, value in identity.items()):
        raise GateError("bounded-process ledger identity mismatch")
    if events[0].get("event") != "created" or any(events[0].get(k) != v for k, v in creation_identity.items()):
        raise GateError("bounded-process ledger creation event mismatch")
    successor_events = [event for event in events if event.get("event") == "protocol_successor_authorized"]
    if successor_events:
        successor = successor_events[0]
        auth = next((event for event in events[:3] if event.get("event") == "amendment_authorized"), None)
        authorization_matches = (
            successor.get("authorization_sha256") is None and successor.get("allowance") is None
            if auth is None else
            successor.get("authorization_sha256") == auth.get("authorization_sha256")
            and successor.get("allowance") == auth.get("allowance")
        )
        if not authorization_matches:
            raise GateError("protocol successor does not preserve the recorded authorization/contract")
        _require_sha256(successor.get("compatibility_sha256"), "successor compatibility projection SHA256")
    reserved = [e for e in events if e.get("event") == "reserved"]
    completed = [e for e in events if e.get("event") == "completed"]
    reserved_ids = [e.get("invocation_id") for e in reserved]
    completed_ids = [e.get("invocation_id") for e in completed]
    if len(set(reserved_ids)) != len(reserved_ids) or len(set(completed_ids)) != len(completed_ids):
        raise GateError("duplicate invocation identity in bounded-process ledger")
    if not set(completed_ids).issubset(reserved_ids):
        raise GateError("completion event lacks a bounded-process reservation")
    unresolved = set(reserved_ids) - set(completed_ids)
    pending = next(iter(unresolved)) if len(unresolved) == 1 else None
    if len(unresolved) > 1 or state.get("pending") != pending:
        raise GateError("unresolved bounded-process reservation; fail closed")
    cpu = sum(float(e["charged_cpu_seconds"]) for e in completed)
    wall = sum(float(e["charged_wall_seconds"]) for e in completed)
    if len(reserved) != state.get("invocations_reserved") or not math.isclose(cpu, state.get("cpu_seconds_charged", -1), rel_tol=1e-12, abs_tol=1e-9) or not math.isclose(wall, state.get("wall_seconds_charged", -1), rel_tol=1e-12, abs_tol=1e-9):
        raise GateError("bounded-process counters disagree with completion ledger")
    qualification_events = [e for e in events if e.get("event") == "qualified"]
    if len(qualification_events) > 1:
        raise GateError("bounded-process preflight qualification is not one-shot")
    qualification_status = "qualified" if qualification_events else "preflight_pending"
    if state.get("qualification_status", "preflight_pending") != qualification_status:
        raise GateError("bounded-process qualification state disagrees with event ledger")
    invocation_cap, cpu_cap, wall_cap = _process_budget_caps(state)
    if state["invocations_reserved"] > invocation_cap or cpu > cpu_cap or wall > wall_cap:
        raise GateError("bounded-process authorized execution ceiling exceeded")
    if state.get("accounting_scope") == "prospective_continuation_only" and continuation_plan is None:
        raise GateError("continuation ledger verification requires the frozen continuation contract")
    if continuation_plan is not None:
        if state.get("accounting_scope") != "prospective_continuation_only" or state.get("amendment_id") != continuation_plan["amendment_id"]:
            raise GateError("continuation ledger is not bound to the authorized amendment")
        validate_continuation_events(continuation_plan, events)
        imports = [event for event in events if event.get("event") == "historical_import"]
        authorizations = [event for event in events if event.get("event") == "amendment_authorized"]
        if len(imports) != 1 or len(authorizations) != 1 or events.index(imports[0]) >= events.index(authorizations[0]):
            raise GateError("continuation ledger must contain one ordered history import and authorization")
        if imports[0].get("history") != continuation_plan["history"]:
            raise GateError("continuation historical import differs from its frozen protocol")
        if imports[0].get("source_artifact_sha256") != continuation_plan["history_artifact_sha256"]:
            raise GateError("continuation historical source artifact identity mismatch")
        if imports[0].get("history_sha256") != sha256_bytes(canonical_json(continuation_plan["history"])):
            raise GateError("continuation historical import checksum mismatch")
        if authorizations[0].get("allowance") != continuation_plan["allowance"]:
            raise GateError("continuation authorization differs from its frozen prospective allowance")
        if authorizations[0].get("authorization_sha256") != continuation_plan["authorization_sha256"]:
            raise GateError("continuation authorization evidence checksum mismatch")
        process_receipts = [event for event in events if event.get("event") == "coordinator_accounted"]
        receipt_invocations = [event.get("invocation_id") for event in process_receipts]
        unavailable_events = [event for event in events if event.get("event") == "coordinator_accounting_unavailable"]
        unavailable_ids = {event.get("invocation_id") for event in unavailable_events}
        missing_receipts = set(completed_ids) - set(receipt_invocations)
        if (len(set(receipt_invocations)) != len(receipt_invocations)
                or set(receipt_invocations) | unavailable_ids != set(completed_ids)
                or set(receipt_invocations) & unavailable_ids
                or missing_receipts != unavailable_ids
                or (unavailable_ids and state.get("terminal_status") is None)):
            raise GateError("every amended bounded process must have exactly one resource receipt or explicit terminal accounting failure")
        for receipt_event in process_receipts:
            receipt_path = Path(receipt_event.get("receipt_path", ""))
            if not receipt_path.is_file() or sha256_file(receipt_path) != receipt_event.get("receipt_sha256"):
                raise GateError("coordinator resource receipt is missing or hash-mismatched")
            receipt = json.loads(receipt_path.read_text())
            if (receipt.get("invocation_id") != receipt_event.get("invocation_id")
                    or receipt.get("run_id") != receipt_event.get("run_id")
                    or receipt.get("measurement_method") != "RUSAGE_SELF_AND_EXCLUSIVE_WALL"):
                raise GateError("coordinator resource receipt identity/method mismatch")
            _require_receipt_measurements(receipt, receipt_event,
                (("cpu_seconds", "coordinator_cpu_seconds"),
                 ("exclusive_wall_seconds", "coordinator_exclusive_wall_seconds")))
        coord_cpu = sum(float(event["coordinator_cpu_seconds"]) for event in process_receipts)
        coord_wall = sum(float(event["coordinator_exclusive_wall_seconds"]) for event in process_receipts)
        phase_receipts = [event for event in events if event.get("event") == "coordinator_phase"]
        phase_tokens = [event.get("token") for event in phase_receipts]
        if len(set(phase_tokens)) != len(phase_tokens):
            raise GateError("duplicate coordinator phase receipt token")
        for receipt_event in phase_receipts:
            cpu_value, wall_value = receipt_event.get("coordinator_cpu_seconds"), receipt_event.get("coordinator_exclusive_wall_seconds")
            if cpu_value is None or wall_value is None:
                if receipt_event.get("status") != "failed" or state.get("terminal_status") is None:
                    raise GateError("coordinator phase receipt has unavailable resource measurements")
            elif (not math.isfinite(float(cpu_value)) or not math.isfinite(float(wall_value))
                  or float(cpu_value) < 0 or float(wall_value) < 0):
                raise GateError("coordinator phase receipt has invalid resource measurements")
            receipt_path = Path(receipt_event.get("receipt_path", ""))
            if not receipt_path.is_file() or sha256_file(receipt_path) != receipt_event.get("receipt_sha256"):
                raise GateError("coordinator phase receipt is missing or hash-mismatched")
            receipt = json.loads(receipt_path.read_text())
            if (receipt.get("token") != receipt_event.get("token")
                    or receipt.get("phase_id") != receipt_event.get("phase_id")
                    or receipt.get("status") != receipt_event.get("status")):
                raise GateError("coordinator phase receipt identity/method mismatch")
            method = receipt.get("measurement_method")
            if method == "RUSAGE_SELF_RESIDUAL_AND_EXCLUSIVE_WALL_RESIDUAL":
                _require_receipt_measurements(receipt, receipt_event,
                    ("coordinator_peak_rss_bytes",) if cpu_value is None else
                    ("coordinator_cpu_seconds", "coordinator_exclusive_wall_seconds", "coordinator_peak_rss_bytes"))
            elif method == "FRESH_WORKER_RUSAGE_SELF_AND_PROC_VM_HWM_RESIDUAL_WALL":
                if receipt.get("coordinator_memory_scope") != "single_fresh_worker_process":
                    raise GateError("coordinator worker memory scope is not explicit")
                if cpu_value is None:
                    if (receipt_event.get("status") != "failed" or state.get("terminal_status") is None
                            or receipt.get("memory_measurement_status") != "unavailable"
                            or receipt.get("coordinator_peak_rss_bytes") is not None
                            or receipt_event.get("coordinator_peak_rss_bytes") is not None):
                        raise GateError("coordinator worker memory failure is not explicitly terminal")
                else:
                    _require_receipt_measurements(receipt, receipt_event,
                        ("coordinator_cpu_seconds", "coordinator_exclusive_wall_seconds",
                         "coordinator_peak_rss_bytes", "coordinator_proc_vm_hwm_bytes"))
                    vm_hwm = receipt.get("coordinator_proc_vm_hwm_bytes")
                    vm_rss = receipt.get("coordinator_proc_vm_rss_bytes")
                    ru_diagnostic = receipt.get("coordinator_rusage_self_ru_maxrss_diagnostic_bytes")
                    if (type(vm_hwm) is not int or vm_hwm <= 0
                            or vm_hwm > CONTINUATION_MEMORY_BYTES
                            or receipt.get("coordinator_peak_rss_bytes") != vm_hwm
                            or type(vm_rss) is not int or vm_rss <= 0
                            or vm_rss > CONTINUATION_MEMORY_BYTES
                            or (ru_diagnostic is not None
                                and (type(ru_diagnostic) is not int or ru_diagnostic <= 0))
                            or receipt_event.get("coordinator_proc_vm_hwm_bytes") != vm_hwm
                            or receipt_event.get("coordinator_proc_vm_rss_bytes") != vm_rss):
                        raise GateError("fresh-worker VmHWM/VmRSS cap data is invalid")
                    if (receipt.get("memory_measurement_status") != "measured"
                            or type(receipt.get("coordinator_pid_starttime_ticks")) is not int
                            or receipt.get("coordinator_pid_starttime_ticks") <= 0):
                        raise GateError("coordinator worker memory identity/measurement is incomplete")
            else:
                raise GateError("coordinator phase receipt has an unknown measurement method")
            if cpu_value is not None and wall_value is not None:
                coord_cpu += float(cpu_value)
                coord_wall += float(wall_value)
        supervised_events = [event for event in events
                             if event.get("event") == "coordinator_worker_supervised"
                             and event.get("protocol_sha256") == expected_protocol_sha256]
        supervised_tokens = [event.get("token") for event in supervised_events]
        if len(set(supervised_tokens)) != len(supervised_tokens):
            raise GateError("duplicate coordinator supervisor measurement token")
        for event in supervised_events:
            if event.get("status") != "verified":
                if state.get("terminal_status") is None:
                    raise GateError("failed coordinator supervisor measurement is not terminal")
                continue
            coord_cpu += float(event["supervisor_cpu_seconds"])
            coord_wall += float(event["supervisor_exclusive_wall_seconds"])
        if (not math.isclose(coord_cpu, state.get("coordinator_cpu_seconds_charged", -1), rel_tol=1e-12, abs_tol=1e-9)
                or not math.isclose(coord_wall, state.get("coordinator_exclusive_wall_seconds_charged", -1), rel_tol=1e-12, abs_tol=1e-9)):
            raise GateError("coordinator resource counters disagree with immutable receipt events")
        if (coord_cpu > state["max_coordinator_cpu_seconds"]
                or coord_wall > state["max_coordinator_exclusive_wall_seconds"]):
            raise GateError("coordinator overhead allowance exceeded")
        started = state.get("overall_start_monotonic_ns")
        deadline = state.get("overall_deadline_monotonic_ns")
        if (started is None) != (deadline is None) or (started is not None
                and deadline != started + int(state["overall_deadline_seconds"] * 1_000_000_000)):
            raise GateError("overall monotonic deadline state is incomplete or inconsistent")
        start_events = [event for event in events if event.get("event") == "program_started"]
        if started is None:
            if start_events:
                raise GateError("program start event exists without a deadline state")
        elif (len(start_events) != 1 or start_events[0].get("start_monotonic_ns") != started
              or start_events[0].get("deadline_monotonic_ns") != deadline
              or start_events[0].get("boot_id") != state.get("kernel_boot_id")
              or start_events[0].get("deadline_seconds") != state["overall_deadline_seconds"]):
            raise GateError("overall deadline is not bound to its immutable program-start event")
        if started is not None and state.get("kernel_boot_id") != _kernel_boot_id():
            raise GateError("continuation monotonic deadline cannot be verified after a kernel boot change")
        phase_starts = [event for event in events if event.get("event") == "coordinator_phase_started"]
        phase_closes = {event.get("token") for event in phase_receipts}
        active_starts = [event for event in phase_starts if event.get("token") not in phase_closes]
        if len(active_starts) > 1:
            raise GateError("continuation ledger contains overlapping unclosed coordinator phases")
        expected_active = None
        if active_starts:
            start_event = active_starts[0]
            expected_active = {key: start_event.get(key) for key in
                               ("token", "phase_id", "pid", "pid_starttime_ticks",
                                "start_monotonic_ns", "start_cpu_seconds", "boot_id")}
        if state.get("active_coordinator_phase") != expected_active:
            raise GateError("active coordinator phase state disagrees with immutable ledger")
        terminal_events = [event for event in events if event.get("event") == "program_terminal"]
        if len(terminal_events) > 1 or (terminal_events and state.get("terminal_status") != terminal_events[0].get("status")):
            raise GateError("continuation terminal state differs from immutable ledger")
    return events


def _initialize_process_ledger(config: dict) -> None:
    directory = config["ledger_dir"]
    directory.mkdir(parents=True, exist_ok=True)
    lock_fd = os.open(directory / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state_path = directory / "process-budget.json"
        events_path = directory / "process-events.jsonl"
        identity = _process_ledger_identity(config)
        if state_path.exists() or events_path.exists():
            if not state_path.is_file() or not events_path.is_file():
                raise GateError("bounded-process ledger is incomplete; fail closed")
            state = json.loads(state_path.read_text())
            if any(state.get(k) != v for k, v in identity.items()):
                raise GateError("bounded-process lifetime identity/limits changed")
            _verify_process_ledger(directory, state, identity["frozen_protocol_sha256"], config.get("continuation_plan"))
            return
        event = {"event": "created", **identity, "previous_hash": "0" * 64}
        event_hash = sha256_bytes(compact_json(event))
        event["event_sha256"] = event_hash
        write_exclusive(events_path, compact_json(event) + b"\n", 0o600)
        event_count = 1
        if config.get("continuation_plan") is not None:
            continuation = config["continuation_plan"]
            amendment = config["protocol"]["continuation_authorization"]
            event_hash = _process_ledger_event(events_path, {
                "event": "historical_import", "amendment_id": continuation["amendment_id"],
                "history": continuation["history"],
                "history_sha256": sha256_bytes(canonical_json(continuation["history"])),
                "source_artifact_sha256": continuation["history_artifact_sha256"],
                "imported_at_freeze": True,
            }, event_hash)
            event_count += 1
            event_hash = _process_ledger_event(events_path, {
                "event": "amendment_authorized", "amendment_id": continuation["amendment_id"],
                "authorization_artifact_id": amendment["authorization_artifact_id"],
                "authorization_sha256": config["artifacts"][amendment["authorization_artifact_id"]]["sha256"],
                "allowance": continuation["allowance"],
                "prior_declared_cap": continuation["history"]["prior_declared_invocation_cap"],
                "authorized_prospectively": True,
            }, event_hash)
            event_count += 1
            state_caps = {"accounting_scope": "prospective_continuation_only"}
        else:
            state_caps = {}
        state = {**identity, **state_caps, "invocations_reserved": 0, "cpu_seconds_charged": 0.0,
                 "wall_seconds_charged": 0.0, "pending": None, "qualification_status": "preflight_pending",
                 "event_count": event_count, "ledger_head": event_hash}
        if config.get("continuation_plan") is not None:
            state.update({"coordinator_cpu_seconds_charged": 0.0,
                          "coordinator_exclusive_wall_seconds_charged": 0.0,
                          "overall_start_monotonic_ns": None, "overall_deadline_monotonic_ns": None,
                          "kernel_boot_id": None, "terminal_status": None})
        write_exclusive(state_path, canonical_json(state), 0o600)


def freeze_program_protocol(source: Path, destination: Path) -> str:
    raw = source.read_bytes()
    protocol = json.loads(raw)
    config = _validate_bounded_program(protocol)
    if (config["ledger_dir"] / "protocol-successor.json").exists():
        raise GateError("program ledger already has an active successor; use its registered frozen package")
    _initialize_process_ledger(config)
    destination.mkdir(parents=True, exist_ok=False)
    _fsync_directory(destination.parent)
    encoded = canonical_json(protocol)
    digest = sha256_bytes(encoded)
    write_exclusive(destination / "program.json", encoded)
    write_exclusive(destination / "PROGRAM.sha256", f"{digest}  program.json\n".encode())
    _fsync_directory(destination)
    return digest


_SUCCESSOR_CODE_ROLES = {"pipeline-executable", "pipeline-dependency", "coordinator-entrypoint"}


def _successor_projection(protocol: dict) -> dict:
    """Remove only source-version and pinned software identity fields."""
    projection = copy.deepcopy(protocol)
    projection["source_commit"] = "<source-commit>"
    projection["protocol_id"] = "<protocol-version>"
    for artifact in projection.get("artifacts", []):
        if artifact.get("role") in _SUCCESSOR_CODE_ROLES:
            artifact["path"] = "<pinned-software-path>"
            artifact["sha256"] = "<pinned-software-sha256>"
    continuation = projection.get("continuation_authorization")
    if isinstance(continuation, dict) and continuation.get("schema") == 2:
        if continuation.get("coordinator_supervisor_artifact_id") == "source-supervisor":
            projection["artifacts"] = [item for item in projection.get("artifacts", [])
                                        if item.get("id") != "source-supervisor"]
        continuation["schema"] = 1
        continuation.pop("coordinator_supervisor_artifact_id", None)
    projection["artifacts"] = sorted(projection.get("artifacts", []), key=lambda item: item.get("id", ""))
    return projection


def _validate_protocol_successor(predecessor: dict, successor: dict,
                                 predecessor_config: dict, successor_config: dict) -> str:
    """Require a pre-execution, software-identity-only protocol successor."""
    if predecessor_config["program_id"] != successor_config["program_id"]:
        raise GateError("protocol successor must retain the same persistent program identity")
    if predecessor_config["ledger_dir"] != successor_config["ledger_dir"]:
        raise GateError("protocol successor must retain the canonical lifetime ledger")
    if not isinstance(predecessor.get("source_commit"), str) or not isinstance(successor.get("source_commit"), str):
        raise GateError("protocol successor source commit identity is missing")
    if predecessor["source_commit"] == successor["source_commit"]:
        raise GateError("protocol successor must pin the repaired source commit")
    if predecessor.get("protocol_id") == successor.get("protocol_id"):
        raise GateError("protocol successor must use a new protocol version identity")

    previous_amendment = predecessor.get("continuation_authorization")
    next_amendment = successor.get("continuation_authorization")
    if isinstance(previous_amendment, dict):
        if (previous_amendment.get("schema") != 1 or not isinstance(next_amendment, dict)
                or next_amendment.get("schema") != 2
                or next_amendment.get("coordinator_supervisor_artifact_id") != "source-supervisor"):
            raise GateError("continuation protocol successor must add the qualified supervisor binding")

    old_artifacts = {item["id"]: item for item in predecessor.get("artifacts", [])}
    new_artifacts = {item["id"]: item for item in successor.get("artifacts", [])}
    added = set(new_artifacts) - set(old_artifacts)
    removed = set(old_artifacts) - set(new_artifacts)
    if removed or (added and added != {"source-supervisor"}):
        raise GateError("protocol successor may add only the qualified coordinator supervisor identity")
    if added:
        extra = new_artifacts["source-supervisor"]
        if extra.get("role") != "coordinator-entrypoint":
            raise GateError("the new supervisor binding must be a coordinator entrypoint")

    if _successor_projection(predecessor) != _successor_projection(successor):
        raise GateError("protocol successor changes frozen science, data, phase plan, criteria, or resource limits")
    compatibility = sha256_bytes(canonical_json(_successor_projection(predecessor)))
    return compatibility


def _ensure_unstarted_ledger(state: dict, events: list[dict]) -> None:
    allowed_events = ["created"]
    if state.get("accounting_scope") == "prospective_continuation_only":
        allowed_events.extend(["historical_import", "amendment_authorized"])
    if [event.get("event") for event in events] != allowed_events:
        raise GateError("protocol succession is allowed only before any admission or execution event")
    if (state.get("invocations_reserved") != 0 or state.get("pending") is not None
            or state.get("qualification_status") != "preflight_pending"
            or state.get("active_coordinator_phase") is not None
            or state.get("terminal_status") is not None
            or state.get("overall_start_monotonic_ns") is not None
            or state.get("overall_deadline_monotonic_ns") is not None
            or state.get("cpu_seconds_charged") != 0
            or state.get("wall_seconds_charged") != 0
            or state.get("coordinator_cpu_seconds_charged", 0) != 0
            or state.get("coordinator_exclusive_wall_seconds_charged", 0) != 0):
        raise GateError("protocol succession requires an unused ledger with no active or reserved phase")


def _verify_unstarted_predecessor(state: dict, events: list[dict], config: dict,
                                  protocol_sha256: str) -> None:
    """Verify the complete pre-execution ledger prefix before successor recovery."""
    _ensure_unstarted_ledger(state, events)
    if (not events or state.get("event_count") != len(events)
            or state.get("ledger_head") != events[-1].get("event_sha256")):
        raise GateError("predecessor ledger prefix does not match its state cache")
    plan = config.get("continuation_plan")
    identity = _process_ledger_identity({
        "budget": config["budget"], "program_id": config["program_id"],
        "frozen_protocol_sha256": protocol_sha256, "continuation_plan": plan,
    })
    if any(state.get(key) != value for key, value in identity.items()):
        raise GateError("predecessor ledger identity differs from its frozen protocol")
    if events[0].get("event") != "created" or any(events[0].get(key) != value for key, value in identity.items()):
        raise GateError("predecessor ledger creation identity is invalid")
    if plan is not None:
        imports = [event for event in events if event.get("event") == "historical_import"]
        authorizations = [event for event in events if event.get("event") == "amendment_authorized"]
        if (len(imports) != 1 or len(authorizations) != 1
                or imports[0].get("history") != plan["history"]
                or imports[0].get("history_sha256") != sha256_bytes(canonical_json(plan["history"]))
                or imports[0].get("source_artifact_sha256") != plan["history_artifact_sha256"]
                or authorizations[0].get("allowance") != plan["allowance"]
                or authorizations[0].get("authorization_sha256") != plan["authorization_sha256"]):
            raise GateError("predecessor authorization/history events do not match the frozen amendment")


def _atomic_write_successor_record(path: Path, event: dict) -> None:
    encoded = canonical_json(event)
    temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    write_exclusive(temporary, encoded, 0o600)
    try:
        os.link(temporary, path)
        _fsync_directory(path.parent)
    except FileExistsError:
        if path.read_bytes() != encoded:
            raise GateError("an incompatible protocol successor record already exists")
    finally:
        temporary.unlink(missing_ok=True)
        _fsync_directory(path.parent)


def _write_successor_package(destination: Path, protocol: dict, digest: str, event: dict) -> None:
    if destination.exists():
        try:
            current, current_sha = _read_frozen_program_unchecked(destination)
            current_record = json.loads((destination / "SUCCESSOR.json").read_text())
        except (OSError, json.JSONDecodeError, GateError) as error:
            raise GateError("existing successor package is incomplete; preserve it and choose a new evidence path") from error
        if current_sha != digest or current_record != event:
            raise GateError("successor evidence path already contains a different immutable package")
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f".{destination.name}.staging-", dir=destination.parent))
    try:
        write_exclusive(staging / "program.json", canonical_json(protocol))
        write_exclusive(staging / "PROGRAM.sha256", f"{digest}  program.json\n".encode("ascii"))
        write_exclusive(staging / "SUCCESSOR.json", canonical_json(event))
        _fsync_directory(staging)
        os.rename(staging, destination)
        _fsync_directory(destination.parent)
    except BaseException:
        if staging.exists():
            for item in staging.iterdir():
                item.unlink()
            staging.rmdir()
        raise


def _read_frozen_program_unchecked(directory: Path) -> tuple[dict, str]:
    raw = (directory / "program.json").read_bytes()
    digest = sha256_bytes(raw)
    marker = (directory / "PROGRAM.sha256").read_text(encoding="ascii").split()
    if marker != [digest, "program.json"]:
        raise GateError("bounded-program protocol checksum mismatch")
    protocol = json.loads(raw)
    return protocol, digest


def _reconcile_active_protocol(directory: Path, config: dict, digest: str) -> None:
    ledger = config["ledger_dir"]
    state_path = ledger / "process-budget.json"
    successor_path = ledger / "protocol-successor.json"
    initial_state = json.loads(state_path.read_text())
    if not successor_path.exists() and initial_state.get("frozen_protocol_sha256") == digest:
        initial_events = _read_process_ledger_events(ledger)
        if (_active_protocol_sha256(initial_events) == digest
                and initial_state.get("event_count") == len(initial_events)
                and initial_state.get("ledger_head") == initial_events[-1].get("event_sha256")):
            return
    if successor_path.exists() and initial_state.get("frozen_protocol_sha256") == digest:
        initial_events = _read_process_ledger_events(ledger)
        active_sha = _active_protocol_sha256(initial_events)
        if (active_sha == digest and initial_state.get("event_count") == len(initial_events)
                and initial_state.get("ledger_head") == initial_events[-1].get("event_sha256")):
            successor = next(event for event in initial_events
                             if event.get("event") == "protocol_successor_authorized")
            if Path(successor.get("successor_program_dir", "")).resolve() != directory.resolve():
                raise GateError("frozen protocol is not the active successor registered in the lifetime ledger")
            if json.loads((directory / "SUCCESSOR.json").read_text()) != successor:
                raise GateError("successor package record differs from the canonical ledger activation")
            predecessor_dir = Path(successor.get("predecessor_program_dir", "")).resolve()
            predecessor_protocol, predecessor_sha = _read_frozen_program_unchecked(predecessor_dir)
            predecessor_config = _validate_bounded_program(predecessor_protocol, allow_legacy_predecessor=True)
            if (predecessor_sha != successor.get("predecessor_protocol_sha256")
                    or sha256_file(directory / "program.json") != digest
                    or _validate_protocol_successor(predecessor_protocol, config["protocol"],
                                                    predecessor_config, config)
                    != successor.get("compatibility_sha256")):
                raise GateError("protocol successor predecessor/compatibility binding is invalid")
            return
    lock_fd = os.open(ledger / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state_path = ledger / "process-budget.json"
        if not state_path.is_file():
            raise GateError("bounded-program lifetime ledger is missing")
        state = json.loads(state_path.read_text())
        events = _read_process_ledger_events(ledger)
        active_sha = _active_protocol_sha256(events)
        successor = next((event for event in events if event.get("event") == "protocol_successor_authorized"), None)
        if successor is not None:
            successor_dir = Path(successor.get("successor_program_dir", "")).resolve()
            if successor_dir != directory.resolve():
                raise GateError("frozen protocol is not the active successor registered in the lifetime ledger")
            package_event = json.loads((directory / "SUCCESSOR.json").read_text())
            if package_event != successor:
                raise GateError("successor package record differs from the canonical ledger activation")
            if sha256_file(directory / "program.json") != successor.get("successor_protocol_sha256"):
                raise GateError("registered successor protocol file hash changed")
            predecessor_dir = Path(successor.get("predecessor_program_dir", "")).resolve()
            predecessor_protocol, predecessor_sha = _read_frozen_program_unchecked(predecessor_dir)
            if predecessor_sha != successor.get("predecessor_protocol_sha256"):
                raise GateError("protocol successor predecessor package hash changed")
            predecessor_config = _validate_bounded_program(
                predecessor_protocol, allow_legacy_predecessor=True)
            if _validate_protocol_successor(predecessor_protocol, config["protocol"],
                                            predecessor_config, config) != successor.get("compatibility_sha256"):
                raise GateError("protocol successor compatibility proof does not match its frozen packages")
            state_is_predecessor = (
                state.get("frozen_protocol_sha256") == successor.get("predecessor_protocol_sha256")
                and state.get("event_count") == successor.get("event_index")
                and state.get("ledger_head") == successor.get("previous_hash")
            )
            if state_is_predecessor:
                _verify_unstarted_predecessor(
                    state, events[:successor["event_index"]], predecessor_config, predecessor_sha)
                state["frozen_protocol_sha256"] = active_sha
                state["event_count"] = len(events)
                state["ledger_head"] = events[-1]["event_sha256"]
                _replace_process_budget(ledger, state)
            elif not (state.get("frozen_protocol_sha256") == active_sha
                      and state.get("event_count") == len(events)
                      and state.get("ledger_head") == events[-1]["event_sha256"]):
                raise GateError("successor ledger cache is inconsistent; fail closed")
        elif state.get("frozen_protocol_sha256") != active_sha:
            raise GateError("bounded-program lifetime ledger is bound to a different frozen protocol")
        if active_sha != digest:
            raise GateError("bounded-process ledger is bound to a different frozen protocol; predecessor is inactive")


def successor_program_protocol(predecessor_directory: Path, source: Path,
                               destination: Path) -> str:
    """Freeze and atomically register a software-only protocol successor."""
    predecessor_protocol, predecessor_sha = _read_frozen_program_unchecked(predecessor_directory)
    predecessor_config = _validate_bounded_program(predecessor_protocol, allow_legacy_predecessor=True)
    successor_protocol = json.loads(source.read_bytes())
    successor_config = _validate_bounded_program(successor_protocol)
    compatibility_sha = _validate_protocol_successor(
        predecessor_protocol, successor_protocol, predecessor_config, successor_config)
    if predecessor_config["ledger_dir"] != successor_config["ledger_dir"]:
        raise GateError("protocol successor may not create or select another ledger")
    ledger = predecessor_config["ledger_dir"]
    encoded = canonical_json(successor_protocol)
    successor_sha = sha256_bytes(encoded)
    destination = destination.resolve()
    lock_fd = os.open(ledger / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state_path = ledger / "process-budget.json"
        state = json.loads(state_path.read_text())
        events = _read_process_ledger_events(ledger)
        successor_path = ledger / "protocol-successor.json"
        if successor_path.exists():
            existing = json.loads(successor_path.read_text())
            if (existing.get("successor_protocol_sha256") != successor_sha
                    or Path(existing.get("successor_program_dir", "")).resolve() != destination
                    or existing.get("compatibility_sha256") != compatibility_sha):
                raise GateError("a different protocol successor is already recorded")
            package_protocol, package_sha = _read_frozen_program_unchecked(destination)
            package_record = json.loads((destination / "SUCCESSOR.json").read_text())
            if package_sha != successor_sha or package_protocol != successor_protocol or package_record != existing:
                raise GateError("registered successor package is missing or inconsistent")
            active_sha = _active_protocol_sha256(events)
            if active_sha != successor_sha:
                raise GateError("recorded protocol successor does not match the requested protocol")
            if (state.get("frozen_protocol_sha256") == existing.get("predecessor_protocol_sha256")
                    and state.get("event_count") == existing.get("event_index")
                    and state.get("ledger_head") == existing.get("previous_hash")):
                _verify_unstarted_predecessor(
                    state, events[:existing["event_index"]], predecessor_config, predecessor_sha)
                state["frozen_protocol_sha256"] = active_sha
                state["event_count"] = len(events)
                state["ledger_head"] = events[-1]["event_sha256"]
                _replace_process_budget(ledger, state)
            elif not (state.get("frozen_protocol_sha256") == active_sha
                      and state.get("event_count") == len(events)
                      and state.get("ledger_head") == events[-1]["event_sha256"]):
                raise GateError("successor ledger state is inconsistent; fail closed")
            _verify_process_ledger(ledger, state, successor_sha, successor_config.get("continuation_plan"))
            return successor_sha

        if _active_protocol_sha256(events) != predecessor_sha:
            raise GateError("protocol successor predecessor is not the active ledger identity")
        _verify_process_ledger(ledger, state, predecessor_sha, predecessor_config.get("continuation_plan"))
        _ensure_unstarted_ledger(state, events)
        if state.get("frozen_protocol_sha256") != predecessor_sha:
            raise GateError("protocol successor predecessor does not match the canonical ledger")

        authorizations = [event for event in events if event.get("event") == "amendment_authorized"]
        continuation = successor_config.get("continuation_plan")
        if continuation is not None and len(authorizations) != 1:
            raise GateError("protocol successor requires the original single authorization event")
        event_body = {
            "schema": 1,
            "event": "protocol_successor_authorized",
            "program_id": successor_config["program_id"],
            "predecessor_protocol_sha256": predecessor_sha,
            "successor_protocol_sha256": successor_sha,
            "predecessor_program_dir": str(predecessor_directory.resolve()),
            "successor_program_dir": str(destination),
            "event_index": len(events),
            "compatibility_sha256": compatibility_sha,
            "authorization_sha256": authorizations[0].get("authorization_sha256") if authorizations else None,
            "allowance": authorizations[0].get("allowance") if authorizations else None,
        }
        event = {**event_body, "previous_hash": state["ledger_head"]}
        event["event_sha256"] = sha256_bytes(compact_json(event))
        _write_successor_package(destination, successor_protocol, successor_sha, event)
        _atomic_write_successor_record(successor_path, event)
        state["frozen_protocol_sha256"] = successor_sha
        state["event_count"] += 1
        state["ledger_head"] = event["event_sha256"]
        _replace_process_budget(ledger, state)
        verified = _verify_process_ledger(ledger, state, successor_sha, successor_config.get("continuation_plan"))
        if verified[-1]["event_sha256"] != event["event_sha256"]:
            raise GateError("successor event was not retained at the active ledger head")
    return successor_sha


def read_frozen_program(directory: Path, *, allow_legacy_predecessor: bool = False,
                        require_active: bool = True) -> tuple[dict, dict, str]:
    protocol, digest = _read_frozen_program_unchecked(directory)
    config = _validate_bounded_program(protocol, allow_legacy_predecessor=allow_legacy_predecessor)
    if require_active:
        _reconcile_active_protocol(directory, config, digest)
    return config["protocol"], config, digest


def _authorize_program_run(directory: Path, config: dict, protocol_sha: str, run_id: str) -> None:
    """Authorize a phase before any input artifact is opened or hashed."""
    run = config["run_by_id"][run_id]
    ledger_dir = config["ledger_dir"]
    lock_fd = os.open(ledger_dir / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state = json.loads((ledger_dir / "process-budget.json").read_text())
        events = _verify_process_ledger(ledger_dir, state, protocol_sha, config.get("continuation_plan"))
        protocol_events = [e for e in events if e.get("protocol_sha256") == protocol_sha]
        continuation = config.get("continuation_plan")
        if continuation is not None:
            if _pending_coordinator_supervision(protocol_events):
                raise GateError("coordinator phase awaits its supervised-worker resource receipt")
            action = validate_continuation_events(continuation, protocol_events)
            if action["next_run_id"] != run_id:
                raise GateError(f"continuation phase gate refuses run; required state is {action['state']}")
            active_phase = state.get("active_coordinator_phase")
            expected_phase = _phase_id_for_state(continuation, action["state"])
            if (not isinstance(active_phase, dict) or active_phase.get("phase_id") != expected_phase
                    or active_phase.get("pid") != os.getpid()):
                raise GateError("continuation invocation requires its measured coordinator phase to be open")
        else:
            qualified = any(e.get("event") == "qualified" for e in protocol_events)
            if config.get("pipeline_descriptor") is not None and run["phase_id"] != "preflight" and not qualified:
                raise GateError("campaign and later phases require successful preflight qualification")
        if state.get("pending") is not None:
            raise GateError("prior bounded-program invocation unresolved; fail closed")
        reserved = [e for e in protocol_events if e.get("event") == "reserved"]
        if continuation is None:
            ordered = [rid for phase in config["phase_plan"] for rid in phase["run_ids"]]
            if len(reserved) >= len(ordered) or ordered[len(reserved)] != run_id:
                raise GateError("bounded-program invocation is out of frozen phase-plan order")
        if any(e.get("run_id") == run_id for e in reserved):
            raise GateError("bounded-program run_id was already consumed")
        if continuation is not None:
            remaining_ids = continuation_remaining_run_ids(continuation, protocol_events)
            remaining = [config["run_by_id"][rid] for rid in remaining_ids]
        else:
            remaining = [config["run_by_id"][rid] for rid in ordered[len(reserved):]]
        invocation_cap, state_cpu_cap, state_wall_cap = _process_budget_caps(state)
        if state["invocations_reserved"] + len(remaining) > invocation_cap:
            raise GateError("complete remaining phase plan does not fit the persistent authorized invocation budget")
        completed = [e for e in protocol_events if e.get("event") == "completed"]
        local_cpu = sum(float(e["charged_cpu_seconds"]) for e in completed)
        local_wall = sum(float(e["charged_wall_seconds"]) for e in completed)
        limits = config["limits"]
        remaining_cpu = sum(float(x.get("max_cpu_seconds", limits["max_cpu_seconds"])) + BOUNDED_SUPERVISOR_CPU_ALLOWANCE_SECONDS for x in remaining)
        remaining_wall = sum(float(x.get("max_wall_seconds", limits["max_wall_seconds"])) + BOUNDED_SUPERVISOR_WALL_ALLOWANCE_SECONDS for x in remaining)
        if (local_cpu + remaining_cpu > limits["max_total_cpu_seconds"]
                or state["cpu_seconds_charged"] + remaining_cpu > state_cpu_cap):
            raise GateError("remaining bounded-program CPU plan does not fit aggregate/lifetime budget")
        if (local_wall + remaining_wall > limits["max_total_wall_seconds"]
                or state["wall_seconds_charged"] + remaining_wall > state_wall_cap):
            raise GateError("remaining bounded-program wall plan does not fit aggregate/lifetime budget")


def _verify_program_runtime(config: dict, phase_id: str) -> None:
    if config.get("pipeline_descriptor") is not None:
        for command in config["commands_by_stage"].values():
            if sha256_file(Path(command["executable"])) != command["sha256"]:
                raise GateError("pinned pipeline executable changed after freeze")
            for item in command["runtime_files"]:
                if sha256_file(Path(item["path"]).resolve()) != item["sha256"]:
                    raise GateError("pinned pipeline dependency changed after freeze")
        return
    command = config["command"]
    if sha256_file(Path(command["executable"])) != command["sha256"]:
        raise GateError("bounded-program executable changed after freeze")
    for item in command["runtime_files"]:
        if sha256_file(Path(item["path"]).resolve()) != item["sha256"]:
            raise GateError("bounded-program runtime dependency changed after freeze")
    for item in config["artifacts"].values():
        if item.get("role") == "assessment" and phase_id != "scoring":
            continue
        if sha256_file(Path(item["path"])) != item["sha256"]:
            raise GateError("bounded-program input artifact changed after freeze")


def _resolve_authenticated_run_output(directory: Path, protocol_sha: str,
                                      ledger_events: list[dict], binding: dict) -> tuple[Path, str]:
    source_run, source_slot = binding["run_id"], binding["slot"]
    matching_events = [event for event in ledger_events
                       if event.get("event") == "completed"
                       and event.get("protocol_sha256") == protocol_sha
                       and event.get("run_id") == source_run]
    if len(matching_events) != 1:
        raise GateError("pipeline predecessor has no unique authenticated completion event")
    completion = matching_events[0]
    if completion.get("status") != "completed":
        raise GateError("pipeline predecessor completion event is not successful")
    source_receipt_path = directory / "runs" / source_run / "receipt.json"
    if not source_receipt_path.is_file():
        raise GateError("pipeline predecessor output is unavailable")
    if (Path(completion.get("receipt_path", "")).resolve() != source_receipt_path.resolve()
            or sha256_file(source_receipt_path) != completion.get("receipt_sha256")):
        raise GateError("pipeline predecessor receipt does not match its immutable ledger event")
    source_receipt = json.loads(source_receipt_path.read_text())
    if (source_receipt.get("protocol_sha256") != protocol_sha
            or source_receipt.get("run_id") != source_run
            or source_receipt.get("invocation_id") != completion.get("invocation_id")
            or source_receipt.get("status") != completion.get("status")):
        raise GateError("pipeline predecessor receipt identity differs from its ledger event")
    slot_record = source_receipt.get("output_slots", {}).get(source_slot)
    if source_receipt.get("status") != "completed" or not isinstance(slot_record, dict):
        raise GateError("pipeline predecessor did not complete with the requested output")
    path = Path(slot_record.get("path", ""))
    expected_path = (directory / "runs" / source_run / "outputs" / f"{source_slot}.json").resolve()
    if path.resolve() != expected_path or not path.is_file():
        raise GateError("pipeline predecessor output path differs from its frozen run-owned slot")
    actual = sha256_file(path)
    if actual != slot_record.get("sha256"):
        raise GateError("pipeline predecessor output hash mismatch")
    return path.resolve(), actual


def _resolve_pipeline_inputs(
    directory: Path,
    config: dict,
    run: dict,
    run_dir: Path,
    protocol_sha: str,
    ledger_events: list[dict],
) -> tuple[list[str], dict[str, str], dict[str, str], dict[str, str]]:
    stage = config["pipeline_descriptor"]["stages"][run["stage_id"]]
    slots: dict[str, str | list[str]] = {}
    input_hashes: dict[str, object] = {}
    input_paths: dict[str, object] = {}
    for slot, binding in run["slot_bindings"].items():
        if isinstance(binding, list):
            references = validate_run_output_list_binding(binding, slot, run["run_id"], config["run_by_id"])
            resolved = [_resolve_authenticated_run_output(directory, protocol_sha, ledger_events, reference)
                        for reference in references]
            slots[slot] = [str(path) for path, _ in resolved]
            input_hashes[slot] = [digest for _, digest in resolved]
            input_paths[slot] = [str(path) for path, _ in resolved]
            continue
        if not isinstance(binding, dict):
            raise GateError(f"pipeline input slot {slot} must bind an object or supported ordered list")
        kind = binding.get("kind")
        if kind == "artifact":
            artifact = config["artifacts"][binding["artifact_id"]]
            if artifact.get("role") == "assessment" and (run["phase_id"] != "scoring" or slot != "assessment_bundle"):
                raise GateError("assessment artifacts are available only to the scoring stage")
            path = Path(artifact["path"])
            actual = sha256_file(path)
            if actual != artifact["sha256"]:
                raise GateError(f"pipeline input artifact {binding['artifact_id']} changed")
            input_hashes[slot] = actual
        elif kind == "controller_file":
            relative = Path(binding["path"])
            path = (directory / relative).resolve()
            if directory.resolve() not in path.parents or not path.is_file():
                raise GateError("controller input is missing or escapes the frozen protocol directory")
            actual = sha256_file(path)
            input_hashes[slot] = actual
        else:
            path, actual = _resolve_authenticated_run_output(directory, protocol_sha, ledger_events, binding)
            input_hashes[slot] = actual
        slots[slot] = str(path.resolve())
        input_paths[slot] = str(path.resolve())
    outputs = {}
    output_dir = run_dir / "outputs"
    for slot in stage["output_roles"]:
        outputs[slot] = str((output_dir / f"{slot}.json").resolve())
    slots.update(outputs)
    try:
        argv = expand_pipeline_argv(config["pipeline_descriptor"], run["stage_id"], slots)
    except PipelineDescriptorError as error:
        raise GateError(str(error)) from error
    return argv, input_hashes, outputs, input_paths


def _nonnegative_measurement(value: object) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        return 0.0
    return max(0.0, float(value))


def charged_program_resources(outcome: dict, status: str, max_cpu: float, max_wall: float) -> tuple[bool, float, float]:
    """Reserve supervisor capacity without discarding measured consumption."""
    consume_full = (
        status in {"timed_out", "cpu_limit", "memory_limit", "output_limit",
                   "orphaned_process_group", "resource_accounting_unavailable",
                   "harness_error", "supervisor_timeout", "supervisor_cpu_limit",
                   "coordinator_limit"}
        or not outcome.get("process_group_rss_available", False)
    )
    target_cpu = _nonnegative_measurement(outcome.get("target_cpu_seconds"))
    target_wall = _nonnegative_measurement(outcome.get("target_wall_seconds"))
    observed_cpu = max(
        _nonnegative_measurement(outcome.get("cpu_seconds")),
        _nonnegative_measurement(outcome.get("worker_process_tree_cpu_seconds")),
    )
    observed_wall = _nonnegative_measurement(outcome.get("wall_seconds"))
    charged_cpu = max(target_cpu + BOUNDED_SUPERVISOR_CPU_ALLOWANCE_SECONDS, observed_cpu)
    charged_wall = max(target_wall + BOUNDED_SUPERVISOR_WALL_ALLOWANCE_SECONDS, observed_wall)
    if consume_full:
        charged_cpu = max(charged_cpu, max_cpu + BOUNDED_SUPERVISOR_CPU_ALLOWANCE_SECONDS)
        charged_wall = max(charged_wall, max_wall + BOUNDED_SUPERVISOR_WALL_ALLOWANCE_SECONDS)
    return consume_full, charged_cpu, charged_wall


def run_program(directory: Path, run_id: str) -> dict:
    coordinator_cpu_started = _self_cpu_seconds()
    coordinator_function_started = time.monotonic()
    coordinator_function_started_ns = time.monotonic_ns()
    protocol, config, protocol_sha = read_frozen_program(directory)
    _safe_program_id(run_id, "run_id")
    runs = {item["run_id"]: item for item in config["runs"]}
    if run_id not in runs:
        raise GateError("run_id is not allowlisted by the frozen bounded-program protocol")
    run = runs[run_id]
    continuation = config.get("continuation_plan")
    if continuation is not None:
        active_guard = _ACTIVE_COORDINATOR_GUARD
        if not isinstance(active_guard, dict):
            raise GateError("continuation run-program requires the enforced coordinator_phase_guard")
    _authorize_program_run(directory, config, protocol_sha, run_id)
    pipeline_mode = config.get("pipeline_descriptor") is not None
    run_dir = directory / "runs" / run_id
    input_bytes = b""
    input_hashes, output_paths, input_paths = {}, {}, {}
    if pipeline_mode:
        command_config = config["commands_by_stage"][run["stage_id"]]
        argv = []
    else:
        command_config = config["command"]
        argv = [command_config["executable"], *command_config["args"], *run.get("args", [])]
    if not pipeline_mode and run["stdin_artifact"] is not None:
        artifact = config["artifacts"][run["stdin_artifact"]]
        if artifact.get("role") == "assessment" and run["phase_id"] != "scoring":
            raise GateError("assessment artifact may be opened only by the scoring phase")
        input_bytes = Path(artifact["path"]).read_bytes()
        if sha256_bytes(input_bytes) != artifact["sha256"]:
            raise GateError("bounded-program stdin changed after freeze")
    limits = config["limits"]
    if len(input_bytes) > limits["max_stdin_bytes"]:
        raise GateError("bounded-program stdin exceeds byte cap; no invocation consumed")
    _verify_program_runtime(config, run["phase_id"])

    ledger_dir = config["ledger_dir"]
    ledger_lock_fd = os.open(ledger_dir / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(ledger_lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state_path = ledger_dir / "process-budget.json"
        state = json.loads(state_path.read_text())
        events = _verify_process_ledger(ledger_dir, state, protocol_sha, config.get("continuation_plan"))
        protocol_events = [e for e in events if e.get("protocol_sha256") == protocol_sha]
        continuation = config.get("continuation_plan")
        if continuation is not None:
            if state.get("terminal_status") is not None:
                raise GateError(f"continuation stopped after {state['terminal_status']}")
            boot_id = _kernel_boot_id()
            if state.get("overall_start_monotonic_ns") is None:
                raise GateError("continuation must start through a measured coordinator phase")
            if state.get("kernel_boot_id") != boot_id:
                raise GateError("continuation deadline is invalid after kernel boot identity changed")
            elapsed_before = (time.monotonic_ns() - state["overall_start_monotonic_ns"]) / 1_000_000_000.0
            if elapsed_before >= continuation["overall_deadline_seconds"]:
                _append_program_terminal(ledger_dir, state, "overall_wall_deadline")
                raise GateError("continuation overall hard wall deadline expired")
            action = validate_continuation_events(continuation, protocol_events)
            if action["next_run_id"] != run_id:
                raise GateError(f"continuation phase gate refuses run; required state is {action['state']}")
            active_phase = state.get("active_coordinator_phase")
            expected_phase = _phase_id_for_state(continuation, action["state"])
            if (not isinstance(active_phase, dict) or active_phase.get("phase_id") != expected_phase
                    or active_phase.get("pid") != os.getpid()):
                raise GateError("continuation invocation requires its measured coordinator phase to be open")
        else:
            qualified = any(e.get("event") == "qualified" for e in protocol_events)
            if pipeline_mode and run["phase_id"] != "preflight" and not qualified:
                raise GateError("campaign and later phases require successful preflight qualification")
        if pipeline_mode:
            # Authenticate predecessor receipts and resolve their output slots
            # while holding the same lock that protects the invocation
            # reservation. This closes the check-to-reservation mutation gap.
            argv, input_hashes, output_paths, input_paths = _resolve_pipeline_inputs(
                directory, config, run, run_dir, protocol_sha, events)
        if state.get("pending") is not None:
            raise GateError("prior bounded-program invocation unresolved; fail closed")
        prior_for_protocol = [e for e in protocol_events if e.get("event") == "reserved"]
        if any(e.get("run_id") == run_id for e in prior_for_protocol):
            raise GateError("bounded-program run_id was already consumed")
        if continuation is None:
            ordered_run_ids = [run_id for phase in config["phase_plan"] for run_id in phase["run_ids"]]
            if len(prior_for_protocol) >= len(ordered_run_ids) or ordered_run_ids[len(prior_for_protocol)] != run_id:
                raise GateError("bounded-program invocation is out of frozen phase-plan order")
        invocation_cap, state_cpu_cap, state_wall_cap = _process_budget_caps(state)
        if state["invocations_reserved"] >= invocation_cap:
            raise GateError("bounded-program authorized invocation budget exhausted")
        protocol_completed = [e for e in protocol_events if e.get("event") == "completed"]
        local_cpu = sum(float(e["charged_cpu_seconds"]) for e in protocol_completed)
        local_wall = sum(float(e["charged_wall_seconds"]) for e in protocol_completed)
        if len(prior_for_protocol) >= limits["max_invocations"]:
            raise GateError("bounded-program protocol invocation budget exhausted")
        if continuation is not None:
            remaining_ids = continuation_remaining_run_ids(continuation, protocol_events)
            remaining_runs = [config["run_by_id"][rid] for rid in remaining_ids]
        else:
            remaining_runs = [config["run_by_id"][rid] for rid in ordered_run_ids[len(prior_for_protocol):]]
        continuation_max_cpu = state_cpu_cap
        continuation_max_wall = state_wall_cap
        if state["invocations_reserved"] + len(remaining_runs) > invocation_cap:
            raise GateError("complete remaining phase plan does not fit the persistent authorized invocation budget")
        remaining_cpu = sum(float(item.get("max_cpu_seconds", limits["max_cpu_seconds"])) + BOUNDED_SUPERVISOR_CPU_ALLOWANCE_SECONDS for item in remaining_runs)
        remaining_wall = sum(float(item.get("max_wall_seconds", limits["max_wall_seconds"])) + BOUNDED_SUPERVISOR_WALL_ALLOWANCE_SECONDS for item in remaining_runs)
        if local_cpu + remaining_cpu > limits["max_total_cpu_seconds"] or state["cpu_seconds_charged"] + remaining_cpu > state_cpu_cap:
            raise GateError("remaining bounded-program phase-plan CPU allowance does not fit aggregate/lifetime budget")
        if local_wall + remaining_wall > limits["max_total_wall_seconds"] or state["wall_seconds_charged"] + remaining_wall > state_wall_cap:
            raise GateError("remaining bounded-program phase-plan wall allowance does not fit aggregate/lifetime budget")
        max_cpu = float(run.get("max_cpu_seconds", limits["max_cpu_seconds"]))
        max_wall = float(run.get("max_wall_seconds", limits["max_wall_seconds"]))
        effective_max_wall = max_wall
        if continuation is not None:
            phase_start = state.get("active_coordinator_phase")
            if not isinstance(phase_start, dict):
                raise GateError("continuation coordinator phase is not active")
            phase_name = phase_start["phase_id"]
            phase_ids = {"primary_preflight": set(continuation["slots"]["primary_preflight"]),
                         "recovery_preflight": set(continuation["slots"]["recovery_preflight"]),
                         "campaign_orchestration": set(continuation["slots"]["campaign"]),
                         "final_scoring_orchestration": set(continuation["slots"]["final_scoring"])}[phase_name]
            phase_completed = [event for event in events if event.get("event") == "completed"
                               and event.get("protocol_sha256") == protocol_sha
                               and event.get("run_id") in phase_ids]
            phase_run_coord = [event for event in events if event.get("event") == "coordinator_accounted"
                               and event.get("protocol_sha256") == protocol_sha
                               and event.get("run_id") in phase_ids]
            phase_raw_cpu = _self_cpu_seconds() - float(phase_start["start_cpu_seconds"])
            phase_residual_cpu = phase_raw_cpu - sum(float(event["coordinator_cpu_seconds"])
                                                     for event in phase_run_coord)
            phase_elapsed = (time.monotonic_ns() - int(phase_start["start_monotonic_ns"])) / 1e9
            phase_residual_wall = (phase_elapsed
                - sum(float(event["worker_span_seconds"]) for event in phase_completed
                      if isinstance(event.get("worker_span_seconds"), (int, float)))
                - sum(float(event["coordinator_exclusive_wall_seconds"]) for event in phase_run_coord))
            coordinator_cpu_used = (float(state["coordinator_cpu_seconds_charged"])
                                     + max(0.0, phase_residual_cpu))
            coordinator_wall_used = (float(state["coordinator_exclusive_wall_seconds_charged"])
                                     + max(0.0, phase_residual_wall))
            elapsed_before = (time.monotonic_ns() - state["overall_start_monotonic_ns"]) / 1_000_000_000.0
            if (not math.isfinite(coordinator_cpu_used) or coordinator_cpu_used < 0
                    or not math.isfinite(coordinator_wall_used) or coordinator_wall_used < 0):
                _append_program_terminal(ledger_dir, state, "coordinator_measurement_unavailable")
                raise GateError("continuation CPU/exclusive-wall accounting is unavailable")
            if coordinator_cpu_used >= state["max_coordinator_cpu_seconds"]:
                _append_program_terminal(ledger_dir, state, "coordinator_cpu_budget_exhausted")
                raise GateError("continuation coordinator CPU allowance is exhausted")
            coordinator_wall_remaining = state["max_coordinator_exclusive_wall_seconds"] - coordinator_wall_used
            future_ids = [rid for rid in remaining_ids if rid != run_id]
            future_worker_reserve = sum(
                float(config["run_by_id"][rid].get("max_wall_seconds", limits["max_wall_seconds"]))
                + BOUNDED_SUPERVISOR_WALL_ALLOWANCE_SECONDS for rid in future_ids)
            available_current_total = (continuation["overall_deadline_seconds"] - elapsed_before
                                      - future_worker_reserve - coordinator_wall_remaining)
            effective_max_wall = min(max_wall, available_current_total - BOUNDED_SUPERVISOR_WALL_ALLOWANCE_SECONDS)
            if coordinator_wall_remaining <= 0 or effective_max_wall <= 0:
                _append_program_terminal(ledger_dir, state, "coordinator_or_wall_budget_exhausted")
                raise GateError("continuation has insufficient remaining coordinator/deadline allowance")

        invocation_id = f"{state['invocations_reserved'] + 1:06d}-{uuid.uuid4().hex}"
        (directory / "runs").mkdir(exist_ok=True)
        run_dir.mkdir(exist_ok=False)
        if pipeline_mode:
            (run_dir / "outputs").mkdir()
        reservation = {"invocation_id": invocation_id, "run_id": run_id,
                       "phase_id": run["phase_id"],
                       "stage_id": run.get("stage_id"),
                       "protocol_sha256": protocol_sha, "command": argv,
                       "executable_sha256": command_config["sha256"],
                       "runtime_files": command_config["runtime_files"],
                       "stdin_artifact": run["stdin_artifact"], "stdin_sha256": sha256_bytes(input_bytes),
                       "input_slot_hashes": input_hashes,
                       "input_slot_paths": input_paths,
                       "output_slot_paths": output_paths,
                       "reserved_at_unix_ns": time.time_ns(), "limits": limits,
                       **({"declared_max_wall_seconds": max_wall,
                           "effective_max_wall_seconds": effective_max_wall,
                           "continuation_amendment_id": continuation["amendment_id"]}
                          if continuation is not None else {})}
        write_exclusive(run_dir / "reservation.json", canonical_json(reservation))
        state["ledger_head"] = _process_ledger_event(
            ledger_dir / "process-events.jsonl", {"event": "reserved", **reservation}, state["ledger_head"])
        state["event_count"] += 1
        state["invocations_reserved"] += 1
        state["pending"] = invocation_id
        _replace_process_budget(ledger_dir, state)

        exec_limits = {"max_cpu_seconds": max_cpu,
                       "max_wall_seconds": effective_max_wall,
                       "max_address_space_bytes": limits["max_address_space_bytes"],
                       "max_stdin_bytes": limits["max_stdin_bytes"],
                       "max_output_bytes": limits["max_output_bytes"],
                       "max_file_bytes": limits["max_file_bytes"]}
        try:
            with _pause_coordinator_exclusive_wall_timer():
                outcome = run_bounded_process(
                    reservation["command"], input_bytes, command_config["environment"],
                    run_dir, exec_limits)
        except BaseException as error:
            outcome = {"status": "harness_error", "returncode": None,
                       "wall_seconds": max_cpu * 0.0, "cpu_seconds": 0.0,
                       "stdout": b"", "stderr": f"{type(error).__name__}: {error}".encode(),
                       "process_group_rss_available": False, "peak_process_group_rss_bytes": None,
                       "waited_child_peak_rss_bytes": None, "harness_diagnostic": str(error)}
        stdout = outcome.pop("stdout", b"")
        stderr = outcome.pop("stderr", b"")
        write_exclusive(run_dir / "stdout.bin", stdout)
        write_exclusive(run_dir / "stderr.bin", stderr)
        status = outcome.get("status", "harness_error")
        worker_span = outcome.get("total_invocation_wall_seconds", outcome.get("wall_seconds"))
        if continuation is not None and (isinstance(worker_span, bool) or not isinstance(worker_span, (int, float))
                                         or not math.isfinite(worker_span) or worker_span < 0):
            status = "resource_accounting_unavailable"
            outcome["harness_diagnostic"] = "bounded-process invocation wall span is unavailable"
            worker_span = None
        coordinator_cpu_total = _self_cpu_seconds() - coordinator_cpu_started
        prior_coordinator_wall = float(state["coordinator_exclusive_wall_seconds_charged"]) if continuation is not None else 0.0
        coordinator_exclusive_wall = None
        if continuation is not None and worker_span is not None:
            coordinator_exclusive_wall = time.monotonic() - coordinator_function_started - float(worker_span)
            if not math.isfinite(coordinator_exclusive_wall) or coordinator_exclusive_wall < -1e-6:
                status = "resource_accounting_unavailable"
                outcome["harness_diagnostic"] = "exclusive coordinator wall time could not be established"
                coordinator_exclusive_wall = None
            else:
                coordinator_exclusive_wall = max(0.0, coordinator_exclusive_wall)
        if continuation is not None and (coordinator_exclusive_wall is None
                                         or not math.isfinite(coordinator_cpu_total)
                                         or coordinator_cpu_total < 0):
            status = "resource_accounting_unavailable"
        if continuation is not None and (
            coordinator_cpu_total > state["max_coordinator_cpu_seconds"]
            or coordinator_exclusive_wall is None
            or coordinator_exclusive_wall > state["max_coordinator_exclusive_wall_seconds"]
        ):
            status = "coordinator_limit"
            outcome["harness_diagnostic"] = "coordinator CPU or exclusive-wall envelope exceeded/unavailable"
        output_records = {}
        if pipeline_mode and status == "completed":
            for slot, output_path in output_paths.items():
                path = Path(output_path)
                if not path.is_file() or path.stat().st_size > limits["max_file_bytes"]:
                    status = "output_contract_failure"
                    outcome["harness_diagnostic"] = f"missing or oversized declared output slot {slot}"
                    break
                path.chmod(0o444)
                output_records[slot] = {"path": str(path), "sha256": sha256_file(path), "bytes": path.stat().st_size}
        consume_full, charged_cpu, charged_wall = charged_program_resources(outcome, status, max_cpu, max_wall)
        receipt = {**reservation, **outcome, "status": status,
                   "output_slots": output_records,
                   "stdout_sha256": sha256_bytes(stdout), "stderr_sha256": sha256_bytes(stderr),
                   "stdout_bytes": len(stdout), "stderr_bytes": len(stderr),
                   "charged_cpu_seconds": charged_cpu, "charged_wall_seconds": charged_wall,
                   "completed_at_unix_ns": time.time_ns()}
        write_exclusive(run_dir / "receipt.json", canonical_json(receipt))
        state["ledger_head"] = _process_ledger_event(
            ledger_dir / "process-events.jsonl",
            {"event": "completed", "invocation_id": invocation_id,
             "protocol_sha256": protocol_sha, "run_id": run_id,
             "receipt_path": str((run_dir / "receipt.json").resolve()),
             "receipt_sha256": sha256_file(run_dir / "receipt.json"),
             "status": status, "cpu_seconds": receipt.get("cpu_seconds", 0.0),
             "wall_seconds": receipt.get("wall_seconds", 0.0),
             "worker_span_seconds": worker_span,
             "charged_cpu_seconds": charged_cpu, "charged_wall_seconds": charged_wall},
            state["ledger_head"])
        state["event_count"] += 1
        state["cpu_seconds_charged"] += charged_cpu
        state["wall_seconds_charged"] += charged_wall
        state["pending"] = None
        if continuation is not None:
            # Sample after target and completion receipts are persisted. The
            # separately stored coordinator receipt records this boundary; its
            # own tiny append cost is reserved inside the fixed envelope.
            coordinator_cpu_total = _self_cpu_seconds() - coordinator_cpu_started
            coordinator_exclusive_wall = (
                time.monotonic() - coordinator_function_started
                - (float(worker_span) if worker_span is not None else 0.0)
                if worker_span is not None else None
            )
            coordinator_measurement_available = (
                worker_span is not None and coordinator_exclusive_wall is not None
                and math.isfinite(coordinator_cpu_total) and coordinator_cpu_total >= 0
                and math.isfinite(coordinator_exclusive_wall) and coordinator_exclusive_wall >= -1e-6
            )
            if coordinator_measurement_available:
                coordinator_exclusive_wall = max(0.0, coordinator_exclusive_wall)
                coordinator_receipt = {
                    "schema": 1,
                    "program_id": config["program_id"],
                    "amendment_id": continuation["amendment_id"],
                    "run_id": run_id,
                    "invocation_id": invocation_id,
                    "phase_id": run["phase_id"],
                    "stage_id": run.get("stage_id"),
                    "measurement_method": "RUSAGE_SELF_AND_EXCLUSIVE_WALL",
                    "cpu_seconds": coordinator_cpu_total,
                    "cpu_scope": "RUSAGE_SELF user+system; excludes child/worker accounting",
                    "exclusive_wall_seconds": coordinator_exclusive_wall,
                    "wall_formula": "program elapsed since first slot minus all worker invocation spans minus previously charged exclusive wall",
                    "overall_elapsed_seconds": (time.monotonic_ns() - state["overall_start_monotonic_ns"]) / 1_000_000_000.0,
                    "worker_span_seconds": worker_span,
                    "measured_before_receipt_write": True,
                    "recorded_at_unix_ns": time.time_ns(),
                }
                coordinator_path = run_dir / "coordinator-receipt.json"
                write_exclusive(coordinator_path, canonical_json(coordinator_receipt))
                coordinator_receipt_sha = sha256_file(coordinator_path)
                state["ledger_head"] = _process_ledger_event(
                    ledger_dir / "process-events.jsonl",
                    {"event": "coordinator_accounted", "protocol_sha256": protocol_sha,
                     "run_id": run_id, "invocation_id": invocation_id,
                     "receipt_path": str(coordinator_path.resolve()),
                     "receipt_sha256": coordinator_receipt_sha,
                     "coordinator_cpu_seconds": coordinator_cpu_total,
                     "coordinator_exclusive_wall_seconds": coordinator_exclusive_wall},
                    state["ledger_head"])
                state["event_count"] += 1
                state["coordinator_cpu_seconds_charged"] += coordinator_cpu_total
                state["coordinator_exclusive_wall_seconds_charged"] += coordinator_exclusive_wall
            else:
                status = "resource_accounting_unavailable"
                state["ledger_head"] = _process_ledger_event(
                    ledger_dir / "process-events.jsonl",
                    {"event": "coordinator_accounting_unavailable", "protocol_sha256": protocol_sha,
                     "run_id": run_id, "invocation_id": invocation_id,
                     "reason": "RUSAGE_SELF or worker wall span was unavailable"},
                    state["ledger_head"])
                state["event_count"] += 1
                _append_program_terminal(ledger_dir, state, "resource_accounting_unavailable")
            if (state["coordinator_cpu_seconds_charged"] > state["max_coordinator_cpu_seconds"]
                    or state["coordinator_exclusive_wall_seconds_charged"] > state["max_coordinator_exclusive_wall_seconds"]):
                _append_program_terminal(ledger_dir, state, "coordinator_allowance_exhausted")
            elif status in {"resource_accounting_unavailable", "coordinator_limit"}:
                _append_program_terminal(ledger_dir, state, status)
            elif time.monotonic_ns() >= state["overall_deadline_monotonic_ns"]:
                _append_program_terminal(ledger_dir, state, "overall_wall_deadline")
        _replace_process_budget(ledger_dir, state)
        return receipt


def verify_program(directory: Path) -> dict:
    protocol, config, protocol_sha = read_frozen_program(directory)
    state = json.loads((config["ledger_dir"] / "process-budget.json").read_text())
    events = _verify_process_ledger(config["ledger_dir"], state, protocol_sha, config.get("continuation_plan"))
    relevant = [e for e in events if e.get("protocol_sha256") == protocol_sha and e.get("event") == "completed"]
    receipt_checks = []
    for event in relevant:
        path = Path(event["receipt_path"])
        if not path.is_file() or sha256_file(path) != event["receipt_sha256"]:
            raise GateError("bounded-program immutable receipt missing or hash-mismatched")
        receipt = json.loads(path.read_text())
        if receipt.get("invocation_id") != event.get("invocation_id") or receipt.get("status") != event.get("status"):
            raise GateError("bounded-program receipt/ledger mismatch")
        run_dir = path.parent
        for stream in ("stdout", "stderr"):
            stream_path = run_dir / f"{stream}.bin"
            if not stream_path.is_file() or sha256_file(stream_path) != receipt.get(f"{stream}_sha256"):
                raise GateError(f"bounded-program captured {stream} evidence is missing or hash-mismatched")
        for slot, output in receipt.get("output_slots", {}).items():
            output_path = Path(output.get("path", ""))
            if not output_path.is_file() or sha256_file(output_path) != output.get("sha256"):
                raise GateError(f"bounded-program output slot {slot} is missing or hash-mismatched")
        for slot, input_path in receipt.get("input_slot_paths", {}).items():
            expected_hashes = receipt.get("input_slot_hashes", {}).get(slot)
            if isinstance(input_path, list):
                if not isinstance(expected_hashes, list) or not input_path or len(input_path) != len(expected_hashes):
                    raise GateError(f"bounded-program input slot {slot} has malformed list path/hash evidence")
                for item_path, expected_hash in zip(input_path, expected_hashes):
                    path = Path(item_path)
                    if not path.is_file() or sha256_file(path) != expected_hash:
                        raise GateError(f"bounded-program input slot {slot} contains a missing or hash-mismatched item")
            else:
                if isinstance(expected_hashes, list):
                    raise GateError(f"bounded-program input slot {slot} has scalar/list evidence mismatch")
                path = Path(input_path)
                if not path.is_file() or sha256_file(path) != expected_hashes:
                    raise GateError(f"bounded-program input slot {slot} is missing or hash-mismatched")
        receipt_checks.append({"run_id": receipt["run_id"], "status": receipt["status"],
                               "receipt_sha256": event["receipt_sha256"]})
    protocol_qualified = any(e.get("event") == "qualified" and e.get("protocol_sha256") == protocol_sha
                             for e in events)
    valid = state.get("pending") is None and (
        config.get("pipeline_descriptor") is None or protocol_qualified)
    if config.get("continuation_plan") is not None:
        valid = valid and state.get("terminal_status") is None and state.get("active_coordinator_phase") is None
        valid = valid and not _pending_coordinator_supervision(events)
    return {"status": "valid" if valid else "incomplete",
            "protocol_sha256": protocol_sha, "invocations_reserved": state["invocations_reserved"],
            "cpu_seconds_charged": state["cpu_seconds_charged"],
            "wall_seconds_charged": state["wall_seconds_charged"], "receipts": receipt_checks}


def qualify_program(directory: Path) -> dict:
    """Derive and append the one-shot qualification from frozen run outputs."""
    protocol, config, protocol_sha = read_frozen_program(directory)
    if config.get("pipeline_descriptor") is None:
        raise GateError("qualification events require descriptor-mode bounded protocol")
    # Qualification must not trust mutable paths merely because an earlier
    # receipt once named them. Recheck the full captured evidence first.
    verify_program(directory)
    continuation = config.get("continuation_plan")
    phase_state = None
    if continuation is not None:
        ledger_state = json.loads((config["ledger_dir"] / "process-budget.json").read_text())
        phase_state = ledger_state.get("active_coordinator_phase", {}).get("phase_id")
    qualification_path = "recovery" if phase_state == "recovery_preflight" else "primary"
    qualification = (config["recovery_qualification"] if qualification_path == "recovery"
                     else config["preflight_qualification"])
    stage_run_ids = qualification["stage_run_ids"]
    ledger_dir = config["ledger_dir"]
    lock_fd = os.open(ledger_dir / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state_path = ledger_dir / "process-budget.json"
        state = json.loads(state_path.read_text())
        events = _verify_process_ledger(ledger_dir, state, protocol_sha, config.get("continuation_plan"))
        if state.get("pending") is not None or state.get("qualification_status") != "preflight_pending":
            raise GateError("preflight qualification is already consumed or has an unresolved invocation")
        completed = {e["run_id"]: e for e in events if e.get("event") == "completed" and e.get("protocol_sha256") == protocol_sha}
        stage_receipts = {}
        for stage_name, run_id in stage_run_ids.items():
            event = completed.get(run_id)
            if event is None or event.get("status") != "completed":
                raise GateError("all frozen preflight stage invocations must complete successfully before qualification")
            receipt_path = Path(event["receipt_path"])
            if not receipt_path.is_file() or sha256_file(receipt_path) != event.get("receipt_sha256"):
                raise GateError("preflight invocation receipt missing or changed")
            stage_receipts[stage_name] = json.loads(receipt_path.read_text())
        scorer_slots = stage_receipts["scorer"].get("output_slots", {})
        aggregator_slots = stage_receipts["aggregator"].get("output_slots", {})
        scores_record = scorer_slots.get("scores_output")
        report_record = aggregator_slots.get("report_output")
        if not isinstance(scores_record, dict) or not isinstance(report_record, dict):
            raise GateError("preflight scorer/aggregator outputs are not bound in run-owned receipts")
        scores_path, report_path = Path(scores_record["path"]), Path(report_record["path"])
        if sha256_file(scores_path) != scores_record["sha256"] or sha256_file(report_path) != report_record["sha256"]:
            raise GateError("preflight score/report output changed after stage completion")
        scores = json.loads(scores_path.read_text())
        report = json.loads(report_path.read_text())
        if continuation is not None:
            import base64
            from scientific_pipeline_preflight import PreflightError, _validate_fdi_synthetic_pipeline

            spec_artifact = config["artifacts"][qualification["spec_artifact_id"]]
            spec = json.loads(Path(spec_artifact["path"]).read_text())
            runner_receipt = stage_receipts["runner"]
            verifier_receipt = stage_receipts["verifier"]
            runner_output = runner_receipt.get("output_slots", {}).get("candidate_outputs")
            verifier_output = verifier_receipt.get("output_slots", {}).get("verified_receipts")
            if not isinstance(runner_output, dict) or not isinstance(verifier_output, dict):
                raise GateError("continuation preflight runner/verifier outputs are not receipt-bound")
            runner_manifest_path = Path(runner_receipt.get("input_slot_paths", {}).get("run_manifest", ""))
            training_bundle_path = Path(runner_receipt.get("input_slot_paths", {}).get("training_bundle", ""))
            scorer_manifest_path = Path(stage_receipts["scorer"].get("input_slot_paths", {}).get("run_manifest", ""))
            assessment_bundle_path = Path(stage_receipts["scorer"].get("input_slot_paths", {}).get("assessment_bundle", ""))
            aggregate_manifest_path = Path(stage_receipts["aggregator"].get("input_slot_paths", {}).get("run_manifest", ""))
            required_inputs = (runner_manifest_path, training_bundle_path, scorer_manifest_path,
                               assessment_bundle_path, aggregate_manifest_path)
            if any(not path.is_file() for path in required_inputs):
                raise GateError("continuation synthetic pipeline manifests/bundles are missing")
            runner_manifest_bytes = runner_manifest_path.read_bytes()
            runner_manifest = json.loads(runner_manifest_bytes)
            training_bundle = json.loads(training_bundle_path.read_bytes())
            scorer_manifest = json.loads(scorer_manifest_path.read_bytes())
            assessment_bundle = json.loads(assessment_bundle_path.read_bytes())
            aggregate_manifest = json.loads(aggregate_manifest_path.read_bytes())
            training_rows = {item.get("id"): item for item in training_bundle.get("artifacts", [])
                             if isinstance(item, dict)}
            assessment_rows = {item.get("id"): item for item in assessment_bundle.get("artifacts", [])
                               if isinstance(item, dict)}
            if len(training_rows) != len(training_bundle.get("artifacts", [])) or len(assessment_rows) != len(assessment_bundle.get("artifacts", [])):
                raise GateError("continuation bundle contains duplicate/malformed artifact identities")
            def decode_bundle(rows: dict, role: str) -> dict[str, bytes]:
                decoded = {}
                for artifact_id, artifact in rows.items():
                    try:
                        content = base64.b64decode(artifact.get("content_base64", ""), validate=True)
                    except Exception as error:
                        raise GateError(f"{role} bundle has invalid base64 content") from error
                    if sha256_bytes(content) != artifact.get("sha256"):
                        raise GateError(f"{role} bundle content hash mismatch")
                    decoded[artifact_id] = content
                return decoded
            training_by_id = decode_bundle(training_rows, "training")
            assessment_by_id = decode_bundle(assessment_rows, "assessment")
            training_path_by_control = {}
            training_bytes_by_control = {}
            assessment_bytes_by_control = {}
            for run in spec.get("runs", []):
                control_id = run.get("control_id")
                if control_id in training_path_by_control:
                    continue
                training_artifact_id = run.get("training_artifact")
                assessment_artifact_id = run.get("assessment_artifact")
                training_by_run = next((row for row in runner_manifest.get("runs", [])
                                        if row.get("run_id") == run.get("run_id")), None)
                if (training_artifact_id not in training_by_id or assessment_artifact_id not in assessment_by_id
                        or not isinstance(training_by_run, dict)):
                    raise GateError("continuation synthetic run is missing its control-specific bundle mapping")
                training_path_by_control[control_id] = training_by_run.get("training_path")
                training_bytes_by_control[control_id] = training_by_id[training_artifact_id]
                assessment_bytes_by_control[control_id] = assessment_by_id[assessment_artifact_id]
            try:
                _validate_fdi_synthetic_pipeline(
                    preflight_id=spec.get("preflight_id"), runner_manifest=runner_manifest,
                    runner_manifest_bytes=runner_manifest_bytes, scorer_manifest=scorer_manifest,
                    runs=spec.get("runs", []), training_bytes_by_control=training_bytes_by_control,
                    assessment_bytes_by_control=assessment_bytes_by_control,
                    training_path_by_control=training_path_by_control,
                    training_bundle=training_bundle, assessment_bundle=assessment_bundle,
                    aggregate_manifest=aggregate_manifest,
                    candidate_records=json.loads(Path(runner_output["path"]).read_text()),
                    verified_records=json.loads(Path(verifier_output["path"]).read_text()),
                    score_output=scores, report_output=report,
                    scores_sha256=sha256_file(scores_path))
            except PreflightError as error:
                raise GateError(f"continuation synthetic pipeline qualification failed: {error}") from error
            evidence_sha256 = sha256_bytes(canonical_json({
                "protocol_sha256": protocol_sha,
                "stage_receipt_sha256": {name: sha256_file(Path(completed[run_id]["receipt_path"]))
                                         for name, run_id in stage_run_ids.items()},
                "scores_sha256": scores_record["sha256"], "report_sha256": report_record["sha256"],
            }))
            active_phase = state.get("active_coordinator_phase")
            required_phase = "primary_preflight" if qualification_path == "primary" else "recovery_preflight"
            if not isinstance(active_phase, dict) or active_phase.get("phase_id") != required_phase:
                raise GateError("continuation qualification requires its measured preflight phase")
            payload = {"event": "qualification_prepared", "protocol_sha256": protocol_sha,
                       "amendment_id": continuation["amendment_id"],
                       "qualification_path": qualification_path, "evidence_sha256": evidence_sha256,
                       "prepared_at_unix_ns": time.time_ns()}
            state["ledger_head"] = _process_ledger_event(ledger_dir / "process-events.jsonl", payload, state["ledger_head"])
            state["event_count"] += 1
            _replace_process_budget(ledger_dir, state)
            return {"status": "qualification_prepared", "protocol_sha256": protocol_sha,
                    "preflight_evidence_sha256": evidence_sha256}
        expected_bindings = qualification["expected_score_bindings"]
        expected_ids = [row["run_id"] for row in expected_bindings]
        if not isinstance(scores, list) or len(scores) != len(expected_ids):
            raise GateError("preflight score output count differs from the frozen run set")
        spec_artifact = config["artifacts"][qualification["spec_artifact_id"]]
        spec = json.loads(Path(spec_artifact["path"]).read_text())
        if [
            {key: run.get(key) for key in ("run_id", "control_id", "method", "seed")}
            for run in spec.get("runs", [])
        ] != expected_bindings:
            raise GateError("pinned synthetic specification disagrees with frozen score bindings")
        controls = {control.get("id"): control for control in spec.get("controls", []) if isinstance(control, dict)}
        if len(controls) != qualification["expected_control_count"]:
            raise GateError("pinned synthetic specification has the wrong control set")
        scorer_manifest_path = Path(stage_receipts["scorer"].get("input_slot_paths", {}).get("run_manifest", ""))
        runner_manifest_path = Path(stage_receipts["runner"].get("input_slot_paths", {}).get("run_manifest", ""))
        if not scorer_manifest_path.is_file() or not runner_manifest_path.is_file():
            raise GateError("preflight runner/scorer input manifests are unavailable")
        scorer_manifest = json.loads(scorer_manifest_path.read_text())
        runner_manifest = json.loads(runner_manifest_path.read_text())
        def contains_assessment_key(value: object) -> bool:
            if isinstance(value, dict):
                return any("assessment" in str(key).lower() or contains_assessment_key(item)
                           for key, item in value.items())
            if isinstance(value, list):
                return any(contains_assessment_key(item) for item in value)
            return False
        if (contains_assessment_key(runner_manifest)
                or runner_manifest.get("preflight_id") != spec.get("preflight_id")):
            raise GateError("runner manifest must be bound to the synthetic spec and hide assessment data")
        expected_runner_runs = [
            {key: run[key] for key in ("run_id", "control_id", "method", "seed", "training_artifact")}
            for run in spec.get("runs", [])
        ]
        if runner_manifest.get("runs") != expected_runner_runs or scorer_manifest.get("runs") != spec.get("runs"):
            raise GateError("runner/scorer manifests do not match the pinned synthetic run matrix")
        synthetic_controls = {row.get("id"): row for row in scorer_manifest.get("controls", []) if isinstance(row, dict)}
        if set(synthetic_controls) != set(controls):
            raise GateError("scorer manifest control IDs differ from the pinned synthetic specification")
        runner_controls = {row.get("id"): row for row in runner_manifest.get("controls", []) if isinstance(row, dict)}
        if set(runner_controls) != set(controls):
            raise GateError("runner manifest control IDs differ from the pinned synthetic specification")
        for control_id, control in controls.items():
            runner_training = runner_controls[control_id].get("training", {})
            scorer_training = synthetic_controls[control_id].get("training", {})
            if (runner_training.get("id") != control["training"]["id"]
                    or runner_training.get("sha256") != scorer_training.get("sha256")
                    or runner_training.get("path") != scorer_training.get("path")):
                raise GateError("runner received a training artifact that differs from the pinned control mapping")
        runner_receipt = stage_receipts["runner"]
        verifier_receipt = stage_receipts["verifier"]
        runner_output = runner_receipt.get("output_slots", {}).get("candidate_outputs")
        verifier_output = verifier_receipt.get("output_slots", {}).get("verified_receipts")
        if not isinstance(runner_output, dict) or not isinstance(verifier_output, dict):
            raise GateError("preflight runner/verifier outputs are not bound to immutable receipts")
        candidates = json.loads(Path(runner_output["path"]).read_text())
        verifications = json.loads(Path(verifier_output["path"]).read_text())
        if not isinstance(candidates, list) or not isinstance(verifications, list) or len(candidates) != len(expected_bindings) or len(verifications) != len(expected_bindings):
            raise GateError("preflight candidate and verifier outputs do not cover the frozen run set")
        candidate_by_id = {row.get("run_id"): row for row in candidates if isinstance(row, dict)}
        verified_by_id = {row.get("run_id"): row for row in verifications if isinstance(row, dict)}
        score_by_id = {row.get("run_id"): row for row in scores if isinstance(row, dict)} if isinstance(scores, list) else {}
        if any(len(mapping) != len(expected_bindings) or set(mapping) != set(expected_ids)
               for mapping in (candidate_by_id, verified_by_id, score_by_id)):
            raise GateError("preflight stage outputs contain duplicate, missing or extra run identities")
        expected_per_control = {}
        for binding in expected_bindings:
            control = controls.get(binding["control_id"])
            if not isinstance(control, dict):
                raise GateError("frozen run refers to an unknown synthetic control")
            output = candidate_by_id[binding["run_id"]]
            verification = verified_by_id[binding["run_id"]]
            score = score_by_id[binding["run_id"]]
            training = control.get("training", {})
            assessment = control.get("assessment", {})
            materialized = synthetic_controls[binding["control_id"]]
            for record in (output, verification, score):
                if any(record.get(key) != binding[key] for key in ("run_id", "control_id", "method", "seed")):
                    raise GateError("preflight candidate, verifier or score has an incorrect run/control mapping")
            expected_assessment_bytes = compact_json({
                "control_id": binding["control_id"], "role": "assessment", "rows": assessment.get("synthetic_rows")}) + b"\n"
            expected_assessment_sha = sha256_bytes(expected_assessment_bytes)
            expected_training_bytes = compact_json({
                "control_id": binding["control_id"], "role": "training", "rows": training.get("synthetic_rows")}) + b"\n"
            expected_training_sha = sha256_bytes(expected_training_bytes)
            assessment_path = Path(materialized.get("assessment", {}).get("path", ""))
            training_path = Path(materialized.get("training", {}).get("path", ""))
            if (materialized.get("training", {}).get("id") != training.get("id")
                    or materialized.get("assessment", {}).get("id") != assessment.get("id")
                    or not training_path.is_file() or training_path.read_bytes() != expected_training_bytes
                    or not assessment_path.is_file() or assessment_path.read_bytes() != expected_assessment_bytes
                    or materialized.get("training", {}).get("sha256") != expected_training_sha
                    or materialized.get("assessment", {}).get("sha256") != expected_assessment_sha):
                raise GateError("synthetic assessment artifact differs from the pinned specification")
            expected_values = [row.get("expected") for row in assessment["synthetic_rows"]]
            if (output.get("training_artifact") != training.get("id") or output.get("training_sha256") != expected_training_sha
                    or not isinstance(output.get("predictions"), list) or output["predictions"] != expected_values):
                raise GateError("runner output does not match the frozen training identity and synthetic assessment")
            if (verification.get("status") != "verified"
                    or verification.get("candidate_sha256") != sha256_bytes(compact_json(output))):
                raise GateError("verifier output does not verify the exact candidate record")
            if (score.get("assessment_artifact") != assessment.get("id")
                    or score.get("assessment_sha256") != expected_assessment_sha
                    or score.get("exact") is not True
                    or score.get("checked_observations") != len(expected_values)):
                raise GateError("independent score does not match its frozen synthetic assessment")
            entry = expected_per_control.setdefault(binding["control_id"], {"run_count": 0, "exact_run_count": 0})
            entry["run_count"] += 1
            entry["exact_run_count"] += 1
        if (not isinstance(report, dict) or report.get("status") != "passed"
                or report.get("assessment_source") != "synthetic"
                or report.get("preflight_id") != spec.get("preflight_id")
                or report.get("run_count") != len(expected_ids)
                or report.get("scored_run_count") != len(expected_ids)
                or report.get("exact_run_count") != len(expected_ids)
                or report.get("per_control") != expected_per_control
                or report.get("run_ids") != expected_ids
                or report.get("run_bindings") != expected_bindings):
            raise GateError("preflight aggregator output does not match the independently checked synthetic results")
        # Recheck every receipt and referenced stage artifact immediately before
        # appending the one-shot event; do not bless a stale earlier read.
        verify_program(directory)
        evidence_sha256 = sha256_bytes(canonical_json({
            "protocol_sha256": protocol_sha,
            "stage_receipt_sha256": {name: sha256_file(Path(completed[run_id]["receipt_path"]))
                                     for name, run_id in stage_run_ids.items()},
            "scores_sha256": scores_record["sha256"], "report_sha256": report_record["sha256"],
        }))
        if continuation is None:
            payload = {"event": "qualified", "protocol_sha256": protocol_sha,
                       "preflight_evidence_sha256": evidence_sha256, "qualified_at_unix_ns": time.time_ns()}
        else:
            active_phase = state.get("active_coordinator_phase")
            required_phase = "primary_preflight" if qualification_path == "primary" else "recovery_preflight"
            if not isinstance(active_phase, dict) or active_phase.get("phase_id") != required_phase:
                raise GateError("continuation preflight qualification requires its measured coordinator phase")
            payload = {"event": "qualification_prepared", "protocol_sha256": protocol_sha,
                       "amendment_id": continuation["amendment_id"],
                       "qualification_path": qualification_path,
                       "evidence_sha256": evidence_sha256,
                       "prepared_at_unix_ns": time.time_ns()}
        state["ledger_head"] = _process_ledger_event(ledger_dir / "process-events.jsonl", payload, state["ledger_head"])
        if continuation is None:
            state["qualification_status"] = "qualified"
        state["event_count"] += 1
        _replace_process_budget(ledger_dir, state)
        return {"status": "qualified", "protocol_sha256": protocol_sha,
                "preflight_evidence_sha256": evidence_sha256}


def validate_protocol(protocol: dict) -> None:
    if protocol.get("schema") != 1:
        raise GateError("unsupported protocol schema")
    source_commit = protocol.get("source_commit")
    if not isinstance(source_commit, str) or len(source_commit) != 40 or any(c not in "0123456789abcdef" for c in source_commit):
        raise GateError("source_commit must be a full lowercase Git SHA1")
    protocol_id = protocol.get("protocol_id")
    if not isinstance(protocol_id, str) or not protocol_id or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_" for c in protocol_id):
        raise GateError("protocol_id must be a nonempty safe identifier")
    limits = protocol.get("limits")
    if not isinstance(limits, dict) or not REQUIRED_LIMITS.issubset(limits):
        raise GateError("protocol must declare all query and resource limits")
    for name in REQUIRED_LIMITS:
        value = limits[name]
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            raise GateError(f"limit {name} must be a finite number")
    if limits["max_solver_invocations"] < 0:
        raise GateError("max_solver_invocations must be nonnegative")
    if int(limits["max_solver_invocations"]) != limits["max_solver_invocations"]:
        raise GateError("max_solver_invocations must be integral")
    for name in ("max_address_space_bytes", "max_query_bytes", "max_output_bytes"):
        if limits[name] < 1:
            raise GateError(f"limit {name} must be positive")
        if int(limits[name]) != limits[name]:
            raise GateError(f"limit {name} must be integral")
    for name in ("max_query_wall_seconds", "max_query_cpu_seconds"):
        if limits[name] <= 0:
            raise GateError(f"limit {name} must be positive")
    if limits["max_solver_invocations"] and (limits["max_total_wall_seconds"] <= 0 or limits["max_total_cpu_seconds"] <= 0):
        raise GateError("aggregate time budgets must be positive when calls are permitted")
    if limits["max_solver_invocations"] and (limits["max_query_wall_seconds"] > limits["max_total_wall_seconds"] or limits["max_query_cpu_seconds"] > limits["max_total_cpu_seconds"]):
        raise GateError("aggregate time budgets must admit at least one complete query")
    program_budget = _validate_program_budget(protocol.get("program_budget"))
    if limits["max_solver_invocations"] > program_budget["max_lifetime_invocations"] - program_budget["historical_invocations"]:
        raise GateError("protocol invocation allowance exceeds program-wide remaining lifetime budget")
    if limits["max_query_cpu_seconds"] > program_budget["max_query_cpu_seconds"] or limits["max_query_wall_seconds"] > program_budget["max_query_wall_seconds"]:
        raise GateError("protocol per-query time limit exceeds program-wide ceiling")
    for limit, cap in (("max_address_space_bytes", "max_address_space_bytes"), ("max_query_bytes", "max_query_bytes"), ("max_output_bytes", "max_output_bytes")):
        if limits[limit] > program_budget[cap]:
            raise GateError(f"protocol {limit} exceeds program-wide ceiling")
    if limits["max_total_cpu_seconds"] > program_budget["max_new_cpu_seconds"] or limits["max_total_wall_seconds"] > program_budget["max_new_wall_seconds"]:
        raise GateError("protocol aggregate time budget exceeds program-wide prospective ceiling")
    if "execution_group" in protocol:
        execution_group = _validate_execution_group(protocol["execution_group"], program_budget)
    else:
        execution_group = None
    solver = protocol.get("solver")
    if not isinstance(solver, dict):
        raise GateError("solver identity is required")
    binary = Path(solver.get("executable", "")).resolve()
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise GateError("solver executable must exist and be executable")
    if solver.get("sha256") != sha256_file(binary):
        raise GateError("solver executable hash does not match frozen identity")
    if not isinstance(solver.get("version"), str) or not solver["version"].strip():
        raise GateError("solver version must be recorded without invoking it")
    fixed_args = solver.get("fixed_args", [])
    if not isinstance(fixed_args, list) or not all(isinstance(arg, str) for arg in fixed_args):
        raise GateError("solver.fixed_args must be a string array")
    environment = solver.get("environment")
    if not isinstance(environment, dict) or not all(isinstance(key, str) and isinstance(value, str) for key, value in environment.items()):
        raise GateError("solver.environment must be a fully specified string mapping")
    if not {"PATH", "LANG", "LC_ALL"}.issubset(environment):
        raise GateError("solver environment must pin PATH, LANG and LC_ALL")
    if protocol.get("subprocess_policy") != "forbidden":
        raise GateError("solver subprocesses must be explicitly forbidden")
    runtime_files = solver.get("runtime_files", [])
    if not isinstance(runtime_files, list):
        raise GateError("solver.runtime_files must be an array")
    for runtime_file in runtime_files:
        if not isinstance(runtime_file, dict) or not isinstance(runtime_file.get("path"), str):
            raise GateError("each solver runtime file needs a path and SHA256")
        library = Path(runtime_file["path"]).resolve()
        if not library.is_file() or runtime_file.get("sha256") != sha256_file(library):
            raise GateError("solver runtime file identity does not match frozen hash")
    if not isinstance(protocol.get("scope"), str) or not protocol["scope"].strip():
        raise GateError("protocol scope must be explicit")
    if not isinstance(protocol.get("hypothesis"), str) or not protocol["hypothesis"].strip():
        raise GateError("frozen falsifiable hypothesis is required")
    if not isinstance(protocol.get("grammar"), str) or not protocol["grammar"].strip():
        raise GateError("frozen candidate grammar is required")
    if not isinstance(protocol.get("acceptance_criteria"), list) or not protocol["acceptance_criteria"]:
        raise GateError("frozen advancement criteria are required")
    controls = protocol.get("controls")
    if not isinstance(controls, list) or len(controls) < 2:
        raise GateError("at least two independent controls are required for admission preflight")
    ids = set()
    for control in controls:
        if not isinstance(control, dict):
            raise GateError("each control must be an object")
        control_id = control.get("id")
        if not isinstance(control_id, str) or not control_id or control_id in ids:
            raise GateError("control IDs must be unique nonempty strings")
        ids.add(control_id)
        for key in ("training_sha256", "assessment_sha256"):
            checksum = control.get(key)
            if not isinstance(checksum, str) or len(checksum) != 64 or any(c not in "0123456789abcdef" for c in checksum):
                raise GateError(f"control {control_id} requires a SHA256 for {key}")
        if control["training_sha256"] == control["assessment_sha256"]:
            raise GateError(f"control {control_id} must separate training and assessment data")
    artifacts = protocol.get("frozen_artifacts")
    if not isinstance(artifacts, list) or not artifacts:
        raise GateError("protocol and dataset evidence artifacts must be hash-pinned")
    artifact_ids = set()
    for artifact in artifacts:
        if not isinstance(artifact, dict) or not {"id", "path", "sha256", "role"}.issubset(artifact) or not isinstance(artifact.get("path"), str):
            raise GateError("each frozen artifact needs unique id, path, SHA256 and role")
        if not isinstance(artifact["id"], str) or not artifact["id"] or artifact["id"] in artifact_ids:
            raise GateError("frozen artifact IDs must be unique nonempty strings")
        artifact_ids.add(artifact["id"])
        artifact_path = Path(artifact["path"]).resolve()
        if not artifact_path.is_file() or artifact.get("sha256") != sha256_file(artifact_path):
            raise GateError("frozen artifact identity does not match declared hash")
    artifact_hashes = {artifact["sha256"] for artifact in artifacts}
    artifacts_by_id = {artifact["id"]: artifact for artifact in artifacts}
    artifacts_by_path_sha = {
        (Path(artifact["path"]).resolve(), artifact["sha256"]): artifact
        for artifact in artifacts
    }
    required_gate_artifacts = (
        ("admission-tool", Path(__file__).resolve().with_name("scientific_pipeline_preflight.py"), "admission-tool"),
        ("execution-gate", Path(__file__).resolve(), "execution-gate"),
    )
    for artifact_id, path, role in required_gate_artifacts:
        entry = artifacts_by_id.get(artifact_id)
        resolved = path.resolve()
        if (
            entry is None
            or Path(entry["path"]).resolve() != resolved
            or entry.get("role") != role
            or entry.get("sha256") != sha256_file(resolved)
        ):
            raise GateError(f"frozen {role} tool identity is missing or stale")
    if execution_group and "stages" in execution_group:
        for stage in execution_group["stages"]:
            checker_sha256 = stage.get("checker_sha256")
            if checker_sha256 is not None and checker_sha256 not in artifact_hashes:
                raise GateError(f"checker for stage {stage['stage_id']} is not a pinned frozen artifact")
            if execution_group["attestation_required"] and checker_sha256 is not None:
                checker_path = next(Path(entry["path"]).resolve() for entry in artifacts if entry["sha256"] == checker_sha256)
                if not os.access(checker_path, os.X_OK):
                    raise GateError(f"checker for stage {stage['stage_id']} must be executable")
    for control in controls:
        for role in ("training", "assessment"):
            artifact_id = control.get(f"{role}_artifact")
            artifact = artifacts_by_id.get(artifact_id)
            if not isinstance(artifact_id, str) or artifact is None:
                raise GateError(f"control {control['id']} must name a frozen {role} artifact ID")
            if artifact.get("sha256") != control[f"{role}_sha256"] or artifact.get("role") != role or artifact.get("control_id") != control["id"]:
                raise GateError(f"control {control['id']} {role} artifact ID/hash/role mapping is inconsistent")
        if control["training_artifact"] == control["assessment_artifact"]:
            raise GateError(f"control {control['id']} must separate training and assessment artifacts")
    run_matrix = protocol.get("run_matrix")
    if not isinstance(run_matrix, list) or not run_matrix:
        raise GateError("explicit run_matrix with control/method/seed/artifact identities is required")
    run_ids = set()
    run_tuples = set()
    controls_by_id = {control["id"]: control for control in controls}
    for run_spec in run_matrix:
        required_run = {"run_id", "control_id", "method", "seed", "training_artifact", "assessment_artifact"}
        if not isinstance(run_spec, dict) or not required_run.issubset(run_spec):
            raise GateError("run_matrix rows require run/control/method/seed/training/assessment identity")
        control = controls_by_id.get(run_spec["control_id"])
        if (not isinstance(run_spec["run_id"], str) or not run_spec["run_id"] or run_spec["run_id"] in run_ids
                or not isinstance(run_spec["method"], str) or not run_spec["method"] or type(run_spec["seed"]) is not int
                or control is None or run_spec["training_artifact"] != control["training_artifact"]
                or run_spec["assessment_artifact"] != control["assessment_artifact"]):
            raise GateError("run_matrix contains duplicate, missing, or misassociated run/artifact identity")
        run_ids.add(run_spec["run_id"])
        run_key = (run_spec["control_id"], run_spec["method"], run_spec["seed"])
        if run_key in run_tuples:
            raise GateError("run_matrix repeats a control/method/seed tuple")
        run_tuples.add(run_key)
    if {run["control_id"] for run in run_matrix} != set(controls_by_id):
        raise GateError("run_matrix must exercise every declared control")
    pipeline_identity = protocol.get("pipeline_identity")
    if not isinstance(pipeline_identity, dict) or set(pipeline_identity) != {"runner", "verifier", "scorer", "aggregator"}:
        raise GateError("pipeline_identity must pin runner, verifier, scorer and aggregator")
    for stage, identity in pipeline_identity.items():
        if not isinstance(identity, dict) or not {"version", "executable", "sha256", "identity_files", "runtime_identity"}.issubset(identity):
            raise GateError(f"pipeline_identity.{stage} is incomplete")
        if not isinstance(identity["version"], str) or not identity["version"].strip():
            raise GateError(f"pipeline_identity.{stage}.version must be explicit")
        _require_sha256(identity["sha256"], f"pipeline_identity.{stage}.sha256")
        executable = Path(identity["executable"]).resolve()
        if not executable.is_file() or not os.access(executable, os.X_OK) or sha256_file(executable) != identity["sha256"]:
            raise GateError(f"pipeline_identity.{stage} executable identity is missing or mismatched")
        executable_artifact = artifacts_by_path_sha.get((executable, identity["sha256"]))
        if executable_artifact is None or executable_artifact.get("role") != "pipeline-executable":
            raise GateError(f"pipeline_identity.{stage} executable must be an exact frozen pipeline-executable artifact")
        if not isinstance(identity["runtime_identity"], str) or not identity["runtime_identity"].strip() or not isinstance(identity["identity_files"], list) or not identity["identity_files"]:
            raise GateError(f"pipeline_identity.{stage} must pin runtime and dependency files")
        for dependency in identity["identity_files"]:
            if not isinstance(dependency, dict) or not isinstance(dependency.get("path"), str):
                raise GateError(f"pipeline_identity.{stage} dependency needs a path and SHA256")
            dependency_path = Path(dependency["path"]).resolve()
            frozen_dependency = artifacts_by_path_sha.get((dependency_path, dependency.get("sha256")))
            if frozen_dependency is None or frozen_dependency.get("role") != "pipeline-dependency":
                raise GateError(f"pipeline_identity.{stage} dependency must exactly match a frozen pipeline-dependency artifact")
        if not identity["identity_files"]:
            raise GateError(f"pipeline_identity.{stage} must pin at least one adapter/dependency file")
    try:
        admission = verify_admission(protocol)
    except (PreflightError, OSError, ValueError, json.JSONDecodeError) as error:
        raise GateError(f"end-to-end synthetic admission preflight failed: {error}") from error
    admission_artifacts = {
        (entry.get("role"), Path(entry["path"]).resolve(), entry["sha256"])
        for entry in artifacts
    }
    for role, path, checksum in (
        ("admission_preflight_spec", admission["spec_path"], admission["spec_sha256"]),
        ("admission_preflight_receipt", admission["receipt_path"], admission["receipt_sha256"]),
    ):
        if (role, path, checksum) not in admission_artifacts:
            raise GateError(f"{role} must be included in frozen_artifacts with its exact path and SHA256")
    queries = protocol.get("queries")
    if not isinstance(queries, list) or (not queries and limits["max_solver_invocations"] != 0):
        raise GateError("a runnable protocol must freeze an allowlist of exact solver queries")
    query_ids = set()
    allowed_uses = 0
    for query in queries:
        if not isinstance(query, dict) or not isinstance(query.get("id"), str) or not query["id"] or query["id"] in query_ids:
            raise GateError("solver query IDs must be unique nonempty strings")
        query_ids.add(query["id"])
        query_path = Path(query.get("path", "")).resolve()
        if not query_path.is_file() or query.get("sha256") != sha256_file(query_path):
            raise GateError(f"frozen solver query {query['id']} is missing or has a hash mismatch")
        max_uses = query.get("max_uses")
        if isinstance(max_uses, bool) or not isinstance(max_uses, int) or max_uses < 1:
            raise GateError(f"frozen solver query {query['id']} requires a positive max_uses")
        if query["sha256"] not in artifact_hashes:
            raise GateError(f"frozen solver query {query['id']} is not a pinned artifact")
        query_args = query.get("args", [])
        if not isinstance(query_args, list) or not all(isinstance(arg, str) for arg in query_args):
            raise GateError(f"frozen solver query {query['id']} args must be a string array")
        if execution_group and "stages" in execution_group:
            stage_id = query.get("stage_id")
            retry_attempt = query.get("retry_attempt")
            stages_by_id = {stage["stage_id"]: stage for stage in execution_group["stages"]}
            if not isinstance(stage_id, str) or stage_id not in stages_by_id:
                raise GateError(f"frozen solver query {query['id']} needs a declared execution stage")
            if isinstance(retry_attempt, bool) or not isinstance(retry_attempt, int) or retry_attempt not in (0, 1):
                raise GateError(f"frozen solver query {query['id']} retry_attempt must be 0 or 1")
            if retry_attempt == 1 and not stages_by_id[stage_id]["retryable"]:
                raise GateError(f"frozen solver query {query['id']} cannot retry a nonretryable stage")
            retry_of = query.get("retry_of_run_id")
            if retry_attempt == 1:
                if not isinstance(retry_of, str) or re.fullmatch(r"call-[0-9]+-[0-9a-f]{32}", retry_of) is None:
                    raise GateError(f"frozen solver query {query['id']} retry requires a valid retry_of_run_id")
            elif retry_of is not None:
                raise GateError(f"frozen solver query {query['id']} initial attempt cannot declare retry_of_run_id")
        elif "stage_id" in query or "retry_attempt" in query:
            raise GateError("stage metadata is only valid with a staged execution_group")
        allowed_uses += max_uses
    if allowed_uses > limits["max_solver_invocations"]:
        raise GateError("frozen query uses exceed protocol invocation budget")


def freeze_protocol(source: Path, destination: Path) -> str:
    raw = source.read_bytes()
    protocol = json.loads(raw)
    validate_protocol(protocol)
    # Keep the frozen JSON byte-for-byte equivalent to the validated source
    # model (apart from canonical key ordering). The validators return derived
    # runtime fields such as resolved paths and historical partial charges;
    # inserting those fields here would change the protocol binding after its
    # end-to-end preflight had already attested it.
    program_budget = _validate_program_budget(protocol["program_budget"])
    if "execution_group" in protocol:
        execution_group = _validate_execution_group(
            protocol["execution_group"], program_budget
        )
    else:
        execution_group = None
    _initialize_program_ledger(program_budget)
    if "execution_group" in protocol:
        _initialize_execution_group(
            execution_group, program_budget["program_id"]
        )
    destination.mkdir(parents=True, exist_ok=False)
    _fsync_directory(destination.parent)
    encoded = canonical_json(protocol)
    digest = sha256_bytes(encoded)
    write_exclusive(destination / "protocol.json", encoded)
    write_exclusive(destination / "PROTOCOL.sha256", f"{digest}  protocol.json\n".encode())
    write_exclusive(destination / "budget.json", canonical_json({
        "schema": 1,
        "protocol_sha256": digest,
        "invocations_reserved": 0,
        "cpu_seconds_charged": 0.0,
        "wall_seconds_charged": 0.0,
        "pending": None,
        "event_count": 0,
        "ledger_head": "0" * 64,
        "terminal_status": None,
    }), 0o600)
    write_exclusive(destination / "events.jsonl", b"", 0o600)
    return digest


def _initialize_program_ledger(config: dict) -> None:
    """Create the one lifetime ledger, importing history exactly once."""
    directory = Path(config["ledger_dir"])
    directory.mkdir(parents=True, exist_ok=True)
    lock_fd = os.open(directory / "program-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    lock = os.fdopen(lock_fd, "r+b")
    fcntl.flock(lock, fcntl.LOCK_EX)
    identity = {
        "schema": 1,
        "program_id": config["program_id"],
        "historical_invocations": config["historical_invocations"],
        "historical_limit": config["historical_limit"],
        "historical_overrun": config["historical_overrun"],
        "historical_partial_wall_seconds": config["historical_partial_wall_seconds"],
        "max_lifetime_invocations": config["max_lifetime_invocations"],
        "max_new_cpu_seconds": config["max_new_cpu_seconds"],
        "max_new_wall_seconds": config["max_new_wall_seconds"],
        "max_query_cpu_seconds": config["max_query_cpu_seconds"],
        "max_query_wall_seconds": config["max_query_wall_seconds"],
        "max_address_space_bytes": config["max_address_space_bytes"],
        "max_query_bytes": config["max_query_bytes"],
        "max_output_bytes": config["max_output_bytes"],
        "historical_evidence": config["historical_evidence"],
    }
    state_path = directory / "program-budget.json"
    events_path = directory / "program-events.jsonl"
    if state_path.exists() or events_path.exists():
        state = json.loads(state_path.read_text())
        if any(state.get(key) != value for key, value in identity.items()):
            raise GateError("existing program ledger identity/ceiling differs; cannot reset or amend")
        _verify_program_ledger(directory, state)
        return
    event = {"event": "historical_import", **identity, "previous_hash": "0" * 64}
    event_hash = sha256_bytes(compact_json(event))
    event["event_sha256"] = event_hash
    write_exclusive(events_path, compact_json(event) + b"\n", 0o600)
    state = {
        **identity,
        "invocations_reserved": config["historical_invocations"],
        "new_cpu_seconds_charged": 0.0,
        "new_wall_seconds_charged": 0.0,
        "historical_cpu_seconds": None,
        "historical_total_wall_seconds": None,
        "historical_partial_wall_seconds": config["historical_partial_wall_seconds"],
        "pending": None,
        "event_count": 1,
        "ledger_head": event_hash,
    }
    write_exclusive(state_path, canonical_json(state), 0o600)


def _initialize_execution_group(config: dict, program_id: str) -> None:
    """Create or verify an immutable group ledger shared by protocol versions."""
    directory = Path(config["ledger_dir"])
    directory.mkdir(parents=True, exist_ok=True)
    lock_fd = os.open(directory / "group-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        identity = {
            "schema": 1,
            "program_id": program_id,
            "group_id": config["group_id"],
            "ledger_dir": str(directory.resolve()),
            "max_invocations": config["max_invocations"],
        }
        if "stages" in config:
            identity["stages"] = config["stages"]
            identity["max_syntax_retries"] = config["max_syntax_retries"]
            identity["attestation_required"] = config["attestation_required"]
            if config["attestation_required"]:
                identity["checker_limits"] = config["checker_limits"]
        state_path = directory / "group-budget.json"
        events_path = directory / "group-events.jsonl"
        if state_path.exists() or events_path.exists():
            if not state_path.is_file() or not events_path.is_file():
                raise GateError("execution-group ledger is incomplete; fail closed")
            state = json.loads(state_path.read_text())
            if any(state.get(key) != value for key, value in identity.items()):
                raise GateError("execution-group identity/ceiling differs; cannot reset or amend")
            _verify_execution_group(directory, state)
            return
        event = {"event": "created", **identity, "previous_hash": "0" * 64}
        event_hash = sha256_bytes(compact_json(event))
        event["event_sha256"] = event_hash
        write_exclusive(events_path, compact_json(event) + b"\n", 0o600)
        state = {
            **identity,
            "invocations_reserved": 0,
            "pending": None,
            "event_count": 1,
            "ledger_head": event_hash,
        }
        if "stages" in config:
            state.update({
                "stage_index": 0,
                "syntax_retries_used": 0,
                "retry_pending": None,
                "terminal_status": None,
                "attestation_pending": None,
                "attestation_attempt": None,
                "checker_attempts": 0,
                "checker_wall_seconds_charged": 0.0,
                "checker_cpu_seconds_charged": 0.0,
                "checker_peak_rss_bytes": 0,
            })
        write_exclusive(state_path, canonical_json(state), 0o600)


def _admit_group_stage(
    state: dict,
    group_config: dict,
    stage_id: str,
    retry_attempt: int,
    retry_of_run_id: str | None,
    query_sha256: str,
) -> dict:
    stages = group_config["stages"]
    if not isinstance(stage_id, str):
        raise GateError("execution stage ID must be a string")
    if isinstance(retry_attempt, bool) or not isinstance(retry_attempt, int) or retry_attempt not in (0, 1):
        raise GateError("execution stage retry attempt must be 0 or 1")
    if state.get("terminal_status") is not None:
        raise GateError(f"execution group halted after {state['terminal_status']}")
    if state.get("attestation_pending") is not None:
        raise GateError("checker attestation is required before stage advancement")
    stage_index = state["stage_index"]
    if stage_index >= len(stages):
        raise GateError("execution group has no remaining stages")
    stage = stages[stage_index]
    if stage_id != stage["stage_id"]:
        raise GateError(f"execution stage out of order; next stage is {stage['stage_id']}")
    pending_retry = state.get("retry_pending")
    if pending_retry is not None:
        if pending_retry["stage_id"] != stage_id or retry_attempt != 1:
            raise GateError("execution group admits only the pending syntax retry")
    elif retry_attempt != 0:
        raise GateError("execution group has no pending syntax retry")
    if retry_attempt == 1:
        if not stage["retryable"] or state["syntax_retries_used"] >= group_config["max_syntax_retries"]:
            raise GateError("execution-group syntax retry allowance exhausted")
        if retry_of_run_id != pending_retry["run_id"]:
            raise GateError("syntax retry must identify the failed invocation it replaces")
        if query_sha256 == pending_retry["query_sha256"]:
            raise GateError("syntax retry query must differ from the failed query bytes")
        state["syntax_retries_used"] += 1
        state["retry_pending"] = None
    elif retry_of_run_id is not None:
        raise GateError("initial stage attempt cannot declare retry_of_run_id")
    return stage


def _complete_group_stage(
    state: dict,
    group_config: dict,
    stage: dict,
    retry_attempt: int,
    run_id: str,
    protocol_sha256: str,
    query_sha256: str,
    stdout_sha256: str,
    stderr_sha256: str,
    status: str,
    parser_diagnostic: bool,
    protocol_dir: str,
    call_dir: str,
) -> None:
    if status == stage["expected_status"]:
        state["retry_pending"] = None
        if group_config.get("attestation_required") and stage.get("checker_sha256") is not None:
            state["attestation_pending"] = {
                "stage_id": stage["stage_id"],
                "run_id": run_id,
                "protocol_sha256": protocol_sha256,
                "query_sha256": query_sha256,
                "stdout_sha256": stdout_sha256,
                "stderr_sha256": stderr_sha256,
                "checker_sha256": stage["checker_sha256"],
                "protocol_dir": protocol_dir,
                "call_dir": call_dir,
            }
        else:
            _advance_group_stage(state, group_config)
        return
    if (
        status == "malformed_input"
        and parser_diagnostic
        and stage["retryable"]
        and retry_attempt == 0
        and state["syntax_retries_used"] < group_config["max_syntax_retries"]
    ):
        state["retry_pending"] = {
            "stage_id": stage["stage_id"],
            "run_id": run_id,
            "query_sha256": query_sha256,
        }
        return
    state["terminal_status"] = f"unexpected_{status}"


def _advance_group_stage(state: dict, group_config: dict) -> None:
    state["stage_index"] += 1
    state["retry_pending"] = None
    if state["stage_index"] == len(group_config["stages"]):
        state["terminal_status"] = "all_stages_completed"


def _verify_attestation_event(directory: Path, state: dict, group_config: dict, event: dict) -> None:
    pending = state.get("attestation_pending")
    attempt = state.get("attestation_attempt")
    if pending is None or attempt is None:
        raise GateError("execution-group attestation event has no pending checker result")
    stage = group_config["stages"][state["stage_index"]]
    receipt_path = Path(event.get("receipt_path", ""))
    result_path = Path(event.get("result_artifact_path", ""))
    if receipt_path.is_absolute() or result_path.is_absolute():
        raise GateError("attestation artifacts must use group-relative paths")
    receipt_path = (directory / receipt_path).resolve()
    result_path = (directory / result_path).resolve()
    if directory.resolve() not in receipt_path.parents or directory.resolve() not in result_path.parents:
        raise GateError("attestation artifact escapes its execution-group ledger")
    if not receipt_path.is_file() or not result_path.is_file():
        raise GateError("attestation receipt or result artifact is missing")
    if receipt_path.stat().st_size > MAX_ATTESTATION_RECEIPT_BYTES or result_path.stat().st_size > MAX_ATTESTATION_RESULT_BYTES:
        raise GateError("attestation artifacts exceed their fixed size limits")
    receipt_bytes = receipt_path.read_bytes()
    result_bytes = result_path.read_bytes()
    if sha256_bytes(receipt_bytes) != event.get("receipt_sha256"):
        raise GateError("attestation receipt hash mismatch")
    receipt = json.loads(receipt_bytes)
    required = {
        "stage_id", "run_id", "protocol_sha256", "query_sha256", "stdout_sha256",
        "stderr_sha256", "checker_sha256", "attempt_id", "passed", "result_artifact_sha256", "checker_stderr_sha256",
    }
    if not isinstance(receipt, dict) or set(receipt) != required or not isinstance(receipt.get("passed"), bool):
        raise GateError("attestation receipt has an invalid schema")
    if sha256_bytes(result_bytes) != receipt.get("result_artifact_sha256"):
        raise GateError("attestation result artifact hash mismatch")
    if receipt.get("checker_stderr_sha256") != event.get("checker_stderr_sha256"):
        raise GateError("receipt checker-stderr hash mismatch")
    if event.get("checker_stdout_path") != event.get("result_artifact_path"):
        raise GateError("checker stdout must be the immutable result artifact")
    if sha256_bytes(result_bytes) != event.get("checker_stdout_sha256"):
        raise GateError("checker stdout hash differs from result artifact")
    _verify_checker_output_artifact(directory, event, "checker_stderr_path", "checker_stderr_sha256")
    expected = {
        "stage_id": pending["stage_id"],
        "run_id": pending["run_id"],
        "protocol_sha256": pending["protocol_sha256"],
        "query_sha256": pending["query_sha256"],
        "stdout_sha256": pending["stdout_sha256"],
        "stderr_sha256": pending["stderr_sha256"],
        "checker_sha256": pending["checker_sha256"],
    }
    if any(receipt.get(key) != value for key, value in expected.items()):
        raise GateError("attestation receipt does not match the pending run or pinned checker")
    if any(event.get(key) != value for key, value in expected.items()):
        raise GateError("attestation event does not match pending run metadata")
    if event.get("result_artifact_sha256") != receipt["result_artifact_sha256"] or event.get("passed") is not receipt["passed"]:
        raise GateError("attestation event does not match its receipt")
    if event.get("status") != ("successful" if receipt["passed"] else "failed"):
        raise GateError("attestation pass/fail classification disagrees with checker exit status")
    limits = group_config["checker_limits"]
    if (
        event["wall_seconds"] > limits["max_checker_wall_seconds"]
        or event["cpu_seconds"] > limits["max_checker_cpu_seconds"]
        or event["peak_rss_bytes"] > limits["max_process_group_rss_bytes"]
    ):
        raise GateError("successful checker exceeded an individual resource limit")
    if stage["stage_id"] != pending["stage_id"]:
        raise GateError("attestation stage differs from current group stage")
    if event.get("checker_stdout_sha256") != receipt["result_artifact_sha256"]:
        raise GateError("attestation result was not produced by the pinned checker")
    if event.get("attempt_id") != attempt["attempt_id"] or receipt.get("attempt_id") != attempt["attempt_id"]:
        raise GateError("attestation does not match its one-shot checker reservation")
    _charge_checker_result(state, event)
    state["attestation_pending"] = None
    state["attestation_attempt"] = None
    if receipt["passed"]:
        _advance_group_stage(state, group_config)
    else:
        state["terminal_status"] = "attestation_failed"


def _verify_checker_output_artifact(directory: Path, event: dict, path_key: str, hash_key: str) -> bytes:
    relative = Path(event.get(path_key, ""))
    if relative.is_absolute():
        raise GateError("checker output artifacts must use group-relative paths")
    path = (directory / relative).resolve()
    if directory.resolve() not in path.parents or not path.is_file():
        raise GateError("checker output artifact is missing or escapes the execution-group ledger")
    if path.stat().st_size > MAX_ATTESTATION_RESULT_BYTES:
        raise GateError("checker output artifact exceeds its fixed size limit")
    data = path.read_bytes()
    if sha256_bytes(data) != event.get(hash_key):
        raise GateError("checker output artifact hash mismatch")
    return data


def _charge_checker_result(state: dict, event: dict) -> None:
    for name in ("wall_seconds", "cpu_seconds", "charged_wall_seconds", "charged_cpu_seconds"):
        value = event.get(name)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or value < 0 or not math.isfinite(value):
            raise GateError("checker work event has invalid time accounting")
    rss = event.get("peak_rss_bytes")
    if isinstance(rss, bool) or not isinstance(rss, int) or rss < 0:
        raise GateError("checker work event has invalid RSS accounting")
    state["checker_wall_seconds_charged"] += float(event["charged_wall_seconds"])
    state["checker_cpu_seconds_charged"] += float(event["charged_cpu_seconds"])
    state["checker_peak_rss_bytes"] = max(state["checker_peak_rss_bytes"], rss)


def _reserve_checker_attempt(state: dict, event: dict, group_config: dict) -> None:
    pending = state.get("attestation_pending")
    if pending is None or state.get("attestation_attempt") is not None:
        raise GateError("checker reservation has no unique pending attestation")
    if state.get("terminal_status") is not None:
        raise GateError("checker reservation follows a terminal group result")
    expected = {
        "stage_id": pending["stage_id"], "run_id": pending["run_id"],
        "protocol_sha256": pending["protocol_sha256"], "query_sha256": pending["query_sha256"],
        "stdout_sha256": pending["stdout_sha256"], "stderr_sha256": pending["stderr_sha256"],
        "checker_sha256": pending["checker_sha256"],
    }
    if any(event.get(key) != value for key, value in expected.items()):
        raise GateError("checker reservation does not match its pending run")
    if state["checker_attempts"] >= group_config["checker_limits"]["max_attempts"]:
        raise GateError("checker attempt budget exhausted")
    state["checker_attempts"] += 1
    state["attestation_attempt"] = {"attempt_id": event["attempt_id"], **expected}


def _set_checker_limits(limits: dict) -> None:
    resource.setrlimit(resource.RLIMIT_AS, (int(limits["max_address_space_bytes"]),) * 2)
    cpu = max(1, math.ceil(float(limits["max_checker_cpu_seconds"])))
    resource.setrlimit(resource.RLIMIT_CPU, (cpu, cpu + 1))
    # stdout and stderr are drained through pipes; this also caps files a
    # checker might create in its private working directory.
    resource.setrlimit(resource.RLIMIT_FSIZE, (int(limits["max_output_bytes"]),) * 2)


def _sample_checker_group(pid: int, stop: threading.Event, result: dict, limits: dict) -> None:
    """Sample summed RSS/CPU for the checker process group, including bridge children."""
    ticks = os.sysconf("SC_CLK_TCK")
    rss_limit = int(limits["max_process_group_rss_bytes"])
    cpu_limit = float(limits["max_checker_cpu_seconds"])
    while not stop.is_set():
        total_rss = 0
        total_cpu = 0.0
        for stat_path in Path("/proc").glob("[0-9]*/stat"):
            try:
                stat_text = stat_path.read_text()
                tail = stat_text[stat_text.rfind(")") + 2 :].split()
                if int(tail[2]) != pid:
                    continue
                # utime/stime plus cutime/cstime account for descendants that
                # the parent has already reaped and which no longer appear in
                # /proc as members of this process group.
                total_cpu += sum(int(tail[index]) for index in (11, 12, 13, 14)) / ticks
                status = stat_path.parent.joinpath("status").read_text()
                for line in status.splitlines():
                    if line.startswith("VmRSS:"):
                        total_rss += int(line.split()[1]) * 1024
                        break
            except (FileNotFoundError, ProcessLookupError, PermissionError, ValueError, IndexError):
                continue
        result["peak_process_group_rss_bytes"] = max(result.get("peak_process_group_rss_bytes", 0), total_rss)
        result["peak_process_group_cpu_seconds"] = max(result.get("peak_process_group_cpu_seconds", 0.0), total_cpu)
        if total_rss > rss_limit:
            result["memory_limit_reached"] = True
            try:
                os.killpg(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            break
        if total_cpu >= cpu_limit:
            result["cpu_limit_reached"] = True
            try:
                os.killpg(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            break
        stop.wait(0.01)


def _run_checker(command: list[str], context: bytes, env: dict, limits: dict) -> dict:
    started = time.monotonic()
    before_cpu = resource.getrusage(resource.RUSAGE_CHILDREN)
    cap = int(limits["max_output_bytes"])
    output_total = [0]
    output_lock = threading.Lock()
    exceeded = threading.Event()
    captured: dict[str, bytes] = {}
    sampled: dict = {}
    proc = subprocess.Popen(
        command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        env=env, cwd=Path(command[0]).resolve().parent,
        preexec_fn=lambda: _set_checker_limits(limits), start_new_session=True,
    )
    process_group = proc.pid
    stop = threading.Event()
    sampler = None
    readers = []
    writer = None
    started_threads = []
    completed = False
    failure = None
    timed_out = False
    orphaned = False
    try:
        stop = threading.Event()
        sampler = threading.Thread(
            target=_sample_checker_group,
            args=(proc.pid, stop, sampled, limits),
            daemon=True,
        )
        sampler.start()
        started_threads.append(sampler)

        def drain(name: str, pipe) -> None:
            data = bytearray()
            while True:
                chunk = pipe.read(65536)
                if not chunk:
                    break
                with output_lock:
                    accepted = min(len(chunk), max(0, cap - output_total[0]))
                    data.extend(chunk[:accepted])
                    output_total[0] += accepted
                    if accepted != len(chunk):
                        exceeded.set()
                        try:
                            os.killpg(proc.pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                if exceeded.is_set():
                    break
            captured[name] = bytes(data)

        readers = [
            threading.Thread(target=drain, args=("stdout", proc.stdout), daemon=True),
            threading.Thread(target=drain, args=("stderr", proc.stderr), daemon=True),
        ]
        for reader in readers:
            reader.start()
            started_threads.append(reader)
        writer = threading.Thread(target=lambda: _write_pipe(proc.stdin, context), daemon=True)
        writer.start()
        started_threads.append(writer)
        try:
            proc.wait(timeout=float(limits["max_checker_wall_seconds"]))
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait()
        deadline = started + float(limits["max_checker_wall_seconds"])
        for thread in [*readers, writer]:
            thread.join(timeout=max(0.0, deadline - time.monotonic()))
        if any(thread.is_alive() for thread in [*readers, writer]):
            timed_out = True
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            for thread in [*readers, writer]:
                thread.join()
        for pipe in (proc.stdin, proc.stdout, proc.stderr):
            if pipe is not None and not pipe.closed:
                pipe.close()
        if not timed_out:
            try:
                os.killpg(proc.pid, 0)
            except ProcessLookupError:
                pass
            else:
                orphaned = True
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        stop.set()
        sampler.join()
        after_cpu = resource.getrusage(resource.RUSAGE_CHILDREN)
        wall = time.monotonic() - started
        cpu = max(
            0.0,
            (after_cpu.ru_utime + after_cpu.ru_stime) - (before_cpu.ru_utime + before_cpu.ru_stime),
            float(sampled.get("peak_process_group_cpu_seconds", 0.0)),
        )
        status = "successful" if proc.returncode == 0 else "failed" if proc.returncode == 1 else "resource_exhausted"
        if timed_out:
            status = "timed_out"
        elif exceeded.is_set() or sampled.get("cpu_limit_reached") or sampled.get("memory_limit_reached"):
            status = "resource_exhausted"
        elif orphaned:
            status = "invalid"
        cpu_limit = float(limits["max_checker_cpu_seconds"])
        wall_limit = float(limits["max_checker_wall_seconds"])
        if wall > wall_limit:
            status = "timed_out"
        elif cpu > cpu_limit:
            status = "resource_exhausted"
        charged_cpu = max(cpu, cpu_limit) if status in ("timed_out", "resource_exhausted") else cpu
        charged_wall = max(wall, wall_limit) if status in ("timed_out", "resource_exhausted") else wall
        result = {
            "status": status, "returncode": proc.returncode,
            "stdout": captured.get("stdout", b""), "stderr": captured.get("stderr", b""),
            "wall_seconds": wall, "cpu_seconds": cpu,
            "charged_wall_seconds": charged_wall, "charged_cpu_seconds": charged_cpu,
            "peak_rss_bytes": sampled.get("peak_process_group_rss_bytes") or 0,
            "orphaned_process_group": orphaned, "output_exceeded": exceeded.is_set(),
        }
        completed = True
        return result
    except Exception as error:
        failure = f"{type(error).__name__}: {error}"
    finally:
        if not completed:
            try:
                os.killpg(process_group, signal.SIGKILL)
            except ProcessLookupError:
                pass
            try:
                proc.wait()
            except Exception:
                pass
            stop.set()
            for thread in started_threads:
                thread.join(timeout=1)
            for pipe in (proc.stdin, proc.stdout, proc.stderr):
                if pipe is not None and not pipe.closed:
                    pipe.close()
    after_cpu = resource.getrusage(resource.RUSAGE_CHILDREN)
    wall = time.monotonic() - started
    cpu = max(
        0.0,
        (after_cpu.ru_utime + after_cpu.ru_stime) - (before_cpu.ru_utime + before_cpu.ru_stime),
        float(sampled.get("peak_process_group_cpu_seconds", 0.0)),
    )
    stderr = captured.get("stderr", b"")
    diagnostic = (failure or "post-launch checker harness failure").encode("utf-8", errors="replace")
    stderr = (stderr + b"\n" + diagnostic)[:cap]
    cpu_limit = float(limits["max_checker_cpu_seconds"])
    wall_limit = float(limits["max_checker_wall_seconds"])
    return {
        "status": "harness_error", "returncode": proc.returncode,
        "stdout": captured.get("stdout", b""), "stderr": stderr,
        "wall_seconds": wall, "cpu_seconds": cpu,
        "charged_wall_seconds": max(wall, wall_limit),
        "charged_cpu_seconds": max(cpu, cpu_limit),
        "peak_rss_bytes": sampled.get("peak_process_group_rss_bytes") or 0,
        "orphaned_process_group": True, "output_exceeded": exceeded.is_set(),
    }



def _write_pipe(pipe, data: bytes) -> None:
    try:
        pipe.write(data)
        pipe.close()
    except (BrokenPipeError, OSError):
        pass


def attest_group(protocol_dir: Path) -> dict:
    """Reserve and execute exactly one pinned checker for a pending stage."""
    protocol_dir = protocol_dir.resolve()
    protocol, digest = read_frozen_protocol(protocol_dir)
    group_config = protocol.get("execution_group")
    if not group_config or not group_config.get("attestation_required"):
        raise GateError("protocol has no checker-attested execution group")
    group_dir = Path(group_config["ledger_dir"]).resolve()
    lock_fd = os.open(group_dir / "group-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state_path = group_dir / "group-budget.json"
        state = json.loads(state_path.read_text())
        _verify_execution_group(group_dir, state)
        pending = state.get("attestation_pending")
        if pending is None:
            raise GateError("no checker result is pending attestation")
        if state.get("attestation_attempt") is not None:
            raise GateError("one-shot checker attempt is already reserved; refusing retry")
        if digest != pending["protocol_sha256"] or str(protocol_dir) != pending["protocol_dir"]:
            raise GateError("attestation must use the exact protocol version that produced the pending result")
        limits = group_config["checker_limits"]
        if state["checker_attempts"] >= limits["max_attempts"]:
            raise GateError("checker attempt budget exhausted")
        if state["checker_wall_seconds_charged"] + limits["max_checker_wall_seconds"] > limits["max_total_wall_seconds"]:
            raise GateError("checker aggregate wall budget exhausted")
        if state["checker_cpu_seconds_charged"] + limits["max_checker_cpu_seconds"] > limits["max_total_cpu_seconds"]:
            raise GateError("checker aggregate CPU budget exhausted")
        expected = {
            key: pending[key] for key in (
                "stage_id", "run_id", "protocol_sha256", "query_sha256", "stdout_sha256", "stderr_sha256", "checker_sha256",
            )
        }
        artifacts = {entry["sha256"]: Path(entry["path"]).resolve() for entry in protocol["frozen_artifacts"]}
        checker = artifacts.get(pending["checker_sha256"])
        if checker is None or not checker.is_file() or not os.access(checker, os.X_OK):
            raise GateError("pinned checker is missing or not executable")
        checker_bytes = checker.read_bytes()
        if sha256_bytes(checker_bytes) != pending["checker_sha256"]:
            raise GateError("pinned checker changed after protocol freeze")
        attempt_id = uuid.uuid4().hex
        reservation = {"event": "attestation_reserved", "attempt_id": attempt_id, **expected, "reserved_at_unix_ns": time.time_ns()}
        _reserve_checker_attempt(state, reservation, group_config)
        state["ledger_head"] = _append_event(group_dir / "group-events.jsonl", reservation, state["ledger_head"])
        state["event_count"] += 1
        _replace_execution_group(group_dir, state)

        checker_temp = tempfile.TemporaryDirectory(prefix="genetik-pinned-checker-")
        checker_copy = Path(checker_temp.name) / checker.name
        write_exclusive(checker_copy, checker_bytes, 0o500)
        call_dir = Path(pending["call_dir"]).resolve()
        context = {
            **expected,
            "stdout_path": str(call_dir / "stdout.bin"),
            "stderr_path": str(call_dir / "stderr.bin"),
            "query_path": str(call_dir / "query.bin"),
            "protocol_dir": str(protocol_dir),
            "group_dir": str(group_dir),
            "frozen_artifacts": protocol["frozen_artifacts"],
        }
        env = {"PATH": "/usr/bin:/bin", "LANG": "C", "LC_ALL": "C"}
        try:
            checker_result = _run_checker([str(checker_copy)], canonical_json(context), env, limits)
        except OSError as error:
            checker_result = {
                "status": "launch_error", "returncode": None, "stdout": b"", "stderr": str(error).encode(),
                "wall_seconds": 0.0, "cpu_seconds": 0.0, "charged_wall_seconds": 0.0,
                "charged_cpu_seconds": 0.0, "peak_rss_bytes": 0,
            }
        except Exception as error:
            checker_result = {
                "status": "harness_error", "returncode": None, "stdout": b"", "stderr": str(error).encode(),
                "wall_seconds": 0.0, "cpu_seconds": 0.0, "charged_wall_seconds": 0.0,
                "charged_cpu_seconds": 0.0, "peak_rss_bytes": 0,
            }
        finally:
            checker_temp.cleanup()
        state = json.loads(state_path.read_text())
        _verify_execution_group(group_dir, state)
        if state.get("attestation_attempt", {}).get("attempt_id") != attempt_id:
            raise GateError("one-shot checker reservation changed during execution")
        if checker_result["status"] not in ("successful", "failed"):
            (group_dir / "attestations").mkdir(mode=0o700, exist_ok=True)
            stdout_rel = str(Path("attestations") / f"{pending['run_id']}.checker.stdout.bin")
            stderr_rel = str(Path("attestations") / f"{pending['run_id']}.checker.stderr.bin")
            write_exclusive(group_dir / stdout_rel, checker_result["stdout"], 0o444)
            write_exclusive(group_dir / stderr_rel, checker_result["stderr"], 0o444)
            failure = {
                "event": "attestation_failed", "attempt_id": attempt_id, **expected,
                "status": checker_result["status"], "wall_seconds": checker_result["wall_seconds"],
                "cpu_seconds": checker_result["cpu_seconds"],
                "charged_wall_seconds": checker_result["charged_wall_seconds"],
                "charged_cpu_seconds": checker_result["charged_cpu_seconds"],
                "peak_rss_bytes": checker_result["peak_rss_bytes"],
                "checker_stdout_path": stdout_rel,
                "checker_stdout_sha256": sha256_bytes(checker_result["stdout"]),
                "checker_stderr_path": stderr_rel,
                "checker_stderr_sha256": sha256_bytes(checker_result["stderr"]),
            }
            _charge_checker_result(state, failure)
            state["attestation_pending"] = None
            state["attestation_attempt"] = None
            state["terminal_status"] = "attestation_failed"
            state["ledger_head"] = _append_event(group_dir / "group-events.jsonl", failure, state["ledger_head"])
            state["event_count"] += 1
            _replace_execution_group(group_dir, state)
            return {"passed": False, "status": checker_result["status"], "attempt_id": attempt_id}

        if not checker_result["stdout"]:
            checker_result["status"] = "invalid"
            (group_dir / "attestations").mkdir(mode=0o700, exist_ok=True)
            stdout_rel = str(Path("attestations") / f"{pending['run_id']}.checker.stdout.bin")
            stderr_rel = str(Path("attestations") / f"{pending['run_id']}.checker.stderr.bin")
            write_exclusive(group_dir / stdout_rel, checker_result["stdout"], 0o444)
            write_exclusive(group_dir / stderr_rel, checker_result["stderr"], 0o444)
            failure = {
                "event": "attestation_failed", "attempt_id": attempt_id, **expected,
                "status": "invalid", "wall_seconds": checker_result["wall_seconds"],
                "cpu_seconds": checker_result["cpu_seconds"],
                "charged_wall_seconds": checker_result["charged_wall_seconds"],
                "charged_cpu_seconds": checker_result["charged_cpu_seconds"],
                "peak_rss_bytes": checker_result["peak_rss_bytes"],
                "checker_stdout_path": stdout_rel,
                "checker_stdout_sha256": sha256_bytes(checker_result["stdout"]),
                "checker_stderr_path": stderr_rel,
                "checker_stderr_sha256": sha256_bytes(checker_result["stderr"]),
            }
            _charge_checker_result(state, failure)
            state["attestation_pending"] = None
            state["attestation_attempt"] = None
            state["terminal_status"] = "attestation_failed"
            state["ledger_head"] = _append_event(group_dir / "group-events.jsonl", failure, state["ledger_head"])
            state["event_count"] += 1
            _replace_execution_group(group_dir, state)
            return {"passed": False, "status": "invalid", "attempt_id": attempt_id}

        passed = checker_result["status"] == "successful"
        result_bytes = checker_result["stdout"]
        result_hash = sha256_bytes(result_bytes)
        receipt = {
            **expected, "attempt_id": attempt_id, "passed": passed,
            "result_artifact_sha256": result_hash,
            "checker_stderr_sha256": sha256_bytes(checker_result["stderr"]),
        }
        receipt_bytes = canonical_json(receipt)
        relative_base = Path("attestations") / pending["run_id"]
        receipt_rel = str(relative_base.with_suffix(".receipt.json"))
        result_rel = str(relative_base.with_suffix(".result.bin"))
        (group_dir / "attestations").mkdir(mode=0o700, exist_ok=True)
        result_dest = group_dir / result_rel
        receipt_dest = group_dir / receipt_rel
        write_exclusive(result_dest, result_bytes, 0o444)
        checker_stderr_rel = str(Path("attestations") / f"{pending['run_id']}.checker.stderr.bin")
        write_exclusive(group_dir / checker_stderr_rel, checker_result["stderr"], 0o444)
        write_exclusive(receipt_dest, receipt_bytes, 0o444)
        event = {
            "event": "attested", **expected, "attempt_id": attempt_id, "passed": passed,
            "status": checker_result["status"],
            "result_artifact_sha256": result_hash, "checker_stdout_sha256": result_hash,
            "checker_stdout_path": result_rel,
            "checker_stderr_path": checker_stderr_rel,
            "checker_stderr_sha256": sha256_bytes(checker_result["stderr"]),
            "receipt_path": receipt_rel, "result_artifact_path": result_rel,
            "receipt_sha256": sha256_bytes(receipt_bytes),
            "wall_seconds": checker_result["wall_seconds"], "cpu_seconds": checker_result["cpu_seconds"],
            "charged_wall_seconds": checker_result["charged_wall_seconds"],
            "charged_cpu_seconds": checker_result["charged_cpu_seconds"],
            "peak_rss_bytes": checker_result["peak_rss_bytes"],
        }
        config_keys = ("stages", "max_syntax_retries", "attestation_required", "checker_limits")
        _verify_attestation_event(group_dir, state, {key: state[key] for key in config_keys}, event)
        state["ledger_head"] = _append_event(group_dir / "group-events.jsonl", event, state["ledger_head"])
        state["event_count"] += 1
        _replace_execution_group(group_dir, state)
        return {"passed": passed, "status": checker_result["status"], "attempt_id": attempt_id, "result_artifact_sha256": result_hash}


def _verify_execution_group(directory: Path, state: dict) -> None:
    events_path = directory / "group-events.jsonl"
    state_path = directory / "group-budget.json"
    if not events_path.is_file() or not state_path.is_file():
        raise GateError("execution-group ledger is incomplete; fail closed")
    previous = "0" * 64
    events = []
    for line in events_path.read_bytes().splitlines():
        event = json.loads(line)
        claimed = event.pop("event_sha256", None)
        if event.get("previous_hash") != previous or claimed != sha256_bytes(compact_json(event)):
            raise GateError("execution-group event hash-chain mismatch")
        previous = claimed
        event["event_sha256"] = claimed
        events.append(event)
    if len(events) != state.get("event_count") or previous != state.get("ledger_head"):
        raise GateError("execution-group state and events disagree")
    identity_fields = ["schema", "program_id", "group_id", "ledger_dir", "max_invocations"]
    if "stages" in state:
        identity_fields.extend(("stages", "max_syntax_retries", "attestation_required"))
        if state.get("attestation_required"):
            identity_fields.append("checker_limits")
    if (
        len(events) < 1
        or events[0].get("event") != "created"
        or any(state.get(key) != events[0].get(key) for key in identity_fields)
    ):
        raise GateError("execution-group identity is missing or inconsistent")
    reservations = [event for event in events if event.get("event") == "reserved"]
    completed = [event for event in events if event.get("event") == "completed"]
    reserved_ids = {event.get("run_id") for event in reservations}
    completed_ids = {event.get("run_id") for event in completed}
    if (
        len(reserved_ids) != len(reservations)
        or len(completed_ids) != len(completed)
        or not completed_ids.issubset(reserved_ids)
        or state.get("invocations_reserved") != len(reservations)
    ):
        raise GateError("execution-group invocation ledger is inconsistent")
    unresolved = reserved_ids - completed_ids
    expected_pending = next(iter(unresolved)) if len(unresolved) == 1 else None
    if len(unresolved) > 1 or state.get("pending") != expected_pending:
        raise GateError("execution-group invocation unresolved; fail closed")
    if state["invocations_reserved"] > state["max_invocations"]:
        raise GateError("execution-group invocation ceiling was exceeded")
    if "stages" in state:
        config_keys = ["stages", "max_syntax_retries", "attestation_required"]
        if state.get("attestation_required"):
            config_keys.append("checker_limits")
        group_config = {key: state[key] for key in config_keys}
        replay = {
            "stage_index": 0,
            "syntax_retries_used": 0,
            "retry_pending": None,
            "terminal_status": None,
            "attestation_pending": None,
            "attestation_attempt": None,
            "checker_attempts": 0,
            "checker_wall_seconds_charged": 0.0,
            "checker_cpu_seconds_charged": 0.0,
            "checker_peak_rss_bytes": 0,
        }
        pending_reservation = None
        for event in events[1:]:
            if event.get("event") == "reserved":
                if pending_reservation is not None:
                    raise GateError("execution-group contains overlapping staged reservations")
                _admit_group_stage(
                    replay,
                    group_config,
                    event.get("stage_id"),
                    event.get("retry_attempt"),
                    event.get("retry_of_run_id"),
                    event.get("query_sha256"),
                )
                pending_reservation = event
            elif event.get("event") == "completed":
                if pending_reservation is None or event.get("run_id") != pending_reservation.get("run_id"):
                    raise GateError("execution-group completion has no matching staged reservation")
                stage = group_config["stages"][replay["stage_index"]]
                if (
                    event.get("stage_id") != stage["stage_id"]
                    or event.get("retry_attempt") != pending_reservation.get("retry_attempt")
                    or event.get("retry_of_run_id") != pending_reservation.get("retry_of_run_id")
                    or event.get("query_sha256") != pending_reservation.get("query_sha256")
                    or event.get("protocol_sha256") != pending_reservation.get("protocol_sha256")
                    or not isinstance(event.get("stdout_sha256"), str)
                    or not isinstance(event.get("stderr_sha256"), str)
                    or event.get("checker_sha256") != stage.get("checker_sha256")
                    or not isinstance(event.get("protocol_dir"), str)
                    or not isinstance(event.get("call_dir"), str)
                ):
                    raise GateError("execution-group completion stage metadata is inconsistent")
                frozen_dir = Path(event["protocol_dir"]).resolve()
                call_dir = Path(event["call_dir"]).resolve()
                if frozen_dir.name != "" and frozen_dir not in call_dir.parents:
                    raise GateError("execution-group call output escapes its frozen protocol directory")
                if call_dir.name != pending_reservation["run_id"]:
                    raise GateError("execution-group call directory does not match its run ID")
                for output_name, expected_hash in (("stdout.bin", event["stdout_sha256"]), ("stderr.bin", event["stderr_sha256"])):
                    output_path = call_dir / output_name
                    if not output_path.is_file() or sha256_file(output_path) != expected_hash:
                        raise GateError("execution-group retained solver output hash mismatch")
                _complete_group_stage(
                    replay,
                    group_config,
                    stage,
                    pending_reservation["retry_attempt"],
                    pending_reservation["run_id"],
                    pending_reservation["protocol_sha256"],
                    pending_reservation["query_sha256"],
                    event.get("stdout_sha256"),
                    event.get("stderr_sha256"),
                    event.get("status"),
                    event.get("parser_diagnostic") is True,
                    event.get("protocol_dir"),
                    event.get("call_dir"),
                )
                pending_reservation = None
            elif event.get("event") == "attested":
                _verify_attestation_event(directory, replay, group_config, event)
            elif event.get("event") == "attestation_reserved":
                _reserve_checker_attempt(replay, event, group_config)
            elif event.get("event") == "attestation_failed":
                attempt = replay.get("attestation_attempt")
                if (
                    attempt is None
                    or event.get("attempt_id") != attempt["attempt_id"]
                    or event.get("status") not in {"timed_out", "resource_exhausted", "invalid", "launch_error", "harness_error"}
                    or any(event.get(key) != attempt.get(key) for key in (
                        "stage_id", "run_id", "protocol_sha256", "query_sha256", "stdout_sha256", "stderr_sha256", "checker_sha256",
                    ))
                ):
                    raise GateError("checker failure has no matching one-shot reservation")
                _verify_checker_output_artifact(directory, event, "checker_stdout_path", "checker_stdout_sha256")
                _verify_checker_output_artifact(directory, event, "checker_stderr_path", "checker_stderr_sha256")
                _charge_checker_result(replay, event)
                replay["attestation_pending"] = None
                replay["attestation_attempt"] = None
                replay["terminal_status"] = "attestation_failed"
            else:
                raise GateError("execution-group contains an unknown event")
        for key in ("stage_index", "syntax_retries_used", "retry_pending", "terminal_status", "attestation_pending", "attestation_attempt"):
            if state.get(key) != replay[key]:
                raise GateError("execution-group staged state disagrees with its event history")
        for key in ("checker_attempts", "checker_wall_seconds_charged", "checker_cpu_seconds_charged", "checker_peak_rss_bytes"):
            if state.get(key) != replay[key]:
                raise GateError("execution-group checker accounting disagrees with event history")
        attempt_events = [event for event in events if event.get("event") == "attestation_reserved"]
        if state.get("checker_attempts") != len(attempt_events):
            raise GateError("checker attempt count disagrees with event history")
        completed_checker_events = [event for event in events if event.get("event") in ("attested", "attestation_failed")]
        wall = sum(float(event.get("charged_wall_seconds", 0)) for event in completed_checker_events)
        cpu = sum(float(event.get("charged_cpu_seconds", 0)) for event in completed_checker_events)
        peak = max((int(event.get("peak_rss_bytes") or 0) for event in completed_checker_events), default=0)
        if not math.isclose(wall, float(state.get("checker_wall_seconds_charged", 0)), rel_tol=1e-12, abs_tol=1e-9):
            raise GateError("checker wall charges disagree with event history")
        if not math.isclose(cpu, float(state.get("checker_cpu_seconds_charged", 0)), rel_tol=1e-12, abs_tol=1e-9):
            raise GateError("checker CPU charges disagree with event history")
        if peak != state.get("checker_peak_rss_bytes"):
            raise GateError("checker RSS maximum disagrees with event history")
        checker_limits = group_config.get("checker_limits")
        if checker_limits and (
            state["checker_attempts"] > checker_limits["max_attempts"]
            or ((wall > checker_limits["max_total_wall_seconds"] or cpu > checker_limits["max_total_cpu_seconds"])
                and state.get("terminal_status") != "attestation_failed")
            or (peak > checker_limits["max_process_group_rss_bytes"] and state.get("terminal_status") != "attestation_failed")
        ):
            raise GateError("checker resource accounting exceeded its frozen aggregate limits")
        if (pending_reservation.get("run_id") if pending_reservation else None) != state.get("pending"):
            raise GateError("execution-group staged pending state disagrees with event history")


def _verify_program_ledger(directory: Path, state: dict) -> None:
    previous = "0" * 64
    events = []
    for line in (directory / "program-events.jsonl").read_bytes().splitlines():
        event = json.loads(line)
        claimed = event.pop("event_sha256", None)
        if event.get("previous_hash") != previous or claimed != sha256_bytes(compact_json(event)):
            raise GateError("program-wide event ledger hash-chain mismatch")
        previous = claimed
        event["event_sha256"] = claimed
        events.append(event)
    if len(events) != state.get("event_count") or previous != state.get("ledger_head"):
        raise GateError("program-wide ledger state and events disagree")
    imports = [event for event in events if event.get("event") == "historical_import"]
    identity_fields = (
        "schema", "program_id", "historical_invocations", "historical_limit", "historical_overrun", "max_lifetime_invocations",
        "historical_partial_wall_seconds",
        "max_new_cpu_seconds", "max_new_wall_seconds", "max_query_cpu_seconds",
        "max_query_wall_seconds", "max_address_space_bytes", "max_query_bytes",
        "max_output_bytes", "historical_evidence",
    )
    if len(imports) != 1 or any(state.get(key) != imports[0].get(key) for key in identity_fields):
        raise GateError("program-wide historical import is missing or inconsistent")
    reservations = sum(event.get("event") == "reserved" for event in events)
    completed_ids = {event.get("run_id") for event in events if event.get("event") == "completed"}
    reserved_ids = {event.get("run_id") for event in events if event.get("event") == "reserved"}
    if len(reserved_ids) != reservations or len(completed_ids) != sum(event.get("event") == "completed" for event in events) or not completed_ids.issubset(reserved_ids):
        raise GateError("program-wide invocation identity ledger is inconsistent")
    if state.get("invocations_reserved") != state.get("historical_invocations") + reservations:
        raise GateError("program-wide invocation count does not match history and reservations")
    unresolved = reserved_ids - completed_ids
    expected_pending = next(iter(unresolved)) if len(unresolved) == 1 else None
    if len(unresolved) > 1 or state.get("pending") != expected_pending:
        raise GateError("program-wide invocation is unresolved; fail closed")
    if state["invocations_reserved"] > state["max_lifetime_invocations"]:
        raise GateError("program-wide lifetime query ceiling was exceeded")
    cpu_charged = sum(float(event["charged_cpu_seconds"]) for event in events if event.get("event") == "completed")
    wall_charged = sum(float(event["charged_wall_seconds"]) for event in events if event.get("event") == "completed")
    if not math.isclose(cpu_charged, state["new_cpu_seconds_charged"], rel_tol=1e-12, abs_tol=1e-9):
        raise GateError("program-wide CPU charges do not match completion ledger")
    if not math.isclose(wall_charged, state["new_wall_seconds_charged"], rel_tol=1e-12, abs_tol=1e-9):
        raise GateError("program-wide wall charges do not match completion ledger")
    if cpu_charged > state["max_new_cpu_seconds"] or wall_charged > state["max_new_wall_seconds"]:
        raise GateError("program-wide prospective time budget was exceeded")


def read_frozen_protocol(directory: Path) -> tuple[dict, str]:
    data = (directory / "protocol.json").read_bytes()
    digest = sha256_bytes(data)
    marker = (directory / "PROTOCOL.sha256").read_text(encoding="ascii").split()
    if len(marker) != 2 or marker[0] != digest or marker[1] != "protocol.json":
        raise GateError("frozen protocol checksum mismatch")
    protocol = json.loads(data)
    validate_protocol(protocol)
    return protocol, digest


def _append_event(path: Path, event: dict, previous_hash: str) -> str:
    event = {**event, "previous_hash": previous_hash}
    event_hash = sha256_bytes(compact_json(event))
    event["event_sha256"] = event_hash
    fd = os.open(path, os.O_WRONLY | os.O_APPEND)
    with os.fdopen(fd, "ab") as stream:
        stream.write(compact_json(event) + b"\n")
        stream.flush()
        os.fsync(stream.fileno())
    return event_hash


def _read_events(directory: Path) -> list[dict]:
    return [json.loads(line) for line in (directory / "events.jsonl").read_bytes().splitlines()]


def _verify_ledger(directory: Path, budget: dict) -> None:
    previous = "0" * 64
    events = []
    for line in (directory / "events.jsonl").read_bytes().splitlines():
        event = json.loads(line)
        claimed = event.pop("event_sha256", None)
        if event.get("previous_hash") != previous or claimed != sha256_bytes(compact_json(event)):
            raise GateError("event ledger hash-chain mismatch")
        previous = claimed
        event["event_sha256"] = claimed
        events.append(event)
    if len(events) != budget.get("event_count") or previous != budget.get("ledger_head"):
        raise GateError("budget state and append-only event ledger disagree")
    reservations = sum(event.get("event") == "reserved" for event in events)
    if reservations != budget.get("invocations_reserved"):
        raise GateError("solver invocation count does not match event ledger")
    completed = {event.get("run_id") for event in events if event.get("event") == "completed"}
    reserved = {event.get("run_id") for event in events if event.get("event") == "reserved"}
    if len(reserved) != reservations or len(completed) != sum(event.get("event") == "completed" for event in events):
        raise GateError("duplicate invocation identity in event ledger")
    if not completed.issubset(reserved):
        raise GateError("completion event has no matching reservation")
    charged_cpu = sum(float(event["charged_cpu_seconds"]) for event in events if event.get("event") == "completed")
    charged_wall = sum(float(event["charged_wall_seconds"]) for event in events if event.get("event") == "completed")
    if not math.isclose(charged_cpu, float(budget["cpu_seconds_charged"]), rel_tol=1e-12, abs_tol=1e-9):
        raise GateError("cumulative CPU charge disagrees with completed-call ledger")
    if not math.isclose(charged_wall, float(budget["wall_seconds_charged"]), rel_tol=1e-12, abs_tol=1e-9):
        raise GateError("cumulative wall charge disagrees with completed-call ledger")
    terminal = next((event.get("terminal_status") for event in reversed(events) if event.get("event") == "completed" and event.get("terminal_status")), None)
    if terminal != budget.get("terminal_status"):
        raise GateError("terminal stop state disagrees with completed-call ledger")
    unresolved = reserved - completed
    expected_pending = next(iter(unresolved)) if len(unresolved) == 1 else None
    if len(unresolved) > 1 or expected_pending != budget.get("pending"):
        raise GateError("interrupted or inconsistent invocation state; fail closed")


def _replace_budget(directory: Path, budget: dict) -> None:
    path = directory / "budget.json"
    temporary = directory / f"budget.{uuid.uuid4().hex}.tmp"
    write_exclusive(temporary, canonical_json(budget), 0o600)
    os.replace(temporary, path)
    _fsync_directory(directory)


def _set_limits(limits: dict) -> None:
    memory = int(limits["max_address_space_bytes"])
    resource.setrlimit(resource.RLIMIT_AS, (memory, memory))
    cpu = max(1, int(limits["max_query_cpu_seconds"] + 0.999999))
    resource.setrlimit(resource.RLIMIT_CPU, (cpu, cpu + 1))
    file_size = int(limits["max_output_bytes"])
    resource.setrlimit(resource.RLIMIT_FSIZE, (file_size, file_size))


def _read_process_group_sample(proc_root: Path, process_group: int, ticks_per_second: int) -> tuple[int, float] | None:
    """Return summed RSS and CPU for visible group members, or None if none are readable."""
    total_rss = 0
    total_cpu_ticks = 0
    readable = 0
    for stat_path in proc_root.glob("[0-9]*/stat"):
        try:
            stat_text = stat_path.read_text()
            tail = stat_text[stat_text.rfind(")") + 2 :].split()
            if int(tail[2]) != process_group:
                continue
            status = stat_path.parent.joinpath("status").read_text()
            rss = next((int(line.split()[1]) * 1024 for line in status.splitlines() if line.startswith("VmRSS:")), None)
            if rss is None:
                continue
            # A live parent's cutime/cstime includes descendants it has already
            # reaped. Such descendants are no longer visible group members, so
            # these cumulative ticks do not double-count visible children.
            total_cpu_ticks += sum(int(tail[index]) for index in (11, 12, 13, 14))
            total_rss += rss
            readable += 1
        except (FileNotFoundError, ProcessLookupError, PermissionError, ValueError, IndexError, StopIteration):
            continue
    if readable == 0:
        return None
    return total_rss, total_cpu_ticks / ticks_per_second


def _wait4_until(pid: int, timeout: float | None):
    """Reap one child with wait4 so its waited-child CPU/RSS are retained."""
    end = None if timeout is None else time.monotonic() + timeout
    while True:
        try:
            waited, status, usage = os.wait4(pid, os.WNOHANG)
        except InterruptedError:
            continue
        if waited:
            return os.waitstatus_to_exitcode(status), usage
        if end is not None and time.monotonic() >= end:
            return None
        time.sleep(0.005)


def _sample_resources(pid: int, stop: threading.Event, result: dict, cpu_limit: float) -> None:
    peak = int(result.get("peak_process_group_rss_bytes", 0))
    peak_cpu = float(result.get("peak_process_group_cpu_seconds", 0.0))
    samples = int(bool(result.get("process_group_rss_available")))
    ticks_per_second = os.sysconf("SC_CLK_TCK")
    while not stop.is_set():
        sample = _read_process_group_sample(Path("/proc"), pid, ticks_per_second)
        if sample is not None:
            rss, cpu = sample
            samples += 1
            peak = max(peak, rss)
            peak_cpu = max(peak_cpu, cpu)
            if cpu >= cpu_limit:
                result["cpu_limit_reached"] = True
                try:
                    os.killpg(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                break
        else:
            # The leader may exit between periodic samples. wait4 supplies its
            # own child accounting, but cannot establish a process-group RSS peak.
            pass
        stop.wait(0.01)
    result["peak_process_group_rss_bytes"] = peak if samples else None
    result["peak_process_group_cpu_seconds"] = peak_cpu if samples else None
    result["process_group_rss_available"] = samples > 0


def has_solver_timeout_diagnostic(stdout: bytes, stderr: bytes) -> bool:
    """Recognize a solver's own timeout report, independent of its exit code."""
    diagnostic = (stdout + b"\n" + stderr).decode("utf-8", "replace").lower()
    return any(
        re.search(r"(?m)^\s*cvc5 interrupted by timeout\.?\s*$", line) is not None
        for line in diagnostic.splitlines()
    )


def classify(returncode: int, stdout: bytes, stderr: bytes, timed_out: bool) -> str:
    """Classify process and solver outcomes from explicit evidence, not exit code alone.

    ``mathematical_invalid`` is emitted only when the solver output explicitly
    contains Genetik's invalid-result constructor. A generic solver error does
    not prove that the represented mathematical expression is invalid.
    """
    if timed_out:
        return "timed_out"
    if returncode in (-signal.SIGXCPU, -signal.SIGXFSZ):
        return "resource_exhausted"
    combined = (stdout + b"\n" + stderr).decode("utf-8", "replace")
    lines = [line.strip().lower() for line in combined.splitlines() if line.strip() and not line.lstrip().startswith(";")]
    diagnostic = combined.lower()
    if any(token in diagnostic for token in ("out of memory", "bad_alloc", "cannot allocate memory", "memory exhausted")):
        return "resource_exhausted"
    if has_parser_diagnostic(stdout, stderr) or any(
        line.startswith(("error", "(error", "fatal", "exception")) for line in lines
    ):
        return "malformed_input"
    if has_solver_timeout_diagnostic(stdout, stderr):
        return "timed_out"
    if any(re.search(r"(?<![a-z0-9_])mathematical_invalid(?![a-z0-9_])", line) for line in lines):
        return "mathematical_invalid"
    statuses = {line for line in lines if line in {"sat", "unsat", "unknown"}}
    if any(line.startswith("unknown ") or line == "fail" or line.startswith("fail ") for line in lines):
        statuses.add("unknown")
    if len(statuses) > 1:
        return "invalid_output"
    if statuses == {"unsat"}:
        return "unsatisfiable"
    if statuses == {"unknown"}:
        return "unknown"
    if statuses == {"sat"}:
        return "successful"
    if returncode < 0:
        return "externally_terminated"
    if returncode != 0 or not stdout.strip():
        return "invalid_output"
    return "successful"


def classify_execution(
    returncode: int,
    stdout: bytes,
    stderr: bytes,
    process_timed_out: bool,
    *,
    output_exceeded: bool = False,
    cpu_limit_reached: bool = False,
    orphaned_process_group: bool = False,
    process_group_rss_available: bool = True,
) -> dict:
    """Preserve solver/process evidence before applying harness limit overrides."""
    raw = classify(returncode, stdout, stderr, process_timed_out)
    solver_timeout = has_solver_timeout_diagnostic(stdout, stderr)
    overrides = []
    if output_exceeded:
        overrides.append("output_limit")
    if cpu_limit_reached:
        overrides.append("cpu_limit")
    if not process_group_rss_available:
        overrides.append("process_group_rss_unavailable")
    if orphaned_process_group:
        overrides.append("orphaned_process_group")
    status = raw
    if output_exceeded or cpu_limit_reached:
        status = "resource_exhausted"
    elif orphaned_process_group:
        status = "invalid_output"
    elif not process_group_rss_available and raw in {"successful", "unsatisfiable"}:
        status = "resource_accounting_unavailable"
    if process_timed_out and solver_timeout:
        timeout_source = "harness_and_solver_reported"
    elif process_timed_out:
        timeout_source = "harness"
    elif solver_timeout:
        timeout_source = "solver_reported"
    else:
        timeout_source = None
    return {
        "status": status,
        "raw_classification": raw,
        "classification_overrides": overrides,
        "process_timed_out": process_timed_out,
        "solver_reported_timeout": solver_timeout,
        "timed_out": process_timed_out or solver_timeout,
        "timeout_source": timeout_source,
    }


def charged_resource_time(status: str, cpu: float, wall: float,
                          cpu_limit: float, wall_limit: float,
                          accounting_unavailable: bool = False) -> tuple[float, float]:
    """Charge full declared allowances for timeout/resource terminal outcomes."""
    if accounting_unavailable or status in {"timed_out", "resource_exhausted", "resource_accounting_unavailable"}:
        return max(cpu, cpu_limit), max(wall, wall_limit)
    return cpu, wall


def terminal_status_for(status: str, accounting_unavailable: bool) -> str | None:
    """Keep primary failures while ensuring unavailable accounting blocks reuse."""
    if status in {"timed_out", "resource_exhausted"}:
        return status
    if accounting_unavailable:
        return "resource_accounting_unavailable"
    return None


def has_parser_diagnostic(stdout: bytes, stderr: bytes) -> bool:
    diagnostic = (stdout + b"\n" + stderr).decode("utf-8", "replace").lower()
    markers = (
        "parse error",
        "parser error",
        "syntax error",
        "expected smt-lib",
        "unexpected token",
        "unexpected character",
    )
    return any(marker in diagnostic for marker in markers)


def run_solver(directory: Path, query_id: str, query_path: Path) -> dict:
    protocol, digest = read_frozen_protocol(directory)
    limits = protocol["limits"]
    if limits["max_solver_invocations"] == 0:
        raise GateError("protocol solver invocation budget exhausted")
    query_specs = {entry["id"]: entry for entry in protocol["queries"]}
    if query_id not in query_specs:
        raise GateError("query ID is not present in the frozen protocol")
    query_spec = query_specs[query_id]
    if query_path.resolve() != Path(query_spec["path"]).resolve():
        raise GateError("query path does not match the frozen protocol entry")
    with query_path.open("rb") as query_stream:
        query = query_stream.read(int(limits["max_query_bytes"]) + 1)
    if len(query) > int(limits["max_query_bytes"]):
        raise GateError("query exceeds frozen byte limit; no invocation consumed")
    if sha256_bytes(query) != query_spec["sha256"]:
        raise GateError("query bytes changed after protocol freeze")
    solver = protocol["solver"]
    executable = str(Path(solver["executable"]).resolve())
    if sha256_file(Path(executable)) != solver["sha256"]:
        raise GateError("solver binary changed after protocol freeze")
    for runtime_file in solver.get("runtime_files", []):
        if sha256_file(Path(runtime_file["path"]).resolve()) != runtime_file["sha256"]:
            raise GateError("solver runtime file changed after protocol freeze")
    for artifact in protocol["frozen_artifacts"]:
        if sha256_file(Path(artifact["path"]).resolve()) != artifact["sha256"]:
            raise GateError("frozen protocol or dataset artifact changed after freeze")

    program_config = protocol["program_budget"]
    program_dir = Path(program_config["ledger_dir"])
    program_lock_path = program_dir / "program-budget.lock"
    local_lock_path = directory / "budget.lock"
    group_config = protocol.get("execution_group")
    if group_config is not None:
        group_config = _validate_execution_group(group_config, program_config)
    group_dir = Path(group_config["ledger_dir"]) if group_config else None
    group_lock_path = group_dir / "group-budget.lock" if group_dir else None
    lock_paths = [program_lock_path, local_lock_path]
    if group_lock_path:
        lock_paths.append(group_lock_path)
    with ExitStack() as locks:
        for lock_path in sorted(lock_paths, key=lambda item: str(item.resolve())):
            lock_fd = os.open(lock_path, os.O_CREAT | os.O_RDWR, 0o600)
            lock = locks.enter_context(os.fdopen(lock_fd, "r+b"))
            fcntl.flock(lock, fcntl.LOCK_EX)
        protocol_again, digest_again = read_frozen_protocol(directory)
        if digest_again != digest or protocol_again != protocol:
            raise GateError("protocol changed during admission")
        budget_path = directory / "budget.json"
        budget = json.loads(budget_path.read_text())
        _verify_ledger(directory, budget)
        program_state_path = program_dir / "program-budget.json"
        program_state = json.loads(program_state_path.read_text())
        _verify_program_ledger(program_dir, program_state)
        expected_identity = {
            "program_id": program_config["program_id"],
            "historical_invocations": program_config["historical_invocations"],
            "max_lifetime_invocations": program_config["max_lifetime_invocations"],
            "historical_evidence": program_config["historical_evidence"],
        }
        if any(program_state.get(key) != value for key, value in expected_identity.items()):
            raise GateError("program ledger identity differs from frozen protocol")
        group_state = None
        if group_config:
            assert group_dir is not None
            group_state_path = group_dir / "group-budget.json"
            group_state = json.loads(group_state_path.read_text())
            _verify_execution_group(group_dir, group_state)
            expected_group = {
                "program_id": program_config["program_id"],
                "group_id": group_config["group_id"],
                "ledger_dir": str(group_dir.resolve()),
                "max_invocations": group_config["max_invocations"],
            }
            if "stages" in group_config:
                expected_group["stages"] = group_config["stages"]
                expected_group["max_syntax_retries"] = group_config["max_syntax_retries"]
                expected_group["attestation_required"] = group_config["attestation_required"]
                if group_config["attestation_required"]:
                    expected_group["checker_limits"] = group_config["checker_limits"]
            if any(group_state.get(key) != value for key, value in expected_group.items()):
                raise GateError("execution-group identity differs from frozen protocol")
            if group_state.get("pending") is not None:
                raise GateError("execution-group prior invocation is unresolved; fail closed")
            if group_state.get("terminal_status") is not None:
                raise GateError(f"execution group halted after {group_state['terminal_status']}")
            if group_state["invocations_reserved"] >= group_state["max_invocations"]:
                raise GateError("execution-group invocation budget exhausted")
        if program_state.get("pending") is not None:
            raise GateError("program-wide prior invocation is unresolved; fail closed")
        if budget.get("pending") is not None:
            raise GateError("previous invocation is unresolved; fail closed")
        if budget.get("terminal_status") is not None:
            raise GateError(f"protocol halted after {budget['terminal_status']}")
        if budget["protocol_sha256"] != digest:
            raise GateError("budget ledger belongs to another protocol")
        if budget["invocations_reserved"] >= int(limits["max_solver_invocations"]):
            raise GateError("cumulative solver invocation budget exhausted")
        query_uses = sum(event.get("event") == "reserved" and event.get("query_id") == query_id for event in _read_events(directory))
        if query_uses >= query_spec["max_uses"]:
            raise GateError("frozen solver query use allowance exhausted")
        if program_state["invocations_reserved"] >= program_state["max_lifetime_invocations"]:
            raise GateError("program-wide cumulative solver invocation budget exhausted")
        per_wall = float(limits["max_query_wall_seconds"])
        per_cpu = float(limits["max_query_cpu_seconds"])
        if program_state["new_wall_seconds_charged"] + per_wall > float(program_state["max_new_wall_seconds"]):
            raise GateError("program-wide prospective solver wall-time budget exhausted")
        if program_state["new_cpu_seconds_charged"] + per_cpu > float(program_state["max_new_cpu_seconds"]):
            raise GateError("program-wide prospective solver CPU budget exhausted")
        if budget["wall_seconds_charged"] + per_wall > float(limits["max_total_wall_seconds"]):
            raise GateError("aggregate solver wall-time budget exhausted")
        if budget["cpu_seconds_charged"] + per_cpu > float(limits["max_total_cpu_seconds"]):
            raise GateError("aggregate solver CPU budget exhausted")
        stage = None
        retry_attempt = None
        retry_of_run_id = None
        if group_config and "stages" in group_config:
            retry_attempt = query_spec["retry_attempt"]
            retry_of_run_id = query_spec.get("retry_of_run_id")
            stage = _admit_group_stage(
                group_state,
                group_config,
                query_spec["stage_id"],
                retry_attempt,
                retry_of_run_id,
                sha256_bytes(query),
            )
        number = int(budget["invocations_reserved"]) + 1
        run_id = f"call-{number:04d}-{uuid.uuid4().hex}"
        run_dir = directory / "calls" / run_id
        (directory / "calls").mkdir(exist_ok=True)
        run_dir.mkdir(exist_ok=False)
        _fsync_directory(directory / "calls")
        command = [executable, *solver.get("fixed_args", []), *query_spec.get("args", [])]
        reservation = {
            "run_id": run_id,
            "protocol_sha256": digest,
            "query_id": query_id,
            "solver": {
                "executable": executable,
                "version": solver["version"],
                "sha256": solver["sha256"],
                "runtime_files": solver.get("runtime_files", []),
                "environment": solver["environment"],
            },
            "command": command,
            "query_sha256": sha256_bytes(query),
            "query_bytes": len(query),
            "reserved_at_unix_ns": time.time_ns(),
            "limits": {key: limits[key] for key in sorted(REQUIRED_LIMITS)},
        }
        if stage is not None:
            reservation["stage_id"] = stage["stage_id"]
            reservation["retry_attempt"] = retry_attempt
            if retry_of_run_id is not None:
                reservation["retry_of_run_id"] = retry_of_run_id
        write_exclusive(run_dir / "query.bin", query)
        write_exclusive(run_dir / "reservation.json", canonical_json(reservation))
        program_state["ledger_head"] = _append_event(
            program_dir / "program-events.jsonl",
            {"event": "reserved", "run_id": run_id, "protocol_sha256": digest, "protocol_dir": str(directory.resolve()), "call_dir": str(run_dir.resolve()), "query_id": query_id, "query_sha256": reservation["query_sha256"], "reserved_at_unix_ns": reservation["reserved_at_unix_ns"]},
            program_state["ledger_head"],
        )
        program_state["event_count"] += 1
        program_state["invocations_reserved"] += 1
        program_state["pending"] = run_id
        _replace_program_budget(program_dir, program_state)
        if group_state is not None:
            assert group_dir is not None
            group_state["ledger_head"] = _append_event(
                group_dir / "group-events.jsonl",
                {
                    "event": "reserved",
                    "run_id": run_id,
                    "protocol_sha256": digest,
                    "protocol_dir": str(directory.resolve()),
                    "call_dir": str(run_dir.resolve()),
                    "query_id": query_id,
                    "query_sha256": reservation["query_sha256"],
                    "reserved_at_unix_ns": reservation["reserved_at_unix_ns"],
                    **({"stage_id": stage["stage_id"], "retry_attempt": retry_attempt} if stage else {}),
                    **({"retry_of_run_id": retry_of_run_id} if retry_of_run_id is not None else {}),
                },
                group_state["ledger_head"],
            )
            group_state["event_count"] += 1
            group_state["invocations_reserved"] += 1
            group_state["pending"] = run_id
            _replace_execution_group(group_dir, group_state)
        # The lifetime reservation is first. If local persistence fails, the
        # program ledger still consumes the slot and leaves a fail-closed
        # pending record instead of letting another protocol omit the call.
        budget["ledger_head"] = _append_event(directory / "events.jsonl", {"event": "reserved", **reservation}, budget["ledger_head"])
        budget["event_count"] += 1
        budget["invocations_reserved"] = number
        budget["pending"] = run_id
        _replace_budget(directory, budget)
        started = time.monotonic()
        timed_out = False
        orphaned_process_group = False
        sampled: dict = {}
        process_launched = False
        child_usage = None
        output_exceeded = threading.Event()
        captured: dict[str, bytes] = {}
        try:
            proc = subprocess.Popen(
                command,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=solver["environment"],
                preexec_fn=lambda: _set_limits(limits),
                start_new_session=True,
            )
            process_launched = True
            initial_sample = _read_process_group_sample(Path("/proc"), proc.pid, os.sysconf("SC_CLK_TCK"))
            if initial_sample is not None:
                sampled["peak_process_group_rss_bytes"], sampled["peak_process_group_cpu_seconds"] = initial_sample
                sampled["process_group_rss_available"] = True
            stop_sampling = threading.Event()
            sampler = threading.Thread(target=_sample_resources, args=(proc.pid, stop_sampling, sampled, per_cpu), daemon=True)
            sampler.start()
            output_lock = threading.Lock()
            output_total = [0]

            def drain(name: str, pipe) -> None:
                data = bytearray()
                cap = int(limits["max_output_bytes"])
                while True:
                    chunk = pipe.read(65536)
                    if not chunk:
                        break
                    with output_lock:
                        remaining = max(0, cap - output_total[0])
                        accepted = min(remaining, len(chunk))
                        data.extend(chunk[:accepted])
                        output_total[0] += accepted
                        exceeded = accepted < len(chunk)
                    if exceeded:
                        output_exceeded.set()
                        try:
                            os.killpg(proc.pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                        break
                captured[name] = bytes(data)

            stdout_reader = threading.Thread(target=drain, args=("stdout", proc.stdout), daemon=True)
            stderr_reader = threading.Thread(target=drain, args=("stderr", proc.stderr), daemon=True)
            stdout_reader.start()
            stderr_reader.start()

            def send_query() -> None:
                try:
                    proc.stdin.write(query)
                    proc.stdin.close()
                except (BrokenPipeError, OSError):
                    pass

            input_writer = threading.Thread(target=send_query, daemon=True)
            input_writer.start()
            deadline = started + per_wall
            waited = _wait4_until(proc.pid, max(0.0, deadline - time.monotonic()))
            if waited is None:
                timed_out = True
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                waited = _wait4_until(proc.pid, None)
            if waited is not None:
                proc.returncode, child_usage = waited
            for thread in (stdout_reader, stderr_reader, input_writer):
                thread.join(timeout=max(0.0, deadline - time.monotonic()))
            if any(thread.is_alive() for thread in (stdout_reader, stderr_reader, input_writer)):
                timed_out = True
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                for thread in (stdout_reader, stderr_reader, input_writer):
                    thread.join()
            if not timed_out:
                try:
                    os.killpg(proc.pid, 0)
                except ProcessLookupError:
                    pass
                else:
                    orphaned_process_group = True
                    try:
                        os.killpg(proc.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
            stop_sampling.set()
            sampler.join()
            returncode = proc.returncode
            stdout, stderr = captured.get("stdout", b""), captured.get("stderr", b"")
        except OSError as error:
            stdout, stderr = b"", str(error).encode()
            returncode = 127
        wall = time.monotonic() - started
        cpu = float(child_usage.ru_utime + child_usage.ru_stime) if child_usage is not None else 0.0
        classification = classify_execution(
            returncode, stdout, stderr, timed_out,
            output_exceeded=output_exceeded.is_set(),
            cpu_limit_reached=(
                bool(sampled.get("cpu_limit_reached"))
                or (child_usage is not None and cpu >= per_cpu)
                or returncode == -signal.SIGXCPU
            ),
            orphaned_process_group=orphaned_process_group,
            process_group_rss_available=(
                bool(sampled.get("process_group_rss_available")) if process_launched else True
            ),
        )
        status = classification["status"]
        accounting_unavailable = process_launched and not bool(sampled.get("process_group_rss_available"))
        parser_diagnostic = has_parser_diagnostic(stdout, stderr)
        write_exclusive(run_dir / "stdout.bin", stdout)
        write_exclusive(run_dir / "stderr.bin", stderr)
        result = {
            **reservation,
            "status": status,
            "raw_classification": classification["raw_classification"],
            "classification_overrides": classification["classification_overrides"],
            "returncode": returncode,
            "wall_seconds": wall,
            "cpu_seconds": cpu,
            "waited_child_cpu_seconds": cpu,
            "sampled_resident_memory_high_water_bytes": sampled.get("peak_process_group_rss_bytes"),
            "process_group_cpu_seconds": sampled.get("peak_process_group_cpu_seconds"),
            "process_group_rss_bytes": sampled.get("peak_process_group_rss_bytes"),
            "process_group_rss_is_lower_bound": True,
            "process_group_cpu_is_lower_bound": True,
            "process_group_rss_available": (
                bool(sampled.get("process_group_rss_available")) if process_launched else None
            ),
            "resource_accounting_terminal_status": (
                "resource_accounting_unavailable" if accounting_unavailable else None
            ),
            "waited_child_peak_rss_bytes": (
                int(child_usage.ru_maxrss * 1024)
                if sys.platform.startswith("linux") and child_usage is not None else None
            ),
            "completed_at_unix_ns": time.time_ns(),
            "timed_out": classification["timed_out"],
            "process_timed_out": classification["process_timed_out"],
            "solver_reported_timeout": classification["solver_reported_timeout"],
            "timeout_source": classification["timeout_source"],
            "cpu_limit_reached": bool(sampled.get("cpu_limit_reached")),
            "output_limit_reached": output_exceeded.is_set(),
            "orphaned_process_group": orphaned_process_group,
            "parser_diagnostic": parser_diagnostic,
            "stdout_sha256": sha256_bytes(stdout),
            "stderr_sha256": sha256_bytes(stderr),
            "stdout_bytes": len(stdout),
            "stderr_bytes": len(stderr),
        }
        # A query that reached any per-process limit consumes that entire
        # declared allowance. This keeps aggregate limits conservative.
        charged_cpu, charged_wall = charged_resource_time(
            status, cpu, wall, per_cpu, per_wall,
            accounting_unavailable=accounting_unavailable,
        )
        result["charged_wall_seconds"] = charged_wall
        result["charged_cpu_seconds"] = charged_cpu
        projected_wall = budget["wall_seconds_charged"] + charged_wall
        projected_cpu = budget["cpu_seconds_charged"] + charged_cpu
        terminal_status = terminal_status_for(status, accounting_unavailable)
        if projected_wall >= float(limits["max_total_wall_seconds"]):
            terminal_status = terminal_status or "aggregate_wall_budget_exhausted"
        if projected_cpu >= float(limits["max_total_cpu_seconds"]):
            terminal_status = terminal_status or "aggregate_cpu_budget_exhausted"
        result["terminal_status"] = terminal_status
        write_exclusive(run_dir / "result.json", canonical_json(result))
        budget["ledger_head"] = _append_event(directory / "events.jsonl", {"event": "completed", **result}, budget["ledger_head"])
        budget["event_count"] += 1
        budget["wall_seconds_charged"] += charged_wall
        budget["cpu_seconds_charged"] += charged_cpu
        budget["pending"] = None
        budget["terminal_status"] = terminal_status
        _replace_budget(directory, budget)
        program_state["ledger_head"] = _append_event(
            program_dir / "program-events.jsonl",
            {"event": "completed", "run_id": run_id, "protocol_sha256": digest, "status": status, "wall_seconds": wall, "cpu_seconds": cpu, "charged_wall_seconds": charged_wall, "charged_cpu_seconds": charged_cpu},
            program_state["ledger_head"],
        )
        program_state["event_count"] += 1
        program_state["new_wall_seconds_charged"] += charged_wall
        program_state["new_cpu_seconds_charged"] += charged_cpu
        program_state["pending"] = None
        _replace_program_budget(program_dir, program_state)
        if group_state is not None:
            assert group_dir is not None
            if stage is not None:
                _complete_group_stage(
                    group_state,
                    group_config,
                    stage,
                    retry_attempt,
                    run_id,
                    digest,
                    reservation["query_sha256"],
                    result["stdout_sha256"],
                    result["stderr_sha256"],
                    status,
                    parser_diagnostic,
                    str(directory.resolve()),
                    str(run_dir.resolve()),
                )
            group_state["ledger_head"] = _append_event(
                group_dir / "group-events.jsonl",
                {
                    "event": "completed",
                    "run_id": run_id,
                    "protocol_sha256": digest,
                    "status": status,
                    "parser_diagnostic": parser_diagnostic,
                    "stdout_sha256": result["stdout_sha256"],
                    "stderr_sha256": result["stderr_sha256"],
                    "protocol_dir": str(directory.resolve()),
                    "call_dir": str(run_dir.resolve()),
                    "wall_seconds": wall,
                    "cpu_seconds": cpu,
                    **({"stage_id": stage["stage_id"], "retry_attempt": retry_attempt} if stage else {}),
                    **({"retry_of_run_id": retry_of_run_id} if retry_of_run_id is not None else {}),
                    **({"query_sha256": reservation["query_sha256"]} if stage else {}),
                    **({"checker_sha256": stage["checker_sha256"]} if stage and stage.get("checker_sha256") else {}),
                },
                group_state["ledger_head"],
            )
            group_state["event_count"] += 1
            group_state["pending"] = None
            _replace_execution_group(group_dir, group_state)
        return result


def _replace_program_budget(directory: Path, budget: dict) -> None:
    path = directory / "program-budget.json"
    temporary = directory / f"program-budget.{uuid.uuid4().hex}.tmp"
    write_exclusive(temporary, canonical_json(budget), 0o600)
    os.replace(temporary, path)
    _fsync_directory(directory)


def _replace_execution_group(directory: Path, budget: dict) -> None:
    path = directory / "group-budget.json"
    temporary = directory / f"group-budget.{uuid.uuid4().hex}.tmp"
    write_exclusive(temporary, canonical_json(budget), 0o600)
    os.replace(temporary, path)
    _fsync_directory(directory)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    freeze = commands.add_parser("freeze", help="write-once freeze a solver protocol")
    freeze.add_argument("--protocol", type=Path, required=True)
    freeze.add_argument("--destination", type=Path, required=True)
    run = commands.add_parser("run", help="run exactly one budgeted solver query")
    run.add_argument("--protocol-dir", type=Path, required=True)
    run.add_argument("--query", type=Path, required=True)
    run.add_argument("--query-id", required=True)
    attest = commands.add_parser("attest", help="execute a pinned checker for a pending execution-group stage")
    attest.add_argument("--protocol-dir", type=Path, required=True)
    freeze_program = commands.add_parser("freeze-program", help="freeze a pinned bounded C++/Python process protocol")
    freeze_program.add_argument("--protocol", type=Path, required=True)
    freeze_program.add_argument("--destination", type=Path, required=True)
    successor_program = commands.add_parser(
        "successor-program", help="append a software-only successor to an unused bounded-program ledger")
    successor_program.add_argument("--predecessor-dir", type=Path, required=True)
    successor_program.add_argument("--protocol", type=Path, required=True)
    successor_program.add_argument("--destination", type=Path, required=True)
    run_program_parser = commands.add_parser("run-program", help="execute one allowlisted bounded-program invocation")
    run_program_parser.add_argument("--protocol-dir", type=Path, required=True)
    run_program_parser.add_argument("--run-id", required=True)
    verify_program_parser = commands.add_parser("verify-program", help="verify bounded-program receipts and ledger")
    verify_program_parser.add_argument("--protocol-dir", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "freeze":
            digest = freeze_protocol(args.protocol, args.destination)
            print(digest)
        elif args.command == "run":
            result = run_solver(args.protocol_dir, args.query_id, args.query)
            print(json.dumps(result, sort_keys=True))
            return 0 if result["status"] == "successful" else 2
        elif args.command == "freeze-program":
            digest = freeze_program_protocol(args.protocol, args.destination)
            print(digest)
        elif args.command == "successor-program":
            digest = successor_program_protocol(args.predecessor_dir, args.protocol, args.destination)
            print(digest)
        elif args.command == "run-program":
            result = run_program(args.protocol_dir, args.run_id)
            print(json.dumps(result, sort_keys=True))
            return 0 if result["status"] == "completed" else 2
        elif args.command == "verify-program":
            result = verify_program(args.protocol_dir)
            print(json.dumps(result, sort_keys=True))
            return 0 if result["status"] == "valid" else 2
        else:
            result = attest_group(args.protocol_dir)
            print(json.dumps(result, sort_keys=True))
            return 0 if result["passed"] else 2
    except (GateError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"research-execution: refused: {error}", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
