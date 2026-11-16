#include <DiscreteExpressionSpecification.hpp>
#include <DiscreteExpressionDomainComparison.hpp>
#include <DiscreteExpressionParsing.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

DiscreteExpression identityPlusTwo() {
    const std::array nodes{DiscreteNode::variable(0), DiscreteNode::integer(2),
                           DiscreteNode::operation(DiscreteKind::Add)};
    return {nodes, 1};
}

DiscreteExpression moduloThree() {
    const std::array nodes{DiscreteNode::variable(0), DiscreteNode::integer(3),
                           DiscreteNode::operation(DiscreteKind::Modulo)};
    return {nodes, 1};
}

DiscreteExpression absoluteValue() {
    const std::array nodes{DiscreteNode::variable(0),
                           DiscreteNode::integer(0),
                           DiscreteNode::operation(DiscreteKind::LessThan),
                           DiscreteNode::integer(0),
                           DiscreteNode::variable(0),
                           DiscreteNode::operation(DiscreteKind::Subtract),
                           DiscreteNode::variable(0),
                           DiscreteNode::operation(DiscreteKind::IfThenElse)};
    return {nodes, 1};
}

DiscreteExpression overfitAbsoluteValue() {
    const std::array nodes{DiscreteNode::variable(0),
                           DiscreteNode::integer(-2),
                           DiscreteNode::operation(DiscreteKind::LessEqual),
                           DiscreteNode::integer(0),
                           DiscreteNode::variable(0),
                           DiscreteNode::operation(DiscreteKind::Subtract),
                           DiscreteNode::variable(0),
                           DiscreteNode::operation(DiscreteKind::IfThenElse)};
    return {nodes, 1};
}

DiscreteExpression conditionalWithInvalidBranch(bool invalidWhenTrue) {
    const auto invalid = [&] {
        const std::array nodes{DiscreteNode::variable(0), DiscreteNode::integer(0),
                               DiscreteNode::operation(DiscreteKind::Modulo)};
        return DiscreteExpression(nodes, 1);
    }();
    const auto good = [&] {
        const std::array nodes{DiscreteNode::variable(0), DiscreteNode::integer(1),
                               DiscreteNode::operation(DiscreteKind::Add)};
        return DiscreteExpression(nodes, 1);
    }();
    const std::array predicateNodes{DiscreteNode::variable(0), DiscreteNode::integer(0),
                                    DiscreteNode::operation(DiscreteKind::LessThan)};
    const DiscreteExpression predicate(predicateNodes, 1);
    return DiscreteExpression::conditional(predicate, invalidWhenTrue ? invalid : good,
                                           invalidWhenTrue ? good : invalid);
}

std::vector<DiscreteSpecificationObservation> absRows() {
    std::vector<DiscreteSpecificationObservation> rows;
    for (const auto x : {-2, -1, 0, 1, 2})
        rows.push_back({{x}, DiscreteValue::integer(x < 0 ? -x : x)});
    return rows;
}

void exactControls() {
    std::vector<DiscreteSpecificationObservation> arithmetic;
    for (const auto x : {-2, -1, 0, 1, 2})
        arithmetic.push_back({{x}, DiscreteValue::integer(x + 2)});
    DiscreteSpecification arithmeticSpec(1, arithmetic);
    const auto arithmeticResult = validateDiscreteSpecification(arithmeticSpec, identityPlusTwo());
    check(arithmeticResult.status == DiscreteSpecificationValidationStatus::ValidatedFiniteDomain,
          "arithmetic identity validates");
    check(arithmeticResult.certificate && arithmeticResult.certificate->checkedRows == 5,
          "arithmetic certificate covers all rows");

    std::vector<DiscreteSpecificationObservation> modulo;
    for (std::int64_t x = -5; x <= 5; ++x) {
        const auto remainder = ((x % 3) + 3) % 3;
        modulo.push_back({{x}, DiscreteValue::integer(remainder)});
    }
    modulo.push_back({{-7}, DiscreteValue::integer(2)});
    DiscreteSpecification moduloSpec(1, modulo);
    const auto moduloResult = validateDiscreteSpecification(moduloSpec, moduloThree());
    check(moduloResult.status == DiscreteSpecificationValidationStatus::ValidatedFiniteDomain,
          "Euclidean modulo including negative operand validates");
    check(moduloResult.certificate && moduloResult.certificate->checkedRows == modulo.size(),
          "modulo certificate covers all rows");
    check(modulo.size() == 12 && moduloResult.work.rowsMatched == 12,
          "modulo certificate includes the independently specified -7 row");

    const auto conditional =
        validateDiscreteSpecification(DiscreteSpecification(1, absRows()), absoluteValue());
    check(conditional.status == DiscreteSpecificationValidationStatus::ValidatedFiniteDomain,
          "conditional true and false branches validate");
    check(conditional.work.rowsMatched == 5, "conditional checks boundary row");

    const std::array boolNodes{DiscreteNode::variable(0), DiscreteNode::integer(0),
                               DiscreteNode::operation(DiscreteKind::Equal)};
    const DiscreteExpression boolCandidate(boolNodes, 1);
    DiscreteSpecification boolSpec(1, {{{-1}, DiscreteValue::boolean(false)},
                                       {{0}, DiscreteValue::boolean(true)},
                                       {{1}, DiscreteValue::boolean(false)}});
    const auto boolResult = validateDiscreteSpecification(boolSpec, boolCandidate);
    check(boolResult.status == DiscreteSpecificationValidationStatus::ValidatedFiniteDomain &&
              boolResult.certificate && boolResult.certificate->checkedRows == 3,
          "Boolean outputs compare both exact values");
}

void counterexampleAndReplay() {
    DiscreteSpecification specification(1, absRows());
    const auto candidate = overfitAbsoluteValue();
    const auto first = validateDiscreteSpecification(specification, candidate);
    const auto second = validateDiscreteSpecification(specification, candidate);
    check(first.status == DiscreteSpecificationValidationStatus::CounterexampleFound,
          "historical overfit is rejected");
    check(first.counterexample && first.counterexample->ordinal == 1 &&
              first.counterexample->inputs == std::vector<std::int64_t>{-1} &&
              first.counterexample->expected == DiscreteValue::integer(1) &&
              first.counterexample->actual == DiscreteValue::integer(-1) &&
              first.counterexample->specificationIdentity == specification.identity() &&
              first.counterexample->candidateIdentity.find("dexpr-v1;") == 0,
          "first counterexample is x=-1 with exact expected and actual values");
    check(second.status == first.status && second.counterexample->ordinal == 1 &&
              second.work.rowsAttempted == first.work.rowsAttempted &&
              second.work.nodeVisits == first.work.nodeVisits &&
              second.work.operations == first.work.operations,
          "counterexample and accounting replay deterministically");

    std::vector<DiscreteSpecificationObservation> trainingSubset;
    for (const auto x : {-2, 0, 1, 2})
        trainingSubset.push_back({{x}, DiscreteValue::integer(x < 0 ? -x : x)});
    check(
        validateDiscreteSpecification(DiscreteSpecification(1, trainingSubset), candidate).status ==
            DiscreteSpecificationValidationStatus::ValidatedFiniteDomain,
        "candidate demonstrates agreement on declared training subset only");
}

std::int64_t familyBReference(std::int64_t x, std::int64_t y) {
    const auto euclidean = [](std::int64_t value, std::int64_t divisor) {
        const auto remainder = value % divisor;
        return remainder < 0 ? remainder + divisor : remainder;
    };
    if (euclidean(x, 2) == 0)
        return 10;
    if (euclidean(y, 3) == 0)
        return 20;
    return 30;
}

void boundedIdentifiability() {
    const auto referenceExpression =
        parseDiscreteExpression("if(x0 % 2 == 0, 10, if(x1 % 3 == 0, 20, 30))", 2);
    const auto selected =
        parseDiscreteExpression("if(x1 == x0 % 2, 30, if(x0 % 2 == 0, 10, 20))", 2);
    for (const auto& [x, y, expectedTarget, expectedSelected] :
         std::array<std::array<std::int64_t, 4>, 2>{{{{0, 0, 10, 30}}, {{1, 2, 30, 20}}}}) {
        const auto target = familyBReference(x, y);
        const auto candidate = selected.evaluate(std::array<std::int64_t, 2>{x, y});
        check(target == expectedTarget && candidate.valid() &&
                  candidate.value->integer() == expectedSelected && target != expectedSelected,
              "family B selected expression has the independently reproduced counterexample");
    }

    const std::array box{DiscreteIntegerDomainRange{-4, 4}, DiscreteIntegerDomainRange{-6, 6}};
    const auto comparisonStarted = std::chrono::steady_clock::now();
    const auto disagreement = compareDiscreteExpressionsOnBox(referenceExpression, selected, box);
    const auto comparisonMilliseconds = std::chrono::duration<double, std::milli>(
                                            std::chrono::steady_clock::now() - comparisonStarted)
                                            .count();
    check(disagreement.status == DiscreteExpressionDomainComparisonStatus::DisagreementFound &&
              disagreement.counterexample &&
              disagreement.counterexample->inputs == std::vector<std::int64_t>{-4, 0} &&
              disagreement.counterexample->leftValue == DiscreteValue::integer(10) &&
              disagreement.counterexample->rightValue == DiscreteValue::integer(30),
          "bounded candidate comparison reports the first deterministic family B disagreement");
    check(disagreement.work.domainPoints == 7 && disagreement.work.candidateEvaluations == 14 &&
              disagreement.work.nodeVisits > 0 && disagreement.work.operations > 0 &&
              disagreement.work.peakLogicalBytesEstimate > 0,
          "bounded disagreement comparison accounts point, evaluator, and memory work");
    std::size_t independentlyChecked = 0, independentMismatches = 0, independentInvalid = 0;
    std::size_t independentNodeVisits = 0, independentEvaluatorOperations = 0;
    const auto assessmentStarted = std::chrono::steady_clock::now();
    for (std::int64_t x = -4; x <= 4; ++x) {
        for (std::int64_t y = -6; y <= 6; ++y) {
            ++independentlyChecked;
            const auto actual = selected.evaluate(std::array<std::int64_t, 2>{x, y});
            independentNodeVisits += actual.nodeVisits;
            independentEvaluatorOperations += actual.operations;
            if (!actual.valid()) {
                ++independentInvalid;
            } else if (actual.value->integer() != familyBReference(x, y)) {
                ++independentMismatches;
            }
        }
    }
    check(
        independentlyChecked == 117 && independentMismatches == 33 && independentInvalid == 0,
        "post-selection reference assessment independently finds 33 valid mismatches in 117 rows");
    const auto assessmentMilliseconds = std::chrono::duration<double, std::milli>(
                                            std::chrono::steady_clock::now() - assessmentStarted)
                                            .count();
    std::cout << "family_B_diagnostic points=" << disagreement.work.domainPoints
              << " pair_evaluations=" << disagreement.work.candidateEvaluations
              << " pair_node_visits=" << disagreement.work.nodeVisits
              << " pair_operations=" << disagreement.work.operations
              << " pair_iterations=" << disagreement.work.iterations
              << " pair_iteration_operations=" << disagreement.work.iterationOperations
              << " pair_logical_bytes=" << disagreement.work.peakLogicalBytesEstimate
              << " comparison_ms=" << comparisonMilliseconds
              << " reference_rows=" << independentlyChecked
              << " reference_mismatches=" << independentMismatches
              << " reference_invalid=" << independentInvalid
              << " selected_node_visits=" << independentNodeVisits
              << " selected_operations=" << independentEvaluatorOperations
              << " assessment_ms=" << assessmentMilliseconds << '\n';

    const auto equivalent = compareDiscreteExpressionsOnBox(
        parseDiscreteExpression("x0", 1), parseDiscreteExpression("x0 + 0", 1),
        std::array{DiscreteIntegerDomainRange{-3, 3}});
    check(equivalent.status ==
                  DiscreteExpressionDomainComparisonStatus::EquivalentOnBoundedDomain &&
              equivalent.work.domainPoints == 7 && equivalent.work.candidateEvaluations == 14,
          "bounded diagnostic does not report equivalent arithmetic candidates as different");
    const auto boundaryRemainder = parseDiscreteExpression("x0 % 2", 1);
    const auto int64Boundary = compareDiscreteExpressionsOnBox(
        boundaryRemainder, boundaryRemainder,
        std::array{DiscreteIntegerDomainRange{std::numeric_limits<std::int64_t>::min(),
                                              std::numeric_limits<std::int64_t>::min() + 1}});
    check(int64Boundary.status ==
                  DiscreteExpressionDomainComparisonStatus::EquivalentOnBoundedDomain &&
              int64Boundary.work.domainPoints == 2,
          "lexicographic bounded enumeration safely crosses negative Int64 modulo boundaries");

    const auto evenCandidate = parseDiscreteExpression("if(x0 % 2 == 0, 10, 20)", 2);
    const auto yModuloCandidate = parseDiscreteExpression("if(x1 % 3 == 0, 10, 20)", 2);
    const DiscreteSpecification ambiguous(2, {{{-2, -3}, DiscreteValue::integer(10)},
                                              {{1, 1}, DiscreteValue::integer(20)},
                                              {{-1, 2}, DiscreteValue::integer(20)}});
    check(validateDiscreteSpecification(ambiguous, evenCandidate).status ==
                  DiscreteSpecificationValidationStatus::ValidatedFiniteDomain &&
              validateDiscreteSpecification(ambiguous, yModuloCandidate).status ==
                  DiscreteSpecificationValidationStatus::ValidatedFiniteDomain &&
              compareDiscreteExpressionsOnBox(
                  evenCandidate, yModuloCandidate,
                  std::array{DiscreteIntegerDomainRange{-2, 2}, DiscreteIntegerDomainRange{-3, 3}})
                      .status == DiscreteExpressionDomainComparisonStatus::DisagreementFound,
          "independent multivariate ambiguous table admits two exact but disagreeing candidates");

    const auto partialReference = [](std::int64_t x, std::int64_t y) {
        const auto remainder = y % 3;
        return x < 0 ? 10 : remainder == 0 ? 20 : 30;
    };
    const DiscreteSpecification partial(
        2, {{{-2, -3}, DiscreteValue::integer(partialReference(-2, -3))},
            {{-1, 1}, DiscreteValue::integer(partialReference(-1, 1))},
            {{2, 3}, DiscreteValue::integer(partialReference(2, 3))}});
    const auto partialEven = validateDiscreteSpecification(partial, evenCandidate);
    const auto partialY = validateDiscreteSpecification(partial, yModuloCandidate);
    check(partialEven.status == DiscreteSpecificationValidationStatus::CounterexampleFound &&
              partialY.status == DiscreteSpecificationValidationStatus::CounterexampleFound &&
              partialEven.work.rowsMatched == 1 && partialY.work.rowsMatched == 1,
          "independent multivariate partial table preserves equal one-of-three candidate scores");

    const auto lazy = parseDiscreteExpression("if(x0 == 0, 7, 1 % 0)", 1);
    const auto lazyAgreement = compareDiscreteExpressionsOnBox(
        lazy, parseDiscreteExpression("7", 1), std::array{DiscreteIntegerDomainRange{0, 0}});
    check(lazyAgreement.status ==
              DiscreteExpressionDomainComparisonStatus::EquivalentOnBoundedDomain,
          "unselected invalid branch remains lazy in bounded comparison");
    const auto invalid = compareDiscreteExpressionsOnBox(
        parseDiscreteExpression("1 % 0", 0), parseDiscreteExpression("7", 0),
        std::span<const DiscreteIntegerDomainRange>{});
    check(invalid.status == DiscreteExpressionDomainComparisonStatus::DisagreementFound &&
              invalid.counterexample &&
              invalid.counterexample->leftStatus == DiscreteEvaluationStatus::MathematicalInvalid &&
              invalid.counterexample->rightValue == DiscreteValue::integer(7),
          "mathematical invalidity differs from a valid value and is not resource exhaustion");
    const auto dualInvalid = compareDiscreteExpressionsOnBox(
        parseDiscreteExpression("1 % 0", 0), parseDiscreteExpression("2 % 0", 0),
        std::span<const DiscreteIntegerDomainRange>{});
    check(dualInvalid.status == DiscreteExpressionDomainComparisonStatus::EquivalentOnBoundedDomain,
          "two mathematical-invalid outcomes agree as bounded observable behavior");

    const auto noWitness = DiscreteExpression::iteration(
        DiscreteKind::First, 5, DiscreteExpression::integer(0), DiscreteExpression::integer(1),
        DiscreteExpression::boolean(false));
    const auto sameNoWitness = compareDiscreteExpressionsOnBox(
        noWitness, noWitness, std::span<const DiscreteIntegerDomainRange>{});
    const auto noWitnessVersusValue = compareDiscreteExpressionsOnBox(
        noWitness, parseDiscreteExpression("7", 0), std::span<const DiscreteIntegerDomainRange>{});
    check(sameNoWitness.status ==
                  DiscreteExpressionDomainComparisonStatus::EquivalentOnBoundedDomain &&
              noWitnessVersusValue.status ==
                  DiscreteExpressionDomainComparisonStatus::DisagreementFound &&
              noWitnessVersusValue.counterexample &&
              noWitnessVersusValue.counterexample->leftStatus ==
                  DiscreteEvaluationStatus::NoWitness &&
              sameNoWitness.work.iterations == 4 && sameNoWitness.work.iterationOperations == 2,
          "completed NoWitness outcomes agree with each other but differ from values");

    DiscreteExpressionDomainComparisonLimits pointLimited;
    pointLimited.maxDomainPoints = 1;
    const auto exhausted = compareDiscreteExpressionsOnBox(
        parseDiscreteExpression("x0", 1), parseDiscreteExpression("x0 + 0", 1),
        std::array{DiscreteIntegerDomainRange{-1, 1}}, pointLimited);
    check(exhausted.status ==
                  DiscreteExpressionDomainComparisonStatus::InconclusiveResourceExhausted &&
              exhausted.work.domainPoints == 1,
          "domain point limit yields an explicit inconclusive outcome");
    auto workLimited = DiscreteExpressionDomainComparisonLimits{};
    workLimited.maxOperations = 1;
    const auto workStop = compareDiscreteExpressionsOnBox(
        parseDiscreteExpression("x0 + 1", 1), parseDiscreteExpression("x0 + 2", 1),
        std::array{DiscreteIntegerDomainRange{0, 0}}, workLimited);
    check(workStop.status ==
                  DiscreteExpressionDomainComparisonStatus::InconclusiveResourceExhausted &&
              workStop.work.operations == 1,
          "aggregate operation cap preserves the evaluator's charged partial second evaluation");
    auto exactOperationBudget = DiscreteExpressionDomainComparisonLimits{};
    exactOperationBudget.maxOperations = 1;
    const auto literalAtZeroRemainingOps = compareDiscreteExpressionsOnBox(
        parseDiscreteExpression("x0 + 1", 1), parseDiscreteExpression("1", 1),
        std::array{DiscreteIntegerDomainRange{0, 0}}, exactOperationBudget);
    check(literalAtZeroRemainingOps.status ==
                  DiscreteExpressionDomainComparisonStatus::EquivalentOnBoundedDomain &&
              literalAtZeroRemainingOps.work.operations == 1 &&
              literalAtZeroRemainingOps.work.candidateEvaluations == 2,
          "zero remaining operation allowance still permits a literal evaluation");
    auto badEvaluationLimits = DiscreteExpressionDomainComparisonLimits{};
    badEvaluationLimits.evaluation.maxIterationDepth = 129;
    const auto badConfiguration = compareDiscreteExpressionsOnBox(
        parseDiscreteExpression("x0", 1), parseDiscreteExpression("x0", 1),
        std::array{DiscreteIntegerDomainRange{0, 0}}, badEvaluationLimits);
    check(badConfiguration.status == DiscreteExpressionDomainComparisonStatus::InvalidConfiguration,
          "invalid checked-evaluator configuration is rejected as configuration");
    std::atomic<bool> stop{true};
    const auto cancelled = compareDiscreteExpressionsOnBox(
        parseDiscreteExpression("x0", 1), parseDiscreteExpression("x0", 1),
        std::array{DiscreteIntegerDomainRange{-1, 1}}, {}, &stop);
    check(cancelled.status == DiscreteExpressionDomainComparisonStatus::InconclusiveCancelled &&
              cancelled.work.domainPoints == 0,
          "cancellation remains distinct and precedes domain evaluation");
}

void lazyConditionals() {
    DiscreteSpecification trueBranch(1, {{{-1}, DiscreteValue::integer(0)}});
    const auto trueResult =
        validateDiscreteSpecification(trueBranch, conditionalWithInvalidBranch(false));
    check(trueResult.status == DiscreteSpecificationValidationStatus::ValidatedFiniteDomain,
          "invalid false branch remains unevaluated when condition is true");

    DiscreteSpecification falseBranch(1, {{{0}, DiscreteValue::integer(1)}});
    const auto falseResult =
        validateDiscreteSpecification(falseBranch, conditionalWithInvalidBranch(true));
    check(falseResult.status == DiscreteSpecificationValidationStatus::ValidatedFiniteDomain,
          "invalid true branch remains unevaluated when condition is false");
}

void reductionsAndInvalidity() {
    const auto zero = DiscreteExpression::integer(0);
    const auto one = DiscreteExpression::integer(1);
    const std::array ownIteratorNode{DiscreteNode::iterator(4)};
    const DiscreteExpression ownIterator(ownIteratorNode, 0, {}, std::array<std::size_t, 1>{4});
    const auto emptySum =
        DiscreteExpression::iteration(DiscreteKind::Sum, 4, one, zero, ownIterator);
    const auto emptyResult = validateDiscreteSpecification(
        DiscreteSpecification(0, {{{}, DiscreteValue::integer(0)}}), emptySum);
    check(emptyResult.status == DiscreteSpecificationValidationStatus::ValidatedFiniteDomain,
          "empty inclusive sum validates as zero");

    const auto negativeSum = DiscreteExpression::iteration(
        DiscreteKind::Sum, 4, DiscreteExpression::integer(-2), zero, ownIterator);
    const auto negativeResult = validateDiscreteSpecification(
        DiscreteSpecification(0, {{{}, DiscreteValue::integer(-3)}}), negativeSum);
    check(negativeResult.status == DiscreteSpecificationValidationStatus::ValidatedFiniteDomain &&
              negativeResult.work.iterations == 3,
          "negative inclusive sum validates against independently summed values");

    const std::array outerIteratorNode{DiscreteNode::iterator(4)};
    const DiscreteExpression outerIterator(outerIteratorNode, 0, {}, std::array<std::size_t, 1>{4});
    const auto outerBody = DiscreteExpression::iteration(DiscreteKind::Sum, 8, zero, outerIterator,
                                                         DiscreteExpression::integer(1, 0));
    const auto nested = DiscreteExpression::iteration(DiscreteKind::Sum, 4, zero, one, outerBody);
    const auto nestedResult = validateDiscreteSpecification(
        DiscreteSpecification(0, {{{}, DiscreteValue::integer(3)}}), nested);
    check(nestedResult.status == DiscreteSpecificationValidationStatus::ValidatedFiniteDomain,
          "nested scoped sums validate against independent loop reference");
    int independent = 0;
    for (std::int64_t outer = 0; outer <= 1; ++outer)
        for (std::int64_t inner = 0; inner <= outer; ++inner)
            independent += 1;
    check(independent == 3 && nestedResult.work.iterations == 5,
          "nested reference and iteration accounting agree");
    DiscreteSpecificationValidationLimits iterationLimit;
    iterationLimit.maxIterations = 1;
    const auto iterationStop = validateDiscreteSpecification(
        DiscreteSpecification(0, {{{}, DiscreteValue::integer(3)}}), nested, iterationLimit);
    check(iterationStop.status ==
                  DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted &&
              iterationStop.work.iterations == 1 && !iterationStop.certificate,
          "cumulative iteration exhaustion retains partial work");

    const std::array overflowNodes{DiscreteNode::integer(std::numeric_limits<std::int64_t>::max()),
                                   DiscreteNode::integer(1),
                                   DiscreteNode::operation(DiscreteKind::Add)};
    const DiscreteExpression overflow(overflowNodes, 0);
    const auto overflowResult = validateDiscreteSpecification(
        DiscreteSpecification(0, {{{}, DiscreteValue::integer(0)}}), overflow);
    check(overflowResult.status == DiscreteSpecificationValidationStatus::InvalidCandidate &&
              overflowResult.evaluatorStatus == DiscreteEvaluationStatus::MathematicalInvalid &&
              overflowResult.evaluationFailure && overflowResult.evaluationFailure->ordinal == 0 &&
              overflowResult.evaluationFailure->inputs.empty() &&
              overflowResult.evaluationFailure->specificationIdentity ==
                  DiscreteSpecification(0, {{{}, DiscreteValue::integer(0)}}).identity(),
          "checked Int64 overflow is an invalid candidate");

    const std::array invalidModuloNodes{DiscreteNode::variable(0), DiscreteNode::integer(0),
                                        DiscreteNode::operation(DiscreteKind::Modulo)};
    const auto invalidModulo = DiscreteExpression(invalidModuloNodes, 1);
    const auto invalidResult = validateDiscreteSpecification(
        DiscreteSpecification(1, {{{-1}, DiscreteValue::integer(0)}}), invalidModulo);
    check(invalidResult.status == DiscreteSpecificationValidationStatus::InvalidCandidate,
          "zero modulo divisor is mathematical invalidity");

    const std::array negativeModuloNodes{DiscreteNode::variable(0), DiscreteNode::integer(-2),
                                         DiscreteNode::operation(DiscreteKind::Modulo)};
    const auto negativeModulo = DiscreteExpression(negativeModuloNodes, 1);
    check(validateDiscreteSpecification(
              DiscreteSpecification(1, {{{-1}, DiscreteValue::integer(0)}}), negativeModulo)
                  .status == DiscreteSpecificationValidationStatus::InvalidCandidate,
          "negative modulo divisor is mathematical invalidity");

    const auto firstWithoutWitness = DiscreteExpression::iteration(
        DiscreteKind::First, 5, zero, one, DiscreteExpression::boolean(false));
    const auto noWitness = validateDiscreteSpecification(
        DiscreteSpecification(0, {{{}, DiscreteValue::integer(0)}}), firstWithoutWitness);
    check(noWitness.status == DiscreteSpecificationValidationStatus::InvalidCandidate &&
              noWitness.evaluatorStatus == DiscreteEvaluationStatus::NoWitness,
          "bounded no-witness outcome remains distinct evaluator invalidity");
}

void failuresAndAccounting() {
    const auto candidate = identityPlusTwo();
    const auto spec = DiscreteSpecification(1, {{{-2}, DiscreteValue::integer(0)},
                                                {{-1}, DiscreteValue::integer(1)},
                                                {{0}, DiscreteValue::integer(2)}});
    DiscreteSpecificationValidationLimits limits;
    limits.maxOperations = 2;
    auto stopped = validateDiscreteSpecification(spec, candidate, limits);
    check(stopped.status == DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted,
          "cumulative operation exhaustion is incomplete");
    check(!stopped.certificate && stopped.work.rowsAttempted > 0 &&
              stopped.work.operations <= limits.maxOperations,
          "partial operation work retained without certificate");

    limits = {};
    limits.maxNodeVisits = 2;
    stopped = validateDiscreteSpecification(spec, candidate, limits);
    check(stopped.status == DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted &&
              stopped.work.nodeVisits == 2 && !stopped.certificate,
          "node-visit limit preserves partial evaluator counters");

    limits = {};
    limits.maxRows = 2;
    stopped = validateDiscreteSpecification(spec, candidate, limits);
    check(stopped.status == DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted &&
              stopped.work.rowsAttempted == 0,
          "row admission exhaustion is incomplete before evaluation");

    limits = {};
    limits.maxRetainedBytes = 1;
    stopped = validateDiscreteSpecification(spec, candidate, limits);
    check(stopped.status == DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted &&
              stopped.work.rowsAttempted == 0,
          "logical-memory admission exhaustion is incomplete before evaluation");

    std::atomic<bool> cancelled{true};
    auto cancelledResult = validateDiscreteSpecification(spec, candidate, {}, &cancelled);
    check(cancelledResult.status == DiscreteSpecificationValidationStatus::InconclusiveCancelled &&
              !cancelledResult.certificate,
          "pre-cancelled validation is incomplete");

    std::size_t cancellationPolls = 0;
    const auto midCancelled = validateDiscreteSpecificationWithStop(
        spec, candidate, {}, [&] { return ++cancellationPolls == 4; });
    check(midCancelled.status == DiscreteSpecificationValidationStatus::InconclusiveCancelled &&
              midCancelled.work.rowsAttempted == 1 && midCancelled.work.nodeVisits == 2 &&
              !midCancelled.certificate,
          "mid-evaluation cancellation retains deterministic partial work");

    auto duplicate = DiscreteSpecification(
        1, {{{0}, DiscreteValue::integer(0)}, {{0}, DiscreteValue::integer(0)}});
    check(validateDiscreteSpecification(duplicate, candidate).status ==
              DiscreteSpecificationValidationStatus::InvalidSpecification,
          "duplicate input tuple is malformed");
    limits = {};
    limits.maxRetainedBytes = 1;
    const auto unadmittedDuplicate = validateDiscreteSpecification(duplicate, candidate, limits);
    check(unadmittedDuplicate.status ==
                  DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted &&
              unadmittedDuplicate.work.duplicateTupleComparisons == 0,
          "storage admission precedes duplicate scans");

    const auto coordinateSpec = DiscreteSpecification(
        2, {{{0, 0}, DiscreteValue::integer(0)}, {{0, 1}, DiscreteValue::integer(0)}});
    const auto coordinateCandidate = DiscreteExpression::integer(0, 2);
    limits = {};
    limits.maxSpecificationComparisons = 1;
    const auto comparisonStop =
        validateDiscreteSpecification(coordinateSpec, coordinateCandidate, limits);
    check(comparisonStop.status ==
                  DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted &&
              comparisonStop.work.duplicateTupleComparisons == 1 &&
              comparisonStop.work.specificationComparisons == 1 &&
              comparisonStop.work.rowsAttempted == 0,
          "coordinate-comparison exhaustion stops duplicate preflight explicitly");

    const auto literalCandidate = DiscreteExpression::integer(7, 1);
    const DiscreteSpecification literalSpec(1, {{{-1}, DiscreteValue::integer(7)},
                                                {{0}, DiscreteValue::integer(7)},
                                                {{1}, DiscreteValue::integer(7)}});
    limits = {};
    limits.maxNodeVisits = 3;
    limits.maxOperations = 0;
    limits.maxIterations = 0;
    const auto literalResult = validateDiscreteSpecification(literalSpec, literalCandidate, limits);
    check(literalResult.status == DiscreteSpecificationValidationStatus::ValidatedFiniteDomain &&
              literalResult.work.nodeVisits == 3 && literalResult.work.operations == 0 &&
              literalResult.work.iterations == 0,
          "zero-operation and zero-iteration budgets admit a literal within its node budget");

    limits.maxRetainedBytes = 64 * 1024 * 1024;
    limits.maxPeakRowWorkspaceBytes = 0;
    const auto noWorkspace = validateDiscreteSpecification(spec, candidate, limits);
    check(noWorkspace.status ==
                  DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted &&
              noWorkspace.work.rowsAttempted == 0,
          "zero per-row workspace is an explicit incomplete result");
    limits.maxPeakRowWorkspaceBytes = 1;
    const auto insufficientWorkspace = validateDiscreteSpecification(spec, candidate, limits);
    check(insufficientWorkspace.status ==
                  DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted &&
              insufficientWorkspace.evaluatorStatus ==
                  DiscreteEvaluationStatus::ResourceExhausted &&
              insufficientWorkspace.work.rowsAttempted == 1 &&
              insufficientWorkspace.work.peakRowWorkspaceAllowanceBytes == 1 &&
              !insufficientWorkspace.certificate,
          "sub-frame per-row workspace allowance propagates evaluator exhaustion");
    const std::array wrongTypeNodes{DiscreteNode::boolean(true)};
    const auto wrongTypeCandidate = DiscreteExpression(wrongTypeNodes, 0);
    check(validateDiscreteSpecification(duplicate, wrongTypeCandidate).status ==
              DiscreteSpecificationValidationStatus::InvalidSpecification,
          "malformed specification is reported before candidate incompatibility");
    check(validateDiscreteSpecification(DiscreteSpecification(1, {}), candidate).status ==
              DiscreteSpecificationValidationStatus::InvalidSpecification,
          "empty specification is malformed");
    check(validateDiscreteSpecification(
              DiscreteSpecification(
                  1, {{{0}, DiscreteValue::integer(0)}, {{1}, DiscreteValue::boolean(false)}}),
              candidate)
                  .status == DiscreteSpecificationValidationStatus::InvalidSpecification,
          "inconsistent expected-value type is malformed");
    check(validateDiscreteSpecification(DiscreteSpecification(1, {{{}, DiscreteValue::integer(0)}}),
                                        candidate)
                  .status == DiscreteSpecificationValidationStatus::InvalidSpecification,
          "incorrect observation dimensions are malformed");

    const std::array boolNodes{DiscreteNode::boolean(true)};
    const auto boolCandidate = DiscreteExpression(boolNodes, 0);
    check(validateDiscreteSpecification(DiscreteSpecification(0, {{{}, DiscreteValue::integer(1)}}),
                                        boolCandidate)
                  .status == DiscreteSpecificationValidationStatus::InvalidCandidate,
          "candidate output type must match specification");

    const std::array openNodes{DiscreteNode::iterator(17)};
    const DiscreteExpression open(openNodes, 0, {}, std::array<std::size_t, 1>{17});
    check(validateDiscreteSpecification(DiscreteSpecification(0, {{{}, DiscreteValue::integer(0)}}),
                                        open)
                  .status == DiscreteSpecificationValidationStatus::InvalidCandidate,
          "open expression is not a candidate");
}

void certificateAccounting() {
    const auto candidate = identityPlusTwo();
    const auto spec = DiscreteSpecification(
        1, {{{-2}, DiscreteValue::integer(0)}, {{-1}, DiscreteValue::integer(1)}});
    const auto result = validateDiscreteSpecification(spec, candidate);
    check(result.certificate && result.certificate->specificationIdentity == spec.identity() &&
              result.certificate->candidateIdentity == "dexpr-v1;d=1;scope=0:;nodes=3;2:0;0:2;3:;",
          "certificate records full specification and candidate identities");
    check(result.certificate->declaredRows == 2 && result.certificate->checkedRows == 2,
          "certificate states exact finite scope");
    check(result.work.rowsAttempted == 2 && result.work.rowsMatched == 2 &&
              result.work.nodeVisits == 6 && result.work.operations == 2,
          "certificate reports summed evaluator work");
    check(result.work.retainedBytesEstimate > 0 && result.work.peakRowWorkspaceAllowanceBytes > 0,
          "logical retained storage and row-workspace allowance are explicit");
    check(result.work.duplicateTupleComparisons == 1 && result.work.specificationComparisons == 1,
          "duplicate preflight charges input-coordinate comparisons");
    check(result.certificate->candidateIdentity.size() ==
                  *genetik_specification_detail::candidateIdentityBytes(candidate) &&
              result.certificate->specificationIdentity.size() ==
                  *genetik_specification_detail::logicalIdentityBytes(spec),
          "canonical identity sizing is exact before serialization");

    std::vector<DiscreteSpecificationObservation> callerRows{{{0}, DiscreteValue::integer(0)}};
    const DiscreteSpecification copied(1, callerRows);
    callerRows.front().inputs.front() = 99;
    callerRows.front().expected = DiscreteValue::integer(99);
    check(copied.observations().front().inputs.front() == 0 &&
              copied.observations().front().expected == DiscreteValue::integer(0),
          "specification owns an immutable copy of caller observations");
}

void crossLanguageCanonicalIdentity() {
    const DiscreteSpecification specification(1, {{{0}, DiscreteValue::integer(0)},
                                                  {{1}, DiscreteValue::boolean(true)},
                                                  {{-2}, DiscreteValue::boolean(false)}});
    check(specification.identity() == "discrete-spec-v1;d=1;rows=3;[0,]=i0;[1,]=b1;[-2,]=b0;",
          "canonical identity matches the independently frozen cross-language literal");
}
} // namespace

int main() {
    try {
        exactControls();
        counterexampleAndReplay();
        boundedIdentifiability();
        lazyConditionals();
        reductionsAndInvalidity();
        failuresAndAccounting();
        certificateAccounting();
        crossLanguageCanonicalIdentity();
        std::cout << "discrete specification checks=" << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
