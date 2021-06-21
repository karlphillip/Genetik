#pragma once
#include "Population_i.hpp"

#include <vector>
#include <memory>


class Population : public Population_i
{
public:
    Population();
    ~Population();

    void setSize(const unsigned int& sz);

    std::unique_ptr<std::vector<std::shared_ptr<Creature_i>> > getPopulation() const;

    float getMean();

    void setCreature(const unsigned int& pos, const std::shared_ptr<Creature_i> creature);

    std::shared_ptr<Creature_i> getBestCreature();

    std::string stats();

private:
    void _computeFitness();

    void _computeFitnessPercent();

    void _computeRouletteRange();

    void _sortPopulation();

    void _updateValues();

    unsigned int _populationSize;

    std::vector<std::shared_ptr<Creature_i> > _population;
};
