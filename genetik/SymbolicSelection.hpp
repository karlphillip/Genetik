#pragma once

#include "Regression.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <functional>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct ParetoSelectionWork {
    std::uint64_t dominanceComparisons = 0;
    std::uint64_t selectionComparisons = 0;
    std::uint64_t objectiveVectorEvaluations = 0;
    std::size_t peakEstimatedMemoryBytes = 0;
    bool operator==(const ParetoSelectionWork&) const = default;
};

class ParetoSelectionLimitExceeded final : public std::runtime_error {
public:
    explicit ParetoSelectionLimitExceeded(const char* message) : std::runtime_error(message) {}
};

struct ExpressionComplexityCosts {
    ExpressionComplexityCosts(double operators = 1.0, double variables = 1.0,
                              double constants = 1.0,
                              std::vector<std::pair<ExpressionKind, double>> individual = {})
        : operatorCost(operators), variableCost(variables), constantCost(constants),
          individualOperatorCosts(std::move(individual)) {}
    double operatorCost = 1.0;
    double variableCost = 1.0;
    double constantCost = 1.0;
    std::vector<std::pair<ExpressionKind, double>> individualOperatorCosts;

    void validate() const {
        for (const double cost : {operatorCost, variableCost, constantCost})
            if (!std::isfinite(cost) || cost < 0.0)
                throw std::invalid_argument(
                    "expression complexity costs must be finite and nonnegative");
        for (std::size_t i = 0; i < individualOperatorCosts.size(); ++i) {
            const auto [kind, cost] = individualOperatorCosts[i];
            if (expressionArity(kind) == 0 || !std::isfinite(cost) || cost < 0.0 ||
                std::find_if(individualOperatorCosts.begin(), individualOperatorCosts.begin() + i,
                             [kind](const auto& entry) { return entry.first == kind; }) !=
                    individualOperatorCosts.begin() + i)
                throw std::invalid_argument("invalid or duplicate individual operator cost");
        }
    }

    double cost(ExpressionKind kind) const {
        const auto found =
            std::find_if(individualOperatorCosts.begin(), individualOperatorCosts.end(),
                         [kind](const auto& entry) { return entry.first == kind; });
        return found == individualOperatorCosts.end() ? operatorCost : found->second;
    }
};

inline long double configuredExpressionComplexity(const Expression& expression,
                                                  const ExpressionComplexityCosts& costs) {
    costs.validate();
    long double result = 0.0L;
    for (const auto& node : expression.nodes()) {
        const auto arity = expressionArity(node.kind());
        const long double nodeCost =
            arity == 0 ? (node.kind() == ExpressionKind::Variable ? costs.variableCost
                                                                  : costs.constantCost)
                       : costs.cost(node.kind());
        if (nodeCost > std::numeric_limits<long double>::max() - result)
            return std::numeric_limits<long double>::max();
        result += nodeCost;
    }
    return result;
}

struct ConfiguredRegressionOrdering {
    explicit ConfiguredRegressionOrdering(ExpressionComplexityCosts costs = {}) : costs_(costs) {
        costs_.validate();
    }
    void validateDirection(ObjectiveDirection direction) const {
        if (direction != ObjectiveDirection::Minimize)
            throw std::invalid_argument("regression requires minimization");
    }
    template <class Candidate>
    bool better(const Candidate& left, const Candidate& right, ObjectiveDirection direction) const {
        validateDirection(direction);
        if (left.fitness() != right.fitness())
            return left.fitness() < right.fitness();
        const auto leftCost = configuredExpressionComplexity(left.expression(), costs_);
        const auto rightCost = configuredExpressionComplexity(right.expression(), costs_);
        if (leftCost != rightCost)
            return leftCost < rightCost;
        return left.text() < right.text();
    }

private:
    ExpressionComplexityCosts costs_;
};

// Optional symbolic-regression ordering policy. Prediction loss remains the
// candidate fitness; this policy only supplies an alternate selection score.
struct ComplexityPenaltyOrdering {
    explicit ComplexityPenaltyOrdering(double penalty, ExpressionComplexityCosts costs = {})
        : penalty_(penalty), costs_(costs) {
        if (!std::isfinite(penalty_) || penalty_ < 0.0)
            throw std::invalid_argument("complexity penalty must be finite and nonnegative");
        costs_.validate();
    }

    void validateDirection(ObjectiveDirection direction) const {
        if (direction != ObjectiveDirection::Minimize)
            throw std::invalid_argument("regression requires minimization");
    }

    template <class Candidate> long double selectionScore(const Candidate& candidate) const {
        const auto penaltyCost = static_cast<long double>(penalty_) *
                                 configuredExpressionComplexity(candidate.expression(), costs_);
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
        const auto leftCost = configuredExpressionComplexity(left.expression(), costs_);
        const auto rightCost = configuredExpressionComplexity(right.expression(), costs_);
        if (leftCost != rightCost)
            return leftCost < rightCost;
        return left.text() < right.text();
    }

private:
    double penalty_;
    ExpressionComplexityCosts costs_;
};

// NSGA-II-style nondominated sorting, crowding-based survival, and rank/crowding
// parent weights. All tie breaks are stable and use expression text last.
struct ParetoRegressionOrdering {
    ParetoRegressionOrdering(
        std::uint64_t maximumComparisons, std::size_t maximumEstimatedMemoryBytes,
        ParetoSelectionWork& work, ExpressionComplexityCosts costs = {},
        std::shared_ptr<const RegressionProblem> problem = {},
        std::function<std::vector<double>(const Expression&, const RegressionProblem&)> objectives =
            {},
        std::size_t maximumObjectives = 4)
        : maximumComparisons_(maximumComparisons),
          maximumEstimatedMemoryBytes_(maximumEstimatedMemoryBytes), work_(&work),
          problem_(std::move(problem)), objectives_(std::move(objectives)),
          maximumObjectives_(maximumObjectives) {
        if (!maximumComparisons_ || !maximumEstimatedMemoryBytes_)
            throw std::invalid_argument("Pareto work and memory limits must be positive");
        if (objectives_ && (!problem_ || !maximumObjectives_ || maximumObjectives_ > 4))
            throw std::invalid_argument(
                "Pareto objective vectors require a problem and a limit in [1,4]");
        costs_ = costs;
        costs_.validate();
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
        const auto leftComplexity = configuredExpressionComplexity(left.expression(), costs_);
        const auto rightComplexity = configuredExpressionComplexity(right.expression(), costs_);
        if (leftComplexity != rightComplexity)
            return leftComplexity < rightComplexity;
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

    template <class Candidate> std::vector<double> objectiveValues(const Candidate& candidate) {
        if (!objectives_)
            return {candidate.fitness()};
        return vectorObjectives(candidate);
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
        if (objectives_) {
            constexpr auto objectiveOverhead =
                sizeof(std::vector<double>) + sizeof(std::string) + 64;
            const auto each = maximumObjectives_ * sizeof(double) + objectiveOverhead;
            if (slots && each > (std::numeric_limits<std::size_t>::max() - bytes) / slots)
                throw ParetoSelectionLimitExceeded(
                    "Pareto objective-vector memory estimate overflow");
            bytes += slots * each;
        }
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
        if (objectives_) {
            const auto a = vectorObjectives(left), b = vectorObjectives(right);
            bool noWorse = configuredExpressionComplexity(left.expression(), costs_) <=
                           configuredExpressionComplexity(right.expression(), costs_);
            bool strictlyBetter = configuredExpressionComplexity(left.expression(), costs_) <
                                  configuredExpressionComplexity(right.expression(), costs_);
            for (std::size_t i = 0; i < a.size(); ++i) {
                noWorse = noWorse && a[i] <= b[i];
                strictlyBetter = strictlyBetter || a[i] < b[i];
            }
            return noWorse && strictlyBetter;
        }
        const auto leftComplexity = configuredExpressionComplexity(left.expression(), costs_);
        const auto rightComplexity = configuredExpressionComplexity(right.expression(), costs_);
        const bool noWorse = left.fitness() <= right.fitness() && leftComplexity <= rightComplexity;
        const bool strictlyBetter =
            left.fitness() < right.fitness() || leftComplexity < rightComplexity;
        return noWorse && strictlyBetter;
    }

    template <class Candidate>
    RankedFronts rankAndCrowding(const std::vector<Candidate>& candidates) {
        objectiveCache_.clear();
        objectiveCount_ = 0;
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
        const auto dimensions = objectives_ ? objectiveCount_ + 1 : 2;
        for (std::size_t objective = 0; objective < dimensions; ++objective) {
            auto ordered = front;
            std::stable_sort(
                ordered.begin(), ordered.end(), [&](std::size_t left, std::size_t right) {
                    chargeComparison(false);
                    if (objectives_) {
                        const auto a = vectorObjectives(candidates[left]);
                        const auto b = vectorObjectives(candidates[right]);
                        const auto av = objective < objectiveCount_
                                            ? static_cast<long double>(a[objective])
                                            : configuredExpressionComplexity(
                                                  candidates[left].expression(), costs_);
                        const auto bv = objective < objectiveCount_
                                            ? static_cast<long double>(b[objective])
                                            : configuredExpressionComplexity(
                                                  candidates[right].expression(), costs_);
                        if (av != bv)
                            return av < bv;
                    } else {
                        if (objective == 0 &&
                            candidates[left].fitness() != candidates[right].fitness())
                            return candidates[left].fitness() < candidates[right].fitness();
                        if (objective == 1) {
                            const auto leftCost = configuredExpressionComplexity(
                                candidates[left].expression(), costs_);
                            const auto rightCost = configuredExpressionComplexity(
                                candidates[right].expression(), costs_);
                            if (leftCost != rightCost)
                                return leftCost < rightCost;
                        }
                    }
                    return better(candidates[left], candidates[right],
                                  ObjectiveDirection::Minimize);
                });
            const auto value = [&](std::size_t index) {
                if (objectives_) {
                    const auto values = vectorObjectives(candidates[index]);
                    return objective < objectiveCount_
                               ? static_cast<long double>(values[objective])
                               : configuredExpressionComplexity(candidates[index].expression(),
                                                                costs_);
                }
                return objective == 0
                           ? static_cast<long double>(candidates[index].fitness())
                           : configuredExpressionComplexity(candidates[index].expression(), costs_);
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

    template <class Candidate> std::vector<double> vectorObjectives(const Candidate& candidate) {
        const auto key = candidate.text();
        if (const auto found = objectiveCache_.find(key); found != objectiveCache_.end())
            return found->second;
        if (work_->objectiveVectorEvaluations == std::numeric_limits<std::uint64_t>::max())
            throw ParetoSelectionLimitExceeded("Pareto objective-vector counter overflow");
        ++work_->objectiveVectorEvaluations;
        auto values = objectives_(candidate.expression(), *problem_);
        if (values.empty() || values.size() > maximumObjectives_ ||
            (objectiveCount_ && values.size() != objectiveCount_))
            throw std::invalid_argument(
                "Pareto objective callback returned an empty or inconsistent vector");
        for (const auto value : values)
            if (!std::isfinite(value))
                throw std::invalid_argument("Pareto objective values must be finite");
        objectiveCount_ = values.size();
        objectiveCache_.emplace(key, values);
        return values;
    }

    std::uint64_t maximumComparisons_;
    std::size_t maximumEstimatedMemoryBytes_;
    ParetoSelectionWork* work_;
    ExpressionComplexityCosts costs_{};
    std::shared_ptr<const RegressionProblem> problem_;
    std::function<std::vector<double>(const Expression&, const RegressionProblem&)> objectives_;
    std::size_t maximumObjectives_ = 4, objectiveCount_ = 0;
    std::unordered_map<std::string, std::vector<double>> objectiveCache_;
};
