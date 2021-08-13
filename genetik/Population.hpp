#pragma once
#include "Population_i.hpp"

#include <vector>
#include <memory>


template <typename T>
class Population : public Population_i<T>
{
public:
    Population(const unsigned int& size)
    : Population_i<T>(size)
    {
        setSize(size);
    }

    ~Population()
    {

    }

    /*
     * setSize: obliterates the old population and creates a new one
     */
    void setSize(const unsigned int& newSize)
    {
        // TODO: does this release dynamic memory?
        // delete current population
        this->_creatures.clear();

        // update population size
        this->_populationSize = newSize;

        // create new population
        for (unsigned int i = 0; i < this->_populationSize; ++i)
        {
            T creature;
            //std::cout << "Population::setSize creature.fitness=" << creature.fitness() << std::endl;
            this->_creatures.push_back(creature);
            //std::cout << "Population::setSize --- pushed ----" << std::endl;
        }

        //std::cout << "Population::setSize sz=" << this->_creatures.size() << std::endl;
        //std::cout << "Population::setSize creatures[0].fitness=" << this->_creatures[0].fitness() << std::endl;
        float v = this->creatures()->at(0).fitness();
        //std::cout << "Population::setSize creatures.at(0).fitness=" << v << std::endl;

        // TODO: update values messes up things on test9()
        updateValues();
    }

    /*
     *
     */
    std::shared_ptr<std::vector<T>> creatures() const
    {
        //return this->_creatures;
        std::shared_ptr<std::vector<T>> ptr = std::make_shared<std::vector<T>>(this->_creatures);
        //std::cout << "Population::creatures size=" << ptr->size() << " #0 fitness=" << ptr->at(0).fitness() << std::endl;
        return ptr;
    }

    /*
     *
     */
    float mean()
    {
        float sumF = 0;
        for (unsigned int i = 0; i < this->_populationSize; ++i)
        {
            sumF += this->_creatures[i].fitness();
        }

        return sumF / (float) this->_populationSize;
    }

    /*
     *
     */
    T bestCreature()
    {
        if ((this->_populationSize == this->_creatures.size()) && (this->_populationSize > 1))
            return this->_creatures[this->_populationSize-1];

        T tmp;
        return tmp;
    }

    /*
     *
     */
    void setCreature(const unsigned int& pos, const T& creature)
    //void setCreature(const unsigned int& pos, const T& creature)
    {
        if (pos < this->_creatures.size())
            this->_creatures[pos] = creature;
    }

    /*
     *
     */
    void updateValues()
    {
        // evaluate the population: updates values in each creature
        _computeFitness();
        _computeFitnessPercent();
        _computeRouletteRange();
    }

    /*
     *
     */
    std::string stats()
    {
        std::string output;
        for (unsigned int i = 0; i < this->_populationSize; ++i)
            output += "Creature #" + std::to_string(i) + " " + this->_creatures[i].stats() + "\n";

        return output;
    }

private:
    /*
     *
     */
    void _computeFitness()
    {
        for (unsigned int i = 0; i < this->_populationSize; ++i)
        {
            //std::cout << "[GA] Population::computeFitness() Creature #" << i << std::endl;
            this->_creatures[i].calculateFitness();
            float fitness = this->_creatures[i].fitness();

            // display chromosome and fitness level of the creature
            //std::cout << "[GA] Population::computeFitness() Creature #" << i << " [" << this->_creatures[i].getString() << "]" << " fitness: " << fitness << std::endl;
            //std::cout << "[GA] Population::computeFitness() Creature #" << i << " fitness: " << fitness << std::endl;
        }
    }

    /*
     *
     */
    void _computeFitnessPercent()
    {
        unsigned int fitnessSum = 0;

        for (unsigned int i = 0; i < this->_populationSize; ++i)
        {
            //std::cout << "Population::_computeFitnessPercent creature #" << i << " fitness=" << this->_creatures[i].fitness() << std::endl;
            fitnessSum += this->_creatures[i].fitness() * 100;
            //std::cout << "Population::_computeFitnessPercent fitnessSum=" << fitnessSum << std::endl;
        }

        for (unsigned int i = 0; i < this->_populationSize; ++i)
        {
            // avoid division by 0 on fp calculation
            if (fitnessSum == 0)
            {
                this->_creatures[i].setFitnessPercent(0.f);
                continue;
            }

            float fp = (this->_creatures[i].fitness() * 100) / (float) fitnessSum / 100.f;
            this->_creatures[i].setFitnessPercent(fp);
        }
    }

    /*
     *
     */
    void _computeRouletteRange()
    {
        _sort();

        float sumFP = 0;
        for (unsigned int i = 0; i < this->_populationSize; ++i)
        {
            // after sorting, the first creature has the smallest fitness level. It is also the first slice of the pizza
            if (i == 0)
            {
                sumFP += this->_creatures[i].fitnessPercent();
                this->_creatures[i].setRouletteRange(0, sumFP);
            }
            else if (i == this->_populationSize-1)
            {
                this->_creatures[i].setRouletteRange(sumFP, 100.f);
            }
            else
            {
                this->_creatures[i].setRouletteRange(sumFP, sumFP + this->_creatures[i].fitnessPercent());
                sumFP += this->_creatures[i].fitnessPercent();
            }
        }
    }

    /*
     *
     */
    void _sort()
    {
        for (unsigned int i = 0; i < this->_populationSize; ++i)
        {
            for (unsigned int j = 0; j < this->_populationSize; ++j)
            {
                if (this->_creatures[i].fitnessPercent() < this->_creatures[j].fitnessPercent())
                {
                    T aux = this->_creatures[i];
                    this->_creatures[i] = this->_creatures[j];
                    this->_creatures[j] = aux;
                }
            }
        }
    }
};
