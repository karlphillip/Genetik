add_executable(genetik_arithmetic_tests strict_arithmetic.cpp)
target_link_libraries(genetik_arithmetic_tests PRIVATE genetik_lib)
add_test(NAME genetik_strict_arithmetic COMMAND genetik_arithmetic_tests)
set_tests_properties(genetik_strict_arithmetic PROPERTIES TIMEOUT 60)
add_executable(genetik_multivariate_tests multivariate.cpp)
target_link_libraries(genetik_multivariate_tests PRIVATE genetik_lib)
add_test(NAME genetik_multivariate_contracts COMMAND genetik_multivariate_tests ${CMAKE_CURRENT_BINARY_DIR}/multivariate-data)
set_tests_properties(genetik_multivariate_contracts PROPERTIES TIMEOUT 60)
add_executable(genetik_fitting_tests constant_fitting.cpp)
target_link_libraries(genetik_fitting_tests PRIVATE genetik_lib)
add_test(NAME genetik_fitting_contracts COMMAND genetik_fitting_tests)
set_tests_properties(genetik_fitting_contracts PROPERTIES TIMEOUT 60)
add_executable(genetik_symbolic_tests symbolic_regression.cpp)
target_link_libraries(genetik_symbolic_tests PRIVATE genetik_lib)
add_test(NAME genetik_symbolic_contracts COMMAND genetik_symbolic_tests)
set_tests_properties(genetik_symbolic_contracts PROPERTIES TIMEOUT 60)
add_executable(genetik_expression_tests expressions.cpp)
target_link_libraries(genetik_expression_tests PRIVATE genetik_lib)
add_test(NAME genetik_expression_contracts COMMAND genetik_expression_tests)
# Includes 20,001-level skew-tree stress; GCC ASan on this Jetson needs ~41 seconds.
set_tests_properties(genetik_expression_contracts PROPERTIES TIMEOUT 60)
add_executable(genetik_generation_tests generation.cpp)
target_link_libraries(genetik_generation_tests PRIVATE genetik_lib)
add_test(NAME genetik_fitness_publication COMMAND genetik_generation_tests)
set_tests_properties(genetik_fitness_publication PROPERTIES TIMEOUT 30)
add_executable(genetik_tests tests.cpp)
target_link_libraries(genetik_tests PRIVATE genetik_lib)
add_test(NAME genetik_contracts COMMAND genetik_tests)
set_tests_properties(genetik_contracts PROPERTIES TIMEOUT 30)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_regression_cli COMMAND ${CMAKE_COMMAND}
        -DGENERATOR=$<TARGET_FILE:genetik_multivariate_demo>
        -DSEARCH=$<TARGET_FILE:genetik_regression_search>
        -DWORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/regression-cli
        -P ${CMAKE_CURRENT_SOURCE_DIR}/regression_cli.cmake)
    # Cadence CLI smoke uses 64/128 calls; full comparisons run outside CTest.
    set_tests_properties(genetik_regression_cli PROPERTIES TIMEOUT 60)
    add_test(NAME genetik_expression_example COMMAND genetik_expression_demo)
    set_tests_properties(genetik_expression_example PROPERTIES TIMEOUT 30
        PASS_REGULAR_EXPRESSION "Arithmetic invalidity at postfix node 3")
    add_test(NAME genetik_demo_bounded COMMAND genetik_demo 42 20)
    add_test(NAME genetik_demo_zero_budget COMMAND genetik_demo 42 0)
    set_tests_properties(genetik_demo_bounded PROPERTIES
        PASS_REGULAR_EXPRESSION "stopping_reason=generation_limit generations=20")
    set_tests_properties(genetik_demo_zero_budget PROPERTIES
        PASS_REGULAR_EXPRESSION "stopping_reason=generation_limit generations=0")
    add_test(NAME genetik_demo_invalid_argument COMMAND genetik_demo invalid)
    set_tests_properties(genetik_demo_invalid_argument PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(genetik_demo_bounded genetik_demo_zero_budget
                        genetik_demo_invalid_argument PROPERTIES TIMEOUT 30)
endif()
# Each public header must compile as the first and only include.
foreach(header DiscreteExpression DiscreteExpressionParsing DiscreteExpressionFormatting DiscreteRegression DiscreteVariation DiscreteSymbolicRegression DiscreteExpressionCheckpoint DiscreteRunCheckpoint IntegerRecurrence SymbolicAlgebra PhysicalDimensions DimensionalRegression MathematicalConstraints ScalarMathematicsRun Version RunCheckpoint ScalarExpressionCheckpoint ExpressionFormatting IntegralExpression IntegralRegression DerivativeRegression ExpressionDerivative ScalarExpressionTemplate ScalarTemplateRegression ExpressionParsing TypedExpressionParsing TypedConstantFitting TypedVariation TypedSymbolicRegression TypedRegression TypedValue TypedExpression Execution Creature_i Creature Population_i Population GeneticAlgorithm_i GeneticAlgorithm RandomContext Fitness Expression EvaluatedCandidate ScalarVariation Regression ExpressionVariation SymbolicRegression ConstantFitting)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/header_${header}.cpp "#include <${header}.hpp>\n")
    add_library(header_${header} OBJECT ${CMAKE_CURRENT_BINARY_DIR}/header_${header}.cpp)
    target_link_libraries(header_${header} PRIVATE genetik_lib)
endforeach()
add_test(NAME genetik_downstream COMMAND ${CMAKE_COMMAND}
    -DGENETIK_SOURCE=${PROJECT_SOURCE_DIR}
    -DDOWNSTREAM_BINARY=${CMAKE_CURRENT_BINARY_DIR}/downstream
    -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/downstream.cmake)
set_tests_properties(genetik_downstream PROPERTIES TIMEOUT 60)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_test(NAME genetik_expression_fp_policy COMMAND ${CMAKE_COMMAND}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER} -DGENETIK_SOURCE=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/fp_policy.cmake)
    set_tests_properties(genetik_expression_fp_policy PROPERTIES TIMEOUT 90)
endif()

# Application support headers also compile without accidental preceding includes.
foreach(header PrimeExactOptions PrimeExactSearch PrimeExactReport PrimeExactDisplay PrimeExactCheckpoint ScalarTemplateData ExpressionTextFile RegressionData RegressionWorkflow MultivariateData StrictArithmeticData ExecutionOptions)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/example_header_${header}.cpp "#include <${PROJECT_SOURCE_DIR}/examples/${header}.hpp>\n")
    add_library(example_header_${header} OBJECT ${CMAKE_CURRENT_BINARY_DIR}/example_header_${header}.cpp)
    target_link_libraries(example_header_${header} PRIVATE genetik_lib)
endforeach()

add_executable(genetik_prime_tests prime_regression.cpp)
target_link_libraries(genetik_prime_tests PRIVATE genetik_lib)
add_test(NAME genetik_prime_contracts COMMAND genetik_prime_tests ${CMAKE_CURRENT_BINARY_DIR}/prime-contract-data)
set_tests_properties(genetik_prime_contracts PROPERTIES TIMEOUT 60)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/example_header_PrimeRegression.cpp "#include <${PROJECT_SOURCE_DIR}/examples/PrimeRegression.hpp>\n")
add_library(example_header_PrimeRegression OBJECT ${CMAKE_CURRENT_BINARY_DIR}/example_header_PrimeRegression.cpp)
target_link_libraries(example_header_PrimeRegression PRIVATE genetik_lib)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_prime_cli COMMAND ${CMAKE_COMMAND}
        -DPRIME=$<TARGET_FILE:genetik_prime_regression>
        -DSEARCH=$<TARGET_FILE:genetik_regression_search>
        -DWORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/prime-cli
        -P ${CMAKE_CURRENT_SOURCE_DIR}/prime_cli.cmake)
    set_tests_properties(genetik_prime_cli PROPERTIES TIMEOUT 60)
endif()
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/header_ScopedTiming.cpp "#include <detail/ScopedTiming.hpp>\n")
add_library(header_ScopedTiming OBJECT ${CMAKE_CURRENT_BINARY_DIR}/header_ScopedTiming.cpp)
target_link_libraries(header_ScopedTiming PRIVATE genetik_lib)

add_executable(genetik_workspace_tests evaluation_workspace.cpp)
target_link_libraries(genetik_workspace_tests PRIVATE genetik_lib)
add_test(NAME genetik_workspace_contracts COMMAND genetik_workspace_tests)
set_tests_properties(genetik_workspace_contracts PROPERTIES TIMEOUT 30)
if(GENETIK_BUILD_EXAMPLES AND GENETIK_RECORDED_REFERENCE)
    add_test(NAME genetik_evaluation_reference COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=$<TARGET_FILE:genetik_evaluation_workspace>
        -DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/evaluation-reference.txt
        -DEXPECTED=${PROJECT_SOURCE_DIR}/docs/evaluation-results/accepted-reference.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/evaluation_reference.cmake)
    set_tests_properties(genetik_evaluation_reference PROPERTIES TIMEOUT 60)
endif()

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/example_header_EvaluationCorpus.cpp "#include <benchmarks/EvaluationCorpus.hpp>\n")
add_library(example_header_EvaluationCorpus OBJECT ${CMAKE_CURRENT_BINARY_DIR}/example_header_EvaluationCorpus.cpp)
target_include_directories(example_header_EvaluationCorpus PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(example_header_EvaluationCorpus PRIVATE genetik_lib)

# Python is development integration tooling only, never an installed dependency.
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
add_test(NAME genetik_installed_package COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/package_integration.py
    --source ${PROJECT_SOURCE_DIR}
    --workspace ${CMAKE_CURRENT_BINARY_DIR}/genetik-delivery-fixture
    --cmake ${CMAKE_COMMAND} --cxx ${CMAKE_CXX_COMPILER})
set_tests_properties(genetik_installed_package PROPERTIES TIMEOUT 600)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_scalar_quickstart COMMAND genetik_scalar_quickstart)
    add_test(NAME genetik_symbolic_quickstart COMMAND genetik_symbolic_quickstart)
    set_tests_properties(genetik_scalar_quickstart genetik_symbolic_quickstart PROPERTIES TIMEOUT 30)
endif()

add_executable(genetik_oop_consumer oop_consumer/main.cpp)
target_link_libraries(genetik_oop_consumer PRIVATE Genetik::Genetik)
add_test(NAME genetik_oop_contracts COMMAND genetik_oop_consumer)
set_tests_properties(genetik_oop_contracts PROPERTIES TIMEOUT 30)

add_executable(genetik_concurrency_tests concurrency.cpp)
target_link_libraries(genetik_concurrency_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_concurrency_contracts COMMAND genetik_concurrency_tests)
set_tests_properties(genetik_concurrency_contracts PROPERTIES TIMEOUT 90)
add_test(NAME genetik_oop_parallel COMMAND genetik_oop_consumer --threads 3)
set_tests_properties(genetik_oop_parallel PROPERTIES TIMEOUT 30)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_character_parallel COMMAND genetik_demo 42 3 --threads 3)
    set_tests_properties(genetik_character_parallel PROPERTIES TIMEOUT 30)
endif()

# Location-based hygiene; frozen data/evidence remains permitted.
add_test(NAME genetik_repository_hygiene COMMAND ${Python3_EXECUTABLE}
    ${PROJECT_SOURCE_DIR}/scripts/check_hygiene.py --source ${PROJECT_SOURCE_DIR} --self-test)
set_tests_properties(genetik_repository_hygiene PROPERTIES TIMEOUT 30)

add_executable(genetik_typed_tests typed.cpp)
target_link_libraries(genetik_typed_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_typed_contracts COMMAND genetik_typed_tests)
add_test(NAME genetik_typed_overlap COMMAND genetik_typed_tests --overlap)
set_tests_properties(genetik_typed_contracts genetik_typed_overlap PROPERTIES TIMEOUT 60)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_typed_example COMMAND genetik_typed_quickstart)
    set_tests_properties(genetik_typed_example PROPERTIES TIMEOUT 30)
endif()

add_executable(genetik_typed_regression_tests typed_regression.cpp)
target_link_libraries(genetik_typed_regression_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_typed_regression_contracts COMMAND genetik_typed_regression_tests)
# O0 + ASan stack-use-after-return bookkeeping is costly on native ARM64;
# keep all fixed numerical/operator fixtures and instrumentation, with a bounded allowance.
set_tests_properties(genetik_typed_regression_contracts PROPERTIES TIMEOUT 300)

add_test(NAME genetik_typed_preparation_overlap COMMAND genetik_typed_regression_tests --overlap)
set_tests_properties(genetik_typed_preparation_overlap PROPERTIES TIMEOUT 90)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_typed_search_quickstart COMMAND genetik_typed_search_quickstart --threads 2)
    set_tests_properties(genetik_typed_search_quickstart PROPERTIES TIMEOUT 60)
endif()
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_typed_regression_cli COMMAND ${CMAKE_COMMAND}
        -DSEARCH=$<TARGET_FILE:genetik_typed_regression> -DWORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/typed-cli
        -P ${CMAKE_CURRENT_SOURCE_DIR}/typed_cli.cmake)
    set_tests_properties(genetik_typed_regression_cli PROPERTIES TIMEOUT 180)
endif()

add_executable(genetik_scientific_tests scientific.cpp)
target_link_libraries(genetik_scientific_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_scientific_contracts COMMAND genetik_scientific_tests)
set_tests_properties(genetik_scientific_contracts PROPERTIES TIMEOUT 180)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/header_ScientificScalar.cpp "#include <detail/ScientificScalar.hpp>\n")
add_library(header_ScientificScalar OBJECT ${CMAKE_CURRENT_BINARY_DIR}/header_ScientificScalar.cpp)
target_link_libraries(header_ScientificScalar PRIVATE Genetik::Genetik)

add_executable(genetik_parsing_tests parsing.cpp)
target_link_libraries(genetik_parsing_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_parsing_contracts COMMAND genetik_parsing_tests)
add_test(NAME genetik_parsing_overlap COMMAND genetik_parsing_tests --overlap)
set_tests_properties(genetik_parsing_contracts genetik_parsing_overlap PROPERTIES TIMEOUT 60)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_parsing_quickstart COMMAND genetik_parsing_quickstart)
    add_test(NAME genetik_expression_file COMMAND ${CMAKE_COMMAND}
        -DEVALUATE=$<TARGET_FILE:genetik_regression_evaluate>
        -DWORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/expression-file
        -P ${CMAKE_CURRENT_SOURCE_DIR}/expression_file.cmake)
    set_tests_properties(genetik_parsing_quickstart PROPERTIES TIMEOUT 30)
    set_tests_properties(genetik_expression_file PROPERTIES TIMEOUT 60)
endif()

add_executable(genetik_template_tests scalar_templates.cpp)
target_link_libraries(genetik_template_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_template_contracts COMMAND genetik_template_tests)
add_test(NAME genetik_template_overlap COMMAND genetik_template_tests --overlap)
set_tests_properties(genetik_template_contracts genetik_template_overlap PROPERTIES TIMEOUT 90)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_templates_quickstart COMMAND genetik_templates_quickstart --threads 2)
    add_test(NAME genetik_template_campaign_smoke COMMAND ${CMAKE_COMMAND}
        -DCAMPAIGN=$<TARGET_FILE:genetik_scalar_template_campaign>
        -DWORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/template-campaign
        -P ${CMAKE_CURRENT_SOURCE_DIR}/template_cli.cmake)
    set_tests_properties(genetik_templates_quickstart PROPERTIES TIMEOUT 30)
    set_tests_properties(genetik_template_campaign_smoke PROPERTIES TIMEOUT 90)
endif()

add_executable(genetik_derivative_tests derivatives.cpp)
target_link_libraries(genetik_derivative_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_derivative_contracts COMMAND genetik_derivative_tests)
add_test(NAME genetik_derivative_overlap COMMAND genetik_derivative_tests --overlap)
set_tests_properties(genetik_derivative_contracts genetik_derivative_overlap PROPERTIES TIMEOUT 90)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_derivatives_quickstart COMMAND genetik_derivatives_quickstart)
    set_tests_properties(genetik_derivatives_quickstart PROPERTIES TIMEOUT 30)
endif()

add_executable(genetik_derivative_regression_tests derivative_regression.cpp)
target_link_libraries(genetik_derivative_regression_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_derivative_regression_contracts COMMAND genetik_derivative_regression_tests)
add_test(NAME genetik_derivative_regression_overlap COMMAND genetik_derivative_regression_tests --overlap)
set_tests_properties(genetik_derivative_regression_contracts genetik_derivative_regression_overlap PROPERTIES TIMEOUT 90)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_derivative_regression_quickstart COMMAND genetik_derivative_regression_quickstart --threads 2)
    set_tests_properties(genetik_derivative_regression_quickstart PROPERTIES TIMEOUT 60)
endif()
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_derivative_regression_cli COMMAND ${CMAKE_COMMAND}
        -DCAMPAIGN=$<TARGET_FILE:genetik_derivative_regression_campaign>
        -DWORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/derivative-regression-cli
        -P ${CMAKE_CURRENT_SOURCE_DIR}/derivative_regression_cli.cmake)
    set_tests_properties(genetik_derivative_regression_cli PROPERTIES TIMEOUT 60)
endif()

add_executable(genetik_integral_tests integrals.cpp)
target_link_libraries(genetik_integral_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_integral_contracts COMMAND genetik_integral_tests)
add_test(NAME genetik_integral_overlap COMMAND genetik_integral_tests --overlap)
set_tests_properties(genetik_integral_contracts genetik_integral_overlap PROPERTIES TIMEOUT 90)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_integrals_quickstart COMMAND genetik_integrals_quickstart --threads 2)
    add_test(NAME genetik_integral_cli COMMAND ${CMAKE_COMMAND}
        -DCAMPAIGN=$<TARGET_FILE:genetik_integral_campaign>
        -DWORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/integral-cli
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integral_cli.cmake)
    set_tests_properties(genetik_integrals_quickstart genetik_integral_cli PROPERTIES TIMEOUT 90)
endif()

add_executable(genetik_prime_exact_tests prime_exact.cpp)
target_link_libraries(genetik_prime_exact_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_prime_exact_contracts COMMAND genetik_prime_exact_tests)
add_test(NAME genetik_prime_exact_overlap COMMAND genetik_prime_exact_tests --overlap)
set_tests_properties(genetik_prime_exact_contracts genetik_prime_exact_overlap PROPERTIES TIMEOUT 90)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_prime_exact_cli COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/prime_exact_cli.py $<TARGET_FILE:genetik_prime_regression> ${CMAKE_CURRENT_BINARY_DIR}/prime-exact-cli)
    set_tests_properties(genetik_prime_exact_cli PROPERTIES TIMEOUT 180)
endif()

add_executable(genetik_checkpoint_tests run_checkpoint.cpp)
target_link_libraries(genetik_checkpoint_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_checkpoint_contracts COMMAND genetik_checkpoint_tests contracts ${CMAKE_CURRENT_BINARY_DIR}/checkpoint-contracts/checkpoint-fixture)
add_test(NAME genetik_checkpoint_overlap COMMAND genetik_checkpoint_tests overlap ${CMAKE_CURRENT_BINARY_DIR}/checkpoint-overlap/checkpoint-fixture)
add_test(NAME genetik_formatting_contracts COMMAND genetik_checkpoint_tests format ${CMAKE_CURRENT_BINARY_DIR}/formatting/checkpoint-fixture)
set_tests_properties(genetik_checkpoint_contracts genetik_checkpoint_overlap genetik_formatting_contracts PROPERTIES TIMEOUT 180)
add_test(NAME genetik_checkpoint_process COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checkpoint_process.py $<TARGET_FILE:genetik_checkpoint_tests> ${CMAKE_CURRENT_BINARY_DIR}/checkpoint-process/checkpoint-fixture)
set_tests_properties(genetik_checkpoint_process PROPERTIES TIMEOUT 240)

if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_checkpoint_quickstart COMMAND genetik_checkpoint_quickstart)
    set_tests_properties(genetik_checkpoint_quickstart PROPERTIES TIMEOUT 30)
endif()
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_checkpoint_cli COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checkpoint_cli.py $<TARGET_FILE:genetik_prime_regression> ${CMAKE_CURRENT_BINARY_DIR}/checkpoint-cli)
    set_tests_properties(genetik_checkpoint_cli PROPERTIES TIMEOUT 300)
endif()

add_executable(genetik_discrete_tests discrete.cpp)
target_link_libraries(genetik_discrete_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_discrete_contracts COMMAND genetik_discrete_tests ${CMAKE_CURRENT_SOURCE_DIR}/discrete-arithmetic.tsv)
add_test(NAME genetik_discrete_overlap COMMAND genetik_discrete_tests --overlap)
set_tests_properties(genetik_discrete_contracts genetik_discrete_overlap PROPERTIES TIMEOUT 120)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_discrete_quickstart COMMAND genetik_discrete_quickstart --threads 2)
    set_tests_properties(genetik_discrete_quickstart PROPERTIES TIMEOUT 60)
endif()

add_executable(genetik_discrete_iteration_tests discrete_iteration.cpp)
target_link_libraries(genetik_discrete_iteration_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_discrete_iteration_contracts COMMAND genetik_discrete_iteration_tests ${CMAKE_CURRENT_BINARY_DIR}/iteration-contracts)
add_test(NAME genetik_discrete_iteration_overlap COMMAND genetik_discrete_iteration_tests ${CMAKE_CURRENT_BINARY_DIR}/iteration-overlap --overlap)
set_tests_properties(genetik_discrete_iteration_contracts genetik_discrete_iteration_overlap PROPERTIES TIMEOUT 180)
# The unchanged complete 1/2/4-worker 100-versus-40/load/100 fixture took
# 307.6s under ASan+UBSan on the Jetson. Preserve all assertions/instrumentation.
if(GENETIK_SANITIZERS)
    set_tests_properties(genetik_discrete_iteration_contracts PROPERTIES TIMEOUT 900)
endif()
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_discrete_iteration_quickstart COMMAND genetik_discrete_iteration_quickstart --threads 2)
    set_tests_properties(genetik_discrete_iteration_quickstart PROPERTIES TIMEOUT 60)
endif()

add_executable(genetik_discrete_prime_tests discrete_prime.cpp)
target_link_libraries(genetik_discrete_prime_tests PRIVATE Genetik::Genetik)
target_include_directories(genetik_discrete_prime_tests PRIVATE ${PROJECT_SOURCE_DIR})
add_test(NAME genetik_discrete_prime_contracts COMMAND genetik_discrete_prime_tests ${CMAKE_CURRENT_SOURCE_DIR}/discrete-primes.tsv ${CMAKE_CURRENT_BINARY_DIR}/discrete-prime)
add_test(NAME genetik_discrete_prime_overlap COMMAND genetik_discrete_prime_tests --overlap)
set_tests_properties(genetik_discrete_prime_contracts PROPERTIES TIMEOUT 600)
# Unchanged ASan+UBSan reference/continuation fixture completed in1484.5s
# including107.1s of recorded diagnostic pauses; keep all checks/instrumentation.
if(GENETIK_SANITIZERS)
    set_tests_properties(genetik_discrete_prime_contracts PROPERTIES TIMEOUT 3600)
endif()
set_tests_properties(genetik_discrete_prime_overlap PROPERTIES TIMEOUT 180)
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_discrete_prime_cli COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/discrete_prime_cli.py $<TARGET_FILE:genetik_discrete_prime_search> ${CMAKE_CURRENT_BINARY_DIR}/discrete-prime-cli)
    set_tests_properties(genetik_discrete_prime_cli PROPERTIES TIMEOUT 600)
endif()

add_executable(genetik_checkpoint_v03_writer compatibility/scalar.cpp)
target_compile_definitions(genetik_checkpoint_v03_writer PRIVATE GENETIK_V03_PRODUCER=1)
target_link_libraries(genetik_checkpoint_v03_writer PRIVATE Genetik::Genetik)
add_executable(genetik_checkpoint_compatibility_tests compatibility/scalar.cpp)
target_link_libraries(genetik_checkpoint_compatibility_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_checkpoint_compatibility COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run.py $<TARGET_FILE:genetik_checkpoint_v03_writer> $<TARGET_FILE:genetik_checkpoint_compatibility_tests> ${CMAKE_CURRENT_BINARY_DIR}/checkpoint-compatibility)
set_tests_properties(genetik_checkpoint_compatibility PROPERTIES TIMEOUT 180)
add_executable(genetik_checkpoint_v04_writer compatibility/scalar_v04.cpp)
target_compile_definitions(genetik_checkpoint_v04_writer PRIVATE GENETIK_V04_PRODUCER=1)
target_link_libraries(genetik_checkpoint_v04_writer PRIVATE Genetik::Genetik)
add_executable(genetik_checkpoint_v04_compatibility_tests compatibility/scalar_v04.cpp)
target_link_libraries(genetik_checkpoint_v04_compatibility_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_checkpoint_v04_compatibility COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run.py $<TARGET_FILE:genetik_checkpoint_v04_writer> $<TARGET_FILE:genetik_checkpoint_v04_compatibility_tests> ${CMAKE_CURRENT_BINARY_DIR}/checkpoint-v04-compatibility)
set_tests_properties(genetik_checkpoint_v04_compatibility PROPERTIES TIMEOUT 180)
add_executable(genetik_discrete_checkpoint_v04_writer compatibility/discrete_v04.cpp)
target_compile_definitions(genetik_discrete_checkpoint_v04_writer PRIVATE GENETIK_V04_PRODUCER=1)
target_link_libraries(genetik_discrete_checkpoint_v04_writer PRIVATE Genetik::Genetik)
add_executable(genetik_discrete_checkpoint_v04_compatibility_tests compatibility/discrete_v04.cpp)
target_link_libraries(genetik_discrete_checkpoint_v04_compatibility_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_discrete_checkpoint_v04_compatibility COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run.py $<TARGET_FILE:genetik_discrete_checkpoint_v04_writer> $<TARGET_FILE:genetik_discrete_checkpoint_v04_compatibility_tests> ${CMAKE_CURRENT_BINARY_DIR}/checkpoint-v04-discrete-compatibility)
set_tests_properties(genetik_discrete_checkpoint_v04_compatibility PROPERTIES TIMEOUT 180)

add_executable(genetik_d5_iteration_tests d5_iteration_discovery.cpp)
target_link_libraries(genetik_d5_iteration_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_d5_iteration_contracts COMMAND genetik_d5_iteration_tests)
add_test(NAME genetik_d5_iteration_overlap COMMAND genetik_d5_iteration_tests --overlap)
set_tests_properties(genetik_d5_iteration_contracts genetik_d5_iteration_overlap PROPERTIES TIMEOUT 600)
add_executable(genetik_d5_integer_tests d5_integer.cpp)
target_link_libraries(genetik_d5_integer_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_d5_integer_contracts COMMAND genetik_d5_integer_tests ${CMAKE_CURRENT_SOURCE_DIR}/d5-integer-references.tsv)
set_tests_properties(genetik_d5_integer_contracts PROPERTIES TIMEOUT 180)
add_executable(genetik_d5_recurrence_tests d5_recurrence.cpp)
target_link_libraries(genetik_d5_recurrence_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_d5_recurrence_contracts COMMAND genetik_d5_recurrence_tests)
add_test(NAME genetik_d5_recurrence_overlap COMMAND genetik_d5_recurrence_tests --overlap)
set_tests_properties(genetik_d5_recurrence_contracts genetik_d5_recurrence_overlap PROPERTIES TIMEOUT 600)
add_executable(genetik_d5_scientific_tests d5_scientific.cpp)
target_link_libraries(genetik_d5_scientific_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_d5_scientific_contracts COMMAND genetik_d5_scientific_tests ${CMAKE_CURRENT_SOURCE_DIR}/d5-scientific-references.tsv)
add_test(NAME genetik_d5_scientific_overlap COMMAND genetik_d5_scientific_tests overlap --overlap)
set_tests_properties(genetik_d5_scientific_contracts genetik_d5_scientific_overlap PROPERTIES TIMEOUT 300)

add_executable(genetik_d5_symbolic_tests d5_symbolic.cpp)
target_link_libraries(genetik_d5_symbolic_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_d5_symbolic_contracts COMMAND genetik_d5_symbolic_tests)
add_test(NAME genetik_d5_symbolic_overlap COMMAND genetik_d5_symbolic_tests --overlap)
set_tests_properties(genetik_d5_symbolic_contracts genetik_d5_symbolic_overlap PROPERTIES TIMEOUT 300)

add_executable(genetik_d5_dimension_tests d5_dimensions.cpp)
target_link_libraries(genetik_d5_dimension_tests PRIVATE Genetik::Genetik)
add_test(NAME genetik_d5_dimension_contracts COMMAND genetik_d5_dimension_tests)
add_test(NAME genetik_d5_dimension_overlap COMMAND genetik_d5_dimension_tests --overlap)
set_tests_properties(genetik_d5_dimension_contracts genetik_d5_dimension_overlap PROPERTIES TIMEOUT 600)
if(GENETIK_SANITIZERS OR GENETIK_THREAD_SANITIZER)
    # Unchanged ASan+UBSan fixture:889.17s; the initial600s cap interrupted it.
    # Keep all dimensional generation/fitting and1/2/4-worker continuation work.
    set_tests_properties(genetik_d5_dimension_contracts PROPERTIES TIMEOUT 1800)
endif()

if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_advanced_mathematics_quickstart COMMAND genetik_advanced_mathematics_quickstart --threads 2)
    set_tests_properties(genetik_advanced_mathematics_quickstart PROPERTIES TIMEOUT 120)
endif()

foreach(header AdvancedInteger ScalarOperators)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/header_${header}.cpp "#include <detail/${header}.hpp>\n")
    add_library(header_${header} OBJECT ${CMAKE_CURRENT_BINARY_DIR}/header_${header}.cpp)
    target_link_libraries(header_${header} PRIVATE Genetik::Genetik)
endforeach()

foreach(header ObservedDifferentialSystem ComplexExpressionParsing ScientificWork ExpressionSecondDerivative ScientificEquationSystem BoundedTensor ComplexExpression AdvancedLinearAlgebra FiniteStatistics ScientificRegression)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/header_${header}.cpp "#include <${header}.hpp>\n")
    add_library(header_${header} OBJECT ${CMAKE_CURRENT_BINARY_DIR}/header_${header}.cpp)
    target_link_libraries(header_${header} PRIVATE Genetik::Genetik)
endforeach()
add_executable(genetik_d6_tests d6_science.cpp)
target_link_libraries(genetik_d6_tests PRIVATE Genetik::Genetik)
if(GENETIK_SCIENTIFIC_NUMERICS)
    target_link_libraries(genetik_d6_tests PRIVATE Genetik::ScientificNumerics)
endif()
add_test(NAME genetik_d6_scientific_contracts COMMAND genetik_d6_tests ${CMAKE_CURRENT_SOURCE_DIR}/d6-scientific-references.tsv)
add_test(NAME genetik_d6_continuation COMMAND genetik_d6_tests continuation)
add_test(NAME genetik_d6_overlap COMMAND genetik_d6_tests overlap)
set_tests_properties(genetik_d6_scientific_contracts genetik_d6_continuation genetik_d6_overlap PROPERTIES TIMEOUT 600 ENVIRONMENT "OPENBLAS_NUM_THREADS=1;OMP_NUM_THREADS=1")

if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_scientific_discovery_quickstart COMMAND genetik_scientific_discovery_quickstart)
    set_tests_properties(genetik_scientific_discovery_quickstart PROPERTIES TIMEOUT 60 ENVIRONMENT "OPENBLAS_NUM_THREADS=1;OMP_NUM_THREADS=1")
endif()

# This opt-in fixture requires locally installed LP64 LAPACK. Base package tests
# remain dependency-free; qualification enables this optional component explicitly.
if(GENETIK_SCIENTIFIC_NUMERICS)
    add_test(NAME genetik_d6_optional_package COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/d6_optional_package.py --source ${PROJECT_SOURCE_DIR} --workspace ${CMAKE_CURRENT_BINARY_DIR}/d6-optional-fixture --cmake ${CMAKE_COMMAND})
    set_tests_properties(genetik_d6_optional_package PROPERTIES TIMEOUT 600)
endif()
if(GENETIK_BUILD_EXAMPLES)
    add_test(NAME genetik_d6_budget_cli COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/d6_budget_cli.py $<TARGET_FILE:genetik_d6_scientific_benchmarks> ${CMAKE_CURRENT_BINARY_DIR}/d6-budget-cli)
    set_tests_properties(genetik_d6_budget_cli PROPERTIES TIMEOUT 180)
endif()
