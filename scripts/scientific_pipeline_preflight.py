#!/usr/bin/env python3
"""Run a bounded, synthetic end-to-end preflight for a research pipeline.

The preflight uses a deliberately small adapter contract: runner, verifier,
scorer, and aggregator commands receive a JSON manifest and an isolated
workspace. No campaign data is read; all datasets are embedded in the
preflight specification and materialized as synthetic files.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path
import time

_COORDINATOR_WORKER_MARKER = "GENETIK_COORDINATOR_WORKER_TOKEN"
_COORDINATOR_MEMORY_LIMIT_BYTES = 512 * 1024 * 1024
_SCRIPT_PATH = Path(__file__).resolve()
_WORKER_ENTRY_CPU_SECONDS = time.process_time()
_WORKER_ENTRY_MONOTONIC_NS = time.monotonic_ns()


class PreflightError(RuntimeError):
    pass


def _consume_worker_nonce() -> bool:
    """Validate the one-shot supervisor pipe before project imports."""
    import hmac
    import select
    raw_fd = os.environ.get("GENETIK_COORDINATOR_WORKER_FD")
    expected_hex = os.environ.get("GENETIK_COORDINATOR_WORKER_NONCE")
    if not raw_fd or not raw_fd.isdecimal() or not expected_hex or len(expected_hex) != 64:
        return False
    fd = int(raw_fd)
    try:
        os.set_blocking(fd, False)
        payload = bytearray()
        deadline = time.monotonic() + 2.0
        while len(payload) <= 32 and time.monotonic() < deadline:
            ready, _, _ = select.select([fd], [], [], max(0.0, deadline - time.monotonic()))
            if not ready:
                break
            block = os.read(fd, 33 - len(payload))
            if not block:
                break
            payload.extend(block)
        os.close(fd)
        return len(payload) == 32 and hmac.compare_digest(
            bytes(payload), bytes.fromhex(expected_hex))
    except (OSError, ValueError):
        return False


def _verify_supervisor_source(pin: object) -> None:
    """Bind the helper through this pinned coordinator entrypoint."""
    import hashlib
    if not isinstance(pin, dict) or set(pin) != {"path", "sha256"}:
        raise RuntimeError("frozen protocol lacks coordinator supervisor source pin")
    helper = _SCRIPT_PATH.with_name("coordinator_supervisor.py").resolve()
    if Path(pin["path"]).resolve() != helper:
        raise RuntimeError("coordinator supervisor path differs from frozen pin")
    if hashlib.sha256(helper.read_bytes()).hexdigest() != pin["sha256"]:
        raise RuntimeError("coordinator supervisor hash differs from frozen pin")


def _resolve_execution_program_directory(protocol: dict[str, object]) -> Path:
    """Resolve the immutable protocol anchor to its ledger-authenticated program.

    A software-only successor may update the executable identity while keeping
    the original scientific protocol as its immutable anchor. Resolution is
    deliberately explicit: direct reads of an inactive predecessor continue
    to fail in ``read_frozen_program``.
    """
    configured = protocol.get("execution_program_dir")
    if not isinstance(configured, str) or not configured:
        raise PreflightError("descriptor pipeline lacks its frozen execution-program anchor")
    from research_execution import resolve_active_successor_program
    return resolve_active_successor_program(Path(configured))


if __name__ == "__main__" and os.environ.get(_COORDINATOR_WORKER_MARKER):
    # The supervisor installs this before exec. Reapply it before importing
    # coordinator or research modules so marker spoofing cannot bypass the
    # worker's address-space boundary.
    import resource
    _, hard = resource.getrlimit(resource.RLIMIT_AS)
    cap = (min(_COORDINATOR_MEMORY_LIMIT_BYTES, hard)
           if hard != resource.RLIM_INFINITY else _COORDINATOR_MEMORY_LIMIT_BYTES)
    resource.setrlimit(resource.RLIMIT_AS, (cap, cap))
    if resource.getrlimit(resource.RLIMIT_AS) != (cap, cap):
        raise SystemExit("coordinator worker could not enforce address-space limit")
    if not _consume_worker_nonce():
        raise SystemExit("coordinator worker requires a verified inherited nonce")


def _supervise_continuation_cli(argv: list[str]) -> int | None:
    """Re-exec continuation preflight under an address-space limit before imports."""
    supervisor_started_mono = time.monotonic()
    supervisor_started_cpu = time.process_time()
    import resource
    launcher_limit_error = None
    try:
        _, hard = resource.getrlimit(resource.RLIMIT_AS)
        cap = (min(_COORDINATOR_MEMORY_LIMIT_BYTES, hard)
               if hard != resource.RLIM_INFINITY else _COORDINATOR_MEMORY_LIMIT_BYTES)
        resource.setrlimit(resource.RLIMIT_AS, (cap, cap))
        if resource.getrlimit(resource.RLIMIT_AS) != (cap, cap):
            raise RuntimeError("coordinator launcher could not enforce address-space limit")
    except (OSError, ValueError, RuntimeError) as error:
        launcher_limit_error = error
    import argparse
    import json
    import subprocess

    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--protocol", type=Path)
    args, _ = parser.parse_known_args(argv)
    if args.protocol is None or not args.protocol.is_file():
        return None
    protocol = json.loads(args.protocol.read_bytes())
    if protocol.get("pipeline_descriptor") is None:
        return None
    try:
        execution_dir = _resolve_execution_program_directory(protocol)
    except Exception as error:
        print(f"scientific-pipeline-preflight: refused: {error}", file=sys.stderr)
        return 2
    if not execution_dir.is_dir():
        return None
    from research_execution import (CONTINUATION_MEMORY_BYTES, GateError,
                                    read_frozen_program, record_supervised_coordinator_worker)
    _, config, _ = read_frozen_program(execution_dir)
    plan = config.get("continuation_plan")
    if not isinstance(plan, dict):
        return None
    ledger = config["ledger_dir"]
    phase_id = "primary_preflight"
    try:
        if launcher_limit_error is not None:
            raise GateError(f"coordinator launcher limit admission failed: {launcher_limit_error}")
        prior = [json.loads(line) for line in (ledger / "process-events.jsonl").read_text().splitlines()]
        phase_id = ("recovery_preflight" if any(event.get("event") == "preflight_rejected"
                                                for event in prior) else "primary_preflight")
        _verify_supervisor_source(plan.get("coordinator_supervisor_entrypoint"))
        sys.path.insert(0, str(_SCRIPT_PATH.parent))
        from coordinator_supervisor import (COORDINATOR_MEMORY_LIMIT_BYTES,
                                            account_supervisor_exception,
                                            attach_supervisor_accounting,
                                            limit_current_process, supervise)
        limit_current_process(COORDINATOR_MEMORY_LIMIT_BYTES)
        state = json.loads((ledger / "process-budget.json").read_text())
        remaining = (plan["overall_deadline_seconds"] if state.get("overall_deadline_monotonic_ns") is None
                     else (state["overall_deadline_monotonic_ns"] - time.monotonic_ns()) / 1_000_000_000)
        if remaining <= 0:
            raise GateError("continuation wall allowance is exhausted before preflight worker launch")
    except BaseException:
        record_supervised_coordinator_worker(execution_dir, phase_id, None, _SCRIPT_PATH)
        raise
    env = os.environ.copy()
    env[_COORDINATOR_WORKER_MARKER] = os.urandom(24).hex()
    try:
        measurement = supervise(
            [sys.executable, str(_SCRIPT_PATH), *argv],
            memory_limit_bytes=CONTINUATION_MEMORY_BYTES,
            wall_limit_seconds=remaining, output_limit_bytes=8 * 1024 * 1024, env=env,
            worker_handshake=True,
        )
    except BaseException as error:
        raw_measurement = getattr(error, "coordinator_measurement", None)
        worker_wall = raw_measurement.get("wall_seconds", 0.0) if isinstance(raw_measurement, dict) else 0.0
        failed_measurement = account_supervisor_exception(
            error, cpu_seconds=time.process_time() - supervisor_started_cpu,
            exclusive_wall_seconds=max(
                0.0, time.monotonic() - supervisor_started_mono - worker_wall),
            address_space_limit_bytes=COORDINATOR_MEMORY_LIMIT_BYTES,
        )
        record_supervised_coordinator_worker(execution_dir, phase_id,
                                             failed_measurement, _SCRIPT_PATH)
        raise
    measurement = dict(measurement)
    attach_supervisor_accounting(
        measurement,
        cpu_seconds=time.process_time() - supervisor_started_cpu,
        exclusive_wall_seconds=max(
            0.0, time.monotonic() - supervisor_started_mono - measurement["wall_seconds"]),
        address_space_limit_bytes=COORDINATOR_MEMORY_LIMIT_BYTES,
    )
    receipt = record_supervised_coordinator_worker(execution_dir, phase_id, measurement, _SCRIPT_PATH)
    if receipt.get("status") != "verified":
        return 3
    sys.stdout.buffer.write(measurement["stdout"])
    sys.stderr.buffer.write(measurement["stderr"])
    if measurement["timed_out"] or measurement["output_limit_exceeded"]:
        return 3
    return measurement["returncode"] if measurement["returncode"] >= 0 else 3


if __name__ == "__main__" and not os.environ.get(_COORDINATOR_WORKER_MARKER):
    _code = _supervise_continuation_cli(sys.argv[1:])
    if _code is not None:
        raise SystemExit(_code)


import argparse
import base64
import hashlib
import json
import math
import re
import resource
import selectors
import signal
import shutil
import subprocess
import time
from typing import Any

from bounded_process import run_bounded_process
from research_pipeline import (
    PipelineDescriptorError, expand_argv, pipeline_identity as build_pipeline_identity,
    validate_descriptor as validate_pipeline_descriptor,
)


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


def _contains_assessment_key(value: Any) -> bool:
    if isinstance(value, dict):
        return any("assessment" in str(key).lower() or _contains_assessment_key(item)
                   for key, item in value.items())
    if isinstance(value, list):
        return any(_contains_assessment_key(item) for item in value)
    return False


def _expand_fdi_index(index_doc: Any, index_path: Path, runs: list[dict[str, Any]],
                     schema: str, filename: str) -> list[dict[str, Any]]:
    if not isinstance(index_doc, dict) or index_doc.get("schema") != schema:
        raise PreflightError(f"{schema} schema mismatch")
    entries = index_doc.get("runs")
    if not isinstance(entries, list) or len(entries) != len(runs):
        raise PreflightError("indexed artifacts do not cover the complete frozen run set")
    index_path = index_path.resolve()
    evidence_root = Path(str(index_path) + ".evidence").resolve()
    expanded = []
    for ordinal, (run, entry) in enumerate(zip(runs, entries)):
        if not isinstance(entry, dict) or entry.get("run_id") != run["run_id"]:
            raise PreflightError("indexed artifacts are missing, duplicated, reordered, or misbound")
        expected = Path(index_path.name + ".evidence") / f"run-{ordinal:02d}" / filename
        relative = entry.get("path")
        if (not isinstance(relative, str) or Path(relative).is_absolute()
                or Path(relative) != expected or ".." in Path(relative).parts):
            raise PreflightError("indexed artifact path is not its canonical invocation-owned slot")
        current = index_path.parent
        for part in Path(relative).parts:
            current = current / part
            if current.is_symlink():
                raise PreflightError("indexed artifact path contains a symlink")
        artifact_path = (index_path.parent / relative).resolve(strict=True)
        if not artifact_path.is_relative_to(evidence_root) or not artifact_path.is_file():
            raise PreflightError("indexed artifact escapes invocation-owned evidence root")
        if not isinstance(entry.get("sha256"), str) or file_sha(artifact_path) != entry["sha256"]:
            raise PreflightError("indexed artifact SHA256 mismatch")
        try:
            value = json.loads(artifact_path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise PreflightError(f"cannot read indexed artifact: {error}") from error
        if not isinstance(value, dict) or value.get("run_id") != run["run_id"]:
            raise PreflightError("indexed artifact content identity mismatch")
        expanded.append(value)
    return expanded


def _validate_fdi_synthetic_pipeline(
    *, preflight_id: str, runner_manifest: dict[str, Any], runner_manifest_bytes: bytes,
    scorer_manifest: dict[str, Any], runs: list[dict[str, Any]],
    training_bytes_by_control: dict[str, bytes], assessment_bytes_by_control: dict[str, bytes],
    training_path_by_control: dict[str, str],
    training_bundle: Any, assessment_bundle: Any, aggregate_manifest: Any,
    candidate_records: Any, verified_records: Any, score_output: Any, report_output: Any,
    scores_sha256: str, candidate_index_path: Path, receipt_index_path: Path,
    protocol_training_bindings: list[dict[str, Any]],
) -> dict[str, Any]:
    """Independently qualify the pinned FDI stages on fixed synthetic fixtures.

    The runner sees only training inputs and a deterministic two-catalogue
    projection. Assessment bytes enter the scorer input only after candidate and verifier outputs are
    frozen. This validator recomputes every fixture prediction and aggregate.
    """
    if runner_manifest.get("schema") != "genetik-fdi-run-manifest-v2" or runner_manifest.get("mode") != "synthetic-preflight":
        raise PreflightError("runner preflight manifest is not the pinned FDI v2 synthetic mode")
    if runner_manifest.get("protocol_id") != preflight_id or runner_manifest.get("preflight_id") != preflight_id:
        raise PreflightError("runner synthetic manifest has the wrong protocol identity")
    if _contains_assessment_key(runner_manifest):
        raise PreflightError("runner-visible synthetic manifest contains assessment data")
    manifest_sha = digest(runner_manifest_bytes)
    ordered_control_ids = sorted({run["control_id"] for run in runs})
    fixture_by_control = {control_id: ("ambiguity" if index % 2 == 0 else "arithmetic")
                          for index, control_id in enumerate(ordered_control_ids)}
    expected_bindings = []
    expected_training = {}
    expected_assessment = {}
    expected_synthetic_ids = {}
    for control_id in ordered_control_ids:
        fixture = fixture_by_control[control_id]
        train_rows = [(0, 0)] if fixture == "ambiguity" else [(0, 2)]
        assess_rows = [(9, 9)] if fixture == "ambiguity" else [(9, 11)]
        train_bytes = b"genetik-fdi-training-v1\n" + b"".join(f"{x}\t{y}\n".encode() for x, y in train_rows)
        assess_bytes = b"genetik-fdi-assessment-v1\n" + b"".join(f"{x}\t{y}\n".encode() for x, y in assess_rows)
        train_id, assess_id = f"synthetic-train-{control_id}", f"synthetic-assess-{control_id}"
        expected_training[control_id] = train_bytes
        expected_assessment[control_id] = assess_bytes
        expected_synthetic_ids[control_id] = (train_id, assess_id)
        declared = next(item for item in scorer_manifest.get("controls", []) if item.get("id") == control_id)
        expected_bindings.append({"control_id": control_id,
            "protocol_training_artifact": declared.get("protocol_training_artifact"),
            "protocol_training_sha256": declared.get("protocol_training_sha256"),
            "synthetic_training_artifact": train_id, "synthetic_training_sha256": digest(train_bytes),
            "protocol_assessment_artifact": declared.get("protocol_assessment_artifact"),
            "protocol_assessment_sha256": declared.get("protocol_assessment_sha256"),
            "synthetic_assessment_artifact": assess_id, "synthetic_assessment_sha256": digest(assess_bytes)})
    if protocol_training_bindings != expected_bindings:
        raise PreflightError("synthetic-to-protocol artifact bindings are incomplete or misassociated")
    expected_runs = []
    control_ids = []
    for run in runs:
        control_id = run["control_id"]
        control_ids.append(control_id)
        train_bytes = training_bytes_by_control[control_id]
        if train_bytes != expected_training[control_id]:
            raise PreflightError("synthetic training bytes differ from the frozen two-fixture projection")
        expected_runs.append({"run_id": run["run_id"], "control_id": control_id,
            "method": run["method"], "seed": run["seed"],
            "training_artifact": expected_synthetic_ids[control_id][0],
            "training_sha256": digest(expected_training[control_id]),
            "training_path": training_path_by_control[control_id]})
    for control_id, path in training_path_by_control.items():
        training_file = Path(path)
        if not training_file.is_file() or training_file.read_bytes() != expected_training[control_id]:
            raise PreflightError("runner-visible training path does not match the exact synthetic training bytes")
    if runner_manifest.get("runs") != expected_runs:
        raise PreflightError("runner manifest run rows do not bind exact synthetic training artifacts")
    expected_catalogue = [{"control_id": control_id, "catalogue": fixture_by_control[control_id],
                           "reference_member": 0}
                          for control_id in ordered_control_ids]
    if runner_manifest.get("catalogue_map") != expected_catalogue:
        raise PreflightError("synthetic runner catalogue map differs from the frozen ambiguity fixture")
    if scorer_manifest.get("schema") != "genetik-fdi-scorer-manifest-v2" or scorer_manifest.get("mode") != "synthetic-preflight":
        raise PreflightError("scorer manifest is not the pinned FDI v2 synthetic mode")
    if scorer_manifest.get("protocol_id") != preflight_id or scorer_manifest.get("preflight_id") != preflight_id:
        raise PreflightError("scorer synthetic manifest has the wrong protocol identity")
    if scorer_manifest.get("runner_manifest_sha256") != manifest_sha or scorer_manifest.get("runs") != expected_runs:
        raise PreflightError("scorer manifest is detached from the training-only runner manifest")
    expected_controls = [{"id": control_id, "catalogue": fixture_by_control[control_id],
        "reference_member": 0,
        "protocol_training_artifact": next(item["protocol_training_artifact"] for item in expected_bindings if item["control_id"] == control_id),
        "protocol_training_sha256": next(item["protocol_training_sha256"] for item in expected_bindings if item["control_id"] == control_id),
        "protocol_assessment_artifact": next(item["protocol_assessment_artifact"] for item in expected_bindings if item["control_id"] == control_id),
        "protocol_assessment_sha256": next(item["protocol_assessment_sha256"] for item in expected_bindings if item["control_id"] == control_id),
        "training": {"id": expected_synthetic_ids[control_id][0],
                     "sha256": digest(expected_training[control_id])},
        "assessment": {"id": expected_synthetic_ids[control_id][1],
                       "sha256": digest(expected_assessment[control_id])}}
        for control_id in ordered_control_ids]
    if scorer_manifest.get("controls") != expected_controls:
        raise PreflightError("scorer manifest does not bind exact synthetic training/assessment identities")
    expected_training_bundle = {"schema": "genetik-fdi-training-bundle-v1", "artifacts": [
        {"id": expected_synthetic_ids[control_id][0], "sha256": digest(expected_training[control_id]),
         "content_base64": base64.b64encode(expected_training[control_id]).decode("ascii")}
        for control_id in ordered_control_ids]}
    if training_bundle != expected_training_bundle:
        raise PreflightError("runner training bundle differs from the exact synthetic training fixture")
    expected_assessment_bundle = {"schema": "genetik-fdi-assessment-bundle-v1", "artifacts": [
        {"id": expected_synthetic_ids[control_id][1],
         "sha256": digest(expected_assessment[control_id]),
         "content_base64": base64.b64encode(expected_assessment[control_id]).decode("ascii")}
        for control_id in ordered_control_ids]}
    if assessment_bundle != expected_assessment_bundle:
        raise PreflightError("scorer assessment bundle differs from the exact synthetic assessment fixture")
    if (not isinstance(aggregate_manifest, dict)
            or aggregate_manifest.get("schema") != "genetik-fdi-aggregate-manifest-v2"
            or aggregate_manifest.get("mode") != "synthetic-preflight"
            or aggregate_manifest.get("protocol_id") != preflight_id
            or aggregate_manifest.get("preflight_id") != preflight_id
            or aggregate_manifest.get("runs") != expected_runs):
        raise PreflightError("aggregator manifest does not bind the full synthetic run set")

    if (not isinstance(candidate_records, dict)
            or candidate_records.get("protocol_id") != preflight_id
            or candidate_records.get("run_manifest_sha256") != manifest_sha):
        raise PreflightError("candidate index is detached from the exact synthetic runner manifest")
    candidate_records = _expand_fdi_index(candidate_records, candidate_index_path, runs,
        "genetik-fdi-candidate-output-index-v1", "candidate.json")
    verified_records = _expand_fdi_index(verified_records, receipt_index_path, runs,
        "genetik-fdi-verified-receipt-index-v1", "receipt.json")
    if not isinstance(score_output, dict) or score_output.get("schema") != "genetik-fdi-scores-v2":
        raise PreflightError("scorer output has the wrong v2 schema")
    scorer_manifest_sha = digest(canonical(scorer_manifest) + b"\n")
    if score_output.get("protocol_id") != preflight_id or score_output.get("run_manifest_sha256") != scorer_manifest_sha:
        raise PreflightError("scorer output is detached from the exact scorer manifest")
    scores = score_output.get("scores")
    if not isinstance(scores, list) or len(scores) != len(runs):
        raise PreflightError("scorer output count differs from the frozen synthetic run set")
    if not isinstance(report_output, dict) or report_output.get("schema") != "genetik-fdi-report-v2":
        raise PreflightError("aggregator output has the wrong v2 schema")
    if report_output.get("protocol_id") != preflight_id:
        raise PreflightError("aggregator report has the wrong synthetic protocol identity")
    if report_output.get("scores_sha256") != scores_sha256:
        raise PreflightError("aggregator report is not bound to exact scorer output bytes")

    for index, run in enumerate(runs):
        candidate = candidate_records[index]
        verified = verified_records[index]
        score = scores[index]
        run_id, control_id = run["run_id"], run["control_id"]
        expected_identity = {key: run[key] for key in ("run_id", "control_id", "method", "seed")}
        fixture = fixture_by_control[control_id]
        if not isinstance(candidate, dict) or candidate.get("schema") != "genetik-fdi-candidate-output-v2":
            raise PreflightError(f"runner output schema is invalid for {run_id}")
        if (any(candidate.get(key) != value for key, value in expected_identity.items())
                or candidate.get("protocol_id") != preflight_id
                or candidate.get("run_manifest_sha256") != manifest_sha
                or candidate.get("training_artifact") != expected_synthetic_ids[control_id][0]
                or candidate.get("training_sha256") != digest(training_bytes_by_control[control_id])):
            raise PreflightError(f"runner candidate identity/training binding is invalid for {run_id}")
        if (candidate.get("completion_status") != "completed"
                or candidate.get("report_validation_status") != "valid_report"
                or not isinstance(candidate.get("transcript"), dict)
                or candidate["transcript"].get("stop_reason") != "one_finite_behavior_class"
                or not isinstance(candidate.get("receipt"), dict)
                or not isinstance(candidate.get("evidence_directory"), str)):
            raise PreflightError(f"runner candidate transcript/receipt is incomplete for {run_id}")
        if _contains_assessment_key(candidate):
            raise PreflightError(f"runner candidate output contains assessment-derived fields for {run_id}")
        if candidate.get("receipt").get("wait4_usage_complete") is not True:
            raise PreflightError(f"runner child resource receipt is incomplete for {run_id}")
        stream_hashes = candidate.get("raw_stream_sha256")
        if not isinstance(stream_hashes, dict) or set(stream_hashes) != {
                "producer_stdout", "peer_stdout", "producer_stderr", "peer_stderr"}:
            raise PreflightError(f"runner stream evidence map is malformed for {run_id}")
        for stream_name, value in stream_hashes.items():
            if not isinstance(value, str) or len(value) != 64 or any(char not in "0123456789abcdef" for char in value):
                raise PreflightError(f"runner stream evidence is malformed for {run_id}")
        if not isinstance(verified, dict) or verified.get("schema") != "genetik-fdi-verified-receipt-v2":
            raise PreflightError(f"verifier receipt schema is invalid for {run_id}")
        if (verified.get("run_id") != run_id
                or verified.get("candidate_output_sha256") != digest(canonical(candidate))
                or verified.get("run_manifest_sha256") != manifest_sha
                or verified.get("training_sha256") != digest(training_bytes_by_control[control_id])
                or verified.get("transcript") != candidate.get("transcript")
                or verified.get("candidate_identity") != {key: candidate.get(key) for key in (
                    "schema","protocol_id","run_id","control_id","method","seed",
                    "training_artifact","training_sha256","completion_status","report_validation_status")}
                or verified.get("verified") is not True):
            raise PreflightError(f"verifier did not authenticate exact candidate/training identity for {run_id}")
        if not isinstance(score, dict) or any(score.get(key) != value for key, value in expected_identity.items()):
            raise PreflightError(f"scorer row identity is invalid for {run_id}")
        if fixture == "ambiguity":
            expected_prediction_map = {"c0": {"training": [0], "assessment": [9]},
                "c1": {"training": [0], "assessment": [9]},
                "c2": {"training": [0], "assessment": [10]}}
            expected_training_ids, expected_assess_ids = ["c0", "c1", "c2"], ["c0", "c1"]
        else:
            expected_prediction_map = {"c0": {"training": [2, -6], "assessment": [11]},
                "c1": {"training": [2, -6], "assessment": [11]}}
            expected_training_ids, expected_assess_ids = ["c0", "c1"], ["c0", "c1"]
            steps = candidate["transcript"].get("steps", [])
            if (len(steps) != 1 or steps[0].get("input") != -8
                    or steps[0].get("response", {}).get("value") != -6
                    or type(steps[0].get("revision")) is not int or steps[0].get("revision") != 1):
                raise PreflightError(f"arithmetic fixture lacks the frozen adaptive counterexample for {run_id}")
        if (score.get("training_exact_candidate_ids") != expected_training_ids
                or score.get("assessment_exact_candidate_ids") != expected_assess_ids
                or score.get("predictions") != expected_prediction_map
                or score.get("exact") is not True or score.get("status") != "scored"):
            raise PreflightError(f"independent scorer disagrees with synthetic {fixture} fixture for {run_id}")
    expected_count = len(runs)
    if (report_output.get("expected_run_count") != expected_count
            or report_output.get("scored_run_count") != expected_count
            or report_output.get("all_scored") is not True
            or report_output.get("acceptance_gate_passed") is not True):
        raise PreflightError("aggregator report does not qualify all synthetic runs")
    expected_per_control = {}
    for run in runs:
        row = expected_per_control.setdefault(run["control_id"], {"run_count": 0, "exact_run_count": 0})
        row["run_count"] += 1
        row["exact_run_count"] += 1
    if report_output.get("per_control") != expected_per_control:
        raise PreflightError("aggregator per-control counts do not match independent synthetic scoring")
    return {"run_count": expected_count, "exact_run_count": expected_count,
            "run_ids": [run["run_id"] for run in runs]}


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
    source_evidence=Path(output["path"]+".evidence")
    if source_evidence.exists():
        destination_evidence=Path(str(destination)+".evidence")
        if destination_evidence.exists():
            raise PreflightError(f"{stage} evidence destination already exists")
        shutil.copytree(source_evidence,destination_evidence,symlinks=True)
    return {"stage": stage, "run_id": run_id, "argv": receipt["command"],
            "executable_sha256": receipt["executable_sha256"],
            "returncode": receipt["returncode"], "status": receipt["status"],
            "wall_seconds": receipt["wall_seconds"], "cpu_seconds": receipt["cpu_seconds"],
            "peak_process_group_rss_bytes": receipt.get("peak_process_group_rss_bytes"),
            "process_group_rss_available": receipt.get("process_group_rss_available"),
            "stdout_sha256": receipt["stdout_sha256"], "stderr_sha256": receipt["stderr_sha256"],
            "bounded_program_receipt_sha256": digest(canonical(receipt))}


def _run_preflight_impl(protocol_path: Path, spec_path: Path, receipt_path: Path) -> dict[str, Any]:
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
        execution_program_dir = _resolve_execution_program_directory(protocol)
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
                "protocol_training_artifact": control["protocol_training_artifact"],
                "protocol_training_sha256": control["protocol_training_sha256"],
                "protocol_assessment_artifact": control["protocol_assessment_artifact"],
                "protocol_assessment_sha256": control["protocol_assessment_sha256"],
            })
        run_bindings = [
            {"run_id": r["run_id"], "control_id": r["control_id"], "method": r["method"], "seed": r["seed"]}
            for r in runs
        ]
        runner_runs = [
            {key: run[key] for key in ("run_id", "control_id", "method", "seed", "training_artifact")}
            for run in runs
        ]
        continuation = execution_config.get("continuation_plan") if execution_config is not None else None
        if continuation is not None:
            def pairs(rows: list[Any], label: str) -> list[tuple[int, int]]:
                result = []
                for row in rows:
                    if isinstance(row, list) and len(row) == 2:
                        pair = row
                    elif isinstance(row, dict):
                        input_keys = [key for key in ("n", "x", "input") if key in row]
                        output_keys = [key for key in ("expected", "value", "y", "output") if key in row]
                        if len(input_keys) != 1 or len(output_keys) != 1:
                            raise PreflightError(f"synthetic {label} rows require one integer input/output pair")
                        pair = [row[input_keys[0]], row[output_keys[0]]]
                    else:
                        raise PreflightError(f"synthetic {label} rows require integer pairs")
                    if any(type(value) is not int or not -(1 << 63) <= value < (1 << 63) for value in pair):
                        raise PreflightError(f"synthetic {label} values must be signed Int64 integers")
                    result.append((pair[0], pair[1]))
                return result

            training_tsv: dict[str, tuple[Path, bytes]] = {}
            assessment_tsv: dict[str, tuple[Path, bytes]] = {}
            fixture_by_control = {control_id: ("ambiguity" if ordinal % 2 == 0 else "arithmetic")
                                  for ordinal, control_id in enumerate(sorted(c["id"] for c in controls))}
            synthetic_ids = {control_id: (f"synthetic-train-{control_id}", f"synthetic-assess-{control_id}")
                             for control_id in fixture_by_control}
            for control in controls:
                control_id = control["id"]
                fixture = fixture_by_control[control_id]
                train_bytes = (b"genetik-fdi-training-v1\n0\t0\n" if fixture == "ambiguity"
                               else b"genetik-fdi-training-v1\n0\t2\n")
                assess_bytes = (b"genetik-fdi-assessment-v1\n9\t9\n" if fixture == "ambiguity"
                                else b"genetik-fdi-assessment-v1\n9\t11\n")
                train_path = workdir / f"{control_id}-synthetic-training.tsv"
                assess_path = workdir / f"{control_id}-synthetic-assessment.tsv"
                train_path.write_bytes(train_bytes)
                assess_path.write_bytes(assess_bytes)
                training_tsv[control_id] = (train_path, train_bytes)
                assessment_tsv[control_id] = (assess_path, assess_bytes)
            runner_runs_v2 = []
            for run in runs:
                train_path, train_bytes = training_tsv[run["control_id"]]
                runner_runs_v2.append({
                    "run_id": run["run_id"], "control_id": run["control_id"],
                    "method": run["method"], "seed": run["seed"],
                    "training_artifact": synthetic_ids[run["control_id"]][0],
                    "training_sha256": digest(train_bytes), "training_path": str(train_path.resolve()),
                })
            interactive_session = continuation["interactive_session"]
            runner_manifest_obj = {
                "schema": "genetik-fdi-run-manifest-v2", "mode": "synthetic-preflight",
                "protocol_id": spec["preflight_id"], "preflight_id": spec["preflight_id"],
                "interactive_session": interactive_session,
                "catalogue_map": [{"control_id": control_id, "catalogue": fixture_by_control[control_id],
                                   "reference_member": 0}
                                   for control_id in sorted(fixture_by_control)],
                "runs": runner_runs_v2,
            }
            training_bundle_obj = {"schema": "genetik-fdi-training-bundle-v1", "artifacts": [
                {"id": synthetic_ids[control["id"]][0], "sha256": digest(training_tsv[control["id"]][1]),
                 "content_base64": base64.b64encode(training_tsv[control["id"]][1]).decode("ascii")}
                for control in sorted(controls, key=lambda item: item["id"])
            ]}
            assessment_bundle_obj = {"schema": "genetik-fdi-assessment-bundle-v1", "artifacts": [
                {"id": synthetic_ids[control["id"]][1], "sha256": digest(assessment_tsv[control["id"]][1]),
                 "content_base64": base64.b64encode(assessment_tsv[control["id"]][1]).decode("ascii")}
                for control in controls
            ]}
            training_bundle = workdir / "training-bundle.json"
            training_bundle.write_bytes(canonical(training_bundle_obj) + b"\n")
            assessment_bundle = workdir / "assessment-bundle.json"
            assessment_bundle.write_bytes(canonical(assessment_bundle_obj) + b"\n")
            scorer_manifest_obj = {
                "schema": "genetik-fdi-scorer-manifest-v2", "mode": "synthetic-preflight",
                "protocol_id": spec["preflight_id"], "preflight_id": spec["preflight_id"],
                "interactive_session": interactive_session,
                "runs": runner_runs_v2,
                "controls": [{"id": control["id"], "catalogue": fixture_by_control[control["id"]],
                              "reference_member": 0,
                              "protocol_training_artifact": control["protocol_training_artifact"],
                              "protocol_training_sha256": control["protocol_training_sha256"],
                              "protocol_assessment_artifact": control["protocol_assessment_artifact"],
                              "protocol_assessment_sha256": control["protocol_assessment_sha256"],
                              "training": {"id": synthetic_ids[control["id"]][0],
                                           "sha256": digest(training_tsv[control["id"]][1])},
                              "assessment": {"id": synthetic_ids[control["id"]][1],
                                             "sha256": digest(assessment_tsv[control["id"]][1])}}
                for control in sorted(controls, key=lambda item: item["id"])],
                "runner_manifest_sha256": digest(canonical(runner_manifest_obj) + b"\n"),
            }
            protocol_training_bindings = [{"control_id": control["id"],
                "protocol_training_artifact": control["protocol_training_artifact"],
                "protocol_training_sha256": control["protocol_training_sha256"],
                "synthetic_training_artifact": synthetic_ids[control["id"]][0],
                "synthetic_training_sha256": digest(training_tsv[control["id"]][1]),
                "protocol_assessment_artifact": control["protocol_assessment_artifact"],
                "protocol_assessment_sha256": control["protocol_assessment_sha256"],
                "synthetic_assessment_artifact": synthetic_ids[control["id"]][1],
                "synthetic_assessment_sha256": digest(assessment_tsv[control["id"]][1])}
                for control in sorted(controls, key=lambda item: item["id"])]
        else:
            runner_manifest_obj = {
                "schema": 1, "preflight_id": spec["preflight_id"],
                "controls": [{"id": c["id"], "training": c["training"]} for c in controls], "runs": runner_runs,
                "expected_run_count": spec["expected_run_count"],
                "outputs_dir": str(workdir / "outputs"),
            }
            scorer_manifest_obj = {
                **runner_manifest_obj, "runs": runs,
                "controls": controls,
                "candidate_outputs": str(workdir / "candidate-outputs.json"),
                "verified_receipts": str(workdir / "verified-receipts.json"),
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
        (workdir / "outputs").mkdir()
        runner_manifest = workdir / "runner-manifest.json"
        runner_manifest.write_bytes(runner_manifest_bytes)
        scorer_manifest = workdir / "scorer-manifest.json"
        scorer_manifest.write_bytes(canonical(scorer_manifest_obj) + b"\n")
        aggregate_manifest = workdir / "aggregate-manifest.json"
        aggregate_manifest_obj = ({
            "schema": "genetik-fdi-aggregate-manifest-v2", "mode": "synthetic-preflight",
            "protocol_id": spec["preflight_id"], "preflight_id": spec["preflight_id"],
            "runs": runner_manifest_obj["runs"],
        } if continuation is not None else {
            "schema": 1, "preflight_id": spec["preflight_id"],
            "runs": runs, "scores": str(workdir / "independent-scores.json"),
            "expected_run_count": len(runs), "report": str(workdir / "scientific-report.json"),
        })
        aggregate_manifest.write_bytes(canonical(aggregate_manifest_obj) + b"\n")
        stage_manifests = {"runner_manifest": runner_manifest, "scorer_manifest": scorer_manifest}
        descriptor = normalized_descriptor
        if descriptor is not None:
            written_preflight_inputs = {}
            selected_qualification = execution_config["preflight_qualification"]
            # Recovery is selected by the authorized state machine rather than
            # by a caller flag. The public wrapper computes that phase before
            # any stage begins; the frozen runner slot set is then chosen here.
            if execution_config.get("continuation_plan") is not None:
                ledger_events = [json.loads(line) for line in (execution_config["ledger_dir"] / "process-events.jsonl").read_text().splitlines()]
                selected_qualification = (execution_config["recovery_qualification"]
                    if any(event.get("event") == "preflight_rejected" for event in ledger_events)
                    else execution_config["preflight_qualification"])
            for stage_name, run_id in selected_qualification["stage_run_ids"].items():
                stage_run = execution_config["run_by_id"][run_id]
                for slot, binding in stage_run["slot_bindings"].items():
                    if binding["kind"] != "controller_file":
                        continue
                    target = (execution_program_dir / binding["path"]).resolve()
                    if execution_program_dir not in target.parents:
                        raise PreflightError("frozen preflight controller input path escapes program directory")
                    if continuation is not None:
                        source = (runner_manifest if slot == "run_manifest" and stage_name in {"runner", "verifier"}
                                  else training_bundle if slot == "training_bundle"
                                  else assessment_bundle if slot == "assessment_bundle"
                                  else scorer_manifest if slot == "run_manifest" and stage_name == "scorer"
                                  else aggregate_manifest)
                    else:
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
                run_id = selected_qualification["stage_run_ids"][stage]
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
        if continuation is None and (not isinstance(output_records, list) or len(output_records) != len(runs)
                or not isinstance(receipt_records, list) or len(receipt_records) != len(runs)):
            raise PreflightError("candidate/receipt count differs from frozen run matrix")
        scores_path = workdir / "independent-scores.json"
        if not scores_path.is_file():
            raise PreflightError("scorer did not produce independent-scores.json")
        report = workdir / "scientific-report.json"
        if not report.is_file():
            raise PreflightError("aggregator did not produce scientific-report.json")
        report_obj = json.loads(report.read_text())
        if continuation is not None:
            runner_manifest_read = json.loads(runner_manifest.read_bytes())
            scorer_manifest_read = json.loads(scorer_manifest.read_bytes())
            score_output = json.loads(scores_path.read_bytes())
            aggregate_manifest_read = json.loads(aggregate_manifest.read_bytes())
            synthetic_result = _validate_fdi_synthetic_pipeline(
                preflight_id=spec["preflight_id"], runner_manifest=runner_manifest_read,
                runner_manifest_bytes=runner_manifest.read_bytes(), scorer_manifest=scorer_manifest_read,
                runs=runs, training_bytes_by_control={key: value[1] for key, value in training_tsv.items()},
                assessment_bytes_by_control={key: value[1] for key, value in assessment_tsv.items()},
                training_path_by_control={key: str(value[0].resolve()) for key, value in training_tsv.items()},
                training_bundle=training_bundle_obj, assessment_bundle=assessment_bundle_obj,
                aggregate_manifest=aggregate_manifest_read,
                candidate_records=output_records, verified_records=receipt_records,
                score_output=score_output, report_output=report_obj,
                scores_sha256=file_sha(scores_path), candidate_index_path=outputs_path,
                receipt_index_path=receipts_path, protocol_training_bindings=protocol_training_bindings)
            expected_per_control = {}
            for run in runs:
                row = expected_per_control.setdefault(run["control_id"], {"run_count": 0, "exact_run_count": 0})
                row["run_count"] += 1
                row["exact_run_count"] += 1
        else:
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
            expected_per_control = {}
            for run in runs:
                score = scores_by_id[run["run_id"]]
                entry = expected_per_control.setdefault(run["control_id"], {"run_count": 0, "exact_run_count": 0})
                entry["run_count"] += 1
                entry["exact_run_count"] += int(score.get("exact") is True)
            if (report_obj.get("preflight_id") != spec["preflight_id"]
                    or report_obj.get("assessment_source") != "synthetic"
                    or report_obj.get("run_count") != len(runs)
                    or report_obj.get("scored_run_count") != len(runs)
                    or report_obj.get("exact_run_count") != sum(row["exact_run_count"] for row in expected_per_control.values())
                    or report_obj.get("per_control") != expected_per_control
                    or report_obj.get("status") != "passed"
                    or report_obj.get("run_ids") != [r["run_id"] for r in runs]
                    or report_obj.get("run_bindings") != run_bindings):
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
                    "synthetic_training_id": synthetic_ids[c["id"]][0] if continuation is not None else c["training"]["id"],
                    "synthetic_training_sha256": digest(training_tsv[c["id"]][1]) if continuation is not None else c["training"]["sha256"],
                    "synthetic_assessment_id": synthetic_ids[c["id"]][1] if continuation is not None else c["assessment"]["id"],
                    "synthetic_assessment_sha256": digest(assessment_tsv[c["id"]][1]) if continuation is not None else c["assessment"]["sha256"],
                }
                for c in spec["controls"]
            ],
            "protocol_training_bindings": protocol_training_bindings if continuation is not None else None,
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


def run_preflight(protocol_path: Path, spec_path: Path, receipt_path: Path) -> dict[str, Any]:
    """Run preflight inside the continuation's measured coordinator phase."""
    protocol = json.loads(protocol_path.read_bytes())
    if protocol.get("pipeline_descriptor") is None:
        return _run_preflight_impl(protocol_path, spec_path, receipt_path)
    execution_program_dir = _resolve_execution_program_directory(protocol)
    from research_execution import (begin_coordinator_phase, coordinator_phase_guard,
                                    finish_coordinator_phase,
                                    read_frozen_program, register_coordinator_entry_baseline)
    register_coordinator_entry_baseline(_WORKER_ENTRY_CPU_SECONDS,
                                        _WORKER_ENTRY_MONOTONIC_NS)
    try:
        _, execution_config, _ = read_frozen_program(execution_program_dir)
    except Exception as error:
        raise PreflightError(f"frozen pipeline execution protocol is invalid: {error}") from error
    continuation = execution_config.get("continuation_plan")
    if continuation is None:
        return _run_preflight_impl(protocol_path, spec_path, receipt_path)
    preflight_pin = continuation.get("preflight_coordinator_entrypoint")
    actual_preflight = Path(__file__).resolve()
    if (not isinstance(preflight_pin, dict)
            or Path(str(preflight_pin.get("path", ""))).resolve() != actual_preflight
            or file_sha(actual_preflight) != preflight_pin.get("sha256")):
        raise PreflightError("continuation preflight coordinator entrypoint hash/path mismatch")
    ledger_events = [json.loads(line) for line in
                     (execution_config["ledger_dir"] / "process-events.jsonl").read_text().splitlines()]
    phase_id = ("recovery_preflight" if any(event.get("event") == "preflight_rejected"
                                             for event in ledger_events)
                else "primary_preflight")
    token = begin_coordinator_phase(execution_program_dir, phase_id)
    try:
        with coordinator_phase_guard(execution_program_dir, token["token"]):
            receipt = _run_preflight_impl(protocol_path, spec_path, receipt_path)
        phase_receipt = finish_coordinator_phase(execution_program_dir, token["token"], status="completed",
                                                 evidence_sha256=file_sha(receipt_path))
        receipt["_coordinator_phase"] = {
            "token": phase_receipt["token"], "phase_id": phase_receipt["phase_id"],
            "pid": phase_receipt["pid"],
            "pid_starttime_ticks": phase_receipt["coordinator_pid_starttime_ticks"],
            "worker_self_peak_rss_bytes": phase_receipt["coordinator_peak_rss_bytes"],
            "worker_self_vm_hwm_bytes": phase_receipt["coordinator_proc_vm_hwm_bytes"],
            "status": phase_receipt["status"],
        }
        return receipt
    except BaseException as error:
        failure_category = "coordinator_phase_failure"
        try:
            _, execution_config, protocol_sha = read_frozen_program(execution_program_dir)
            events = [json.loads(line) for line in
                      (execution_config["ledger_dir"] / "process-events.jsonl").read_text().splitlines()]
            scoped = [event for event in events if event.get("protocol_sha256") == protocol_sha]
            expected_ids = set(continuation["slots"][phase_id])
            stage_events = [event for event in scoped if event.get("event") == "completed"
                            and event.get("run_id") in expected_ids]
            statuses = {event.get("run_id"): event.get("status") for event in stage_events}
            hard_statuses = {"timed_out", "resource_exhausted", "resource_accounting_unavailable",
                             "coordinator_limit", "orphaned_process_group"}
            if any(status in hard_statuses for status in statuses.values()):
                failure_category = "coordinator_phase_failure"
            elif set(statuses) != expected_ids or any(status != "completed" for status in statuses.values()):
                failure_category = "stage_failure"
            else:
                failure_category = "qualification_failure"
        except Exception:
            failure_category = "coordinator_phase_failure"
        try:
            finish_coordinator_phase(execution_program_dir, token["token"], status="failed",
                                     reason=f"{type(error).__name__}: {error}",
                                     failure_category=failure_category)
        except BaseException as accounting_error:
            raise PreflightError(
                f"preflight failed ({type(error).__name__}: {error}); phase accounting also failed "
                f"({type(accounting_error).__name__}: {accounting_error})") from error
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
        execution_program_dir = _resolve_execution_program_directory(protocol)
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
        print(json.dumps({"status": result["status"], "preflight_id": result["preflight_id"],
                          "receipt_sha256": file_sha(args.receipt),
                          "coordinator_phase": result.get("_coordinator_phase")}, sort_keys=True))
        return 0
    except (PreflightError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"scientific-pipeline-preflight: refused: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
