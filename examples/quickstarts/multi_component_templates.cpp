#include <ExpressionParsing.hpp>
#include <MultiComponentTemplateRegression.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <random>
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

        auto [left, right] = domain->components(result.best->expression());
        const std::array<Expression, 2> components{left, right};
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

        const ExpressionLimits expandedLimits{31, 10, 20.0, 40};
        const auto sharedBody = parseExpression("2*x0", 2, {}, expandedLimits);
        const auto p = Expression::variable(0, 2, expandedLimits);
        const auto q = Expression::variable(1, 2, expandedLimits);
        const SharedLearnableFunctionTemplate shared("g", sharedBody, 0, 0, {p, q}, expandedLimits);
        const auto sharedData =
            RegressionProblem::create({{{1.0, 0.0}, 2.0}, {{0.0, 1.0}, 6.0}}, expandedLimits);
        const auto sharedFit = shared.fit(*sharedData, 0.0, 10.0);
        if (sharedFit.parameterCount != 1 || sharedFit.coefficient != 4.0 || sharedFit.mse != 4.0 ||
            sharedFit.expanded.evaluate(std::array<double, 2>{2.0, 3.0}).value() != 20.0)
            throw std::runtime_error("shared learnable function consumer contract failed");
        ExpressionGrammar sharedGrammar;
        sharedGrammar.operators = {ExpressionKind::Multiply};
        sharedGrammar.constants.clear();
        sharedGrammar.initialMaxDepth = 3;
        sharedGrammar.mutationMaxDepth = 3;
        sharedGrammar.growBinaryProbability = 1.0;
        std::mt19937 sharedRandom(20261126);
        const auto generatedShared = SharedLearnableFunctionTemplate::generateBody(
            "g", 0, {p, q}, expandedLimits, sharedGrammar, sharedRandom);
        const auto changedShared = generatedShared.mutateBody(sharedGrammar, sharedRandom);
        const auto beforeShared = generatedShared.expanded(2.0);
        const auto afterShared = changedShared.expanded(2.0);
        if (beforeShared.evaluate(std::array<double, 2>{2.0, 0.0}).value() ==
                afterShared.evaluate(std::array<double, 2>{2.0, 0.0}).value() ||
            beforeShared.evaluate(std::array<double, 2>{0.0, 3.0}).value() ==
                afterShared.evaluate(std::array<double, 2>{0.0, 3.0}).value())
            throw std::runtime_error("shared function body variation contract failed");
        std::cout << "shared_function=g(p)+g(q) coefficient=" << sharedFit.coefficient
                  << " parameter_count=" << sharedFit.parameterCount << " mse=" << sharedFit.mse
                  << " prediction(2,3)="
                  << sharedFit.expanded.evaluate(std::array<double, 2>{2.0, 3.0}).value()
                  << " generated_body=" << generatedShared.body().toString()
                  << " varied_body=" << changedShared.body().toString()
                  << " varied_prediction(2,3)="
                  << afterShared.evaluate(std::array<double, 2>{2.0, 3.0}).value() << '\n';
        const ExpressionLimits threeLimits{31, 10, 8.0, 40};
        const auto threeSource = parseExpression("x0+x1+x2", 3, {}, threeLimits);
        const MultiScalarExpressionTemplate threeScaffold(
            threeSource, std::vector<std::size_t>{0, 1, 3},
            std::vector<ExpressionLimits>{{9, 5, 8.0, 10}, {7, 4, 8.0, 8}, {5, 3, 8.0, 2}},
            threeLimits);
        const auto threeModel = threeScaffold.materialize(
            std::vector<Expression>{parseExpression("sin(x0)", 3, {}, threeLimits),
                                    parseExpression("x1*3", 3, {}, threeLimits),
                                    parseExpression("x2*x2", 3, {}, threeLimits)});
        const auto threeParts = threeScaffold.componentsVector(threeModel);
        const auto threeProblem =
            RegressionProblem::create({{{0.5, 2.0, 3.0}, std::sin(0.5) + 15.0}}, threeLimits);
        const auto threeDomain = MultiComponentTemplateProblem::createMany(
            threeScaffold, threeProblem, std::vector<std::vector<std::size_t>>{{0}, {1}, {2}},
            std::vector<std::vector<ExpressionKind>>{
                {ExpressionKind::Sin, ExpressionKind::Multiply},
                {ExpressionKind::Multiply},
                {ExpressionKind::Multiply}});
        const std::array<double, 3> threeInput{0.5, 2.0, 3.0};
        if (threeParts.size() != 3 || !threeModel.evaluate(threeInput).valid() ||
            std::abs(threeModel.evaluate(threeInput).value() - (std::sin(0.5) + 15.0)) > 1.0e-14 ||
            threeDomain->allowedVariables(2) != std::vector<std::size_t>{2})
            throw std::runtime_error("three-site installed consumer contract failed");
        std::cout << "three_site_components=" << threeParts[0].toString() << ';'
                  << threeParts[1].toString() << ';' << threeParts[2].toString()
                  << " prediction=" << threeModel.evaluate(threeInput).value() << '\n';
        return result.stoppingReason == SearchStop::ExactTrainingFit &&
                       exactHeldOut == assessment.size()
                   ? 0
                   : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
