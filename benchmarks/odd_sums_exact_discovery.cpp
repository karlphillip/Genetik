#include "odd_sums_verifier.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

using namespace genetik::constructive;
using odd_sums_verifier::inductionCheck;
using odd_sums_verifier::Proof;

namespace {
const char* name(Proof p) {
    switch (p) {
    case Proof::Proved:
        return "proved_by_exact_induction_over_Z";
    case Proof::Counterexample:
        return "counterexample_found";
    case Proof::Inconclusive:
        return "inconclusive";
    }
    return "inconclusive";
}
} // namespace

int main() {
    // Fixed budget, declared before discovery. Only the five observed values
    // enter the synthesizer; neither the recurrence nor held-out values do.
    constexpr std::size_t maxNodes = 7;
    constexpr std::size_t maxStructures = 60000;
    constexpr std::size_t maxCompositionAttempts = 250000;
    constexpr std::uint64_t maxTrainingPointEvaluations = 1000000;
    constexpr std::uint64_t maxTransitionSteps = 10000000;
    constexpr std::uint64_t maxNodeVisits = 20000000;
    constexpr std::uint64_t maxArithmeticOperations = 10000000;
    constexpr std::size_t maxStorageBytes = 32 * 1024 * 1024;
    RecurrenceSynthesisConfig config;
    config.maximumNodes = maxNodes;
    config.maximumStructures = maxStructures;
    config.maximumCompositionAttempts = maxCompositionAttempts;
    config.maximumTrainingPointEvaluations = maxTrainingPointEvaluations;
    config.maximumTransitionSteps = maxTransitionSteps;
    config.maximumNodeVisits = maxNodeVisits;
    config.maximumArithmeticOperations = maxArithmeticOperations;
    config.maximumEstimatedStorageBytes = maxStorageBytes;
    const std::vector<SequencePoint> training{{0, 0}, {1, 1}, {2, 4}, {3, 9}, {4, 16}};
    const auto found = synthesizeRecurrenceTransition(2, {0, 1}, training, config);

    // Preserve and print the returned candidate before any held-out assessment.
    std::cout << "expression=" << found.canonicalTransition << "\n";
    std::cout << "search.stop=" << toString(found.stop)
              << " training=" << toString(found.trainingRecovery)
              << " matched=" << found.matchedTrainingPoints << '/' << training.size()
              << " nodes=" << found.expressionNodes << "\n";
    std::cout << "search.work composition_attempts=" << found.stats.compositionAttempts
              << " unique=" << found.stats.uniqueStructures
              << " candidate_rollouts=" << found.stats.candidateRollouts
              << " training_points=" << found.stats.trainingPoints
              << " transition_steps=" << found.stats.transitionSteps
              << " node_visits=" << found.stats.nodeVisits
              << " arithmetic_operations=" << found.stats.arithmeticOperations
              << " stored_bytes_estimate=" << found.stats.estimatedStoredBytes << "\n";
    std::cout << "outcome.exact_training_match="
              << (found.trainingRecovery == TrainingRecoveryStatus::Exact ? "yes" : "no") << "\n";

    const std::vector<SequencePoint> heldOut{{5, 25}, {6, 36}, {7, 49}};
    const auto assessment = assessRecurrenceTransition(found, 2, {0, 1}, heldOut);
    std::cout << "outcome.exact_held_out_predictions="
              << (assessment.status == AssessmentStatus::ExactHeldOut ? "yes" : "no")
              << " status=" << toString(assessment.status)
              << " matched=" << assessment.matchedObservations << '/' << heldOut.size() << "\n";
    const auto proof = found.transition ? inductionCheck(*found.transition) : Proof::Inconclusive;
    std::cout << "outcome.induction=" << name(proof)
              << " scope=all_natural_indices_over_mathematical_integers"
              << " checked_Int64=finite_valid_rollouts_only\n";

    // Control: this recurrence agrees through s(2)=4, then adds x1 mod 2
    // and diverges at s(3). It is not used in discovery.
    const std::array<DiscreteNode, 11> controlNodes{DiscreteNode::integer(2),
                                                    DiscreteNode::variable(2),
                                                    DiscreteNode::operation(DiscreteKind::Multiply),
                                                    DiscreteNode::variable(1),
                                                    DiscreteNode::operation(DiscreteKind::Subtract),
                                                    DiscreteNode::integer(2),
                                                    DiscreteNode::operation(DiscreteKind::Add),
                                                    DiscreteNode::variable(1),
                                                    DiscreteNode::integer(2),
                                                    DiscreteNode::operation(DiscreteKind::Modulo),
                                                    DiscreteNode::operation(DiscreteKind::Add)};
    const DiscreteExpression control(controlNodes, 3);
    SynthesisResult controlResult;
    controlResult.transition = control;
    controlResult.canonicalTransition = control.toString();
    const auto controlAssessment =
        assessRecurrenceTransition(controlResult, 2, {0, 1}, {{2, 4}, {3, 9}});
    const auto controlProof = inductionCheck(control);
    IntegerRecurrence controlModel(0, {0, 1}, control);
    const auto controlAtThree = controlModel.evaluate(3);
    std::cout << "control.expression=" << control.toString() << "\n"
              << "control.held_out=" << toString(controlAssessment.status)
              << " matched=" << controlAssessment.matchedObservations << "/2"
              << " induction=" << name(controlProof)
              << " first_counterexample_index=3 expected=9 actual="
              << (controlAtThree.valid() ? controlAtThree.value->integer() : -1) << "\n";
    return found.trainingRecovery == TrainingRecoveryStatus::Exact &&
                   assessment.status == AssessmentStatus::ExactHeldOut && proof == Proof::Proved &&
                   controlAssessment.status == AssessmentStatus::PartialHeldOut &&
                   controlProof == Proof::Counterexample
               ? 0
               : 1;
}
