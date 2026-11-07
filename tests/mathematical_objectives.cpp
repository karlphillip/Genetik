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
    throws<std::invalid_argument>([&] {
        (void)fitConstants(Expression::constant(0.0, 1, robustProblem->limits()), robustTraining,
                           lm);
    });

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
        informationCriteria();
        std::cout << "mathematical objective and information-criterion contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
