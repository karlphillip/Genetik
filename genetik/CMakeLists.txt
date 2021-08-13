set(HEADER_FILES Creature.hpp Creature_i.hpp Population.hpp Population_i.hpp GeneticAlgorithm.hpp GeneticAlgorithm_i.hpp)
set(SOURCE_FILES "")
#set(SOURCE_FILES Creature.cpp Population.cpp GeneticAlgorithm.cpp)

add_library(genetik_lib STATIC ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(genetik_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(genetik_lib PROPERTIES LINKER_LANGUAGE CXX)
