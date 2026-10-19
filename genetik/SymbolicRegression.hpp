#pragma once
#include "ExpressionVariation.hpp"
#include "GeneticAlgorithm.hpp"
#include "Population.hpp"

struct SymbolicRegressionConfig {
    std::size_t population = 128;
    std::uint32_t maxGenerationAttempts = 250;
    std::uint64_t maxEvaluations = 32128;
    std::size_t elites = 2;
    float crossoverRate = .9f, mutationRate = .35f;
    ExpressionGrammar grammar;
};
enum class SearchStop { ExactTrainingFit, GenerationLimit, EvaluationBudget, NoViableCandidates };
inline const char* searchStopName(SearchStop stop) {
    switch (stop) {
    case SearchStop::ExactTrainingFit: return "exact_training_fit";
    case SearchStop::GenerationLimit: return "generation_limit";
    case SearchStop::EvaluationBudget: return "evaluation_budget";
    case SearchStop::NoViableCandidates: return "no_viable_candidates";
    }
    throw std::logic_error("unknown search stop");
}
struct SymbolicRegressionResult {
    std::optional<ExpressionCandidate> best;
    std::optional<double> initialBestMSE;
    std::optional<std::uint64_t> bestGeneration;
    std::uint64_t attemptedGenerations = 0, publishedGenerations = 0, candidateEvaluations = 0;
    VariationStatistics variation;
    SearchStop stoppingReason = SearchStop::GenerationLimit;
};

// Only controls generation-boundary budgets/stopping; evolution/publication stays in the shared GA.
inline SymbolicRegressionResult symbolicRegression(std::shared_ptr<const RegressionProblem> problem,
                              const SymbolicRegressionConfig& config, RandomContext& random) {
    if (random.evaluationAttempts() != 0) throw std::invalid_argument("symbolic search requires a fresh run context");
    if (config.population == 0 || config.population > std::numeric_limits<unsigned int>::max() ||
        config.elites >= config.population || !std::isfinite(config.crossoverRate) ||
        !std::isfinite(config.mutationRate) || config.crossoverRate < 0 || config.crossoverRate > 1 ||
        config.mutationRate < 0 || config.mutationRate > 1)
        throw std::invalid_argument("invalid symbolic search configuration");
    ExpressionVariation variation(problem, config.grammar);
    SymbolicRegressionResult result;
    if (config.maxEvaluations < config.population) {
        result.stoppingReason = SearchStop::EvaluationBudget; return result;
    }
    std::vector<ExpressionCandidate> initial;
    initial.reserve(config.population);
    for (std::size_t i = 0; i < config.population; ++i) initial.push_back(variation.initialize(random.engine()));
    using P = Population<ExpressionCandidate, RegressionOrdering>;
    P population(initial, random, ObjectiveDirection::Minimize);
    GeneticAlgorithm<P, ExpressionCandidate, ExpressionVariation> ga(config.crossoverRate, config.mutationRate,
                                                                    random, std::move(variation), config.elites);
    if (population.bestSeen()) result.initialBestMSE = population.bestSeen()->fitness();
    for (;;) {
        const auto best = population.bestSeen();
        if (!best) { result.stoppingReason = SearchStop::NoViableCandidates; break; }
        if (best->fitness() == 0) { result.stoppingReason = SearchStop::ExactTrainingFit; break; }
        if (ga.attempts() >= config.maxGenerationAttempts) { result.stoppingReason = SearchStop::GenerationLimit; break; }
        if (config.maxEvaluations - random.evaluationAttempts() < config.population) {
            result.stoppingReason = SearchStop::EvaluationBudget; break;
        }
        if (ga.evolve(population) == GenerationOutcome::NoViableCandidates) {
            result.stoppingReason = SearchStop::NoViableCandidates; break;
        }
    }
    result.best = population.bestSeen(); result.bestGeneration = population.bestGeneration();
    result.attemptedGenerations = ga.attempts(); result.publishedGenerations = ga.evolutions();
    result.candidateEvaluations = random.evaluationAttempts(); result.variation = ga.variation().statistics();
    return result;
}
