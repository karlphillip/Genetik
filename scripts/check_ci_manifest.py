#!/usr/bin/env python3
"""Fail CI if intended CTest coverage was silently omitted (CTest JSON >=3.14)."""
import argparse
import json
from pathlib import Path

PORTABLE = {
    "genetik_checkpoint_compatibility",
    "genetik_discrete_contracts", "genetik_discrete_overlap", "genetik_discrete_quickstart",
    "genetik_discrete_iteration_contracts", "genetik_discrete_iteration_overlap", "genetik_discrete_iteration_quickstart",
    "genetik_discrete_prime_contracts", "genetik_discrete_prime_overlap", "genetik_discrete_prime_cli",
    "genetik_checkpoint_contracts","genetik_checkpoint_overlap","genetik_checkpoint_process","genetik_checkpoint_cli","genetik_checkpoint_quickstart","genetik_formatting_contracts",
    'genetik_prime_exact_contracts','genetik_prime_exact_overlap','genetik_prime_exact_cli',
    'genetik_integral_contracts', 'genetik_integral_overlap', 'genetik_integrals_quickstart', 'genetik_integral_cli',
    'genetik_derivative_regression_contracts','genetik_derivative_regression_overlap','genetik_derivative_regression_quickstart','genetik_derivative_regression_cli',
    'genetik_derivative_contracts', 'genetik_derivative_overlap', 'genetik_derivatives_quickstart',
    'genetik_template_contracts', 'genetik_template_overlap', 'genetik_templates_quickstart', 'genetik_template_campaign_smoke',
    'genetik_parsing_contracts', 'genetik_parsing_overlap', 'genetik_parsing_quickstart', 'genetik_expression_file',
    'genetik_scientific_contracts',
    'genetik_repository_hygiene', 'genetik_typed_regression_contracts', 'genetik_typed_preparation_overlap', 'genetik_typed_search_quickstart', 'genetik_typed_regression_cli', 'genetik_typed_contracts', 'genetik_typed_overlap', 'genetik_typed_example',
    'genetik_strict_arithmetic', 'genetik_multivariate_contracts',
    'genetik_fitting_contracts', 'genetik_symbolic_contracts',
    'genetik_expression_contracts', 'genetik_fitness_publication',
    'genetik_contracts', 'genetik_regression_cli', 'genetik_expression_example',
    'genetik_demo_bounded', 'genetik_demo_zero_budget', 'genetik_demo_invalid_argument',
    'genetik_downstream', 'genetik_expression_fp_policy', 'genetik_prime_contracts',
    'genetik_prime_cli', 'genetik_workspace_contracts', 'genetik_installed_package',
    'genetik_scalar_quickstart', 'genetik_symbolic_quickstart', 'genetik_oop_contracts',
    'genetik_concurrency_contracts', 'genetik_oop_parallel', 'genetik_character_parallel',
}
TSAN = {'genetik_discrete_overlap','genetik_discrete_iteration_overlap','genetik_discrete_prime_overlap','genetik_checkpoint_overlap','genetik_prime_exact_overlap','genetik_integral_overlap','genetik_derivative_regression_overlap','genetik_derivative_overlap', 'genetik_template_overlap', 'genetik_parsing_overlap', 'genetik_scientific_contracts', 'genetik_typed_preparation_overlap', 'genetik_concurrency_contracts', 'genetik_oop_parallel', 'genetik_typed_overlap'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--tsan', action='store_true')
    args = parser.parse_args()
    tests = json.loads(args.manifest.read_text())['tests']
    names = [test['name'] for test in tests]
    expected = TSAN if args.tsan else PORTABLE
    if len(names) != len(set(names)) or set(names) != expected:
        raise SystemExit(f'Unexpected CTest manifest: missing={sorted(expected-set(names))}, '
                         f'extra={sorted(set(names)-expected)}, count={len(names)}')
    print(f'Qualified test manifest ({len(names)}): ' + ', '.join(sorted(names)))


if __name__ == '__main__':
    main()
