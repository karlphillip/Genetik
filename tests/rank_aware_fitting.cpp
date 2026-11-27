#include <SymbolicRegression.hpp>
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
    std::cout << "fixture=global_tight_budget baseline_train=" << baselineGloballyTight.loss.score()
              << " rank_train=" << globallyTight.loss.score()
              << " baseline_total_work=" << baselineGloballyTight.totalBudgetUnits
              << " selection_work=" << globallyTight.rankSelectionWork.derivativeEvaluations
              << " fit_loss_calls=" << globallyTightObjective.work().lossAttempts
              << " total_work=" << globallyTight.totalBudgetUnits
              << " remaining=" << globallyTightObjective.remaining()
              << " baseline_stop=" << lmStop(*baselineGloballyTight.optimizerStop)
              << " rank_stop=" << constantFitStopName(globallyTight.stop) << '\n';
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
    std::cout << "fixture=ordinary_generated_search seed=20261127 candidates="
              << found.candidateEvaluations << " training=" << found.best->fitness()
              << " heldout=" << score(*test, found.best->expression())
              << " selected_direction_count=" << found.rankSelectionWork.selectedDirections
              << " selected_derivative_evaluations="
              << found.rankSelectionWork.derivativeEvaluations
              << " selected_rows=" << found.rankSelectionWork.rows
              << " loss_calls=" << found.trainingWork.lossAttempts
              << " fit_loss_calls=" << found.trainingWork.fittingLossAttempts
              << " lm_objective_evaluations=" << found.optimizerWork.objectiveEvaluations
              << " work_total="
              << found.trainingWork.lossAttempts + found.rankSelectionWork.derivativeEvaluations
              << " stop=" << searchStopName(found.stoppingReason)
              << " equation=" << found.best->text() << '\n';
}
int main() {
    directFixtures();
    ordinarySearch();
}
