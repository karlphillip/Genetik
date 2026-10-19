execute_process(COMMAND "${CMAKE_COMMAND}" -S "${GENETIK_SOURCE}/tests/downstream"
    -B "${DOWNSTREAM_BINARY}" -DGENETIK_SOURCE=${GENETIK_SOURCE}
    -DCMAKE_CXX_COMPILER=${CXX_COMPILER} -DCMAKE_BUILD_TYPE=Release RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "downstream configuration failed: ${result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${DOWNSTREAM_BINARY}" -j2 RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "downstream build failed: ${result}")
endif()
execute_process(COMMAND "${DOWNSTREAM_BINARY}/consumer" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "downstream execution failed: ${result}")
endif()
