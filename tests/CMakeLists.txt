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
foreach(header TypedVariation TypedSymbolicRegression TypedRegression TypedValue TypedExpression Execution Creature_i Creature Population_i Population GeneticAlgorithm_i GeneticAlgorithm RandomContext Fitness Expression EvaluatedCandidate ScalarVariation Regression ExpressionVariation SymbolicRegression ConstantFitting)
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
    set_tests_properties(genetik_expression_fp_policy PROPERTIES TIMEOUT 30)
endif()

# Application support headers also compile without accidental preceding includes.
foreach(header RegressionData RegressionWorkflow MultivariateData StrictArithmeticData ExecutionOptions)
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
set_tests_properties(genetik_installed_package PROPERTIES TIMEOUT 300)
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
set_tests_properties(genetik_typed_regression_contracts PROPERTIES TIMEOUT 90)
