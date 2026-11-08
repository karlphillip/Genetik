#include <ExpressionParsing.hpp>
#include <MultiComponentTemplateRegression.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    try {
        const ExpressionLimits modelLimits{31, 10, 8.0, 40};
        const auto scaffoldSource = parseExpression("x0+x1", 2, {}, modelLimits);
        const std::array<ExpressionLimits, 2> componentLimits{ExpressionLimits{15, 5, 8.0, 20},
                                                              ExpressionLimits{15, 5, 8.0, 20}};
        const MultiScalarExpressionTemplate scaffold(scaffoldSource, 0, 1, componentLimits,
                                                     modelLimits);

        std::vector<RegressionRow> training;
        for (const double x : {-1.0, -0.5, 0.0, 0.5, 1.0})
            for (const double y : {-1.0, -0.25, 0.5, 1.0})
                training.push_back({{x, y}, std::sin(x) + y * y});
        auto problem = RegressionProblem::create(training, modelLimits);
        const auto domain = MultiComponentTemplateProblem::create(
            scaffold, problem, std::array<std::vector<std::size_t>, 2>{{{0}, {1}}},
            std::array<std::vector<ExpressionKind>, 2>{
                {{ExpressionKind::Sin, ExpressionKind::Add, ExpressionKind::Multiply},
                 {ExpressionKind::Add, ExpressionKind::Multiply}}});

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

        SymbolicRegressionConfig config;
        config.population = 64;
        config.elites = 2;
        config.maxGenerationAttempts = 24;
        config.maxEvaluations = 1600;
        config.maxTrainingLossAttempts = 1600 * training.size();
        RandomContext random(142857);
        const auto result =
            multiComponentTemplateRegression(domain, {first, second}, config, random);
        if (!result.best)
            throw std::runtime_error("search returned no evaluated model");

        const auto components = domain->components(result.best->expression());
        std::size_t exactHeldOut = 0;
        const std::vector<std::array<double, 2>> assessment{
            {{-0.75, 0.75}}, {{0.25, -0.75}}, {{1.25, 0.5}}};
        for (const auto& input : assessment) {
            const double reference = std::sin(input[0]) + input[1] * input[1];
            const auto prediction = result.best->expression().evaluate(input);
            if (prediction.valid() && prediction.value() == reference)
                ++exactHeldOut;
        }
        std::cout << "component_0=" << components[0].toString() << '\n'
                  << "component_1=" << components[1].toString() << '\n'
                  << "model=" << result.best->text() << '\n'
                  << "training_mse=" << result.best->fitness() << '\n'
                  << "heldout_exact=" << exactHeldOut << '/' << assessment.size() << '\n'
                  << "candidate_evaluations=" << result.candidateEvaluations
                  << " row_attempts=" << result.trainingWork.rowAttempts
                  << " generations=" << result.publishedGenerations << '\n'
                  << "stop=" << searchStopName(result.stoppingReason) << '\n';
        return result.stoppingReason == SearchStop::ExactTrainingFit &&
                       exactHeldOut == assessment.size()
                   ? 0
                   : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
