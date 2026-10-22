#pragma once
#include "GeneticAlgorithm_i.hpp"
#include "ScalarVariation.hpp"
#include <algorithm>
#include <exception>
#include <limits>
#include <stdexcept>
#include <vector>

// One evolve() call is one bounded generation; callers own the stopping policy.
template <typename P, typename C, typename Variation = ScalarVariation<C>>
class GeneticAlgorithm : public GeneticAlgorithm_i<P, C> {
    friend struct RunCheckpointAccess;
public:
    GeneticAlgorithm(const float& crossoverRate, const float& mutationRate, RandomContext& random,
                     Variation variation = {}, std::size_t eliteCount = 0)
        : GeneticAlgorithm_i<P, C>(crossoverRate, mutationRate, random),
          variation_(std::move(variation)), elites_(eliteCount) {}

    Variation& variation() noexcept { return variation_; }
    const Variation& variation() const noexcept { return variation_; }
    std::uint64_t attempts() const noexcept { return attempts_; }
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
            if (attempts_ == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("generation attempts exhausted");
            ++attempts_;
            const auto snapshot = population.creatures();
            if (elites_ >= snapshot->size()) throw std::invalid_argument("elite count must be smaller than population");
            std::vector<C> next;
            next.reserve(snapshot->size());
            for (std::size_t i = 0; i < elites_; ++i) {
                const auto& elite = (*snapshot)[snapshot->size() - 1 - i];
                if (elite.fitnessState() != FitnessState::Valid) break;
                next.push_back(elite);
                if constexpr(requires {variation_.markElite(next.back());}) variation_.markElite(next.back());
            }
            while (next.size() < snapshot->size()) {
                const C father = roulette(population);
                const C mother = roulette(population);
                if (father.fitnessState() != FitnessState::Valid || mother.fitnessState() != FitnessState::Valid)
                    throw std::logic_error("selection must return valid evaluated parents");
                if (father.problemIdentity() != snapshot->front().problemIdentity() ||
                    mother.problemIdentity() != snapshot->front().problemIdentity())
                    throw std::invalid_argument("selected parent belongs to another problem");
                C a = father, b = mother;
                crossover(father, mother, a, b);
                next.push_back(mutation(a));
                if constexpr(requires {variation_.markOffspring(next.back(),father);}) variation_.markOffspring(next.back(),father);
                if (next.size() < snapshot->size()) {
                    next.push_back(mutation(b));
                    if constexpr(requires {variation_.markOffspring(next.back(),mother);}) variation_.markOffspring(next.back(),mother);
                }
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
        variation_.crossover(father, mother, a, b, this->_crossoverRate, this->_rng);
    }
    C mutation(const C& creature) override {
        return variation_.mutation(creature, this->_mutationRate, this->_rng);
    }
    C roulette(const P& population) override {
        if (&population.randomContext() != &this->_random)
            throw std::invalid_argument("population and algorithm must share a run context");
        population.requireReady();
        if constexpr (requires { population.selectionView(); })
            return select(population.selectionView());
        else return select(*population.creatures());
    }
private:
    Variation variation_;
    std::size_t elites_;
    std::uint64_t attempts_ = 0;
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
