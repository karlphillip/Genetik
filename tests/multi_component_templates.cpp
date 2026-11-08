#include <ExpressionParsing.hpp>
#include <MultiComponentTemplateRegression.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

template <class Function> void rejects(Function&& function, const char* message) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(message);
}

ExpressionLimits limits() {
    return {31, 10, 8.0, 40};
}

std::array<ExpressionGrammar, 2> grammars() {
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

std::shared_ptr<const MultiComponentTemplateProblem>
makeDomain(const std::vector<RegressionRow>& rows) {
    const auto modelLimits = limits();
    const auto source = parseExpression("x0+x1", 2, {}, modelLimits);
    const std::array<ExpressionLimits, 2> componentLimits{ExpressionLimits{15, 5, 8.0, 20},
                                                          ExpressionLimits{15, 5, 8.0, 20}};
    const MultiScalarExpressionTemplate scaffold(source, 0, 1, componentLimits, modelLimits);
    return MultiComponentTemplateProblem::create(
        scaffold, RegressionProblem::create(rows, modelLimits),
        std::array<std::vector<std::size_t>, 2>{{{0}, {1}}},
        std::array<std::vector<ExpressionKind>, 2>{
            {{ExpressionKind::Sin, ExpressionKind::Add, ExpressionKind::Multiply},
             {ExpressionKind::Add, ExpressionKind::Multiply}}});
}

std::vector<RegressionRow> observations() {
    std::vector<RegressionRow> rows;
    for (const double x : {-1.0, -0.5, 0.0, 0.5, 1.0})
        for (const double y : {-1.0, -0.25, 0.5, 1.0})
            rows.push_back({{x, y}, std::sin(x) + y * y});
    return rows;
}

void structuralContracts() {
    const auto modelLimits = limits();
    const auto source = parseExpression("x0-x0", 2, {}, modelLimits);
    const MultiScalarExpressionTemplate scaffold(
        source, 0, 1, {ExpressionLimits{7, 4, 8.0, 10}, ExpressionLimits{7, 4, 8.0, 10}},
        modelLimits);
    const auto left = parseExpression("x0", 2, {}, modelLimits);
    const auto right = parseExpression("x1", 2, {}, modelLimits);
    const auto materialized = scaffold.materialize({left, right});
    const auto extracted = scaffold.components(materialized);
    check(extracted[0].sameRepresentation(left) && extracted[1].sameRepresentation(right),
          "independent site identities survive replacement and extraction");
    rejects(
        [&] {
            (void)MultiScalarExpressionTemplate(
                parseExpression("x0+x1", 2, {}, modelLimits), 2, 0,
                {ExpressionLimits{7, 4, 8.0, 10}, ExpressionLimits{7, 4, 8.0, 10}}, modelLimits);
        },
        "ancestor and descendant sites must be rejected");
    rejects([&] { (void)scaffold.components(parseExpression("x0+x1", 2, {}, modelLimits)); },
            "changed frozen operator must be rejected");
    rejects(
        [&] {
            (void)MultiScalarExpressionTemplate(
                source, 0, 1, {ExpressionLimits{7, 4, 8.0, 10}, ExpressionLimits{7, 4, 8.0, 10}},
                modelLimits, 1);
        },
        "source matching workspace must respect its byte estimate");

    const auto shallowModel = ExpressionLimits{31, 3, 8.0, 4};
    const auto shallowSource = parseExpression("x0+x1", 2, {}, shallowModel);
    const MultiScalarExpressionTemplate bounded(
        shallowSource, 0, 1, {ExpressionLimits{15, 3, 8.0, 8}, ExpressionLimits{15, 3, 8.0, 8}},
        shallowModel);
    rejects(
        [&] {
            (void)bounded.materialize({parseExpression("sin(sin(x0))", 2, {}, {15, 3, 8.0, 8}),
                                       parseExpression("sin(sin(x1))", 2, {}, {15, 3, 8.0, 8})});
        },
        "complete model depth and arithmetic limits apply across both sites");

    const auto domain = makeDomain(observations());
    rejects([&] { (void)domain->materialize({parseExpression("x1", 2, {}, modelLimits), right}); },
            "component-specific variable restrictions must be enforced");
    rejects(
        [&] {
            (void)domain->materialize({parseExpression("x0", 2, {}, modelLimits),
                                       parseExpression("sin(x1)", 2, {}, modelLimits)});
        },
        "component-specific operator restrictions must be enforced");
    rejects(
        [&] {
            (void)domain->materialize(
                {parseExpression("sin(sin(sin(sin(sin(x0)))))", 2, {}, modelLimits), right});
        },
        "individual component depth limit must be enforced");

    const ExpressionLimits workModelLimits{31, 10, 8.0, 5};
    const auto workSource = parseExpression("x0+x1", 2, {}, workModelLimits);
    const MultiScalarExpressionTemplate workScaffold(
        workSource, 0, 1, {ExpressionLimits{15, 8, 8.0, 1}, ExpressionLimits{15, 8, 8.0, 1}},
        workModelLimits);
    const auto workDomain = MultiComponentTemplateProblem::create(
        workScaffold, RegressionProblem::create(observations(), workModelLimits),
        std::array<std::vector<std::size_t>, 2>{{{0}, {1}}},
        std::array<std::vector<ExpressionKind>, 2>{
            {{ExpressionKind::Sin, ExpressionKind::Add, ExpressionKind::Multiply},
             {ExpressionKind::Add, ExpressionKind::Multiply}}});
    rejects(
        [&] {
            (void)workDomain->materialize(
                {parseExpression("x0+x0+x0", 2, {}, {31, 10, 8.0, 10}), right});
        },
        "individual arithmetic-work limits must be enforced");

    const auto parameterModel =
        domain->materialize({parseExpression("sin(2*x0)", 2, {}, modelLimits),
                             parseExpression("x1*3", 2, {}, modelLimits)});
    const auto parameterIndices =
        MultiComponentTemplateParameters(domain).parameters(parameterModel);
    check(parameterIndices == std::vector<std::size_t>{0, 5},
          "constant fitting selects parameters from both sites in full-model coordinates");
    rejects(
        [&] {
            const ExpressionLimits tightLimits{31, 3, 8.0, 40};
            const auto tightSource = parseExpression("x0+x1", 2, {}, tightLimits);
            const MultiScalarExpressionTemplate tightScaffold(
                tightSource, 0, 1,
                {ExpressionLimits{15, 3, 8.0, 20}, ExpressionLimits{15, 3, 8.0, 20}}, tightLimits);
            const auto tightDomain = MultiComponentTemplateProblem::create(
                tightScaffold, RegressionProblem::create(observations(), tightLimits),
                std::array<std::vector<std::size_t>, 2>{{{0}, {1}}},
                std::array<std::vector<ExpressionKind>, 2>{
                    {{ExpressionKind::Sin, ExpressionKind::Add, ExpressionKind::Multiply},
                     {ExpressionKind::Add, ExpressionKind::Multiply}}});
            const auto oversized = parseExpression("sin(sin(x0))+x1", 2, {}, limits());
            (void)tightDomain->components(oversized);
        },
        "caller-constructed models must meet the complete model limits");

    auto context = std::make_shared<const MultiComponentTemplateContext>(domain);
    MultiComponentTemplateVariation variation(context, grammars());
    std::mt19937 random(142857);
    const auto initial = variation.initialize(random);
    const auto initialParts = domain->components(initial.expression());
    check(initialParts[0].variableCount() == 2 && initialParts[1].variableCount() == 2,
          "component generators retain the full input schema");
    domain->validateComponent(0, initialParts[0]);
    domain->validateComponent(1, initialParts[1]);
    check(variation.statistics().initializations == 1,
          "initialization accounting includes one complete candidate");
}

void completeModelObjective() {
    const auto rows = observations();
    const auto domain = makeDomain(rows);
    const auto modelLimits = limits();
    const auto sine = parseExpression("sin(x0)", 2, {}, modelLimits);
    const auto square = parseExpression("x1*x1", 2, {}, modelLimits);
    const auto knownModel = domain->materialize({sine, square});
    const auto completeProblem = domain->problem();
    check(completeProblem->measure(knownModel).loss.score() == 0.0,
          "full scaffold objective evaluates both unknown components jointly");

    SymbolicRegressionConfig config;
    config.population = 64;
    config.elites = 2;
    config.maxGenerationAttempts = 24;
    config.maxEvaluations = 64 * 25;
    config.maxTrainingLossAttempts = 64 * 25 * rows.size();
    config.grammar = grammars()[0];
    RandomContext random(142857);
    const auto result = multiComponentTemplateRegression(domain, grammars(), config, random);
    check(result.best.has_value(), "fixed-seed discovery returns an evaluated model");
    check(domain->components(result.best->expression())[0].variableCount() == 2,
          "selected first component retains variable schema");
    check(domain->components(result.best->expression())[1].variableCount() == 2,
          "selected second component retains variable schema");
    if (result.stoppingReason == SearchStop::ExactTrainingFit)
        check(result.best->fitness() == 0.0, "exact stop reports zero complete-model loss");
}
} // namespace

int main() {
    try {
        structuralContracts();
        completeModelObjective();
        std::cout << "multi-component template contracts passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
