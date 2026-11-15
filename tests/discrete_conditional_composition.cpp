#include <DiscreteConditionalComposition.hpp>
#include <DiscreteExpressionParsing.hpp>

#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}

DiscreteSpecification specification(const std::vector<std::int64_t>& xs, auto function) {
    std::vector<DiscreteSpecificationObservation> rows;
    for (auto x : xs)
        rows.push_back({{x}, DiscreteValue::integer(function(x))});
    return {1, std::move(rows)};
}

std::vector<genetik_conditional_composition_detail::TrainingTrace>
branchTraces(const DiscreteSpecification& training, std::span<const DiscreteExpression> branches) {
    std::vector<genetik_conditional_composition_detail::TrainingTrace> traces(branches.size());
    for (std::size_t b = 0; b < branches.size(); ++b) {
        auto& trace = traces[b];
        trace.values.reserve(training.observations().size());
        trace.valid.reserve(training.observations().size());
        for (const auto& row : training.observations()) {
            const auto value = branches[b].evaluate(row.inputs);
            trace.valid.push_back(value.valid() ? 1 : 0);
            trace.values.push_back(
                value.valid() ? static_cast<std::uint64_t>(value.value->integer()) : 0);
        }
    }
    return traces;
}

DiscreteConditionalCompositionResult comparePruned(const DiscreteSpecification& training,
                                                   std::span<const DiscreteExpression> predicates,
                                                   std::span<const DiscreteExpression> branches) {
    const auto ordinary = composeDiscreteConditional(training, predicates, branches);
    auto limits = DiscreteConditionalCompositionLimits{};
    const auto traces = branchTraces(training, branches);
    const auto pruned =
        genetik_conditional_composition_detail::composeDiscreteConditionalWithBranchTraces(
            training, predicates, branches, limits, nullptr, traces, true);
    check(pruned.status == ordinary.status && pruned.matchingRows == ordinary.matchingRows &&
              pruned.expression.has_value() == ordinary.expression.has_value() &&
              (!pruned.expression ||
               pruned.expression->toString() == ordinary.expression->toString()),
          "specification pruning exactly preserves complete selected result");
    return pruned;
}

void constructionControls() {
    const auto x = DiscreteExpression::variable(0, 1);
    const auto one = DiscreteExpression::integer(1, 1);
    const auto two = DiscreteExpression::integer(2, 1);
    const auto three = DiscreteExpression::integer(3, 1);
    const auto addOne = DiscreteExpression::binary(DiscreteKind::Add, x, one);
    const auto addTwo = DiscreteExpression::binary(DiscreteKind::Add, x, two);
    const auto minusOne = DiscreteExpression::binary(DiscreteKind::Subtract, x, one);
    const std::array branches{addOne, addTwo, minusOne};

    // Independently specified: add 2 exactly when x is 1 mod 3.
    const auto modPredicate = parseDiscreteExpression("x0 % 3 == 1", 1);
    const std::array predicates{modPredicate};
    const auto modular = specification({-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5}, [](auto value) {
        auto residue = value % 3;
        if (residue < 0)
            residue += 3;
        return value + (residue == 1 ? 2 : 1);
    });
    auto result = composeDiscreteConditional(modular, predicates, branches);
    check(result.status == DiscreteConditionalCompositionStatus::ExactTraining,
          "modular piecewise control was constructed exactly");
    check(result.work.estimatedBytes <= DiscreteConditionalCompositionLimits{}.maxEstimatedBytes,
          "exact control's memory estimate is admitted");
    check(result.expression && result.expression->type() == DiscreteType::Int64,
          "modular control returns a typed expression");
    check(result.work.nodeVisits + result.work.operations + result.work.specificationComparisons <=
              DiscreteConditionalCompositionLimits{}.maxEvaluationWork,
          "modular control's aggregate evaluation work is bounded");
    check(result.work.nodeVisits + result.work.operations + result.work.specificationComparisons <=
              DiscreteConditionalCompositionLimits{}.maxEvaluationWork,
          "modular control's aggregate evaluation work is bounded");
    for (const auto& row : modular.observations()) {
        auto value = result.expression->evaluate(row.inputs);
        check(value.valid() && *value.value == row.expected,
              "constructed modular control evaluates exactly");
    }
    const auto modularPruned = comparePruned(modular, predicates, branches);
    check(modularPruned.work.pruningWork > 0 && modularPruned.work.estimatedBytes > 0,
          "modular arithmetic control charges pruning work and memory");

    // Arithmetic piecewise control uses a separate declared predicate.
    const auto lessZero = parseDiscreteExpression("x0 < 0", 1);
    const std::array signPredicates{lessZero};
    const auto arithmetic = specification(
        {-4, -2, -1, 0, 1, 2, 4}, [](auto value) { return value < 0 ? value - 1 : value + 1; });
    result = composeDiscreteConditional(arithmetic, signPredicates, branches);
    check(result.status == DiscreteConditionalCompositionStatus::ExactTraining,
          "arithmetic piecewise control was constructed exactly");
    const auto arithmeticPruned = comparePruned(arithmetic, signPredicates, branches);
    check(arithmeticPruned.status == result.status &&
              arithmeticPruned.expression->toString() == result.expression->toString(),
          "independent arithmetic branch structure preserves the exact selected expression");

    // Negative control: the pool has no expression for the even/odd distinction.
    const auto parity = specification(
        {0, 1, 2, 3, 4, 5}, [](auto value) { return value % 2 == 0 ? value + 1 : value - 1; });
    result = composeDiscreteConditional(parity, signPredicates, branches);
    check(result.status == DiscreteConditionalCompositionStatus::Partial &&
              result.matchingRows < result.totalRows,
          "negative control reports partial agreement");
    const auto partialPruned = comparePruned(parity, signPredicates, branches);
    check(partialPruned.status == DiscreteConditionalCompositionStatus::Partial,
          "ambiguous partial control preserves the complete selected result");

    const auto ambiguous =
        specification({-2, -1, 0, 1, 2}, [](auto value) { return value == 0 ? 2 : 1; });
    const std::array ambiguousBranches{one, two, addOne};
    const std::array ambiguousPredicates{lessZero, modPredicate};
    const auto ambiguousPruned = comparePruned(ambiguous, ambiguousPredicates, ambiguousBranches);
    check(ambiguousPruned.expression &&
              ambiguousPruned.status == DiscreteConditionalCompositionStatus::Partial,
          "ambiguous specification keeps deterministic size and tuple tie-breaking");
}

void invalidAndLimits() {
    const auto x = DiscreteExpression::variable(0, 1);
    const auto one = DiscreteExpression::integer(1, 1);
    const auto maximum = DiscreteExpression::integer(std::numeric_limits<std::int64_t>::max(), 1);
    const auto plus = DiscreteExpression::binary(DiscreteKind::Add, x, one);
    const std::array predicates{parseDiscreteExpression("x0 >= 0", 1)};
    const std::array branches{maximum, plus};
    const auto overflow = specification({std::numeric_limits<std::int64_t>::max()}, [](auto) {
        return std::numeric_limits<std::int64_t>::max();
    });
    auto result = composeDiscreteConditional(overflow, predicates, branches);
    check(result.status == DiscreteConditionalCompositionStatus::ExactTraining,
          "lazy unselected overflow branch is not evaluated");
    const auto overflowPruned = comparePruned(overflow, predicates, branches);
    check(overflowPruned.status == result.status &&
              overflowPruned.expression->toString() == result.expression->toString(),
          "pruning keeps an invalid unselected branch masked");

    const auto allInvalid = specification({1}, [](auto) { return 0; });
    const std::array truePredicate{DiscreteExpression::boolean(true, 1)};
    const auto overflowingBranch = DiscreteExpression::binary(
        DiscreteKind::Add, x,
        DiscreteExpression::integer(std::numeric_limits<std::int64_t>::max(), 1));
    const std::array overflowBranches{overflowingBranch, overflowingBranch};
    result = composeDiscreteConditional(allInvalid, truePredicate, overflowBranches);
    check(result.status == DiscreteConditionalCompositionStatus::Invalid,
          "all mathematically invalid candidates have a distinct outcome");
    const auto invalidPruned = comparePruned(allInvalid, truePredicate, overflowBranches);
    check(invalidPruned.status == DiscreteConditionalCompositionStatus::Invalid &&
              invalidPruned.work.prunedCombinations == invalidPruned.work.combinations &&
              invalidPruned.work.evaluatedCombinations == 0,
          "two invalid branches cannot be misclassified as a zero-score candidate");

    const DiscreteSpecification partialRows(1, {{{-1}, DiscreteValue::integer(1)},
                                                {{0}, DiscreteValue::integer(1)},
                                                {{1}, DiscreteValue::integer(0)}});
    const std::array partialPredicate{DiscreteExpression::boolean(true, 1)};
    const std::array partialBranches{DiscreteExpression::integer(1, 1),
                                     DiscreteExpression::integer(0, 1)};
    auto partialLimits = DiscreteConditionalCompositionLimits{};
    partialLimits.maxEvaluationWork = 104;
    const auto ordinaryPartial =
        composeDiscreteConditional(partialRows, partialPredicate, partialBranches, partialLimits);
    const auto partialTraces = branchTraces(partialRows, partialBranches);
    const auto guardedPartial =
        genetik_conditional_composition_detail::composeDiscreteConditionalWithBranchTraces(
            partialRows, partialPredicate, partialBranches, partialLimits, nullptr, partialTraces,
            true);
    check(ordinaryPartial.status == DiscreteConditionalCompositionStatus::Partial &&
              guardedPartial.status == ordinaryPartial.status &&
              guardedPartial.matchingRows == ordinaryPartial.matchingRows &&
              guardedPartial.expression->toString() == ordinaryPartial.expression->toString() &&
              guardedPartial.work.prunedCombinations == 0 &&
              guardedPartial.work.evaluatedCombinations == 4,
          "aggregate work guard evaluates a dominated tuple when its evaluator cost is not safely "
          "admitted");

    const auto zero = DiscreteExpression::integer(0, 1);
    auto largerBranch = zero;
    for (int i = 0; i < 4; ++i)
        largerBranch = DiscreteExpression::binary(DiscreteKind::Add, largerBranch, zero);
    const std::array memoryBranches{DiscreteExpression::integer(1, 1), largerBranch};
    const auto memoryTraining = specification({0}, [](auto) { return 1; });
    auto memoryLimits = DiscreteConditionalCompositionLimits{};
    struct MaskStorage {
        std::vector<std::uint8_t> valid, matches;
    };
    const auto rows = memoryTraining.observations().size();
    const auto baseBytes =
        sizeof(DiscreteConditionalCompositionResult) + sizeof(DiscreteSpecificationObservation) +
        sizeof(std::int64_t) +
        partialPredicate[0].nodeCount() * DiscreteExpression::storageBytesPerNode() +
        memoryBranches[0].nodeCount() * DiscreteExpression::storageBytesPerNode() +
        memoryBranches[1].nodeCount() * DiscreteExpression::storageBytesPerNode() +
        std::max(memoryLimits.evaluation.maxWorkspaceBytes,
                 DiscreteSpecificationValidationLimits{}.maxPeakRowWorkspaceBytes);
    const auto pairCount = memoryBranches.size() * memoryBranches.size();
    const auto maskBytes = memoryBranches.size() * rows * 2 + sizeof(std::vector<MaskStorage>) +
                           128 + memoryBranches.size() * (sizeof(MaskStorage) + 64) +
                           pairCount * (sizeof(std::size_t) + sizeof(std::uint8_t)) +
                           sizeof(std::vector<std::size_t>) + sizeof(std::vector<std::uint8_t>) +
                           128;
    const auto mixedNodes = partialPredicate[0].nodeCount() + memoryBranches[0].nodeCount() +
                            memoryBranches[1].nodeCount() + 1;
    memoryLimits.maxEstimatedBytes =
        baseBytes + maskBytes + 2 * mixedNodes * DiscreteExpression::storageBytesPerNode();
    const auto memoryTraces = branchTraces(memoryTraining, memoryBranches);
    const auto memoryLimited =
        genetik_conditional_composition_detail::composeDiscreteConditionalWithBranchTraces(
            memoryTraining, partialPredicate, memoryBranches, memoryLimits, nullptr, memoryTraces,
            true);
    check(memoryLimited.status == DiscreteConditionalCompositionStatus::ResourceExhausted &&
              memoryLimited.work.prunedCombinations == 0,
          "dominated tuple cannot bypass candidate memory admission");
    check(result.work.estimatedBytes > sizeof(DiscreteConditionalCompositionResult),
          "invalid attempts still report candidate construction peak");

    DiscreteConditionalCompositionLimits limits;
    limits.maxCandidateCombinations = 1;
    result = composeDiscreteConditional(allInvalid, truePredicate, overflowBranches, limits);
    check(result.status == DiscreteConditionalCompositionStatus::ResourceExhausted,
          "candidate-combination limit is explicit");

    limits = {};
    limits.maxEvaluationWork = 1;
    result = composeDiscreteConditional(allInvalid, truePredicate, overflowBranches, limits);
    check(result.status == DiscreteConditionalCompositionStatus::ResourceExhausted,
          "evaluation-work limit is explicit");
    const auto ordinaryTightStatus = result.status;
    check(result.work.nodeVisits + result.work.operations + result.work.specificationComparisons <=
              1,
          "evaluation work never exceeds its declared aggregate limit");
    const auto invalidTraces = branchTraces(allInvalid, overflowBranches);
    result = genetik_conditional_composition_detail::composeDiscreteConditionalWithBranchTraces(
        allInvalid, truePredicate, overflowBranches, limits, nullptr, invalidTraces, true);
    check(result.status == ordinaryTightStatus &&
              result.status == DiscreteConditionalCompositionStatus::ResourceExhausted &&
              result.work.pruningWork <= limits.maxEvaluationWork,
          "pruning work exhaustion remains a resource outcome");

    limits = {};
    limits.maxNodes = 2;
    result = composeDiscreteConditional(allInvalid, truePredicate, overflowBranches, limits);
    check(result.status == DiscreteConditionalCompositionStatus::ResourceExhausted,
          "too-small structure bounds report a resource outcome");

    limits = {};
    limits.maxDepth = 1;
    result = composeDiscreteConditional(allInvalid, truePredicate, overflowBranches, limits);
    check(result.status == DiscreteConditionalCompositionStatus::ResourceExhausted,
          "too-small depth bounds report a resource outcome");

    limits = {};
    limits.maxEstimatedBytes = 1;
    result = composeDiscreteConditional(allInvalid, truePredicate, overflowBranches, limits);
    check(result.status == DiscreteConditionalCompositionStatus::ResourceExhausted,
          "logical memory estimate limit is explicit");

    const auto duplicateRows = DiscreteSpecification(
        1, {{{1}, DiscreteValue::integer(0)}, {{1}, DiscreteValue::integer(0)}});
    result = composeDiscreteConditional(duplicateRows, truePredicate, overflowBranches);
    check(result.status == DiscreteConditionalCompositionStatus::Invalid,
          "malformed duplicate-row specification is rejected");

    std::atomic<bool> stop{true};
    result = composeDiscreteConditional(allInvalid, truePredicate, overflowBranches, {}, &stop);
    check(result.status == DiscreteConditionalCompositionStatus::Cancelled,
          "pre-cancelled composition reports cancellation");
    limits = {};
    result = genetik_conditional_composition_detail::composeDiscreteConditionalWithBranchTraces(
        allInvalid, truePredicate, overflowBranches, limits, &stop, invalidTraces, true);
    check(result.status == DiscreteConditionalCompositionStatus::Cancelled,
          "pruning cancellation is not converted to invalidity or partial score");
}
} // namespace

int main() {
    try {
        constructionControls();
        invalidAndLimits();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "bounded conditional composition contracts passed\n";
}
