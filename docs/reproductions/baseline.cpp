// Reproduction for initial revision b8059f0 only; not a current API example.
#include "GeneticAlgorithm.hpp"
#include "Population.hpp"
#include "Creature.hpp"
int main() {
    using C = Creature<char>; using P = Population<C>;
    std::string target = TARGET;
    C perfect(std::vector<char>(target.begin(), target.end()));
    perfect.calculateFitness();
    perfect.setGene(0, '?');
    std::cout << "stale_fitness=" << perfect.fitness() << '\n';
    perfect.calculateFitness();
    C worst(std::vector<char>(74, '?')); worst.calculateFitness();
    P p(2); p.setCreature(0, worst); p.setCreature(1, perfect); p.updateValues();
    GeneticAlgorithm<P,C> ga(1,1);
    int low=0; for(int i=0;i<1000;++i) low += ga.roulette(p).fitness()==0;
    std::cout << "zero_selected=" << low << " of 1000\n";
    C almost(std::vector<char>(target.begin(), target.end())); almost.setGene(0, '?'); almost.calculateFitness();
    p.setCreature(0, almost); p.setCreature(1, perfect); p.updateValues();
    float start,end; p.creatures()->front().rouletteRange(start,end);
    std::cout << "equal_scores_first_range=" << start << "," << end << '\n';
    P singleton(1); singleton.setCreature(0, perfect); singleton.updateValues();
    std::cout << "singleton_best_matches_stored=" << (singleton.bestCreature().chromosome()==singleton.creatures()->at(0).chromosome()) << '\n';
    try { P empty(0); } catch(const std::exception& e) { std::cout << "zero_population=" << e.what() << '\n'; }
}
