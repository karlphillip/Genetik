add_executable(genetik_tests tests.cpp)
target_link_libraries(genetik_tests PRIVATE genetik_lib)
add_test(NAME genetik_contracts COMMAND genetik_tests)
set_tests_properties(genetik_contracts PROPERTIES TIMEOUT 30)
if(GENETIK_BUILD_EXAMPLES)
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
foreach(header Creature_i Creature Population_i Population GeneticAlgorithm_i GeneticAlgorithm RandomContext)
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
