set(HEADER_FILES GeneticAlgorithm_i.hpp GeneticAlgorithm.hpp Population_i.hpp Population.hpp Creature_i.hpp Creature.hpp Chromosome_i.hpp Chromosome.hpp Settings.hpp)
set(SOURCE_FILES GeneticAlgorithm.cpp Population.cpp Creature.cpp Chromosome.cpp)

add_library(genetik_lib STATIC ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(genetik_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
