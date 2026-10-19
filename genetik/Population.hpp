#pragma once
#include "Population_i.hpp"
#include "RandomContext.hpp"
#include <algorithm>
#include <exception>
#include <limits>
#include <optional>
#include <stdexcept>

// Serial, value-owned committed state. All potentially throwing preparation precedes swap.
template <typename T>
class Population : public Population_i<T> {
    static_assert(std::is_base_of_v<Creature_i<typename T::gene_type>, T>,
                  "candidates must use the encapsulated Creature_i scalar genome");
    static_assert(std::is_nothrow_destructible_v<T>);
public:
    Population(const unsigned int& size, RandomContext& random,
               ObjectiveDirection direction = ObjectiveDirection::Maximize)
        : random_(random), direction_(direction) { validateDirection(); setSize(size); }
    Population(const std::vector<T>& initial, RandomContext& random,
               ObjectiveDirection direction = ObjectiveDirection::Maximize)
        : random_(random), direction_(direction) { validateDirection(); publishGeneration(initial); }
    Population(const Population&) = delete;
    Population& operator=(const Population&) = delete;
    Population(Population&&) = delete;
    Population& operator=(Population&&) = delete;

    RandomContext& randomContext() const noexcept { return random_; }
    ObjectiveDirection direction() const noexcept { return direction_; }
    GenerationOutcome lastOutcome() const noexcept { return outcome_; }
    std::optional<std::uint64_t> generation() const noexcept {
        return committed_ ? std::optional(committed_->generation) : std::nullopt;
    }
    // Best over committed generations only, also available while edits are pending.
    std::optional<T> bestSeen() const {
        return committed_ ? std::optional<T>(committed_->best) : std::nullopt;
    }
    GenerationOutcome setSize(const unsigned int& size) override {
        pending_.reset();
        try {
            if (size == 0) throw std::invalid_argument("population must be nonempty");
            std::vector<T> fresh;
            fresh.reserve(size);
            for (unsigned int i = 0; i < size; ++i) fresh.emplace_back(random_.engine());
            return publishGeneration(std::move(fresh));
        } catch (...) { outcome_ = GenerationOutcome::OperationalFailure; throw; }
    }
    std::shared_ptr<std::vector<T>> creatures() const override {
        return std::make_shared<std::vector<T>>(committed_ ? committed_->members : std::vector<T>{});
    }
    // Explicit score/selection boundary: pending edits and absent initial state are rejected.
    void requireReady() const {
        if (pending_) throw std::logic_error("population has pending edits; call updateValues()");
        if (!committed_) throw std::logic_error("no viable initial population has committed");
        requireEvaluated(committed_->members);
    }
    double mean() override {
        requireReady();
        // Incremental weighted sum avoids overflowing a sum of admissible finite scores.
        double mean = 0;
        std::size_t count = 0;
        for (const auto& c : committed_->members) if (valid(c)) {
            ++count;
            mean = mean * (static_cast<double>(count - 1) / count) + c.fitness() / count;
        }
        return mean; // mean of valid members only; count is nonzero in a committed state
    }
    T bestCreature() override { requireReady(); return committed_->members.back(); }
    void setCreature(const unsigned int& pos, const T& creature) override {
        if (!committed_) throw std::logic_error("no committed population to edit");
        try {
            if (!pending_) pending_ = std::make_unique<std::vector<T>>(committed_->members);
            pending_->at(pos) = creature;
            pending_->at(pos).invalidateFitness();
        } catch (...) { pending_.reset(); throw; }
    }
    GenerationOutcome updateValues() override {
        // Updating an unchanged population is still an explicitly requested evaluation attempt.
        if (pending_) {
            auto edits = std::move(pending_);
            return publishGeneration(std::move(*edits));
        }
        if (!committed_) return outcome_ = GenerationOutcome::NoViableCandidates;
        try { return publishGeneration(committed_->members); }
        catch (...) { outcome_ = GenerationOutcome::OperationalFailure; throw; }
    }
    std::string stats() override {
        std::string out;
        if (committed_) for (std::size_t i = 0; i < committed_->members.size(); ++i) {
            auto c = committed_->members[i];
            out += "Creature #" + std::to_string(i) + " " + c.stats() + "\n";
        }
        return out;
    }

    // Reevaluate all supplied candidates, including unchanged copies. No implicit retry.
    // On operational exceptions the original error is nested below candidate/attempt context.
    GenerationOutcome publishGeneration(const std::vector<T>& supplied) {
        pending_.reset();
        try {
            // Copy even rvalues: callers may retain mutable references to supplied candidates.
            auto next = supplied;
            if (next.empty()) throw std::invalid_argument("population must be nonempty");
            if (committed_ && committed_->generation == std::numeric_limits<std::uint64_t>::max())
                throw std::overflow_error("committed generation counter exhausted");
            for (std::size_t i = 0; i < next.size(); ++i) {
                random_.recordEvaluationAttempt();
                try { next[i].calculateFitness(); }
                catch (...) {
                    std::throw_with_nested(std::runtime_error("evaluation attempt " +
                        std::to_string(random_.evaluationAttempts()) + " at candidate " +
                        std::to_string(i) + " failed"));
                }
            }
            requireEvaluated(next);
            if (std::none_of(next.begin(), next.end(), valid))
                return outcome_ = GenerationOutcome::NoViableCandidates;
            rank(next);
            // Prepare the archive too: copying a custom candidate may throw.
            T best = committed_ && !better(next.back().fitness(), committed_->best.fitness())
                ? committed_->best : next.back();
            auto ready = std::make_unique<State>(State{std::move(next), std::move(best),
                committed_ ? committed_->generation + 1 : 0});
            committed_.swap(ready); // noexcept publication of population, generation and archive
            outcome_ = GenerationOutcome::Committed;
            return outcome_;
        } catch (...) { outcome_ = GenerationOutcome::OperationalFailure; throw; }
    }
private:
    struct State { std::vector<T> members; T best; std::uint64_t generation; };
    static bool valid(const T& c) { return c.fitnessState() == FitnessState::Valid; }
    static void requireEvaluated(const std::vector<T>& members) {
        for (const auto& c : members) {
            if (c.fitnessState() == FitnessState::Unevaluated)
                throw std::logic_error("unevaluated candidate at score boundary");
            if (valid(c) && !std::isfinite(c.fitness()))
                throw std::logic_error("non-finite valid score at score boundary");
        }
    }
    void validateDirection() const {
        if (direction_ != ObjectiveDirection::Maximize && direction_ != ObjectiveDirection::Minimize)
            throw std::invalid_argument("unknown objective direction");
    }
    bool better(double a, double b) const {
        return direction_ == ObjectiveDirection::Maximize ? a > b : a < b;
    }
    void rank(std::vector<T>& members) const {
        // Invalid first, valid from worst to best. Never compare an invalid numerical score.
        std::stable_sort(members.begin(), members.end(), [&](const T& a, const T& b) {
            if (valid(a) != valid(b)) return !valid(a);
            return valid(a) && better(b.fitness(), a.fitness());
        });
        const auto first = std::find_if(members.begin(), members.end(), valid);
        const auto n = static_cast<std::size_t>(members.end() - first);
        const double total = static_cast<double>(n) * (static_cast<double>(n) + 1) / 2;
        for (auto it = members.begin(); it != first; ++it) {
            it->setFitnessPercent(0); it->setRouletteRange(0, 0);
        }
        // Linear rank roulette, averaged ranks for ties. All valid candidates have positive weight.
        double cumulative = 0;
        for (std::size_t i = 0; i < n;) {
            std::size_t end = i + 1;
            while (end < n && first[i].fitness() == first[end].fitness()) ++end;
            const double weight = (static_cast<double>(i + 1) + end) / 2;
            for (; i < end; ++i) {
                const auto start = static_cast<float>(cumulative);
                cumulative += 100 * weight / total;
                first[i].setFitnessPercent(static_cast<float>(100 * weight / total));
                first[i].setRouletteRange(start, i + 1 == n ? 100.f : static_cast<float>(cumulative));
            }
        }
    }
    RandomContext& random_;
    const ObjectiveDirection direction_;
    std::unique_ptr<State> committed_;
    std::unique_ptr<std::vector<T>> pending_;
    GenerationOutcome outcome_ = GenerationOutcome::NoViableCandidates;
};
