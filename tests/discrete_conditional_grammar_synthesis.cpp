#include <DiscreteConditionalGrammarSynthesis.hpp>

#include <algorithm>
#include <atomic>
#include <array>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
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

DiscreteConditionalGrammar
frozenMultiRegionGrammar(std::size_t dimensions, std::vector<std::int64_t> predicateConstants,
                         std::vector<std::int64_t> branchConstants,
                         std::vector<DiscreteKind> predicates, std::vector<DiscreteKind> branches,
                         std::size_t predicateNodes, std::size_t branchNodes = 8) {
    DiscreteConditionalGrammar value;
    value.variables =
        dimensions == 1 ? std::vector<std::size_t>{0} : std::vector<std::size_t>{0, 1};
    value.predicateConstants = std::move(predicateConstants);
    value.branchConstants = std::move(branchConstants);
    value.predicateOperators = std::move(predicates);
    value.branchOperators = std::move(branches);
    value.maximumPredicateNodes = predicateNodes;
    value.maximumBranchNodes = branchNodes;
    value.maximumExpressionDepth = 8;
    value.maximumConditionalNesting = 1;
    value.expressionLimits.maxNodes = 8;
    value.expressionLimits.maxDepth = 8;
    return value;
}

DiscreteConditionalGrammarResult runMultiRegion(const DiscreteSpecification& training,
                                                const DiscreteConditionalGrammar& grammar) {
    DiscreteConditionalGrammarLimits limits;
    limits.maximumPoolExpressions = 100000;
    limits.maximumConstructionAttempts = 500000;
    limits.maximumCandidateCombinations = 1000000;
    limits.maximumConditionalNodes = 32;
    limits.maximumConditionalDepth = 10;
    limits.maximumEvaluationWork = 20000000;
    limits.maximumEstimatedBytes = 128 * 1024 * 1024;
    return synthesizeDiscreteConditionalFromGrammar(training, grammar, limits);
}

DiscreteConditionalGrammarResult runPartitionDirected(const DiscreteSpecification& training,
                                                      DiscreteConditionalGrammar grammar) {
    grammar.enablePartitionDirectedComposition = true;
    DiscreteConditionalGrammarLimits limits;
    limits.maximumPoolExpressions = 100000;
    limits.maximumConstructionAttempts = 500000;
    limits.maximumCandidateCombinations = 1000000; // Historical Cartesian cap is unchanged.
    limits.maximumPartitionDirectedJoins = 1000000;
    limits.maximumConditionalNodes = 32;
    limits.maximumConditionalDepth = 10;
    limits.maximumEvaluationWork = 20000000;
    limits.maximumEstimatedBytes = 128 * 1024 * 1024;
    return synthesizeDiscreteConditionalFromGrammar(training, grammar, limits);
}

void partitionDirectedMatchesExhaustiveSmallPool() {
    const auto x = DiscreteExpression::variable(0, 1);
    const auto zero = DiscreteExpression::integer(0, 1);
    const auto one = DiscreteExpression::integer(1, 1);
    const auto five = DiscreteExpression::integer(5, 1);
    const auto predicate = DiscreteExpression::binary(DiscreteKind::LessThan, x, zero);
    const auto denominator = DiscreteExpression::binary(DiscreteKind::Add, x, one);
    const auto guardedValue = DiscreteExpression::binary(DiscreteKind::Modulo, x, denominator);
    const DiscreteSpecification training(
        1, {{{-1}, DiscreteValue::integer(5)}, {{1}, DiscreteValue::integer(1)}});
    const std::vector<DiscreteExpression> predicates{predicate};
    const std::vector<DiscreteExpression> branches{five, guardedValue};
    using Trace = genetik_conditional_composition_detail::TrainingTrace;
    std::vector<Trace> predicateTraces, branchTraces;
    const auto trace = [&](const DiscreteSpecification& specification,
                           const DiscreteExpression& expression) {
        Trace output;
        output.boolean = expression.type() == DiscreteType::Bool;
        for (const auto& row : specification.observations()) {
            const auto evaluated = expression.evaluate(row.inputs);
            output.valid.push_back(evaluated.valid() ? 1 : 0);
            output.values.push_back(
                evaluated.valid()
                    ? (output.boolean ? (evaluated.value->boolean() ? 1U : 0U)
                                      : static_cast<std::uint64_t>(evaluated.value->integer()))
                    : 0);
        }
        return output;
    };
    predicateTraces.push_back(trace(training, predicate));
    for (const auto& branch : branches)
        branchTraces.push_back(trace(training, branch));
    DiscreteConditionalCompositionLimits limits;
    limits.maxCandidateCombinations = 4;
    limits.maxPartitionDirectedJoins = 16;
    limits.maxNodes = 16;
    limits.maxDepth = 8;
    limits.maxEvaluationWork = 10000;
    limits.maxEstimatedBytes = 16 * 1024 * 1024;
    const auto exhaustive = composeDiscreteConditional(training, predicates, branches, limits);
    const auto partitioned =
        genetik_conditional_composition_detail::composeDiscreteConditionalPartitionDirected(
            training, predicates, branches, predicateTraces, branchTraces, limits);
    if (partitioned.status != exhaustive.status ||
        partitioned.matchingRows != exhaustive.matchingRows || !partitioned.expression ||
        !exhaustive.expression ||
        partitioned.expression->toString() != exhaustive.expression->toString())
        std::cerr << "small exhaustive=" << static_cast<int>(exhaustive.status) << "/"
                  << exhaustive.matchingRows << "/"
                  << (exhaustive.expression ? exhaustive.expression->toString() : "none")
                  << " partitioned=" << static_cast<int>(partitioned.status) << "/"
                  << partitioned.matchingRows << "/"
                  << (partitioned.expression ? partitioned.expression->toString() : "none")
                  << " diagnostic=" << partitioned.diagnostic << "\n";
    check(exhaustive.status == DiscreteConditionalCompositionStatus::ExactTraining &&
              partitioned.status == exhaustive.status &&
              partitioned.matchingRows == exhaustive.matchingRows && exhaustive.expression &&
              partitioned.expression &&
              partitioned.expression->toString() == exhaustive.expression->toString(),
          "partition joins preserve exhaustive selection, score and tie order on a small pool");
    const auto lazy = partitioned.expression->evaluate(std::array<std::int64_t, 1>{-1});
    check(lazy.valid() && lazy.value->integer() == 5,
          "invalid modulo at an inactive training row is safely masked by the conditional");
    check(partitioned.work.partitionJoinPairs < partitioned.work.combinations &&
              partitioned.work.cartesianCombinationsAvoided > 0,
          "partitioned selector joins fewer structural classes than exhaustive tuples");
    check(partitioned.work.candidateExpressionsConstructed > 0 &&
              partitioned.work.candidateConstructionWork >=
                  partitioned.work.candidateExpressionsConstructed,
          "partitioned conditional materialization has explicit work accounting");
    auto structureLimited = limits;
    structureLimited.maxNodes = 8;
    const auto exhaustiveStructureLimited =
        composeDiscreteConditional(training, predicates, branches, structureLimited);
    const auto partitionedStructureLimited =
        genetik_conditional_composition_detail::composeDiscreteConditionalPartitionDirected(
            training, predicates, branches, predicateTraces, branchTraces, structureLimited);
    check(exhaustiveStructureLimited.status == DiscreteConditionalCompositionStatus::Partial &&
              partitionedStructureLimited.status == exhaustiveStructureLimited.status &&
              partitionedStructureLimited.matchingRows == exhaustiveStructureLimited.matchingRows &&
              exhaustiveStructureLimited.expression && partitionedStructureLimited.expression &&
              exhaustiveStructureLimited.expression->toString() ==
                  partitionedStructureLimited.expression->toString(),
          "partition join retains the exhaustive partial winner under coupled node limits");
    auto exhaustedLimits = limits;
    exhaustedLimits.maxEvaluationWork = 1;
    const auto exhausted =
        genetik_conditional_composition_detail::composeDiscreteConditionalPartitionDirected(
            training, predicates, branches, predicateTraces, branchTraces, exhaustedLimits);
    check(exhausted.status == DiscreteConditionalCompositionStatus::ResourceExhausted &&
              exhausted.work.estimatedBytes > 0,
          "partition indexing work exhaustion remains distinct from mathematical invalidity");
    auto joinLimited = limits;
    joinLimited.maxPartitionDirectedJoins = 1;
    const auto joinExhausted =
        genetik_conditional_composition_detail::composeDiscreteConditionalPartitionDirected(
            training, predicates, branches, predicateTraces, branchTraces, joinLimited);
    check(joinExhausted.status == DiscreteConditionalCompositionStatus::ResourceExhausted,
          "partition structural-join cap is enforced separately from historical tuple cap");
    std::atomic<bool> cancelledNow{true};
    const auto cancelled =
        genetik_conditional_composition_detail::composeDiscreteConditionalPartitionDirected(
            training, predicates, branches, predicateTraces, branchTraces, limits, &cancelledNow);
    check(cancelled.status == DiscreteConditionalCompositionStatus::Cancelled,
          "partition search cancellation remains distinct from resource exhaustion");
    const auto invalidRemainder = DiscreteExpression::binary(DiscreteKind::Modulo, x, zero);
    const std::vector<DiscreteExpression> invalidPredicates{
        DiscreteExpression::binary(DiscreteKind::Equal, invalidRemainder, zero)};
    const std::vector<Trace> invalidPredicateTraces{trace(training, invalidPredicates.front())};
    const auto invalidPredicate =
        genetik_conditional_composition_detail::composeDiscreteConditionalPartitionDirected(
            training, invalidPredicates, branches, invalidPredicateTraces, branchTraces, limits);
    check(invalidPredicate.status == DiscreteConditionalCompositionStatus::Invalid,
          "mathematically invalid predicate traces cannot be hidden by either branch");
    auto evaluatorLimited = limits;
    evaluatorLimited.evaluation.maxNodeVisits = 1;
    const auto exhaustiveEvaluatorLimited =
        composeDiscreteConditional(training, predicates, branches, evaluatorLimited);
    const auto partitionedEvaluatorLimited =
        genetik_conditional_composition_detail::composeDiscreteConditionalPartitionDirected(
            training, predicates, branches, predicateTraces, branchTraces, evaluatorLimited);
    check(exhaustiveEvaluatorLimited.status ==
                  DiscreteConditionalCompositionStatus::ResourceExhausted &&
              partitionedEvaluatorLimited.status ==
                  DiscreteConditionalCompositionStatus::ResourceExhausted,
          "combined selected-expression evaluator quotas preserve resource exhaustion");

    const DiscreteSpecification ambiguous(1, {{{0}, DiscreteValue::integer(3)}});
    const std::vector<DiscreteExpression> constantPredicates{DiscreteExpression::boolean(true, 1),
                                                             DiscreteExpression::boolean(false, 1)};
    const std::vector<DiscreteExpression> partialBranches{DiscreteExpression::integer(1, 1),
                                                          DiscreteExpression::integer(2, 1)};
    predicateTraces.clear();
    branchTraces.clear();
    for (const auto& expression : constantPredicates)
        predicateTraces.push_back(trace(ambiguous, expression));
    for (const auto& expression : partialBranches)
        branchTraces.push_back(trace(ambiguous, expression));
    limits.maxCandidateCombinations = 8;
    limits.maxPartitionDirectedJoins = 8;
    const auto ambiguousExhaustive =
        composeDiscreteConditional(ambiguous, constantPredicates, partialBranches, limits);
    const auto ambiguousPartitioned =
        genetik_conditional_composition_detail::composeDiscreteConditionalPartitionDirected(
            ambiguous, constantPredicates, partialBranches, predicateTraces, branchTraces, limits);
    check(ambiguousExhaustive.status == DiscreteConditionalCompositionStatus::Partial &&
              ambiguousPartitioned.status == ambiguousExhaustive.status &&
              ambiguousPartitioned.matchingRows == ambiguousExhaustive.matchingRows &&
              ambiguousPartitioned.expression && ambiguousExhaustive.expression &&
              ambiguousPartitioned.expression->toString() ==
                  ambiguousExhaustive.expression->toString(),
          "ambiguous partial specifications preserve exhaustive selection and empty partitions");
    for (const auto expected :
         {std::array<std::int64_t, 2>{3, 3}, std::array<std::int64_t, 2>{3, 4}}) {
        const DiscreteSpecification duplicates(1, {{{0}, DiscreteValue::integer(expected[0])},
                                                   {{0}, DiscreteValue::integer(expected[1])}});
        predicateTraces.clear();
        branchTraces.clear();
        for (const auto& expression : constantPredicates)
            predicateTraces.push_back(trace(duplicates, expression));
        for (const auto& expression : partialBranches)
            branchTraces.push_back(trace(duplicates, expression));
        const auto duplicateExhaustive =
            composeDiscreteConditional(duplicates, constantPredicates, partialBranches, limits);
        const auto duplicateResult =
            genetik_conditional_composition_detail::composeDiscreteConditionalPartitionDirected(
                duplicates, constantPredicates, partialBranches, predicateTraces, branchTraces,
                limits);
        check(duplicateExhaustive.status == DiscreteConditionalCompositionStatus::Invalid &&
                  duplicateResult.status == duplicateExhaustive.status,
              "partitioned selection rejects duplicate rows with identical or conflicting labels");
    }
}

std::int64_t euclideanModulo(std::int64_t x, std::int64_t modulus) {
    auto value = x % modulus;
    return value < 0 ? value + modulus : value;
}

void multiRegionGrammarRecovery() {
    // The three specifications and exact finite domains are frozen in
    // docs/MULTIREGION_CONDITIONAL_PROTOCOL.md. Only their generated tables
    // and grammar declarations are passed to synthesis.
    std::vector<DiscreteSpecificationObservation> aRows;
    for (const auto x : {-6, -2, -1, 0, 1, 3, 4, 6}) {
        const auto y = x < 0 ? 10 : euclideanModulo(x, 3) == 0 ? 20 : 30;
        aRows.push_back({{x}, DiscreteValue::integer(y)});
    }
    const DiscreteSpecification familyA(1, std::move(aRows));
    auto grammarA = frozenMultiRegionGrammar(
        1, {0, 3}, {10, 20, 30},
        {DiscreteKind::LessThan, DiscreteKind::Modulo, DiscreteKind::Equal}, {}, 5);
    auto oneSelectorA = grammarA;
    oneSelectorA.maximumConditionalNesting = 0;
    const auto baselineA = runMultiRegion(familyA, oneSelectorA);
    check(baselineA.status == DiscreteConditionalGrammarStatus::Partial &&
              baselineA.matchingRows < familyA.observations().size() && baselineA.expression,
          "matched one-selector baseline cannot fit three constant output regions");
    const auto repeatedBaselineA = runMultiRegion(familyA, oneSelectorA);
    check(repeatedBaselineA.matchingRows == baselineA.matchingRows &&
              repeatedBaselineA.expression &&
              repeatedBaselineA.expression->toString() == baselineA.expression->toString(),
          "single-selector baseline score and selection are deterministic");
    const auto aStarted = std::chrono::steady_clock::now();
    const auto recoveredA = runMultiRegion(familyA, grammarA);
    const auto aMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - aStarted)
            .count();
    check(recoveredA.status == DiscreteConditionalGrammarStatus::ExactTraining &&
              recoveredA.matchingRows == familyA.observations().size() &&
              recoveredA.expression.has_value(),
          "nested grammar recovers sign/modular three-region training table");
    for (const auto x : {-9, -4, -3, 2, 5, 7, 9}) {
        const auto expected = x < 0 ? 10 : euclideanModulo(x, 3) == 0 ? 20 : 30;
        const auto value = recoveredA.expression->evaluate(std::array<std::int64_t, 1>{x});
        check(value.valid() && value.value->integer() == expected,
              "sign/modular selected expression matches held-out observation");
    }
    check(runMultiRegion(familyA, grammarA).expression->toString() ==
              recoveredA.expression->toString(),
          "sign/modular nested recovery tie-breaking is deterministic");
    std::cout << "multiregion A generated=" << recoveredA.work.generatedExpressions
              << " attempts=" << recoveredA.work.constructionAttempts
              << " predicates=" << recoveredA.work.predicatePoolSize
              << " branches=" << recoveredA.work.branchPoolSize
              << " tuples=" << recoveredA.work.composition.combinations
              << " work=" << recoveredA.work.poolEvaluationWork
              << " bytes=" << recoveredA.work.estimatedPeakBytes << " runtime_ms=" << aMilliseconds
              << " expression=" << recoveredA.expression->toString()
              << " baseline_score=" << baselineA.matchingRows << "/" << baselineA.totalRows
              << " baseline_expression=" << baselineA.expression->toString() << "\n";

    std::vector<DiscreteSpecificationObservation> bRows;
    for (const auto& [x, y] : std::array<std::pair<std::int64_t, std::int64_t>, 8>{
             {{-2, -3}, {-2, 1}, {-1, -3}, {-1, 1}, {0, 2}, {1, 3}, {2, 4}, {3, 6}}}) {
        const auto output = euclideanModulo(x, 2) == 0 ? 10 : euclideanModulo(y, 3) == 0 ? 20 : 30;
        bRows.push_back({{x, y}, DiscreteValue::integer(output)});
    }
    const DiscreteSpecification familyB(2, std::move(bRows));
    auto grammarB = frozenMultiRegionGrammar(2, {0, 2, 3}, {10, 20, 30},
                                             {DiscreteKind::Modulo, DiscreteKind::Equal}, {}, 5);
    const auto baselineB = runMultiRegion(familyB, [&] {
        auto g = grammarB;
        g.maximumConditionalNesting = 0;
        return g;
    }());
    check(baselineB.status == DiscreteConditionalGrammarStatus::Partial,
          "matched multivariate one-selector baseline cannot fit three constant regions");
    const auto recoveredB = runMultiRegion(familyB, grammarB);
    check(recoveredB.status == DiscreteConditionalGrammarStatus::ResourceExhausted &&
              recoveredB.work.branchPoolSize == 516 &&
              recoveredB.work.composition.combinations == 0,
          "multivariate modular composition preserves its frozen tuple-cap exhaustion result");
    std::cout << "multiregion B exhausted generated=" << recoveredB.work.generatedExpressions
              << " attempts=" << recoveredB.work.constructionAttempts
              << " predicates=" << recoveredB.work.predicatePoolSize
              << " branches=" << recoveredB.work.branchPoolSize
              << " tuples=" << recoveredB.work.composition.combinations
              << " work=" << recoveredB.work.poolEvaluationWork
              << " bytes=" << recoveredB.work.estimatedPeakBytes
              << " baseline_score=" << baselineB.matchingRows << "/" << baselineB.totalRows
              << " baseline_expression="
              << (baselineB.expression ? baselineB.expression->toString() : "none") << "\n";
    const auto bStarted = std::chrono::steady_clock::now();
    const auto directedB = runPartitionDirected(familyB, grammarB);
    const auto bMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - bStarted)
            .count();
    check(directedB.status == DiscreteConditionalGrammarStatus::ExactTraining &&
              directedB.expression && directedB.matchingRows == familyB.observations().size() &&
              directedB.expression->toString() ==
                  "if((x1 == (x0 % 2)), 30, if((0 == (x0 % 2)), 10, 20))",
          "partition-directed joins recover the frozen multivariate modular control");
    const auto directedBRepeat = runPartitionDirected(familyB, grammarB);
    check(directedBRepeat.status == directedB.status && directedBRepeat.expression &&
              directedBRepeat.expression->toString() == directedB.expression->toString() &&
              directedBRepeat.work.composition.partitionJoinPairs ==
                  directedB.work.composition.partitionJoinPairs &&
              directedBRepeat.work.poolEvaluationWork == directedB.work.poolEvaluationWork,
          "partition-directed result and accounting are deterministic");
    check(directedB.work.composition.combinations == 34U * 516U * 516U &&
              directedB.work.composition.cartesianCombinationsAvoided > 8000000 &&
              directedB.work.composition.partitionJoinPairs < 1000000 &&
              directedB.work.composition.partitionRowsScanned > 0 &&
              directedB.work.poolEvaluationWork < 20000000,
          "partition-directed search avoids Cartesian materialization within separate limits");
    for (const auto& [x, y] : std::array<std::pair<std::int64_t, std::int64_t>, 6>{
             {{-4, 5}, {-3, -6}, {-1, 0}, {0, 1}, {1, -3}, {4, 2}}}) {
        const auto expected = euclideanModulo(x, 2) == 0   ? 10
                              : euclideanModulo(y, 3) == 0 ? 20
                                                           : 30;
        const auto value = directedB.expression->evaluate(std::array<std::int64_t, 2>{x, y});
        check(value.valid() && value.value->integer() == expected,
              "partition-directed multivariate rule matches held-out rows");
    }
    std::cout << "partition B joins=" << directedB.work.composition.partitionJoinPairs
              << " generated=" << directedB.work.generatedExpressions
              << " attempts=" << directedB.work.constructionAttempts
              << " predicates=" << directedB.work.predicatePoolSize
              << " branches=" << directedB.work.branchPoolSize
              << " avoided=" << directedB.work.composition.cartesianCombinationsAvoided
              << " partition_rows=" << directedB.work.composition.partitionRowsScanned
              << " index_comparisons=" << directedB.work.composition.partitionIndexComparisons
              << " candidate_expressions="
              << directedB.work.composition.candidateExpressionsConstructed
              << " candidate_construction_work="
              << directedB.work.composition.candidateConstructionWork
              << " evaluator_rows=" << directedB.work.composition.evaluatedRows
              << " evaluator_work="
              << directedB.work.composition.nodeVisits + directedB.work.composition.operations
              << " work=" << directedB.work.poolEvaluationWork
              << " logical_bytes=" << directedB.work.estimatedPeakBytes
              << " runtime_ms=" << bMilliseconds
              << " expression=" << directedB.expression->toString() << "\n";
    std::vector<DiscreteSpecificationObservation> cRows;
    for (const auto x : {-3, -1, 0, 1, 2}) {
        const auto output = x == 0 ? 7 : x < 0 ? euclideanModulo(x, -x) : -1;
        cRows.push_back({{x}, DiscreteValue::integer(output)});
    }
    const DiscreteSpecification familyC(1, std::move(cRows));
    auto grammarC =
        frozenMultiRegionGrammar(1, {0}, {7, 0, -1}, {DiscreteKind::Equal, DiscreteKind::LessThan},
                                 {DiscreteKind::Modulo, DiscreteKind::Negate}, 3);
    auto oneSelectorC = grammarC;
    oneSelectorC.maximumConditionalNesting = 0;
    check(runMultiRegion(familyC, oneSelectorC).status == DiscreteConditionalGrammarStatus::Partial,
          "guarded control requires more than one output selector");
    const auto recoveredC = runMultiRegion(familyC, grammarC);
    check(recoveredC.status == DiscreteConditionalGrammarStatus::ResourceExhausted &&
              recoveredC.work.generatedExpressions == 82774 &&
              recoveredC.work.constructionAttempts == 41532,
          "expansive guarded modulo grammar retains its frozen aggregate-work exhaustion");

    constexpr auto lo = std::numeric_limits<std::int64_t>::min();
    std::vector<DiscreteSpecificationObservation> dRows;
    for (const auto x : std::array<std::int64_t, 5>{lo, -3, -1, 0, 2})
        dRows.push_back({{x}, DiscreteValue::integer(x == lo ? 7 : x < 0 ? -x : -1)});
    const DiscreteSpecification familyD(1, std::move(dRows));
    auto grammarD =
        frozenMultiRegionGrammar(1, {lo, 0}, {7, -1}, {DiscreteKind::Equal, DiscreteKind::LessThan},
                                 {DiscreteKind::Negate}, 3, 7);
    grammarD.expressionLimits.maxConstantMagnitude = std::uint64_t{1} << 63;
    auto baselineDGrammar = grammarD;
    baselineDGrammar.maximumConditionalNesting = 0;
    const auto baselineD = runMultiRegion(familyD, baselineDGrammar);
    check(baselineD.status == DiscreteConditionalGrammarStatus::Partial,
          "matched single-selector guarded overflow baseline is partial");
    check(baselineD.expression.has_value(),
          "matched guarded baseline returns its best partial candidate");
    const auto dStarted = std::chrono::steady_clock::now();
    const auto recoveredD = runMultiRegion(familyD, grammarD);
    const auto dMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - dStarted)
            .count();
    if (recoveredD.status != DiscreteConditionalGrammarStatus::ExactTraining)
        std::cerr << "guarded overflow status=" << static_cast<int>(recoveredD.status)
                  << " matches=" << recoveredD.matchingRows << "/" << recoveredD.totalRows
                  << " diagnostic=" << recoveredD.diagnostic
                  << " generated=" << recoveredD.work.generatedExpressions
                  << " attempts=" << recoveredD.work.constructionAttempts
                  << " predicates=" << recoveredD.work.predicatePoolSize
                  << " branches=" << recoveredD.work.branchPoolSize
                  << " combinations=" << recoveredD.work.composition.combinations << "\n";
    check(recoveredD.status == DiscreteConditionalGrammarStatus::ExactTraining &&
              recoveredD.expression && recoveredD.work.invalidExpressions > 0,
          "nested grammar recovers guarded invalid-unselected overflow branch");
    const auto repeatedD = runMultiRegion(familyD, grammarD);
    check(repeatedD.status == recoveredD.status && repeatedD.expression &&
              repeatedD.expression->toString() == recoveredD.expression->toString(),
          "guarded nested recovery tie-breaking is deterministic");
    std::atomic<bool> stopNested{true};
    DiscreteConditionalGrammarLimits nestedLimits;
    nestedLimits.maximumPoolExpressions = 100000;
    nestedLimits.maximumConstructionAttempts = 500000;
    nestedLimits.maximumCandidateCombinations = 1000000;
    nestedLimits.maximumEvaluationWork = 20000000;
    nestedLimits.maximumEstimatedBytes = 128 * 1024 * 1024;
    const auto cancelled =
        synthesizeDiscreteConditionalFromGrammar(familyD, grammarD, nestedLimits, &stopNested);
    check(cancelled.status == DiscreteConditionalGrammarStatus::Cancelled,
          "opt-in nested grammar preserves cancellation status");
    nestedLimits.maximumConstructionAttempts = 1;
    const auto exhausted =
        synthesizeDiscreteConditionalFromGrammar(familyD, grammarD, nestedLimits);
    check(exhausted.status == DiscreteConditionalGrammarStatus::ResourceExhausted,
          "opt-in nested grammar preserves construction-attempt exhaustion");
    for (const auto x : std::array<std::int64_t, 5>{lo + 1, -8, -2, 1, 9}) {
        const auto expected = x == lo ? 7 : x < 0 ? -x : -1;
        const auto value = recoveredD.expression->evaluate(std::array<std::int64_t, 1>{x});
        check(value.valid() && value.value->integer() == expected,
              "guarded overflow rule matches held-out values");
    }
    check(recoveredA.expression->outerBindings().empty() &&
              recoveredD.expression->outerBindings().empty(),
          "automatically composed conditionals remain lexically closed");
    std::cout << "multiregion D generated=" << recoveredD.work.generatedExpressions
              << " attempts=" << recoveredD.work.constructionAttempts
              << " predicates=" << recoveredD.work.predicatePoolSize
              << " branches=" << recoveredD.work.branchPoolSize
              << " tuples=" << recoveredD.work.composition.combinations
              << " work=" << recoveredD.work.poolEvaluationWork
              << " bytes=" << recoveredD.work.estimatedPeakBytes << " runtime_ms=" << dMilliseconds
              << " expression=" << recoveredD.expression->toString()
              << " baseline_score=" << baselineD.matchingRows << "/" << baselineD.totalRows
              << " baseline_expression=" << baselineD.expression->toString() << "\n";
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

    auto excessiveNesting = incompleteTyped;
    excessiveNesting.maximumConditionalNesting = 9;
    const auto invalidBound = synthesize(training, excessiveNesting);
    check(invalidBound.status == DiscreteConditionalGrammarStatus::Invalid,
          "nested conditional grammar rejects nesting beyond its fixed cap");

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

void specificationPruningSanitizerControls() {
    const auto conjunctionTraining = twoInputSpecification({-6, -3, -1, 0, 1, 3, 6}, {-2, -1, 1, 2},
                                                           [](std::int64_t x, std::int64_t y) {
                                                               auto residue = x % 3;
                                                               if (residue < 0)
                                                                   residue += 3;
                                                               return residue == 0 && y < 0 ? 1 : 0;
                                                           });
    auto conjunctionGrammar = composedPredicateGrammar(1, {0, 3}, {false}, true);
    conjunctionGrammar.enableSpecificationGuidedPruning = true;
    const auto conjunction = synthesizeComposed(conjunctionTraining, conjunctionGrammar);
    check(conjunction.status == DiscreteConditionalGrammarStatus::ExactTraining &&
              conjunction.expression &&
              conjunction.expression->toString() ==
                  "if(if((x1 < 0), (0 == (x0 % 3)), false), 1, 0)",
          "ASan conjunction pruning preserves the qualified exact rule");

    const auto disjunctionTraining = twoInputSpecification(
        {-2, -1, 0, 1, 2}, {-3, -2, -1, 0, 1, 2, 3}, [](std::int64_t x, std::int64_t y) {
            auto residue = y % 2;
            if (residue < 0)
                residue += 2;
            return x < 0 || residue == 0 ? 1 : 0;
        });
    auto disjunctionGrammar = composedPredicateGrammar(1, {0, 2}, {true}, false, true);
    disjunctionGrammar.enableSpecificationGuidedPruning = true;
    const auto disjunction = synthesizeComposed(disjunctionTraining, disjunctionGrammar);
    check(disjunction.status == DiscreteConditionalGrammarStatus::ExactTraining &&
              disjunction.expression &&
              disjunction.expression->toString() == "if(if((x0 < 0), true, (0 == (x1 % 2))), 1, 0)",
          "ASan disjunction pruning preserves the qualified exact rule");
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
    const auto conjunctionStart = std::chrono::steady_clock::now();
    const auto conjunction = synthesizeComposed(conjunctionTraining, conjunctionGrammar);
    const auto conjunctionMilliseconds = std::chrono::duration<double, std::milli>(
                                             std::chrono::steady_clock::now() - conjunctionStart)
                                             .count();
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
    check(conjunction.work.predicateCandidatesBeforeReduction == 19810 &&
              conjunction.work.branchCandidatesBeforeReduction == 4 &&
              conjunction.work.trainingTraceEvaluations == 554792 &&
              conjunction.work.generatedExpressions == 40449 &&
              conjunction.work.constructionAttempts == 20625 &&
              conjunction.work.composition.combinations == 4448 &&
              conjunction.work.poolEvaluationWork == 2858764 &&
              conjunction.work.cachedTraceNodeVisits == 555268 &&
              conjunction.work.cachedTraceOperations == 407772 &&
              conjunction.work.cachedTraceArithmeticComparisonOperations == 8576 &&
              conjunction.work.cachedTraceConditionalOperations == 399196 &&
              conjunction.work.signatureRowTouches == 554792 &&
              conjunction.work.cacheLookups == 191721 &&
              conjunction.work.cacheConstructionWork == 575099 &&
              conjunction.work.cachedTraceReuses == 171890 &&
              conjunction.work.evaluatorNodeVisits == 402684 &&
              conjunction.work.evaluatorOperations == 170560 &&
              conjunction.work.estimatedPeakBytes == 107382682,
          "conjunction trace profile remains reproducible");
    auto prunedConjunctionGrammar = conjunctionGrammar;
    prunedConjunctionGrammar.enableSpecificationGuidedPruning = true;
    const auto prunedConjunctionStart = std::chrono::steady_clock::now();
    const auto prunedConjunction =
        synthesizeComposed(conjunctionTraining, prunedConjunctionGrammar);
    const auto prunedConjunctionMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                  prunedConjunctionStart)
            .count();
    check(prunedConjunction.status == conjunction.status &&
              prunedConjunction.matchingRows == conjunction.matchingRows &&
              prunedConjunction.expression && conjunction.expression &&
              prunedConjunction.expression->toString() == conjunction.expression->toString() &&
              prunedConjunction.work.composition.prunedCombinations > 0 &&
              prunedConjunction.work.poolEvaluationWork == 2372906 &&
              prunedConjunction.work.composition.pruningWork == 5584 &&
              prunedConjunction.work.composition.prunedCombinations == 3840 &&
              prunedConjunction.work.composition.evaluatedCombinations == 608 &&
              prunedConjunction.work.composition.prunedCombinations +
                      prunedConjunction.work.composition.evaluatedCombinations ==
                  prunedConjunction.work.composition.combinations &&
              prunedConjunction.work.estimatedPeakBytes == 107385570 &&
              prunedConjunction.work.poolEvaluationWork < conjunction.work.poolEvaluationWork,
          "specification pruning preserves the conjunction winner and removes dominated "
          "combinations");
    std::cout << "pruning conjunction baseline=" << conjunction.work.poolEvaluationWork
              << " pruned=" << prunedConjunction.work.composition.prunedCombinations
              << " evaluated=" << prunedConjunction.work.composition.evaluatedCombinations
              << " work=" << prunedConjunction.work.poolEvaluationWork
              << " prune_work=" << prunedConjunction.work.composition.pruningWork << " evaluator="
              << (prunedConjunction.work.composition.nodeVisits +
                  prunedConjunction.work.composition.operations)
              << " baseline_ms=" << conjunctionMilliseconds
              << " pruned_ms=" << prunedConjunctionMilliseconds
              << " bytes=" << prunedConjunction.work.estimatedPeakBytes << '\n';
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
    const auto disjunctionStart = std::chrono::steady_clock::now();
    const auto disjunction = synthesizeComposed(disjunctionTraining, disjunctionGrammar);
    const auto disjunctionMilliseconds = std::chrono::duration<double, std::milli>(
                                             std::chrono::steady_clock::now() - disjunctionStart)
                                             .count();
    check(disjunction.status == DiscreteConditionalGrammarStatus::ExactTraining &&
              disjunction.expression,
          "composed independent arithmetic conditions recover the disjunction training grid");
    check(std::count_if(
              disjunction.expression->nodes().begin(), disjunction.expression->nodes().end(),
              [](const auto& node) { return node.kind() == DiscreteKind::IfThenElse; }) >= 2,
          "selected disjunction contains a composed Boolean predicate beneath the outer selector");
    check(disjunction.work.predicateCandidatesBeforeReduction == 19810 &&
              disjunction.work.branchCandidatesBeforeReduction == 4 &&
              disjunction.work.trainingTraceEvaluations == 693490 &&
              disjunction.work.generatedExpressions == 40449 &&
              disjunction.work.constructionAttempts == 20625 &&
              disjunction.work.composition.combinations == 10688 &&
              disjunction.work.poolEvaluationWork == 4776342 &&
              disjunction.work.cachedTraceNodeVisits == 694085 &&
              disjunction.work.cachedTraceOperations == 501235 &&
              disjunction.work.cachedTraceArithmeticComparisonOperations == 10064 &&
              disjunction.work.cachedTraceConditionalOperations == 491171 &&
              disjunction.work.signatureRowTouches == 693490 &&
              disjunction.work.cacheLookups == 191721 &&
              disjunction.work.cacheConstructionWork == 713916 &&
              disjunction.work.cachedTraceReuses == 171890 &&
              disjunction.work.evaluatorNodeVisits == 1379881 &&
              disjunction.work.evaluatorOperations == 600579 &&
              disjunction.work.estimatedPeakBytes == 113751847,
          "disjunction trace profile remains reproducible");
    auto prunedDisjunctionGrammar = disjunctionGrammar;
    prunedDisjunctionGrammar.enableSpecificationGuidedPruning = true;
    const auto prunedDisjunctionStart = std::chrono::steady_clock::now();
    const auto prunedDisjunction =
        synthesizeComposed(disjunctionTraining, prunedDisjunctionGrammar);
    const auto prunedDisjunctionMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                  prunedDisjunctionStart)
            .count();
    check(prunedDisjunction.status == disjunction.status &&
              prunedDisjunction.matchingRows == disjunction.matchingRows &&
              prunedDisjunction.expression && disjunction.expression &&
              prunedDisjunction.expression->toString() == disjunction.expression->toString() &&
              prunedDisjunction.work.composition.prunedCombinations > 0 &&
              prunedDisjunction.work.poolEvaluationWork == 3110215 &&
              prunedDisjunction.work.composition.pruningWork == 12104 &&
              prunedDisjunction.work.composition.prunedCombinations == 9082 &&
              prunedDisjunction.work.composition.evaluatedCombinations == 1606 &&
              prunedDisjunction.work.composition.prunedCombinations +
                      prunedDisjunction.work.composition.evaluatedCombinations ==
                  prunedDisjunction.work.composition.combinations &&
              prunedDisjunction.work.estimatedPeakBytes == 113755043 &&
              prunedDisjunction.work.poolEvaluationWork < disjunction.work.poolEvaluationWork,
          "specification pruning preserves the disjunction winner and removes dominated "
          "combinations");
    std::cout << "pruning disjunction baseline=" << disjunction.work.poolEvaluationWork
              << " pruned=" << prunedDisjunction.work.composition.prunedCombinations
              << " evaluated=" << prunedDisjunction.work.composition.evaluatedCombinations
              << " work=" << prunedDisjunction.work.poolEvaluationWork
              << " prune_work=" << prunedDisjunction.work.composition.pruningWork << " evaluator="
              << (prunedDisjunction.work.composition.nodeVisits +
                  prunedDisjunction.work.composition.operations)
              << " baseline_ms=" << disjunctionMilliseconds
              << " pruned_ms=" << prunedDisjunctionMilliseconds
              << " bytes=" << prunedDisjunction.work.estimatedPeakBytes << '\n';
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
        const auto firstPruned =
            prunedConjunction.expression->evaluate(std::array<std::int64_t, 2>{x, y});
        const auto secondPruned =
            prunedDisjunction.expression->evaluate(std::array<std::int64_t, 2>{x, y});
        check(firstValue.valid() && firstValue.value->integer() == (residue3 == 0 && y < 0 ? 1 : 0),
              "conjunction agrees on held-out negative and modulo-boundary observations");
        check(secondValue.valid() &&
                  secondValue.value->integer() == (x < 0 || residue2 == 0 ? 1 : 0),
              "disjunction agrees on held-out independent arithmetic observations");
        check(firstPruned.valid() && firstPruned.value->integer() == firstValue.value->integer() &&
                  secondPruned.valid() &&
                  secondPruned.value->integer() == secondValue.value->integer(),
              "pruned selections preserve both held-out recovery outcomes");
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
    const auto integerZero = DiscreteExpression::integer(0, 1);
    const auto integerZeroKey = genetik_conditional_grammar_detail::behaviorKey(
        integerZero, training, {}, work, 1000, nullptr, valid, exhausted);
    check(valid && !exhausted && validKey != integerZeroKey,
          "typed trace signatures keep Boolean false distinct from integer zero");
    const auto invalidKey = genetik_conditional_grammar_detail::behaviorKey(
        invalidBoolean, training, {}, work, 1000, nullptr, valid, exhausted);
    check(!valid && !exhausted && validKey != invalidKey &&
              invalidKey.valid == std::vector<std::uint8_t>({0, 0}),
          "finite training-behavior reduction keeps invalid rows distinct from false values");

    check(!genetik_conditional_grammar_detail::componentFitsConditionalDepth(6, 6) &&
              genetik_conditional_grammar_detail::componentFitsConditionalDepth(5, 6),
          "a smaller too-deep trace representative yields to a larger feasible one");
}

void compositionalTraceDifferential() {
    const auto minimum = std::numeric_limits<std::int64_t>::min();
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const std::vector<std::int64_t> inputs{minimum, minimum + 1, -7, -4, -3, -2,          -1,     0,
                                           1,       2,           3,  4,  7,  maximum - 1, maximum};
    std::vector<DiscreteSpecificationObservation> rows;
    for (const auto input : inputs)
        rows.push_back({{input}, DiscreteValue::integer(0)});
    const DiscreteSpecification training(1, std::move(rows));
    const auto x = DiscreteExpression::variable(0, 1);
    const auto zero = DiscreteExpression::integer(0, 1);
    const auto one = DiscreteExpression::integer(1, 1);
    const auto two = DiscreteExpression::integer(2, 1);
    const auto three = DiscreteExpression::integer(3, 1);
    const auto intMax = DiscreteExpression::integer(maximum, 1);
    const auto intMin = DiscreteExpression::integer(minimum, 1);
    const auto negative = DiscreteExpression::binary(DiscreteKind::LessThan, x, zero);
    const auto invalidModulo = DiscreteExpression::binary(DiscreteKind::Modulo, x, zero);
    const auto invalidComparison =
        DiscreteExpression::binary(DiscreteKind::Equal, invalidModulo, zero);
    const auto skippedInvalid = DiscreteExpression::conditional(
        negative, DiscreteExpression::boolean(true, 1), invalidComparison);
    const auto selectedInvalid = DiscreteExpression::conditional(
        negative, invalidComparison, DiscreteExpression::boolean(false, 1));
    const auto invalidCondition =
        DiscreteExpression::conditional(invalidComparison, DiscreteExpression::boolean(true, 1),
                                        DiscreteExpression::boolean(false, 1));
    const auto notNegative = DiscreteExpression::conditional(
        negative, DiscreteExpression::boolean(false, 1), DiscreteExpression::boolean(true, 1));
    const auto conjunction = DiscreteExpression::conditional(negative, notNegative,
                                                             DiscreteExpression::boolean(false, 1));
    const auto nested = DiscreteExpression::conditional(
        conjunction, DiscreteExpression::boolean(true, 1), skippedInvalid);
    const std::vector<DiscreteExpression> expressions{
        x,
        DiscreteExpression::binary(DiscreteKind::Add, x, one),
        DiscreteExpression::binary(DiscreteKind::Subtract, x, one),
        DiscreteExpression::binary(DiscreteKind::Multiply, x, two),
        DiscreteExpression::unary(DiscreteKind::Negate, x),
        DiscreteExpression::binary(DiscreteKind::Modulo, x, three),
        invalidModulo,
        DiscreteExpression::binary(DiscreteKind::Add, x, intMax),
        DiscreteExpression::binary(DiscreteKind::Subtract, x, intMin),
        DiscreteExpression::binary(DiscreteKind::Equal, x, zero),
        DiscreteExpression::binary(DiscreteKind::NotEqual, x, zero),
        DiscreteExpression::binary(DiscreteKind::LessThan, x, zero),
        DiscreteExpression::binary(DiscreteKind::LessEqual, x, zero),
        DiscreteExpression::binary(DiscreteKind::GreaterThan, x, zero),
        DiscreteExpression::binary(DiscreteKind::GreaterEqual, x, zero),
        skippedInvalid,
        selectedInvalid,
        invalidCondition,
        notNegative,
        conjunction,
        nested,
    };
    for (const auto& expression : expressions) {
        std::size_t charged = training.observations().size();
        std::size_t peakBytes = 0;
        DiscreteConditionalGrammarWork counters;
        genetik_conditional_grammar_detail::TrainingTraceCache cache(
            training, 10000000, 32 * 1024 * 1024, charged, counters, peakBytes, {}, nullptr);
        genetik_conditional_grammar_detail::BehaviorSignature trace;
        bool valid = false;
        check(cache.signature(expression, trace, valid) ==
                  genetik_conditional_grammar_detail::TrainingTraceCache::Status::Ready,
              "compositional trace construction completes within test bounds");
        check(trace.boolean == (expression.type() == DiscreteType::Bool) &&
                  valid == std::all_of(trace.valid.begin(), trace.valid.end(),
                                       [](std::uint8_t value) { return value != 0; }),
              "cached trace preserves its result type and aggregate validity");
        for (std::size_t row = 0; row < training.observations().size(); ++row) {
            const auto evaluated = expression.evaluate(training.observations()[row].inputs);
            check(trace.valid[row] == (evaluated.valid() ? 1 : 0),
                  "cached trace validity matches the reference evaluator row by row");
            const auto expected =
                evaluated.valid() ? (evaluated.value->type() == DiscreteType::Bool
                                         ? (evaluated.value->boolean() ? 1ULL : 0ULL)
                                         : static_cast<std::uint64_t>(evaluated.value->integer()))
                                  : 0ULL;
            check(trace.values[row] == expected,
                  "cached trace values match checked evaluator results row by row");
        }
    }

    std::size_t charged = training.observations().size();
    std::size_t peakBytes = 0;
    DiscreteConditionalGrammarWork counters;
    DiscreteEvaluationLimits limits;
    limits.maxOperations = 0;
    genetik_conditional_grammar_detail::TrainingTraceCache operationLimited(
        training, 100000, 32 * 1024 * 1024, charged, counters, peakBytes, limits, nullptr);
    genetik_conditional_grammar_detail::BehaviorSignature ignored;
    bool valid = false;
    check(operationLimited.signature(expressions[1], ignored, valid) ==
              genetik_conditional_grammar_detail::TrainingTraceCache::Status::ResourceExhausted,
          "cached evaluation preserves per-expression operation exhaustion");

    limits = {};
    limits.maxNodeVisits = 1;
    charged = training.observations().size();
    peakBytes = 0;
    counters = {};
    genetik_conditional_grammar_detail::TrainingTraceCache visitLimited(
        training, 100000, 32 * 1024 * 1024, charged, counters, peakBytes, limits, nullptr);
    check(visitLimited.signature(expressions[1], ignored, valid) ==
              genetik_conditional_grammar_detail::TrainingTraceCache::Status::ResourceExhausted,
          "cached evaluation preserves per-expression node-visit exhaustion");

    limits = {};
    limits.maxWorkspaceBytes = DiscreteExpression::evaluationFrameBytes();
    charged = training.observations().size();
    peakBytes = 0;
    counters = {};
    genetik_conditional_grammar_detail::TrainingTraceCache workspaceLimited(
        training, 100000, 32 * 1024 * 1024, charged, counters, peakBytes, limits, nullptr);
    check(workspaceLimited.signature(expressions[1], ignored, valid) ==
              genetik_conditional_grammar_detail::TrainingTraceCache::Status::ResourceExhausted,
          "cached evaluation preserves evaluator frame-workspace exhaustion");

    limits = {};
    limits.maxOperations = 2;
    const DiscreteSpecification negativeOnly(1, {{{-1}, DiscreteValue::integer(0)}});
    charged = negativeOnly.observations().size();
    peakBytes = 0;
    counters = {};
    genetik_conditional_grammar_detail::TrainingTraceCache lazyWithinLimit(
        negativeOnly, 100000, 32 * 1024 * 1024, charged, counters, peakBytes, limits, nullptr);
    check(lazyWithinLimit.signature(skippedInvalid, ignored, valid) ==
              genetik_conditional_grammar_detail::TrainingTraceCache::Status::Ready,
          "cached per-expression limits count only the selected lazy conditional branch");

    const auto expensiveRight = DiscreteExpression::binary(
        DiscreteKind::Multiply, DiscreteExpression::binary(DiscreteKind::Add, x, one),
        DiscreteExpression::binary(DiscreteKind::Add, x, two));
    const auto invalidLeft =
        DiscreteExpression::binary(DiscreteKind::Add, invalidModulo, expensiveRight);
    limits = {};
    limits.maxNodeVisits = 4;
    limits.maxOperations = 1;
    charged = training.observations().size();
    peakBytes = 0;
    counters = {};
    genetik_conditional_grammar_detail::TrainingTraceCache strictShortCircuit(
        training, 1000000, 32 * 1024 * 1024, charged, counters, peakBytes, limits, nullptr);
    check(strictShortCircuit.signature(invalidLeft, ignored, valid) ==
                  genetik_conditional_grammar_detail::TrainingTraceCache::Status::Ready &&
              !valid,
          "cached strict operation skips its right subtree after an invalid left operand");

    limits = {};
    limits.maxAggregateIterations = 1000000001;
    charged = training.observations().size();
    peakBytes = 0;
    counters = {};
    genetik_conditional_grammar_detail::TrainingTraceCache invalidLimits(
        training, 100000, 32 * 1024 * 1024, charged, counters, peakBytes, limits, nullptr);
    bool cacheRejectedLimits = false;
    try {
        invalidLimits.signature(expressions[0], ignored, valid);
    } catch (const std::invalid_argument&) {
        cacheRejectedLimits = true;
    }
    bool evaluatorRejectedLimits = false;
    try {
        (void)expressions[0].evaluate(training.observations()[0].inputs, limits);
    } catch (const std::invalid_argument&) {
        evaluatorRejectedLimits = true;
    }
    check(cacheRejectedLimits && evaluatorRejectedLimits,
          "cached and reference evaluation reject the same malformed iteration quotas");

    charged = training.observations().size();
    peakBytes = 0;
    counters = {};
    genetik_conditional_grammar_detail::TrainingTraceCache workLimited(
        training, 1, 32 * 1024 * 1024, charged, counters, peakBytes, {}, nullptr);
    check(workLimited.signature(expressions[0], ignored, valid) ==
              genetik_conditional_grammar_detail::TrainingTraceCache::Status::ResourceExhausted,
          "aggregate cache-work exhaustion is distinct from cancellation and invalidity");

    std::atomic<bool> stop{true};
    charged = training.observations().size();
    peakBytes = 0;
    counters = {};
    genetik_conditional_grammar_detail::TrainingTraceCache cancelled(
        training, 100000, 32 * 1024 * 1024, charged, counters, peakBytes, {}, &stop);
    check(cancelled.signature(expressions[0], ignored, valid) ==
              genetik_conditional_grammar_detail::TrainingTraceCache::Status::Cancelled,
          "cache cancellation is distinct from mathematical invalidity");
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--composed-pruning") {
            composedPredicateRecovery();
            return EXIT_SUCCESS;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--pruned-only") {
            specificationPruningSanitizerControls();
            return EXIT_SUCCESS;
        }
        partitionDirectedMatchesExhaustiveSmallPool();
        modularPiecewiseRecovery();
        arithmeticPiecewiseRecovery();
        multiRegionGrammarRecovery();
        negativeAndResourceControls();
        lazyOverflowRecovery();
        invalidArithmeticAndLazyBranches();
        composedPredicateRecovery();
        booleanNotProduction();
        invalidTraceDistinction();
        compositionalTraceDifferential();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
