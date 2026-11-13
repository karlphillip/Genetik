#include <DiscreteExpressionComponents.hpp>
#include <DiscreteExpressionParsing.hpp>
#include <IntegerRecurrence.hpp>

#include <array>
#include <atomic>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

void sameEvaluation(const DiscreteExpression& first, const DiscreteExpression& second,
                    std::int64_t input) {
    const auto left = first.evaluate(std::array{input});
    const auto right = second.evaluate(std::array{input});
    check(left.status == right.status, "component evaluation status differs");
    check(left.value == right.value, "component value differs");
    check(left.nodeVisits == right.nodeVisits, "component node-visit work differs");
    check(left.operations == right.operations, "component arithmetic work differs");
    check(left.iterations == right.iterations, "component iteration work differs");
    check(left.iterationOperations == right.iterationOperations,
          "component iteration-operation work differs");
}

DiscreteExpression nestedSum(std::size_t outer, std::size_t inner) {
    const std::string text = "sum(k" + std::to_string(outer) + ", 0, 2, sum(k" +
                             std::to_string(inner) + ", 0, k" + std::to_string(outer) +
                             ", ((x0 + k" + std::to_string(outer) + ") + k" +
                             std::to_string(inner) + ")))";
    return parseDiscreteExpression(text, 1);
}

void arithmeticAndSharing() {
    DiscreteExpressionComponents components(1);
    const auto x = components.addExpression(DiscreteExpression::variable(0, 1));
    const auto one = components.addExpression(DiscreteExpression::integer(1, 1));
    const auto xPlusOne = components.binary(DiscreteKind::Add, x, one);
    const auto doubled = components.binary(DiscreteKind::Multiply, xPlusOne, xPlusOne);
    const auto materialized = components.materialize(doubled);
    check(materialized.success(), "shared arithmetic component did not materialize");
    check(materialized.expansion.nodes == 7, "expanded arithmetic node count");
    check(materialized.expansion.componentOccurrences == 7, "expanded component occurrence count");
    check(materialized.expansion.expandedEdges == 6 &&
              materialized.work.componentEdgesExpanded == 6,
          "expanded dependency-edge accounting");
    check(materialized.expansion.materializationWorkUpperBound == 132 &&
              materialized.work.totalUnits == 20 &&
              materialized.work.totalWorkUpperBound == 132,
          "materialization work accounting differs from the frozen logical-unit contract");
    check(materialized.work.expressionValidationWorkUpperBound == materialized.expansion.nodes * 16,
          "final expression validation allowance is not reported separately");

    const auto directInner = DiscreteExpression::binary(
        DiscreteKind::Add, DiscreteExpression::variable(0, 1), DiscreteExpression::integer(1, 1));
    const auto direct = DiscreteExpression::binary(DiscreteKind::Multiply, directInner, directInner);
    check(materialized.expression->sameRepresentation(direct), "expanded postfix order changed");
    for (const auto input : {-4, 0, 7})
        sameEvaluation(*materialized.expression, direct, input);

    const auto stats = components.storageStats();
    check(stats.components == 4 && stats.edges == 4, "unique DAG storage accounting");
    check(components.expansionStats(doubled).expandedEdges == 6,
          "expanded edges should count repeated dependency occurrences");
    check(stats.retainedBytesEstimate > 0, "retained storage estimate is empty");
    const auto text = components.toString(doubled);
    check(text == components.toString(doubled), "component serialization is nondeterministic");
    bool textLimit = false;
    try {
        (void)components.toString(doubled, 4);
    } catch (const std::length_error&) {
        textLimit = true;
    }
    check(textLimit, "component structural text ignored its byte limit");

    DiscreteExpressionComponents other(1);
    const auto foreignLive = other.addExpression(DiscreteExpression::integer(9, 1));
    bool liveForeignRejected = false;
    try {
        (void)components.type(foreignLive);
    } catch (const std::invalid_argument&) {
        liveForeignRejected = true;
    }
    check(liveForeignRejected, "live foreign component handle was accepted");

    DiscreteExpressionComponents readers(1);
    const auto readX = readers.addExpression(DiscreteExpression::variable(0, 1));
    const auto readTwo = readers.addExpression(DiscreteExpression::integer(2, 1));
    const auto readRoot = readers.binary(DiscreteKind::Add, readX, readTwo);
    std::array<std::string, 2> concurrentExports;
    std::array<std::int64_t, 2> concurrentResults{};
    std::jthread first([&] {
        auto value = readers.materialize(readRoot);
        concurrentExports[0] = value.expression->toString();
        concurrentResults[0] = value.expression->evaluate(std::array<std::int64_t, 1>{3}).value->integer();
    });
    std::jthread second([&] {
        auto value = readers.materialize(readRoot);
        concurrentExports[1] = value.expression->toString();
        concurrentResults[1] = value.expression->evaluate(std::array<std::int64_t, 1>{3}).value->integer();
    });
    first.join();
    second.join();
    check(concurrentExports[0] == concurrentExports[1] && concurrentResults[0] == 5 &&
              concurrentResults[1] == 5,
          "concurrent read-only materialization was nondeterministic");
}

void modularAndConditional() {
    DiscreteExpressionComponents components(1);
    const auto x = components.addExpression(DiscreteExpression::variable(0, 1));
    const auto five = components.addExpression(DiscreteExpression::integer(5, 1));
    const auto two = components.addExpression(DiscreteExpression::integer(2, 1));
    const auto mod = components.binary(DiscreteKind::Modulo, x, five);
    const auto modulusThree = components.addExpression(DiscreteExpression::integer(3, 1));
    const auto nestedMod = components.binary(DiscreteKind::Modulo, mod, modulusThree);
    const auto mixed = components.binary(DiscreteKind::Add, mod, two);
    const auto negative = components.addExpression(parseDiscreteExpression("x0 < 0", 1));
    const auto zeroMinusX = components.binary(DiscreteKind::Subtract, two, x);
    const auto root = components.conditional(negative, zeroMinusX, mixed);
    const auto result = components.materialize(root);
    check(result.success(), "conditional component did not materialize");
    const auto nestedModResult = components.materialize(nestedMod);
    check(nestedModResult.success(), "nested modulo component did not materialize");
    check(nestedModResult.expression->evaluate(std::array<std::int64_t, 1>{-7}).value->integer() == 0,
          "nested modulo did not use Euclidean remainder for a negative operand");
    check(nestedModResult.expression->evaluate(std::array<std::int64_t, 1>{-8}).value->integer() == 2,
          "nested modulo independent negative result");

    const auto modExpression = DiscreteExpression::binary(
        DiscreteKind::Modulo, DiscreteExpression::variable(0, 1), DiscreteExpression::integer(5, 1));
    const auto mixedExpression = DiscreteExpression::binary(
        DiscreteKind::Add, modExpression, DiscreteExpression::integer(2, 1));
    const auto invalidModulo = DiscreteExpression::binary(
        DiscreteKind::Modulo, DiscreteExpression::variable(0, 1), DiscreteExpression::integer(0, 1));
    const auto direct = DiscreteExpression::conditional(
        DiscreteExpression::binary(DiscreteKind::LessThan, DiscreteExpression::variable(0, 1),
                                   DiscreteExpression::integer(0, 1)),
        DiscreteExpression::binary(DiscreteKind::Subtract, DiscreteExpression::integer(2, 1),
                                   DiscreteExpression::variable(0, 1)),
        mixedExpression);
    check(result.expression->sameRepresentation(direct), "conditional component order differs");
    for (const auto input : {-7, -1, 0, 8})
        sameEvaluation(*result.expression, direct, input);

    const auto boolWhenTrue = components.addExpression(parseDiscreteExpression("x0 < 0", 1));
    const auto boolWhenFalse = components.addExpression(parseDiscreteExpression("x0 == 0", 1));
    const auto booleanRoot = components.conditional(negative, boolWhenTrue, boolWhenFalse);
    check(components.type(booleanRoot) == DiscreteType::Bool,
          "Bool-valued conditional component lost its output type");
    const auto booleanModel = components.materialize(booleanRoot);
    check(booleanModel.success(), "Bool-valued conditional did not materialize");
    const auto boolAtNegative = booleanModel.expression->evaluate(std::array<std::int64_t, 1>{-3});
    const auto boolAtNearNegative = booleanModel.expression->evaluate(std::array<std::int64_t, 1>{-1});
    const auto boolAtZero = booleanModel.expression->evaluate(std::array<std::int64_t, 1>{0});
    const auto boolAtPositive = booleanModel.expression->evaluate(std::array<std::int64_t, 1>{1});
    check(boolAtNegative.valid() && boolAtNegative.value->type() == DiscreteType::Bool &&
              boolAtNegative.value->boolean(),
          "Bool conditional true branch mismatch");
    check(boolAtNearNegative.valid() && boolAtNearNegative.value->boolean(),
          "Bool conditional true-branch result mismatch");
    check(boolAtZero.valid() && boolAtZero.value->type() == DiscreteType::Bool &&
              boolAtZero.value->boolean() && boolAtPositive.valid() &&
              !boolAtPositive.value->boolean(),
          "Bool conditional false branch mismatch");

    // The malformed arithmetic is in the untaken branch and must remain lazy.
    DiscreteExpressionComponents lazyComponents(1);
    const auto nonnegative = lazyComponents.addExpression(parseDiscreteExpression("x0 >= 0", 1));
    const auto safe = lazyComponents.addExpression(DiscreteExpression::variable(0, 1));
    const auto bad = lazyComponents.addExpression(invalidModulo);
    const auto lazy = lazyComponents.conditional(nonnegative, safe, bad);
    const auto lazyModel = lazyComponents.materialize(lazy);
    check(lazyModel.success(), "lazy conditional component did not materialize");
    const auto atZero = lazyModel.expression->evaluate(std::array<std::int64_t, 1>{0});
    check(atZero.valid() && atZero.value->integer() == 0, "untaken invalid component was evaluated");
    const auto atNegative = lazyModel.expression->evaluate(std::array<std::int64_t, 1>{-1});
    check(atNegative.status == DiscreteEvaluationStatus::MathematicalInvalid,
          "taken invalid component did not preserve mathematical invalidity");
}

void reductionsAndBinderHygiene() {
    const auto sum = nestedSum(10, 20);
    DiscreteExpressionComponents components(1);
    const auto shared = components.addExpression(sum);
    const auto twice = components.binary(DiscreteKind::Add, shared, shared);
    const auto result = components.materialize(twice);
    check(result.success(), "closed nested reduction did not materialize");
    check(result.expansion.binders == 4, "reused nested reductions did not count binders per occurrence");
    check(result.expression->binders().size() == 4, "reused nested reductions did not get fresh binders");
    for (std::size_t i = 0; i < result.expression->binders().size(); ++i)
        for (std::size_t j = i + 1; j < result.expression->binders().size(); ++j)
            check(result.expression->binders()[i] != result.expression->binders()[j],
                  "reused reduction binder identity collided");

    const auto directLeft = nestedSum(1, 0);
    const auto directRight = nestedSum(3, 2);
    const auto direct = DiscreteExpression::binary(DiscreteKind::Add, directLeft, directRight);
    check(result.expression->sameRepresentation(direct), "alpha-renamed reduction expansion differs");
    check(parseDiscreteExpression(result.expression->toString(), 1).sameRepresentation(*result.expression),
          "binder-renamed model does not round-trip through the ordinary parser");
    for (const auto input : {-2, 0, 3})
        sameEvaluation(*result.expression, direct, input);
    for (const auto& [input, expected] : std::array<std::pair<std::int64_t, std::int64_t>, 3>{
             {{-2, 0}, {0, 24}, {3, 60}}}) {
        const auto value = result.expression->evaluate(std::array{input});
        check(value.valid() && value.value->integer() == expected,
              "nested sum disagrees with independent closed-form value");
    }

    const auto cancellable = parseDiscreteExpression("sum(k0, 0, 100, (x0 + k0))", 1);
    DiscreteExpressionComponents cancellationComponents(1);
    const auto loop = cancellationComponents.addExpression(cancellable);
    const auto loopModel = cancellationComponents.materialize(loop);
    int polls = 0;
    const auto stopped = loopModel.expression->evaluateWithCancellation(
        std::array<std::int64_t, 1>{4}, {}, [&] { return ++polls == 30; });
    check(stopped.status == DiscreteEvaluationStatus::Cancelled && stopped.iterations > 0 &&
              !stopped.value,
          "materialized reduction did not preserve cancellation");
    int directPolls = 0;
    const auto directStopped = cancellable.evaluateWithCancellation(
        std::array<std::int64_t, 1>{4}, {}, [&] { return ++directPolls == 30; });
    check(stopped.status == directStopped.status && stopped.iterations == directStopped.iterations &&
              stopped.nodeVisits == directStopped.nodeVisits && stopped.operations == directStopped.operations,
          "materialized cancellation differs from direct expression");
    DiscreteEvaluationLimits constrained;
    constrained.maxNodeVisits = 4;
    const auto limited = loopModel.expression->evaluate(std::array<std::int64_t, 1>{4}, constrained);
    const auto directLimited = cancellable.evaluate(std::array<std::int64_t, 1>{4}, constrained);
    check(limited.status == DiscreteEvaluationStatus::ResourceExhausted &&
              limited.status == directLimited.status && limited.nodeVisits == directLimited.nodeVisits &&
              limited.operations == directLimited.operations,
          "materialized evaluation resource exhaustion differs from direct expression");
}

void invalidityAndLimits() {
    DiscreteExpressionComponents components(1);
    const auto x = components.addExpression(DiscreteExpression::variable(0, 1));
    const auto maximum = components.addExpression(DiscreteExpression::integer(1, 1));
    const auto add = components.binary(DiscreteKind::Add, x, maximum);
    const auto overflow = components.materialize(add);
    check(overflow.success(), "overflow candidate did not materialize");
    check(overflow.expression->evaluate(std::array{std::numeric_limits<std::int64_t>::max()}).status ==
              DiscreteEvaluationStatus::MathematicalInvalid,
          "checked overflow semantics changed");

    DiscreteExpressionComponents recurrenceComponents(3);
    const auto older = recurrenceComponents.addExpression(DiscreteExpression::variable(1, 3));
    const auto newer = recurrenceComponents.addExpression(DiscreteExpression::variable(2, 3));
    const auto transition = recurrenceComponents.binary(DiscreteKind::Add, older, newer);
    const auto transitionModel = recurrenceComponents.materialize(transition);
    check(transitionModel.success(), "recurrence transition components did not materialize");
    const IntegerRecurrence fibonacci(0, {1, 1}, *transitionModel.expression);
    check(fibonacci.evaluate(7).valid() && fibonacci.evaluate(7).value->integer() == 21,
          "materialized component transition changed recurrence state binding");

    // A variable-only model remains valid under a policy that permits no constants.
    DiscreteExpressionComponents variableOnly(1);
    const auto variable = variableOnly.addExpression(DiscreteExpression::variable(0, 1));
    DiscreteExpressionLimits noConstants;
    noConstants.maxConstants = 0;
    const auto variableModel = variableOnly.materialize(variable, noConstants);
    check(variableModel.success() && variableModel.expression->constantCount() == 0,
          "zero-constant expression policy was rejected by materialization");

    DiscreteExpressionLimits insufficientStorage;
    insufficientStorage.maxStorageBytes = 1;
    const auto storageResult = variableOnly.materialize(variable, insufficientStorage);
    check(storageResult.status == DiscreteComponentMaterializationStatus::ResourceExhausted &&
              !storageResult.expression,
          "insufficient expression storage did not return explicit exhaustion");

    DiscreteExpressionComponents bounded(1);
    const auto bx = bounded.addExpression(DiscreteExpression::variable(0, 1));
    const auto one = bounded.addExpression(DiscreteExpression::integer(1, 1));
    const auto root = bounded.binary(DiscreteKind::Add, bx, one);
    DiscreteExpressionLimits lowExpressionLimits;
    lowExpressionLimits.maxNodes = 2;
    const auto exhausted = bounded.materialize(root, lowExpressionLimits);
    check(exhausted.status == DiscreteComponentMaterializationStatus::ResourceExhausted &&
              !exhausted.expression,
          "node exhaustion returned a model");

    DiscreteComponentLimits tinyStorage;
    tinyStorage.maxRetainedBytes = sizeof(std::size_t);
    DiscreteExpressionComponents storageLimited(1, tinyStorage);
    bool storageRejected = false;
    try {
        (void)storageLimited.addExpression(DiscreteExpression::integer(1, 1));
    } catch (const std::length_error&) {
        storageRejected = true;
    }
    check(storageRejected && storageLimited.storageStats().components == 0,
          "retained-storage failure was not transactional");

    DiscreteComponentLimits tinyMaterialization;
    tinyMaterialization.maxMaterializationBytes = 64;
    DiscreteExpressionComponents memoryLimited(1, tinyMaterialization);
    const auto mx = memoryLimited.addExpression(DiscreteExpression::variable(0, 1));
    const auto mone = memoryLimited.addExpression(DiscreteExpression::integer(1, 1));
    const auto mroot = memoryLimited.binary(DiscreteKind::Add, mx, mone);
    const auto noMemory = memoryLimited.materialize(mroot);
    check(noMemory.status == DiscreteComponentMaterializationStatus::ResourceExhausted &&
              !noMemory.expression,
          "materialization memory estimate was not enforced");

    const auto exactExpressionBytes = DiscreteExpression::storageBytesPerNode();
    DiscreteComponentLimits exactMaterializationLimits;
    exactMaterializationLimits.maxMaterializationBytes = exactExpressionBytes + sizeof(DiscreteNode) +
                                                        3 * sizeof(std::size_t);
    DiscreteExpressionComponents exactMaterialization(1, exactMaterializationLimits);
    const auto exactVariable = exactMaterialization.addExpression(DiscreteExpression::variable(0, 1));
    DiscreteExpressionLimits exactExpressionPolicy;
    exactExpressionPolicy.maxStorageBytes = exactExpressionBytes;
    check(exactMaterialization.materialize(exactVariable, exactExpressionPolicy).success(),
          "exact materialization storage boundary was rejected");
    --exactMaterializationLimits.maxMaterializationBytes;
    DiscreteExpressionComponents belowMaterialization(1, exactMaterializationLimits);
    const auto belowVariable = belowMaterialization.addExpression(DiscreteExpression::variable(0, 1));
    check(belowMaterialization.materialize(belowVariable, exactExpressionPolicy).status ==
              DiscreteComponentMaterializationStatus::ResourceExhausted,
          "one-byte-short materialization storage boundary was accepted");

    DiscreteExpressionComponents retainedProbe(1);
    const auto retainedProbeHandle = retainedProbe.addExpression(DiscreteExpression::variable(0, 1));
    (void)retainedProbeHandle;
    const auto exactRetainedBytes = retainedProbe.storageStats().retainedBytesEstimate;
    DiscreteComponentLimits exactRetained;
    exactRetained.maxRetainedBytes = exactRetainedBytes;
    DiscreteExpressionComponents exactRetainedGraph(1, exactRetained);
    (void)exactRetainedGraph.addExpression(DiscreteExpression::variable(0, 1));
    exactRetained.maxRetainedBytes = exactRetainedBytes - 1;
    DiscreteExpressionComponents belowRetainedGraph(1, exactRetained);
    bool oneByteShortRejected = false;
    try {
        (void)belowRetainedGraph.addExpression(DiscreteExpression::variable(0, 1));
    } catch (const std::length_error&) {
        oneByteShortRejected = true;
    }
    check(oneByteShortRejected, "one-byte-short retained-storage boundary was accepted");

    DiscreteComponentLimits tinyWork;
    tinyWork.maxMaterializationWork = 55;
    DiscreteExpressionComponents workLimited(1, tinyWork);
    const auto wx = workLimited.addExpression(DiscreteExpression::variable(0, 1));
    const auto wone = workLimited.addExpression(DiscreteExpression::integer(1, 1));
    bool workRejected = false;
    try {
        (void)workLimited.binary(DiscreteKind::Add, wx, wone);
    } catch (const std::length_error&) {
        workRejected = true;
    }
    check(workRejected && workLimited.storageStats().components == 2,
          "materialization-work admission was not transactional");

    DiscreteComponentLimits exactWork;
    exactWork.maxMaterializationWork = 56;
    DiscreteExpressionComponents exactWorkGraph(1, exactWork);
    const auto exactWorkX = exactWorkGraph.addExpression(DiscreteExpression::variable(0, 1));
    const auto exactWorkOne = exactWorkGraph.addExpression(DiscreteExpression::integer(1, 1));
    const auto exactWorkRoot = exactWorkGraph.binary(DiscreteKind::Add, exactWorkX, exactWorkOne);
    const auto exactWorkResult = exactWorkGraph.materialize(exactWorkRoot);
    check(exactWorkResult.success() && exactWorkResult.work.totalUnits == 8 &&
              exactWorkResult.work.componentEdgesExpanded == 2 &&
              exactWorkResult.work.expressionValidationWorkUpperBound == 48 &&
              exactWorkResult.expansion.materializationWorkUpperBound == 56,
          "exact materialization work boundary or counters differ");

    DiscreteComponentLimits exactConstruction;
    exactConstruction.maxConstructionWork = 12;
    DiscreteExpressionComponents exactlyConstructed(1, exactConstruction);
    const auto constructionX = exactlyConstructed.addExpression(DiscreteExpression::variable(0, 1));
    const auto constructionOne = exactlyConstructed.addExpression(DiscreteExpression::integer(1, 1));
    (void)exactlyConstructed.binary(DiscreteKind::Add, constructionX, constructionOne);
    check(exactlyConstructed.storageStats().constructionWork == 12,
          "construction work counter does not match its logical-unit contract");
    DiscreteComponentLimits constructionShort;
    constructionShort.maxConstructionWork = 11;
    DiscreteExpressionComponents constructionRejected(1, constructionShort);
    bool constructionWorkRejected = false;
    try {
        const auto xShort = constructionRejected.addExpression(DiscreteExpression::variable(0, 1));
        const auto oneShort = constructionRejected.addExpression(DiscreteExpression::integer(1, 1));
        (void)constructionRejected.binary(DiscreteKind::Add, xShort, oneShort);
    } catch (const std::length_error&) {
        constructionWorkRejected = true;
    }
    check(constructionWorkRejected, "construction work budget was not enforced");

    DiscreteComponentLimits exponentialLimit;
    exponentialLimit.maxExpandedNodes = 31;
    DiscreteExpressionComponents exponential(1, exponentialLimit);
    auto repeated = exponential.addExpression(DiscreteExpression::variable(0, 1));
    for (int level = 0; level < 4; ++level)
        repeated = exponential.binary(DiscreteKind::Add, repeated, repeated);
    bool exponentialRejected = false;
    try {
        (void)exponential.binary(DiscreteKind::Add, repeated, repeated);
    } catch (const std::length_error&) {
        exponentialRejected = true;
    }
    check(exponentialRejected && exponential.storageStats().components == 5,
          "exponential expansion bypassed the expanded-node admission cap");
}

void handlesAndTyping() {
    DiscreteExpressionComponents components(1);
    const auto boolean = components.addExpression(parseDiscreteExpression("x0 < 1", 1));
    const auto integer = components.addExpression(DiscreteExpression::integer(4, 1));
    bool typeRejected = false;
    try {
        (void)components.binary(DiscreteKind::Add, boolean, integer);
    } catch (const std::invalid_argument&) {
        typeRejected = true;
    }
    check(typeRejected && components.storageStats().components == 2,
          "ill-typed composition was published");
    bool orderRejected = false;
    try {
        (void)components.binary(DiscreteKind::Sum, integer, integer);
    } catch (const std::invalid_argument&) {
        orderRejected = true;
    }
    check(orderRejected, "bound iteration was incorrectly treated as an ordinary operator");

    DiscreteExpressionComponents liveOther(1);
    const auto foreign = liveOther.addExpression(DiscreteExpression::integer(0, 1));
    bool foreignRejected = false;
    try {
        (void)components.type(foreign);
    } catch (const std::invalid_argument&) {
        foreignRejected = true;
    }
    check(foreignRejected, "live foreign handle was accepted");

    std::optional<DiscreteComponentHandle> stale;
    {
        DiscreteExpressionComponents other(1);
        stale.emplace(other.addExpression(DiscreteExpression::integer(0, 1)));
    }
    bool staleRejected = false;
    try {
        (void)components.type(*stale);
    } catch (const std::invalid_argument&) {
        staleRejected = true;
    }
    check(staleRejected, "expired component handle was accepted");

    const std::array freeIteratorNodes{DiscreteNode::iterator(77)};
    bool freeIteratorRejected = false;
    try {
        (void)DiscreteExpression(freeIteratorNodes, 1);
    } catch (const std::invalid_argument&) {
        freeIteratorRejected = true;
    }
    check(freeIteratorRejected, "free iterator was accepted by the base expression");
    const std::array scopedIterator{DiscreteNode::iterator(77)};
    const std::array outerBinding{std::size_t{77}};
    const DiscreteExpression scoped(scopedIterator, 1, {}, outerBinding);
    bool openComponentRejected = false;
    try {
        (void)components.addExpression(scoped);
    } catch (const std::invalid_argument&) {
        openComponentRejected = true;
    }
    check(openComponentRejected, "component graph accepted a free outer iterator");
}
} // namespace

int main() {
    arithmeticAndSharing();
    modularAndConditional();
    reductionsAndBinderHygiene();
    invalidityAndLimits();
    handlesAndTyping();
    return checks == 0 ? 1 : 0;
}
