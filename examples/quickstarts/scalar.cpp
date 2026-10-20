#include <Creature_i.hpp>
#include <Population.hpp>
#include <GeneticAlgorithm.hpp>
#include <iostream>
#include <random>

// Minimize squared distance to 0.25. Each candidate owns one real-valued gene.
class Parameter final : public Creature_i<double> {
public:
    explicit Parameter(std::mt19937& engine)
        : Creature_i<double>({std::uniform_real_distribution<double>(-1,1)(engine)}) {}
    std::string stats() override {return std::to_string(gene(0));}
protected:
    Fitness evaluate() const override {
        const double residual=gene(0)-.25;
        return Fitness::fromScore(residual*residual);
    }
    double mutatedGene(unsigned int, std::mt19937& engine) const override {
        return std::uniform_real_distribution<double>(-1,1)(engine);
    }
};
int main() {
    RandomContext random(42); // outlives population and algorithm
    Population<Parameter> population(16,random,ObjectiveDirection::Minimize);
    GeneticAlgorithm<decltype(population),Parameter> algorithm(.9f,.3f,random);
    for(unsigned generation=0;generation<10;++generation) {
        if(algorithm.evolve(population)!=GenerationOutcome::Committed) {
            std::cout<<"stopping_reason=no_viable_candidates\n";
            return 0;
        }
    }
    const auto best=population.bestSeen();
    if(!best || random.evaluationAttempts()!=16*11)return 1;
    std::cout<<"seed="<<random.seed()<<" x="<<best->gene(0)<<" loss="<<best->fitness()
             <<" generations="<<algorithm.evolutions()<<" evaluations="<<random.evaluationAttempts()
             <<" stopping_reason=generation_limit\n";
    return 0; // the declared bound is a normal stop; exact discovery is unnecessary
}
