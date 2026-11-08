#pragma once

#include "Regression.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct ParetoSelectionWork {
    std::uint64_t dominanceComparisons = 0;
    std::uint64_t selectionComparisons = 0;
    std::size_t peakEstimatedMemoryBytes = 0;
    bool operator==(const ParetoSelectionWork&) const = default;
};

class ParetoSelectionLimitExceeded final : public std::runtime_error {
public:
    explicit ParetoSelectionLimitExceeded(const char* message) : std::runtime_error(message) {}
};

// Optional symbolic-regression ordering policy. Prediction loss remains the
// candidate fitness; this policy only supplies an alternate selection score.
struct ComplexityPenaltyOrdering {
    explicit ComplexityPenaltyOrdering(double penalty) : penalty_(penalty) {
        if (!std::isfinite(penalty_) || penalty_ < 0.0)
            throw std::invalid_argument("complexity penalty must be finite and nonnegative");
    }

    void validateDirection(ObjectiveDirection direction) const {
        if (direction != ObjectiveDirection::Minimize)
            throw std::invalid_argument("regression requires minimization");
    }

    template <class Candidate>
    long double selectionScore(const Candidate& candidate) const noexcept {
        const auto penaltyCost = static_cast<long double>(penalty_) *
                                 static_cast<long double>(candidate.expression().nodeCount());
        const auto score = static_cast<long double>(candidate.fitness()) + penaltyCost;
        return std::isfinite(score) ? score : std::numeric_limits<long double>::max();
    }

    template <class Candidate>
    bool better(const Candidate& left, const Candidate& right, ObjectiveDirection direction) const {
        validateDirection(direction);
        const auto leftScore = selectionScore(left);
        const auto rightScore = selectionScore(right);
        if (leftScore != rightScore)
            return leftScore < rightScore;
        if (left.fitness() != right.fitness())
            return left.fitness() < right.fitness();
        if (left.expression().nodeCount() != right.expression().nodeCount())
            return left.expression().nodeCount() < right.expression().nodeCount();
        return left.text() < right.text();
    }

private:
    double penalty_;
};

// NSGA-II-style nondominated sorting, crowding-based survival, and rank/crowding
// parent weights. All tie breaks are stable and use expression text last.
struct ParetoRegressionOrdering {
    ParetoRegressionOrdering(std::uint64_t maximumComparisons,
                             std::size_t maximumEstimatedMemoryBytes, ParetoSelectionWork& work)
        : maximumComparisons_(maximumComparisons),
          maximumEstimatedMemoryBytes_(maximumEstimatedMemoryBytes), work_(&work) {
        if (!maximumComparisons_ || !maximumEstimatedMemoryBytes_)
            throw std::invalid_argument("Pareto work and memory limits must be positive");
    }

    void validateDirection(ObjectiveDirection direction) const {
        if (direction != ObjectiveDirection::Minimize)
            throw std::invalid_argument("regression requires minimization");
    }

    template <class Candidate>
    bool better(const Candidate& left, const Candidate& right, ObjectiveDirection direction) const {
        validateDirection(direction);
        if (left.fitnessState() != right.fitnessState())
            return left.fitnessState() == FitnessState::Valid;
        if (left.fitnessState() != FitnessState::Valid)
            return left.text() < right.text();
        if (left.fitness() != right.fitness())
            return left.fitness() < right.fitness();
        if (left.expression().nodeCount() != right.expression().nodeCount())
            return left.expression().nodeCount() < right.expression().nodeCount();
        return left.text() < right.text();
    }

    template <class Candidate>
    void checkEnvironmentalMemory(const std::vector<Candidate>& parents,
                                  const std::vector<Candidate>& offspring, std::size_t capacity) {
        (void)capacity;
        const auto count = checkedAdd(parents.size(), offspring.size());
        const auto bytes = estimateSelectionBytes(count, parents, offspring, true);
        recordMemory(bytes);
    }

    template <class Candidate>
    void environmentalSelect(std::vector<Candidate>& combined,
                             const std::vector<Candidate>& parents, std::size_t capacity) {
        (void)parents;
        if (combined.size() < capacity)
            throw std::logic_error("Pareto survivor pool is smaller than its capacity");
        const auto order = rankAndCrowding(combined);
        std::vector<std::size_t> selected;
        selected.reserve(capacity);
        std::vector<std::string> selectedStructures;
        selectedStructures.reserve(capacity);
        auto appendUnique = [&](std::size_t index) {
            for (const auto& structure : selectedStructures) {
                chargeComparison(false);
                if (structure == combined[index].text())
                    return;
            }
            selected.push_back(index);
            selectedStructures.push_back(combined[index].text());
        };
        auto wasSelected = [&](std::size_t index) {
            for (const auto selectedIndex : selected) {
                chargeComparison(false);
                if (selectedIndex == index)
                    return true;
            }
            return false;
        };
        std::size_t frontBegin = 0;
        while (frontBegin < order.fronts.size() && selected.size() < capacity) {
            auto front = order.fronts[frontBegin++];
            const auto remaining = capacity - selected.size();
            if (front.size() <= remaining) {
                std::sort(front.begin(), front.end(), [&](std::size_t a, std::size_t b) {
                    chargeComparison(false);
                    return better(combined[b], combined[a], ObjectiveDirection::Minimize);
                });
            } else {
                std::stable_sort(front.begin(), front.end(), [&](std::size_t a, std::size_t b) {
                    chargeComparison(false);
                    if (order.crowding[a] != order.crowding[b])
                        return order.crowding[a] > order.crowding[b];
                    return better(combined[b], combined[a], ObjectiveDirection::Minimize);
                });
            }
            for (const auto index : front) {
                appendUnique(index);
                if (selected.size() == capacity)
                    break;
            }
        }
        // A small initial population can contain fewer unique structures than
        // its requested size. Fill remaining slots deterministically afterward.
        for (std::size_t index = 0; index < combined.size() && selected.size() < capacity; ++index)
            if (combined[index].fitnessState() == FitnessState::Valid && !wasSelected(index))
                selected.push_back(index);
        for (std::size_t index = 0; index < combined.size() && selected.size() < capacity;
             ++index) {
            if (combined[index].fitnessState() != FitnessState::Valid && !wasSelected(index))
                selected.push_back(index);
        }
        std::vector<Candidate> survivors;
        survivors.reserve(capacity);
        for (const auto index : selected)
            survivors.push_back(std::move(combined[index]));
        std::stable_sort(survivors.begin(), survivors.end(),
                         [&](const Candidate& a, const Candidate& b) {
                             chargeComparison(false);
                             return better(b, a, ObjectiveDirection::Minimize);
                         });
        combined = std::move(survivors);
    }

    template <class Candidate>
    void rank(std::vector<Candidate>& candidates, ObjectiveDirection direction) {
        validateDirection(direction);
        const auto bytes =
            estimateSelectionBytes(candidates.size(), candidates, std::vector<Candidate>{}, false);
        recordMemory(bytes);
        std::stable_sort(candidates.begin(), candidates.end(),
                         [&](const Candidate& a, const Candidate& b) {
                             chargeComparison(false);
                             if (a.fitnessState() != b.fitnessState())
                                 return a.fitnessState() != FitnessState::Valid;
                             if (a.fitnessState() != FitnessState::Valid)
                                 return a.text() < b.text();
                             return better(b, a, direction);
                         });

        std::vector<double> weights(candidates.size(), 0.0);
        // Keep the established best-at-back convention while selection uses
        // the Pareto rank and crowding values.
        const auto sortedOrder = rankAndCrowding(candidates);
        std::vector<std::size_t> frontRank(candidates.size(), 0);
        for (std::size_t rank = 0; rank < sortedOrder.fronts.size(); ++rank)
            for (const auto index : sortedOrder.fronts[rank])
                frontRank[index] = rank;
        std::size_t validCount = 0;
        for (const auto& candidate : candidates)
            if (candidate.fitnessState() == FitnessState::Valid)
                ++validCount;
        double totalWeight = 0.0;
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            if (candidates[index].fitnessState() != FitnessState::Valid)
                continue;
            const double diversity = std::isinf(sortedOrder.crowding[index])
                                         ? 1.0
                                         : std::min(1.0, sortedOrder.crowding[index]);
            const double frontWeight =
                static_cast<double>(sortedOrder.fronts.size() - frontRank[index]);
            weights[index] = frontWeight + diversity;
            totalWeight += weights[index];
        }
        double cumulative = 0.0;
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            if (candidates[index].fitnessState() != FitnessState::Valid) {
                candidates[index].setFitnessPercent(0);
                candidates[index].setRouletteRange(0, 0);
                continue;
            }
            const float start = static_cast<float>(cumulative);
            cumulative += 100.0 * weights[index] / totalWeight;
            const float end = validCount && index == candidates.size() - 1
                                  ? 100.0f
                                  : static_cast<float>(cumulative);
            candidates[index].setFitnessPercent(
                static_cast<float>(100.0 * weights[index] / totalWeight));
            candidates[index].setRouletteRange(start, end);
        }
    }

    template <class Candidate>
    std::vector<Candidate> frontier(const std::vector<Candidate>& candidates,
                                    std::size_t maximumPoints) {
        if (!maximumPoints)
            throw std::invalid_argument("Pareto frontier limit must be positive");
        const auto bytes =
            estimateSelectionBytes(candidates.size(), candidates, std::vector<Candidate>{}, false);
        recordMemory(bytes);
        const auto ranked = rankAndCrowding(candidates);
        if (ranked.fronts.empty())
            return {};
        auto firstFront = ranked.fronts.front();
        std::stable_sort(
            firstFront.begin(), firstFront.end(), [&](std::size_t left, std::size_t right) {
                chargeComparison(false);
                if (ranked.crowding[left] != ranked.crowding[right])
                    return ranked.crowding[left] > ranked.crowding[right];
                return better(candidates[left], candidates[right], ObjectiveDirection::Minimize);
            });
        std::vector<std::size_t> selectedIndices;
        selectedIndices.reserve(std::min(maximumPoints, firstFront.size()));
        for (const auto index : firstFront) {
            if (std::any_of(selectedIndices.begin(), selectedIndices.end(), [&](std::size_t item) {
                    chargeComparison(false);
                    return candidates[item].text() == candidates[index].text();
                }))
                continue;
            selectedIndices.push_back(index);
            if (selectedIndices.size() == maximumPoints)
                break;
        }
        constexpr auto pointBytes = sizeof(Candidate) + sizeof(double) + sizeof(std::size_t);
        if (selectedIndices.size() > std::numeric_limits<std::size_t>::max() / pointBytes)
            throw ParetoSelectionLimitExceeded("Pareto frontier memory estimate overflow");
        std::size_t outputBytes = selectedIndices.size() * pointBytes;
        for (const auto index : selectedIndices) {
            const auto nodes = candidates[index].expression().nodeCount();
            const auto textBytes = candidates[index].text().size();
            constexpr auto nodeBytes = sizeof(ExpressionNode) + sizeof(char);
            if (textBytes > std::numeric_limits<std::size_t>::max() - outputBytes ||
                nodes >
                    (std::numeric_limits<std::size_t>::max() - outputBytes - textBytes) / nodeBytes)
                throw ParetoSelectionLimitExceeded("Pareto frontier memory estimate overflow");
            outputBytes += textBytes + nodes * nodeBytes;
        }
        if (outputBytes > std::numeric_limits<std::size_t>::max() / 2)
            throw ParetoSelectionLimitExceeded("Pareto frontier memory estimate overflow");
        const auto frontierBytes = work_->peakEstimatedMemoryBytes + 2 * outputBytes;
        if (frontierBytes < work_->peakEstimatedMemoryBytes)
            throw ParetoSelectionLimitExceeded("Pareto frontier memory estimate overflow");
        recordMemory(frontierBytes);
        if (frontierBytes > maximumEstimatedMemoryBytes_)
            throw ParetoSelectionLimitExceeded("Pareto frontier memory limit exhausted");

        std::vector<Candidate> result;
        result.reserve(selectedIndices.size());
        for (const auto index : selectedIndices)
            result.push_back(candidates[index]);
        std::stable_sort(result.begin(), result.end(),
                         [&](const Candidate& left, const Candidate& right) {
                             chargeComparison(false);
                             return better(left, right, ObjectiveDirection::Minimize);
                         });
        return result;
    }

private:
    struct RankedFronts {
        std::vector<std::vector<std::size_t>> fronts;
        std::vector<double> crowding;
    };

    static std::size_t checkedAdd(std::size_t left, std::size_t right) {
        if (right > std::numeric_limits<std::size_t>::max() - left)
            throw ParetoSelectionLimitExceeded("Pareto candidate count overflow");
        return left + right;
    }

    template <class Candidate>
    std::size_t estimateSelectionBytes(std::size_t count, const std::vector<Candidate>& parents,
                                       const std::vector<Candidate>& offspring,
                                       bool environmental = false) {
        if (count &&
            count > std::numeric_limits<std::size_t>::max() / count / (2 * sizeof(std::size_t)))
            throw ParetoSelectionLimitExceeded("Pareto memory estimate overflow");
        std::size_t bytes = 2 * count * count * sizeof(std::size_t);
        const auto slots =
            environmental
                ? checkedAdd(count, checkedAdd(parents.size(),
                                               checkedAdd(offspring.size(), offspring.size())))
                : checkedAdd(count, count);
        const auto perCandidate =
            sizeof(Candidate) + 8 * sizeof(std::size_t) + sizeof(double) + sizeof(long double);
        if (slots && perCandidate > (std::numeric_limits<std::size_t>::max() - bytes) / slots)
            throw ParetoSelectionLimitExceeded("Pareto memory estimate overflow");
        bytes += slots * perCandidate;
        auto addExpressionStorage = [&](const auto& candidates) {
            for (const auto& candidate : candidates) {
                const auto nodes = candidate.expression().nodeCount();
                const auto textBytes = candidate.text().size();
                if (textBytes > std::numeric_limits<std::size_t>::max() - bytes)
                    throw ParetoSelectionLimitExceeded("Pareto memory estimate overflow");
                if (nodes > (std::numeric_limits<std::size_t>::max() - bytes - textBytes) /
                                (sizeof(ExpressionNode) + sizeof(char)))
                    throw ParetoSelectionLimitExceeded("Pareto memory estimate overflow");
                bytes += nodes * (sizeof(ExpressionNode) + sizeof(char)) + textBytes;
            }
        };
        addExpressionStorage(parents);
        addExpressionStorage(offspring);
        if (environmental) {
            addExpressionStorage(parents);
            addExpressionStorage(offspring);
            for (const auto& candidate : parents)
                addTextStorage(candidate, bytes);
            for (const auto& candidate : offspring)
                addTextStorage(candidate, bytes);
        } else {
            addExpressionStorage(parents);
        }
        recordMemory(bytes);
        if (bytes > maximumEstimatedMemoryBytes_)
            throw ParetoSelectionLimitExceeded("Pareto estimated-memory limit exhausted");
        return bytes;
    }

    void recordMemory(std::size_t bytes) {
        work_->peakEstimatedMemoryBytes = std::max(work_->peakEstimatedMemoryBytes, bytes);
    }

    static void addTextStorage(const auto& candidate, std::size_t& bytes) {
        const auto textBytes = candidate.text().size();
        if (textBytes > std::numeric_limits<std::size_t>::max() - bytes)
            throw ParetoSelectionLimitExceeded("Pareto memory estimate overflow");
        bytes += textBytes;
    }

    void chargeComparison(bool dominance = true) {
        const auto total = work_->dominanceComparisons + work_->selectionComparisons;
        if (total >= maximumComparisons_)
            throw ParetoSelectionLimitExceeded("Pareto comparison-work limit exhausted");
        auto& counter = dominance ? work_->dominanceComparisons : work_->selectionComparisons;
        if (counter == std::numeric_limits<std::uint64_t>::max())
            throw ParetoSelectionLimitExceeded("Pareto comparison counter overflow");
        ++counter;
    }

    template <class Candidate> bool dominates(const Candidate& left, const Candidate& right) {
        chargeComparison(true);
        if (left.fitnessState() != FitnessState::Valid)
            return false;
        if (right.fitnessState() != FitnessState::Valid)
            return true;
        const auto leftComplexity = left.expression().nodeCount();
        const auto rightComplexity = right.expression().nodeCount();
        const bool noWorse = left.fitness() <= right.fitness() && leftComplexity <= rightComplexity;
        const bool strictlyBetter =
            left.fitness() < right.fitness() || leftComplexity < rightComplexity;
        return noWorse && strictlyBetter;
    }

    template <class Candidate>
    RankedFronts rankAndCrowding(const std::vector<Candidate>& candidates) {
        const auto count = candidates.size();
        RankedFronts result;
        result.crowding.assign(count, 0.0);
        std::vector<std::vector<std::size_t>> dominated(count);
        std::vector<std::size_t> dominationCount(count, 0);
        std::vector<std::size_t> current;
        for (std::size_t left = 0; left < count; ++left) {
            if (candidates[left].fitnessState() != FitnessState::Valid)
                continue;
            for (std::size_t right = left + 1; right < count; ++right) {
                if (candidates[right].fitnessState() != FitnessState::Valid)
                    continue;
                if (dominates(candidates[left], candidates[right])) {
                    dominated[left].push_back(right);
                    ++dominationCount[right];
                } else if (dominates(candidates[right], candidates[left])) {
                    dominated[right].push_back(left);
                    ++dominationCount[left];
                }
            }
        }
        for (std::size_t index = 0; index < count; ++index)
            if (candidates[index].fitnessState() == FitnessState::Valid &&
                dominationCount[index] == 0)
                current.push_back(index);
        while (!current.empty()) {
            result.fronts.push_back(current);
            std::vector<std::size_t> next;
            for (const auto index : current)
                for (const auto dominatedIndex : dominated[index]) {
                    chargeComparison(false);
                    if (candidates[dominatedIndex].fitnessState() == FitnessState::Valid &&
                        --dominationCount[dominatedIndex] == 0)
                        next.push_back(dominatedIndex);
                }
            current = std::move(next);
        }
        for (auto& front : result.fronts)
            assignCrowding(candidates, front, result.crowding);
        return result;
    }

    template <class Candidate>
    void assignCrowding(const std::vector<Candidate>& candidates,
                        const std::vector<std::size_t>& front, std::vector<double>& crowding) {
        if (front.size() <= 2) {
            for (const auto index : front)
                crowding[index] = std::numeric_limits<double>::infinity();
            return;
        }
        for (unsigned objective = 0; objective < 2; ++objective) {
            auto ordered = front;
            std::stable_sort(
                ordered.begin(), ordered.end(), [&](std::size_t left, std::size_t right) {
                    chargeComparison(false);
                    if (objective == 0 && candidates[left].fitness() != candidates[right].fitness())
                        return candidates[left].fitness() < candidates[right].fitness();
                    if (objective == 1 && candidates[left].expression().nodeCount() !=
                                              candidates[right].expression().nodeCount())
                        return candidates[left].expression().nodeCount() <
                               candidates[right].expression().nodeCount();
                    return better(candidates[left], candidates[right],
                                  ObjectiveDirection::Minimize);
                });
            const auto value = [&](std::size_t index) {
                return objective == 0
                           ? static_cast<long double>(candidates[index].fitness())
                           : static_cast<long double>(candidates[index].expression().nodeCount());
            };
            const auto low = value(ordered.front());
            const auto high = value(ordered.back());
            if (!(high > low))
                continue;
            crowding[ordered.front()] = std::numeric_limits<double>::infinity();
            crowding[ordered.back()] = std::numeric_limits<double>::infinity();
            const auto scale = std::max(std::abs(low), std::abs(high));
            const auto range = high / scale - low / scale;
            for (std::size_t position = 1; position + 1 < ordered.size(); ++position) {
                if (std::isinf(crowding[ordered[position]]))
                    continue;
                const auto increment =
                    (value(ordered[position + 1]) / scale - value(ordered[position - 1]) / scale) /
                    range;
                const auto total =
                    static_cast<long double>(crowding[ordered[position]]) + increment;
                crowding[ordered[position]] = static_cast<double>(total);
            }
        }
    }

    std::uint64_t maximumComparisons_;
    std::size_t maximumEstimatedMemoryBytes_;
    ParetoSelectionWork* work_;
};
