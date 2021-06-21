#pragma once
#include "Settings.hpp"

class GeneticAlgorithm : public GeneticAlgorithm_i
{
public:    
    GeneticAlgorithm(const Settings& settings);

    void run();

private:
    void _crossover();
    void _mutation();
    void _roulette();

    Settings _settings;   
};
