#pragma once
#include "Population_i.hpp"
#include "detail/ScopedTiming.hpp"
#include "RandomContext.hpp"
#include <algorithm>
#include <exception>
#include <limits>
#include <optional>
#include <type_traits>
#include <stdexcept>

// Serial, value-owned committed state. All potentially throwing preparation precedes swap.
template<class T> struct FitnessOrdering {
    bool better(const T& a, const T& b, ObjectiveDirection direction) const {
        return direction == ObjectiveDirection::Maximize ? a.fitness() > b.fitness() : a.fitness() < b.fitness();
    }
};

template <typename T, typename Ordering = FitnessOrdering<T>>
class Population : public Population_i<T> {
    friend struct RunCheckpointAccess;
    static_assert(std::is_base_of_v<EvaluatedCandidate, T>,
                  "candidates must share the EvaluatedCandidate contract");
    static_assert(std::is_nothrow_destructible_v<T>);
public:
    Population(const unsigned int& size, RandomContext& random,
               ObjectiveDirection direction = ObjectiveDirection::Maximize, bool profilePublication = false)
        : random_(random), direction_(direction), profilePublication_(profilePublication) { validateDirection(); setSize(size); }
    Population(const std::vector<T>& initial, RandomContext& random,
               ObjectiveDirection direction = ObjectiveDirection::Maximize, bool profilePublication = false)
        : random_(random), direction_(direction), profilePublication_(profilePublication) { validateDirection(); publishGeneration(initial); }
    Population(const Population&) = delete;
    Population& operator=(const Population&) = delete;
    Population(Population&&) = delete;
    Population& operator=(Population&&) = delete;

    double publicationSeconds() const noexcept {return publicationSeconds_;}
    RandomContext& randomContext() const noexcept { return random_; }
    ObjectiveDirection direction() const noexcept { return direction_; }
    GenerationOutcome lastOutcome() const noexcept { return outcome_; }
    std::optional<std::uint64_t> generation() const noexcept {
        return committed_ ? std::optional(committed_->generation) : std::nullopt;
    }
    std::optional<std::uint64_t> bestGeneration() const noexcept {
        return committed_ ? std::optional(committed_->bestGeneration) : std::nullopt;
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
            if constexpr (std::is_constructible_v<T, std::mt19937&>) {
                for (unsigned int i = 0; i < size; ++i) fresh.emplace_back(random_.engine());
            } else throw std::invalid_argument("candidate requires configured vector initialization");
            return publishGeneration(std::move(fresh));
        } catch (...) { outcome_ = GenerationOutcome::OperationalFailure; throw; }
    }
    std::shared_ptr<std::vector<T>> creatures() const override {
        return std::make_shared<std::vector<T>>(committed_ ? committed_->members : std::vector<T>{});
    }
    // Read-only committed view, valid only until the next successful publication or
    // destruction. Do not retain it across engine operations. Snapshot via creatures()
    // when an independently owned value is required.
    const std::vector<T>& selectionView() const { requireReady(); return committed_->members; }
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
            const auto identity = next.front().problemIdentity();
            for (const auto& c : next)
                if (c.problemIdentity() != identity) throw std::invalid_argument("mixed evaluation problems");
            if (problemBound_ && identity != problem_)
                throw std::invalid_argument("problem changed: create a new run");
            if (!problemBound_) { problem_ = identity; problemBound_ = true; }
            if(random_.execution().requested()==1) {
                auto executionScope=random_.execution().serialScope();
            for (std::size_t i = 0; i < next.size(); ++i) {
                if(random_.execution().stopRequested())throw EvaluationCancelled();
                random_.recordEvaluationAttempt();
                try {
                    if constexpr(requires {next[i].setPreparationGeneration(std::uint64_t{});})
                        next[i].setPreparationGeneration(committed_ ? committed_->generation+1:0);
                    next[i].calculateFitness();
                }
                catch (...) {
                    std::throw_with_nested(std::runtime_error("evaluation attempt " +
                        std::to_string(random_.evaluationAttempts()) + " at candidate " +
                        std::to_string(i) + " failed"));
                }
            }
                if(random_.execution().stopRequested())throw EvaluationCancelled();
            } else {
                auto& executor=random_.execution();
                for(const auto& candidate:next)
                    if(!candidate.parallelEvaluationSafe())throw std::invalid_argument("candidate does not support parallel evaluation");
                if(next.size()>std::numeric_limits<std::uint64_t>::max()-random_.evaluationAttempts())
                    throw std::overflow_error("candidate counter cannot cover parallel batch");
                std::vector<unsigned char> began(next.size(),0);
                std::size_t detached=0;
                std::exception_ptr completionFailure;
                auto finish=[&]() noexcept {
                    for(std::size_t i=0;i<detached;++i)try{next[i].finishParallelEvaluation();}
                    catch(...){if(!completionFailure)completionFailure=std::current_exception();}
                };
                try {
                    for(auto& candidate:next){
                        if constexpr(requires {candidate.setPreparationGeneration(std::uint64_t{});})
                            candidate.setPreparationGeneration(committed_ ? committed_->generation+1:0);
                        candidate.beginParallelEvaluation(executor);++detached;
                    }
                }catch(...){finish();throw;}
                std::exception_ptr failure;
                // Count only calls that actually start. Workers never touch RandomContext.
                try {
                    executor.run(next.size(),[&](std::size_t i){
                        began[i]=1;
                        try{next[i].calculateFitness();}
                        catch(...){std::throw_with_nested(std::runtime_error("parallel candidate slot "+std::to_string(i)+" failed"));}
                    });
                }catch(...){failure=std::current_exception();}
                finish();
                for(auto started:began)if(started)random_.recordEvaluationAttempt();
                if(completionFailure && (!failure || isEvaluationStop(failure)))std::rethrow_exception(completionFailure);
                if(failure)std::rethrow_exception(failure);
            }
            genetik_detail::ScopedTiming publicationTimer(profilePublication_ ? &publicationSeconds_:nullptr);
            requireEvaluated(next);
            if (std::none_of(next.begin(), next.end(), valid))
                return outcome_ = GenerationOutcome::NoViableCandidates;
            rank(next);
            // Prepare the archive too: copying a custom candidate may throw.
            const bool improved = !committed_ || better(next.back(), committed_->best);
            T best = improved ? next.back() : committed_->best;
            const auto generation = committed_ ? committed_->generation + 1 : 0;
            auto ready = std::make_unique<State>(State{std::move(next), std::move(best),
                generation, improved ? generation : committed_->bestGeneration});
            committed_.swap(ready); // noexcept publication of population, generation and archive
            outcome_ = GenerationOutcome::Committed;
            return outcome_;
        } catch (...) { outcome_ = GenerationOutcome::OperationalFailure; throw; }
    }
private:
    Population(RandomContext& random,ObjectiveDirection direction,std::nullptr_t)
        :random_(random),direction_(direction){validateDirection();}
    struct State { std::vector<T> members; T best; std::uint64_t generation; std::uint64_t bestGeneration; };
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
        if constexpr (requires { ordering_.validateDirection(direction_); }) ordering_.validateDirection(direction_);
    }
    bool better(const T& a, const T& b) const { return ordering_.better(a, b, direction_); }
    void rank(std::vector<T>& members) const {
        // Invalid first, valid from worst to best. Never compare an invalid numerical score.
        std::stable_sort(members.begin(), members.end(), [&](const T& a, const T& b) {
            if (valid(a) != valid(b)) return !valid(a);
            return valid(a) && better(b, a);
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
            while (end < n && !better(first[i], first[end]) && !better(first[end], first[i])) ++end;
            const double weight = (static_cast<double>(i + 1) + end) / 2;
            for (; i < end; ++i) {
                const auto start = static_cast<float>(cumulative);
                cumulative += 100 * weight / total;
                first[i].setFitnessPercent(static_cast<float>(100 * weight / total));
                first[i].setRouletteRange(start, i + 1 == n ? 100.f : static_cast<float>(cumulative));
            }
        }
    }
    Ordering ordering_;
    std::shared_ptr<const void> problem_;
    bool problemBound_ = false;
    RandomContext& random_;
    const ObjectiveDirection direction_;
    bool profilePublication_=false;
    double publicationSeconds_=0;
    std::unique_ptr<State> committed_;
    std::unique_ptr<std::vector<T>> pending_;
    GenerationOutcome outcome_ = GenerationOutcome::NoViableCandidates;
};
