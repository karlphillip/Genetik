#pragma once
#include <Creature_i.hpp>
#include <vector>
#include <memory>


class Population_i
{
public:
    /*
     * ~Population_i: pure virtual destructor
     */
    virtual ~Population_i() = default;

    /*
     * setSize: creates a new population and obliterates the old population.
     */
    virtual void setSize(const unsigned int& sz) = 0;

    /*
     *
     */
    virtual std::unique_ptr<std::vector<std::shared_ptr<Creature_i>> > getPopulation() const = 0;

    /*
     *
     */
    virtual float getMean() = 0;

    /*
     *
     */
    virtual std::shared_ptr<Creature_i> getBestCreature() = 0;

    /*
     *
     */
    virtual void setCreature(const unsigned int& pos, const std::shared_ptr<Creature_i> creature) = 0;

    /*
     *
     */
    virtual std::string stats() = 0;

private:
    /*
     *
     */
    virtual void _computeFitness() = 0;

    /*
     *
     */
    virtual void _computeFitnessPercent() = 0;

    /*
     *
     */
    virtual void _computeRouletteRange() = 0;

    /*
     *
     */
    virtual void _sortPopulation() = 0;

    /*
     *
     */
    virtual void _updateValues() = 0;
};

/*
Population_i::~Population_i()
{
}
*/
