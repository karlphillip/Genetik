#include <DiscreteRunCheckpoint.hpp>
#include <DiscreteExpressionParsing.hpp>
#include <barrier>
#include <iostream>
namespace {
void check(bool x, const char *s) {
    if (!x)
        throw std::runtime_error(s);
}
auto problem() {
    return DiscreteRegressionProblem::create(1, {{{0}, 0}, {{0}, 1}, {{3}, 14}}, {},
                                             DiscreteEvaluationLimits{100000, 100000, 1048576, 64, 4096, 4});
}
DiscreteSearchConfig config() {
    DiscreteSearchConfig c;
    c.population = 8;
    c.elites = 1;
    c.maxGenerations = 12;
    c.maxLossAttempts = 104;
    c.grammar.operators = {DiscreteKind::Add, DiscreteKind::Multiply, DiscreteKind::Sum};
    c.grammar.initialMaxDepth = 3;
    c.grammar.mutationMaxDepth = 3;
    c.grammar.iteratorDiscovery = true;
    c.grammar.recordIterationDiagnostics = true;
    c.grammar.iterationBoundsAreTerminals = true;
    return c;
}
std::string key(const DiscreteEvolutionRun &run) { return run.checkpoint().payload; }
void continuation(unsigned workers) {
    auto p = problem();
    auto c = config();
    std::string whole;
    {
        RandomContext rng(42, ExecutionConfig{workers});
        DiscreteEvolutionRun run(p, c, rng);
        for (int i = 0; i < 12; ++i)
            run.step();
        whole = key(run);
    }
    CheckpointDocument saved;
    {
        RandomContext rng(42, ExecutionConfig{workers});
        DiscreteEvolutionRun run(p, c, rng);
        for (int i = 0; i < 4; ++i)
            run.step();
        saved = decodeCheckpoint(encodeCheckpoint(run.checkpoint()));
    }
    RandomContext rng(42, ExecutionConfig{workers});
    auto restored = DiscreteEvolutionRun::restore(saved, p, c, rng);
    for (int i = 4; i < 12; ++i)
        restored->step();
    check(key(*restored) == whole, "iterator-policy entire-state continuation");
    auto wrong = c;
    wrong.grammar.iteratorDiscovery = false;
    RandomContext fresh(42, ExecutionConfig{workers});
    bool rejected = false;
    try {
        (void)DiscreteEvolutionRun::restore(saved, p, wrong, fresh);
    } catch (const CheckpointError &) {
        rejected = true;
    }
    check(rejected && fresh.evaluationAttempts() == 0, "policy identity rejection before work");
}
void generation() {
    auto p = problem();
    auto c = config();
    DiscreteVariation v(p, c.grammar);
    std::mt19937 r(13);
    std::size_t loops = 0;
    for (int i = 0; i < 200; ++i) {
        auto a = v.initialize(r), b = v.initialize(r);
        auto text = a.text();
        v.validate(a.expression());
        for (auto id : a.expression().binders()) {
            bool referenced = false;
            for (const auto &n : a.expression().nodes())
                if (n.kind() == DiscreteKind::Iterator && n.binder() == id)
                    referenced = true;
            check(referenced, "new binder body must depend on binder");
            ++loops;
        }
        auto m = v.mutation(a, 1, r);
        v.validate(m.expression());
        auto x = a, y = b;
        v.crossover(a, b, x, y, 1, r);
        v.validate(x.expression());
        v.validate(y.expression());
        check(a.text() == text, "parents remain owned and unchanged");
        check(parseDiscreteExpression(x.text(), 1).sameRepresentation(x.expression()),
              "hygienic canonical roundtrip");
    }
    check(loops && v.statistics().bodyRepairs, "opt-in dependent bodies generated");
}
void overlap() {
    std::barrier gate(4);
    ExecutionContext e({4});
    e.run(4, [&](std::size_t) {
        gate.arrive_and_wait();
        generation();
    });
    check(e.peakTasks() == 4, "forced independent generator overlap");
}
} // namespace
int main(int argc, char **) {
    try {
        if (argc > 1) {
            overlap();
            continuation(4);
        } else {
            generation();
            for (unsigned workers : {1, 2, 4})
                continuation(workers);
            overlap();
        }
        std::cout << "D5 iteration-discovery contracts passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
