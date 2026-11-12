#!/usr/bin/env python3
"""Fail-closed, persistent execution gate for external research solvers.

This tool is research-only. A protocol directory is immutable after freeze;
each attempted solver process consumes one atomically reserved query slot.
"""

from __future__ import annotations

import argparse
from contextlib import ExitStack
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
PROGRAM_LIMIT_FIELDS = {
    "max_invocations", "max_total_cpu_seconds", "max_total_wall_seconds",
    "max_cpu_seconds", "max_wall_seconds", "max_address_space_bytes",
    "max_stdin_bytes", "max_output_bytes", "max_file_bytes",
}


def _safe_program_id(value: object, field: str) -> str:
    if not isinstance(value, str) or PROGRAM_SAFE_ID.fullmatch(value) is None:
        raise GateError(f"{field} must be a safe identifier")
    return value


def _validate_bounded_program(protocol: object) -> dict:
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
    for key in ("max_lifetime_invocations",):
        if isinstance(budget.get(key), bool) or not isinstance(budget.get(key), int) or budget[key] < 0:
            raise GateError(f"program_budget.{key} must be a nonnegative integer")
    for key in ("max_lifetime_cpu_seconds", "max_lifetime_wall_seconds"):
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
    if limits["max_invocations"] > budget["max_lifetime_invocations"]:
        raise GateError("protocol invocation cap exceeds program lifetime cap")
    if limits["max_total_cpu_seconds"] > budget["max_lifetime_cpu_seconds"] or limits["max_total_wall_seconds"] > budget["max_lifetime_wall_seconds"]:
        raise GateError("protocol time cap exceeds program lifetime cap")
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
                    if item["phase_id"] != "preflight" or rel.is_absolute() or ".." in rel.parts or not rel.parts:
                        raise GateError("controller files are relative preflight-only typed inputs")
                    if slot == "assessment_bundle" and item["phase_id"] != "preflight":
                        raise GateError("synthetic controller assessment input is preflight-only")
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
    required_phases = ["preflight", "campaign", "verification", "scoring", "aggregation", "recovery"]
    if not isinstance(phase_plan, list) or [phase.get("phase_id") for phase in phase_plan if isinstance(phase, dict)] != required_phases:
        raise GateError("phase_plan must name preflight, campaign, verification, scoring, aggregation and recovery in order")
    flattened = []
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
    if flattened != [run["run_id"] for run in runs]:
        raise GateError("phase plan must cover the exact allowlist once in execution order")
    qualification = protocol.get("preflight_qualification") if descriptor_raw is not None else None
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
    if planned_cpu > budget["max_lifetime_cpu_seconds"] or planned_wall > budget["max_lifetime_wall_seconds"]:
        raise GateError("complete phase plan exceeds persistent program-wide CPU/wall ceilings")
    if len(runs) > budget["max_lifetime_invocations"]:
        raise GateError("complete phase plan exceeds persistent program-wide invocation ceiling")
    normalized_command = ({**command, "executable": str(executable)} if command is not None else None)
    return {"protocol": protocol, "program_id": program_id, "ledger_dir": expected_ledger,
            "command": normalized_command, "artifacts": artifacts_by_id,
            "commands_by_stage": command_by_stage, "pipeline_descriptor": pipeline_descriptor if descriptor_raw is not None else None,
            "preflight_qualification": qualification,
            "runs": runs, "run_by_id": run_by_id, "phase_plan": phase_plan,
            "planned_cpu_seconds": planned_cpu, "planned_wall_seconds": planned_wall,
            "limits": limits, "budget": budget}


def _process_ledger_identity(config: dict) -> dict:
    budget = config["budget"]
    protocol_sha256 = config.get("frozen_protocol_sha256")
    if protocol_sha256 is None:
        protocol_sha256 = sha256_bytes(canonical_json(config["protocol"]))
    return {"schema": 1, "kind": "bounded_program_ledger", "program_id": config["program_id"],
            "frozen_protocol_sha256": protocol_sha256,
            "max_lifetime_invocations": budget["max_lifetime_invocations"],
            "max_lifetime_cpu_seconds": budget["max_lifetime_cpu_seconds"],
            "max_lifetime_wall_seconds": budget["max_lifetime_wall_seconds"]}


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


def _verify_process_ledger(directory: Path, state: dict, expected_protocol_sha256: str) -> list[dict]:
    if state.get("frozen_protocol_sha256") != expected_protocol_sha256:
        raise GateError("bounded-process ledger is bound to a different frozen protocol")
    events = []
    previous = "0" * 64
    for line in (directory / "process-events.jsonl").read_bytes().splitlines():
        event = json.loads(line)
        claimed = event.pop("event_sha256", None)
        if event.get("previous_hash") != previous or claimed != sha256_bytes(compact_json(event)):
            raise GateError("bounded-process ledger hash-chain mismatch")
        event["event_sha256"] = claimed
        previous = claimed
        events.append(event)
    if len(events) != state.get("event_count") or previous != state.get("ledger_head"):
        raise GateError("bounded-process ledger state/head mismatch")
    identity = _process_ledger_identity({"budget": {key: state.get(key) for key in (
        "max_lifetime_invocations", "max_lifetime_cpu_seconds", "max_lifetime_wall_seconds")},
        "program_id": state.get("program_id"),
        "frozen_protocol_sha256": state.get("frozen_protocol_sha256")})
    if not events or any(state.get(key) != value for key, value in identity.items()):
        raise GateError("bounded-process ledger identity mismatch")
    if events[0].get("event") != "created" or any(events[0].get(k) != v for k, v in identity.items()):
        raise GateError("bounded-process ledger creation event mismatch")
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
    if state["invocations_reserved"] > state["max_lifetime_invocations"] or cpu > state["max_lifetime_cpu_seconds"] or wall > state["max_lifetime_wall_seconds"]:
        raise GateError("bounded-process lifetime ceiling exceeded")
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
            _verify_process_ledger(directory, state, identity["frozen_protocol_sha256"])
            return
        event = {"event": "created", **identity, "previous_hash": "0" * 64}
        event_hash = sha256_bytes(compact_json(event))
        event["event_sha256"] = event_hash
        write_exclusive(events_path, compact_json(event) + b"\n", 0o600)
        state = {**identity, "invocations_reserved": 0, "cpu_seconds_charged": 0.0,
                 "wall_seconds_charged": 0.0, "pending": None, "qualification_status": "preflight_pending", "event_count": 1,
                 "ledger_head": event_hash}
        write_exclusive(state_path, canonical_json(state), 0o600)


def freeze_program_protocol(source: Path, destination: Path) -> str:
    raw = source.read_bytes()
    protocol = json.loads(raw)
    config = _validate_bounded_program(protocol)
    _initialize_process_ledger(config)
    destination.mkdir(parents=True, exist_ok=False)
    _fsync_directory(destination.parent)
    encoded = canonical_json(protocol)
    digest = sha256_bytes(encoded)
    write_exclusive(destination / "program.json", encoded)
    write_exclusive(destination / "PROGRAM.sha256", f"{digest}  program.json\n".encode())
    _fsync_directory(destination)
    return digest


def read_frozen_program(directory: Path) -> tuple[dict, dict, str]:
    raw = (directory / "program.json").read_bytes()
    digest = sha256_bytes(raw)
    marker = (directory / "PROGRAM.sha256").read_text(encoding="ascii").split()
    if marker != [digest, "program.json"]:
        raise GateError("bounded-program protocol checksum mismatch")
    config = _validate_bounded_program(json.loads(raw))
    return config["protocol"], config, digest


def _authorize_program_run(directory: Path, config: dict, protocol_sha: str, run_id: str) -> None:
    """Authorize a phase before any input artifact is opened or hashed."""
    run = config["run_by_id"][run_id]
    ledger_dir = config["ledger_dir"]
    lock_fd = os.open(ledger_dir / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state = json.loads((ledger_dir / "process-budget.json").read_text())
        events = _verify_process_ledger(ledger_dir, state, protocol_sha)
        protocol_events = [e for e in events if e.get("protocol_sha256") == protocol_sha]
        qualified = any(e.get("event") == "qualified" for e in protocol_events)
        if config.get("pipeline_descriptor") is not None and run["phase_id"] != "preflight" and not qualified:
            raise GateError("campaign and later phases require successful preflight qualification")
        if state.get("pending") is not None:
            raise GateError("prior bounded-program invocation unresolved; fail closed")
        reserved = [e for e in protocol_events if e.get("event") == "reserved"]
        ordered = [rid for phase in config["phase_plan"] for rid in phase["run_ids"]]
        if len(reserved) >= len(ordered) or ordered[len(reserved)] != run_id:
            raise GateError("bounded-program invocation is out of frozen phase-plan order")
        if any(e.get("run_id") == run_id for e in reserved):
            raise GateError("bounded-program run_id was already consumed")
        remaining = [config["run_by_id"][rid] for rid in ordered[len(reserved):]]
        if state["invocations_reserved"] + len(remaining) > state["max_lifetime_invocations"]:
            raise GateError("complete remaining phase plan does not fit the persistent invocation budget")
        completed = [e for e in protocol_events if e.get("event") == "completed"]
        local_cpu = sum(float(e["charged_cpu_seconds"]) for e in completed)
        local_wall = sum(float(e["charged_wall_seconds"]) for e in completed)
        limits = config["limits"]
        remaining_cpu = sum(float(x.get("max_cpu_seconds", limits["max_cpu_seconds"])) + BOUNDED_SUPERVISOR_CPU_ALLOWANCE_SECONDS for x in remaining)
        remaining_wall = sum(float(x.get("max_wall_seconds", limits["max_wall_seconds"])) + BOUNDED_SUPERVISOR_WALL_ALLOWANCE_SECONDS for x in remaining)
        if (local_cpu + remaining_cpu > limits["max_total_cpu_seconds"]
                or state["cpu_seconds_charged"] + remaining_cpu > state["max_lifetime_cpu_seconds"]):
            raise GateError("remaining bounded-program CPU plan does not fit aggregate/lifetime budget")
        if (local_wall + remaining_wall > limits["max_total_wall_seconds"]
                or state["wall_seconds_charged"] + remaining_wall > state["max_lifetime_wall_seconds"]):
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


def _resolve_pipeline_inputs(
    directory: Path,
    config: dict,
    run: dict,
    run_dir: Path,
    protocol_sha: str,
    ledger_events: list[dict],
) -> tuple[list[str], dict[str, str], dict[str, str], dict[str, str]]:
    stage = config["pipeline_descriptor"]["stages"][run["stage_id"]]
    slots: dict[str, str] = {}
    input_hashes: dict[str, str] = {}
    input_paths: dict[str, str] = {}
    for slot, binding in run["slot_bindings"].items():
        kind = binding["kind"]
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
            source_run = binding["run_id"]
            matching_events = [
                event for event in ledger_events
                if event.get("event") == "completed"
                and event.get("protocol_sha256") == protocol_sha
                and event.get("run_id") == source_run
            ]
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
            slot_record = source_receipt.get("output_slots", {}).get(binding["slot"])
            if source_receipt.get("status") != "completed" or not isinstance(slot_record, dict):
                raise GateError("pipeline predecessor did not complete with the requested output")
            path = Path(slot_record["path"])
            expected_path = (directory / "runs" / source_run / "outputs" / f"{binding['slot']}.json").resolve()
            if path.resolve() != expected_path:
                raise GateError("pipeline predecessor output path differs from its frozen run-owned slot")
            actual = sha256_file(path)
            if actual != slot_record.get("sha256"):
                raise GateError("pipeline predecessor output hash mismatch")
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
                   "harness_error", "supervisor_timeout", "supervisor_cpu_limit"}
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
    protocol, config, protocol_sha = read_frozen_program(directory)
    _safe_program_id(run_id, "run_id")
    runs = {item["run_id"]: item for item in config["runs"]}
    if run_id not in runs:
        raise GateError("run_id is not allowlisted by the frozen bounded-program protocol")
    run = runs[run_id]
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
        events = _verify_process_ledger(ledger_dir, state, protocol_sha)
        qualified = any(e.get("event") == "qualified" and e.get("protocol_sha256") == protocol_sha for e in events)
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
        prior_for_protocol = [e for e in events if e.get("protocol_sha256") == protocol_sha and e.get("event") == "reserved"]
        if any(e.get("run_id") == run_id for e in prior_for_protocol):
            raise GateError("bounded-program run_id was already consumed")
        ordered_run_ids = [run_id for phase in config["phase_plan"] for run_id in phase["run_ids"]]
        if len(prior_for_protocol) >= len(ordered_run_ids) or ordered_run_ids[len(prior_for_protocol)] != run_id:
            raise GateError("bounded-program invocation is out of frozen phase-plan order")
        if state["invocations_reserved"] >= state["max_lifetime_invocations"]:
            raise GateError("bounded-program lifetime invocation budget exhausted")
        protocol_completed = [e for e in events if e.get("protocol_sha256") == protocol_sha and e.get("event") == "completed"]
        local_cpu = sum(float(e["charged_cpu_seconds"]) for e in protocol_completed)
        local_wall = sum(float(e["charged_wall_seconds"]) for e in protocol_completed)
        if len(prior_for_protocol) >= limits["max_invocations"]:
            raise GateError("bounded-program protocol invocation budget exhausted")
        remaining_runs = [config["run_by_id"][rid] for rid in ordered_run_ids[len(prior_for_protocol):]]
        if state["invocations_reserved"] + len(remaining_runs) > state["max_lifetime_invocations"]:
            raise GateError("complete remaining phase plan does not fit the persistent invocation budget")
        remaining_cpu = sum(float(item.get("max_cpu_seconds", limits["max_cpu_seconds"])) + BOUNDED_SUPERVISOR_CPU_ALLOWANCE_SECONDS for item in remaining_runs)
        remaining_wall = sum(float(item.get("max_wall_seconds", limits["max_wall_seconds"])) + BOUNDED_SUPERVISOR_WALL_ALLOWANCE_SECONDS for item in remaining_runs)
        if local_cpu + remaining_cpu > limits["max_total_cpu_seconds"] or state["cpu_seconds_charged"] + remaining_cpu > state["max_lifetime_cpu_seconds"]:
            raise GateError("remaining bounded-program phase-plan CPU allowance does not fit aggregate/lifetime budget")
        if local_wall + remaining_wall > limits["max_total_wall_seconds"] or state["wall_seconds_charged"] + remaining_wall > state["max_lifetime_wall_seconds"]:
            raise GateError("remaining bounded-program phase-plan wall allowance does not fit aggregate/lifetime budget")
        max_cpu = float(run.get("max_cpu_seconds", limits["max_cpu_seconds"]))
        max_wall = float(run.get("max_wall_seconds", limits["max_wall_seconds"]))

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
                       "reserved_at_unix_ns": time.time_ns(), "limits": limits}
        write_exclusive(run_dir / "reservation.json", canonical_json(reservation))
        state["ledger_head"] = _process_ledger_event(
            ledger_dir / "process-events.jsonl", {"event": "reserved", **reservation}, state["ledger_head"])
        state["event_count"] += 1
        state["invocations_reserved"] += 1
        state["pending"] = invocation_id
        _replace_process_budget(ledger_dir, state)

        exec_limits = {"max_cpu_seconds": max_cpu,
                       "max_wall_seconds": max_wall,
                       "max_address_space_bytes": limits["max_address_space_bytes"],
                       "max_stdin_bytes": limits["max_stdin_bytes"],
                       "max_output_bytes": limits["max_output_bytes"],
                       "max_file_bytes": limits["max_file_bytes"]}
        try:
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
             "charged_cpu_seconds": charged_cpu, "charged_wall_seconds": charged_wall},
            state["ledger_head"])
        state["event_count"] += 1
        state["cpu_seconds_charged"] += charged_cpu
        state["wall_seconds_charged"] += charged_wall
        state["pending"] = None
        _replace_process_budget(ledger_dir, state)
        return receipt


def verify_program(directory: Path) -> dict:
    protocol, config, protocol_sha = read_frozen_program(directory)
    state = json.loads((config["ledger_dir"] / "process-budget.json").read_text())
    events = _verify_process_ledger(config["ledger_dir"], state, protocol_sha)
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
            input_path = Path(input_path)
            if not input_path.is_file() or sha256_file(input_path) != receipt.get("input_slot_hashes", {}).get(slot):
                raise GateError(f"bounded-program input slot {slot} is missing or hash-mismatched")
        receipt_checks.append({"run_id": receipt["run_id"], "status": receipt["status"],
                               "receipt_sha256": event["receipt_sha256"]})
    protocol_qualified = any(e.get("event") == "qualified" and e.get("protocol_sha256") == protocol_sha
                             for e in events)
    valid = state.get("pending") is None and (
        config.get("pipeline_descriptor") is None or protocol_qualified)
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
    qualification = config["preflight_qualification"]
    stage_run_ids = qualification["stage_run_ids"]
    ledger_dir = config["ledger_dir"]
    lock_fd = os.open(ledger_dir / "process-budget.lock", os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(lock_fd, "r+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state_path = ledger_dir / "process-budget.json"
        state = json.loads(state_path.read_text())
        events = _verify_process_ledger(ledger_dir, state, protocol_sha)
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
        payload = {"event": "qualified", "protocol_sha256": protocol_sha,
                   "preflight_evidence_sha256": evidence_sha256, "qualified_at_unix_ns": time.time_ns()}
        state["ledger_head"] = _process_ledger_event(ledger_dir / "process-events.jsonl", payload, state["ledger_head"])
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
