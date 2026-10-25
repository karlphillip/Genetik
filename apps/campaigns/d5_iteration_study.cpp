#include <DiscreteRunCheckpoint.hpp>
#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionFormatting.hpp>
#include <chrono>
#include <iostream>
#include <fstream>

namespace {
std::int64_t reference(bool divisors, std::int64_t n) {
    std::int64_t s = 0;
    for (std::int64_t k = 1; k <= n; ++k)
        s += divisors ? (n % k == 0 ? 1 : 0) : k * k;
    return s;
}
struct Counts {
    std::size_t loops = 0, refs = 0, valid = 0, invalid = 0;
};
Counts count(const DiscreteEvolutionRun &run) {
    Counts c;
    const auto &members = run.population().selectionView();
    for (const auto &x : members) {
        c.loops += !x.expression().binders().empty();
        c.refs += std::any_of(x.expression().nodes().begin(), x.expression().nodes().end(),
                              [](auto &n) { return n.kind() == DiscreteKind::Iterator; });
        c.valid += x.fitnessState() == FitnessState::Valid;
        c.invalid += x.fitnessState() == FitnessState::Invalid;
    }
    return c;
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 5)
            throw std::invalid_argument(
                "usage: d5_iteration_study squares|divisors baseline|dependent seed output-directory");
        const bool divisors = std::string_view(argv[1]) == "divisors",
                   dependent = std::string_view(argv[2]) == "dependent";
        const auto seed = static_cast<unsigned>(std::stoul(argv[3]));
        std::filesystem::path output = argv[4];
        std::filesystem::create_directories(output);
        DiscreteExpressionLimits l;
        l.maxNodes = 25;
        l.maxDepth = 6;
        l.maxIterationDepth = 2;
        l.maxBinders = 8;
        DiscreteEvaluationLimits e{100000, 100000, 1048576, 64, 4096, 2};
        DiscreteDatasetLimits d;
        d.maxNodeVisitsPerLoss = d.maxOperationsPerLoss = 1000000;
        d.maxIterationsPerLoss = 65536;
        std::vector<DiscreteRegressionRow> rows;
        for (std::int64_t n = divisors ? 1 : 0; n <= (divisors ? 16 : 8); ++n)
            rows.push_back({{n}, reference(divisors, n)});
        auto p = DiscreteRegressionProblem::create(1, rows, l, e, d);
        DiscreteSearchConfig c;
        c.population = 32;
        c.elites = 2;
        c.maxGenerations = 40;
        c.maxLossAttempts = 1312;
        c.grammar.operators = {DiscreteKind::Add,    DiscreteKind::Subtract, DiscreteKind::Multiply,
                               DiscreteKind::Modulo, DiscreteKind::Equal,    DiscreteKind::IfThenElse,
                               DiscreteKind::Sum};
        c.grammar.constants = {0, 1, 2};
        c.grammar.initialMaxDepth = 5;
        c.grammar.mutationMaxDepth = 4;
        c.grammar.maxIterationNesting = 2;
        c.grammar.iterationBoundsAreTerminals = true;
        c.grammar.positiveConstantDivisorsOnly = false;
        c.grammar.iteratorDiscovery = dependent;
        c.grammar.recordIterationDiagnostics = true;
        c.contextIdentity = "D5-iterator-study/1";
        const auto target =
            parseDiscreteExpression(divisors ? "sum(k0,1,x0,if(x0%k0==0,1,0))" : "sum(k0,1,x0,k0*k0)", 1, l);
        if (p->measure(target).loss != Fitness::valid(0))
            throw std::runtime_error("independent direct reference failed");
        RandomContext rng(seed, ExecutionConfig{1});
        const auto start = std::chrono::steady_clock::now();
        DiscreteEvolutionRun run(p, c, rng);
        std::ofstream metrics(output / "progress.jsonl");
        std::size_t dependentImprovements = 0, publishedWithRefs = 0;
        double prior = std::numeric_limits<double>::infinity();
        const auto snapshot = [&] {
            auto q = count(run);
            const auto best = run.population().bestSeen();
            if (q.refs)
                ++publishedWithRefs;
            if (best && best->fitness() < prior) {
                if (std::any_of(best->expression().nodes().begin(), best->expression().nodes().end(),
                                [](auto &n) { return n.kind() == DiscreteKind::Iterator; }))
                    ++dependentImprovements;
                prior = best->fitness();
            }
            metrics << "{\"generation\":" << *run.population().generation()
                    << ",\"attempts\":" << run.work().lossAttempts << ",\"loops\":" << q.loops
                    << ",\"refs\":" << q.refs << ",\"valid\":" << q.valid << ",\"invalid\":" << q.invalid
                    << ",\"best_loss\":" << (best ? best->fitness() : -1) << "}\n";
        };
        snapshot();
        std::string stopping = "boundary";
        try {
            while (!run.stoppingReason()) {
                run.step();
                snapshot();
            }
        } catch (const DiscreteResourceExhausted &) {
            stopping = "resource";
        }
        if (stopping == "boundary") {
            const auto stop = run.stoppingReason();
            stopping = stop == DiscreteSearchStop::ExactFit          ? "exact-fit"
                       : stop == DiscreteSearchStop::LossBudget      ? "loss-budget"
                       : stop == DiscreteSearchStop::GenerationLimit ? "generation-limit"
                       : stop == DiscreteSearchStop::Cancelled       ? "cancelled"
                                                                     : "no-viable";
        }
        run.save(output / "final.gtkc");
        const auto best = run.population().bestSeen();
        std::size_t assessed = 0, matches = 0, invalid = 0, resource = 0, diagnosticWork = 0;
        if (best)
            for (std::int64_t n = divisors ? 17 : 9; n <= (divisors ? 32 : 12); ++n) {
                auto result = best->expression().evaluate(std::array{n}, e);
                diagnosticWork += result.operations;
                ++assessed;
                if (result.valid())
                    matches += result.value->integer() == reference(divisors, n);
                else if (result.status == DiscreteEvaluationStatus::ResourceExhausted)
                    ++resource;
                else
                    ++invalid;
            }
        auto s = run.algorithm().variation().statistics();
        std::cout << "{\"task\":\"" << argv[1] << "\",\"profile\":\"" << argv[2] << "\",\"seed\":" << seed
                  << ",\"loss\":" << (best ? best->fitness() : -1) << ",\"canonical\":\""
                  << (best ? best->text() : "") << "\",\"assessment_matches\":" << matches
                  << ",\"assessment_count\":" << assessed << ",\"assessment_invalid\":" << invalid
                  << ",\"assessment_resource\":" << resource
                  << ",\"diagnostic_operations\":" << diagnosticWork
                  << ",\"generation\":" << *run.population().generation()
                  << ",\"loss_attempts\":" << run.work().lossAttempts
                  << ",\"operations\":" << run.work().operations
                  << ",\"iterations\":" << run.work().iterations
                  << ",\"invalid_losses\":" << run.work().invalidLosses
                  << ",\"generated_loops\":" << s.iterationGenerated
                  << ",\"generated_dependent\":" << s.dependentGenerated
                  << ",\"body_repairs\":" << s.bodyRepairs
                  << ",\"aligned_crossovers\":" << s.alignedCrossovers
                  << ",\"published_generations_with_refs\":" << publishedWithRefs
                  << ",\"dependent_best_improvements\":" << dependentImprovements << ",\"stopping\":\""
                  << stopping << "\",\"seconds\":"
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << "}\n";
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
