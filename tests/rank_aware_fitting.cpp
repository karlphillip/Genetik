#include <ScalarMathematicsRun.hpp>
#include <array>
#include <cmath>
#include <iostream>

static void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
using K = ExpressionKind;
static Expression c(double v) {
    return Expression::constant(v, 1);
}
static Expression v() {
    return Expression::variable(0);
}
static Expression bin(K k, const Expression& a, const Expression& b) {
    return Expression::binary(k, a, b);
}
static std::vector<RegressionRow> rows(std::span<const double> xs) {
    std::vector<RegressionRow> out;
    for (double x : xs)
        out.push_back({{x}, 2 * x + 3});
    return out;
}
static std::vector<RegressionRow> heldout() {
    const std::array xs{-.5, 2.5, 4.};
    return rows(xs);
}
static double score(const RegressionProblem& p, const Expression& e) {
    return p.measure(e).loss.score();
}
static const char* lmStop(LMStop s) {
    switch (s) {
    case LMStop::GradientTolerance:
        return "gradient_tolerance";
    case LMStop::StepTolerance:
        return "step_tolerance";
    case LMStop::LossTolerance:
        return "loss_tolerance";
    case LMStop::IterationLimit:
        return "iteration_limit";
    case LMStop::EvaluationLimit:
        return "evaluation_limit";
    case LMStop::DerivativeLimit:
        return "derivative_limit";
    case LMStop::WorkspaceLimit:
        return "workspace_limit";
    case LMStop::SingularSystem:
        return "singular_system";
    case LMStop::InvalidStart:
        return "invalid_start";
    case LMStop::DampingLimit:
        return "damping_limit";
    case LMStop::Cancelled:
        return "cancelled";
    }
    return "unknown";
}
static Expression redundant() {
    return bin(K::Add, bin(K::Multiply, bin(K::Add, c(.5), c(.5)), v()), c(0));
}
static Expression fullRank() {
    return bin(K::Add, bin(K::Multiply, c(1), v()), c(0));
}
static std::vector<RegressionRow> affineRows(std::span<const double> xs) {
    std::vector<RegressionRow> out;
    for (const double x : xs)
        out.push_back({{x}, 2 * x + 3});
    return out;
}
static ConstantFitConfig lm(bool rank, std::uint64_t cap = 512) {
    ConstantFitConfig f;
    f.method = ConstantFitMethod::LevenbergMarquardt;
    f.lower = -10;
    f.upper = 10;
    f.maxLossAttempts = cap;
    f.rankAware = rank;
    f.rankTolerance = 1e-10;
    f.levenbergMarquardt.maxIterations = 64;
    f.levenbergMarquardt.maxObjectiveEvaluations = 128;
    f.levenbergMarquardt.maxJacobianEvaluations = 128;
    f.levenbergMarquardt.maxMathematicalOperations = 1000000;
    f.levenbergMarquardt.lossTolerance = 1e-24;
    return f;
}
static void directFixtures() {
    const std::array trainX{-3., -2., -1., 0., 1., 2., 3.};
    auto train = RegressionProblem::create(rows(trainX), {64, 12, 32});
    auto test = RegressionProblem::create(heldout(), {64, 12, 32});
    const auto start = redundant();

    // The default LM path still publishes a valid first score when the
    // evaluator exhausts its arithmetic allowance on that measured point.
    auto defaultTightOps = lm(false);
    defaultTightOps.levenbergMarquardt.maxMathematicalOperations = 20;
    TrainingObjective defaultTightObjective(train, 1000);
    const auto defaultTight = fitConstants(start, defaultTightObjective, defaultTightOps);
    check(defaultTight.stop == ConstantFitStop::EvaluationLimit &&
              defaultTight.loss.state() == FitnessState::Valid &&
              defaultTight.startingLoss.score() == 13 && defaultTight.loss.score() == 13 &&
              defaultTight.optimizerWork.mathematicalOperations == 21 &&
              defaultTightObjective.work().lossAttempts == 1,
          "default LM retains valid baseline score on arithmetic exhaustion");
    std::cout << "fixture=default_arithmetic_exhaustion cap=20 stop="
              << constantFitStopName(defaultTight.stop)
              << " start=" << defaultTight.startingLoss.score()
              << " returned=" << defaultTight.loss.score()
              << " operations=" << defaultTight.optimizerWork.mathematicalOperations << '\n';

    TrainingObjective baselineObjective(train, 1000), rankObjective(train, 1000);
    const auto baseline = fitConstants(start, baselineObjective, lm(false));
    const auto rank = fitConstants(start, rankObjective, lm(true));
    check(baseline.loss.state() == FitnessState::Valid && rank.loss.state() == FitnessState::Valid,
          "valid direct fits");
    check(baseline.rankSelectionStop == "disabled" &&
              baseline.rankSelectionWork == RankSelectionWork{},
          "default LM preserves the preexisting non-rank-aware path");
    check(baseline.optimizerStop == LMStop::SingularSystem,
          "baseline detects redundant Jacobian directions as singular");
    check(rank.selectedDirections.size() == 2 && rank.selectedDirections[0] == 0 &&
              rank.selectedDirections[1] == 5,
          "pivot tie selects first coefficient occurrence then intercept");
    const auto n = rank.expression.nodes();
    check(std::abs(n[0].constantValue() + n[1].constantValue() - 2) < 1e-8,
          "identifiable redundant coefficient sum");
    check(std::abs(n[5].constantValue() - 3) < 1e-8, "redundant intercept recovery");
    check(rank.loss.score() < 1e-18 && score(*test, rank.expression) < 1e-18,
          "redundant held-out prediction recovery");
    check(rank.rankSelectionWork.derivativeEvaluations == 21 && rank.totalBudgetUnits <= 512,
          "selector and fit share the local work cap");
    check(rank.preSelectionBaselineOperations + rank.rankSelectionWork.derivativeOperations +
                  rank.rankSelectionWork.primalOperations +
                  rank.rankSelectionWork.linearAlgebraOperations +
                  rank.optimizerWork.mathematicalOperations <=
              lm(true).levenbergMarquardt.maxMathematicalOperations,
          "rank selection and LM share the mathematical-operation cap");
    check(rank.totalBudgetUnits ==
              rankObjective.work().lossAttempts + rank.rankSelectionWork.derivativeEvaluations,
          "combined work includes selection and all fit calls");
    check(rank.preSelectionBaselineOperations > 0 &&
              rank.preSelectionBaselineOperations + rank.rankSelectionWork.derivativeOperations +
                      rank.rankSelectionWork.primalOperations +
                      rank.rankSelectionWork.linearAlgebraOperations +
                      rank.optimizerWork.mathematicalOperations <=
                  lm(true).levenbergMarquardt.maxMathematicalOperations,
          "successful result retains baseline and selector operation counters");
    TrainingObjective selectedObjective(train, 1000);
    const std::array selectedNodes{std::size_t{0}, std::size_t{1}, std::size_t{5}};
    const auto selectedFit =
        fitSelectedConstants(start, selectedObjective, selectedNodes, lm(true));
    check(selectedFit.selectedDirections == rank.selectedDirections &&
              selectedFit.loss.score() < 1e-18,
          "public selected-constant API uses rank-aware LM");
    std::cout << "fixture=supplied_redundant start_train=" << baseline.startingLoss.score()
              << " baseline_train=" << baseline.loss.score() << " rank_train=" << rank.loss.score()
              << " baseline_held=" << score(*test, baseline.expression)
              << " rank_held=" << score(*test, rank.expression)
              << " selected=" << rank.selectedDirections[0] << ',' << rank.selectedDirections[1]
              << " fitted=" << rank.expression.nodes()[0].constantValue() << ','
              << rank.expression.nodes()[1].constantValue() << ','
              << rank.expression.nodes()[5].constantValue()
              << " selection_work=" << rank.rankSelectionWork.derivativeEvaluations
              << " loss_calls=" << rank.lossAttempts
              << " lm_objective_evaluations=" << rank.optimizerWork.objectiveEvaluations
              << " total_work=" << rank.totalBudgetUnits
              << " baseline_stop=" << lmStop(*baseline.optimizerStop)
              << " rank_stop=" << lmStop(*rank.optimizerStop) << '\n';

    TrainingObjective fullBaselineObjective(train, 1000), fullRankObjective(train, 1000);
    const auto fullStart = fullRank();
    const auto fullBaseline = fitConstants(fullStart, fullBaselineObjective, lm(false));
    const auto full = fitConstants(fullStart, fullRankObjective, lm(true));
    check(full.selectedDirections.size() == 2, "full-rank control keeps both directions");
    check(full.loss.score() < 1e-18 && score(*test, full.expression) < 1e-18,
          "full-rank train and held-out control");
    check(std::abs(full.expression.nodes()[0].constantValue() - 2) < 1e-8 &&
              std::abs(full.expression.nodes()[3].constantValue() - 3) < 1e-8,
          "full-rank coefficients");
    std::cout << "fixture=supplied_full_rank baseline_start=" << fullBaseline.startingLoss.score()
              << " baseline_train=" << fullBaseline.loss.score()
              << " rank_train=" << full.loss.score()
              << " baseline_held=" << score(*test, fullBaseline.expression)
              << " rank_held=" << score(*test, full.expression)
              << " baseline_lm_objective_evaluations="
              << fullBaseline.optimizerWork.objectiveEvaluations
              << " baseline_loss_calls=" << fullBaseline.lossAttempts
              << " selected=" << full.selectedDirections[0] << ',' << full.selectedDirections[1]
              << " selection_work=" << full.rankSelectionWork.derivativeEvaluations
              << " loss_calls=" << full.lossAttempts
              << " lm_objective_evaluations=" << full.optimizerWork.objectiveEvaluations
              << " total_work=" << full.totalBudgetUnits
              << " baseline_stop=" << lmStop(*fullBaseline.optimizerStop)
              << " rank_stop=" << lmStop(*full.optimizerStop) << '\n';

    TrainingObjective tightBaselineObjective(train, 1000), tightObjective(train, 1000);
    const auto tightBaseline = fitConstants(start, tightBaselineObjective, lm(false, 8));
    const auto tight = fitConstants(start, tightObjective, lm(true, 8));
    check(tightBaseline.loss.score() == tight.loss.score() && tightBaseline.totalBudgetUnits <= 8,
          "tight baseline and opt-in use the same total cap and start");
    check(tight.loss.state() == FitnessState::Valid && tight.loss.score() == score(*train, start),
          "tight budget retains the valid starting incumbent");
    check(tight.stop == ConstantFitStop::EvaluationLimit && tight.selectedDirections.empty(),
          "tight rank-selection stop is explicit");
    std::cout << "fixture=supplied_tight_budget baseline_train=" << tightBaseline.loss.score()
              << " rank_train=" << tight.loss.score()
              << " baseline_total_work=" << tightBaseline.totalBudgetUnits
              << " selection_work=" << tight.rankSelectionWork.derivativeEvaluations
              << " total_work=" << tight.totalBudgetUnits
              << " baseline_stop=" << lmStop(*tightBaseline.optimizerStop)
              << " rank_stop=" << constantFitStopName(tight.stop) << '\n';

    TrainingObjective baselineGloballyTightObjective(train, 10), globallyTightObjective(train, 10);
    const auto baselineGloballyTight =
        fitConstants(start, baselineGloballyTightObjective, lm(false, 512));
    const auto globallyTight = fitConstants(start, globallyTightObjective, lm(true, 512));
    check(baselineGloballyTight.loss.score() == globallyTight.loss.score() &&
              baselineGloballyTight.totalBudgetUnits <= 10,
          "global tight comparison uses identical starts and caps");
    check(globallyTight.stop == ConstantFitStop::EvaluationLimit &&
              globallyTight.loss.score() == score(*train, start),
          "global allowance preserves the valid incumbent");
    check(globallyTightObjective.remaining() == 0 &&
              globallyTight.rankSelectionWork.derivativeEvaluations == 9,
          "rank selector spends the shared global allowance");
    check(globallyTight.totalBudgetUnits == 10 && globallyTight.lossAttempts == 1,
          "global cap reports selector units separately from objective calls");

    ExecutionContext exhaustedExecution({2});
    auto exhaustedChild = globallyTightObjective.forkParallel(exhaustedExecution);
    check(globallyTightObjective.remaining() == 0 && exhaustedChild->remaining() == 0,
          "parallel fork carries serial selector units into shared admission");
    bool parentForkMeasureDenied = false, childForkMeasureDenied = false;
    try {
        (void)globallyTightObjective.measure(start);
    } catch (const TrainingLossBudgetExceeded&) {
        parentForkMeasureDenied = true;
    }
    try {
        (void)exhaustedChild->measure(start);
    } catch (const TrainingLossBudgetExceeded&) {
        childForkMeasureDenied = true;
    }
    check(parentForkMeasureDenied && childForkMeasureDenied &&
              globallyTightObjective.remaining() == 0 && exhaustedChild->remaining() == 0,
          "neither side of exhausted fork can measure");

    bool deniedMeasure = false;
    const auto beforeDeniedMeasure = globallyTightObjective.work();
    try {
        (void)globallyTightObjective.measure(start);
    } catch (const TrainingLossBudgetExceeded&) {
        deniedMeasure = true;
    }
    check(deniedMeasure && globallyTightObjective.work() == beforeDeniedMeasure &&
              globallyTightObjective.remaining() == 0,
          "exhausted shared allowance rejects measure before doing work");
    const auto reused = fitConstants(start, globallyTightObjective, lm(true, 512));
    check(reused.stop == ConstantFitStop::EvaluationLimit && reused.lossAttempts == 0 &&
              reused.totalBudgetUnits == 0 &&
              globallyTightObjective.work() == beforeDeniedMeasure &&
              globallyTightObjective.remaining() == 0,
          "exhausted shared allowance rejects another fit without work");
    std::cout << "fixture=global_tight_budget baseline_train=" << baselineGloballyTight.loss.score()
              << " rank_train=" << globallyTight.loss.score()
              << " baseline_total_work=" << baselineGloballyTight.totalBudgetUnits
              << " selection_work=" << globallyTight.rankSelectionWork.derivativeEvaluations
              << " fit_loss_calls=" << globallyTightObjective.work().lossAttempts
              << " total_work=" << globallyTight.totalBudgetUnits
              << " remaining=" << globallyTightObjective.remaining()
              << " baseline_stop=" << lmStop(*baselineGloballyTight.optimizerStop)
              << " rank_stop=" << constantFitStopName(globallyTight.stop) << '\n';

    // A partially spent serial objective retains only its genuine residual
    // allowance after it is converted to shared parent/worker admission.
    TrainingObjective partiallySpent(train, 40);
    const auto partialMeasured = partiallySpent.measure(start);
    (void)partialMeasured;
    ExecutionContext partiallySpentExecution({2});
    auto partiallySpentChild = partiallySpent.forkParallel(partiallySpentExecution);
    check(partiallySpent.remaining() == 39 && partiallySpentChild->remaining() == 39,
          "fork preserves legitimate unspent objective units");
    (void)partiallySpentChild->measure(start);
    check(partiallySpent.remaining() == 38 && partiallySpentChild->remaining() == 38,
          "parent and child consume the same remaining allowance");

    // The redundant affine topology has deterministic arithmetic stop points:
    // the smaller cap stops before any improving trial; the larger cap accepts
    // the first improvement, then stops while rebuilding its Jacobian.
    for (const auto& [operationCap, expectImprovement] :
         {std::pair<std::size_t, bool>{520, false}, {650, true}}) {
        auto operationConfig = lm(true, 512);
        operationConfig.levenbergMarquardt.maxMathematicalOperations = operationCap;
        TrainingObjective operationObjective(train, 1000);
        const auto fitted = fitConstants(start, operationObjective, operationConfig);
        const auto selectorOperations = fitted.preSelectionBaselineOperations +
                                        fitted.rankSelectionWork.derivativeOperations +
                                        fitted.rankSelectionWork.primalOperations +
                                        fitted.rankSelectionWork.linearAlgebraOperations;
        check(fitted.stop == ConstantFitStop::EvaluationLimit &&
                  fitted.optimizerStop == LMStop::EvaluationLimit &&
                  fitted.loss.state() == FitnessState::Valid &&
                  fitted.improved == expectImprovement &&
                  (expectImprovement ? fitted.loss.score() < 13 : fitted.loss.score() == 13) &&
                  fitted.optimizerWork.mathematicalOperations + selectorOperations == operationCap,
              "arithmetic exhaustion propagates through rank-aware LM with incumbent and work");
        std::cout << "fixture=rank_arithmetic_exhaustion cap=" << operationCap
                  << " stop=" << constantFitStopName(fitted.stop)
                  << " native_stop=" << lmStop(*fitted.optimizerStop)
                  << " start=" << fitted.startingLoss.score() << " returned=" << fitted.loss.score()
                  << " improved=" << fitted.improved
                  << " operations=" << fitted.optimizerWork.mathematicalOperations
                  << " selector_operations=" << selectorOperations << '\n';
    }

    auto limitedOps = lm(true, 512);
    limitedOps.levenbergMarquardt.maxMathematicalOperations = 100;
    TrainingObjective operationLimitedObjective(train, 1000);
    const auto operationLimited = fitConstants(start, operationLimitedObjective, limitedOps);
    const auto admittedOps = operationLimited.preSelectionBaselineOperations +
                             operationLimited.rankSelectionWork.derivativeOperations +
                             operationLimited.rankSelectionWork.primalOperations +
                             operationLimited.rankSelectionWork.linearAlgebraOperations +
                             operationLimited.optimizerWork.mathematicalOperations;
    check(operationLimited.stop == ConstantFitStop::EvaluationLimit && admittedOps <= 100,
          "100-operation limit stops before spending beyond the shared cap");
    std::cout << "fixture=operation_cap cap=100 spent=" << admittedOps
              << " stop=" << constantFitStopName(operationLimited.stop) << '\n';
}

static void actualStartAndWeightedJacobians() {
    const std::array trainX{-2., -1., 0., 1., 2.};
    const std::array heldX{-.5, 2.5};
    std::vector<RegressionRow> productRows, productHeldRows;
    for (const double x : trainX)
        productRows.push_back({{x}, 12 * x});
    for (const double x : heldX)
        productHeldRows.push_back({{x}, 12 * x});
    const auto productTrain = RegressionProblem::create(productRows, {64, 12, 32});
    const auto productHeld = RegressionProblem::create(productHeldRows, {64, 12, 32});
    const auto product = bin(K::Multiply, bin(K::Multiply, c(2), c(3)), v());
    TrainingObjective productObjective(productTrain, 512);
    const auto productFit = fitConstants(product, productObjective, lm(true));
    check(productFit.selectedDirections.size() == 1 && productFit.selectedDirections[0] == 0,
          "nonlinear selector differentiates at actual coefficients (2,3)");
    check(productFit.loss.score() < 1e-18 && score(*productHeld, productFit.expression) < 1e-18,
          "one useful product direction fits 12*x");
    std::cout << "fixture=actual_coefficient_product selected=" << productFit.selectedDirections[0]
              << " train=" << productFit.loss.score()
              << " held=" << score(*productHeld, productFit.expression) << '\n';

    const std::vector<RegressionRow> weightedRows{{{1.0}, 4.0}, {{3.0}, 0.0}};
    RegressionObjectiveConfig weighted;
    weighted.loss = RegressionLoss::WeightedMeanSquaredError;
    weighted.observationWeights = {1.0, 0.0};
    const auto weightedProblem =
        RegressionProblem::createWithObjective(weightedRows, weighted, {64, 12, 32});
    const auto affine = bin(K::Add, bin(K::Multiply, c(0), v()), c(0));
    TrainingObjective weightedObjective(weightedProblem, 512);
    const auto weightedFit = fitConstants(affine, weightedObjective, lm(true));
    check(weightedFit.selectedDirections.size() == 1 && weightedFit.selectedDirections[0] == 0 &&
              weightedFit.loss.score() < 1e-18,
          "weighted selector ignores the zero-weight Jacobian row and fits the active row");
    check(std::abs(weightedFit.expression.nodes()[0].constantValue() - 4) < 1e-8,
          "weighted active-row coefficient recovery");
    const auto invalidZeroWeight = bin(K::Divide, c(1), bin(K::Subtract, v(), c(3)));
    TrainingObjective invalidWeightedObjective(weightedProblem, 512);
    const auto invalidWeightedFit =
        fitConstants(invalidZeroWeight, invalidWeightedObjective, lm(true));
    check(invalidWeightedFit.stop == ConstantFitStop::InvalidStart &&
              invalidWeightedFit.loss.state() == FitnessState::Invalid,
          "zero-weight rows still require valid predictions");
    std::cout << "fixture=weighted_affine selected=" << weightedFit.selectedDirections[0]
              << " coefficient=" << weightedFit.expression.nodes()[0].constantValue()
              << " train=" << weightedFit.loss.score() << '\n';

    // On these small inputs the intercept column pivots first (postfix index 3),
    // while LM requires selected constant indices in increasing postfix order.
    const std::array smallX{-.25, 0., .25};
    const std::array smallHeldX{-.125, .125};
    const auto smallTrain = RegressionProblem::create(affineRows(smallX), {64, 12, 32});
    const auto smallHeld = RegressionProblem::create(affineRows(smallHeldX), {64, 12, 32});
    TrainingObjective smallObjective(smallTrain, 512);
    const auto smallFit = fitConstants(fullRank(), smallObjective, lm(true));
    check(smallFit.selectedDirections == std::vector<std::size_t>({0, 3}) &&
              std::abs(smallFit.expression.nodes()[0].constantValue() - 2) < 1e-8 &&
              std::abs(smallFit.expression.nodes()[3].constantValue() - 3) < 1e-8,
          "pivot ordering is converted to increasing postfix LM order");
    check(smallFit.loss.score() < 1e-18 && score(*smallHeld, smallFit.expression) < 1e-18,
          "intercept-first pivot fit predicts held-out inputs");
    std::cout << "fixture=intercept_first selected=" << smallFit.selectedDirections[0] << ','
              << smallFit.selectedDirections[1]
              << " a=" << smallFit.expression.nodes()[0].constantValue()
              << " c=" << smallFit.expression.nodes()[3].constantValue()
              << " train=" << smallFit.loss.score()
              << " held=" << score(*smallHeld, smallFit.expression) << '\n';
}
static void ordinarySearch() {
    const std::array trainX{-2., -1., 0., 1., 2.};
    auto problem = RegressionProblem::create(rows(trainX), {32, 8, 8});
    auto config = SymbolicRegressionConfig{};
    config.population = 10;
    config.elites = 1;
    config.maxGenerationAttempts = 4;
    config.maxEvaluations = 50;
    config.maxTrainingLossAttempts = 50000;
    config.grammar.operators = {K::Add, K::Multiply};
    config.grammar.continuousProbability = 1;
    config.grammar.continuousLower = -2;
    config.grammar.continuousUpper = 2;
    config.grammar.initialMaxDepth = 2;
    config.grammar.mutationMaxDepth = 2;
    config.grammar.proposalLimit = 16;
    config.fitting = lm(true, 512);
    config.fitting->lower = -8;
    config.fitting->upper = 8;
    config.trainingLossThreshold = 0;
    RandomContext random(20261127);
    const auto found = symbolicRegression(problem, config, random);
    RandomContext repeatRandom(20261127);
    const auto repeated = symbolicRegression(problem, config, repeatRandom);
    check(found.best && found.best->fitnessState() == FitnessState::Valid,
          "ordinary symbolic search returns a valid model");
    check(repeated.best && repeated.best->text() == found.best->text() &&
              repeated.trainingWork == found.trainingWork &&
              repeated.rankSelectionWork == found.rankSelectionWork,
          "fixed-seed generated search is deterministic");
    check(found.rawInitialExpressions.size() == config.population &&
              found.candidateEvaluations >= config.population,
          "candidates came from the declared grammar search");
    check(found.rankSelectionWork.derivativeEvaluations > 0 &&
              found.rankSelectionWork.selectedDirections > 0,
          "ordinary search ran rank selection and retained independent directions");
    check(found.trainingWork.lossAttempts + found.rankSelectionWork.derivativeEvaluations <=
              config.maxTrainingLossAttempts,
          "search selector and fit obey one global cap");
    const auto test = RegressionProblem::create(heldout(), {32, 8, 8});
    auto baseConfig = config;
    baseConfig.fitting = lm(false, 512);
    baseConfig.fitting->lower = -8;
    baseConfig.fitting->upper = 8;
    RandomContext baseRandom(20261127);
    const auto base = symbolicRegression(problem, baseConfig, baseRandom);
    check(base.best && base.best->text() == "(x0 + 2.9999999999292117)" &&
              base.candidateEvaluations == 50 && base.trainingWork.lossAttempts == 294 &&
              std::abs(base.best->fitness() - 2) < 1e-8 &&
              std::abs(score(*test, base.best->expression()) - 7.5) < 1e-8,
          "fixed-seed disabled-selector arm matches the measured main-base replay");
    std::cout << "fixture=ordinary_generated_search_base seed=20261127 candidates="
              << base.candidateEvaluations << " training=" << base.best->fitness()
              << " heldout=" << score(*test, base.best->expression())
              << " objective_calls=" << base.trainingWork.lossAttempts
              << " equation=" << base.best->text() << '\n';
    std::cout << "fixture=ordinary_generated_search_corrected seed=20261127 candidates="
              << found.candidateEvaluations << " training=" << found.best->fitness()
              << " heldout=" << score(*test, found.best->expression())
              << " selected_direction_count=" << found.rankSelectionWork.selectedDirections
              << " selected_derivative_evaluations="
              << found.rankSelectionWork.derivativeEvaluations
              << " selected_rows=" << found.rankSelectionWork.rows
              << " loss_calls=" << found.trainingWork.lossAttempts
              << " fit_loss_calls=" << found.trainingWork.fittingLossAttempts
              << " objective_calls=" << found.trainingWork.lossAttempts
              << " lm_objective_evaluations=" << found.optimizerWork.objectiveEvaluations
              << " work_total="
              << found.trainingWork.lossAttempts + found.rankSelectionWork.derivativeEvaluations
              << " stop=" << searchStopName(found.stoppingReason)
              << " equation=" << found.best->text() << '\n';
}
static void rankAwareFallback() {
    const std::array trainX{-3., -2., -1., 0., 1., 2., 3.};
    auto train = RegressionProblem::create(rows(trainX), {64, 12, 32});
    auto test = RegressionProblem::create(heldout(), {64, 12, 32});

    auto fallbackConfig = lm(false);
    fallbackConfig.rankAwareFallback = true;
    auto refinementObjective = std::make_shared<TrainingObjective>(train, 1000);
    ConstantRefinement refinement(refinementObjective, fallbackConfig);
    check(!refinement.parallelEvaluationSafe(),
          "fallback preparation declares the existing rank-selector serial restriction");
    bool refinementForkRejected = false;
    try {
        ExecutionContext execution({2});
        (void)refinement.forkParallel(execution);
    } catch (const std::invalid_argument&) {
        refinementForkRejected = true;
    }
    check(refinementForkRejected,
          "fallback refinement cannot bypass the serial-only restriction via a manual fork");
    TrainingObjective redundantObjective(train, 1000);
    const auto rescued = fitConstants(redundant(), redundantObjective, fallbackConfig);
    check(rescued.ordinaryOptimizerStop == LMStop::SingularSystem &&
              rescued.rankAwareFallbackAttempted && rescued.rankAwareFallbackImproved &&
              rescued.rankAwareFallbackOutcome == "improved",
          "unchanged singular ordinary fit receives one successful rank-aware fallback");
    check(rescued.loss.state() == FitnessState::Valid && rescued.loss.score() < 1e-18 &&
              score(*train, rescued.expression) < 1e-18 && score(*test, rescued.expression) < 1e-18,
          "fallback result and independently recomputed scores recover the redundant model");
    check(rescued.selectedDirections == std::vector<std::size_t>{0, 5} &&
              rescued.rankAwareFallbackOptimizerStop == LMStop::LossTolerance,
          "fallback reports selected postfix directions and native retry stop");
    check(rescued.ordinaryPhaseLossAttempts == 1 && rescued.fallbackPhaseLossAttempts > 0 &&
              rescued.fallbackPhaseSelectionWork.derivativeEvaluations > 0 &&
              rescued.totalBudgetUnits ==
                  rescued.ordinaryPhaseLossAttempts + rescued.fallbackPhaseLossAttempts +
                      rescued.fallbackPhaseSelectionWork.derivativeEvaluations &&
              redundantObjective.work().lossAttempts == rescued.lossAttempts,
          "fallback work is charged to shared per-fit objective allowance");
    check(rescued.optimizerWork.objectiveEvaluations ==
              rescued.ordinaryPhaseOptimizerWork.objectiveEvaluations +
                  rescued.fallbackPhaseOptimizerWork.objectiveEvaluations,
          "returned optimizer work reconciles both phases");

    TrainingObjective fullObjective(train, 1000);
    const auto full = fitConstants(fullRank(), fullObjective, fallbackConfig);
    check(full.ordinaryOptimizerStop != LMStop::SingularSystem &&
              !full.rankAwareFallbackAttempted &&
              full.rankAwareFallbackOutcome == "ordinary_not_singular" &&
              full.loss.state() == FitnessState::Valid && score(*test, full.expression) < 1e-18,
          "full-rank ordinary fit succeeds without invoking fallback");

    auto arithmeticLimited = fallbackConfig;
    arithmeticLimited.levenbergMarquardt.maxMathematicalOperations = 20;
    TrainingObjective otherStopObjective(train, 1000);
    const auto otherStop = fitConstants(redundant(), otherStopObjective, arithmeticLimited);
    check(otherStop.ordinaryOptimizerStop == LMStop::EvaluationLimit &&
              !otherStop.rankAwareFallbackAttempted &&
              otherStop.rankAwareFallbackOutcome == "ordinary_not_singular" &&
              otherStop.loss.state() == FitnessState::Valid &&
              otherStop.loss.score() == score(*train, otherStop.expression),
          "fallback is not attempted after an ordinary non-singular stop");

    auto insufficient = fallbackConfig;
    insufficient.maxLossAttempts =
        23; // ordinary baseline plus too little for 21 selector rows and LM
    TrainingObjective insufficientObjective(train, 1000);
    const auto retained = fitConstants(redundant(), insufficientObjective, insufficient);
    check(retained.ordinaryOptimizerStop == LMStop::SingularSystem &&
              !retained.rankAwareFallbackAttempted &&
              retained.rankAwareFallbackOutcome == "insufficient_budget" &&
              retained.loss.state() == FitnessState::Valid && retained.loss.score() == 13 &&
              retained.expression.sameRepresentation(redundant()) &&
              retained.totalBudgetUnits == retained.ordinaryPhaseLossAttempts,
          "insufficient retry budget preserves the ordinary valid incumbent");

    auto arithmeticExhaustedConfig = fallbackConfig;
    arithmeticExhaustedConfig.levenbergMarquardt.maxMathematicalOperations = 520;
    TrainingObjective arithmeticExhaustedObjective(train, 1000);
    const auto arithmeticExhausted =
        fitConstants(redundant(), arithmeticExhaustedObjective, arithmeticExhaustedConfig);
    check(arithmeticExhausted.ordinaryOptimizerStop == LMStop::SingularSystem &&
              arithmeticExhausted.rankAwareFallbackAttempted &&
              arithmeticExhausted.loss.state() == FitnessState::Valid &&
              arithmeticExhausted.loss.score() == 13 &&
              arithmeticExhausted.expression.sameRepresentation(redundant()) &&
              arithmeticExhausted.totalBudgetUnits ==
                  arithmeticExhausted.ordinaryPhaseLossAttempts +
                      arithmeticExhausted.fallbackPhaseLossAttempts +
                      arithmeticExhausted.fallbackPhaseSelectionWork.derivativeEvaluations &&
              arithmeticExhausted.preSelectionBaselineOperations +
                      arithmeticExhausted.fallbackPhaseSelectionWork.derivativeOperations +
                      arithmeticExhausted.fallbackPhaseSelectionWork.primalOperations +
                      arithmeticExhausted.fallbackPhaseSelectionWork.linearAlgebraOperations +
                      arithmeticExhausted.optimizerWork.mathematicalOperations <=
                  arithmeticExhaustedConfig.levenbergMarquardt.maxMathematicalOperations,
          "arithmetic-exhausted retry retains the valid ordinary incumbent and reconciles work");

    // Exercise the exact same policy through ordinary symbolic search. The
    // first population slot is a deterministic supplied regression fixture;
    // the remaining candidates still use the normal grammar and GA path.
    SymbolicRegressionConfig search;
    search.population = 4;
    search.elites = 1;
    search.maxGenerationAttempts = 1;
    search.maxEvaluations = 8;
    search.maxTrainingLossAttempts = 10000;
    search.grammar.operators = {K::Add, K::Multiply};
    search.grammar.continuousProbability = 1;
    search.grammar.continuousLower = -2;
    search.grammar.continuousUpper = 2;
    search.grammar.initialMaxDepth = 2;
    search.grammar.mutationMaxDepth = 2;
    search.grammar.proposalLimit = 16;
    search.fitting = fallbackConfig;
    search.fitting->lower = -8;
    search.fitting->upper = 8;
    search.initialExpressionSlots = 1;
    search.maximumInitializerWorkUnits = 1;
    RandomContext random(81723);
    auto searchResult = symbolicRegression(
        train, search, random, {}, [](const RegressionProblem&, std::size_t, std::uint64_t) {
            return InitialExpressionBatch{{redundant()}, 1, "synthetic_fallback_fixture", {}};
        });
    check(searchResult.best && searchResult.best->fitness() < 1e-18 &&
              score(*train, searchResult.best->expression()) < 1e-18,
          "ordinary search returns a model whose training loss independently recomputes");
    check(searchResult.rankAwareFallbackWork.ordinarySingularStops > 0 &&
              searchResult.rankAwareFallbackWork.attempts > 0 &&
              searchResult.rankAwareFallbackWork.improvedFits > 0 &&
              searchResult.trainingWork.lossAttempts +
                      searchResult.rankSelectionWork.derivativeEvaluations <=
                  search.maxTrainingLossAttempts &&
              searchResult.rankAwareFallbackWork.ordinaryLossAttempts +
                      searchResult.rankAwareFallbackWork.fallbackLossAttempts <=
                  searchResult.trainingWork.lossAttempts,
          "ordinary search exercises fallback under its shared search-wide allowance");
    bool parallelRejected = false;
    try {
        ExecutionContext execution({2});
        RandomContext parallelRandom(81723, execution);
        auto parallelConfig = search;
        (void)symbolicRegression(train, parallelConfig, parallelRandom);
    } catch (const std::invalid_argument&) {
        parallelRejected = true;
    }
    check(parallelRejected,
          "rank-aware fallback preserves the serial-only candidate-preparation restriction");
    bool checkpointedRunRejected = false;
    try {
        const auto scalarProblem = ScalarMathematicsProblem::scientific(rows(trainX), {64, 12, 32});
        RandomContext checkpointRandom(81723);
        (void)ScalarMathematicsRun(scalarProblem, search, checkpointRandom);
    } catch (const std::invalid_argument&) {
        checkpointedRunRejected = true;
    }
    check(checkpointedRunRejected,
          "rank-aware fallback preserves checkpointed scalar-run incompatibility");
    std::cout << "fixture=rank_aware_fallback_direct ordinary_stop="
              << lmStop(*rescued.ordinaryOptimizerStop)
              << " attempted=" << rescued.rankAwareFallbackAttempted
              << " outcome=" << rescued.rankAwareFallbackOutcome
              << " train=" << rescued.loss.score() << " held=" << score(*test, rescued.expression)
              << " ordinary_calls=" << rescued.ordinaryPhaseLossAttempts
              << " fallback_calls=" << rescued.fallbackPhaseLossAttempts << " selector_evaluations="
              << rescued.fallbackPhaseSelectionWork.derivativeEvaluations
              << " ordinary_lm_math=" << rescued.ordinaryPhaseOptimizerWork.mathematicalOperations
              << " fallback_lm_math=" << rescued.fallbackPhaseOptimizerWork.mathematicalOperations
              << " fallback_stop="
              << (rescued.rankAwareFallbackOptimizerStop
                      ? lmStop(*rescued.rankAwareFallbackOptimizerStop)
                      : "none")
              << '\n';
    std::cout << "fixture=rank_aware_fallback_search candidates="
              << searchResult.candidateEvaluations
              << " singular=" << searchResult.rankAwareFallbackWork.ordinarySingularStops
              << " attempts=" << searchResult.rankAwareFallbackWork.attempts
              << " improved=" << searchResult.rankAwareFallbackWork.improvedFits
              << " ordinary_calls=" << searchResult.rankAwareFallbackWork.ordinaryLossAttempts
              << " fallback_calls=" << searchResult.rankAwareFallbackWork.fallbackLossAttempts
              << " ordinary_lm_math="
              << searchResult.rankAwareFallbackWork.ordinaryOptimizerWork.mathematicalOperations
              << " fallback_lm_math="
              << searchResult.rankAwareFallbackWork.fallbackOptimizerWork.mathematicalOperations
              << " train=" << searchResult.best->fitness()
              << " total_calls=" << searchResult.trainingWork.lossAttempts
              << " selector_evaluations=" << searchResult.rankSelectionWork.derivativeEvaluations
              << '\n';
}
int main() {
    directFixtures();
    actualStartAndWeightedJacobians();
    ordinarySearch();
    rankAwareFallback();
}
