cmake_minimum_required(VERSION 3.5)
project(GENETIK LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/genetik)

add_executable(genetik_app ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_link_libraries(genetik_app genetik_lib)
