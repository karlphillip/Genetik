#pragma once
#include <vector>
#include <string>
#include <iostream>


template <typename T>
class Creature_i
{
public:
    /*
     * Default constructor
     */
    Creature_i()
    {
        //std::cout << "Creature_i::Creature_i" << std::endl;
        _fitness = _fitnessPercent = _rouletteRangeStart = _rouletteRangeEnd = 0;
    }

    /*
     * Parameterized constructor
     *
     * @param chromosome initializes the creature's chromosome from existing data
     */
    Creature_i(const std::vector<T>& chromosome)
    {
        //std::cout << "Creature_i::Creature_i(chromosome)" << std::endl;
        _fitness = _fitnessPercent = _rouletteRangeStart = _rouletteRangeEnd = 0;
        _chromosome = chromosome;
    }

    /*
     * Copy constructor
     *
     * @param other indicates the object that needs to be copied
     */
    Creature_i(const Creature_i<T>& other)
    {        
        //std::cout << "Creature_i::Creature_i(other)" << std::endl;
        _chromosome = other._chromosome;
        _fitness = other._fitness;
        _fitnessPercent = other._fitnessPercent;
        _rouletteRangeStart = other._rouletteRangeStart;
        _rouletteRangeEnd = other._rouletteRangeEnd;
        //std::cout << "Creature_i::Creature_i(other) other._fitness=" << other._fitness << " fitness=" << _fitness << std::endl;
    }

    /*
     * Copy assignment operator
     * @param other indicates the rhs object that needs to be copied
     */
    Creature_i& operator=(const Creature_i<T>& other)
    {
       //std::cout << "Creature_i::operator= (copy assignment)" << std::endl;
       _chromosome = other._chromosome;
       _fitness = other._fitness;
       _fitnessPercent = other._fitnessPercent;
       _rouletteRangeStart = other._rouletteRangeStart;
       _rouletteRangeEnd = other._rouletteRangeEnd;
       return *this;
    }

    /*
     * Virtual destructor
     */
    virtual ~Creature_i() = default;

    /*
     * The genetic code of the Creature.
     *
     * @return a vector with the chromosome (genes).
     */
    virtual std::vector<T> chromosome() const
    {
        return _chromosome;
    }

    /*
     * Randomly changes a specific gene of the chromosome.
     *
     * @pos the index of the gene that needs to be mutated.
     */
    virtual void mutateGene(const unsigned int& pos) = 0;

    /* Change a specific gene given an index position in the chromosome.
     *
     * @param pos the index of the gene that needs to be changed.
     * @param value the new value of the gene.
     */
    virtual void setGene(const unsigned int& pos, const T& value) = 0;

    /*
     * The value stored by a gene at a specific position of the chromosome.
     *
     * @param pos the index of the gene.
     * @return the value of the gene at that specific position.
     */
    virtual T gene(const unsigned int& pos) const = 0;

    /*
     * The fitness function used to compute the fitness level of the creature.
     */
    virtual void calculateFitness() = 0;

    /*
     * The fitness level is usually a value between 0.0 and 1.0 and its an indicator
     * of performance.
     *
     * @return the fitness level of the creature
     */
    virtual float fitness() const = 0;

    /*
     * Updates the fitness percent of the creature.
     *
     * @param fp the new floating-point value for fitness percent.
     */
    virtual void setFitnessPercent(const float& fp) = 0;

    /*
     * A metric used to assess how well a creature performed when compared to the rest
     * of the population.
     *
     * @return a floating-point value that indicates how well the creature performed
     */
    virtual float fitnessPercent() const = 0;

    /*
     * Defines the limits of the slice this creature has.
     *
     * @param start
     * @param end
     */
    virtual void setRouletteRange(const float& start, const float& end) = 0;

    /* The roulette range defines the starting and endings positions of the slice of pie for a creature.
     * The range varies from 0.f to 100.f.
     *
     * @param start (out) returns the initial position of its slice on a pie.
     * @param end (out) returns the end position of its slice on a pie.
     */
    virtual void rouletteRange(float& start, float& end) const = 0;

    /*
     * A string that contains the chromosome data along with fitness values.
     *
     * @return a std::string object that contains info about the creature.
     */
    virtual std::string stats() = 0;

protected:
    /*
     * A vector that holds the genetic information of the creature.
     */
    std::vector<T> _chromosome;

    /*
     * A floating-point value that indicates the fitness level of the creature.
     */
    float _fitness;

    /*
     * A floating-point statistic that is used to determine how fit the creature is to be able to have children.
     */
    float _fitnessPercent;

    /*
     * Data that is used to define the start and end positions of the slice a creature occupies in the roulette-pie.
     */
    float _rouletteRangeStart, _rouletteRangeEnd;
};
