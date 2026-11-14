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
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

DiscreteSpecification specification(const std::vector<std::int64_t>& xs, auto function) {
    std::vector<DiscreteSpecificationObservation> rows;
    for (auto x : xs) rows.push_back({{x}, DiscreteValue::integer(function(x))});
    return {1, std::move(rows)};
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
    const auto modular = specification({-5,-4,-3,-2,-1,0,1,2,3,4,5}, [](auto value) {
        auto residue = value % 3; if (residue < 0) residue += 3;
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
        check(value.valid() && *value.value == row.expected, "constructed modular control evaluates exactly");
    }

    // Arithmetic piecewise control uses a separate declared predicate.
    const auto lessZero = parseDiscreteExpression("x0 < 0", 1);
    const std::array signPredicates{lessZero};
    const auto arithmetic = specification({-4,-2,-1,0,1,2,4}, [](auto value) {
        return value < 0 ? value - 1 : value + 1;
    });
    result = composeDiscreteConditional(arithmetic, signPredicates, branches);
    check(result.status == DiscreteConditionalCompositionStatus::ExactTraining,
          "arithmetic piecewise control was constructed exactly");

    // Negative control: the pool has no expression for the even/odd distinction.
    const auto parity = specification({0,1,2,3,4,5}, [](auto value) {
        return value % 2 == 0 ? value + 1 : value - 1;
    });
    result = composeDiscreteConditional(parity, signPredicates, branches);
    check(result.status == DiscreteConditionalCompositionStatus::Partial &&
          result.matchingRows < result.totalRows, "negative control reports partial agreement");
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

    const auto allInvalid = specification({1}, [](auto) { return 0; });
    const std::array truePredicate{DiscreteExpression::boolean(true, 1)};
    const auto overflowingBranch = DiscreteExpression::binary(
        DiscreteKind::Add, x, DiscreteExpression::integer(std::numeric_limits<std::int64_t>::max(), 1));
    const std::array overflowBranches{overflowingBranch, overflowingBranch};
    result = composeDiscreteConditional(allInvalid, truePredicate, overflowBranches);
    check(result.status == DiscreteConditionalCompositionStatus::Invalid,
          "all mathematically invalid candidates have a distinct outcome");
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
    check(result.work.nodeVisits + result.work.operations + result.work.specificationComparisons <= 1,
          "evaluation work never exceeds its declared aggregate limit");

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

    const auto duplicateRows = DiscreteSpecification(1, {{{1}, DiscreteValue::integer(0)},
                                                           {{1}, DiscreteValue::integer(0)}});
    result = composeDiscreteConditional(duplicateRows, truePredicate, overflowBranches);
    check(result.status == DiscreteConditionalCompositionStatus::Invalid,
          "malformed duplicate-row specification is rejected");

    std::atomic<bool> stop{true};
    result = composeDiscreteConditional(allInvalid, truePredicate, overflowBranches, {}, &stop);
    check(result.status == DiscreteConditionalCompositionStatus::Cancelled,
          "pre-cancelled composition reports cancellation");
}
}

int main() {
    try { constructionControls(); invalidAndLimits(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
    std::cout << "bounded conditional composition contracts passed\n";
}
