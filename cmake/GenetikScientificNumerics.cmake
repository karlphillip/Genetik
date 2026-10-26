# Optional Linux/LP64 LAPACK facade, no dependency in Genetik::Genetik.
include(CMakeFindDependencyMacro)
if(DEFINED BLA_VENDOR AND NOT BLA_VENDOR STREQUAL "OpenBLAS")
    message(FATAL_ERROR "ScientificNumerics requires the qualified LP64 OpenBLAS backend")
endif()
set(BLA_VENDOR OpenBLAS)
set(BLA_SIZEOF_INTEGER 4)
find_dependency(LAPACK)
if(NOT TARGET Genetik::ScientificNumerics)
    add_library(Genetik::ScientificNumerics INTERFACE IMPORTED)
    set_target_properties(Genetik::ScientificNumerics PROPERTIES
        INTERFACE_LINK_LIBRARIES "Genetik::Genetik;${LAPACK_LIBRARIES}"
        INTERFACE_COMPILE_DEFINITIONS GENETIK_HAS_LAPACK=1)
endif()
