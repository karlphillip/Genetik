find_package(Python COMPONENTS Interpreter Development REQUIRED)
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(_native MODULE NO_EXTRAS bindings.cpp)
target_link_libraries(_native PRIVATE Genetik::Genetik)
target_include_directories(_native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
install(TARGETS _native LIBRARY DESTINATION genetik RUNTIME DESTINATION genetik)
