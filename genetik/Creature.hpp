/*
 *
 */
/* how to force the compiler to instantiate a template: https://stackoverflow.com/a/8752879/176769
 * and not have to explicitely instantiate the template at the end of the .cpp file: template class Creature<char>;
 */
#pragma once
#include "Creature_i.hpp"

#include <iomanip>
#include <random>
#include <sstream>


// global defines
#define TARGET          "'Never underestimate a droid' - General Leia Organa, The Rise of Skywalker"
#define CHROMOSOME_SZ   74
//#define TARGET          "FFFFFFFFFF"
//#define CHROMOSOME_SZ   10

// global variables
static std::mt19937 rng = std::mt19937(std::random_device()());
static std::uniform_int_distribution<unsigned int> asciiRange(32, 126);

/*
 * Creates a string representation of a float value with N decimal places
 * @return the floating-point number as a string object.
 */
inline std::string round(float num, unsigned int decimal_places)
{
    std::stringstream stream;
    stream << std::fixed << std::setprecision(decimal_places) << num;
    return stream.str();
}


template <typename T>
class Creature : public Creature_i<T>
{
public:

    /*
     * Default constructor
     */
    Creature()
    : Creature_i<T>()
    {
        //std::cout << "Creature::Creature()" << std::endl;

        // initialize chromosome with random genes
        for (unsigned int x = 0; x < CHROMOSOME_SZ; ++x)
        {
            /*
            if (x == 0)
            {
                this->_chromosome.push_back(65);
                continue;
            }
            */

            //unsigned char gene = (unsigned char) asciiRange(rng);
            unsigned char gene = (unsigned char) asciiRange(rng);
            this->_chromosome.push_back(gene);
        }

        //this->_fitness = 88;
        //std::cout << "Creature::Creature() fitness=" << this->_fitness << std::endl;
    }

    /*
     * Parameterized constructor
     * @param chromosome initializes the creature's chromosome from existing data
     */
    Creature(const std::vector<T>& chromosome)
    : Creature_i<T>(chromosome)
    {        
        //this->_fitness = 99;
        //std::cout << "Creature::Creature(chromosome) fitness=" << this->_fitness << std::endl;
    }

    /*
     * Copy constructor
     * @param other indicates the Creature to copy from
     */
    Creature(const Creature<T>& other)
    : Creature_i<T>(other)
    {
        //std::cout << "Creature::Creature(other) other.fitness=" << other._fitness << " this.fitness=" << this->_fitness <<std::endl;
    }

    /*
     * Copy assignment operator
     * @param other indicates the Creature to copy from
     */
    Creature& operator=(const Creature<T>& other)
    {
       //std::cout << "Creature::operator= (copy assignment)" << std::endl;
       this->_chromosome = other._chromosome;
       this->_fitness = other._fitness;
       this->_fitnessPercent = other._fitnessPercent;
       this->_rouletteRangeStart = other._rouletteRangeStart;
       this->_rouletteRangeEnd = other._rouletteRangeEnd;

       //std::cout << "Creature::operator= (copy assignment) this.fitness=" << this->_fitness << std::endl;
       return *this;
    }

    /*
     * Returns the Creature chromosome.
     * @return a vector with the genes.
     */
    std::vector<T> chromosome() const
    {
        return this->_chromosome;
    }

    /*
     *
     */
    void mutateGene(const unsigned int& pos)
    {
        if (pos >= this->_chromosome.size())
            return;

        T gene = asciiRange(rng);
        this->_chromosome[pos] = gene;
    }

    /*
     *
     */
    void setGene(const unsigned int& pos, const T& value)
    {
        if (pos >= this->_chromosome.size())
            return;

        this->_chromosome[pos] = value;
    }

    /*
     *
     */
    T gene(const unsigned int& pos) const
    {
        if (pos < this->_chromosome.size())
            return this->_chromosome[pos];

        return 0;
    }

    void calculateFitness()
    {
        static std::string target = TARGET;
        //static std::string target = "FFFFFFFF";

        // TODO: maybe we shouldnt compute this everytime the function is called
        //static bool once = false;
        //if (once == true) return;
        //once = true;

        if (this->_chromosome.size() != target.size())
        {
            std::cout << "Creature::fitness !!! chromosome and target have different sizes" << std::endl;
            std::cout << "Creature::fitness !!! chromosome sz=" << this->_chromosome.size() << std::endl;
            std::cout << "Creature::fitness !!!     target sz=" << target.size() << std::endl;
            return;
        }

        unsigned int matchedChars = 0;
        for (unsigned int i = 0; i < target.size(); ++i)
        {
            if (target[i] == this->_chromosome[i])
                matchedChars++;
        }

        // update fitness level
        this->_fitness = matchedChars / (float) target.size();
    }

    /*
     *
     */
    float fitness() const
    { 
        return this->_fitness;
    }

    /*
     *
     */
    void setFitnessPercent(const float& fp)
    {
        this->_fitnessPercent = fp;
    }

    /*
     *
     */
    float fitnessPercent() const
    {
        return this->_fitnessPercent;
    }

    /*
     *
     */
    void setRouletteRange(const float& start, const float& end)
    {
        this->_rouletteRangeStart = start;
        this->_rouletteRangeEnd = end;
    }

    /*
     *
     */
    void rouletteRange(float& start, float& end) const
    {
        start = this->_rouletteRangeStart;
        end = this->_rouletteRangeEnd;
    }

    std::string getString()
    {
        std::string output;

        for (unsigned int i = 0; i < this->_chromosome.size(); ++i)
            output += (T)this->_chromosome[i];

        return output;
    }

    /*
     *
     */
    std::string stats()
    {
        //std::cout << "Creature::stats fitness=" << fitness() << " fitnessPercent=" << fitnessPercent() << std::endl;

        std::string output = "str=[ " + getString() + " ] ";
        output += " f=" + round(fitness(), 3);
        output += " fP=" + round(fitnessPercent(), 3);

        float rRangeStart = 0, rRangeEnd = 0;
        rouletteRange(rRangeStart, rRangeEnd);
        output += " rouletteRange=(" + round(rRangeStart, 3) + ", " + round(rRangeEnd, 3) + ")";

        return output;
    }
};
