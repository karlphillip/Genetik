#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
format_tool="${CLANG_FORMAT:-clang-format-18}"
format_diff_tool="${CLANG_FORMAT_DIFF:-clang-format-diff-18}"
tidy_tool="${CLANG_TIDY:-clang-tidy-18}"
compile_db="${1:-$repo_root/build/quality}"

command -v "$format_tool" >/dev/null 2>&1 || {
    echo "required formatter not found: $format_tool" >&2
    exit 2
}
command -v "$tidy_tool" >/dev/null 2>&1 || {
    echo "required static analyzer not found: $tidy_tool" >&2
    exit 2
}
command -v "$format_diff_tool" >/dev/null 2>&1 || {
    echo "required changed-lines formatter not found: $format_diff_tool" >&2
    exit 2
}

format_version="$("$format_tool" --version)"
tidy_version="$("$tidy_tool" --version)"
[[ "$format_version" == *"version 18."* ]] || {
    echo "clang-format 18 is required; found: $format_version" >&2
    exit 2
}
[[ "$tidy_version" == *"version 18."* ]] || {
    echo "clang-tidy 18 is required; found: $tidy_version" >&2
    exit 2
}
[[ -f "$compile_db/compile_commands.json" ]] || {
    echo "compile database not found: $compile_db/compile_commands.json" >&2
    exit 2
}

cd "$repo_root"
format_diff="$(git diff --no-color --unified=0 origin/main -- \
    genetik/Expression.hpp \
    genetik/ExpressionDerivative.hpp \
    genetik/ExpressionSecondDerivative.hpp \
    genetik/PhysicalDimensions.hpp \
    genetik/Regression.hpp \
    genetik/InformationCriteria.hpp \
    genetik/ConstantFitting.hpp \
    genetik/SymbolicRegression.hpp \
    genetik/SymbolicAlgebra.hpp \
    genetik/TypedExpression.hpp \
    genetik/TypedExpressionParsing.hpp \
    genetik/TypedVariation.hpp \
    genetik/detail/ScalarOperators.hpp \
    genetik/detail/ExpressionParser.hpp \
    apps/regression_search.cpp \
    examples/quickstarts/smooth_operators.cpp \
    examples/quickstarts/objectives.cpp \
    examples/quickstarts/selection_strategies.cpp \
    examples/quickstarts/automatic_separable.cpp \
    benchmarks/symbolic_selection_comparison.cpp \
    benchmarks/multi_component_template_comparison.cpp \
    genetik/SymbolicSelection.hpp \
    genetik/MultiComponentTemplateRegression.hpp \
    genetik/AutomaticSeparableRegression.hpp \
    benchmarks/automatic_separable_noise_followup.cpp \
    genetik/Population.hpp \
    tests/symbolic_selection.cpp \
    tests/multi_component_templates.cpp \
    tests/automatic_separable_regression.cpp \
    tests/mathematical_objectives.cpp \
    tests/smooth_operators.cpp \
    examples/quickstarts/first_search.cpp \
    tests/expressions.cpp \
    genetik/DiscreteExpressionSpecification.hpp \
    genetik/DiscreteExpressionComponents.hpp \
    examples/quickstarts/discrete_specification.cpp \
    examples/quickstarts/discrete_components.cpp \
    tests/discrete_specification.cpp \
    tests/discrete_expression_components.cpp \
    tests/research_process_fixture.cpp |
    "$format_diff_tool" -p1 -binary "$format_tool")"
if [[ -n "$format_diff" ]]; then
    printf '%s\n' "$format_diff" >&2
    echo "changed C++ lines do not match .clang-format" >&2
    exit 1
fi

gcc_include="$(c++ -print-file-name=include)"
gcc_multiarch="$(cc -print-multiarch)"
tidy_args=(
    -p "$compile_db"
    --warnings-as-errors='*'
    --extra-arg-before=-isystem
    "--extra-arg-before=$gcc_include"
)
if [[ -n "$gcc_multiarch" ]]; then
    tidy_args+=(--extra-arg-before=-isystem
                "--extra-arg-before=/usr/include/$gcc_multiarch")
fi

"$tidy_tool" "${tidy_args[@]}" \
    tests/expressions.cpp \
    tests/advanced_regression.cpp \
    tests/smooth_operators.cpp \
    tests/mathematical_objectives.cpp \
    tests/symbolic_selection.cpp \
    tests/multi_component_templates.cpp \
    tests/automatic_separable_regression.cpp \
    tests/discrete_specification.cpp \
    tests/discrete_expression_components.cpp \
    tests/research_process_fixture.cpp \
    benchmarks/automatic_separable_comparison.cpp \
    benchmarks/automatic_separable_noise_followup.cpp \
    benchmarks/symbolic_selection_comparison.cpp
