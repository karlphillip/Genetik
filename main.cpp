#include "genetik/GeneticAlgorithm.hpp"

#include <iostream>

// class template specialization:


int main()
{
    std::cout << "main()" << std::endl;

    Settings config;
    config.crossoverRate = 0.5f;
    config.mutationRate = 0.9f;
    GeneticAlgorithm ga(config);

    ga.run();

    return 0;
}
