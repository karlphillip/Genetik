#!/usr/bin/env python3
"""Validate references against explicit Genetik grammars using its C++ parser/evaluator."""
import argparse
import hashlib
import json
import math
import pathlib
import subprocess
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--validator", required=True, help="Genetik expression-validation executable")
    ap.add_argument("--manifest", required=True, help="standalone JSON task manifest")
    ap.add_argument("--output", help="optional machine-readable output path")
    args = ap.parse_args()
    manifest_path = pathlib.Path(args.manifest).resolve()
    manifest = json.loads(manifest_path.read_text())
    base = manifest_path.parent
    results, failed = [], False
    for task in manifest["tasks"]:
        cfg = task["grammar"]
        fit = task.get("fitting")
        config_errors = []
        resources = task.get("resources", {})
        resource_keys = ("population_size", "candidate_evaluations", "training_loss_attempts",
                        "generation_attempts", "worker_count", "process_tree_rss_cap_bytes",
                        "virtual_address_space_cap_bytes")
        for key in resource_keys:
            if type(resources.get(key)) is not int or resources[key] <= 0:
                config_errors.append(f"invalid benchmark resource: {key} must be a positive integer")
        try:
            wall_seconds = float(resources["wall_seconds"])
            affinity = resources["cpu_affinity"]
            if not math.isfinite(wall_seconds) or wall_seconds <= 0:
                config_errors.append("invalid benchmark resource: wall_seconds must be finite and positive")
            if not isinstance(affinity, list) or any(type(cpu) is not int or cpu < 0 for cpu in affinity):
                config_errors.append("invalid benchmark resource: cpu_affinity must be nonnegative integer IDs")
            elif len(set(affinity)) != len(affinity) or len(affinity) < resources.get("worker_count", 0):
                config_errors.append("invalid benchmark resource: CPU affinity must uniquely cover every worker")
        except (KeyError, TypeError, ValueError):
            config_errors.append("missing or invalid benchmark wall-time/CPU resource configuration")
        if fit is not None:
            if fit.get("method") not in ("Coordinate", "LevenbergMarquardt"):
                config_errors.append("unsupported fitting method")
            for key in ("max_loss_attempts", "max_iterations", "max_jacobian_evaluations",
                        "max_mathematical_operations", "max_workspace_bytes"):
                if type(fit.get(key)) is not int or fit[key] <= 0:
                    config_errors.append(f"invalid fitting resource: {key} must be a positive integer")
            try:
                lower, upper = float(fit["lower"]), float(fit["upper"])
                if not math.isfinite(lower) or not math.isfinite(upper) or lower > upper:
                    config_errors.append("invalid fitting parameter bounds")
                if [lower, upper] != [float(x) for x in cfg["fit_parameter_bounds"]]:
                    config_errors.append("fitting bounds differ from declared grammar bounds")
                if max(abs(lower), abs(upper)) > float(cfg["max_abs_constant"]):
                    config_errors.append("fitting parameter bounds exceed the expression constant limit")
            except (KeyError, TypeError, ValueError):
                config_errors.append("missing or invalid fitting parameter bounds")
        for key in ("variables", "max_nodes", "max_depth", "max_arithmetic_operations", "proposal_limit"):
            if type(cfg.get(key)) is not int or cfg[key] <= 0:
                config_errors.append(f"invalid grammar limit: {key} must be a positive integer")
        for key in ("initial_max_depth", "mutation_max_depth"):
            value = cfg.get(key)
            max_depth = cfg.get("max_depth")
            if (type(value) is not int or value < 0 or type(max_depth) is not int
                    or value > max_depth):
                config_errors.append(f"invalid grammar limit: {key} must be between zero and max_depth")
        if not isinstance(cfg.get("operators"), list) or not cfg["operators"] or any(
                not isinstance(op, str) or not op for op in cfg.get("operators", [])):
            config_errors.append("invalid grammar operator vocabulary")
        try:
            max_constant = float(cfg["max_abs_constant"])
            terminals = cfg["constant_terminals"]
            if not math.isfinite(max_constant) or max_constant < 0:
                config_errors.append("max_abs_constant must be finite and nonnegative")
            if not isinstance(terminals, list) or any(
                    not math.isfinite(float(value)) or abs(float(value)) > max_constant
                    for value in terminals):
                config_errors.append("constant terminals must be finite and within max_abs_constant")
        except (KeyError, TypeError, ValueError):
            config_errors.append("missing or invalid constant-terminal configuration")
        try:
            for key in ("integer_exponents", "negative_exponents"):
                values = cfg[key]
                if not isinstance(values, list) or any(type(v) is not int or v < (1 if key == "negative_exponents" else 0) or v > 8 for v in values):
                    config_errors.append(f"invalid grammar restriction: {key}")
            real_exponents = cfg["real_exponents"]
            if not isinstance(real_exponents, list) or any(not math.isfinite(float(v)) or abs(float(v)) > float(cfg["max_abs_constant"]) for v in real_exponents):
                config_errors.append("invalid grammar restriction: real_exponents")
            probability = float(cfg["continuous_probability"])
            bounds = [float(v) for v in cfg["continuous_bounds"]]
            if not math.isfinite(probability) or not 0 <= probability <= 1 or len(bounds) != 2 or not all(map(math.isfinite, bounds)) or bounds[0] > bounds[1]:
                config_errors.append("invalid continuous-terminal grammar configuration")
            if len(bounds) == 2 and max(abs(bounds[0]), abs(bounds[1])) > float(cfg["max_abs_constant"]):
                config_errors.append("continuous-terminal bounds exceed the expression constant limit")
            if type(cfg["power_exponents_are_constants"]) is not bool:
                config_errors.append("invalid power_exponents_are_constants setting")
            if "pow" in cfg["operators"] and cfg["power_exponents_are_constants"] and not real_exponents:
                config_errors.append("constant real-power grammar requires an exponent initialization source")
        except (KeyError, TypeError, ValueError):
            config_errors.append("missing or invalid exponent grammar configuration")
        identity_fields = {
            "operators": cfg["operators"], "variables": cfg["variables"],
            "max_nodes": cfg["max_nodes"], "max_depth": cfg["max_depth"],
            "max_arithmetic_operations": cfg["max_arithmetic_operations"],
            "max_abs_constant": cfg["max_abs_constant"],
            "fit_parameter_bounds": cfg["fit_parameter_bounds"],
            "constant_terminals": cfg["constant_terminals"],
            "initial_max_depth": cfg["initial_max_depth"],
            "mutation_max_depth": cfg["mutation_max_depth"],
            "proposal_limit": cfg["proposal_limit"],
            "integer_exponents": cfg["integer_exponents"],
            "negative_exponents": cfg["negative_exponents"],
            "real_exponents": cfg["real_exponents"],
            "continuous_probability": cfg["continuous_probability"],
            "continuous_bounds": cfg["continuous_bounds"],
            "power_exponents_are_constants": cfg["power_exponents_are_constants"],
        }
        grammar_id = hashlib.sha256(json.dumps(identity_fields, sort_keys=True,
                                                separators=(",", ":")).encode()).hexdigest()
        common = [args.validator, "--reference", task["reference"],
                  "--variables", str(cfg["variables"]), "--operators", ",".join(cfg["operators"]),
                  "--max-nodes", str(cfg["max_nodes"]), "--max-depth", str(cfg["max_depth"]),
                  "--max-operations", str(cfg["max_arithmetic_operations"]),
                  "--max-abs-constant", str(cfg["max_abs_constant"]),
                  "--fit-lower", str(cfg["fit_parameter_bounds"][0]),
                  "--fit-upper", str(cfg["fit_parameter_bounds"][1]),
                  "--integer-exponents", ",".join(map(str, cfg["integer_exponents"])),
                  "--negative-exponents", ",".join(map(str, cfg["negative_exponents"])),
                  "--power-exponents-are-constants", str(cfg["power_exponents_are_constants"]).lower()]
        splits = task.get("domain_observations", []) or [None]
        checks = []
        if config_errors:
            failed = True
            checks.append({"dataset": None, "valid": False, "error": "; ".join(config_errors)})
        for rel in ([] if config_errors else splits):
            command = common.copy()
            if rel is not None:
                command += ["--data", str((base / rel).resolve())]
            run = subprocess.run(command, text=True, capture_output=True)
            if run.returncode:
                failed = True
                checks.append({"dataset": rel, "valid": False, "error": run.stderr.strip()})
            else:
                checks.append({"dataset": rel, "valid": True, "audit": json.loads(run.stdout)})
        results.append({"task": task["id"], "reference": task["reference"],
                        "grammar": identity_fields, "grammar_identity_sha256": grammar_id,
                        "resources": resources,
                        "fitting": task.get("fitting", {}),
                        "parameterization": task.get("parameterization", "each reference literal is an independent bounded constant slot"),
                        "status": "representable" if all(c["valid"] for c in checks) else "rejected",
                        "checks": checks})
    output = {"schema": "genetik-benchmark-preflight-v1", "manifest": str(manifest_path),
              "manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
              "status": "rejected" if failed else "valid", "tasks": results}
    rendered = json.dumps(output, indent=2, sort_keys=True) + "\n"
    pathlib.Path(args.output).write_text(rendered) if args.output else sys.stdout.write(rendered)
    return 2 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
