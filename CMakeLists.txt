cmake_minimum_required(VERSION 3.5)
project(GENETIK LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/genetik)

# create demo application
add_executable(genetik_demo ${CMAKE_CURRENT_SOURCE_DIR}/demo.cpp)
target_link_libraries(genetik_demo genetik_lib)

# create test application
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
