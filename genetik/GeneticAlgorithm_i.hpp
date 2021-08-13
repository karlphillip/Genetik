#pragma once
#include "Population_i.hpp"

#include <random>


template <typename P, typename C>
class GeneticAlgorithm_i
{
public:

    /*
     * Parameterized constructor initializes values used during genetic operations.
     *
     * @param crossoverRate a value that indicates the rate of crossover (0.0 to 1.0)
     * @param mutationRate a value that indicates the rate of mutation (0.0 to 1.0)
     */
    GeneticAlgorithm_i(const float& crossoverRate, const float& mutationRate)
    {
        _crossoverRate = crossoverRate;
        _mutationRate = mutationRate;

        _evolutions = 0;        
        _rng = std::mt19937(std::random_device()());
    }

    /*
     * Virtual destructor
     */
    virtual ~GeneticAlgorithm_i() = default;

    /*
     * Performs a single iteration of genetic evolution on the population in order to
     * create a new generation of individuals.
     *
     * @param population the population that needs to be evolved.
     */
    virtual void evolve(P& population) = 0;

    /*
     * How many times the evolucionary process took place during the genetic algorithm execution.
     *
     * @return the number of times evolve() has been called.
     */
    virtual unsigned int evolutions() = 0;

    /*
     * Performs the genetic operation known as crossover.
     *
     * @param father the creature that will play the role of father during the operation.
     * @param mother the creature that will play the role of mother during the operation.
     * @param childA the first object that stores the result of the crossover process.
     * @param childB the second object that stores the result of the crossover process.
     */
    virtual void crossover(const C& father, const C& mother, C& childA, C& childB) = 0;

    /*
     * Performs the genetic operation known as mutation.
     *
     * @param creature the individual that will undergo the mutation operation.
     * @return the mutated creature.
     */
    virtual C mutation(const C& creature) = 0;

    /* The roulette process randomly selects an individual of the population.
     *
     * @param population the population that needs to be used for the random selection process.
     * @return a random creature from the population.
     */
    virtual C roulette(const P& population) = 0;

protected:

    /*
     * Defines the rate of crossover.
     */
    float _crossoverRate;

    /*
     * Defines the rate of mutation.
     */
    float _mutationRate;

    /*
     * A counter that tracks how many calls to evolve() have been done.
     */
    unsigned int _evolutions;

    /*
     * A pseudo random number generator.
     */
    std::mt19937 _rng;
};
