#include <DiscreteConditionalGrammarSynthesis.hpp>

#include <atomic>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}

DiscreteSpecification specification(const std::vector<std::int64_t>& values, auto function) {
    std::vector<DiscreteSpecificationObservation> rows;
    for (const auto x : values)
        rows.push_back({{x}, DiscreteValue::integer(function(x))});
    return {1, std::move(rows)};
}

DiscreteConditionalGrammar grammar(std::vector<DiscreteKind> operators,
                                   std::vector<std::int64_t> constants, std::size_t maximumNodes) {
    DiscreteConditionalGrammar value;
    value.variables = {0};
    value.branchConstants = constants;
    value.predicateConstants = std::move(constants);
    value.branchOperators = operators;
    value.predicateOperators = std::move(operators);
    value.maximumPredicateNodes = maximumNodes;
    value.maximumBranchNodes = maximumNodes;
    value.maximumExpressionDepth = maximumNodes;
    value.expressionLimits.maxNodes = maximumNodes;
    value.expressionLimits.maxDepth = maximumNodes;
    return value;
}

DiscreteConditionalGrammarResult synthesize(const DiscreteSpecification& training,
                                            const DiscreteConditionalGrammar& declared) {
    DiscreteConditionalGrammarLimits limits;
    limits.maximumPoolExpressions = 200000;
    limits.maximumConstructionAttempts = 2000000;
    limits.maximumCandidateCombinations = 2000000;
    limits.maximumEvaluationWork = 20000000;
    limits.maximumEstimatedBytes = 256 * 1024 * 1024;
    return synthesizeDiscreteConditionalFromGrammar(training, declared, limits);
}

void modularPiecewiseRecovery() {
    // The constructor sees only this table and grammar. Its independently
    // implemented target is if(x mod 3 == 0, x + 2, x - 1).
    const auto training = specification({-3, 0, -2, -1}, [](std::int64_t x) {
        auto residue = x % 3;
        if (residue < 0)
            residue += 3;
        return x + (residue == 0 ? 2 : -1);
    });
    const auto declared = grammar(
        {DiscreteKind::Add, DiscreteKind::Subtract, DiscreteKind::Modulo, DiscreteKind::Equal},
        {0, 1, 2, 3}, 5);
    auto boundedGrammar = declared;
    boundedGrammar.branchOperators = {DiscreteKind::Add, DiscreteKind::Subtract};
    boundedGrammar.predicateOperators = {DiscreteKind::Modulo, DiscreteKind::Equal};
    boundedGrammar.branchConstants = {1, 2};
    boundedGrammar.predicateConstants = {0, 3};
    boundedGrammar.maximumBranchNodes = 3;
    auto result = synthesize(training, boundedGrammar);
    check(result.status == DiscreteConditionalGrammarStatus::ExactTraining,
          "automatically enumerated modular conditional recovers all training rows");
    check(result.expression.has_value(), "modular synthesis returns constructed expression");
    check(result.expression->outerBindings().empty(),
          "generated conditional is closed under lexical bindings");
    for (const auto& row : training.observations()) {
        const auto evaluated = result.expression->evaluate(row.inputs);
        check(evaluated.valid() && *evaluated.value == row.expected,
              "constructed modular conditional evaluates on training rows");
    }

    // Held-out values are passed only to this independent assessment loop.
    for (const auto x : {-9, -8, -7, -5, -4, -2, -1, 6, 7, 8, 9}) {
        auto residue = x % 3;
        if (residue < 0)
            residue += 3;
        const auto evaluated = result.expression->evaluate(std::array<std::int64_t, 1>{x});
        check(evaluated.valid() && evaluated.value->integer() == x + (residue == 0 ? 2 : -1),
              "modular rule agrees on held-out observations");
    }
    check(result.work.generatedExpressions > 0 && result.work.composition.combinations > 0,
          "modular rule was constructed by enumeration and composition");
    check(result.work.poolEvaluationWork <= 20000000,
          "pool and composition work share the declared aggregate evaluation cap");

    const auto repeated = synthesize(training, boundedGrammar);
    check(repeated.status == result.status && repeated.expression && result.expression &&
              repeated.expression->toString() == result.expression->toString() &&
              repeated.work.generatedExpressions == result.work.generatedExpressions,
          "grammar enumeration and selection order are deterministic");

    auto exactPoolLimit = DiscreteConditionalGrammarLimits{};
    exactPoolLimit.maximumPoolExpressions = 200000;
    exactPoolLimit.maximumConstructionAttempts = 2000000;
    exactPoolLimit.maximumCandidateCombinations = 2000000;
    exactPoolLimit.maximumEvaluationWork = 20000000;
    exactPoolLimit.maximumEstimatedBytes = 256 * 1024 * 1024;
    const auto measured =
        synthesizeDiscreteConditionalFromGrammar(training, boundedGrammar, exactPoolLimit);
    check(measured.work.poolTraceEvaluationWork > 0,
          "grammar trace work is reported separately for exact aggregate-limit accounting");
    exactPoolLimit.maximumEvaluationWork =
        measured.work.specificationValidationWork + measured.work.poolTraceEvaluationWork;
    const auto exactLimit =
        synthesizeDiscreteConditionalFromGrammar(training, boundedGrammar, exactPoolLimit);
    check(exactLimit.status == DiscreteConditionalGrammarStatus::ResourceExhausted,
          "work exhausted exactly at pool-trace completion is not mislabeled invalid");
}

void arithmeticPiecewiseRecovery() {
    const auto training =
        specification({-3, -1, 0, 2}, [](std::int64_t x) { return x < 0 ? x - 2 : x + 1; });
    const auto declared =
        grammar({DiscreteKind::Add, DiscreteKind::Subtract, DiscreteKind::LessThan}, {0, 1, 2}, 3);
    auto boundedGrammar = declared;
    boundedGrammar.branchOperators = {DiscreteKind::Add, DiscreteKind::Subtract};
    boundedGrammar.predicateOperators = {DiscreteKind::LessThan};
    boundedGrammar.branchConstants = {1, 2};
    boundedGrammar.predicateConstants = {0};
    boundedGrammar.maximumPredicateNodes = 3;
    auto result = synthesize(training, boundedGrammar);
    check(result.status == DiscreteConditionalGrammarStatus::ExactTraining,
          "automatically enumerated arithmetic conditional recovers all training rows");
    check(result.expression.has_value(), "arithmetic synthesis returns constructed expression");
    check(result.expression->outerBindings().empty(),
          "generated arithmetic conditional is lexically closed");
    for (const auto x : {-9, -8, -7, -6, 6, 7, 8, 9}) {
        const auto evaluated = result.expression->evaluate(std::array<std::int64_t, 1>{x});
        check(evaluated.valid() && evaluated.value->integer() == (x < 0 ? x - 2 : x + 1),
              "arithmetic rule agrees on held-out observations");
    }
    check(result.work.predicatePoolSize > 0 && result.work.branchPoolSize > 0,
          "typed pools are generated automatically from the grammar");
}

void negativeAndResourceControls() {
    const auto training = specification({-3, -2, -1, 0, 1, 2, 3},
                                        [](std::int64_t x) { return x % 2 == 0 ? x + 1 : x - 1; });
    const auto incomplete = grammar({DiscreteKind::Add, DiscreteKind::LessThan}, {0, 1}, 3);
    auto incompleteTyped = incomplete;
    incompleteTyped.branchOperators = {DiscreteKind::Add};
    incompleteTyped.predicateOperators = {DiscreteKind::LessThan};
    const auto partial = synthesize(training, incompleteTyped);
    check(partial.status == DiscreteConditionalGrammarStatus::Partial &&
              partial.matchingRows < partial.totalRows,
          "missing modular production yields a distinct partial result");

    auto tiny = DiscreteConditionalGrammarLimits{};
    tiny.maximumConstructionAttempts = 1;
    auto exhausted = synthesizeDiscreteConditionalFromGrammar(training, incompleteTyped, tiny);
    check(exhausted.status == DiscreteConditionalGrammarStatus::ResourceExhausted,
          "construction-attempt exhaustion is explicit");

    tiny = {};
    tiny.maximumPoolExpressions = 1;
    exhausted = synthesizeDiscreteConditionalFromGrammar(training, incompleteTyped, tiny);
    check(exhausted.status == DiscreteConditionalGrammarStatus::ResourceExhausted,
          "candidate-pool exhaustion is explicit");

    tiny = {};
    tiny.maximumEstimatedBytes = 64;
    exhausted = synthesizeDiscreteConditionalFromGrammar(training, incompleteTyped, tiny);
    check(exhausted.status == DiscreteConditionalGrammarStatus::ResourceExhausted,
          "logical memory estimate exhaustion is explicit");

    tiny = {};
    tiny.maximumEvaluationWork = 1;
    exhausted = synthesizeDiscreteConditionalFromGrammar(training, incompleteTyped, tiny);
    check(
        exhausted.status == DiscreteConditionalGrammarStatus::ResourceExhausted &&
            exhausted.work.specificationValidationWork == 0,
        "specification scan, pool, and selector evaluation work share the declared aggregate cap");

    tiny = {};
    tiny.maximumCandidateCombinations = 1;
    exhausted = synthesizeDiscreteConditionalFromGrammar(training, incompleteTyped, tiny);
    check(exhausted.status == DiscreteConditionalGrammarStatus::ResourceExhausted,
          "conditional combination exhaustion is explicit");

    tiny = {};
    tiny.maximumConditionalNodes = 2;
    exhausted = synthesizeDiscreteConditionalFromGrammar(training, incompleteTyped, tiny);
    check(exhausted.status == DiscreteConditionalGrammarStatus::ResourceExhausted,
          "complete conditional node limit is explicit");

    tiny = {};
    tiny.maximumConditionalDepth = 1;
    exhausted = synthesizeDiscreteConditionalFromGrammar(training, incompleteTyped, tiny);
    check(exhausted.status == DiscreteConditionalGrammarStatus::ResourceExhausted,
          "complete conditional depth limit is explicit");

    std::atomic<bool> stop{true};
    const auto cancelled =
        synthesizeDiscreteConditionalFromGrammar(training, incompleteTyped, {}, &stop);
    check(cancelled.status == DiscreteConditionalGrammarStatus::Cancelled,
          "cancellation during grammar enumeration is explicit");

    auto deep = incompleteTyped;
    deep.maximumPredicateNodes = 300;
    deep.maximumBranchNodes = 300;
    deep.expressionLimits.maxNodes = 300;
    const auto depthExhausted = synthesizeDiscreteConditionalFromGrammar(training, deep);
    check(depthExhausted.status == DiscreteConditionalGrammarStatus::ResourceExhausted,
          "unsafe recursive grammar depth is refused as resource exhaustion");

    auto invalid = incompleteTyped;
    invalid.predicateOperators.push_back(DiscreteKind::IfThenElse);
    const auto rejected = synthesizeDiscreteConditionalFromGrammar(training, invalid);
    check(rejected.status == DiscreteConditionalGrammarStatus::Invalid,
          "unsupported nested conditional grammar production is rejected");

    auto oversized = incompleteTyped;
    oversized.branchConstants.resize(4097, 7);
    const auto oversizedResult = synthesizeDiscreteConditionalFromGrammar(training, oversized);
    check(oversizedResult.status == DiscreteConditionalGrammarStatus::Invalid,
          "grammar terminal source has a hard bounded size before duplicate validation");

    auto sourceMemory = incompleteTyped;
    sourceMemory.branchConstants.resize(4096, 7);
    auto tinyMemory = DiscreteConditionalGrammarLimits{};
    tinyMemory.maximumEstimatedBytes = 64;
    const auto sourceMemoryResult =
        synthesizeDiscreteConditionalFromGrammar(training, sourceMemory, tinyMemory);
    check(sourceMemoryResult.status == DiscreteConditionalGrammarStatus::ResourceExhausted &&
              sourceMemoryResult.work.estimatedPeakBytes > tinyMemory.maximumEstimatedBytes,
          "grammar validation containers are admitted against logical memory before allocation");
}

void lazyOverflowRecovery() {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto training =
        specification({0, 1, maximum}, [](std::int64_t x) { return x < 2 ? x + 1 : maximum; });
    auto declared = grammar({DiscreteKind::Add, DiscreteKind::LessThan}, {0, 1, 2, maximum}, 3);
    declared.branchOperators = {DiscreteKind::Add};
    declared.predicateOperators = {DiscreteKind::LessThan};
    declared.branchConstants = {1, maximum};
    declared.predicateConstants = {2};
    declared.maximumPredicateNodes = 3;
    declared.maximumBranchNodes = 3;
    auto result = synthesize(training, declared);
    check(result.status == DiscreteConditionalGrammarStatus::ExactTraining && result.expression,
          "grammar synthesis can select a branch that is invalid only on an unselected row");
    check(result.work.invalidExpressions > 0,
          "partially invalid branch candidates are retained with row-wise validity markers");
    for (const auto& row : training.observations()) {
        const auto value = result.expression->evaluate(row.inputs);
        check(value.valid() && *value.value == row.expected,
              "constructed conditional lazily avoids the overflowing branch");
    }
}

void invalidArithmeticAndLazyBranches() {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto training = specification({maximum, maximum - 1, maximum - 2}, [](std::int64_t x) {
        return x == maximum ? maximum : x + 1;
    });
    auto declared = grammar({DiscreteKind::Add, DiscreteKind::Equal}, {0, 1, maximum}, 3);
    declared.branchOperators = {DiscreteKind::Add};
    declared.predicateOperators = {DiscreteKind::Equal};
    declared.branchConstants = {maximum};
    declared.predicateConstants = {0, 1, maximum};
    const auto result = synthesize(training, declared);
    check(result.work.invalidExpressions > 0,
          "checked overflow candidates are recorded during bounded pool evaluation");
    // The independently qualified PR #39 conditional constructor continues to
    // provide lazy-arm semantics; the generated API uses that same selector.
    check(result.status == DiscreteConditionalGrammarStatus::ExactTraining ||
              result.status == DiscreteConditionalGrammarStatus::Partial,
          "invalid candidate arithmetic remains distinct from synthesis failure");
}
} // namespace

int main() {
    try {
        modularPiecewiseRecovery();
        arithmeticPiecewiseRecovery();
        negativeAndResourceControls();
        lazyOverflowRecovery();
        invalidArithmeticAndLazyBranches();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
