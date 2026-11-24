#include <ConstantFitting.hpp>
#include <InformationCriteria.hpp>
#include <SymbolicRegression.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

template <class Exception, class Action> void throws(Action&& action) {
    bool caught = false;
    try {
        action();
    } catch (const Exception&) {
        caught = true;
    }
    check(caught, "expected exception");
}

const std::vector<RegressionRow> outlierRows{{{0.0}, 0.0}, {{1.0}, 0.0}, {{2.0}, 10.0}};

void lossContracts() {
    const auto zero = Expression::constant(0.0, 1);
    const auto mse = RegressionProblem::create(outlierRows);
    RegressionObjectiveConfig maeConfig;
    maeConfig.loss = RegressionLoss::MeanAbsoluteError;
    const auto mae = RegressionProblem::createWithObjective(outlierRows, maeConfig, {31, 6, 8});
    RegressionObjectiveConfig huberConfig;
    huberConfig.loss = RegressionLoss::Huber;
    huberConfig.huberDelta = 1.0;
    const auto huber = RegressionProblem::createWithObjective(outlierRows, huberConfig, {31, 6, 8});
    const auto mseMetrics = mse->measureDetailed(zero);
    check(mseMetrics.metrics.loss.score() == 100.0 / 3.0,
          "default MSE definition remains unchanged");
    check(mseMetrics.rmse && *mseMetrics.rmse == std::sqrt(100.0 / 3.0),
          "RMSE is available as a reporting metric");
    check(mae->measure(zero).loss.score() == 10.0 / 3.0, "MAE definition");
    check(huber->measure(zero).loss.score() == 9.5 / 3.0, "Huber definition and delta");

    RegressionObjectiveConfig weightedConfig;
    weightedConfig.loss = RegressionLoss::WeightedMeanSquaredError;
    weightedConfig.observationWeights = {2.0, 1.0, 1.0};
    const auto weighted =
        RegressionProblem::createWithObjective(outlierRows, weightedConfig, {31, 6, 8});
    check(weighted->measure(zero).loss.score() == 25.0,
          "weighted MSE is normalized by total weight");
    check(weighted->objective().observationWeights == weightedConfig.observationWeights,
          "objective owns immutable copied weights");
    TrainingObjective weightedTraining(weighted, 8);
    const auto weightedMeasured = weightedTraining.measure(Expression::constant(0.0, 1));
    check(weightedMeasured.loss.score() == 25.0 && weightedTraining.work().rowAttempts == 3 &&
              weightedTraining.work().lossAttempts == 1 && weighted->parallelEvaluationSafe(),
          "weighted objective is immutable, parallel-safe, and row-accounted");
    std::array<RegressionMetrics, 2> parallelMetrics;
    std::thread first([&] { parallelMetrics[0] = weighted->measure(zero); });
    std::thread second([&] { parallelMetrics[1] = weighted->measure(zero); });
    first.join();
    second.join();
    check(parallelMetrics[0].loss == parallelMetrics[1].loss &&
              parallelMetrics[0].maxAbsoluteError == parallelMetrics[1].maxAbsoluteError &&
              parallelMetrics[0].samplesAttempted == parallelMetrics[1].samplesAttempted,
          "concurrent immutable weighted measurements are deterministic");
    throws<std::invalid_argument>([&] {
        auto bad = weightedConfig;
        bad.observationWeights.pop_back();
        (void)RegressionProblem::createWithObjective(outlierRows, bad, {31, 6, 8});
    });
    throws<std::invalid_argument>([&] {
        auto bad = weightedConfig;
        bad.observationWeights[0] = -1.0;
        (void)RegressionProblem::createWithObjective(outlierRows, bad, {31, 6, 8});
    });
    throws<std::invalid_argument>([&] {
        auto bad = weightedConfig;
        bad.observationWeights[0] = std::numeric_limits<double>::infinity();
        (void)RegressionProblem::createWithObjective(outlierRows, bad, {31, 6, 8});
    });
    throws<std::invalid_argument>([&] {
        auto bad = weightedConfig;
        bad.observationWeights = {0.0, 0.0, 0.0};
        (void)RegressionProblem::createWithObjective(outlierRows, bad, {31, 6, 8});
    });
    throws<std::invalid_argument>([&] {
        auto bad = huberConfig;
        bad.huberDelta = 0.0;
        (void)RegressionProblem::createWithObjective(outlierRows, bad, {31, 6, 8});
    });
    const auto largeMae =
        RegressionProblem::createWithObjective({{{0.0}, 1e200}}, maeConfig, {31, 6, 8})
            ->measureDetailed(Expression::constant(0.0, 1));
    check(largeMae.metrics.loss == Fitness::valid(1e200) && !largeMae.rmse,
          "non-MSE loss can remain valid when optional RMSE overflows");
}

void fittingAndSelectionUseObjective() {
    const auto fit = [](RegressionLoss loss) {
        RegressionObjectiveConfig objective;
        objective.loss = loss;
        if (loss == RegressionLoss::Huber)
            objective.huberDelta = 1.0;
        auto problem = RegressionProblem::createWithObjective(outlierRows, objective, {31, 6, 8});
        TrainingObjective training(problem);
        ConstantFitConfig config;
        config.lower = -2.0;
        config.upper = 8.0;
        config.initialStep = 1.0;
        config.minimumStep = 1e-7;
        config.maxLossAttempts = 512;
        const auto result =
            fitConstants(Expression::constant(0.0, 1, problem->limits()), training, config);
        check(result.loss.state() == FitnessState::Valid && training.work().lossAttempts > 1,
              "coordinate fitting invokes the selected objective");
        return result.expression.nodes().front().constantValue();
    };
    const double mseOptimum = fit(RegressionLoss::MeanSquaredError);
    const double repeatedMseOptimum = fit(RegressionLoss::MeanSquaredError);
    const double maeOptimum = fit(RegressionLoss::MeanAbsoluteError);
    const double huberOptimum = fit(RegressionLoss::Huber);
    check(std::abs(mseOptimum - 10.0 / 3.0) < 2e-6,
          "MSE constant optimum is the independently derived mean");
    check(std::abs(maeOptimum) < 1e-12, "MAE constant optimum is the independently derived median");
    check(std::abs(huberOptimum - 0.5) < 2e-6,
          "Huber constant optimum follows the clipped score equation");
    check(mseOptimum != maeOptimum && maeOptimum != huberOptimum && mseOptimum != huberOptimum,
          "MSE, MAE and Huber yield different fitted parameters");
    check(mseOptimum == repeatedMseOptimum,
          "coordinate objective fitting is deterministic without RNG state");

    RegressionObjectiveConfig huber;
    huber.loss = RegressionLoss::Huber;
    auto robustProblem = RegressionProblem::createWithObjective(outlierRows, huber, {31, 6, 8});
    ConstantFitConfig lm;
    lm.method = ConstantFitMethod::LevenbergMarquardt;
    TrainingObjective robustTraining(robustProblem);
    lm.lower = -2.0;
    lm.upper = 8.0;
    lm.levenbergMarquardt.lossTolerance = 1e-14;
    const auto robustFit =
        fitConstants(Expression::constant(0.0, 1, robustProblem->limits()), robustTraining, lm);
    check(robustFit.loss == robustProblem->measure(robustFit.expression).loss &&
              std::abs(robustFit.expression.nodes().front().constantValue() - 0.5) < 2e-6 &&
              std::abs(robustFit.loss.score() - 9.25 / 3.0) < 2e-6,
          "Huber LM fits through the same public scorer");
    RegressionObjectiveConfig maeObjective;
    maeObjective.loss = RegressionLoss::MeanAbsoluteError;
    const auto maeProblem =
        RegressionProblem::createWithObjective(outlierRows, maeObjective, {31, 6, 8});
    TrainingObjective maeTraining(maeProblem);
    throws<std::invalid_argument>([&] {
        (void)fitConstants(Expression::constant(0.0, 1, maeProblem->limits()), maeTraining, lm);
    });

    RegressionObjectiveConfig oneOutlier;
    oneOutlier.loss = RegressionLoss::Huber;
    oneOutlier.huberDelta = 1.0;
    auto analyticProblem = RegressionProblem::createWithObjective(
        {{{0}, 0}, {{0}, 0}, {{0}, 0}, {{0}, 0}, {{0}, 100}}, oneOutlier);
    TrainingObjective analyticTraining(analyticProblem);
    auto analyticConfig = lm;
    analyticConfig.lower = -1.0;
    analyticConfig.upper = 2.0;
    auto analyticCoordinateConfig = analyticConfig;
    analyticCoordinateConfig.method = ConstantFitMethod::Coordinate;
    TrainingObjective analyticCoordinateObjective(analyticProblem, 100000);
    const auto analyticCoordinate = fitConstants(
        Expression::constant(0.0, 1), analyticCoordinateObjective, analyticCoordinateConfig);
    const auto analytic =
        fitConstants(Expression::constant(0.0, 1), analyticTraining, analyticConfig);
    check(std::abs(analyticCoordinate.expression.nodes().front().constantValue() - 0.25) < 2e-6 &&
              std::abs(analyticCoordinate.loss.score() - 19.875) < 1e-10,
          "existing coordinate fitter reaches the same analytic Huber optimum");
    check(std::abs(analytic.expression.nodes().front().constantValue() - 0.25) < 1e-7 &&
              std::abs(analytic.loss.score() - 19.875) < 1e-10,
          "Huber LM reaches the analytic clipped-score constant optimum");
    std::cout << "Huber outlier constant: coordinate c="
              << analyticCoordinate.expression.nodes().front().constantValue()
              << " loss=" << analyticCoordinate.loss.score()
              << " loss_attempts=" << analyticCoordinate.lossAttempts
              << "; LM c=" << analytic.expression.nodes().front().constantValue()
              << " loss=" << analytic.loss.score()
              << " objective_evaluations=" << analytic.optimizerWork.objectiveEvaluations
              << " prediction_evaluations=" << analytic.optimizerWork.forwardEvaluations << '\n';

    // The pseudo-residual and its Jacobian are differentiable at zero and at
    // both Huber thresholds, with the exact linear-tail derivative outside.
    const auto huberResiduals = expressionLeastSquares(
        Expression::variable(0), {{{0}, 0.0}, {{0}, 1.0}, {{0}, -1.0}, {{0}, 2.0}, {{0}, -2.0}},
        {0}, 1000, {}, 1.0);
    const auto atThresholds = huberResiduals(std::array{0.0}, true, 1000);
    check(atThresholds.valid && atThresholds.residuals[0] == 0.0 &&
              atThresholds.jacobian[0] == 1.0 / std::sqrt(10.0) &&
              atThresholds.jacobian[1] == 1.0 / std::sqrt(10.0) &&
              atThresholds.jacobian[2] == 1.0 / std::sqrt(10.0) &&
              std::abs(atThresholds.jacobian[3] - 1.0 / std::sqrt(30.0)) < 1e-15 &&
              std::abs(atThresholds.jacobian[4] - 1.0 / std::sqrt(30.0)) < 1e-15,
          "Huber transform handles zero, both thresholds, and linear tails");
    check(std::abs(genetik_detail::checkedSSE(atThresholds.residuals) - 0.8) < 1e-15,
          "Huber pseudo-residual SSE equals the normalized public Huber objective");

    auto mseProblem = RegressionProblem::create(outlierRows, {31, 6, 8});
    ExpressionCandidate mseZero(Expression::constant(0.0, 1), mseProblem);
    ExpressionCandidate mseThree(Expression::constant(3.0, 1, mseProblem->limits()), mseProblem);
    mseZero.calculateFitness();
    mseThree.calculateFitness();
    check(RegressionOrdering{}.better(mseThree, mseZero, ObjectiveDirection::Minimize),
          "evolutionary ordering uses MSE fitness");
    ExpressionCandidate huberZero(Expression::constant(0.0, 1), robustProblem);
    ExpressionCandidate huberThree(Expression::constant(3.0, 1, robustProblem->limits()),
                                   robustProblem);
    huberZero.calculateFitness();
    huberThree.calculateFitness();
    check(RegressionOrdering{}.better(huberZero, huberThree, ObjectiveDirection::Minimize),
          "evolutionary ordering follows the selected robust fitness");
}

void huberLmComparisonAndBudget() {
    constexpr double delta = 1.0;
    const auto target = [](double x) { return 2.0 * std::pow(x, 1.4); };
    std::vector<RegressionRow> rows;
    for (double x : {0.5, 1.0, 1.5, 2.0, 2.5})
        rows.push_back({{x}, target(x) + (x == 1.5 ? 5.0 : 0.0)});
    RegressionObjectiveConfig huber;
    huber.loss = RegressionLoss::Huber;
    huber.huberDelta = delta;
    const auto problem = RegressionProblem::createWithObjective(rows, huber, {31, 6, 8});
    const auto x = Expression::variable(0);
    const auto initial = Expression::binary(
        ExpressionKind::Multiply, Expression::constant(2.0, 1),
        Expression::binary(ExpressionKind::Pow, x, Expression::constant(1.4, 1)));
    ConstantFitConfig coordinate;
    coordinate.lower = 0.1;
    coordinate.upper = 4.0;
    coordinate.initialStep = 0.25;
    coordinate.minimumStep = 1e-5;
    coordinate.maxLossAttempts = 3000;
    coordinate.maxSweeps = 40;
    coordinate.maxProposals = 3000;
    TrainingObjective coordinateObjective(problem, 100000);
    const auto coordinateFit = fitConstants(initial, coordinateObjective, coordinate);

    auto lm = coordinate;
    lm.method = ConstantFitMethod::LevenbergMarquardt;
    lm.levenbergMarquardt.maxIterations = 100;
    lm.levenbergMarquardt.maxObjectiveEvaluations = 300;
    lm.levenbergMarquardt.maxJacobianEvaluations = 300;
    lm.levenbergMarquardt.lossTolerance = 1e-14;
    TrainingObjective lmObjective(problem, 100000);
    const auto lmFit = fitConstants(initial, lmObjective, lm);
    auto heldOutRmse = [&](const Expression& expression) {
        double squares = 0.0;
        for (double value : {3.0, 4.0}) {
            const auto prediction = expression.evaluate(std::array{value});
            check(prediction.valid(), "Huber comparison held-out prediction valid");
            const double residual = prediction.value() - target(value);
            squares += residual * residual;
        }
        return std::sqrt(squares / 2.0);
    };
    auto report = [&](const char* name, const ConstantFitResult& fit,
                      std::uint64_t predictionEvaluations) {
        const auto parameters = fit.expression.nodes();
        std::cout << "Huber nonlinear " << name << ": a=" << parameters[0].constantValue()
                  << " b=" << parameters[2].constantValue()
                  << " training_loss=" << problem->measure(fit.expression).loss.score()
                  << " heldout_rmse=" << heldOutRmse(fit.expression)
                  << " prediction_evaluations=" << predictionEvaluations
                  << " stop=" << constantFitStopName(fit.stop) << '\n';
    };
    report("coordinate", coordinateFit, coordinateObjective.work().rowAttempts);
    report("LM", lmFit, lmFit.optimizerWork.forwardEvaluations);
    check(lmFit.loss == problem->measure(lmFit.expression).loss,
          "Huber LM score belongs to returned nonlinear parameters");

    auto square = Expression::power(Expression::constant(1.0, 1), 2);
    const auto zeroTarget = RegressionProblem::createWithObjective({{{0}, 0.0}}, huber);
    auto budget = lm;
    budget.lower = -1.0;
    budget.upper = 2.0;
    budget.maxLossAttempts = 3;
    budget.levenbergMarquardt.maxObjectiveEvaluations = 20;
    budget.levenbergMarquardt.lossTolerance = 0.0;
    budget.levenbergMarquardt.gradientTolerance = 1e-30;
    budget.levenbergMarquardt.stepTolerance = 1e-30;
    TrainingObjective budgetObjective(zeroTarget, 3);
    const auto budgetFit = fitConstants(square, budgetObjective, budget);
    check(budgetFit.stop == ConstantFitStop::EvaluationLimit &&
              budgetFit.optimizerWork.objectiveEvaluations == 3 &&
              budgetFit.optimizerWork.acceptedSteps > 0 &&
              budgetFit.loss == zeroTarget->measure(budgetFit.expression).loss,
          "Huber LM budget stop retains the accepted parameter score");
}

void informationCriteria() {
    const auto problem = RegressionProblem::create(
        {{{-1.0}, 0.2}, {{0.0}, 0.9}, {{1.0}, 2.1}, {{2.0}, 2.8}, {{3.0}, 4.2}, {{4.0}, 4.9}});
    const std::array models{InformationCriterionCandidate{Expression::constant(1.8, 1), 0},
                            InformationCriterionCandidate{Expression::variable(0), 0},
                            InformationCriterionCandidate{
                                Expression::binary(ExpressionKind::Add, Expression::variable(0),
                                                   Expression::constant(0.1, 1)),
                                1}};
    const auto aic = selectInformationCriterion(*problem, models, InformationCriterion::Akaike);
    const auto bic = selectInformationCriterion(*problem, models, InformationCriterion::Bayesian);
    check(aic.status == InformationCriterionStatus::Success && aic.selectedIndex,
          "AIC selects from an external post-search candidate set");
    check(selectInformationCriterion(*problem, std::span<const InformationCriterionCandidate>{},
                                     InformationCriterion::Akaike)
                  .status == InformationCriterionStatus::NoCandidates,
          "empty post-search candidate set is explicit");
    check(bic.status == InformationCriterionStatus::Success && bic.selectedIndex,
          "BIC selects from an external post-search candidate set");
    check(aic.candidates[0].structuralComplexity == 1 &&
              aic.candidates[2].structuralComplexity > aic.candidates[0].structuralComplexity &&
              aic.candidates[0].likelihoodParameterCount == 1 &&
              aic.candidates[2].likelihoodParameterCount == 2,
          "structural complexity is not confused with fitted or variance parameters");
    check(aic.work.candidatesEvaluated == models.size() &&
              aic.work.rowEvaluations == models.size() * problem->sampleCount() &&
              aic.work.expressionOperations > 0 && aic.work.peakEstimatedMemoryBytes > 0,
          "model-selection work is measured");
    const double expectedLinearMse = 6.35 / 6.0;
    const double expectedLinearAic =
        2.0 + 6.0 * (std::log(2.0 * std::acos(-1.0)) + 1.0 + std::log(expectedLinearMse));
    check(aic.candidates[1].mse && std::abs(*aic.candidates[1].mse - expectedLinearMse) < 1e-14 &&
              aic.candidates[1].criterion &&
              std::abs(*aic.candidates[1].criterion - expectedLinearAic) < 1e-13,
          "Gaussian AIC independently matches the stated likelihood formula");

    const auto exact = RegressionProblem::create({{{0.0}, 0.0}, {{1.0}, 1.0}, {{2.0}, 2.0}});
    const std::array exactModel{InformationCriterionCandidate{Expression::variable(0), 0}};
    const auto zeroVariance =
        selectInformationCriterion(*exact, exactModel, InformationCriterion::Akaike);
    check(zeroVariance.status == InformationCriterionStatus::ZeroResidualVariance &&
              !zeroVariance.selectedIndex && zeroVariance.candidates[0].mse == 0.0,
          "zero residual variance is explicitly undefined");
    const auto few = RegressionProblem::create({{{0.0}, 1.0}, {{1.0}, 3.0}});
    const std::array fitted{InformationCriterionCandidate{
        Expression::binary(ExpressionKind::Add, Expression::variable(0),
                           Expression::constant(0.0, 1)),
        1}};
    check(selectInformationCriterion(*few, fitted, InformationCriterion::Bayesian).status ==
              InformationCriterionStatus::InsufficientObservations,
          "insufficient observations are checked against likelihood parameters");

    RegressionObjectiveConfig mae;
    mae.loss = RegressionLoss::MeanAbsoluteError;
    const auto robust = RegressionProblem::createWithObjective(outlierRows, mae, {31, 6, 8});
    check(selectInformationCriterion(*robust, models, InformationCriterion::Akaike).status ==
              InformationCriterionStatus::UnsupportedObjective,
          "Gaussian likelihood rejects incompatible objective configurations");
    const std::array invalidModels{InformationCriterionCandidate{
        Expression::unary(ExpressionKind::Log, Expression::variable(0)), 0}};
    const auto invalidProblem = RegressionProblem::create({{{-1.0}, 1.0}, {{1.0}, 2.0}});
    const auto invalidPrediction =
        selectInformationCriterion(*invalidProblem, invalidModels, InformationCriterion::Akaike);
    check(invalidPrediction.status == InformationCriterionStatus::InvalidPrediction &&
              invalidPrediction.candidates[0].status ==
                  InformationCriterionStatus::InvalidPrediction,
          "mathematically invalid model prediction is explicit");
    InformationCriterionConfig exhausted;
    exhausted.maximumExpressionOperations = 1;
    const auto limited =
        selectInformationCriterion(*problem, models, InformationCriterion::Akaike, exhausted);
    check(limited.status == InformationCriterionStatus::ResourceExhausted && !limited.selectedIndex,
          "incomplete criterion search never returns a selected model");
    InformationCriterionConfig memoryLimit;
    memoryLimit.maximumEstimatedMemoryBytes = 1;
    check(selectInformationCriterion(*problem, models, InformationCriterion::Akaike, memoryLimit)
                  .status == InformationCriterionStatus::ResourceExhausted,
          "estimated-memory limit is enforced before scoring");
}
} // namespace

int main() {
    try {
        lossContracts();
        fittingAndSelectionUseObjective();
        huberLmComparisonAndBudget();
        informationCriteria();
        std::cout << "mathematical objective and information-criterion contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
