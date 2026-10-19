# Verify a public-header-only consumer cannot silently compile under known unsafe modes.
foreach(option -ffast-math -ffinite-math-only)
    execute_process(COMMAND "${CXX_COMPILER}" -std=c++20 ${option}
        -I${GENETIK_SOURCE}/genetik -fsyntax-only -x c++ ${GENETIK_SOURCE}/genetik/Expression.hpp
        RESULT_VARIABLE result ERROR_VARIABLE diagnostics OUTPUT_QUIET)
    if(result EQUAL 0 OR NOT diagnostics MATCHES "Genetik expressions require strict floating point")
        message(FATAL_ERROR "unsafe FP mode was not rejected as expected: ${option}: ${diagnostics}")
    endif()
endforeach()
