#pragma once
#include <cmath>
#include <stdexcept>

enum class FitnessState { Unevaluated, Valid, Invalid };
enum class ObjectiveDirection { Maximize, Minimize };
enum class GenerationOutcome { Committed, NoViableCandidates, OperationalFailure };

// A score exists only in the Valid state. Neither ordering nor selection lives here.
class Fitness {
public:
    Fitness() = default;
    static Fitness valid(double score) {
        if (!std::isfinite(score)) throw std::invalid_argument("valid fitness must be finite");
        return Fitness(FitnessState::Valid, score);
    }
    static Fitness invalid() noexcept { return Fitness(FitnessState::Invalid, 0); }
    // Adapter for numerical evaluators: non-finite output is mathematical invalidity.
    static Fitness fromScore(double score) {
        return std::isfinite(score) ? valid(score) : invalid();
    }
    FitnessState state() const noexcept { return state_; }
    double score() const {
        if (state_ != FitnessState::Valid) throw std::logic_error("fitness has no valid score");
        return score_;
    }
    bool operator==(const Fitness&) const = default;
private:
    Fitness(FitnessState state, double score) noexcept : state_(state), score_(score) {}
    FitnessState state_ = FitnessState::Unevaluated;
    double score_ = 0;
};
