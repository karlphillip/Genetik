#include <ConstantFitting.hpp>
#include <InformationCriteria.hpp>

#include <array>
#include <cmath>
#include <iostream>

int main() {
    const std::vector<RegressionRow> rows{{{0.0}, 0.0}, {{1.0}, 0.0}, {{2.0}, 10.0}};
    const auto showFit = [&](RegressionLoss loss, double delta, const char* label) {
        RegressionObjectiveConfig objective;
        objective.loss = loss;
        objective.huberDelta = delta;
        const auto problem = RegressionProblem::createWithObjective(rows, objective, {31, 6, 8});
        TrainingObjective training(problem);
        ConstantFitConfig fitting;
        fitting.lower = -2.0;
        fitting.upper = 8.0;
        fitting.initialStep = 1.0;
        const auto fit =
            fitConstants(Expression::constant(0.0, 1, problem->limits()), training, fitting);
        if (fit.loss.state() != FitnessState::Valid)
            return false;
        std::cout << label << " optimum=" << fit.expression.toString()
                  << " objective=" << fit.loss.score()
                  << " attempts=" << training.work().lossAttempts << '\n';
        return true;
    };

    if (!showFit(RegressionLoss::MeanSquaredError, 1.0, "MSE") ||
        !showFit(RegressionLoss::MeanAbsoluteError, 1.0, "MAE") ||
        !showFit(RegressionLoss::Huber, 1.0, "Huber"))
        return 1;

    const auto gaussian = RegressionProblem::create(
        {{{-1.0}, 0.2}, {{0.0}, 0.9}, {{1.0}, 2.1}, {{2.0}, 2.8}, {{3.0}, 4.2}, {{4.0}, 4.9}});
    const std::array candidates{InformationCriterionCandidate{Expression::constant(1.8, 1), 0},
                                InformationCriterionCandidate{Expression::variable(0), 0},
                                InformationCriterionCandidate{
                                    Expression::binary(ExpressionKind::Add, Expression::variable(0),
                                                       Expression::constant(0.1, 1)),
                                    0}};
    const auto selection =
        selectInformationCriterion(*gaussian, candidates, InformationCriterion::Bayesian);
    if (selection.status != InformationCriterionStatus::Success || !selection.selectedIndex)
        return 1;
    const auto index = *selection.selectedIndex;
    std::cout << "BIC selected=" << candidates[index].expression.toString()
              << " criterion=" << *selection.candidates[index].criterion
              << " fitted_parameters=" << selection.candidates[index].fittedParameterCount
              << " structural_nodes=" << selection.candidates[index].structuralComplexity
              << " rows=" << selection.work.rowEvaluations
              << " expression_operations=" << selection.work.expressionOperations << '\n';
}
