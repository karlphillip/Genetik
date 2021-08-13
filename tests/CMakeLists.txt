include_directories(${CMAKE_SOURCE_DIR}/genetik)
add_executable(genetik_tests tests.cpp)
target_link_libraries(genetik_tests genetik_lib)

install(TARGETS genetik_tests RUNTIME DESTINATION bin)