#include <ConstructiveRecurrenceSynthesis.hpp>

#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace genetik::constructive;

int main() {
    // Freeze every remaining search and assessment limit before constructing
    // either observation set or invoking synthesis.
    RecurrenceSynthesisConfig search;
    search.operators = {DiscreteKind::Add, DiscreteKind::Subtract,
                        DiscreteKind::Multiply, DiscreteKind::Modulo};
    search.constants = {-1, 0, 1, 2, 3};
    search.maximumNodes = 5;
    search.expressionLimits.maxNodes = 5;
    search.expressionLimits.maxDepth = 5;
    search.expressionLimits.maxVariables = 4;
    search.expressionLimits.maxConstants = 5;
    search.expressionLimits.maxConstantMagnitude = 3;
    search.expressionLimits.maxStorageBytes = 4 * 1024 * 1024;
    search.expressionLimits.maxIterationDepth = 8;
    search.expressionLimits.maxBinders = 128;
    search.maximumStructures = 60000;
    search.maximumCompositionAttempts = 250000;
    search.maximumTrainingPointEvaluations = 1000000;
    search.maximumTransitionSteps = 10000000;
    search.maximumNodeVisits = 20000000;
    search.maximumArithmeticOperations = 10000000;
    search.maximumEstimatedStorageBytes = 32 * 1024 * 1024;
    search.maximumSteps = 32;
    search.evaluationLimits.maxNodeVisits = 4096;
    search.evaluationLimits.maxOperations = 4096;
    search.evaluationLimits.maxWorkspaceBytes = 1024 * 1024;
    search.evaluationLimits.maxIterationsPerOperation = 10000;
    search.evaluationLimits.maxAggregateIterations = 100000;
    search.evaluationLimits.maxIterationDepth = 8;

    RecurrenceAssessmentConfig assessmentLimits;
    assessmentLimits.maximumObservations = 100;
    assessmentLimits.maximumTransitionSteps = 10000;
    assessmentLimits.maximumNodeVisits = 100000;
    assessmentLimits.maximumArithmeticOperations = 100000;
    assessmentLimits.maximumSteps = 32;
    assessmentLimits.evaluationLimits = search.evaluationLimits;

    const std::vector<std::int64_t> reference{0, 0, 1, 1, 2, 4, 7, 13, 24, 44,
                                               81, 149, 274, 504};
    const std::vector<SequencePoint> training{{0, 0}, {1, 0}, {2, 1}, {3, 1}, {4, 2},
                                               {5, 4}, {6, 7}, {7, 13}, {8, 24}, {9, 44}};
    const std::vector<SequencePoint> heldOut{{10, 81}, {11, 149}, {12, 274}, {13, 504}};
    std::cout << "fixed limits: nodes=" << search.maximumNodes
              << " structures=" << search.maximumStructures
              << " compositions=" << search.maximumCompositionAttempts
              << " training-point-evaluations=" << search.maximumTrainingPointEvaluations
              << " transitions=" << search.maximumTransitionSteps
              << " node-visits=" << search.maximumNodeVisits
              << " arithmetic-operations=" << search.maximumArithmeticOperations
              << " estimated-storage-bytes=" << search.maximumEstimatedStorageBytes
              << " steps=" << search.maximumSteps
              << " expression-depth=" << search.expressionLimits.maxDepth
              << " variables=" << search.expressionLimits.maxVariables
              << " constants=" << search.expressionLimits.maxConstants
              << " constant-magnitude=" << search.expressionLimits.maxConstantMagnitude
              << " expression-storage-bytes=" << search.expressionLimits.maxStorageBytes
              << " expression-iteration-depth=" << search.expressionLimits.maxIterationDepth
              << " binders=" << search.expressionLimits.maxBinders
              << " evaluator-node-visits=" << search.evaluationLimits.maxNodeVisits
              << " evaluator-arithmetic-operations=" << search.evaluationLimits.maxOperations
              << " evaluator-workspace-bytes=" << search.evaluationLimits.maxWorkspaceBytes
              << " iterations-per-operation=" << search.evaluationLimits.maxIterationsPerOperation
              << " aggregate-iterations=" << search.evaluationLimits.maxAggregateIterations
              << " evaluator-iteration-depth=" << search.evaluationLimits.maxIterationDepth
              << " assessment-observations=" << assessmentLimits.maximumObservations
              << " assessment-transitions=" << assessmentLimits.maximumTransitionSteps
              << " assessment-node-visits=" << assessmentLimits.maximumNodeVisits
              << " assessment-arithmetic-operations=" << assessmentLimits.maximumArithmeticOperations
              << " assessment-steps=" << assessmentLimits.maximumSteps
              << "\n";

    const auto selected = synthesizeRecurrenceTransition(3, {0, 0, 1}, training, search);
    const auto held = assessRecurrenceTransition(selected, 3, {0, 0, 1}, heldOut,
                                                  assessmentLimits);
    std::cout << "selected=" << selected.canonicalTransition
              << " training=" << selected.matchedTrainingPoints << '/' << training.size()
              << " initial-terms=3/3 predicted-training="
              << selected.matchedTrainingPoints - 3 << "/" << training.size() - 3
              << " held-out=" << held.matchedObservations << '/' << held.observationCount
              << " structures=" << selected.stats.uniqueStructures
              << " arithmetic-operations=" << selected.stats.arithmeticOperations
              << " held-out-arithmetic-operations=" << held.arithmeticOperations
              << " stop=" << toString(selected.stop) << '\n';
    if (!selected.transition || selected.trainingRecovery != TrainingRecoveryStatus::Exact ||
        held.status != AssessmentStatus::ExactHeldOut)
        throw std::runtime_error("Tribonacci benchmark did not meet its exact-match controls");

    // Reuse the frozen transition from another initial state. Expected values
    // come from the independent reference vector, not from synthesis output.
    IntegerRecurrence reused(0, {2, 0, 1}, *selected.transition);
    const std::array<std::int64_t, 7> alternateReference{2, 0, 1, 3, 4, 8, 15};
    std::size_t alternateMatches = 0;
    for (std::size_t index = 3; index < 7; ++index) {
        const auto value = reused.evaluate(static_cast<std::int64_t>(index));
        if (!value.valid()) throw std::runtime_error("alternate initial-state rollout invalid");
        alternateMatches += value.value->integer() == alternateReference[index];
        std::cout << "alternate-index=" << index << " expected=" << alternateReference[index]
                  << " actual=" << value.value->integer() << '\n';
    }
    std::cout << "alternate-initial=[2,0,1] next=" << reused.evaluate(3).value->integer()
              << " rollout-matches=" << alternateMatches << "/4"
              << " recovered-three-term-sum="
              << (selected.canonicalTransition == "(x1 + (x2 + x3))" ? "yes" : "no")
              << " finite-agreement-is-universal-proof=no\n";
    if (alternateMatches != 4) throw std::runtime_error("alternate initial-state rollout mismatch");
}
