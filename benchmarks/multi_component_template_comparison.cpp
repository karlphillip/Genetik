#include <ExpressionParsing.hpp>
#include <MultiComponentTemplateRegression.hpp>

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
constexpr ExpressionLimits modelLimits{31, 10, 8.0, 40};
constexpr std::array<std::uint64_t, 2> seeds{142857, 271828};

std::vector<RegressionRow> trainingRows() {
    std::vector<RegressionRow> rows;
    for (const double x : {-1.0, -0.5, 0.0, 0.5, 1.0})
        for (const double y : {-1.0, -0.25, 0.5, 1.0})
            rows.push_back({{x, y}, std::sin(x) + y * y});
    return rows;
}

std::vector<RegressionRow> heldOutRows() {
    std::vector<RegressionRow> rows;
    for (const std::array<double, 2> input :
         {std::array{-0.75, 0.75}, std::array{0.25, -0.75}, std::array{1.25, 0.5}})
        rows.push_back({{input[0], input[1]}, std::sin(input[0]) + input[1] * input[1]});
    return rows;
}

std::array<ExpressionGrammar, 2> componentGrammars() {
    ExpressionGrammar first;
    first.operators = {ExpressionKind::Sin, ExpressionKind::Add, ExpressionKind::Multiply};
    first.constants.clear();
    first.initialMaxDepth = 2;
    first.mutationMaxDepth = 2;
    first.growBinaryProbability = 1.0;
    ExpressionGrammar second;
    second.operators = {ExpressionKind::Add, ExpressionKind::Multiply};
    second.constants.clear();
    second.initialMaxDepth = 2;
    second.mutationMaxDepth = 2;
    second.growBinaryProbability = 1.0;
    return {first, second};
}

SymbolicRegressionConfig configFor(const std::vector<RegressionRow>& rows) {
    SymbolicRegressionConfig config;
    config.population = 64;
    config.elites = 2;
    config.maxGenerationAttempts = 24;
    config.maxEvaluations = 1600;
    config.maxTrainingLossAttempts = 1600 * rows.size();
    config.grammar.operators = {ExpressionKind::Add, ExpressionKind::Multiply, ExpressionKind::Sin};
    config.grammar.constants.clear();
    config.grammar.initialMaxDepth = 4;
    config.grammar.mutationMaxDepth = 3;
    config.grammar.growBinaryProbability = 0.65;
    return config;
}

void report(std::uint64_t seed, const char* method, const SymbolicRegressionResult& result,
            const RegressionProblem& heldOut, bool structuralRecovery) {
    const auto score = result.best ? heldOut.measure(result.best->expression()).loss.score()
                                   : std::numeric_limits<double>::infinity();
    std::cout << seed << ',' << method << ',' << searchStopName(result.stoppingReason) << ','
              << (result.best ? result.best->fitness() : std::numeric_limits<double>::infinity())
              << ',' << score << ',' << (result.best ? result.best->expression().nodeCount() : 0)
              << ',' << structuralRecovery << ',' << result.candidateEvaluations << ','
              << result.trainingWork.rowAttempts << ',' << result.publishedGenerations << '\n';
}
} // namespace

int main() {
    const auto training = trainingRows();
    const auto assessment = heldOutRows();
    auto problem = RegressionProblem::create(training, modelLimits);
    auto heldOut = RegressionProblem::create(assessment, modelLimits);
    const auto source = parseExpression("x0+x1", 2, {}, modelLimits);
    const MultiScalarExpressionTemplate scaffold(
        source, 0, 1, {ExpressionLimits{15, 5, 8.0, 20}, ExpressionLimits{15, 5, 8.0, 20}},
        modelLimits);
    const auto domain = MultiComponentTemplateProblem::create(
        scaffold, problem, std::array<std::vector<std::size_t>, 2>{{{0}, {1}}},
        std::array<std::vector<ExpressionKind>, 2>{
            {{ExpressionKind::Sin, ExpressionKind::Add, ExpressionKind::Multiply},
             {ExpressionKind::Add, ExpressionKind::Multiply}}});
    const auto grammars = componentGrammars();
    const auto referenceComponents = std::array{parseExpression("sin(x0)", 2, {}, modelLimits),
                                                parseExpression("x1*x1", 2, {}, modelLimits)};
    const auto referenceModel = domain->materialize(referenceComponents);
    std::cout << "seed,method,stop,training_mse,heldout_mse,nodes,structural_recovery,"
                 "candidate_calls,row_attempts,generations\n";
    std::cout << std::setprecision(17);
    for (const auto seed : seeds) {
        auto config = configFor(training);
        RandomContext templateRandom(seed);
        const auto templateResult =
            multiComponentTemplateRegression(domain, grammars, config, templateRandom);
        bool templateStructure = false;
        if (templateResult.best)
            templateStructure =
                domain->components(templateResult.best->expression())[0].sameRepresentation(
                    referenceComponents[0]) &&
                domain->components(templateResult.best->expression())[1].sameRepresentation(
                    referenceComponents[1]);
        report(seed, "two_component_template", templateResult, *heldOut, templateStructure);

        RandomContext ordinaryRandom(seed);
        const auto ordinaryResult = symbolicRegression(problem, config, ordinaryRandom);
        report(seed, "ordinary_symbolic", ordinaryResult, *heldOut,
               ordinaryResult.best &&
                   ordinaryResult.best->expression().sameRepresentation(referenceModel));
    }
}
