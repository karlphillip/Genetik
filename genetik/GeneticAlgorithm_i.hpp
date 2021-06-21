#pragma once

class GeneticAlgorithm_i
{
public:
    /*
     * ~GeneticAlgorithm: pure virtual destructor
     */
    virtual ~GeneticAlgorithm() = default;

    /*
     *
     */
    void run();

private:
    /*
     *
     */
    void _crossover();

    /*
     *
     */
    void _mutation();

    /*
     *
     */
    void _roulette();
};
