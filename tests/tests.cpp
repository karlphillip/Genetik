#include "../genetik/GeneticAlgorithm.hpp"
#include "../genetik/Population.hpp"
#include "../genetik/Creature.hpp"

#include <iostream>
#include <memory>

// global template aliases
typedef Creature<char> CreatureType;
typedef Population<CreatureType> PopulationType;


// check creature initialization (parameterized constructor)
void test1()
{
    char message[] = "This is a chromosome!";
    std::vector<char> chromosome;
    for (char &c : message)
        chromosome.push_back(c);

    CreatureType c(chromosome);

    float rangeStart, rangeEnd;
    c.rouletteRange(rangeStart, rangeEnd);

    std::cout << "test1: chromosome=" << c.getString() << " f=" << c.fitness() << " fp=" << c.fitnessPercent() <<
                 " rouletteRange=(" << rangeStart << "," << rangeEnd << ")" << std::endl;
}

// check creature initialization (copy constructor)
void test2()
{
    char message[] = "This is a chromosome!";
    std::vector<char> chromosome;
    for (char &c : message)
        chromosome.push_back(c);

    CreatureType c1(chromosome);
    float rangeStart, rangeEnd;
    c1.rouletteRange(rangeStart, rangeEnd);

    std::cout << "test2: c1 chromosome=" << c1.getString() << " f=" << c1.fitness() << " fp=" << c1.fitnessPercent() <<
                 " rouletteRange=(" << rangeStart << "," << rangeEnd << ")" << std::endl;

    // invoke copy constructor
    CreatureType c2(c1);
    c2.rouletteRange(rangeStart, rangeEnd);

    std::cout << "test2: c2 chromosome=" << c2.getString() << " f=" << c2.fitness() << " fp=" << c2.fitnessPercent() <<
                 " rouletteRange=(" << rangeStart << "," << rangeEnd << ")" << std::endl;
}

// check creature initialization (copy assignment operator)
void test3()
{
    char message[] = "This is a chromosome!";
    std::vector<char> chromosome;
    for (char &c : message)
        chromosome.push_back(c);

    CreatureType c1(chromosome);
    float rangeStart, rangeEnd;
    c1.rouletteRange(rangeStart, rangeEnd);

    std::cout << "test3: c1 chromosome=" << c1.getString() << " f=" << c1.fitness() << " fp=" << c1.fitnessPercent() <<
                 " rouletteRange=(" << rangeStart << "," << rangeEnd << ")" << std::endl;

    // invoke copy assignment operator
    CreatureType c2;
    c2 = c1;
    c2.rouletteRange(rangeStart, rangeEnd);

    std::cout << "test3: c2 chromosome=" << c2.getString() << " f=" << c2.fitness() << " fp=" << c2.fitnessPercent() <<
                 " rouletteRange=(" << rangeStart << "," << rangeEnd << ")" << std::endl;
}

// change genes to specific values
void test4()
{
    CreatureType c;
    c.setGene(0, 65); // letter A
    c.setGene(1, 66); // letter B
    c.setGene(2, 67); // letter C

    std::cout << "test4: chromosome=" << c.getString() << std::endl;
}

// check gene mutation
void test5()
{
    CreatureType c;
    std::cout << "test5: chromosome=" << c.getString() << std::endl;
    std::cout << "test5: gene[0]=" << c.gene(0) << std::endl;

    c.mutateGene(0);
    std::cout << "test5: gene[0]=" << c.gene(0) << " (mutated)" << std::endl;
    std::cout << "test5: chromosome=" << c.getString() << std::endl;
}

// check other creature setters
void test6()
{
    CreatureType c;
    std::cout << "test6: chromosome=" << c.getString() << std::endl;

    c.setFitnessPercent(0.5f);
    c.setRouletteRange(1.5f, 3.0f);
    std::cout << "test6: " << c.stats() << std::endl;
}

// validate creature copy
void test7()
{
    CreatureType orig;
    std::cout << "test7: Orig   : " << orig.stats() << std::endl;

    CreatureType copy = orig;
    copy.mutateGene(0);

    std::cout << "test7: Orig   : " << orig.stats() << std::endl;
    std::cout << "test7: Mutated: " << copy.stats() << std::endl;
}

// validate fitness for predefined chromosome
void test8()
{
    char message[] = "'Never underestimate a droid' - General Leia Organa, The Roar of Skywalker";
    std::vector<char> chromosome;
    for (char &c : message)
        if (c != '\0')
            chromosome.push_back(c);

    CreatureType c(chromosome);
    std::cout << "test8: chromosome=" << c.getString() << std::endl;

    float fitness = c.fitness();
    std::cout << "test8: fitness=" << fitness << std::endl; // expected: 0.9605
}

// validate GA crossover
void test9()
{
    char message1[] = "ABCDEFGH";
    std::vector<char> chromosome1;
    for (char &c : message1)
        if (c != '\0')
            chromosome1.push_back(c);

    CreatureType father(chromosome1);
    std::cout << "test9: Father: " << father.stats() << std::endl;

    char message2[] = "KLMNOPQR";
    std::vector<char> chromosome2;
    for (char &c : message2)
        if (c != '\0')
            chromosome2.push_back(c);

    CreatureType mother(chromosome2);
    std::cout << "test9: Mother: " << mother.stats() << std::endl;

    CreatureType childA, childB;

    GeneticAlgorithm<PopulationType, CreatureType> ga(0.8f, 0.1f);
    ga.crossover(father, mother, childA, childB);

    std::cout << "test9: ChildA: " << childA.stats() << std::endl;
    std::cout << "test9: ChildB: " << childB.stats() << std::endl;
}

// calculate fitness for random genes
void test10()
{
    CreatureType c;
    std::cout << "test10: chromosome=" << c.getString() << std::endl;

    float fitness =  c.fitness();
    std::cout << "test10: fitness=" << fitness << std::endl;
}

// fitness calculation must return different results for different chromosome combinations
void test11()
{
    CreatureType c1;
    c1.setGene(0, 50);
    c1.setGene(1, 0);
    c1.setGene(2, 150);
    float fitness1 = c1.fitness();

    CreatureType c2;
    c2.setGene(0, 0);
    c2.setGene(1, 0);
    c2.setGene(2, 0);
    float fitness2 = c2.fitness();

    std::cout << "test11: chromosome1=" << c1.getString() << " \tfitness1=" << fitness1 << std::endl;
    std::cout << "test11: chromosome2=" << c2.getString() << " \tfitness2=" << fitness2 << std::endl;
}

// population initialization
void test12()
{
    PopulationType pop = PopulationType(5);
    std::cout << "test12: pop.stats()\n" << pop.stats() << std::endl;
}

// genetic algorithm evolves one generation
void test13()
{
    PopulationType pop(2);
    std::cout << "test13: pop.stats()\n" << pop.stats() << std::endl;
    std::cout << "test13: population mean: " << pop.mean() << std::endl;

    GeneticAlgorithm<PopulationType, CreatureType> ga(0.8f, 0.5f);
    ga.evolve(pop);

    std::cout << "\ntest13: pop.stats():\n" << pop.stats() << std::endl;
    std::cout << "test13: population mean: " << pop.mean() << std::endl;
}

int main()
{
    std::cout << std::fixed << std::setprecision(3);

    test1();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test2();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test3();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test4();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test5();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test6();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test7();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test8();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test9();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test10();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test11();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test12();
    std::cout << "\n----------------------------------------\n" << std::endl;

    test13();

    return 0;
}
