#pragma once
#include "EvaluatedCandidate.hpp"
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Value-owned scalar genomes only. No writable views/references escape this base.
// Custom evaluators/variation read genes by value; editing methods cannot be overridden.
template <typename T>
class Creature_i : public EvaluatedCandidate {
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
            EvaluatedCandidate::operator=(other);
        }
        return *this;
    }
    Creature_i(Creature_i&& other) noexcept
        : EvaluatedCandidate(std::move(other)), chromosome_(std::move(other.chromosome_)) {}
    Creature_i& operator=(Creature_i&& other) noexcept {
        if (this != &other) {
            chromosome_ = std::move(other.chromosome_);
            EvaluatedCandidate::operator=(std::move(other));
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
protected:
    virtual T mutatedGene(unsigned int pos, std::mt19937& rng) const = 0;
private:
    std::vector<T> chromosome_;
};
