#pragma once
#include "ExpressionVariation.hpp"
#include "ConstantFitting.hpp"
#include "GeneticAlgorithm.hpp"
#include "Population.hpp"

struct SymbolicRegressionConfig {
    std::size_t population = 128;
    std::uint32_t maxGenerationAttempts = 250;
    std::uint64_t maxEvaluations = 32128;
    std::size_t elites = 2;
    float crossoverRate = .9f, mutationRate = .35f;
    ExpressionGrammar grammar;
    std::optional<ConstantFitConfig> fitting; // disabled retains discrete mode
    std::uint64_t maxTrainingLossAttempts = std::numeric_limits<std::uint64_t>::max();
    double trainingLossThreshold = 0;
};
enum class SearchStop { ExactTrainingFit, GenerationLimit, EvaluationBudget, NoViableCandidates, TrainingLossThreshold };
inline const char* searchStopName(SearchStop stop) {
    switch (stop) {
    case SearchStop::TrainingLossThreshold: return "training_loss_threshold";
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
    TrainingWork trainingWork;
    std::optional<double> rawInitialBestMSE;
    std::vector<std::string> rawInitialExpressions; // exact pre-refinement audit, in slot order
    struct Improvement { std::uint64_t lossAttempts, generation; double mse; };
    std::vector<Improvement> improvements; // committed best only, at publication boundaries
    SearchStop stoppingReason = SearchStop::GenerationLimit;
};

// Only controls generation-boundary budgets/stopping; evolution/publication stays in the shared GA.
inline SymbolicRegressionResult symbolicRegression(std::shared_ptr<const RegressionProblem> problem,
                              const SymbolicRegressionConfig& config, RandomContext& random,
                              std::shared_ptr<TrainingObjective> objective = {}) {
    if (random.evaluationAttempts() != 0) throw std::invalid_argument("symbolic search requires a fresh run context");
    if (config.population == 0 || config.population > std::numeric_limits<unsigned int>::max() ||
        config.elites >= config.population || !std::isfinite(config.crossoverRate) ||
        !std::isfinite(config.mutationRate) || config.crossoverRate < 0 || config.crossoverRate > 1 ||
        config.mutationRate < 0 || config.mutationRate > 1 ||
        !std::isfinite(config.trainingLossThreshold) || config.trainingLossThreshold < 0)
        throw std::invalid_argument("invalid symbolic search configuration");
    if (!objective) objective = std::make_shared<TrainingObjective>(problem, config.maxTrainingLossAttempts);
    if (objective->problem() != problem || objective->maximum() != config.maxTrainingLossAttempts ||
        objective->work() != TrainingWork{})
        throw std::invalid_argument("symbolic search requires a fresh objective with matching problem and loss allowance");
    auto preparation = std::make_shared<ConstantRefinement>(objective, config.fitting);
    ExpressionVariation variation(problem, config.grammar, preparation);
    SymbolicRegressionResult result;
    if (config.maxEvaluations < config.population || config.maxTrainingLossAttempts < config.population) {
        result.stoppingReason = SearchStop::EvaluationBudget; return result;
    }
    std::vector<ExpressionCandidate> initial;
    initial.reserve(config.population);
    for (std::size_t i = 0; i < config.population; ++i) {
        initial.push_back(variation.initialize(random.engine()));
        result.rawInitialExpressions.push_back(initial.back().text());
    }
    using P = Population<ExpressionCandidate, RegressionOrdering>;
    // Recognize only the explicit allowance stop, through the engine's nested context.
    const auto budgetFailure = [](std::exception_ptr error) {
        while (error) {
            try { std::rethrow_exception(error); }
            catch (const TrainingLossBudgetExceeded&) { return true; }
            catch (const std::exception& e) {
                const auto* nested = dynamic_cast<const std::nested_exception*>(&e);
                error = nested ? nested->nested_ptr() : std::exception_ptr{};
            }
            catch (...) { return false; }
        }
        return false;
    };
    std::unique_ptr<P> storage;
    try { storage = std::make_unique<P>(initial, random, ObjectiveDirection::Minimize); }
    catch (...) {
        if (!budgetFailure(std::current_exception())) throw;
        result.stoppingReason = SearchStop::EvaluationBudget;
        result.candidateEvaluations = random.evaluationAttempts(); result.trainingWork = objective->work();
        result.variation = variation.statistics(); result.rawInitialBestMSE = preparation->rawInitialBest;
        return result; // no initial generation committed
    }
    auto& population = *storage;
    preparation->initializing = false;
    result.rawInitialBestMSE = preparation->rawInitialBest;
    GeneticAlgorithm<P, ExpressionCandidate, ExpressionVariation> ga(config.crossoverRate, config.mutationRate,
                                                                    random, std::move(variation), config.elites);
    if (population.bestSeen()) {
        result.initialBestMSE = population.bestSeen()->fitness();
        result.improvements.push_back({objective->work().lossAttempts, 0, *result.initialBestMSE});
    }
    auto previous = population.bestSeen();
    for (;;) {
        const auto best = population.bestSeen();
        if (!best) { result.stoppingReason = SearchStop::NoViableCandidates; break; }
        if (best->fitness() <= config.trainingLossThreshold) {
            result.stoppingReason = config.trainingLossThreshold == 0 ? SearchStop::ExactTrainingFit : SearchStop::TrainingLossThreshold;
            break;
        }
        if (ga.attempts() >= config.maxGenerationAttempts) { result.stoppingReason = SearchStop::GenerationLimit; break; }
        if (config.maxEvaluations - random.evaluationAttempts() < config.population || objective->remaining() < config.population) {
            result.stoppingReason = SearchStop::EvaluationBudget; break;
        }
        try {
            if (ga.evolve(population) == GenerationOutcome::NoViableCandidates) {
                result.stoppingReason = SearchStop::NoViableCandidates; break;
            }
        } catch (...) {
            if (!budgetFailure(std::current_exception())) throw;
            result.stoppingReason = SearchStop::EvaluationBudget; break;
        }
        const auto current = population.bestSeen();
        if (current && (!previous || current->text() != previous->text() || current->fitness() != previous->fitness()))
            result.improvements.push_back({objective->work().lossAttempts, *population.generation(), current->fitness()});
        previous = current;
    }
    result.best = population.bestSeen(); result.bestGeneration = population.bestGeneration();
    result.attemptedGenerations = ga.attempts(); result.publishedGenerations = ga.evolutions();
    result.trainingWork = objective->work();
    result.candidateEvaluations = random.evaluationAttempts(); result.variation = ga.variation().statistics();
    return result;
}
