#pragma once
#include "DiscreteVariation.hpp"
#include "GeneticAlgorithm.hpp"
#include "Population.hpp"
#include <cmath>

struct DiscreteSearchConfig {
    std::size_t population=64,elites=2;
    std::uint32_t maxGenerations=100;
    std::uint64_t maxLossAttempts=6464;
    float crossoverRate=.9f,mutationRate=.35f;
    DiscreteGrammar grammar;
    // Optional immutable application semantics; empty preserves D2 identity bytes.
    std::string contextIdentity;
};
enum class DiscreteSearchStop { ExactFit, GenerationLimit, LossBudget, NoViableCandidates, Cancelled };
struct DiscreteSearchResult {
    std::optional<DiscreteExpressionCandidate> best;
    DiscreteSearchStop stop=DiscreteSearchStop::GenerationLimit;
    std::uint64_t attemptedGenerations=0,publishedGenerations=0,candidateRequests=0;
    DiscreteTrainingWork work;
    DiscreteVariationStatistics variation;
};
inline DiscreteSearchResult discreteSymbolicRegression(std::shared_ptr<const DiscreteRegressionProblem> problem,const DiscreteSearchConfig& config,RandomContext& random) {
    if(!problem||random.evaluationAttempts()!=0||config.population<2||config.population>100000||config.elites>=config.population||!std::isfinite(config.crossoverRate)||!std::isfinite(config.mutationRate)||config.crossoverRate<0||config.crossoverRate>1||config.mutationRate<0||config.mutationRate>1)
        throw std::invalid_argument("invalid discrete search configuration or nonfresh context");
    auto objective=std::make_shared<DiscreteTrainingObjective>(problem,config.maxLossAttempts,&random.execution());
    DiscreteVariation variation(problem,config.grammar,objective); // validate before RNG or candidate work
    DiscreteSearchResult result;
    if(config.maxLossAttempts<config.population){result.stop=DiscreteSearchStop::LossBudget;return result;}
    if(random.execution().stopRequested()){result.stop=DiscreteSearchStop::Cancelled;return result;}
    std::vector<DiscreteExpressionCandidate> initial;initial.reserve(config.population);
    for(std::size_t i=0;i<config.population;++i)initial.push_back(variation.initialize(random.engine()));
    using P=Population<DiscreteExpressionCandidate,DiscreteRegressionOrdering>;
    GeneticAlgorithm<P,DiscreteExpressionCandidate,DiscreteVariation> ga(config.crossoverRate,config.mutationRate,random,std::move(variation),config.elites);
    std::unique_ptr<P> population;
    try{
        population=std::make_unique<P>(initial,random,ObjectiveDirection::Minimize);
        for(;;){
            const auto best=population->bestSeen();
            if(!best){result.stop=DiscreteSearchStop::NoViableCandidates;break;}
            if(best->fitness()==0){result.stop=DiscreteSearchStop::ExactFit;break;}
            if(ga.attempts()==config.maxGenerations){result.stop=DiscreteSearchStop::GenerationLimit;break;}
            if(objective->remaining()<config.population){result.stop=DiscreteSearchStop::LossBudget;break;}
            if(random.execution().stopRequested()){result.stop=DiscreteSearchStop::Cancelled;break;}
            if(ga.evolve(*population)==GenerationOutcome::NoViableCandidates){result.stop=DiscreteSearchStop::NoViableCandidates;break;}
        }
    }catch(...){const auto failure=std::current_exception();if(!isEvaluationStop(failure))throw;result.stop=isEvaluationBudget(failure)?DiscreteSearchStop::LossBudget:DiscreteSearchStop::Cancelled;}
    if(population)result.best=population->bestSeen();
    result.attemptedGenerations=ga.attempts();result.publishedGenerations=ga.evolutions();result.candidateRequests=random.evaluationAttempts();result.work=objective->work();result.variation=ga.variation().statistics();return result;
}
