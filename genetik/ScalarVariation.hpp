#pragma once
#include <algorithm>
#include <random>
#include <stdexcept>

template<class C> struct ScalarVariation {
    void crossover(const C& father, const C& mother, C& a, C& b, float rate, std::mt19937& rng) {
        const auto size = father.chromosome().size();
        if (size == 0 || size != mother.chromosome().size())
            throw std::invalid_argument("crossover requires equal nonempty chromosomes");
        // Copy parents before assigning outputs, allowing outputs to alias inputs.
        C left = father, right = mother;
        if (std::uniform_real_distribution<float>(0, 1)(rng) < rate) {
            const auto cut = std::uniform_int_distribution<std::size_t>(0, size - 1)(rng);
            for (auto i = cut; i < size; ++i) {
                left.setGene(i, mother.gene(i));
                right.setGene(i, father.gene(i));
            }
        }
        a = left;
        b = right;
    }
    C mutation(const C& creature, float rate, std::mt19937& rng) {
        const auto size = creature.chromosome().size();
        if (size == 0) throw std::invalid_argument("mutation requires a nonempty chromosome");
        C mutated = creature;
        if (std::uniform_real_distribution<float>(0, 1)(rng) >= rate)
            return mutated;
        const auto count = std::uniform_int_distribution<std::size_t>(1,
            std::max<std::size_t>(1, size / 100))(rng);
        for (std::size_t i = 0; i < count; ++i) {
            const auto pos = std::uniform_int_distribution<std::size_t>(0, size - 1)(rng);
            mutated.mutateGene(pos, rng);
        }
        return mutated;
    }
};
