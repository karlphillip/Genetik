#include <GeneticAlgorithm.hpp>
#include <Population.hpp>
#include <Creature.hpp>
static_assert(__cplusplus >= 202002L);
int main() {
    RandomContext run(9);
    Population<Creature<char>> population(3, run);
    GeneticAlgorithm<decltype(population), Creature<char>> ga(0.5f, 0.9f, run);
    if (ga.evolve(population) != GenerationOutcome::Committed) return 1;
    const auto best = population.bestSeen();
    return ga.evolutions() == 1 && population.generation() == 1 && best &&
           best->fitnessState() == FitnessState::Valid && run.evaluationAttempts() == 6 ? 0 : 1;
}
