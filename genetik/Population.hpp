#pragma once
#include "Population_i.hpp"
#include "RandomContext.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

// Maximizes finite, nonnegative fitness. Call updateValues() after setCreature().
template <typename T>
class Population : public Population_i<T> {
public:
    Population(const unsigned int& size, RandomContext& random)
        : Population_i<T>(size), random_(random) { setSize(size); }

    RandomContext& randomContext() const noexcept { return random_; }

    void setSize(const unsigned int& size) override {
        if (size == 0) throw std::invalid_argument("population must be nonempty");
        std::vector<T> fresh;
        fresh.reserve(size);
        for (unsigned int i = 0; i < size; ++i) fresh.emplace_back(random_.engine());
        // Evaluate before committing so a failed resize preserves the old population.
        for (auto& c : fresh) {
            c.calculateFitness();
            validateFitness(c.fitness());
        }
        this->_creatures = std::move(fresh);
        this->_populationSize = size;
        _computeFitnessPercent();
        _computeRouletteRange();
    }

    // Compatibility: an independent snapshot, never a mutable view of the population.
    std::shared_ptr<std::vector<T>> creatures() const override {
        return std::make_shared<std::vector<T>>(this->_creatures);
    }
    float mean() override {
        double sum = 0;
        for (const auto& c : this->_creatures) sum += c.fitness();
        return static_cast<float>(sum / this->_creatures.size());
    }
    T bestCreature() override { return this->_creatures.back(); }
    void setCreature(const unsigned int& pos, const T& creature) override {
        this->_creatures.at(pos) = creature;
    }
    void updateValues() override {
        _computeFitness();
        _computeFitnessPercent();
        _computeRouletteRange();
    }
    std::string stats() override {
        std::string out;
        for (unsigned int i = 0; i < this->_populationSize; ++i)
            out += "Creature #" + std::to_string(i) + " " + this->_creatures[i].stats() + "\n";
        return out;
    }
private:
    static void validateFitness(float value) {
        if (!std::isfinite(value) || value < 0)
            throw std::domain_error("fitness must be finite and nonnegative (maximization)");
    }
    void _computeFitness() override {
        for (auto& c : this->_creatures) {
            c.calculateFitness();
            validateFitness(c.fitness());
        }
    }
    void _computeFitnessPercent() override {
        double sum = 0;
        for (const auto& c : this->_creatures) sum += c.fitness();
        for (auto& c : this->_creatures)
            c.setFitnessPercent(static_cast<float>(sum == 0 ?
                100.0 / this->_creatures.size() : 100.0 * c.fitness() / sum));
    }
    void _computeRouletteRange() override {
        _sort();
        double cumulative = 0;
        for (std::size_t i = 0; i < this->_creatures.size(); ++i) {
            const float start = static_cast<float>(cumulative);
            cumulative = std::min(100.0, cumulative + this->_creatures[i].fitnessPercent());
            this->_creatures[i].setRouletteRange(start,
                i + 1 == this->_creatures.size() ? 100.0f : static_cast<float>(cumulative));
        }
    }
    void _sort() override {
        std::stable_sort(this->_creatures.begin(), this->_creatures.end(),
            [](const T& a, const T& b) { return a.fitness() < b.fitness(); });
    }
    RandomContext& random_;
};
