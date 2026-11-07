#include <SymbolicRegression.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}

std::shared_ptr<const RegressionProblem> quadraticProblem() {
    return RegressionProblem::create({{{-1.0}, 1.0}, {{0.0}, 0.0}, {{1.0}, 1.0}}, {31, 6, 4});
}

Expression squareExpression() {
    return Expression::binary(ExpressionKind::Multiply, Expression::variable(0),
                              Expression::variable(0));
}

void penaltyKeepsLossSeparate() {
    const auto problem = quadraticProblem();
    ExpressionCandidate linear(Expression::variable(0), problem);
    ExpressionCandidate square(squareExpression(), problem);
    linear.calculateFitness();
    square.calculateFitness();
    ComplexityPenaltyOrdering ordering(1.0);
    check(linear.fitness() == 4.0 / 3.0 && square.fitness() == 0.0,
          "candidate fitness remains prediction MSE");
    check(ordering.selectionScore(linear) < ordering.selectionScore(square),
          "penalty score is explicit and separate from prediction loss");
    check(ordering.better(linear, square, ObjectiveDirection::Minimize),
          "complexity penalty can prefer a smaller approximate model");
    ComplexityPenaltyOrdering noPenalty(0.0);
    check(noPenalty.better(square, linear, ObjectiveDirection::Minimize),
          "a sufficiently accurate complex model remains eligible");
    ComplexityPenaltyOrdering moderatePenalty(0.1);
    check(moderatePenalty.better(square, linear, ObjectiveDirection::Minimize),
          "a complex model wins when its accuracy improvement exceeds the penalty");
}

void paretoFrontAndSurvival() {
    const auto problem = quadraticProblem();
    std::vector<ExpressionCandidate> candidates{
        ExpressionCandidate(Expression::variable(0), problem),
        ExpressionCandidate(squareExpression(), problem),
        ExpressionCandidate(Expression::binary(ExpressionKind::Add, squareExpression(),
                                               Expression::constant(0.0, 1)),
                            problem),
        ExpressionCandidate(Expression::variable(0), problem)};
    for (auto& candidate : candidates)
        candidate.calculateFitness();

    ParetoSelectionWork work;
    ParetoRegressionOrdering ordering(10'000, 1'000'000, work);
    const auto frontier = ordering.frontier(candidates, 8);
    check(frontier.size() == 2, "frontier removes dominated and duplicate structures");
    check(frontier[0].fitness() == 0.0 && frontier[1].fitness() == 4.0 / 3.0,
          "frontier is returned in deterministic loss order");
    check(frontier[0].expression().nodeCount() == 3 && frontier[1].expression().nodeCount() == 1,
          "frontier preserves the accuracy-complexity tradeoff");

    std::vector<ExpressionCandidate> pool = candidates;
    ordering.environmentalSelect(pool, candidates, 2);
    check(pool.size() == 2 && pool[0].text() != pool[1].text(),
          "crowding survival retains distinct Pareto representatives");
    ordering.rank(pool, ObjectiveDirection::Minimize);
    check(pool.front().fitnessState() == FitnessState::Valid && pool.back().fitness() == 0.0,
          "Pareto ranking preserves best-at-back population semantics");
    check(work.dominanceComparisons > 0 && work.selectionComparisons > 0 &&
              work.peakEstimatedMemoryBytes > 0,
          "Pareto comparisons and estimated memory are accounted");

    const auto x = Expression::variable(0);
    const auto zero = Expression::constant(0.0, 1);
    ExpressionCandidate invalid(Expression::binary(ExpressionKind::Divide, x, zero), problem);
    invalid.calculateFitness();
    check(invalid.fitnessState() == FitnessState::Invalid,
          "mathematically invalid candidates retain invalid fitness");
    auto withInvalid = candidates;
    withInvalid.push_back(invalid);
    const auto validFront = ordering.frontier(withInvalid, 8);
    check(validFront.size() == 2,
          "invalid candidates are excluded from the prediction-complexity frontier");
    auto survivorsWithInvalid = withInvalid;
    ordering.environmentalSelect(survivorsWithInvalid, withInvalid, 5);
    check(survivorsWithInvalid.size() == 5 &&
              std::count_if(survivorsWithInvalid.begin(), survivorsWithInvalid.end(),
                            [](const auto& candidate) {
                                return candidate.fitnessState() == FitnessState::Invalid;
                            }) == 1 &&
              survivorsWithInvalid.front().fitnessState() == FitnessState::Invalid &&
              std::all_of(
                  survivorsWithInvalid.begin(), survivorsWithInvalid.end(),
                  [](const auto& candidate) { return candidate.expression().nodeCount() > 0; }),
          "survival keeps invalid candidates behind all valid candidates without moving one slot "
          "twice");

    ParetoSelectionWork publicationWork;
    ParetoRegressionOrdering publicationOrdering(10'000, 1'000'000, publicationWork);
    RandomContext publicationRandom(81);
    std::vector<ExpressionCandidate> parents{ExpressionCandidate(Expression::variable(0), problem),
                                             ExpressionCandidate(squareExpression(), problem)};
    Population<ExpressionCandidate, ParetoRegressionOrdering> population(
        parents, publicationRandom, ObjectiveDirection::Minimize, false, publicationOrdering);
    std::vector<ExpressionCandidate> invalidOffspring{invalid, invalid};
    const auto publication = population.publishGeneration(invalidOffspring);
    const auto selectedParent = population.bestSeen();
    check(publication == GenerationOutcome::Committed && population.generation() == 1 &&
              selectedParent && selectedParent->fitnessState() == FitnessState::Valid,
          "environmental survival retains valid parents when every offspring is invalid");

    ExpressionCandidate leftPlusZero(Expression::binary(ExpressionKind::Add, x, zero), problem);
    ExpressionCandidate zeroPlusRight(Expression::binary(ExpressionKind::Add, zero, x), problem);
    leftPlusZero.calculateFitness();
    zeroPlusRight.calculateFitness();
    const std::vector<ExpressionCandidate> tiedCandidates{leftPlusZero, zeroPlusRight};
    auto tied = ordering.frontier(tiedCandidates, 8);
    check(tied.size() == 2 && tied[0].fitness() == tied[1].fitness() &&
              tied[0].expression().nodeCount() == tied[1].expression().nodeCount(),
          "equal-objective ties remain distinct nondominated structures");
    const auto tiedAgain = ordering.frontier(tiedCandidates, 8);
    check(tiedAgain.size() == tied.size() && tiedAgain[0].text() == tied[0].text() &&
              tiedAgain[1].text() == tied[1].text(),
          "candidate ordering is deterministic for tied objective values");
}

void limitsAndSearchDeterminism() {
    auto problem = quadraticProblem();
    SymbolicRegressionConfig config;
    config.population = 12;
    config.elites = 2;
    config.maxGenerationAttempts = 8;
    config.maxEvaluations = 108;
    config.grammar.operators = {ExpressionKind::Add, ExpressionKind::Multiply};
    config.selectionStrategy = SymbolicRegressionConfig::SelectionStrategy::Pareto;
    config.maximumParetoFrontier = 4;

    auto run = [&](unsigned threads) {
        RandomContext random(731, ExecutionConfig{threads});
        return symbolicRegression(problem, config, random);
    };
    const auto serial = run(1);
    const auto parallel = run(2);
    check(serial.best && parallel.best && serial.best->text() == parallel.best->text() &&
              serial.paretoFrontier.size() == parallel.paretoFrontier.size(),
          "Pareto ordering and selected outputs are deterministic across evaluation concurrency");
    check(serial.paretoFrontier.size() <= config.maximumParetoFrontier &&
              serial.paretoWork.dominanceComparisons > 0 &&
              serial.paretoWork.selectionComparisons > 0 &&
              serial.paretoWork.peakEstimatedMemoryBytes <=
                  config.maximumParetoEstimatedMemoryBytes,
          "search returns a bounded frontier and reports bounded selection work");
    for (std::size_t i = 0; i < serial.paretoFrontier.size(); ++i) {
        const auto& point = serial.paretoFrontier[i];
        check(point.predictionLoss == point.candidate.fitness() &&
                  point.structuralComplexity == point.candidate.expression().nodeCount(),
              "frontier reports prediction loss and structural complexity separately");
        for (std::size_t j = i + 1; j < serial.paretoFrontier.size(); ++j)
            check(serial.paretoFrontier[i].candidate.text() !=
                      serial.paretoFrontier[j].candidate.text(),
                  "structurally duplicate expressions appear once in the frontier");
    }

    config.selectionStrategy = SymbolicRegressionConfig::SelectionStrategy::ComplexityPenalty;
    config.complexityPenalty = 0.25;
    RandomContext penalizedRandom(731);
    const auto penalized = symbolicRegression(problem, config, penalizedRandom);
    check(penalized.best && penalized.bestSelectionScore &&
              *penalized.bestSelectionScore == static_cast<long double>(penalized.best->fitness()) +
                                                   0.25L * penalized.best->expression().nodeCount(),
          "penalty search returns prediction loss and selection score independently");

    config.selectionStrategy = SymbolicRegressionConfig::SelectionStrategy::Pareto;
    config.maximumParetoComparisons = 1;
    RandomContext workLimited(731);
    const auto limited = symbolicRegression(problem, config, workLimited);
    check(limited.stoppingReason == SearchStop::SelectionResourceLimit && !limited.best,
          "comparison exhaustion is an explicit incomplete result");
    config.maximumParetoComparisons = 10'000;
    config.maximumParetoEstimatedMemoryBytes = 1;
    RandomContext memoryLimited(731);
    const auto memory = symbolicRegression(problem, config, memoryLimited);
    check(memory.stoppingReason == SearchStop::SelectionResourceLimit && !memory.best &&
              memory.paretoWork.peakEstimatedMemoryBytes > config.maximumParetoEstimatedMemoryBytes,
          "estimated-memory exhaustion is explicit");
}
} // namespace

int main() {
    try {
        penaltyKeepsLossSeparate();
        paretoFrontAndSurvival();
        limitsAndSearchDeterminism();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
