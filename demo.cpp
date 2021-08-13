#include "genetik/GeneticAlgorithm.hpp"
#include "genetik/Population.hpp"
#include "genetik/Creature.hpp"

#include <iostream>
#include <memory>

// global template aliases
typedef Creature<char> CreatureType;
typedef Population<CreatureType> PopulationType;


int main()
{
    // set the number of decimal places to a fixed value for improved aesthetics
    std::cout << std::fixed << std::setprecision(3);

    // initialize the population of N creatures
    unsigned int populationSize = 100;
    PopulationType population(populationSize);

    // track the fittest creature that appeared during the evolutionary process
    //CreatureType bestCreatureEver = population.bestCreature();

    // create the GeneticAlgorithm itself
    float crossoverRate = 0.5f;
    float mutationRate = 0.9f;
    GeneticAlgorithm<PopulationType, CreatureType> ga(crossoverRate, mutationRate);

    while (true)
    {        
        // perform one evolution step
        ga.evolve(population);               

        // print info to monitor progress        
        std::cout << "* Generation #" << ga.evolutions() <<
                     " avg fitness: " << population.mean() <<
                     " best fitness: " << population.bestCreature().fitness() << std::endl;

        // print fittest creature of the current generation
        std::cout << "  " << population.bestCreature().stats() << std::endl;
        //std::cout << std::endl;

        /*
        // track the best candidate found so far
        if (population.bestCreature().fitness() > bestCreatureEver.fitness())
        {
            bestCreatureEver = population.bestCreature();
            //std::cout << "  " << bestCreatureEver.stats() << std::endl;
        }
        */

        // check if one creature achieved maximum fitness and stop the evolution
        if (population.bestCreature().fitness() == 1.0f)
            break;
    }

    return 0;
}
