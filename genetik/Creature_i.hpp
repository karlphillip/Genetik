#pragma once
#include "Fitness.hpp"
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Value-owned scalar genomes only. No writable views/references escape this base.
// Custom evaluators/variation read genes by value; editing methods cannot be overridden.
template <typename T>
class Creature_i {
    static_assert(std::is_arithmetic_v<T>, "scalar genomes require arithmetic genes");
public:
    using gene_type = T;
    Creature_i() = default;
    explicit Creature_i(const std::vector<T>& chromosome) : chromosome_(chromosome) {}
    Creature_i(const Creature_i&) = default;
    Creature_i& operator=(const Creature_i& other) {
        if (this != &other) {
            auto copy = other.chromosome_; // allocate before changing either genome or cache
            chromosome_.swap(copy);
            fitness_ = other.fitness_;
            percent_ = other.percent_; start_ = other.start_; end_ = other.end_;
        }
        return *this;
    }
    Creature_i(Creature_i&& other) noexcept
        : chromosome_(std::move(other.chromosome_)), fitness_(other.fitness_),
          percent_(other.percent_), start_(other.start_), end_(other.end_) { other.invalidateFitness(); }
    Creature_i& operator=(Creature_i&& other) noexcept {
        if (this != &other) {
            chromosome_ = std::move(other.chromosome_);
            fitness_ = other.fitness_;
            percent_ = other.percent_; start_ = other.start_; end_ = other.end_;
            other.invalidateFitness();
        }
        return *this;
    }
    virtual ~Creature_i() = default;
    virtual std::vector<T> chromosome() const final { return chromosome_; }
    virtual T gene(const unsigned int& pos) const final { return chromosome_.at(pos); }
    virtual void setGene(const unsigned int& pos, const T& value) final {
        chromosome_.at(pos); // a rejected edit does not change genome or validity
        invalidateFitness();
        chromosome_[pos] = value;
    }
    virtual void mutateGene(const unsigned int& pos, std::mt19937& rng) final {
        chromosome_.at(pos);
        const T value = mutatedGene(pos, rng);
        setGene(pos, value);
    }
    virtual void replaceChromosome(const std::vector<T>& chromosome) final {
        // Copy even rvalues: a caller may retain writable references into their buffer.
        auto copy = chromosome;
        invalidateFitness();
        chromosome_.swap(copy);
    }
    virtual void invalidateFitness() noexcept final {
        fitness_ = Fitness{};
        percent_ = start_ = end_ = 0;
    }
    virtual void calculateFitness() final {
        invalidateFitness();
        const Fitness result = evaluate();
        if (result.state() == FitnessState::Unevaluated)
            throw std::logic_error("evaluator returned Unevaluated instead of Valid or Invalid");
        fitness_ = result;
    }
    virtual Fitness evaluation() const noexcept final { return fitness_; }
    virtual FitnessState fitnessState() const noexcept final { return fitness_.state(); }
    virtual double fitness() const final { return fitness_.score(); }
    virtual void setFitnessPercent(const float& value) final { percent_ = value; }
    virtual float fitnessPercent() const final { return percent_; }
    virtual void setRouletteRange(const float& start, const float& end) final { start_ = start; end_ = end; }
    virtual void rouletteRange(float& start, float& end) const final { start = start_; end = end_; }
    virtual std::string stats() = 0;
protected:
    virtual Fitness evaluate() const = 0;
    virtual T mutatedGene(unsigned int pos, std::mt19937& rng) const = 0;
private:
    std::vector<T> chromosome_;
    Fitness fitness_;
    float percent_ = 0, start_ = 0, end_ = 0;
};
