#include <GeneticAlgorithm.hpp>
#include <Population.hpp>
#include <Creature.hpp>
static_assert(__cplusplus >= 202002L);
int main() {
    RandomContext run(9);
    Population<Creature<char>> population(3, run);
    GeneticAlgorithm<decltype(population), Creature<char>> ga(0.5f, 0.9f, run);
    ga.evolve(population);
    return ga.evolutions() == 1 ? 0 : 1;
}
