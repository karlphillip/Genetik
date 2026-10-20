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
foreach(header Creature_i Creature Population_i Population GeneticAlgorithm_i GeneticAlgorithm RandomContext Fitness Expression EvaluatedCandidate ScalarVariation Regression ExpressionVariation SymbolicRegression ConstantFitting)
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
foreach(header RegressionData RegressionWorkflow MultivariateData StrictArithmeticData)
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
