#include <SymbolicRegression.hpp>
#include <chrono>
#include <iomanip>
#include <iostream>

static std::vector<RegressionRow> samples(const std::vector<double>& xs) {
    std::vector<RegressionRow> rows;
    for (double x : xs) rows.push_back({{x}, x*x + x + 1});
    return rows;
}
static void error(const std::exception& e) {
    std::cerr << e.what() << '\n';
    try { std::rethrow_if_nested(e); }
    catch (const std::exception& cause) { error(cause); }
    catch (...) { std::cerr << "non-standard cause\n"; }
}
int main() {
    std::cout << std::setprecision(17);
    std::vector<double> trainX, interiorX;
    for (int k = 0; k <= 16; ++k) trainX.push_back(-2. + k/4.);
    for (int k = 0; k < 16; ++k) interiorX.push_back(-15./8 + k/4.);
    const auto training = RegressionProblem::create(samples(trainX));
    SymbolicRegressionConfig config;
    std::cout << "campaign=initial mode=serial_cpu seeds=0,1,2,3,42 population=128 max_attempts=250 max_evaluations=32128"
              << " nodes=31 depth=6 constants=-2,-1,0,1,2 selector=linear_rank elites=2 crossover=0.9 mutation=0.35"
              << " init_max_depth=4 mutation_max_depth=3 full_probability=0.5 grow_binary_probability=0.65 proposals=8"
              << " objective=training_mse success_stop=exact_zero recovery_max_abs=1e-9\n";
    struct Run { std::uint32_t seed; SymbolicRegressionResult search; RegressionMetrics training; double seconds; };
    std::vector<Run> runs;
    bool failure = false;
    // No interior/exterior data are evaluated until all training searches and the adjustment decision finish.
    for (std::uint32_t seed : {0u, 1u, 2u, 3u, 42u}) {
        RandomContext random(seed);
        const auto start = std::chrono::steady_clock::now();
        try {
            auto result = symbolicRegression(training, config, random);
            const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            RegressionMetrics metrics;
            if (result.best) metrics = training->measure(result.best->expression());
            const auto s = result.variation;
            std::cout << "seed=" << seed << " initial_mse=" << result.initialBestMSE.value_or(-1)
                      << " final_mse=" << (result.best ? result.best->fitness() : -1)
                      << " training_max_abs=" << metrics.maxAbsoluteError.value_or(-1)
                      << " best_generation=" << result.bestGeneration.value_or(0)
                      << " attempts=" << result.attemptedGenerations << " published=" << result.publishedGenerations
                      << " evaluations=" << result.candidateEvaluations << " seconds=" << seconds
                      << " stop=" << searchStopName(result.stoppingReason)
                      << " init_proposals=" << s.initializations << " mutation_proposals=" << s.mutationProposals
                      << " crossover_proposals=" << s.crossoverProposals << " rejected=" << s.structuralRejections
                      << " mutation_fallbacks=" << s.mutationFallbacks << " crossover_fallbacks=" << s.crossoverFallbacks
                      << " mutation_changes=" << s.mutationChanges << " crossover_changes=" << s.crossoverChanges << '\n';
            if (result.best) std::cout << "expression seed=" << seed << " nodes=" << result.best->expression().nodeCount()
                                      << " depth=" << result.best->expression().depth() << " " << result.best->text() << '\n';
            runs.push_back({seed, std::move(result), metrics, seconds});
        } catch (const std::exception& e) {
            failure = true;
            std::cout << "seed=" << seed << " stop=operational_failure evaluations=" << random.evaluationAttempts() << '\n';
            error(e);
        }
    }
    const bool recovered = std::any_of(runs.begin(), runs.end(), [](const auto& r) {
        return r.training.maxAbsoluteError && *r.training.maxAbsoluteError <= 1e-9;
    });
    if (!recovered) {
        std::cout << "training_adjustment_decision=inspect_training_diagnostics_before_any_heldout_evaluation\n";
        return 2;
    }
    std::cout << "training_adjustment_decision=not_needed representative_rule=mse_then_nodes_then_text_then_lowest_seed\n";
    // Selection uses only committed training ordering, never evaluation-only results.
    const Run* representative = nullptr;
    RegressionOrdering order;
    for (const auto& run : runs) if (run.search.best && (!representative ||
        order.better(*run.search.best, *representative->search.best, ObjectiveDirection::Minimize))) representative = &run;
    std::cout << "representative_seed=" << representative->seed << " expression=" << representative->search.best->text() << '\n';
    const auto interior = RegressionProblem::create(samples(interiorX));
    const auto exterior = RegressionProblem::create(samples({-4., -3., 3., 4.}));
    for (const auto& run : runs) if (run.search.best) {
        const auto a = interior->measure(run.search.best->expression()), b = exterior->measure(run.search.best->expression());
        std::cout << "evaluation seed=" << run.seed << " interior_max_abs=" << a.maxAbsoluteError.value_or(-1)
                  << " exterior_max_abs=" << b.maxAbsoluteError.value_or(-1)
                  << " training_recovery=" << (*run.training.maxAbsoluteError <= 1e-9)
                  << " interior_recovery=" << (a.maxAbsoluteError && *a.maxAbsoluteError <= 1e-9)
                  << " exterior_recovery=" << (b.maxAbsoluteError && *b.maxAbsoluteError <= 1e-9)
                  << " postrun_candidate_metric_calls=3 postrun_samples="
                  << run.training.samplesAttempted + a.samplesAttempted + b.samplesAttempted << '\n';
    }
    return failure ? 1 : 0;
}
