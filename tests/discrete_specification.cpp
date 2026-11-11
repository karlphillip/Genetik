#include <DiscreteExpressionSpecification.hpp>

#include <array>
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
} // namespace

int main() {
    try {
        exactControls();
        counterexampleAndReplay();
        lazyConditionals();
        reductionsAndInvalidity();
        failuresAndAccounting();
        certificateAccounting();
        std::cout << "discrete specification checks=" << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
