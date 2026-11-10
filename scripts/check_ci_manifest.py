#!/usr/bin/env python3
"""Fail CI if intended CTest coverage was silently omitted (CTest JSON >=3.14)."""
import argparse
import json
from pathlib import Path

PORTABLE = {
    "genetik_research_execution_contracts",
    "genetik_research_paths",
    "genetik_d6_budget_cli", "genetik_d6_scientific_contracts", "genetik_d6_continuation", "genetik_d6_overlap", "genetik_scientific_discovery_quickstart",
    'genetik_d5_iteration_contracts', 'genetik_d5_iteration_overlap', 'genetik_d5_integer_contracts', 'genetik_d5_recurrence_contracts', 'genetik_d5_recurrence_overlap', 'genetik_d5_scientific_contracts', 'genetik_d5_scientific_overlap', 'genetik_d5_symbolic_contracts', 'genetik_d5_symbolic_overlap', 'genetik_d5_dimension_contracts', 'genetik_d5_dimension_overlap', 'genetik_advanced_mathematics_quickstart',
    "genetik_checkpoint_compatibility",
    "genetik_discrete_contracts", "genetik_discrete_overlap", "genetik_discrete_quickstart",
    "genetik_discrete_iteration_contracts", "genetik_discrete_iteration_overlap", "genetik_discrete_iteration_quickstart",
    "genetik_discrete_prime_contracts", "genetik_discrete_prime_overlap", "genetik_discrete_prime_cli",
    "genetik_checkpoint_contracts","genetik_checkpoint_overlap","genetik_checkpoint_process","genetik_checkpoint_cli","genetik_checkpoint_quickstart","genetik_formatting_contracts",
    "genetik_checkpoint_v04_compatibility", "genetik_discrete_checkpoint_v04_compatibility",
    "genetik_checkpoint_v05_compatibility", "genetik_discrete_checkpoint_v05_compatibility",
    'genetik_prime_exact_contracts','genetik_prime_exact_overlap','genetik_prime_exact_cli',
    'genetik_integral_contracts', 'genetik_integral_overlap', 'genetik_integrals_quickstart', 'genetik_integral_cli',
    'genetik_rational_regression_quickstart',
    'genetik_multivariate_rational_contracts',
    'genetik_multivariate_rational_domain_contracts',
    'genetik_multivariate_rational_quickstart',
    'genetik_rational_function_contracts',
    'genetik_first_search_quickstart',
    'genetik_derivative_regression_contracts','genetik_derivative_regression_overlap','genetik_derivative_regression_quickstart','genetik_derivative_regression_cli',
    'genetik_derivative_contracts', 'genetik_derivative_overlap', 'genetik_derivatives_quickstart',
    'genetik_template_contracts', 'genetik_template_overlap', 'genetik_templates_quickstart', 'genetik_template_campaign_smoke',
    'genetik_parsing_contracts', 'genetik_parsing_overlap', 'genetik_parsing_quickstart', 'genetik_expression_file',
    'genetik_scientific_contracts',
    'genetik_scientific_benchmark_preflight_contracts',
    'genetik_advanced_regression_contracts', 'genetik_mixed_expression_contracts',
    'genetik_mixed_expression_overlap', 'genetik_mixed_multitu_contracts',
    'genetik_checkpoint_v06_compatibility', 'genetik_discrete_checkpoint_v06_compatibility',
    'genetik_equation_archive_contracts',
    'genetik_smooth_operator_contracts', 'genetik_smooth_operators_quickstart',
    'genetik_repository_hygiene', 'genetik_typed_regression_contracts', 'genetik_typed_preparation_overlap', 'genetik_typed_search_quickstart', 'genetik_typed_regression_cli', 'genetik_typed_contracts', 'genetik_typed_overlap', 'genetik_typed_example',
    'genetik_strict_arithmetic', 'genetik_multivariate_contracts',
    'genetik_fitting_contracts', 'genetik_symbolic_contracts',
    'genetik_objective_regression_contracts', 'genetik_objectives_quickstart',
    'genetik_symbolic_selection_contracts', 'genetik_selection_strategies_quickstart',
    'genetik_multi_component_template_contracts', 'genetik_multi_component_templates_quickstart',
    'genetik_automatic_separable_contracts', 'genetik_automatic_separable_quickstart',
    'genetik_expression_contracts', 'genetik_fitness_publication',
    'genetik_contracts', 'genetik_regression_cli', 'genetik_expression_example',
    'genetik_demo_bounded', 'genetik_demo_zero_budget', 'genetik_demo_invalid_argument',
    'genetik_constructive_recurrence_contracts', 'genetik_constructive_recurrence_example',
    'genetik_downstream', 'genetik_expression_fp_policy', 'genetik_prime_contracts',
    'genetik_prime_cli', 'genetik_workspace_contracts', 'genetik_installed_package',
    'genetik_scalar_quickstart', 'genetik_symbolic_quickstart', 'genetik_oop_contracts',
    'genetik_concurrency_contracts', 'genetik_oop_parallel', 'genetik_character_parallel',
}
TSAN = {'genetik_d6_overlap','genetik_d5_iteration_overlap', 'genetik_d5_recurrence_overlap', 'genetik_d5_scientific_overlap', 'genetik_d5_symbolic_overlap', 'genetik_d5_dimension_overlap','genetik_discrete_overlap','genetik_discrete_iteration_overlap','genetik_discrete_prime_overlap','genetik_checkpoint_overlap','genetik_prime_exact_overlap','genetik_integral_overlap','genetik_derivative_regression_overlap','genetik_derivative_overlap', 'genetik_template_overlap', 'genetik_parsing_overlap', 'genetik_scientific_contracts', 'genetik_typed_preparation_overlap', 'genetik_concurrency_contracts', 'genetik_oop_parallel', 'genetik_typed_overlap'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--tsan', action='store_true')
    parser.add_argument('--optional-numerics',action='store_true')
    args = parser.parse_args()
    tests = json.loads(args.manifest.read_text())['tests']
    names = [test['name'] for test in tests]
    expected = set(TSAN if args.tsan else PORTABLE)
    if args.optional_numerics and not args.tsan:expected.add("genetik_d6_optional_package")
    if len(names) != len(set(names)) or set(names) != expected:
        raise SystemExit(f'Unexpected CTest manifest: missing={sorted(expected-set(names))}, '
                         f'extra={sorted(set(names)-expected)}, count={len(names)}')
    print(f'Qualified test manifest ({len(names)}): ' + ', '.join(sorted(names)))


if __name__ == '__main__':
    main()
