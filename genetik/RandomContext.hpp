#pragma once
#include "Execution.hpp"
#include <cstdint>
#include <random>
#include <limits>
#include <stdexcept>

// Own one per serial run. It must outlive every population and algorithm using it.
// Deliberately not copyable/movable: consumers borrow its engine.
class RandomContext {
public:
    explicit RandomContext(std::uint32_t seed, ExecutionConfig execution={})
        : seed_(seed), engine_(seed), ownedExecution_(execution), execution_(&ownedExecution_) {}
    RandomContext(std::uint32_t seed, ExecutionContext& execution)
        : seed_(seed), engine_(seed), execution_(&execution) {}
    ExecutionContext& execution() noexcept {return *execution_;}
    const ExecutionContext& execution() const noexcept {return *execution_;}
    RandomContext(const RandomContext&) = delete;
    RandomContext& operator=(const RandomContext&) = delete;
    RandomContext(RandomContext&&) = delete;
    RandomContext& operator=(RandomContext&&) = delete;
    std::uint32_t seed() const noexcept { return seed_; }
    std::mt19937& engine() noexcept { return engine_; }
    std::uint64_t evaluationAttempts() const noexcept { return evaluationAttempts_; }
    // Called immediately before each population-driven evaluator invocation.
    void recordEvaluationAttempt() {
        if (evaluationAttempts_ == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("evaluation attempt counter exhausted");
        ++evaluationAttempts_;
    }
private:
    std::uint64_t evaluationAttempts_ = 0;
    const std::uint32_t seed_;
    std::mt19937 engine_;
    ExecutionContext ownedExecution_;
    ExecutionContext* execution_;
};
