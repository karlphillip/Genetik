#include <ExpressionParsing.hpp>
#include <MultiComponentTemplateRegression.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
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
    const std::array<Expression, 2> legacyExtracted = extracted;
    check(legacyExtracted[0].sameRepresentation(left) &&
              legacyExtracted[1].sameRepresentation(right),
          "existing two-site array extraction callers remain source compatible");
    auto [legacyLeftRange, legacyRightRange] = scaffold.componentRanges(materialized);
    const auto vectorRanges = scaffold.componentRangesVector(materialized);
    check(vectorRanges.size() == 2 && vectorRanges[0] == legacyLeftRange &&
              vectorRanges[1] == legacyRightRange,
          "two-site range structured binding remains compatible and vector ranges are explicit");
    check(legacyLeftRange.second > legacyLeftRange.first &&
              legacyRightRange.second > legacyRightRange.first,
          "existing two-site array range callers remain source compatible");
    auto [legacyLeft, legacyRight] = scaffold.components(materialized);
    check(legacyLeft.sameRepresentation(left) && legacyRight.sameRepresentation(right),
          "two-site component structured binding remains source compatible");
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

    const auto reversedScaffold = parseExpression("x0+x1", 2, {}, modelLimits);
    const MultiScalarExpressionTemplate reversedTemplate(
        reversedScaffold, 1, 0,
        {ExpressionLimits{15, 5, 8.0, 20}, ExpressionLimits{15, 5, 8.0, 20}}, modelLimits);
    const auto reversedRows = std::vector<RegressionRow>{
        {{0.0, 0.0}, 0.0}, {{1.0, 0.0}, 2.0}, {{0.0, 1.0}, 3.0}, {{1.0, 1.0}, 5.0}};
    const auto reversedDomain = MultiComponentTemplateProblem::create(
        reversedTemplate, RegressionProblem::create(reversedRows, modelLimits),
        std::array<std::vector<std::size_t>, 2>{{{1}, {0}}},
        std::array<std::vector<ExpressionKind>, 2>{
            {{ExpressionKind::Multiply}, {ExpressionKind::Multiply}}});
    const auto reversedModel = reversedDomain->materialize(
        {parseExpression("x1*1", 2, {}, modelLimits), parseExpression("x0*1", 2, {}, modelLimits)});
    const auto reversedParameterIndices =
        MultiComponentTemplateParameters(reversedDomain).parameters(reversedModel);
    check(reversedParameterIndices == std::vector<std::size_t>{1, 4},
          "constant indices follow model postfix order when site IDs are reversed");
    auto fittingProblem = reversedDomain->problem();
    TrainingObjective fittingObjective(fittingProblem);
    ConstantFitConfig fittingConfig;
    fittingConfig.lower = 0.0;
    fittingConfig.upper = 5.0;
    fittingConfig.initialStep = 1.0;
    fittingConfig.minimumStep = 1.0e-6;
    fittingConfig.maxSweeps = 8;
    fittingConfig.lossThreshold = 0.0;
    const auto fitted = fitSelectedConstants(reversedModel, fittingObjective,
                                             reversedParameterIndices, fittingConfig);
    const auto fittedComponents = reversedDomain->components(fitted.expression);
    check(fitted.loss.score() == 0.0 && fitted.improved &&
              fittedComponents[0].nodes()[1].constantValue() == 3.0 &&
              fittedComponents[1].nodes()[1].constantValue() == 2.0,
          "ordered selection fits constants in both reversed-position components");
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

void threeIndependentSites() {
    const ExpressionLimits modelLimits{31, 10, 8.0, 40};
    const auto source = parseExpression("x0+x1+x2", 3, {}, modelLimits);
    const std::vector<std::size_t> roots{0, 1, 3};
    const std::vector<ExpressionLimits> componentLimits{
        {9, 5, 8.0, 10}, {7, 4, 8.0, 8}, {5, 3, 8.0, 2}};
    const MultiScalarExpressionTemplate scaffold(source, roots, componentLimits, modelLimits);
    std::vector<RegressionRow> rows;
    for (double x : {-1.0, -0.5, 0.5, 1.0})
        for (double y : {-1.0, 0.0, 0.75})
            for (double z : {-0.75, 0.25, 1.0})
                rows.push_back({{x, y, z}, std::sin(x) + 3.0 * y + z * z});
    const auto domain = MultiComponentTemplateProblem::createMany(
        scaffold, RegressionProblem::create(rows, modelLimits),
        std::vector<std::vector<std::size_t>>{{0}, {1}, {2}},
        std::vector<std::vector<ExpressionKind>>{{ExpressionKind::Sin, ExpressionKind::Multiply},
                                                 {ExpressionKind::Multiply},
                                                 {ExpressionKind::Multiply}});
    const std::vector<ExpressionGrammar> grammar = [] {
        ExpressionGrammar a;
        a.operators = {ExpressionKind::Sin, ExpressionKind::Multiply};
        a.initialMaxDepth = 2;
        a.mutationMaxDepth = 2;
        a.constants = {1.0};
        ExpressionGrammar b;
        b.operators = {ExpressionKind::Multiply};
        b.initialMaxDepth = 2;
        b.mutationMaxDepth = 2;
        b.constants = {1.0};
        return std::vector<ExpressionGrammar>{a, b, b};
    }();
    const auto recovered = domain->materialize(std::vector<Expression>{
        parseExpression("sin(x0)", 3, {}, modelLimits), parseExpression("x1*3", 3, {}, modelLimits),
        parseExpression("x2*x2", 3, {}, modelLimits)});
    check(domain->problem()->measure(recovered).loss.score() == 0.0,
          "three independently recovered unknown components reproduce the target");
    const auto parts = domain->componentsVector(recovered);
    check(parts[0].toString() == "sin(x0)" && parts[1].toString() == "(x1 * 3.0)" &&
              parts[2].toString() == "(x2 * x2)",
          "component recovery is reported independently from the supplied scaffold");
    rejects(
        [&] {
            (void)domain->materialize(
                std::vector<Expression>{parseExpression("sin(x0)", 3, {}, modelLimits),
                                        parseExpression("x1*3", 3, {}, modelLimits),
                                        parseExpression("x2*(x2*(x2*x2))", 3, {}, modelLimits)});
        },
        "third component enforces its own stricter depth and work limits");

    auto context = std::make_shared<const MultiComponentTemplateContext>(domain);
    MultiComponentTemplateVariation variation(context, grammar);
    rejects(
        [&] {
            (void)MultiComponentTemplateVariation(context,
                                                  std::vector<ExpressionGrammar>{grammar[0]});
        },
        "variation rejects grammar count mismatch before indexing");
    std::mt19937 rng(73);
    auto first = variation.initialize(rng);
    auto second = variation.initialize(rng);
    auto mutated = variation.mutation(first, 1.0f, rng);
    ExpressionCandidate childA = first, childB = second;
    variation.crossover(first, second, childA, childB, 1.0f, rng);
    for (const auto* candidate : {&first, &second, &mutated, &childA, &childB})
        (void)domain->componentsVector(candidate->expression());
    check(variation.statistics().initializations == 2 &&
              variation.statistics().mutationProposals > 0 &&
              variation.statistics().crossoverProposals > 0,
          "public generation, mutation and crossover exercise all three sites");

    std::vector<RegressionRow> heldRows;
    for (double x : {-0.75, 0.25, 1.25})
        for (double y : {-0.4, 0.4, 1.1})
            for (double z : {-0.5, 0.5, 1.2})
                heldRows.push_back({{x, y, z}, std::sin(x) + 3.0 * y + z * z});
    const auto heldProblem = RegressionProblem::create(heldRows, modelLimits);
    SymbolicRegressionConfig searchConfig;
    searchConfig.population = 8;
    searchConfig.elites = 1;
    searchConfig.maxGenerationAttempts = 4;
    searchConfig.maxEvaluations = 32;
    searchConfig.maxTrainingLossAttempts = 32 * rows.size() * 20;
    searchConfig.grammar = grammar.front();
    ConstantFitConfig searchFit;
    searchFit.lower = 0.0;
    searchFit.upper = 4.0;
    searchFit.initialStep = 0.5;
    searchFit.minimumStep = 1.0e-6;
    searchFit.maxSweeps = 12;
    searchConfig.fitting = searchFit;
    constexpr std::uint64_t searchSeed = 99173;
    RandomContext searchRandom(searchSeed);
    const auto searched =
        multiComponentTemplateRegression(domain, grammar, searchConfig, searchRandom);
    check(searched.best.has_value(), "public three-site symbolic search returns a valid model");
    const auto selectedParts = domain->componentsVector(searched.best->expression());
    const auto trainingMse = domain->problem()->measure(searched.best->expression()).loss.score();
    const auto heldMse = heldProblem->measure(searched.best->expression()).loss.score();
    std::cout << "three_site_search seed=" << searchSeed << " population=8 max_evaluations=32"
              << " generation_attempts=4 max_training_loss_attempts=" << 32 * rows.size() * 20
              << " grammar=site0:{sin,multiply},site1:{multiply},site2:{multiply}"
              << " selected=" << selectedParts[0].toString() << ';' << selectedParts[1].toString()
              << ';' << selectedParts[2].toString() << " training_mse=" << trainingMse
              << " heldout_mse=" << heldMse << " candidate_calls=" << searched.candidateEvaluations
              << " row_attempts=" << searched.trainingWork.rowAttempts
              << " fit_attempts=" << searched.trainingWork.fittingLossAttempts
              << " generations=" << searched.publishedGenerations << '\n';

    const auto initial = domain->materialize(std::vector<Expression>{
        parseExpression("sin(x0)", 3, {}, modelLimits), parseExpression("x1*1", 3, {}, modelLimits),
        parseExpression("x2*x2", 3, {}, modelLimits)});
    TrainingObjective objective(domain->problem());
    ConstantFitConfig fit;
    fit.lower = 0.0;
    fit.upper = 4.0;
    fit.initialStep = 1.0;
    fit.minimumStep = 1.0e-6;
    fit.maxSweeps = 8;
    fit.lossThreshold = 0.0;
    const auto indices = MultiComponentTemplateParameters(domain).parameters(initial);
    check(indices.size() == 1, "three-site fitter selects the one learnable coefficient");
    const auto fitted = fitSelectedConstants(initial, objective, indices, fit);
    const auto fittedParts = domain->componentsVector(fitted.expression);
    check(fitted.loss.score() == 0.0 && fittedParts[1].toString() == "(x1 * 3.0)",
          "public coordinate fitting recovers the second component coefficient");
    std::cout << "three_site_recovery=" << parts[0].toString() << ';' << parts[1].toString() << ';'
              << parts[2].toString()
              << " mse=" << domain->problem()->measure(recovered).loss.score()
              << " fitted=" << fittedParts[1].toString()
              << " candidate_initializations=" << variation.statistics().initializations
              << " mutation_proposals=" << variation.statistics().mutationProposals
              << " crossover_proposals=" << variation.statistics().crossoverProposals
              << " search_candidates=" << searched.candidateEvaluations
              << " search_rows=" << searched.trainingWork.rowAttempts
              << " fit_loss_attempts=" << objective.work().lossAttempts
              << " fit_row_attempts=" << objective.work().rowAttempts << '\n';
}

void sharedNamedFunction() {
    const ExpressionLimits expandedLimits{31, 10, 20.0, 40};
    const auto body = parseExpression("2*x0", 2, {}, expandedLimits);
    const auto p = Expression::variable(0, 2, expandedLimits);
    const auto q = Expression::variable(1, 2, expandedLimits);
    const SharedLearnableFunctionTemplate shared("g", body, 0, 0, {p, q}, expandedLimits);
    const auto data =
        RegressionProblem::create({{{1.0, 0.0}, 2.0}, {{0.0, 1.0}, 6.0}}, expandedLimits);
    const auto fitted = shared.fit(*data, 0.0, 10.0);
    check(fitted.parameterCount == 1 && shared.parameterCount() == 1 && fitted.coefficient == 4.0 &&
              fitted.mse == 4.0,
          "shared calls fit one coefficient with the unique least-squares optimum a=4");
    const auto absoluteData = RegressionProblem::create(
        {{{1.0, 0.0}, 2.0}, {{0.0, 1.0}, 6.0}}, expandedLimits, RegressionLoss::MeanAbsoluteError);
    rejects([&] { (void)shared.fit(*absoluteData, 0.0, 10.0); },
            "shared coefficient fitter explicitly rejects non-MSE losses");
    check(fitted.expanded.evaluate(std::array<double, 2>{1.0, 0.0}).value() == 4.0 &&
              fitted.expanded.evaluate(std::array<double, 2>{0.0, 1.0}).value() == 4.0 &&
              fitted.expanded.evaluate(std::array<double, 2>{2.0, 3.0}).value() == 20.0,
          "both distinct call inputs share the same fitted parameter and predict 4, 4, 20");
    std::cout << "shared_function_fit name=" << shared.name()
              << " coefficients=" << fitted.coefficient
              << " parameter_count=" << fitted.parameterCount
              << " predictions=4,4,20 mse=" << fitted.mse << " analytic_row_evaluations=6\n";

    ExpressionGrammar bodyGrammar;
    bodyGrammar.operators = {ExpressionKind::Multiply};
    bodyGrammar.constants.clear();
    bodyGrammar.initialMaxDepth = 3;
    bodyGrammar.mutationMaxDepth = 3;
    bodyGrammar.growBinaryProbability = 1.0;
    std::mt19937 bodyRandom(20261126);
    const auto generated = SharedLearnableFunctionTemplate::generateBody(
        "g", 0, {p, q}, expandedLimits, bodyGrammar, bodyRandom);
    const auto changed = generated.mutateBody(bodyGrammar, bodyRandom);
    const auto generatedFitData =
        RegressionProblem::create({{{2.0, 0.0}, 4.0}, {{0.0, 3.0}, 9.0}}, expandedLimits);
    const auto generatedFit = generated.fit(*generatedFitData, 0.0, 10.0);
    const auto originalModel = generated.expanded(2.0);
    const auto changedModel = changed.expanded(2.0);
    const auto originalAtP = originalModel.evaluate(std::array<double, 2>{2.0, 0.0}).value();
    const auto originalAtQ = originalModel.evaluate(std::array<double, 2>{0.0, 3.0}).value();
    const auto atP = changedModel.evaluate(std::array<double, 2>{2.0, 0.0}).value();
    const auto atQ = changedModel.evaluate(std::array<double, 2>{0.0, 3.0}).value();
    check(changed.body().nodeCount() > 0 && atP != originalAtP && atQ != originalAtQ &&
              changed.parameterCount() == 1 && generatedFit.parameterCount == 1,
          "one genetic body mutation reaches both distinct shared call sites");

    // Fixed-seed body selection: candidates are generated and mutated by the
    // public bounded grammar API; the target body is not supplied to selection.
    std::vector<RegressionRow> bodyRows;
    for (double pv : {-1.5, -0.5, 0.5, 1.5})
        for (double qv : {-1.0, 0.25, 1.25, 2.0})
            bodyRows.push_back({{pv, qv}, 3.0 * (pv * pv + qv * qv)});
    const auto bodyProblem = RegressionProblem::create(bodyRows, expandedLimits);
    std::mt19937 selectionRandom(20261127);
    double bestBodyMse = std::numeric_limits<double>::infinity();
    double bestBodyCoefficient = 0.0;
    std::string bestBody;
    std::optional<Expression> bestBodyModel;
    std::size_t generatedBodies = 0, mutatedBodies = 0;
    for (std::size_t i = 0; i < 64; ++i) {
        auto candidate = SharedLearnableFunctionTemplate::generateBody(
            "g", 0, {p, q}, expandedLimits, bodyGrammar, selectionRandom);
        if (i % 2 == 1) {
            candidate = candidate.mutateBody(bodyGrammar, selectionRandom);
            ++mutatedBodies;
        } else {
            ++generatedBodies;
        }
        const auto fit = candidate.fit(*bodyProblem, 0.0, 4.0);
        if (fit.mse < bestBodyMse) {
            bestBodyMse = fit.mse;
            bestBodyCoefficient = fit.coefficient;
            bestBody = candidate.body().toString();
            bestBodyModel = fit.expanded;
        }
    }
    std::cout << "shared_body_selection seed=20261127 grammar={multiply}, candidates=64 generated="
              << generatedBodies << " mutated=" << mutatedBodies << " selected=" << bestBody
              << " coefficient=" << bestBodyCoefficient << " training_mse=" << bestBodyMse << '\n';
    check(bestBody == "((x0 * x0) * 1.0)" && bestBodyCoefficient == 3.0 && bestBodyMse == 0.0 &&
              bestBodyModel.has_value() &&
              bestBodyModel->evaluate(std::array<double, 2>{2.0, 0.0}).value() == 12.0 &&
              bestBodyModel->evaluate(std::array<double, 2>{0.0, 3.0}).value() == 27.0,
          "fixed-seed body selection asserts selected model, fitted coefficient and both calls");
    std::cout << "shared_body_generation seed=20261126 generated=" << generated.body().toString()
              << " mutated=" << changed.body().toString()
              << " generated_fit_a=" << generatedFit.coefficient << " before_calls=" << originalAtP
              << ',' << originalAtQ << " after_calls=" << atP << ',' << atQ << '\n';
    rejects([&] { (void)SharedLearnableFunctionTemplate("g", body, 2, 0, {p, q}, expandedLimits); },
            "shared function rejects an out-of-schema argument binding");
    rejects(
        [&] {
            const auto wrongArityInput = Expression::variable(0, 1, expandedLimits);
            (void)SharedLearnableFunctionTemplate("g", body, 0, 0, {p, wrongArityInput},
                                                  expandedLimits);
        },
        "shared function rejects mismatched call input schemas");
    rejects(
        [&] {
            const auto twoArgumentBody = parseExpression("2*(x0+x1)", 2, {}, expandedLimits);
            (void)SharedLearnableFunctionTemplate("g", twoArgumentBody, 0, 0, {p, q},
                                                  expandedLimits);
        },
        "shared function rejects an unbound second formal argument");
    for (const double scale : {1.0, std::ldexp(1.0, 600), std::ldexp(1.0, -600)}) {
        const auto scaled = RegressionProblem::create(
            {{{scale, 0.0}, scale}, {{0.0, scale}, scale}}, expandedLimits);
        const auto scaleFit = shared.fit(*scaled, 0.0, 2.0);
        check(scaleFit.parameterCount == 1 && scaleFit.coefficient == 1.0 && scaleFit.mse == 0.0,
              "scaled shared least squares preserves a=1 and zero MSE");
    }
    const double huge = std::ldexp(1.0, 1023);
    const auto hugeRows = RegressionProblem::create(
        {{{huge, huge}, huge}, {{huge / 2.0, huge / 2.0}, huge / 2.0}}, expandedLimits);
    const auto hugeFit = shared.fit(*hugeRows, 0.0, 0.5);
    check(hugeFit.coefficient == 0.5 && hugeFit.mse == 0.0,
          "finite unit-fit-bound case handles overflowing binary64 sum of call features");
    const double tiny = std::ldexp(1.0, -1000);
    const double largeTarget = std::ldexp(1.0, 40);
    const auto cancellingRows = RegressionProblem::create(
        {{{tiny, 0.0}, largeTarget}, {{-tiny, 0.0}, largeTarget}}, expandedLimits);
    const auto cancellingFit = shared.fit(*cancellingRows, -2.0, 2.0);
    check(cancellingFit.coefficient == 0.0 && cancellingFit.mse == largeTarget * largeTarget,
          "exactly cancelling normalized cross-product gives zero coefficient");
    const auto mixedScaleRows =
        RegressionProblem::create({{{tiny, 0.0}, tiny}, {{0.0, 0.0}, largeTarget}}, expandedLimits);
    const auto mixedScaleFit = shared.fit(*mixedScaleRows, 0.0, 2.0);
    check(mixedScaleFit.coefficient == 1.0,
          "tiny informative row is retained beside a much larger orthogonal target");
    const auto zeroFeature =
        RegressionProblem::create({{{0.0, 0.0}, 0.0}, {{0.0, 0.0}, 0.0}}, expandedLimits);
    rejects([&] { (void)shared.fit(*zeroFeature, 0.0, 2.0); },
            "all-zero feature control is genuinely non-identifiable");
    rejects(
        [&] {
            const auto tooSmall = ExpressionLimits{6, 10, 20.0, 40};
            (void)SharedLearnableFunctionTemplate("g", body, 0, 0, {p, q}, tooSmall);
        },
        "shared function enforces limits on its fully expanded expression");
}
} // namespace

int main() {
    try {
        structuralContracts();
        completeModelObjective();
        threeIndependentSites();
        sharedNamedFunction();
        std::cout << "multi-component template contracts passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
