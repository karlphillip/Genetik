"""Canonical, role-constrained argv descriptors for staged research pipelines."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re


STAGES = ("runner", "verifier", "scorer", "aggregator")
STAGE_INPUT_ROLES = {
    "runner": frozenset({"run_manifest", "training_bundle"}),
    "verifier": frozenset({"run_manifest", "candidate_outputs"}),
    "scorer": frozenset({"run_manifest", "candidate_outputs", "verified_receipts", "assessment_bundle"}),
    "aggregator": frozenset({"run_manifest", "scores_input"}),
}
STAGE_OUTPUT_ROLES = {
    "runner": frozenset({"candidate_outputs"}),
    "verifier": frozenset({"verified_receipts"}),
    "scorer": frozenset({"scores_output"}),
    "aggregator": frozenset({"report_output"}),
}
SLOT = re.compile(r"\{([a-z][a-z0-9_]*)\}\Z")


class PipelineDescriptorError(ValueError):
    pass


def canonical(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def descriptor_digest(descriptor: dict) -> str:
    value = dict(descriptor)
    value.pop("sha256", None)
    return hashlib.sha256(canonical(value)).hexdigest()


def pipeline_identity(descriptor: dict) -> dict:
    return {
        "descriptor_sha256": descriptor.get("sha256"),
        "stage_entrypoints": {name: descriptor["stages"][name]["entrypoint_artifact"] for name in STAGES},
        "stage_argv_sha256": {
            name: hashlib.sha256(canonical(descriptor["stages"][name]["argv"])).hexdigest()
            for name in STAGES
        },
    }


def validate_descriptor(descriptor: object, frozen_artifacts: list[dict]) -> dict:
    if not isinstance(descriptor, dict) or descriptor.get("schema") != 1:
        raise PipelineDescriptorError("pipeline_descriptor schema 1 is required")
    if descriptor.get("sha256") != descriptor_digest(descriptor):
        raise PipelineDescriptorError("pipeline descriptor digest mismatch")
    stages = descriptor.get("stages")
    if not isinstance(stages, dict) or tuple(stages.keys()) != STAGES:
        # JSON key order is canonicalized at freeze; require the exact set but
        # normalize order before returning to callers.
        if not isinstance(stages, dict) or set(stages) != set(STAGES):
            raise PipelineDescriptorError("pipeline descriptor must define all four stages")
    artifacts = {}
    for item in frozen_artifacts:
        if not isinstance(item, dict) or not {"id", "path", "sha256", "role"}.issubset(item):
            raise PipelineDescriptorError("frozen pipeline artifact identity is incomplete")
        artifact_id = item["id"]
        if not isinstance(artifact_id, str) or artifact_id in artifacts:
            raise PipelineDescriptorError("frozen pipeline artifact IDs must be unique")
        if not isinstance(item["path"], str) or not isinstance(item["sha256"], str) or not re.fullmatch(r"[0-9a-f]{64}", item["sha256"]):
            raise PipelineDescriptorError(f"frozen artifact {artifact_id} has an invalid path or SHA256")
        artifacts[artifact_id] = item
        # Only code/runtime identities are opened during command validation.
        # Training and assessment are bound by immutable metadata here and are
        # materialized by their designated stages later.
        if item["role"] in {"pipeline-executable", "pipeline-dependency"}:
            path = Path(item["path"]).resolve()
            try:
                actual = hashlib.sha256(path.read_bytes()).hexdigest()
            except OSError as error:
                raise PipelineDescriptorError(f"pinned pipeline artifact {artifact_id} is unavailable") from error
            if actual != item["sha256"]:
                raise PipelineDescriptorError(f"pinned pipeline artifact {artifact_id} hash mismatch")
    normalized = {"schema": 1, "stages": {}}
    for stage_name in STAGES:
        stage = stages[stage_name]
        if not isinstance(stage, dict) or not {"executable_artifact", "entrypoint_artifact", "argv", "environment", "version", "runtime_identity", "dependencies", "input_roles", "output_roles"}.issubset(stage):
            raise PipelineDescriptorError(f"pipeline stage {stage_name} is incomplete")
        exe_id = stage["executable_artifact"]
        executable = artifacts.get(exe_id)
        if not isinstance(executable, dict) or executable.get("role") != "pipeline-executable":
            raise PipelineDescriptorError(f"{stage_name} executable must reference a pinned pipeline-executable artifact")
        executable_path = str(Path(executable["path"]).resolve())
        argv = stage["argv"]
        if not isinstance(argv, list) or not argv or not all(isinstance(arg, str) for arg in argv) or argv[0] != executable_path:
            raise PipelineDescriptorError(f"{stage_name} argv must be complete and start with its pinned executable path")
        allowed = set(STAGE_INPUT_ROLES[stage_name] | STAGE_OUTPUT_ROLES[stage_name])
        entrypoint = artifacts.get(stage["entrypoint_artifact"])
        if not isinstance(entrypoint, dict) or entrypoint.get("role") not in {"pipeline-executable", "pipeline-dependency"}:
            raise PipelineDescriptorError(f"{stage_name} entrypoint must be a pinned executable/script artifact")
        entrypoint_path = str(Path(entrypoint["path"]).resolve())
        if entrypoint_path not in argv:
            raise PipelineDescriptorError(f"{stage_name} fixed argv does not invoke its pinned entrypoint")
        used_slots = set()
        for token in argv[1:]:
            if "{" in token or "}" in token:
                match = SLOT.fullmatch(token)
                if match is None:
                    raise PipelineDescriptorError(f"{stage_name} argv contains an invalid or embedded path slot")
                slot = match.group(1)
                if slot not in allowed:
                    raise PipelineDescriptorError(f"{stage_name} cannot receive the {slot} slot")
                used_slots.add(slot)
        environment = stage["environment"]
        if not isinstance(environment, dict) or not {"PATH", "LANG", "LC_ALL"}.issubset(environment) or not all(isinstance(k, str) and isinstance(v, str) for k, v in environment.items()):
            raise PipelineDescriptorError(f"{stage_name} environment must be complete and pinned")
        if not isinstance(stage["version"], str) or not stage["version"].strip() or not isinstance(stage["runtime_identity"], str) or not stage["runtime_identity"].strip():
            raise PipelineDescriptorError(f"{stage_name} runtime identity and version must be explicit")
        dependencies = stage["dependencies"]
        if not isinstance(dependencies, list) or not all(isinstance(dep, str) for dep in dependencies):
            raise PipelineDescriptorError(f"{stage_name} dependencies must be artifact IDs")
        for dependency_id in dependencies:
            dependency = artifacts.get(dependency_id)
            if not isinstance(dependency, dict) or dependency.get("role") != "pipeline-dependency":
                raise PipelineDescriptorError(f"{stage_name} dependency {dependency_id} is not pinned")
        pinned_runtime_paths = {executable_path, entrypoint_path}
        pinned_runtime_paths.update(str(Path(artifacts[dep]["path"]).resolve()) for dep in dependencies)
        for token in argv[1:]:
            if SLOT.fullmatch(token):
                continue
            candidate = Path(token)
            if candidate.is_absolute() and candidate.is_file() and str(candidate.resolve()) not in pinned_runtime_paths:
                raise PipelineDescriptorError(f"{stage_name} argv contains an unpinned executable or script path")
        input_roles, output_roles = stage["input_roles"], stage["output_roles"]
        if not isinstance(input_roles, list) or not all(role in STAGE_INPUT_ROLES[stage_name] for role in input_roles):
            raise PipelineDescriptorError(f"{stage_name} declares a forbidden input role")
        if not isinstance(output_roles, list) or not all(role in STAGE_OUTPUT_ROLES[stage_name] for role in output_roles):
            raise PipelineDescriptorError(f"{stage_name} declares a forbidden output role")
        if stage_name == "runner" and "assessment_bundle" in used_slots:
            raise PipelineDescriptorError("runner must not receive assessment data")
        if stage_name == "runner":
            assessment_artifacts = [a for a in artifacts.values() if a.get("role") == "assessment"]
            forbidden = [str(Path(a["path"]).resolve()) for a in assessment_artifacts]
            forbidden += [a.get("id", "") for a in assessment_artifacts]
            forbidden += [a.get("sha256", "") for a in assessment_artifacts]
            if any(value and value in "\0".join(argv + [str(v) for v in environment.values()]) for value in forbidden):
                raise PipelineDescriptorError("runner argv/environment contains a pinned assessment identity")
        if not set(input_roles).issubset(used_slots | {"run_manifest"}):
            raise PipelineDescriptorError(f"{stage_name} input roles are not all bound by argv")
        if not set(output_roles).issubset(used_slots):
            raise PipelineDescriptorError(f"{stage_name} output roles are not all bound by argv")
        normalized["stages"][stage_name] = stage
    return normalized


def expand_argv(descriptor: dict, stage_name: str, slots: dict[str, str]) -> list[str]:
    stage = descriptor["stages"][stage_name]
    allowed = set(STAGE_INPUT_ROLES[stage_name] | STAGE_OUTPUT_ROLES[stage_name])
    result = []
    for token in stage["argv"]:
        match = SLOT.fullmatch(token)
        if match is None:
            result.append(token)
            continue
        slot = match.group(1)
        if slot not in allowed or slot not in slots:
            raise PipelineDescriptorError(f"{stage_name} missing permitted slot {slot}")
        path = Path(slots[slot])
        if not path.is_absolute():
            raise PipelineDescriptorError(f"{stage_name} slot {slot} must be an absolute path")
        result.append(str(path.resolve()))
    return result
