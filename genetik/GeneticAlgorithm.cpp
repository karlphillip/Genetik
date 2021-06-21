#include "GeneticAlgorithm.hpp"

#include <iostream>

GeneticAlgorithm::GeneticAlgorithm(const Settings& settings)
{
    _settings = settings;    
}

void GeneticAlgorithm::run()
{
    std::cout << "GeneticAlgorithm::run: crossoverRate=" << _settings.crossoverRate << " mutationRate=" << _settings.mutationRate << std::endl;
}

void GeneticAlgorithm::_crossover()
{

}
void GeneticAlgorithm::_mutation()
{

}
void GeneticAlgorithm::_roulette()
{

}
