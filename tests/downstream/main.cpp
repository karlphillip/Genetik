#include <GeneticAlgorithm.hpp>
#include <Population.hpp>
#include <Creature.hpp>
#include <Expression.hpp>
#include <array>
#include <SymbolicRegression.hpp>
static_assert(__cplusplus >= 202002L);
int main() {
    const auto x = Expression::variable(0);
    const auto square = Expression::binary(ExpressionKind::Multiply, x, x);
    const auto polynomial = Expression::binary(ExpressionKind::Add,
        Expression::binary(ExpressionKind::Add, square, x), Expression::constant(1, 1));
    if (polynomial.evaluate(std::array{2.}).value() != 7 || polynomial.nodeCount() != 7 ||
        polynomial.depth() != 4 || polynomial.toString() != "(((x0 * x0) + x0) + 1.0)") return 1;
    const auto problem = RegressionProblem::create({{{0}, .125}, {{1}, .375}});
    SymbolicRegressionConfig config;
    config.population = 4; config.elites = 1; config.maxGenerationAttempts = 1; config.maxEvaluations = 8;
    RandomContext symbolic(17);
    const auto result = symbolicRegression(problem, config, symbolic);
    if (!result.best || result.candidateEvaluations > 8 || result.publishedGenerations != 1) return 1;
    RandomContext run(9);
    Population<Creature<char>> population(3, run);
    GeneticAlgorithm<decltype(population), Creature<char>> ga(0.5f, 0.9f, run);
    if (ga.evolve(population) != GenerationOutcome::Committed) return 1;
    const auto best = population.bestSeen();
    return ga.evolutions() == 1 && population.generation() == 1 && best &&
           best->fitnessState() == FitnessState::Valid && run.evaluationAttempts() == 6 ? 0 : 1;
}
