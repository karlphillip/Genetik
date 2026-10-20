find_package(Threads REQUIRED)
add_library(genetik_lib INTERFACE)
target_link_libraries(genetik_lib INTERFACE Threads::Threads)
add_library(Genetik::Genetik ALIAS genetik_lib)
set_target_properties(genetik_lib PROPERTIES EXPORT_NAME Genetik)
target_include_directories(genetik_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Genetik>)
target_compile_features(genetik_lib INTERFACE cxx_std_20)
include(${PROJECT_SOURCE_DIR}/cmake/GenetikFloatingPoint.cmake)
# Export strips producer-selected flags; GenetikConfig selects the consumer frontend.
genetik_require_floating_point(genetik_lib TRUE)
if(GENETIK_INSTALL)
    install(TARGETS genetik_lib EXPORT GenetikTargets)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Genetik FILES_MATCHING PATTERN "*.hpp")
endif()
