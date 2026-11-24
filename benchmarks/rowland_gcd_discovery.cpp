#include <ConstructiveRecurrenceSynthesis.hpp>
#include <DiscreteExpressionParsing.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <string_view>
#include <vector>

using namespace genetik::constructive;

namespace {
// Frozen before either search arm. The candidate's GCD is first in its
// explicit opt-in grammar; the ordinary operators retain their existing order.
constexpr std::size_t maximumNodes = 7;
constexpr std::size_t maximumStructures = 3000000;
constexpr std::size_t maximumCompositionAttempts = 15000000;
constexpr std::uint64_t maximumTrainingPointEvaluations = 1000000;
constexpr std::uint64_t maximumTransitionSteps = 50000000;
constexpr std::uint64_t maximumNodeVisits = 100000000;
constexpr std::uint64_t maximumArithmeticOperations = 100000000;
constexpr std::size_t maximumEstimatedStorageBytes = 768 * 1024 * 1024;
constexpr std::size_t maximumSteps = 64;

constexpr std::size_t lastTrainingIndex = 8;
constexpr std::size_t firstHeldOutIndex = lastTrainingIndex + 1;
constexpr std::size_t lastHeldOutIndex = 13;

std::vector<std::int64_t> referenceSequence(std::size_t lastIndex) {
    std::vector<std::int64_t> values{7};
    for (std::size_t k = 0; k < lastIndex; ++k) {
        const auto increment = std::gcd(static_cast<std::int64_t>(k + 2), values.back());
        values.push_back(values.back() + increment);
    }
    return values;
}

std::vector<SequencePoint> rows(const std::vector<std::int64_t>& values, std::size_t first,
                                std::size_t last) {
    std::vector<SequencePoint> result;
    for (std::size_t i = first; i <= last; ++i)
        result.push_back({static_cast<std::int64_t>(i), values.at(i)});
    return result;
}

RecurrenceSynthesisConfig config(bool enableGcd) {
    RecurrenceSynthesisConfig result;
    result.operators =
        enableGcd ? std::vector<DiscreteKind>{DiscreteKind::Gcd, DiscreteKind::Add,
                                              DiscreteKind::Subtract, DiscreteKind::Multiply,
                                              DiscreteKind::Modulo}
                  : std::vector<DiscreteKind>{DiscreteKind::Add, DiscreteKind::Subtract,
                                              DiscreteKind::Multiply, DiscreteKind::Modulo};
    result.constants = {-1, 0, 1, 2, 3};
    result.maximumNodes = maximumNodes;
    result.maximumStructures = maximumStructures;
    result.maximumCompositionAttempts = maximumCompositionAttempts;
    result.maximumTrainingPointEvaluations = maximumTrainingPointEvaluations;
    result.maximumTransitionSteps = maximumTransitionSteps;
    result.maximumNodeVisits = maximumNodeVisits;
    result.maximumArithmeticOperations = maximumArithmeticOperations;
    result.maximumEstimatedStorageBytes = maximumEstimatedStorageBytes;
    result.maximumSteps = maximumSteps;
    return result;
}

void runArm(std::string_view label, bool enableGcd, const std::vector<SequencePoint>& training,
            const std::vector<SequencePoint>& heldOut, const std::vector<std::int64_t>& reference) {
    const auto found = synthesizeRecurrenceTransition(1, {7}, training, config(enableGcd));
    std::cout << "arm=" << label
              << " expression=" << (found.transition ? found.canonicalTransition : "<none>")
              << '\n';
    std::cout << "arm=" << label << " stop=" << toString(found.stop)
              << " training=" << toString(found.trainingRecovery)
              << " matched=" << found.matchedTrainingPoints << '/' << training.size()
              << " structures=" << found.stats.uniqueStructures
              << " composition_attempts=" << found.stats.compositionAttempts
              << " training_point_evaluations=" << found.stats.trainingPoints
              << " transition_steps=" << found.stats.transitionSteps
              << " node_visits=" << found.stats.nodeVisits
              << " arithmetic_operations=" << found.stats.arithmeticOperations
              << " stored_bytes_estimate=" << found.stats.estimatedStoredBytes << '\n';

    if (!found.transition) {
        std::cout << "arm=" << label << " exact_training_match=no"
                  << " exact_held_out_match=no first_divergence=unavailable"
                  << " reference_transition_recovered=no\n";
        return;
    }

    const auto assessment = assessRecurrenceTransition(found, 1, {7}, heldOut);
    std::int64_t firstMismatch = -1;
    std::int64_t actualAtMismatch = 0;
    const IntegerRecurrence selectedModel(0, {7}, *found.transition);
    for (std::int64_t index = 0; index <= static_cast<std::int64_t>(lastHeldOutIndex); ++index) {
        const auto predicted = selectedModel.evaluate(index);
        if (!predicted.valid() ||
            predicted.value->integer() != reference.at(static_cast<std::size_t>(index))) {
            firstMismatch = index;
            if (predicted.valid())
                actualAtMismatch = predicted.value->integer();
            break;
        }
    }
    bool referenceTransitionRecovered = false;
    if (enableGcd) {
        // Constructed only after selection; the rule is never a search seed,
        // template, or candidate filter.
        const auto referenceTransition = parseDiscreteExpression("x1 + gcd(x0 + 2, x1)", 2);
        const auto commutedGcdTransition = parseDiscreteExpression("x1 + gcd(x1, x0 + 2)", 2);
        const auto sameTree = [&](const DiscreteExpression& other) {
            return found.transition->dimensions() == other.dimensions() &&
                   std::equal(found.transition->nodes().begin(), found.transition->nodes().end(),
                              other.nodes().begin(), other.nodes().end());
        };
        // GCD is symmetric, so the second form is the same mathematical rule.
        referenceTransitionRecovered =
            sameTree(referenceTransition) || sameTree(commutedGcdTransition);
    }
    const auto exactTraining = found.trainingRecovery == TrainingRecoveryStatus::Exact;
    const auto exactHeldOut = assessment.status == AssessmentStatus::ExactHeldOut;
    std::cout << "arm=" << label << " exact_training_match=" << (exactTraining ? "yes" : "no")
              << " exact_held_out_match=" << (exactHeldOut ? "yes" : "no")
              << " held_out=" << toString(assessment.status)
              << " matched=" << assessment.matchedObservations << '/' << heldOut.size();
    if (firstMismatch >= 0)
        std::cout << " first_divergence_index=" << firstMismatch
                  << " expected=" << reference.at(static_cast<std::size_t>(firstMismatch))
                  << " actual=" << actualAtMismatch;
    else
        std::cout << " first_divergence=none";
    std::cout << " reference_transition_recovered=" << (referenceTransitionRecovered ? "yes" : "no")
              << '\n';
}
} // namespace

int main() {
    // Data roles and all search settings are fixed in this program before
    // either arm starts. The Rowland recurrence below only materializes the
    // observations and post-selection assessment values.
    const auto sequence = referenceSequence(lastHeldOutIndex);
    const auto training = rows(sequence, 0, lastTrainingIndex);
    const auto heldOut = rows(sequence, firstHeldOutIndex, lastHeldOutIndex);
    std::cout << "protocol.training_indices=0.." << lastTrainingIndex
              << " initial_state=[7] held_out_indices=" << firstHeldOutIndex << ".."
              << lastHeldOutIndex << '\n';
    std::cout << "protocol.constants=[-1,0,1,2,3] maximum_nodes=" << maximumNodes
              << " maximum_structures=" << maximumStructures
              << " maximum_composition_attempts=" << maximumCompositionAttempts
              << " maximum_training_point_evaluations=" << maximumTrainingPointEvaluations
              << " maximum_transition_steps=" << maximumTransitionSteps
              << " maximum_node_visits=" << maximumNodeVisits
              << " maximum_arithmetic_operations=" << maximumArithmeticOperations
              << " maximum_storage_bytes=" << maximumEstimatedStorageBytes
              << " maximum_steps=" << maximumSteps << '\n';
    std::cout << "protocol.ordinary_operators=[add,subtract,multiply,modulo]"
              << " gcd_operators=[gcd,add,subtract,multiply,modulo]\n";

    runArm("main_ordinary", false, training, heldOut, sequence);
    runArm("candidate_gcd", true, training, heldOut, sequence);
    return 0;
}
