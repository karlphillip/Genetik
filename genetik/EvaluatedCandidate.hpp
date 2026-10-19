#pragma once
#include "Fitness.hpp"
#include <memory>
#include <string>
#include <utility>

// Shared evaluation/cache contract; representations own their genomes separately.
class EvaluatedCandidate {
protected:
    EvaluatedCandidate() = default;
    EvaluatedCandidate(const EvaluatedCandidate&) = default;
    EvaluatedCandidate& operator=(const EvaluatedCandidate&) = default;
    EvaluatedCandidate(EvaluatedCandidate&& other) noexcept { swapEvaluation(other); }
    EvaluatedCandidate& operator=(EvaluatedCandidate&& other) noexcept {
        if (this != &other) { invalidateFitness(); swapEvaluation(other); }
        return *this;
    }
public:
    virtual ~EvaluatedCandidate() = default;
    virtual void invalidateFitness() noexcept final {
        fitness_ = Fitness{};
        percent_ = start_ = end_ = 0;
    }
    virtual void calculateFitness() final {
        invalidateFitness();
        const Fitness result = prepareEvaluation();
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
    virtual std::shared_ptr<const void> problemIdentity() const noexcept { return {}; }
    virtual std::string stats() = 0;
protected:
    // Runs only on the private staged value during population publication.
    virtual Fitness prepareEvaluation() { return evaluate(); }
    virtual Fitness evaluate() const = 0;
    void swapEvaluation(EvaluatedCandidate& other) noexcept {
        std::swap(fitness_, other.fitness_); std::swap(percent_, other.percent_);
        std::swap(start_, other.start_); std::swap(end_, other.end_);
    }
private:
    Fitness fitness_;
    float percent_ = 0, start_ = 0, end_ = 0;
};
