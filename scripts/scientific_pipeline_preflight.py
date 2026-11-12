#!/usr/bin/env python3
"""Run a bounded, synthetic end-to-end preflight for a research pipeline.

The preflight uses a deliberately small adapter contract: runner, verifier,
scorer, and aggregator commands receive a JSON manifest and an isolated
workspace. No campaign data is read; all datasets are embedded in the
preflight specification and materialized as synthetic files.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import resource
import selectors
import signal
import subprocess
import sys
import time
from typing import Any

from bounded_process import run_bounded_process
from research_pipeline import (
    PipelineDescriptorError, expand_argv, pipeline_identity as build_pipeline_identity,
    validate_descriptor as validate_pipeline_descriptor,
)


class PreflightError(RuntimeError):
    pass


SAFE_IDENTIFIER = re.compile(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}\Z")


def canonical(value: Any) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_sha(path: Path) -> str:
    return digest(path.read_bytes())


def protocol_binding(protocol: dict[str, Any]) -> str:
    """Hash the protocol excluding only the attestation that would be circular."""
    value = dict(protocol)
    value.pop("admission_preflight", None)
    value["frozen_artifacts"] = [
        item for item in value.get("frozen_artifacts", [])
        if item.get("role") not in {"admission_preflight_spec", "admission_preflight_receipt"}
    ]
    return digest(canonical(value))


def _require_sha(value: Any, field: str) -> str:
    if not isinstance(value, str) or len(value) != 64 or any(c not in "0123456789abcdef" for c in value):
        raise PreflightError(f"{field} must be a lowercase SHA256")
    return value


def _require_identifier(value: Any, field: str) -> str:
    if not isinstance(value, str) or SAFE_IDENTIFIER.fullmatch(value) is None:
        raise PreflightError(f"{field} must be a path-safe identifier")
    return value


def _index_run_records(records: Any, runs: list[dict[str, Any]], stage: str) -> dict[str, dict[str, Any]]:
    """Require exactly one well-formed record for each frozen run identity."""
    if not isinstance(records, list) or len(records) != len(runs):
        raise PreflightError(f"{stage} output count/identity differs from frozen run matrix")
    expected_ids = {run["run_id"] for run in runs}
    indexed: dict[str, dict[str, Any]] = {}
    for row in records:
        if not isinstance(row, dict) or not isinstance(row.get("run_id"), str):
            raise PreflightError(f"{stage} output contains a malformed run identity")
        run_id = row["run_id"]
        if run_id in indexed:
            raise PreflightError(f"{stage} output repeats run identity {run_id}")
        indexed[run_id] = row
    if set(indexed) != expected_ids:
        raise PreflightError(f"{stage} output identity set differs from frozen run matrix")
    return indexed


def validate_spec(spec: dict[str, Any], base: Path, protocol: dict[str, Any]) -> tuple[dict[str, Path], list[dict[str, Any]], dict | None]:
    if spec.get("schema") != 1:
        raise PreflightError("unsupported or incomplete preflight specification")
    _require_identifier(spec.get("preflight_id"), "preflight_id")
    if spec.get("protocol_binding_sha256") != protocol_binding(protocol):
        raise PreflightError("preflight spec is not bound to this exact campaign protocol")
    protocol_controls = {
        item["id"]: {key: item.get(key) for key in (
            "training_artifact", "training_sha256", "assessment_artifact", "assessment_sha256")}
        for item in protocol.get("controls", [])
    }
    spec_controls = {
        item.get("id"): {key: item.get(key) for key in (
            "protocol_training_artifact", "protocol_training_sha256",
            "protocol_assessment_artifact", "protocol_assessment_sha256")}
        for item in spec.get("controls", [])
    }
    spec_controls = {
        key: {
            "training_artifact": value["protocol_training_artifact"],
            "training_sha256": value["protocol_training_sha256"],
            "assessment_artifact": value["protocol_assessment_artifact"],
            "assessment_sha256": value["protocol_assessment_sha256"],
        }
        for key, value in spec_controls.items()
    }
    if spec_controls != protocol_controls:
        raise PreflightError("synthetic controls do not bind one-to-one to protocol control artifact identities")
    if not isinstance(protocol.get("run_matrix"), list) or spec.get("runs") != protocol["run_matrix"]:
        raise PreflightError("preflight does not exercise the exact frozen run/control/method/seed matrix")
    pipeline_identity = protocol.get("pipeline_identity")
    if not isinstance(pipeline_identity, dict):
        raise PreflightError("campaign protocol must pin runner, verifier, scorer and aggregator identities")
    controls = spec.get("controls")
    runs = spec.get("runs")
    if not isinstance(controls, list) or len(controls) < 2 or not isinstance(runs, list) or not runs:
        raise PreflightError("at least two controls and a nonempty run matrix are required")
    artifact_paths: dict[str, Path] = {}
    control_ids: set[str] = set()
    for control in controls:
        if not isinstance(control, dict):
            raise PreflightError("each control must be an object")
        _require_identifier(control.get("id"), "control id")
        control_id = control["id"]
        if control_id in control_ids:
            raise PreflightError(f"duplicate control identity: {control_id}")
        control_ids.add(control_id)
        for role in ("training", "assessment"):
            artifact = control.get(role)
            if not isinstance(artifact, dict):
                raise PreflightError(f"control {control_id} needs a {role} artifact identity")
            _require_identifier(artifact.get("id"), f"control {control_id} {role} artifact id")
            artifact_id = artifact["id"]
            if artifact_id in artifact_paths:
                raise PreflightError(f"duplicate artifact identity: {artifact_id}")
            rows = artifact.get("synthetic_rows")
            if not isinstance(rows, list) or not rows:
                raise PreflightError(f"synthetic {role} rows required for {artifact_id}")
            artifact_bytes = canonical({"control_id": control_id, "role": role, "rows": rows}) + b"\n"
            expected = artifact.get("sha256")
            if expected is not None and expected != digest(artifact_bytes):
                raise PreflightError(f"synthetic artifact checksum mismatch: {artifact_id}")
            artifact["sha256"] = digest(artifact_bytes)
            path = base / f"{artifact_id}.json"
            if path.resolve().parent != base.resolve():
                raise PreflightError(f"synthetic artifact path escapes its evidence directory: {artifact_id}")
            if path.exists():
                raise PreflightError(f"synthetic artifact path collision: {artifact_id}")
            path.write_bytes(artifact_bytes)
            artifact_paths[artifact_id] = path
        if control["training"]["id"] == control["assessment"]["id"]:
            raise PreflightError(f"control {control_id} reuses training as assessment")
        if control["training"].get("synthetic_rows") == control["assessment"].get("synthetic_rows"):
            raise PreflightError(f"control {control_id} synthetic splits must be distinct")

    run_ids: set[str] = set()
    tuples: set[tuple[str, str, int]] = set()
    for run in runs:
        if not isinstance(run, dict):
            raise PreflightError("each run must be an object")
        _require_identifier(run.get("run_id"), "run_id")
        run_id = run["run_id"]
        if run_id in run_ids:
            raise PreflightError(f"duplicate run identity: {run_id}")
        run_ids.add(run_id)
        control_id, method, seed = run.get("control_id"), run.get("method"), run.get("seed")
        if control_id not in control_ids or not isinstance(method, str) or SAFE_IDENTIFIER.fullmatch(method) is None or type(seed) is not int:
            raise PreflightError(f"run {run_id} has an invalid control/method/seed association")
        key = (control_id, method, seed)
        if key in tuples:
            raise PreflightError(f"duplicate control/method/seed association: {key}")
        tuples.add(key)
        control = next(c for c in controls if c["id"] == control_id)
        if (
            run.get("training_artifact") != control.get("protocol_training_artifact")
            or run.get("assessment_artifact") != control.get("protocol_assessment_artifact")
        ):
            raise PreflightError(f"run {run_id} protocol artifact mapping does not match control {control_id}")
    if {run["control_id"] for run in runs} != control_ids:
        raise PreflightError("run matrix must exercise every declared control")
    expected_count = spec.get("expected_run_count")
    if type(expected_count) is not int or expected_count != len(runs):
        raise PreflightError("expected_run_count does not match complete run matrix")
    descriptor = protocol.get("pipeline_descriptor")
    if descriptor is not None:
        try:
            normalized_descriptor = validate_pipeline_descriptor(descriptor, protocol.get("frozen_artifacts", []))
        except PipelineDescriptorError as error:
            raise PreflightError(str(error)) from error
        if spec.get("pipeline_descriptor_sha256") != descriptor.get("sha256"):
            raise PreflightError("preflight spec does not bind the frozen pipeline descriptor")
        if "commands" in spec:
            raise PreflightError("preflight spec cannot override descriptor stage commands")
        command_set = None
        if protocol.get("pipeline_identity") != build_pipeline_identity(descriptor):
            raise PreflightError("pipeline_identity differs from authoritative descriptor")
    else:
        normalized_descriptor = None
        command_set = spec.get("commands")
        if not isinstance(command_set, dict) or set(command_set) != {"runner", "verifier", "scorer", "aggregator"}:
            raise PreflightError("runner, verifier, scorer and aggregator commands are required")
        if not isinstance(protocol.get("pipeline_identity"), dict):
            raise PreflightError("campaign protocol must pin runner, verifier, scorer and aggregator identities")
        if spec.get("pipeline_descriptor_sha256") is not None:
            raise PreflightError("legacy preflight spec cannot declare a descriptor digest")
    pipeline_identity = protocol.get("pipeline_identity")
    for stage, command in (command_set or {}).items():
        if not isinstance(command, dict) or not isinstance(command.get("argv"), list) or not command["argv"]:
            raise PreflightError(f"{stage} command argv is required")
        executable = Path(command["argv"][0]).resolve()
        if command.get("executable") != str(executable) or not executable.is_file() or not os.access(executable, os.X_OK):
            raise PreflightError(f"{stage} executable missing or not executable")
        if command.get("sha256") != file_sha(executable):
            raise PreflightError(f"{stage} executable hash mismatch")
        if not all(isinstance(arg, str) for arg in command["argv"]):
            raise PreflightError(f"{stage} argv must contain strings")
        identity_files = command.get("identity_files")
        if not isinstance(identity_files, list) or not identity_files:
            raise PreflightError(f"{stage} must pin its adapter and runtime dependency files")
        seen_files: set[Path] = set()
        for identity in identity_files:
            if not isinstance(identity, dict) or not isinstance(identity.get("path"), str):
                raise PreflightError(f"{stage} dependency identity needs path and SHA256")
            identity_path = Path(identity["path"]).resolve()
            if identity_path in seen_files or not identity_path.is_file() or identity.get("sha256") != file_sha(identity_path):
                raise PreflightError(f"{stage} dependency identity is duplicate, missing, or has a hash mismatch")
            seen_files.add(identity_path)
        argv_files = {
            Path(arg).resolve()
            for arg in command["argv"][1:]
            if "{" not in arg and Path(arg).is_file()
        }
        if not argv_files or not argv_files.issubset(seen_files):
            raise PreflightError(f"{stage} command entrypoint files must be among its pinned identity_files")
        if not isinstance(command.get("runtime_identity"), str) or not command["runtime_identity"].strip():
            raise PreflightError(f"{stage} runtime_identity must be explicitly pinned")
        pinned_identity = pipeline_identity.get(stage)
        if not isinstance(pinned_identity, dict) or pinned_identity.get("executable") != str(executable) or any(
            pinned_identity.get(field) != command.get(field)
            for field in ("version", "sha256", "identity_files", "runtime_identity")
        ):
            raise PreflightError(f"{stage} command identity differs from campaign protocol")
        if not isinstance(command.get("version"), str) or not command["version"].strip():
            raise PreflightError(f"{stage} version must be explicitly recorded")
    limits = spec.get("limits")
    if not isinstance(limits, dict):
        raise PreflightError("preflight resource limits are required")
    for name in ("wall_seconds",):
        value = limits.get(name)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0:
            raise PreflightError(f"positive finite {name} limit is required")
    cpu_seconds = limits.get("cpu_seconds")
    if type(cpu_seconds) is not int or cpu_seconds < 1:
        raise PreflightError("cpu_seconds must be an integer of at least one second (RLIMIT_CPU granularity)")
    for name in ("address_space_bytes", "output_bytes"):
        value = limits.get(name)
        if type(value) is not int or value < 1024 * 1024:
            raise PreflightError(f"{name} must be an integer of at least one MiB")
    return artifact_paths, runs, normalized_descriptor


def _invoke(stage: str, command_spec: dict[str, Any], manifest: Path, workdir: Path,
            timeout: float, cpu_seconds: int, address_space_bytes: int,
            output_cap: int) -> dict[str, Any]:
    argv = [arg.replace("{manifest}", str(manifest)).replace("{workdir}", str(workdir)).replace("{stage}", stage) for arg in command_spec["argv"]]
    started = time.monotonic()
    def set_child_limits() -> None:
        resource.setrlimit(resource.RLIMIT_CPU, (cpu_seconds, cpu_seconds))
        resource.setrlimit(resource.RLIMIT_AS, (address_space_bytes, address_space_bytes))

    process = subprocess.Popen(
        argv, cwd=workdir, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        start_new_session=True, preexec_fn=set_child_limits,
        env={
            "PATH": os.defpath,
            "LANG": "C",
            "LC_ALL": "C",
            "TZ": "UTC",
            "PYTHONHASHSEED": "0",
            "PYTHONDONTWRITEBYTECODE": "1",
            "PYTHONPYCACHEPREFIX": str(workdir / ".python-cache"),
        },
    )
    assert process.stdout is not None and process.stderr is not None
    captured = {process.stdout: bytearray(), process.stderr: bytearray()}
    selector = selectors.DefaultSelector()
    for stream in captured:
        os.set_blocking(stream.fileno(), False)
        selector.register(stream, selectors.EVENT_READ)
    stop_reason = None
    deadline = started + timeout
    try:
        while selector.get_map():
            if time.monotonic() >= deadline:
                stop_reason = "wall-time limit exceeded"
                break
            for key, _ in selector.select(timeout=0.01):
                stream = key.fileobj
                used = sum(len(value) for value in captured.values())
                chunk = os.read(stream.fileno(), min(65536, max(1, output_cap + 1 - used)))
                if not chunk:
                    selector.unregister(stream)
                    stream.close()
                    continue
                captured[stream].extend(chunk)
                if sum(len(value) for value in captured.values()) > output_cap:
                    stop_reason = "output limit exceeded"
                    break
            if stop_reason is not None:
                break
            if process.poll() is not None and not selector.get_map():
                break
    finally:
        if stop_reason is not None:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        # End the entire stage process group, including a helper that kept a
        # pipe open after the adapter leader exited.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        returncode = process.wait()
        selector.close()
        for stream in (process.stdout, process.stderr):
            if not stream.closed:
                stream.close()
    if stop_reason is not None:
        raise PreflightError(f"{stage} {stop_reason}")
    stdout = bytes(captured[process.stdout])
    stderr = bytes(captured[process.stderr])
    wall = time.monotonic() - started
    if returncode != 0:
        raise PreflightError(f"{stage} failed with exit status {returncode}: {stderr.decode(errors='replace')[:500]}")
    return {"stage": stage, "argv": argv, "executable_sha256": command_spec["sha256"], "returncode": returncode, "wall_seconds": wall, "stdout_sha256": digest(stdout), "stderr_sha256": digest(stderr)}


def _invoke_descriptor_stage(stage: str, execution_program_dir: Path, run_id: str,
                             workdir: Path) -> dict[str, Any]:
    # Lazy import avoids the research_execution <-> preflight module cycle.
    from research_execution import run_program
    receipt = run_program(execution_program_dir, run_id)
    if receipt.get("status") != "completed" or receipt.get("stage_id") != stage:
        raise PreflightError(f"{stage} bounded-program invocation failed: {receipt.get('status')}")
    names = {"runner": ("candidate_outputs", "candidate-outputs.json"),
             "verifier": ("verified_receipts", "verified-receipts.json"),
             "scorer": ("scores_output", "independent-scores.json"),
             "aggregator": ("report_output", "scientific-report.json")}
    slot, filename = names[stage]
    output = receipt.get("output_slots", {}).get(slot)
    if not isinstance(output, dict) or not Path(output["path"]).is_file():
        raise PreflightError(f"{stage} bounded-program receipt omitted its declared output")
    destination = workdir / filename
    destination.write_bytes(Path(output["path"]).read_bytes())
    return {"stage": stage, "run_id": run_id, "argv": receipt["command"],
            "executable_sha256": receipt["executable_sha256"],
            "returncode": receipt["returncode"], "status": receipt["status"],
            "wall_seconds": receipt["wall_seconds"], "cpu_seconds": receipt["cpu_seconds"],
            "peak_process_group_rss_bytes": receipt.get("peak_process_group_rss_bytes"),
            "process_group_rss_available": receipt.get("process_group_rss_available"),
            "stdout_sha256": receipt["stdout_sha256"], "stderr_sha256": receipt["stderr_sha256"],
            "bounded_program_receipt_sha256": digest(canonical(receipt))}


def run_preflight(protocol_path: Path, spec_path: Path, receipt_path: Path) -> dict[str, Any]:
    protocol_bytes = protocol_path.read_bytes()
    protocol = json.loads(protocol_bytes)
    spec_bytes = spec_path.read_bytes()
    spec = json.loads(spec_bytes)
    execution_program_dir = None
    execution_config = None
    if protocol.get("pipeline_descriptor") is not None:
        configured_dir = protocol.get("execution_program_dir")
        if not isinstance(configured_dir, str):
            raise PreflightError("descriptor pipeline requires its frozen bounded-program directory")
        execution_program_dir = Path(configured_dir).resolve()
        from research_execution import read_frozen_program
        try:
            _, execution_config, _ = read_frozen_program(execution_program_dir)
        except Exception as error:
            raise PreflightError(f"frozen pipeline execution protocol is invalid: {error}") from error
        if (execution_config.get("pipeline_descriptor") is None
                or execution_config["protocol"]["pipeline_descriptor"].get("sha256") != protocol["pipeline_descriptor"].get("sha256")):
            raise PreflightError("bounded-program descriptor differs from scientific pipeline descriptor")
        spec_artifact_id = execution_config["preflight_qualification"]["spec_artifact_id"]
        pinned_spec = execution_config["artifacts"][spec_artifact_id]
        if (Path(pinned_spec["path"]).resolve() != spec_path.resolve()
                or file_sha(spec_path) != pinned_spec["sha256"]):
            raise PreflightError("synthetic preflight spec differs from the pinned bounded-program artifact")
    preflight_id = _require_identifier(spec.get("preflight_id"), "preflight_id")
    if receipt_path.exists():
        raise PreflightError("receipt destination already exists; evidence is write-once")
    if not protocol_binding(protocol):
        raise PreflightError("protocol binding could not be computed")
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    evidence_root = receipt_path.parent / f"{preflight_id}-{os.urandom(8).hex()}"
    evidence_root.mkdir(mode=0o700, exist_ok=False)
    try:
        synthetic = evidence_root / "synthetic"
        synthetic.mkdir()
        workdir = evidence_root / "pipeline"
        workdir.mkdir()
        paths, runs, normalized_descriptor = validate_spec(spec, synthetic, protocol)
        controls = []
        for control in spec["controls"]:
            controls.append({
                "id": control["id"],
                "training": {"id": control["training"]["id"], "path": str(paths[control["training"]["id"]]), "sha256": file_sha(paths[control["training"]["id"]])},
                "assessment": {"id": control["assessment"]["id"], "path": str(paths[control["assessment"]["id"]]), "sha256": file_sha(paths[control["assessment"]["id"]])},
            })
        run_bindings = [
            {"run_id": r["run_id"], "control_id": r["control_id"], "method": r["method"], "seed": r["seed"]}
            for r in runs
        ]
        runner_runs = [
            {key: run[key] for key in ("run_id", "control_id", "method", "seed", "training_artifact")}
            for run in runs
        ]
        runner_manifest_obj = {
            "schema": 1, "preflight_id": spec["preflight_id"],
            "controls": [{"id": c["id"], "training": c["training"]} for c in controls], "runs": runner_runs,
            "expected_run_count": spec["expected_run_count"],
            "outputs_dir": str(workdir / "outputs"),
        }
        def contains_assessment_field(value: Any) -> bool:
            if isinstance(value, dict):
                return any("assessment" in str(key).lower() or contains_assessment_field(item)
                           for key, item in value.items())
            if isinstance(value, list):
                return any(contains_assessment_field(item) for item in value)
            return False
        if contains_assessment_field(runner_manifest_obj):
            raise PreflightError("runner-visible manifest contains an assessment field")
        runner_manifest_bytes = canonical(runner_manifest_obj) + b"\n"
        for control in controls:
            if control["assessment"]["path"].encode() in runner_manifest_bytes or control["assessment"]["id"].encode() in runner_manifest_bytes:
                raise PreflightError("runner-visible manifest contains an assessment artifact binding")
        scorer_manifest_obj = {
            **runner_manifest_obj, "runs": runs,
            "controls": controls,
            "candidate_outputs": str(workdir / "candidate-outputs.json"),
            "verified_receipts": str(workdir / "verified-receipts.json"),
        }
        (workdir / "outputs").mkdir()
        runner_manifest = workdir / "runner-manifest.json"
        runner_manifest.write_bytes(runner_manifest_bytes)
        scorer_manifest = workdir / "scorer-manifest.json"
        scorer_manifest.write_bytes(canonical(scorer_manifest_obj) + b"\n")
        aggregate_manifest = workdir / "aggregate-manifest.json"
        aggregate_manifest.write_bytes(canonical({
            "schema": 1, "preflight_id": spec["preflight_id"],
            "runs": runs, "scores": str(workdir / "independent-scores.json"),
            "expected_run_count": len(runs), "report": str(workdir / "scientific-report.json"),
        }) + b"\n")
        stage_manifests = {"runner_manifest": runner_manifest, "scorer_manifest": scorer_manifest}
        descriptor = normalized_descriptor
        if descriptor is not None:
            written_preflight_inputs = {}
            for stage_name, run_id in execution_config["preflight_qualification"]["stage_run_ids"].items():
                stage_run = execution_config["run_by_id"][run_id]
                for slot, binding in stage_run["slot_bindings"].items():
                    if binding["kind"] != "controller_file":
                        continue
                    target = (execution_program_dir / binding["path"]).resolve()
                    if execution_program_dir not in target.parents:
                        raise PreflightError("frozen preflight controller input path escapes program directory")
                    source = (runner_manifest if slot == "run_manifest" and stage_name in {"runner", "verifier"}
                              else scorer_manifest if stage_name == "scorer"
                              else aggregate_manifest)
                    source_bytes = source.read_bytes()
                    if target.exists():
                        if target not in written_preflight_inputs or target.read_bytes() != written_preflight_inputs[target]:
                            raise PreflightError("frozen preflight controller input is already present")
                        continue
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(source_bytes)
                    written_preflight_inputs[target] = source_bytes
        stages = []
        for stage in ("runner", "verifier", "scorer", "aggregator"):
            manifest = scorer_manifest if stage == "scorer" else aggregate_manifest if stage == "aggregator" else runner_manifest
            if descriptor is not None:
                run_id = execution_config["preflight_qualification"]["stage_run_ids"][stage]
                record = _invoke_descriptor_stage(stage, execution_program_dir, run_id, workdir)
            else:
                record = _invoke(
                    stage, spec["commands"][stage], manifest, workdir,
                    float(spec["limits"]["wall_seconds"]),
                    spec["limits"]["cpu_seconds"],
                    spec["limits"]["address_space_bytes"],
                    spec["limits"]["output_bytes"],
                )
            stages.append(record)
            status_path = workdir / f"{stage}.status.json"
            if descriptor is None:
                if not status_path.is_file():
                    raise PreflightError(f"{stage} did not produce its required status artifact")
                status = json.loads(status_path.read_text())
                if status.get("status") != "passed" or status.get("preflight_id") != spec["preflight_id"] or status.get("run_bindings") != run_bindings:
                    raise PreflightError(f"{stage} status artifact is invalid")
                record["status_artifact_sha256"] = file_sha(status_path)
        outputs_path = workdir / "candidate-outputs.json"
        receipts_path = workdir / "verified-receipts.json"
        if not outputs_path.is_file() or not receipts_path.is_file():
            raise PreflightError("runner or verifier omitted candidate outputs or execution receipts")
        output_records = json.loads(outputs_path.read_text())
        receipt_records = json.loads(receipts_path.read_text())
        if not isinstance(output_records, list) or len(output_records) != len(runs) or not isinstance(receipt_records, list) or len(receipt_records) != len(runs):
            raise PreflightError("candidate/receipt count differs from frozen run matrix")
        controls_by_id = {control["id"]: control for control in spec["controls"]}
        output_by_id = {row.get("run_id"): row for row in output_records if isinstance(row, dict)}
        receipt_by_id = {row.get("run_id"): row for row in receipt_records if isinstance(row, dict)}
        if len(output_by_id) != len(runs) or len(receipt_by_id) != len(runs):
            raise PreflightError("candidate or receipt output has duplicate/malformed run identity")
        for run in runs:
            control = controls_by_id[run["control_id"]]
            train_path = paths[control["training"]["id"]]
            output = output_by_id.get(run["run_id"])
            if not isinstance(output, dict) or any(output.get(key) != run[key] for key in ("run_id", "control_id", "method", "seed")):
                raise PreflightError(f"candidate output is misassociated for {run['run_id']}")
            if output.get("training_artifact") != control["training"]["id"] or output.get("training_sha256") != file_sha(train_path):
                raise PreflightError(f"candidate output has wrong control-specific training identity for {run['run_id']}")
            if not isinstance(output.get("predictions"), list) or not output["predictions"]:
                raise PreflightError(f"candidate output predictions are missing for {run['run_id']}")
            receipt = receipt_by_id.get(run["run_id"])
            if not isinstance(receipt, dict) or any(receipt.get(key) != run[key] for key in ("run_id", "control_id", "method", "seed")):
                raise PreflightError(f"execution receipt is misassociated for {run['run_id']}")
            if receipt.get("candidate_sha256") != digest(canonical(output)) or receipt.get("status") != "verified":
                raise PreflightError(f"execution receipt does not verify candidate output for {run['run_id']}")
        scores_path = workdir / "independent-scores.json"
        if not scores_path.is_file():
            raise PreflightError("scorer did not produce independent-scores.json")
        score_records = json.loads(scores_path.read_text())
        scores_by_id = _index_run_records(score_records, runs, "scorer")
        for run in runs:
            control = controls_by_id[run["control_id"]]
            assessment_path = paths[control["assessment"]["id"]]
            expected_values = [row["expected"] for row in json.loads(assessment_path.read_text())["rows"]]
            score = scores_by_id.get(run["run_id"])
            output = output_by_id[run["run_id"]]
            if not isinstance(score, dict) or any(score.get(key) != run[key] for key in ("run_id", "control_id", "method", "seed")):
                raise PreflightError(f"score is misassociated for {run['run_id']}")
            if score.get("assessment_artifact") != control["assessment"]["id"] or score.get("assessment_sha256") != file_sha(assessment_path):
                raise PreflightError(f"score used wrong control-specific assessment artifact for {run['run_id']}")
            if output.get("predictions") != expected_values or score.get("exact") is not True or score.get("checked_observations") != len(expected_values):
                raise PreflightError(f"independent synthetic assessment mismatch for {run['run_id']}")
        expected_per_control: dict[str, dict[str, int]] = {}
        for run in runs:
            score = scores_by_id[run["run_id"]]
            entry = expected_per_control.setdefault(
                run["control_id"], {"run_count": 0, "exact_run_count": 0}
            )
            entry["run_count"] += 1
            entry["exact_run_count"] += int(score.get("exact") is True)
        expected_exact_count = sum(row["exact_run_count"] for row in expected_per_control.values())
        report = workdir / "scientific-report.json"
        if not report.is_file():
            raise PreflightError("aggregator did not produce scientific-report.json")
        report_obj = json.loads(report.read_text())
        if (
            report_obj.get("preflight_id") != spec["preflight_id"]
            or report_obj.get("assessment_source") != "synthetic"
            or report_obj.get("run_count") != len(runs)
            or report_obj.get("scored_run_count") != len(runs)
            or report_obj.get("exact_run_count") != expected_exact_count
            or report_obj.get("per_control") != expected_per_control
            or report_obj.get("status") != "passed"
            or report_obj.get("run_ids") != [r["run_id"] for r in runs]
            or report_obj.get("run_bindings") != run_bindings
        ):
            raise PreflightError("final scientific report does not qualify the complete synthetic run set")
        evidence_hashes = {
            path.relative_to(evidence_root).as_posix(): file_sha(path)
            for path in sorted(evidence_root.rglob("*")) if path.is_file()
        }
        receipt = {
            "schema": 1, "status": "passed", "preflight_id": spec["preflight_id"],
            "protocol_binding_sha256": protocol_binding(protocol),
            "protocol_input_sha256": digest(protocol_bytes), "spec_sha256": digest(spec_bytes),
            "evidence_directory": str(evidence_root.resolve()),
            "control_artifacts": [
                {
                    "control_id": c["id"],
                    "protocol_training_artifact": c["protocol_training_artifact"],
                    "protocol_training_sha256": c["protocol_training_sha256"],
                    "protocol_assessment_artifact": c["protocol_assessment_artifact"],
                    "protocol_assessment_sha256": c["protocol_assessment_sha256"],
                    "synthetic_training_id": c["training"]["id"],
                    "synthetic_training_sha256": c["training"]["sha256"],
                    "synthetic_assessment_id": c["assessment"]["id"],
                    "synthetic_assessment_sha256": c["assessment"]["sha256"],
                }
                for c in spec["controls"]
            ],
            "run_ids": [run["run_id"] for run in runs],
            "run_bindings": run_bindings,
            "pipeline_identity": (protocol.get("pipeline_identity") if descriptor is not None else spec["commands"]),
            "pipeline_descriptor_sha256": (protocol["pipeline_descriptor"]["sha256"] if descriptor is not None else None),
            "stages": stages,
            "report_sha256": file_sha(report),
            "evidence_hashes": evidence_hashes,
        }
        encoded = json.dumps(receipt, sort_keys=True, indent=2).encode() + b"\n"
        descriptor = os.open(receipt_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o444)
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(encoded)
            stream.flush()
            os.fsync(stream.fileno())
        if execution_program_dir is not None:
            from research_execution import qualify_program
            qualify_program(execution_program_dir)
        for path in sorted(evidence_root.rglob("*"), reverse=True):
            if path.is_file():
                path.chmod(0o444)
            elif path.is_dir():
                path.chmod(0o555)
        evidence_root.chmod(0o555)
        return receipt
    except Exception:
        # Partial evidence remains in its unique directory for diagnosis.
        raise


def verify_admission(protocol: dict[str, Any]) -> dict[str, Any]:
    admission = protocol.get("admission_preflight")
    if not isinstance(admission, dict) or not {"preflight_id", "spec_path", "spec_sha256", "receipt_path", "receipt_sha256", "protocol_binding_sha256"}.issubset(admission):
        raise PreflightError("successful end-to-end admission preflight is mandatory")
    spec_path, receipt_path = Path(admission["spec_path"]).resolve(), Path(admission["receipt_path"]).resolve()
    if not spec_path.is_file() or file_sha(spec_path) != admission["spec_sha256"]:
        raise PreflightError("admission preflight specification missing or hash mismatch")
    if not receipt_path.is_file() or file_sha(receipt_path) != admission["receipt_sha256"]:
        raise PreflightError("admission preflight receipt missing or hash mismatch")
    receipt = json.loads(receipt_path.read_text())
    binding = protocol_binding(protocol)
    if receipt.get("schema") != 1 or receipt.get("status") != "passed" or receipt.get("preflight_id") != admission["preflight_id"]:
        raise PreflightError("admission preflight receipt is not a successful matching receipt")
    if receipt.get("spec_sha256") != admission["spec_sha256"] or receipt.get("protocol_binding_sha256") != binding or admission["protocol_binding_sha256"] != binding:
        raise PreflightError("admission preflight is bound to a different protocol")
    execution_program_dir = None
    execution_config = None
    if protocol.get("pipeline_descriptor") is not None:
        execution_program_dir = Path(protocol.get("execution_program_dir", "")).resolve()
        from research_execution import read_frozen_program, verify_program
        try:
            _, execution_config, _ = read_frozen_program(execution_program_dir)
            verification = verify_program(execution_program_dir)
        except Exception as error:
            raise PreflightError(f"bounded pipeline ledger verification failed: {error}") from error
        if (verification.get("status") != "valid"
                or execution_config["protocol"]["pipeline_descriptor"].get("sha256") != protocol["pipeline_descriptor"].get("sha256")):
            raise PreflightError("bounded pipeline is not qualified by its immutable execution ledger")
    spec = json.loads(spec_path.read_text())
    expected_controls = {entry["id"] for entry in protocol.get("controls", [])}
    spec_controls = spec.get("controls")
    if not isinstance(spec_controls, list) or len(spec_controls) < 2:
        raise PreflightError("admission preflight must include at least two controls")
    spec_control_ids = [entry.get("id") for entry in spec_controls if isinstance(entry, dict)]
    if len(spec_control_ids) != len(spec_controls) or set(spec_control_ids) != expected_controls or len(set(spec_control_ids)) != len(spec_control_ids):
        raise PreflightError("admission preflight control set must match protocol controls exactly")
    receipt_controls = receipt.get("control_artifacts")
    expected_control_artifacts = [
        {
            "control_id": control["id"],
            "protocol_training_artifact": control["training_artifact"],
            "protocol_training_sha256": control["training_sha256"],
            "protocol_assessment_artifact": control["assessment_artifact"],
            "protocol_assessment_sha256": control["assessment_sha256"],
            "synthetic_training_id": spec_control["training"]["id"],
            "synthetic_training_sha256": digest(canonical({"control_id": control["id"], "role": "training", "rows": spec_control["training"]["synthetic_rows"]}) + b"\n"),
            "synthetic_assessment_id": spec_control["assessment"]["id"],
            "synthetic_assessment_sha256": digest(canonical({"control_id": control["id"], "role": "assessment", "rows": spec_control["assessment"]["synthetic_rows"]}) + b"\n"),
        }
        for control in protocol.get("controls", [])
        for spec_control in spec.get("controls", []) if spec_control.get("id") == control["id"]
    ]
    if not isinstance(receipt_controls, list) or receipt_controls != expected_control_artifacts or len(receipt_controls) != len(expected_controls):
        raise PreflightError("preflight control mapping does not cover protocol controls exactly once")
    expected_run_bindings = [
        {"run_id": run["run_id"], "control_id": run["control_id"], "method": run["method"], "seed": run["seed"]}
        for run in protocol["run_matrix"]
    ]
    if spec.get("protocol_binding_sha256") != binding or spec.get("runs") != protocol["run_matrix"] or receipt.get("run_bindings") != expected_run_bindings:
        raise PreflightError("preflight run matrix differs from campaign protocol")
    descriptor = protocol.get("pipeline_descriptor")
    if descriptor is not None:
        try:
            normalized_descriptor = validate_pipeline_descriptor(descriptor, protocol.get("frozen_artifacts", []))
        except PipelineDescriptorError as error:
            raise PreflightError(str(error)) from error
        if (spec.get("pipeline_descriptor_sha256") != descriptor.get("sha256") or "commands" in spec
                or receipt.get("pipeline_descriptor_sha256") != descriptor.get("sha256")
                or receipt.get("pipeline_identity") != protocol.get("pipeline_identity")):
            raise PreflightError("preflight spec attempts to replace or misbind the frozen command descriptor")
        if receipt.get("pipeline_descriptor_sha256") != descriptor.get("sha256") or receipt.get("pipeline_identity") != protocol.get("pipeline_identity"):
            raise PreflightError("preflight receipt differs from the frozen pipeline descriptor")
        artifact_by_id = {artifact["id"]: artifact for artifact in protocol["frozen_artifacts"]}
    else:
        normalized_descriptor = None
        expected_pipeline = protocol.get("pipeline_identity")
        spec_pipeline = {
            stage: {key: command.get(key) for key in ("version", "executable", "sha256", "identity_files", "runtime_identity")}
            for stage, command in spec.get("commands", {}).items()
        }
        if spec_pipeline != expected_pipeline or receipt.get("pipeline_identity") != spec.get("commands"):
            raise PreflightError("preflight stage commands differ from pinned campaign pipeline identities")
    evidence_root = Path(receipt.get("evidence_directory", "")).resolve()
    if not evidence_root.is_dir() or evidence_root.parent != receipt_path.parent.resolve():
        raise PreflightError("preflight evidence directory is missing")
    evidence_hashes = receipt.get("evidence_hashes")
    if not isinstance(evidence_hashes, dict) or not evidence_hashes:
        raise PreflightError("preflight receipt does not pin its complete evidence tree")
    actual_files = {
        path.relative_to(evidence_root).as_posix(): path
        for path in evidence_root.rglob("*") if path.is_file()
    }
    if set(actual_files) != set(evidence_hashes):
        raise PreflightError("preflight evidence tree has missing or unexpected files")
    for relative, path in actual_files.items():
        if Path(relative).is_absolute() or ".." in Path(relative).parts or file_sha(path) != evidence_hashes[relative]:
            raise PreflightError(f"preflight evidence hash mismatch: {relative}")
    expected_runs = protocol.get("run_matrix", [])
    if {run.get("control_id") for run in expected_runs if isinstance(run, dict)} != expected_controls:
        raise PreflightError("admission run matrix must exercise every protocol control")
    score_path = evidence_root / "pipeline" / "independent-scores.json"
    if not score_path.is_file():
        raise PreflightError("preflight scorer output is missing")
    _index_run_records(json.loads(score_path.read_text()), expected_runs, "scorer")
    report_path = evidence_root / "pipeline" / "scientific-report.json"
    if not report_path.is_file() or file_sha(report_path) != receipt.get("report_sha256"):
        raise PreflightError("preflight final report is missing or has a hash mismatch")
    expected_stage_order = ["runner", "verifier", "scorer", "aggregator"]
    if not isinstance(receipt.get("stages"), list) or [
        item.get("stage") for item in receipt["stages"] if isinstance(item, dict)
    ] != expected_stage_order:
        raise PreflightError("preflight did not complete the required end-to-end pipeline")
    for stage_record, stage_name in zip(receipt["stages"], expected_stage_order):
        if stage_record.get("stage") != stage_name:
            raise PreflightError("preflight stage order mismatch")
        if normalized_descriptor is not None:
            run_id = stage_record.get("run_id")
            if execution_config["preflight_qualification"]["stage_run_ids"].get(stage_name) != run_id:
                raise PreflightError(f"preflight {stage_name} run ID differs from frozen phase plan")
            run_receipt = execution_program_dir / "runs" / run_id / "receipt.json"
            if not run_receipt.is_file() or digest(canonical(json.loads(run_receipt.read_text()))) != stage_record.get("bounded_program_receipt_sha256"):
                raise PreflightError(f"preflight {stage_name} bounded-process receipt changed")
            saved = json.loads(run_receipt.read_text())
            if saved.get("status") != "completed" or saved.get("command") != stage_record.get("argv"):
                raise PreflightError(f"preflight {stage_name} command/status does not match ledger receipt")
        else:
            command = spec["commands"].get(stage_name, {})
            executable = Path(command.get("argv", [""])[0]).resolve()
            if not executable.is_file() or file_sha(executable) != stage_record.get("executable_sha256"):
                raise PreflightError(f"preflight {stage_name} executable changed after qualification")
            for identity in command.get("identity_files", []):
                dependency = Path(identity["path"]).resolve()
                if not dependency.is_file() or file_sha(dependency) != identity.get("sha256"):
                    raise PreflightError(f"preflight {stage_name} dependency changed after qualification")
        if normalized_descriptor is None:
            status_path = evidence_root / "pipeline" / f"{stage_name}.status.json"
            if not status_path.is_file() or file_sha(status_path) != stage_record.get("status_artifact_sha256"):
                raise PreflightError(f"preflight {stage_name} status artifact changed after qualification")
            status = json.loads(status_path.read_text())
            if (status.get("status") != "passed" or status.get("preflight_id") != receipt["preflight_id"]
                    or status.get("run_bindings") != expected_run_bindings):
                raise PreflightError(f"preflight {stage_name} status artifact does not match the run matrix")
    if receipt.get("run_ids") != [run["run_id"] for run in protocol["run_matrix"]]:
        raise PreflightError("preflight receipt has missing or duplicate run identities")
    report = json.loads(report_path.read_text())
    score_path = evidence_root / "pipeline" / "independent-scores.json"
    if not score_path.is_file():
        raise PreflightError("preflight independent score artifact is missing")
    score_rows = json.loads(score_path.read_text())
    scores = {row.get("run_id"): row for row in score_rows if isinstance(row, dict)} if isinstance(score_rows, list) else {}
    expected_per_control: dict[str, dict[str, int]] = {}
    for run in protocol["run_matrix"]:
        score = scores.get(run["run_id"])
        if not isinstance(score, dict) or any(score.get(key) != run[key] for key in ("run_id", "control_id", "method", "seed")):
            raise PreflightError("preflight independent score mapping is incomplete or inconsistent")
        entry = expected_per_control.setdefault(run["control_id"], {"run_count": 0, "exact_run_count": 0})
        entry["run_count"] += 1
        entry["exact_run_count"] += int(score.get("exact") is True)
    expected_exact_count = sum(row["exact_run_count"] for row in expected_per_control.values())
    if (
        report.get("status") != "passed"
        or report.get("assessment_source") != "synthetic"
        or report.get("preflight_id") != receipt["preflight_id"]
        or report.get("run_count") != len(expected_run_bindings)
        or report.get("scored_run_count") != len(expected_run_bindings)
        or report.get("exact_run_count") != expected_exact_count
        or report.get("per_control") != expected_per_control
        or report.get("run_ids") != [row["run_id"] for row in expected_run_bindings]
        or report.get("run_bindings") != expected_run_bindings
    ):
        raise PreflightError("preflight final report no longer matches the qualified synthetic run matrix")
    return {
        "spec_path": spec_path,
        "spec_sha256": admission["spec_sha256"],
        "receipt_path": receipt_path,
        "receipt_sha256": admission["receipt_sha256"],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--protocol", type=Path, required=True)
    parser.add_argument("--spec", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        result = run_preflight(args.protocol, args.spec, args.receipt)
        print(json.dumps({"status": result["status"], "preflight_id": result["preflight_id"], "receipt_sha256": file_sha(args.receipt)}, sort_keys=True))
        return 0
    except (PreflightError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"scientific-pipeline-preflight: refused: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
