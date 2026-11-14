#include <DiscreteConditionalGrammarSynthesis.hpp>

#include <algorithm>
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

DiscreteSpecification twoInputSpecification(const std::vector<std::int64_t>& first,
                                            const std::vector<std::int64_t>& second,
                                            auto function) {
    std::vector<DiscreteSpecificationObservation> rows;
    for (const auto x : first)
        for (const auto y : second)
            rows.push_back({{x, y}, DiscreteValue::integer(function(x, y))});
    return {2, std::move(rows)};
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

DiscreteConditionalGrammar
composedPredicateGrammar(std::size_t nesting, std::vector<std::int64_t> constants,
                         std::vector<bool> booleans, bool conjunction = false,
                         bool disjunction = false, bool negation = false) {
    DiscreteConditionalGrammar declared;
    declared.variables = {0, 1};
    declared.branchConstants = {0, 1};
    declared.predicateConstants = std::move(constants);
    declared.booleanConstants = std::move(booleans);
    declared.branchOperators.clear();
    declared.predicateOperators = {DiscreteKind::Modulo, DiscreteKind::Equal,
                                   DiscreteKind::LessThan};
    declared.maximumPredicateNodes = 10;
    declared.maximumPredicateIntegerNodes = 3;
    declared.maximumBranchNodes = 1;
    declared.maximumExpressionDepth = 4;
    declared.maximumBooleanNesting = nesting;
    declared.allowBooleanAnd = conjunction;
    declared.allowBooleanOr = disjunction;
    declared.allowBooleanNot = negation;
    declared.expressionLimits.maxNodes = 16;
    declared.expressionLimits.maxDepth = 8;
    return declared;
}

DiscreteConditionalGrammarResult synthesizeComposed(const DiscreteSpecification& training,
                                                    const DiscreteConditionalGrammar& grammar) {
    DiscreteConditionalGrammarLimits limits;
    limits.maximumPoolExpressions = 200000;
    limits.maximumConstructionAttempts = 1000000;
    limits.maximumCandidateCombinations = 1000000;
    limits.maximumConditionalNodes = 16;
    limits.maximumConditionalDepth = 8;
    limits.maximumEvaluationWork = 50000000;
    limits.maximumEstimatedBytes = 256 * 1024 * 1024;
    return synthesizeDiscreteConditionalFromGrammar(training, grammar, limits);
}

void composedPredicateRecovery() {
    const auto conjunctionTraining = twoInputSpecification({-6, -3, -1, 0, 1, 3, 6}, {-2, -1, 1, 2},
                                                           [](std::int64_t x, std::int64_t y) {
                                                               auto residue = x % 3;
                                                               if (residue < 0)
                                                                   residue += 3;
                                                               return residue == 0 && y < 0 ? 1 : 0;
                                                           });
    auto conjunctionGrammar = composedPredicateGrammar(1, {0, 3}, {false}, true);
    const auto conjunction = synthesizeComposed(conjunctionTraining, conjunctionGrammar);
    if (conjunction.status != DiscreteConditionalGrammarStatus::ExactTraining)
        std::cerr << "conjunction: status=" << static_cast<int>(conjunction.status)
                  << " matches=" << conjunction.matchingRows << '/' << conjunction.totalRows
                  << " diagnostic=" << conjunction.diagnostic
                  << " predicates=" << conjunction.work.predicatePoolSize
                  << " attempts=" << conjunction.work.constructionAttempts
                  << " generated=" << conjunction.work.generatedExpressions
                  << " bytes=" << conjunction.work.estimatedPeakBytes
                  << " combinations=" << conjunction.work.composition.combinations << '\n';
    check(conjunction.status == DiscreteConditionalGrammarStatus::ExactTraining &&
              conjunction.expression,
          "composed modular and comparison predicate exactly recovers the training grid");
    check(conjunction.work.predicatePoolSize > 0 && conjunction.work.composition.combinations > 0 &&
              conjunction.work.poolEvaluationWork <= 50000000,
          "Boolean enumeration and selector share the declared cumulative work budget");
    check(std::count_if(
              conjunction.expression->nodes().begin(), conjunction.expression->nodes().end(),
              [](const auto& node) { return node.kind() == DiscreteKind::IfThenElse; }) >= 2,
          "selected conjunction contains a composed Boolean predicate beneath the outer selector");
    check(conjunction.work.invalidExpressions > 0,
          "invalid modular candidates remain accounted during Boolean predicate enumeration");

    // A matched grammar with the same integer productions, constants, node/depth
    // bounds and output branches but no Boolean composition must not fit the grid.
    auto atomic = conjunctionGrammar;
    atomic.maximumBooleanNesting = 0;
    const auto atomicResult = synthesizeComposed(conjunctionTraining, atomic);
    check(atomicResult.status == DiscreteConditionalGrammarStatus::Partial &&
              atomicResult.matchingRows < atomicResult.totalRows,
          "atomic-predicate baseline cannot recover the conjunction within matched bounds");

    const auto disjunctionTraining = twoInputSpecification(
        {-2, -1, 0, 1, 2}, {-3, -2, -1, 0, 1, 2, 3}, [](std::int64_t x, std::int64_t y) {
            auto residue = y % 2;
            if (residue < 0)
                residue += 2;
            return x < 0 || residue == 0 ? 1 : 0;
        });
    auto disjunctionGrammar = composedPredicateGrammar(1, {0, 2}, {true}, false, true);
    const auto disjunction = synthesizeComposed(disjunctionTraining, disjunctionGrammar);
    check(disjunction.status == DiscreteConditionalGrammarStatus::ExactTraining &&
              disjunction.expression,
          "composed independent arithmetic conditions recover the disjunction training grid");
    check(std::count_if(
              disjunction.expression->nodes().begin(), disjunction.expression->nodes().end(),
              [](const auto& node) { return node.kind() == DiscreteKind::IfThenElse; }) >= 2,
          "selected disjunction contains a composed Boolean predicate beneath the outer selector");
    auto disjunctionAtomicGrammar = disjunctionGrammar;
    disjunctionAtomicGrammar.maximumBooleanNesting = 0;
    const auto disjunctionAtomic =
        synthesizeComposed(disjunctionTraining, disjunctionAtomicGrammar);
    check(disjunctionAtomic.status == DiscreteConditionalGrammarStatus::Partial &&
              disjunctionAtomic.matchingRows < disjunctionAtomic.totalRows,
          "atomic-predicate baseline cannot recover the disjunction within matched bounds");

    // These observations were not supplied to either constructor invocation.
    for (const auto [x, y] : std::array<std::array<std::int64_t, 2>, 8>{{{{-9, -4}},
                                                                         {{-4, 5}},
                                                                         {{-1, 3}},
                                                                         {{0, -5}},
                                                                         {{3, -2}},
                                                                         {{6, 7}},
                                                                         {{8, 0}},
                                                                         {{12, -1}}}}) {
        auto residue3 = x % 3;
        if (residue3 < 0)
            residue3 += 3;
        auto residue2 = y % 2;
        if (residue2 < 0)
            residue2 += 2;
        const auto firstValue = conjunction.expression->evaluate(std::array<std::int64_t, 2>{x, y});
        const auto secondValue =
            disjunction.expression->evaluate(std::array<std::int64_t, 2>{x, y});
        check(firstValue.valid() && firstValue.value->integer() == (residue3 == 0 && y < 0 ? 1 : 0),
              "conjunction agrees on held-out negative and modulo-boundary observations");
        check(secondValue.valid() &&
                  secondValue.value->integer() == (x < 0 || residue2 == 0 ? 1 : 0),
              "disjunction agrees on held-out independent arithmetic observations");
    }

    const auto repeated = synthesizeComposed(disjunctionTraining, disjunctionGrammar);
    check(repeated.expression && disjunction.expression &&
              repeated.expression->toString() == disjunction.expression->toString() &&
              repeated.work.constructionAttempts == disjunction.work.constructionAttempts,
          "Boolean grammar construction and equal-score tie breaking are deterministic");
    auto invalidNesting = disjunctionGrammar;
    invalidNesting.maximumBooleanNesting = 17;
    check(synthesizeComposed(disjunctionTraining, invalidNesting).status ==
              DiscreteConditionalGrammarStatus::Invalid,
          "Boolean nesting has a hard grammar bound");
}

void booleanNotProduction() {
    auto declared = grammar({DiscreteKind::LessThan}, {0}, 6);
    declared.variables = {0};
    declared.booleanConstants = {false, true};
    declared.predicateOperators = {DiscreteKind::LessThan};
    declared.maximumPredicateNodes = 6;
    declared.maximumPredicateIntegerNodes = 3;
    declared.maximumBooleanNesting = 1;
    declared.allowBooleanNot = true;
    declared.maximumExpressionDepth = 4;
    declared.expressionLimits.maxNodes = 8;
    declared.expressionLimits.maxDepth = 8;
    DiscreteConditionalGrammarLimits limits;
    limits.maximumPoolExpressions = 10000;
    limits.maximumConstructionAttempts = 10000;
    limits.maximumEstimatedBytes = 16 * 1024 * 1024;
    genetik_conditional_grammar_detail::Enumerator enumerator(declared, limits, 1, nullptr);
    const auto predicates = enumerator.all(DiscreteType::Bool, 6, true);
    const auto found =
        std::find_if(predicates.begin(), predicates.end(), [](const auto& expression) {
            return expression.toString() == "if((x0 < 0), false, true)";
        });
    check(found != predicates.end() && !enumerator.exhausted(),
          "declared grammar automatically enumerates NOT through lazy IfThenElse");
    const auto negative = found->evaluate(std::array<std::int64_t, 1>{-1});
    const auto nonnegative = found->evaluate(std::array<std::int64_t, 1>{1});
    check(negative.valid() && !negative.value->boolean() && nonnegative.valid() &&
              nonnegative.value->boolean(),
          "generated NOT production has the expected Boolean semantics");

    const auto x = DiscreteExpression::variable(0, 1);
    const auto zero = DiscreteExpression::integer(0, 1);
    const auto negativeCondition = DiscreteExpression::binary(DiscreteKind::LessThan, x, zero);
    const auto zeroRemainder = DiscreteExpression::binary(DiscreteKind::Modulo, x, zero);
    const auto invalidPredicate =
        DiscreteExpression::binary(DiscreteKind::Equal, zeroRemainder, zero);
    const auto lazyPredicate = DiscreteExpression::conditional(
        negativeCondition, DiscreteExpression::boolean(true, 1), invalidPredicate);
    const auto skipped = lazyPredicate.evaluate(std::array<std::int64_t, 1>{-1});
    const auto selected = lazyPredicate.evaluate(std::array<std::int64_t, 1>{1});
    check(skipped.valid() && skipped.value->boolean() &&
              selected.status == DiscreteEvaluationStatus::MathematicalInvalid,
          "Boolean IfThenElse short-circuits an unselected invalid predicate branch");
}

void invalidTraceDistinction() {
    const std::vector<DiscreteSpecificationObservation> rows{{{0}, DiscreteValue::integer(0)},
                                                             {{1}, DiscreteValue::integer(1)}};
    const DiscreteSpecification training(1, rows);
    const auto validFalse = DiscreteExpression::boolean(false, 1);
    const auto x = DiscreteExpression::variable(0, 1);
    const auto zero = DiscreteExpression::integer(0, 1);
    const auto invalidRemainder = DiscreteExpression::binary(DiscreteKind::Modulo, x, zero);
    const auto invalidBoolean =
        DiscreteExpression::binary(DiscreteKind::Equal, invalidRemainder, zero);
    std::size_t work = 0;
    bool valid = false, exhausted = false;
    const auto validKey = genetik_conditional_grammar_detail::behaviorKey(
        validFalse, training, {}, work, 1000, nullptr, valid, exhausted);
    check(valid && !exhausted, "valid Boolean candidate produces a valid training trace");
    const auto invalidKey = genetik_conditional_grammar_detail::behaviorKey(
        invalidBoolean, training, {}, work, 1000, nullptr, valid, exhausted);
    check(!valid && !exhausted && validKey != invalidKey &&
              invalidKey.find('!') != std::string::npos,
          "finite training-behavior reduction keeps invalid rows distinct from false values");
}
} // namespace

int main() {
    try {
        modularPiecewiseRecovery();
        arithmeticPiecewiseRecovery();
        negativeAndResourceControls();
        lazyOverflowRecovery();
        invalidArithmeticAndLazyBranches();
        composedPredicateRecovery();
        booleanNotProduction();
        invalidTraceDistinction();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
