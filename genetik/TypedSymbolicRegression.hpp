#pragma once
#include "TypedVariation.hpp"
#include "SymbolicRegression.hpp"
#include "ConstantFitting.hpp"
#include "GeneticAlgorithm.hpp"
#include "Population.hpp"

struct TypedSymbolicRegressionConfig {
    std::size_t population = 128;
    std::uint32_t maxGenerationAttempts = 250;
    std::uint64_t maxEvaluations = 32128;
    std::size_t elites = 2;
    float crossoverRate = .9f, mutationRate = .35f;
    TypedGrammar grammar;
    // Optional aggregate fitting is added at checkpoint 3.
    std::uint64_t maxTrainingLossAttempts = std::numeric_limits<std::uint64_t>::max();
    double trainingLossThreshold = 0;
    std::uint64_t fitEveryGenerations=1; // positive; generation zero always eligible
};
struct TypedSymbolicRegressionResult {
    std::optional<TypedExpressionCandidate> best;
    std::optional<double> initialBestMSE;
    std::optional<std::uint64_t> bestGeneration;
    std::uint64_t attemptedGenerations = 0, publishedGenerations = 0, candidateEvaluations = 0;
    TypedVariationStatistics variation;
    std::uint64_t arithmeticOperations=0;
    TrainingWork trainingWork;
    PreparationStatistics preparationWork;
    TrainingWork initialWork;
    std::vector<std::string> preparedInitialExpressions;
    std::vector<Fitness> preparedInitialFitness;
    std::uint64_t unpublishedLossAttempts=0,unpublishedRowAttempts=0,unpublishedCandidateRequests=0;
    std::optional<double> rawInitialBestMSE;
    std::vector<std::string> rawInitialExpressions; // exact pre-refinement audit, in slot order
    struct Improvement { std::uint64_t lossAttempts, generation; double mse; };
    std::vector<Improvement> improvements; // committed best only, at publication boundaries
    SearchStop stoppingReason = SearchStop::GenerationLimit;
};

// Only controls generation-boundary budgets/stopping; evolution/publication stays in the shared GA.
inline TypedSymbolicRegressionResult typedSymbolicRegression(std::shared_ptr<const TypedRegressionProblem> problem,
                              const TypedSymbolicRegressionConfig& config, RandomContext& random,
                              std::shared_ptr<TypedTrainingObjective> objective = {}) {
    if (random.evaluationAttempts() != 0) throw std::invalid_argument("symbolic search requires a fresh run context");
    if (config.fitEveryGenerations==0 || config.population == 0 || config.population > std::numeric_limits<unsigned int>::max() ||
        config.elites >= config.population || !std::isfinite(config.crossoverRate) ||
        !std::isfinite(config.mutationRate) || config.crossoverRate < 0 || config.crossoverRate > 1 ||
        config.mutationRate < 0 || config.mutationRate > 1 ||
        !std::isfinite(config.trainingLossThreshold) || config.trainingLossThreshold < 0)
        throw std::invalid_argument("invalid symbolic search configuration");
    if (!objective) objective = std::make_shared<TypedTrainingObjective>(problem, config.maxTrainingLossAttempts);
    if (objective->problem() != problem || objective->maximum() != config.maxTrainingLossAttempts ||
        objective->work() != TrainingWork{})
        throw std::invalid_argument("symbolic search requires a fresh objective with matching problem and loss allowance");
    if(random.execution().requested()>1 && !objective->parallelEvaluationSafe())
        throw std::invalid_argument("parallel search requires independent objective factories and no detailed refinement diagnostics");
    if(random.execution().requested()>1)(void)objective->forkParallel(random.execution()); // validate factory before initialization
    auto preparation = std::make_shared<TypedScoring>(objective);
    TypedVariation variation(problem, config.grammar, preparation);
    TypedSymbolicRegressionResult result;
    if (config.maxEvaluations < config.population || config.maxTrainingLossAttempts < config.population) {
        result.stoppingReason = SearchStop::EvaluationBudget; return result;
    }
    std::vector<TypedExpressionCandidate> initial;
    initial.reserve(config.population);
    {
    
    for (std::size_t i = 0; i < config.population; ++i) {
        initial.push_back(variation.initialize(random.engine()));
        result.rawInitialExpressions.push_back(initial.back().text());
    }
    }
    using P = Population<TypedExpressionCandidate, TypedRegressionOrdering>;
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
    try {
        
        storage = std::make_unique<P>(initial, random, ObjectiveDirection::Minimize,false);
    }
    catch (...) {
        const auto failure=std::current_exception();
        if (!budgetFailure(failure) && !isEvaluationCancelled(failure)) throw;
        result.stoppingReason = budgetFailure(failure)?SearchStop::EvaluationBudget:SearchStop::Cancelled;
        result.candidateEvaluations = random.evaluationAttempts(); result.trainingWork = objective->work(); result.arithmeticOperations=objective->arithmeticOperations();
        result.initialWork=result.trainingWork;
        result.variation = variation.statistics(); result.rawInitialBestMSE = std::optional<double>{};
        
        result.unpublishedLossAttempts=result.trainingWork.lossAttempts;
        result.unpublishedRowAttempts=result.trainingWork.rowAttempts;
        result.unpublishedCandidateRequests=result.candidateEvaluations;
        return result; // no initial generation committed
    }
    auto& population = *storage;

    result.initialWork=objective->work();
    if(population.generation()) {
        const auto prepared=population.creatures(); // retain detached snapshot through iteration in C++20
        for(const auto& candidate:*prepared) {
            result.preparedInitialExpressions.push_back(candidate.text());
            result.preparedInitialFitness.push_back(candidate.evaluation());
        }
    }
    auto committedLosses=population.generation() ? objective->work().lossAttempts:0;
    auto committedRows=population.generation() ? objective->work().rowAttempts:0;
    auto committedRequests=population.generation() ? random.evaluationAttempts():0;
    result.rawInitialBestMSE = std::optional<double>{};
    GeneticAlgorithm<P, TypedExpressionCandidate, TypedVariation> ga(config.crossoverRate, config.mutationRate,
                                                                    random, std::move(variation), config.elites);
    if (population.bestSeen()) {
        result.initialBestMSE = population.bestSeen()->fitness();
        result.rawInitialBestMSE=result.initialBestMSE;
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
            const auto failure=std::current_exception();
            if (!budgetFailure(failure) && !isEvaluationCancelled(failure)) throw;
            result.stoppingReason = budgetFailure(failure)?SearchStop::EvaluationBudget:SearchStop::Cancelled; break;
        }
        committedLosses=objective->work().lossAttempts;committedRows=objective->work().rowAttempts;
        committedRequests=random.evaluationAttempts();
        const auto current = population.bestSeen();
        if (current && (!previous || current->text() != previous->text() || current->fitness() != previous->fitness()))
            result.improvements.push_back({objective->work().lossAttempts, *population.generation(), current->fitness()});
        previous = current;
    }
    result.best = population.bestSeen(); result.bestGeneration = population.bestGeneration();
    result.attemptedGenerations = ga.attempts(); result.publishedGenerations = ga.evolutions();
    result.trainingWork = objective->work(); result.arithmeticOperations=objective->arithmeticOperations();
    
    result.unpublishedLossAttempts=result.trainingWork.lossAttempts-committedLosses;
    result.unpublishedRowAttempts=result.trainingWork.rowAttempts-committedRows;
    result.unpublishedCandidateRequests=random.evaluationAttempts()-committedRequests;

    result.candidateEvaluations = random.evaluationAttempts(); result.variation = ga.variation().statistics();
    return result;
}
