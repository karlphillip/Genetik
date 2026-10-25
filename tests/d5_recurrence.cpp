#include <IntegerRecurrence.hpp>
#include <DiscreteExpressionParsing.hpp>
#include <iostream>
#include <barrier>
namespace {
void check(bool v, const char *what) {
    if (!v)
        throw std::runtime_error(what);
}
auto parse(std::string_view s, std::size_t n) { return parseDiscreteExpression(s, n); }
void mathematical() {
    IntegerRecurrence fibonacci(0, {0, 1}, parse("x1+x2", 3));
    std::int64_t a = 0, b = 1;
    for (std::int64_t n = 0; n <= 92; ++n) {
        auto result = fibonacci.evaluate(n);
        check(result.valid() && result.value->integer() == a, "independent iterative Fibonacci reference");
        if (n < 91) {
            auto next = a + b;
            a = b;
            b = next;
        } else if (n == 91)
            a = b;
    }
    check(fibonacci.evaluate(93).status == DiscreteEvaluationStatus::MathematicalInvalid,
          "F93 checked overflow");
    IntegerRecurrence factorial(0, {1}, parse("(x0+1)*x1", 2));
    check(factorial.evaluate(0).value->integer() == 1 && factorial.evaluate(5).value->integer() == 120,
          "first-order factorial reference");
    check(factorial.evaluate(21).status == DiscreteEvaluationStatus::MathematicalInvalid,
          "recurrence arithmetic overflow");
    IntegerRecurrence invalid(0, {3}, parse("1%0", 2));
    check(invalid.evaluate(0).value->integer() == 3 &&
              invalid.evaluate(1).status == DiscreteEvaluationStatus::MathematicalInvalid,
          "initial condition does not evaluate transition");
    auto limits = DiscreteEvaluationLimits{};
    auto v = fibonacci.evaluate(10);
    limits.maxOperations = v.operations;
    check(fibonacci.evaluate(10, {}, limits).valid(), "exact recurrence work boundary");
    --limits.maxOperations;
    check(fibonacci.evaluate(10, {}, limits).status == DiscreteEvaluationStatus::ResourceExhausted,
          "recurrence mid-step resource outcome");
    int polls = 0;
    check(fibonacci.evaluateWithCancellation(20, {}, DiscreteEvaluationLimits{}, [&] { return ++polls == 5; })
                  .status == DiscreteEvaluationStatus::Cancelled,
          "recurrence cooperative cancellation");
    IntegerRecurrence bounded(0, {0, 1}, parse("x1+x2", 3), {2, 1048576});
    check(bounded.evaluate(10).status == DiscreteEvaluationStatus::ResourceExhausted, "step admission");
    auto restored = IntegerRecurrence::restore(decodeCheckpoint(encodeCheckpoint(fibonacci.checkpoint())));
    check(restored.toString() == fibonacci.toString() && restored.evaluate(20).value->integer() == 6765,
          "owned model node-codec roundtrip");
}
void continuation(unsigned workers) {
    std::vector<IntegerRecurrenceObservation> rows{{0, {}, 0}, {1, {}, 1}, {2, {}, 1},
                                                   {3, {}, 2}, {4, {}, 3}, {4, {}, 4}};
    auto p = recurrenceRegressionProblem(0, {0, 1}, rows);
    DiscreteSearchConfig c;
    c.population = 8;
    c.elites = 1;
    c.maxGenerations = 8;
    c.maxLossAttempts = 72;
    c.grammar.operators = {DiscreteKind::Add, DiscreteKind::Subtract};
    c.grammar.initialMaxDepth = 2;
    c.grammar.mutationMaxDepth = 2;
    std::string expected;
    {
        RandomContext rng(17, ExecutionConfig{workers});
        DiscreteEvolutionRun run(p, c, rng);
        for (int i = 0; i < 8; ++i)
            run.step();
        expected = run.checkpoint().payload;
    }
    CheckpointDocument d;
    {
        RandomContext rng(17, ExecutionConfig{workers});
        DiscreteEvolutionRun run(p, c, rng);
        for (int i = 0; i < 3; ++i)
            run.step();
        d = decodeCheckpoint(encodeCheckpoint(run.checkpoint()));
    }
    RandomContext rng(17, ExecutionConfig{workers});
    auto resumed = DiscreteEvolutionRun::restore(d, p, c, rng);
    for (int i = 3; i < 8; ++i)
        resumed->step();
    check(resumed->checkpoint().payload == expected, "recurrence full transition evolution continuation");
    auto other = recurrenceRegressionProblem(0, {1, 1}, rows);
    RandomContext fresh(17, ExecutionConfig{workers});
    bool rejected = false;
    try {
        (void)DiscreteEvolutionRun::restore(d, other, c, fresh);
    } catch (const CheckpointError &) {
        rejected = true;
    }
    check(rejected && fresh.evaluationAttempts() == 0, "initial definitions participate in identity");
}
void overlap() {
    IntegerRecurrence model(0, {0, 1}, parse("x1+x2", 3));
    std::barrier gate(4);
    ExecutionContext e({4});
    e.run(4, [&](std::size_t i) {
        gate.arrive_and_wait();
        for (int n = 0; n < 20; ++n) {
            auto r = model.evaluate(static_cast<std::int64_t>(i + 10));
            check(r.valid(), "private shared-model workspace");
        }
        continuation(1);
    });
    check(e.peakTasks() == 4, "forced recurrence overlap");
}
} // namespace
int main(int argc, char **) {
    try {
        if (argc > 1) {
            overlap();
        } else {
            mathematical();
            for (unsigned n : {1, 2, 4})
                continuation(n);
            overlap();
        }
        std::cout << "Recurrence contracts passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
