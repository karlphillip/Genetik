#pragma once
#include "Creature_i.hpp"
#include <vector>
#include <memory>

template <typename T>
class Population_i
{
public:

    /*
     * Parameterized constructor that initializes the population with a specific size.
     *
     * @param size a value that indicates the number of population creatures.
     */
    Population_i(const unsigned int& size)
    {
        _populationSize = size;
    }

    /*
     * Virtual destructor
     */
    virtual ~Population_i() = default;

    /*
     * Obliterates the old population and creates new creatures to replace them.
     *
     * @param size a value that indicates the number of population creatures.
     */
    virtual void setSize(const unsigned int& size) = 0;

    /*
     * The vector of creatures.
     *
     * @return a shared pointer to the vector of creatures.
     */
    virtual std::shared_ptr<std::vector<T>> creatures() const = 0;

    /*
     * Calculates the average fitness of the population.
     *
     * @return the average fitness of the population.
     */
    virtual float mean() = 0;

    /*
     * Identifies the creature with the highest fitness level.
     *
     * @return the most fit creature of the population.
     */
    virtual T bestCreature() = 0;

    /*
     * Replaces a creature in the population given its index position in the vector.
     *
     * @param pos the index of the creature that needs to be replaced.
     * @param creature the new creature that needs to be placed in the population.
     */
    virtual void setCreature(const unsigned int& pos, const T& creature) = 0;

    /*
     * Triggers the recalculation of fitness, fitness percetange and roulette range,
     * for each creature of the population.
     */
    virtual void updateValues() = 0;

    /*
     * A string that contains information about every creature of the population.
     *
     * @return a std::string object that contains info about the population.
     */
    virtual std::string stats() = 0;

protected:
    /*
     * Triggers the fitness calculation for each creature of the population.
     */
    virtual void _computeFitness() = 0;

    /*
     * Triggers the calculation of the global fit metric for each creature.
     * This value describes how fit a creature is when compared to everybody else in the population.
     */
    virtual void _computeFitnessPercent() = 0;

    /*
     * Triggers the roulette calculation that identifies how big is the slice of the pie for each creature.
     * The roulette method is used to select parents for the crossover operation. The more fit a
     * creature is, the larger is their slice of the pie. That also means they have a bigger chance to
     * be selected as a parent.
     */
    virtual void _computeRouletteRange() = 0;

    /*
     * A sorting operation that places the fittest creatures at the beginning of the vector.
     */
    virtual void _sort() = 0;

    /*
     * A variable that defines the number of creatures of the population.
     */
    unsigned int _populationSize;

    /*
     * A vector that stores a list of creatures that make up the population.
     */
    std::vector<T> _creatures;
};
