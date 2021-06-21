#include "Creature.hpp"
#include "Population.hpp"

#include <iostream>


Population::Population()
: _populationSize(0)
{
}

Population::~Population()
{
}

void Population::setSize(const unsigned int& populationSize)
{

}

std::unique_ptr<std::vector<std::shared_ptr<Creature_i>> > Population::getPopulation() const
{
    return std::make_unique<std::vector<std::shared_ptr<Creature_i>> >(_population);
}

float Population::getMean()
{
    return 0;
}

void Population::setCreature(const unsigned int& pos, const std::shared_ptr<Creature_i> creature)
{

}

void Population::_computeFitness()
{

}

void Population::_computeFitnessPercent()
{

}

void Population::_sortPopulation()
{

}

void Population::_computeRouletteRange()
{

}
void Population::_updateValues()
{

}

std::string Population::stats()
{
    return std::string();
}

std::shared_ptr<Creature_i> Population::getBestCreature()
{
    return std::shared_ptr<Creature_i>();
}
