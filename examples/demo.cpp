#include "GeneticAlgorithm.hpp"
#include "Population.hpp"
#include "Creature.hpp"
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>

using CreatureType = Creature<char>;
using PopulationType = Population<CreatureType>;

static std::uint32_t number(const char* arg) {
    const std::string_view text(arg);
    std::uint32_t value;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
        throw std::invalid_argument("arguments must be unsigned 32-bit integers");
    return value;
}
static void printError(const std::exception& error) {
    std::cerr << error.what() << '\n';
    try { std::rethrow_if_nested(error); }
    catch (const std::exception& nested) { printError(nested); }
    catch (...) { std::cerr << "non-standard underlying exception\n"; }
}
int main(int argc, char** argv) {
    try {
        if (argc > 3) throw std::invalid_argument("usage: genetik_demo [seed [max_generations]]");
        const auto seed = argc > 1 ? number(argv[1]) : 42u;
        const auto limit = argc > 2 ? number(argv[2]) : 1000u;
        const auto start = std::chrono::steady_clock::now();
        RandomContext random(seed);
        PopulationType population(100, random);
        GeneticAlgorithm<PopulationType, CreatureType> ga(0.5f, 0.9f, random);
        std::cout << std::fixed << std::setprecision(3)
                  << "seed=" << random.seed() << " population=100 crossover=0.5 mutation=0.9"
                  << " selection=linear_rank objective=maximize max_generations=" << limit << " mode=serial\n";
        auto best = population.bestSeen().value();
        bool noViable = false;
        while (ga.evolutions() < limit && best.fitness() < 1.0f) {
            if (ga.evolve(population) != GenerationOutcome::Committed) {
                noViable = true;
                break;
            }
            best = population.bestSeen().value();
            std::cout << "* Generation #" << ga.evolutions() << " avg fitness: " << population.mean()
                      << " best fitness: " << population.bestCreature().fitness() << '\n';
        }
        std::cout << "best_seen: " << best.stats() << '\n'
                  << "stopping_reason=" << (noViable ? "no_viable_candidates" : best.fitness() == 1 ? "target_reached" : "generation_limit")
                  << " generations=" << ga.evolutions() << " evaluation_attempts=" << random.evaluationAttempts() << " elapsed_seconds="
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << '\n';
    } catch (const std::exception& error) {
        printError(error);
        return 1;
    }
}
