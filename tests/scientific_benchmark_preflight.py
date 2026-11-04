import json
import pathlib
import subprocess
import sys
import tempfile

binary, source = map(pathlib.Path, sys.argv[1:3])
source = source.resolve()
preflight = source / "scripts/scientific_benchmark_preflight.py"
runner = source / "scripts/run_validated_campaign.py"

grammar = {
    "operators": ["+", "*"], "variables": 1, "max_nodes": 5, "max_depth": 3,
    "max_arithmetic_operations": 4, "max_abs_constant": 8,
    "fit_parameter_bounds": [-8, 8], "constant_terminals": [0.5],
    "initial_max_depth": 3, "mutation_max_depth": 3, "proposal_limit": 8,
    "integer_exponents": list(range(9)), "negative_exponents": list(range(1, 9)),
    "real_exponents": [0.5, 1, 2], "continuous_probability": 0,
    "continuous_bounds": [-8, 8], "power_exponents_are_constants": True,
}
fitting = {"method": "LevenbergMarquardt", "max_loss_attempts": 16,
           "max_iterations": 16, "max_jacobian_evaluations": 16,
           "max_mathematical_operations": 200000000, "max_workspace_bytes": 67108864,
           "lower": -8, "upper": 8}
resources = {"population_size": 32, "candidate_evaluations": 4096,
             "training_loss_attempts": 69632, "generation_attempts": 127,
             "worker_count": 1, "cpu_affinity": [0], "wall_seconds": 120,
             "process_tree_rss_cap_bytes": 1073741824,
             "virtual_address_space_cap_bytes": 2147483648}

with tempfile.TemporaryDirectory(prefix="genetik-benchmark-preflight-") as temp:
    root = pathlib.Path(temp)
    manifest = root / "manifest.json"
    output = root / "preflight.json"
    marker = root / "runner-started"
    observations = root / "observations.csv"
    observations.write_text("x0,y\n0,1\n1,2\n")
    fake_runner = root / "runner.py"
    fake_runner.write_text(
        "import hashlib, sys\nfrom pathlib import Path\n"
        "if len(sys.argv) == 3 and sys.argv[1] == '--genetik-protocol-identity':\n"
        " print(hashlib.sha256(Path(sys.argv[2]).read_bytes()).hexdigest()); raise SystemExit(0)\n"
        "Path(" + repr(str(marker)) + ").touch()\n")
    subprocess.run([str(binary), "--self-test", str(root / "domain-fixture.csv")], check=True)

    manifest.write_text(json.dumps({"schema": "fixture", "tasks": [
        {"id": "valid", "reference": "(x0*x0)+1", "grammar": grammar,
         "fitting": fitting, "resources": resources,
         "domain_observations": [observations.name]}
    ]}))
    good = subprocess.run([sys.executable, str(runner), "--validator", str(binary),
                           "--manifest", str(manifest), "--preflight", str(preflight),
                           "--preflight-output", str(output), "--runner", str(fake_runner)])
    assert good.returncode == 0 and marker.exists()
    assert json.loads(output.read_text())["tasks"][0]["status"] == "domain_validated"
    assert json.loads(output.read_text())["status"] == "domain_validated"

    # The runner receives a preflight gate only after nonempty task and domain
    # validation; optional identity mode verifies a cooperating runner's view.
    marker.unlink()
    identified = subprocess.run([sys.executable, str(runner), "--validator", str(binary),
                                 "--manifest", str(manifest), "--preflight", str(preflight),
                                 "--preflight-output", str(output),
                                 "--require-runner-protocol-identity", "--runner", str(fake_runner)])
    assert identified.returncode == 0 and marker.exists()
    marker.unlink()
    mismatching_runner = root / "mismatching_runner.py"
    mismatching_runner.write_text(
        "import sys\nfrom pathlib import Path\n"
        "if len(sys.argv) == 3 and sys.argv[1] == '--genetik-protocol-identity':\n"
        " print('0' * 64); raise SystemExit(0)\n"
        "Path(" + repr(str(marker)) + ").touch()\n")
    mismatch = subprocess.run([sys.executable, str(runner), "--validator", str(binary),
                               "--manifest", str(manifest), "--preflight", str(preflight),
                               "--preflight-output", str(output),
                               "--require-runner-protocol-identity", "--runner",
                               str(mismatching_runner)], stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
    assert mismatch.returncode != 0 and not marker.exists()

    invalid_grammar = dict(grammar, max_nodes=10, max_depth=5,
                           max_arithmetic_operations=9,
                           operators=["+", "*", "sin"])
    invalid_fitting = dict(fitting, max_jacobian_evaluations=0)
    invalid_power_grammar = dict(grammar, operators=["powi"], integer_exponents=[1, 2])
    invalid_resources = dict(resources, worker_count=0)
    manifest.write_text(json.dumps({"schema": "fixture", "tasks": [
        {"id": "out-of-cap", "reference": "sin(((0.8*x0)+0.25))+(0.1*x0*x0)",
         "grammar": invalid_grammar, "fitting": fitting, "resources": resources},
        {"id": "bad-fit-budget", "reference": "(x0*x0)+1", "grammar": grammar,
         "fitting": invalid_fitting},
        {"id": "bad-power-grammar", "reference": "powi(x0,3)",
         "grammar": invalid_power_grammar, "fitting": fitting, "resources": resources},
        {"id": "bad-resources", "reference": "(x0*x0)+1", "grammar": grammar,
         "fitting": fitting, "resources": invalid_resources}
    ]}))
    bad = subprocess.run([sys.executable, str(runner), "--validator", str(binary),
                          "--manifest", str(manifest), "--preflight", str(preflight),
                          "--preflight-output", str(output), "--runner", str(fake_runner)],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    assert bad.returncode != 0 and not marker.exists()
    rejected = json.loads(output.read_text())
    assert rejected["tasks"][0]["status"] == "rejected"
    assert rejected["tasks"][1]["status"] == "rejected"
    assert rejected["tasks"][2]["status"] == "rejected"
    assert rejected["tasks"][3]["status"] == "rejected"

    # Configuration-only errors must fail the campaign gate without reaching
    # the C++ parser or starting the campaign runner.
    manifest.write_text(json.dumps({"schema": "fixture", "tasks": [
        {"id": "invalid-resource-only", "reference": "(x0*x0)+1",
         "grammar": grammar, "fitting": fitting,
         "resources": dict(resources, candidate_evaluations=0)},
        {"id": "fitting-bound-exceeds-expression-cap", "reference": "(x0*x0)+1",
         "grammar": dict(grammar, fit_parameter_bounds=[-9, 9]),
         "fitting": dict(fitting, lower=-9, upper=9),
         "resources": resources},
        {"id": "constant-terminal-exceeds-expression-cap", "reference": "(x0*x0)+1",
         "grammar": dict(grammar, constant_terminals=[9]), "fitting": fitting,
         "resources": resources},
        {"id": "initial-depth-exceeds-limit", "reference": "(x0*x0)+1",
         "grammar": dict(grammar, initial_max_depth=4), "fitting": fitting,
         "resources": resources}
    ]}))
    bad_config = subprocess.run([sys.executable, str(runner), "--validator", str(binary),
                                 "--manifest", str(manifest), "--preflight", str(preflight),
                                 "--preflight-output", str(output), "--runner", str(fake_runner)],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    assert bad_config.returncode != 0 and not marker.exists()
    rejected_config = json.loads(output.read_text())
    assert rejected_config["status"] == "rejected"
    assert all(task["status"] == "rejected" for task in rejected_config["tasks"])

    # Empty task arrays and missing/empty/duplicate IDs fail before execution.
    for tasks in ([],
                  [{"reference": "x0", "grammar": grammar, "fitting": fitting,
                    "resources": resources, "domain_observations": [observations.name]}],
                  [{"id": "  ", "reference": "x0", "grammar": grammar, "fitting": fitting,
                    "resources": resources, "domain_observations": [observations.name]}],
                  [{"id": "same", "reference": "x0", "grammar": grammar, "fitting": fitting,
                    "resources": resources, "domain_observations": [observations.name]},
                   {"id": "same", "reference": "x0", "grammar": grammar, "fitting": fitting,
                    "resources": resources, "domain_observations": [observations.name]}]):
        manifest.write_text(json.dumps({"schema": "fixture", "tasks": tasks}))
        failed = subprocess.run([sys.executable, str(runner), "--validator", str(binary),
                                 "--manifest", str(manifest), "--preflight", str(preflight),
                                 "--preflight-output", str(output), "--runner", str(fake_runner)],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        assert failed.returncode != 0 and not marker.exists()

    # The vocabulary itself is checked, including operators unused by the reference.
    for operators in (["+", "not_an_operator"], ["+", "+"]):
        manifest.write_text(json.dumps({"schema": "fixture", "tasks": [
            {"id": "bad-vocabulary", "reference": "x0+1",
             "grammar": dict(grammar, operators=operators), "fitting": fitting,
             "resources": resources, "domain_observations": [observations.name]}
        ]}))
        failed = subprocess.run([sys.executable, str(runner), "--validator", str(binary),
                                 "--manifest", str(manifest), "--preflight", str(preflight),
                                 "--preflight-output", str(output), "--runner", str(fake_runner)],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        assert failed.returncode != 0 and not marker.exists()
    duplicate_cli = subprocess.run([str(binary), "--reference", "x0", "--variables", "1",
                                    "--operators", "+,+", "--max-nodes", "2",
                                    "--max-depth", "1", "--max-operations", "1",
                                    "--max-abs-constant", "8", "--fit-lower", "-8",
                                    "--fit-upper", "8"])
    assert duplicate_cli.returncode != 0

    # Empty observations deliberately yield syntax-only qualification and may
    # never be promoted by the campaign wrapper to domain-validated readiness.
    manifest.write_text(json.dumps({"schema": "fixture", "tasks": [
        {"id": "syntax-only", "reference": "x0+1", "grammar": grammar,
         "fitting": fitting, "resources": resources}
    ]}))
    syntax = subprocess.run([sys.executable, str(preflight), "--validator", str(binary),
                             "--manifest", str(manifest), "--output", str(output)])
    assert syntax.returncode == 0 and json.loads(output.read_text())["status"] == "syntax_only"
    blocked = subprocess.run([sys.executable, str(runner), "--validator", str(binary),
                              "--manifest", str(manifest), "--preflight", str(preflight),
                              "--preflight-output", str(output), "--runner", str(fake_runner)],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    assert blocked.returncode != 0 and not marker.exists()
print("scientific benchmark preflight isolation/gate tests passed")
