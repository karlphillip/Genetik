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
    // Public checked-loss entry point exercises invocation-local row storage.
    std::vector<RegressionRow> repeatedRows;
    for(int i=0;i<64;++i)repeatedRows.push_back({{2.},7.});
    const auto repeated=RegressionProblem::create(repeatedRows);
    std::uint64_t visited=0;
    const auto measured=repeated->measure(polynomial,&visited);
    if(measured.loss.score()!=0 || visited!=64 || measured.samplesAttempted!=64) return 1;
    const auto problem = RegressionProblem::create({{{0}, .125}, {{1}, .375}});
    SymbolicRegressionConfig config;
    config.population = 4; config.elites = 1; config.maxGenerationAttempts = 1; config.maxEvaluations = 8;
    RandomContext symbolic(17);
    const auto result = symbolicRegression(problem, config, symbolic);
    if (!result.best || result.candidateEvaluations > 8 || result.publishedGenerations != 1) return 1;
    const auto continuousProblem = RegressionProblem::create({{{0}, 2.5}, {{1}, 2.5}}, {31,6,8});
    TrainingObjective objective(continuousProblem);
    const auto fitted = fitConstants(Expression::constant(1,1), objective);
    if (fitted.loss.score() != 0 || fitted.expression.nodes()[0].constantValue() != 2.5) return 1;
    config.fitting = ConstantFitConfig{};
    config.maxTrainingLossAttempts = 64; config.fitting->maxLossAttempts = 4;
    RandomContext continuous(9,{2});
    const auto refined = symbolicRegression(continuousProblem, config, continuous);
    if (!refined.best || refined.trainingWork.fittingInvocations == 0 || refined.trainingWork.lossAttempts > 64) return 1;
    // Ordinary caller-supplied three-input rows; no example/file support headers.
    const auto multivariate = RegressionProblem::create({{{1,2,3}, -2.5}, {{2,1,0}, 1}}, {31,6,8});
    if (Expression::variable(2,3).evaluate(std::array{1.,2.,3.}).value() != 3) return 1;
    RandomContext multivariateRun(2);
    const auto multivariateResult = symbolicRegression(multivariate, config, multivariateRun);
    if (!multivariateResult.best || multivariateResult.best->expression().variableCount() != 3 ||
        multivariateResult.trainingWork.lossAttempts > 64 ||
        multivariate->measure(multivariateResult.best->expression()).loss != multivariateResult.best->evaluation()) return 1;
    const auto u=Expression::variable(0,2),v=Expression::variable(1,2);
    const auto mixed=Expression::binary(ExpressionKind::Add,Expression::unary(ExpressionKind::Log,u),
                                       Expression::binary(ExpressionKind::Divide,v,u));
    if (mixed.nodeCount()!=6 || mixed.toString()!="(log(x0) + (x1 / x0))" ||
        std::abs(mixed.evaluate(std::array{2.,4.}).value()-2.6931471805599453)>2e-15) return 1;
    config.grammar.operators={ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply,ExpressionKind::Divide,ExpressionKind::Log};
    RandomContext expandedRun(9);
    const auto expandedResult=symbolicRegression(multivariate,config,expandedRun);
    if (!expandedResult.best || expandedResult.trainingWork.lossAttempts>64) return 1;
    // A sequence application supplies normalized index/value rows through the same public API.
    const auto sequence = RegressionProblem::create({{{.001},.0002},{{.002},.0003},{{.003},.0005}}, {31,6,8});
    config.trainingLossThreshold=0;config.fitting->lossThreshold=0;
    config.fitEveryGenerations=4;
    RandomContext sequenceRun(42,{3});
    const auto sequenceResult=symbolicRegression(sequence,config,sequenceRun);
    if (!sequenceResult.best || sequenceResult.trainingWork.lossAttempts>64 || sequenceResult.preparationWork.skippedBySchedule==0 ||
        sequence->measure(sequenceResult.best->expression()).loss!=sequenceResult.best->evaluation()) return 1;
    RandomContext run(9,{3});
    Population<Creature<char>> population(3, run);
    GeneticAlgorithm<decltype(population), Creature<char>> ga(0.5f, 0.9f, run);
    if (ga.evolve(population) != GenerationOutcome::Committed) return 1;
    const auto best = population.bestSeen();
    return ga.evolutions() == 1 && population.generation() == 1 && best &&
           best->fitnessState() == FitnessState::Valid && run.evaluationAttempts() == 6 ? 0 : 1;
}
