#pragma once
#include "GeneticAlgorithm_i.hpp"
#include <algorithm>
#include <exception>
#include <limits>
#include <stdexcept>
#include <vector>

// One evolve() call is one bounded generation; callers own the stopping policy.
template <typename P, typename C>
class GeneticAlgorithm : public GeneticAlgorithm_i<P, C> {
public:
    GeneticAlgorithm(const float& crossoverRate, const float& mutationRate, RandomContext& random)
        : GeneticAlgorithm_i<P, C>(crossoverRate, mutationRate, random) {}

    GenerationOutcome lastOutcome() const noexcept { return outcome_; }
    GenerationOutcome evolve(P& population) override {
        try {
            if (&population.randomContext() != &this->_random)
                throw std::invalid_argument("population and algorithm must share a run context");
            if (this->_evolutions == std::numeric_limits<unsigned int>::max())
                throw std::overflow_error("generation counter exhausted");
            if (!population.generation())
                return outcome_ = GenerationOutcome::NoViableCandidates;
            population.requireReady();
            const auto snapshot = population.creatures();
            std::vector<C> next;
            next.reserve(snapshot->size());
            while (next.size() < snapshot->size()) {
                const C father = select(*snapshot);
                const C mother = select(*snapshot);
                C a = father, b = mother;
                crossover(father, mother, a, b);
                next.push_back(mutation(a));
                if (next.size() < snapshot->size()) next.push_back(mutation(b));
            }
            outcome_ = population.publishGeneration(std::move(next));
            if (outcome_ == GenerationOutcome::Committed) ++this->_evolutions;
            return outcome_;
        } catch (...) {
            outcome_ = GenerationOutcome::OperationalFailure;
            std::throw_with_nested(std::runtime_error("generation attempt after " +
                std::to_string(this->_evolutions) + " completed evolutions failed"));
        }
    }
    unsigned int evolutions() override { return this->_evolutions; }

    void crossover(const C& father, const C& mother, C& a, C& b) override {
        const auto size = father.chromosome().size();
        if (size == 0 || size != mother.chromosome().size())
            throw std::invalid_argument("crossover requires equal nonempty chromosomes");
        // Copy parents before assigning outputs, allowing outputs to alias inputs.
        C left = father, right = mother;
        if (std::uniform_real_distribution<float>(0, 1)(this->_rng) < this->_crossoverRate) {
            const auto cut = std::uniform_int_distribution<std::size_t>(0, size - 1)(this->_rng);
            for (auto i = cut; i < size; ++i) {
                left.setGene(i, mother.gene(i));
                right.setGene(i, father.gene(i));
            }
        }
        a = left;
        b = right;
    }
    C mutation(const C& creature) override {
        const auto size = creature.chromosome().size();
        if (size == 0) throw std::invalid_argument("mutation requires a nonempty chromosome");
        C mutated = creature;
        if (std::uniform_real_distribution<float>(0, 1)(this->_rng) >= this->_mutationRate)
            return mutated;
        const auto count = std::uniform_int_distribution<std::size_t>(1,
            std::max<std::size_t>(1, size / 100))(this->_rng);
        for (std::size_t i = 0; i < count; ++i) {
            const auto pos = std::uniform_int_distribution<std::size_t>(0, size - 1)(this->_rng);
            mutated.mutateGene(pos, this->_rng);
        }
        return mutated;
    }
    C roulette(const P& population) override {
        if (&population.randomContext() != &this->_random)
            throw std::invalid_argument("population and algorithm must share a run context");
        population.requireReady();
        return select(*population.creatures());
    }
private:
    GenerationOutcome outcome_ = GenerationOutcome::NoViableCandidates;
    C select(const std::vector<C>& creatures) {
        if (creatures.empty()) throw std::invalid_argument("population must be nonempty");
        for (const auto& c : creatures)
            if (c.fitnessState() == FitnessState::Unevaluated)
                throw std::logic_error("unevaluated parent candidate");
        const float draw = std::uniform_real_distribution<float>(0, 100)(this->_rng);
        for (const auto& c : creatures) {
            float start, end;
            c.rouletteRange(start, end);
            if (c.fitnessState() == FitnessState::Valid && draw >= start && draw < end) return c;
        }
        throw std::domain_error("selection ranges do not cover draw; call updateValues()");
    }
};
