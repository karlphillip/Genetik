#pragma once
#include "GeneticAlgorithm_i.hpp"

#include <iostream>
#include <vector>


template <typename P, typename C>
class GeneticAlgorithm : public GeneticAlgorithm_i<P, C>
{
public:    
    /*
     * Parameterized constructor initializes member variables.
     *
     * @param crossoverRate a value that indicates the rate of crossover (0.0 to 1.0)
     * @param mutationRate a value that indicates the rate of mutation (0.0 to 1.0)
     * @return nothing
     */
    GeneticAlgorithm(const float& crossoverRate, const float& mutationRate)
    : GeneticAlgorithm_i<P, C>(crossoverRate, mutationRate)
    {
    }

    /*
     *
     */
    void evolve(P& population)
    {
        //std::cout << "GeneticAlgorithm::evolve crossoverRate=" << this->_crossoverRate << " mutationRate=" << this->_mutationRate << std::endl;

        std::vector<C> newPopulation;

        // make sure rounding won't cause "invalid index" problems later when the old population gets replaced
        unsigned int halfSize = static_cast<unsigned int>(((float)population.creatures()->size()/2.f) + 0.5f);
        //std::cout << "GeneticAlgorithm::evolve halfSize=" << halfSize << std::endl;

        // perform crossover/mutation on half of the population
        for (unsigned int i = 0; i < halfSize; ++i)
        {
            //std::cout << "GeneticAlgorithm::evolve selection pop #" << i << std::endl;

            /*
             * perform selection: use the roulette to randomly select parents for crossover.
             * a creature that occupies a bigger slice of pie has a bigger probability of being selected.
             */
            C father = roulette(population);
            C mother = roulette(population);

            //std::cout << "GeneticAlgorithm::evolve father:" << father.stats() << std::endl;
            //std::cout << "GeneticAlgorithm::evolve mother:" << mother.stats() << std::endl;

            /*
             * perform crossver
             */
            C childA, childB;
            crossover(father, mother, childA, childB);
            //std::cout << "GeneticAlgorithm::evolve childA:" << childA.stats() << std::endl;
            //std::cout << "GeneticAlgorithm::evolve childB:" << childB.stats() << std::endl;

            /*
             * perform mutation
             */
            childA = mutation(childA);
            //std::cout << "GeneticAlgorithm::evolve mutated childA:" << childA.stats() << std::endl;

            childB = mutation(childB);
            //std::cout << "GeneticAlgorithm::evolve mutated childB:" << childB.stats() << std::endl;

            // save children as part of the new population
            newPopulation.push_back(childA);
            newPopulation.push_back(childB);
        }

        // replace old population members with new individuals
        for (unsigned int i = 0; i < population.creatures()->size(); ++i)
        {
            //std::cout << "[GA] GeneticAlgorithm::run setCreature() #" << i << std::endl;
            population.setCreature(i, newPopulation[i]);
        }

        //std::cout << "[GA] GeneticAlgorithm::run() recalculating fitness levels..." << std::endl;

        // recalculate fitness levels of the entire population
        population.updateValues();

        this->_evolutions++;
    }

    /*
     *
     */
    unsigned int evolutions()
    {
        return this->_evolutions;
    }

    void crossover(const C& father, const C& mother, C& childA, C& childB)
    {
        // not the best way to retrieve the size of a chromosome
        static unsigned int chromosomeSize = 0;
        if (chromosomeSize == 0)
        {
            C creature;
            chromosomeSize = creature.chromosome().size();
        }

        std::uniform_int_distribution<unsigned int> dist_i(0, chromosomeSize-1);
        unsigned int cutPoint = dist_i(this->_rng);

        // copy ALL father's chromosomes to child A, and mother's to child B
        childA = father;
        childB = mother;

        // check if this creature falls within crossover rate and overwrite part of the genes
        std::uniform_real_distribution<float> dist_f(0.0f, 1.0f);
        float random = dist_f(this->_rng);
        if (random < this->_crossoverRate)
        {
            // use cut point to copy the characteristics from the other parent
            for (unsigned int i = cutPoint; i < chromosomeSize; ++i)
            {
                childA.setGene(i, mother.gene(i)); // first half is from the father
                childB.setGene(i, father.gene(i)); // first half is from the mother
            }
        }

        //std::cout << "[GA] crossover: cutPoint=" << cutPoint << " random=" << random << " crossoverRate=" << _crossoverRate << std::endl;
    }

    C mutation(const C& creature)
    {        
        std::uniform_real_distribution<float> distUnit(0.0f, 1.0f);
        float random = distUnit(this->_rng);
        //std::cout << "GeneticAlgorithm::mutation random=" << random << " _mutationRate=" << this->_mutationRate << std::endl;

        // if random falls outside the mutation rate, the creature is not mutated
        C mutatedCreature = creature;
        if (random >= this->_mutationRate)
            return mutatedCreature;

        /*
         * on the other hand, when random falls within the mutation rate, 1 gene to 1% of its genes can be mutated
         * the question is, how many genes is 1%?
         */
        unsigned int onePercent = (unsigned int) mutatedCreature.chromosome().size() * 0.01f;
        if (onePercent == 0)
            onePercent = 1;
        //std::cout << "GeneticAlgorithm::mutation onePercent=" << onePercent << std::endl;

        // how many genes should be mutated?
        std::uniform_int_distribution<unsigned int> distMutation(1, onePercent);
        unsigned int numMutations = distMutation(this->_rng);
        //std::cout << "GeneticAlgorithm::mutation numMutations=" << numMutations << std::endl;

        std::uniform_int_distribution<unsigned int> distGenePos(0, mutatedCreature.chromosome().size()-1);
        for (unsigned int m = 0; m < numMutations; ++m)
        {
            unsigned int genePos = distGenePos(this->_rng);
            //std::cout << "GeneticAlgorithm::mutation genePos=" << genePos << std::endl;

            mutatedCreature.mutateGene(genePos);
        }

        return mutatedCreature;
    }

    C roulette(const P& population)
    {
        // configurable: specify the range of the population that is used to select random creatures from
        std::uniform_int_distribution<unsigned int> dist(50, 100); // 50, 100 represent the 50% of the fittest individuals

        // generate a random number for the pizza slice that will be used to retrieve a single creature
        unsigned int randomSlice = dist(this->_rng);

        std::shared_ptr<std::vector<C>> creatures = population.creatures();
        for (unsigned int i = 0; i < creatures->size(); ++i)
        {
            float start = 0, end = 0;
            creatures->at(i).rouletteRange(start, end);

            if (randomSlice >= start && randomSlice <= end)
                return creatures->at(i);
        }

        std::cout << "GeneticAlgorithm::roulette !!! zero population (probably)" << std::endl;
        C tmp;
        return tmp;
    }
};
