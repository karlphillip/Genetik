#!/usr/bin/env python3
"""Generate the frozen S14 reference datasets; not used by fitters or scorers."""
import csv
import hashlib
import json
import math
import pathlib
import random
import sys

ROOT = pathlib.Path(sys.argv[1]).resolve()
SOURCE_SHA = sys.argv[2]
DATA = ROOT / "datasets"
DATA.mkdir(parents=True, exist_ok=True)

TASKS = {
    "rational_mixed_xy": {
        "reference": "(1+(0.4*x0)-(0.3*x1)+(0.2*(x0*x1)))/(1+(0.1*x0)+(0.15*x1))",
        "fn": lambda x, y: (1 + .4*x - .3*y + .2*x*y) / (1 + .1*x + .15*y),
    },
    "rational_denominator_xy": {
        "reference": "(0.5+x0+x1)/(1+(0.12*(x0*x1))+(0.08*(x0*x0)))",
        "fn": lambda x, y: (.5+x+y)/(1+.12*x*y+.08*x*x),
    },
    "rational_quadratic": {
        "reference": "(1+(0.2*x0)-(0.4*x1)+(0.1*(x0*x0))+(0.2*(x0*x1))-(0.15*(x1*x1)))/(1+(0.08*x0)-(0.05*x1)+(0.03*(x0*x1)))",
        "fn": lambda x, y: (1+.2*x-.4*y+.1*x*x+.2*x*y-.15*y*y)/(1+.08*x-.05*y+.03*x*y),
    },
    "rational_near_pole": {
        "reference": "(1+(0.3*x0)-(0.2*x1))/(1-(0.7*x0)-(0.5*x1))",
        "fn": lambda x, y: (1+.3*x-.2*y)/(1-.7*x-.5*y),
        "train_values": [-.4, -.2, 0, .2, .4],
    },
    "polynomial_control": {
        "reference": "1+(2*x0)-(0.5*x1)+(0.3*(x0*x0))+(0.4*(x0*x1))-(0.2*(x1*x1))",
        "fn": lambda x, y: 1+2*x-.5*y+.3*x*x+.4*x*y-.2*y*y,
    },
    "sine_control": {
        "reference": "sin((1.1*x0)-(0.4*x1))+(0.1*x0)",
        "fn": lambda x, y: math.sin(1.1*x-.4*y)+.1*x,
    },
}

def write_csv(path, rows):
    with path.open("w", newline="") as stream:
        out = csv.writer(stream, lineterminator="\n")
        out.writerow(["x0", "x1", "y"])
        for x, y, z in rows:
            out.writerow([format(x, ".17g"), format(y, ".17g"), format(z, ".17g")])

def track_rows(fn, values, seed=None):
    clean = [(x, y, fn(x, y)) for x in values for y in values]
    if seed is None:
        return clean
    rms = math.sqrt(sum(z*z for _, _, z in clean) / len(clean))
    rng = random.Random(seed)
    return [(x, y, z + rng.gauss(0, .01*rms)) for x, y, z in clean]

training_values = [-.75, -.5, -.25, 0, .25, .5, .75]
assessment_values = [-.68, -.39, -.11, .17, .44, .71]
extrap_values = [-1.0, -.7, -.4, .2, .55, .9, 1.0]
noise_seeds = [14011, 14017, 14023]
tasks = []
dataset_hashes = {}
for family, item in TASKS.items():
    train_axis = item.get("train_values", training_values)
    for track, noise_seed in [("clean", None)] + [(f"noise_{seed}", seed) for seed in noise_seeds]:
        stem = f"{family}__{track}"
        train_path = DATA / f"{stem}__training.csv"
        held_path = DATA / f"{stem}__heldout.csv"
        extrap_path = DATA / f"{stem}__extrapolation.csv"
        write_csv(train_path, track_rows(item["fn"], train_axis, noise_seed))
        write_csv(held_path, track_rows(item["fn"], assessment_values))
        # Safe extrapolation extends the training domain but stays away from a
        # denominator root. The near-pole task has its own finite stress rows.
        write_csv(extrap_path, track_rows(item["fn"], extrap_values))
        for p in (train_path, held_path, extrap_path):
            dataset_hashes[str(p.relative_to(ROOT))] = hashlib.sha256(p.read_bytes()).hexdigest()
        grammar = {
            "variables": 2, "operators": ["+", "-", "*", "/", "powi", "sin"],
            "max_nodes": 127, "max_depth": 16, "max_arithmetic_operations": 2048,
            "max_abs_constant": 8.0, "fit_parameter_bounds": [-8.0, 8.0],
            "constant_terminals": [-2, -1, 0, 1, 2], "initial_max_depth": 4,
            "mutation_max_depth": 3, "proposal_limit": 8,
            "integer_exponents": [0, 1, 2], "negative_exponents": [1, 2],
            "real_exponents": [.5, 1, 2], "continuous_probability": .5,
            "continuous_bounds": [-3, 3], "power_exponents_are_constants": True,
        }
        tasks.append({
            "id": stem, "reference": item["reference"], "grammar": grammar,
            "parameterization": "Reference literals are only preflight/scoring data; no equation or coefficient is provided to fitting or evolution.",
            "domain_observations": [str(train_path.relative_to(ROOT)), str(held_path.relative_to(ROOT)), str(extrap_path.relative_to(ROOT))],
            "fitting": {"method": "LevenbergMarquardt", "lower": -8, "upper": 8,
                        "max_loss_attempts": 12, "max_iterations": 8,
                        "max_jacobian_evaluations": 8, "max_mathematical_operations": 1000000,
                        "max_workspace_bytes": 4194304},
            "resources": {"population_size": 32, "candidate_evaluations": 512,
                          "training_loss_attempts": 12000, "generation_attempts": 16,
                          "worker_count": 1, "wall_seconds": 120,
                          "process_tree_rss_cap_bytes": 6442450944,
                          "virtual_address_space_cap_bytes": 6442450944,
                          "cpu_affinity": [0]},
            "family": family, "track": track, "noise_seed": noise_seed,
            "dataset_sha256": {"training": dataset_hashes[str(train_path.relative_to(ROOT))],
                               "heldout": dataset_hashes[str(held_path.relative_to(ROOT))],
                               "extrapolation": dataset_hashes[str(extrap_path.relative_to(ROOT))]},
        })

manifest = {
    "schema": "s14-multivariate-rational-preflight-v1",
    "campaign": {
        "protocol_id": "s14-multivariate-rational-confirmation-v1",
        "source_sha": SOURCE_SHA,
        "algorithmic_conditions": ["ga_coordinate", "ga_lm", "constructive_qr", "constructive_lm"],
        "ga_seeds": [14101, 14103, 14107],
        "constructive_runs_per_training_dataset": 2,
        "population_size": 32, "candidate_evaluation_limit": 512,
        "global_training_loss_attempt_limit": 12000, "generation_attempt_limit": 16,
        "worker_count": 1, "cpu_affinity": [0], "wall_seconds_per_run": 120,
        "process_tree_rss_cap_bytes": 6442450944,
        "virtual_address_space_cap_bytes": 6442450944,
        "ga_constant_fit": {"lower": -8, "upper": 8, "local_loss_attempts": 12,
                             "max_sweeps": 6, "max_proposals": 128,
                             "lm_iterations": 8, "lm_objective_evaluations": 12,
                             "lm_jacobian_evaluations": 8, "lm_math_operations": 1000000,
                             "lm_workspace_bytes": 4194304},
        "constructive": {"degree_maximum": 2, "maximum_parameters": 11,
                         "maximum_rows": 1000, "maximum_workspace_bytes": 16777216,
                         "maximum_arithmetic_operations": 50000000,
                         "maximum_linear_algebra_operations": 50000000,
                         "coefficient_bound": 8, "qr_lm_iterations": 16,
                         "lm_objective_evaluations": 32, "lm_jacobian_evaluations": 16},
        "noise": {"fraction_of_training_target_rms": 0.01,
                  "independent_python_random_seeds": noise_seeds,
                  "heldout_and_extrapolation_noise": 0},
        "domain_assessment": {"x_interval": [-1, 1], "y_interval": [-1, 1],
                              "maximum_cells": 1024, "maximum_depth": 8,
                              "minimum_denominator_separation": 1e-10},
        "advancement_criteria": {
            "clean_identifiable_rational_rmse": 1e-8,
            "noisy_rational_heldout_rmse_fraction_of_target_rms": 0.025,
            "noisy_rational_safe_extrapolation_rmse_fraction_of_target_rms": 0.05,
            "near_pole_stress_extrapolation_rmse_fraction_of_target_rms": 0.10,
            "max_invalid_points_in_declared_safe_prediction_regions": 0,
            "coordinate_lm_fixed_topology_training_loss_regression_fraction": 0.10,
            "nonrational_control_symbolic_recovery_claim": False,
        },
    },
    "tasks": tasks,
}
(ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
(ROOT / "dataset-checksums.json").write_text(json.dumps(dataset_hashes, indent=2, sort_keys=True) + "\n")
print(json.dumps({"tasks": len(tasks), "datasets": len(dataset_hashes), "noise_seeds": noise_seeds}, sort_keys=True))
