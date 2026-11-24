#include <ConstructiveRecurrenceSynthesis.hpp>
#include <DiscreteExpressionParsing.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <numeric>
#include <optional>
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

std::string_view evaluationStatusName(DiscreteEvaluationStatus status) {
    switch (status) {
    case DiscreteEvaluationStatus::Valid: return "valid";
    case DiscreteEvaluationStatus::MathematicalInvalid: return "mathematical_invalid";
    case DiscreteEvaluationStatus::ResourceExhausted: return "resource_exhausted";
    case DiscreteEvaluationStatus::Cancelled: return "cancelled";
    case DiscreteEvaluationStatus::NoWitness: return "no_witness";
    }
    return "unknown";
}

bool hasValidInteger(const DiscreteEvaluationResult& evaluation) {
    return evaluation.valid() && evaluation.value &&
           evaluation.value->type() == DiscreteType::Int64;
}

void printDivergence(std::string_view prefix, std::int64_t index, std::int64_t expected,
                     const DiscreteEvaluationResult& evaluation) {
    std::cout << prefix << "first_divergence_index=" << index << " expected=" << expected
              << " evaluation_status=" << evaluationStatusName(evaluation.status) << " actual=";
    if (hasValidInteger(evaluation))
        std::cout << evaluation.value->integer() << '\n';
    else
        std::cout << "unavailable\n";
}

bool sameUnderAddAndGcdSwaps(const DiscreteExpression& left, const DiscreteExpression& right) {
    if (left.dimensions() != right.dimensions()) return false;
    std::function<bool(std::size_t, std::size_t)> equal = [&](std::size_t a, std::size_t b) {
        const auto an = left.nodes()[a];
        const auto bn = right.nodes()[b];
        if (an.kind() != bn.kind()) return false;
        const auto arity = discreteArity(an.kind());
        if (arity == 0) return an == bn;
        if (arity != 2) return false;
        const auto ac = left.children(a);
        const auto bc = right.children(b);
        if (equal(ac[0], bc[0]) && equal(ac[1], bc[1])) return true;
        if (an.kind() == DiscreteKind::Add || an.kind() == DiscreteKind::Gcd)
            return equal(ac[0], bc[1]) && equal(ac[1], bc[0]);
        return false;
    };
    return equal(left.nodeCount() - 1, right.nodeCount() - 1);
}

std::string_view rowlandRecognition(const DiscreteExpression& expression,
                                    const DiscreteExpression& reference) {
    return sameUnderAddAndGcdSwaps(expression, reference) ? "equivalent" : "unrecognized";
}

bool verifyAssessmentControls() {
    const auto reference = parseDiscreteExpression("x1 + gcd(x0 + 2, x1)", 2);
    const auto discovered = parseDiscreteExpression("gcd(x1, 2 + x0) + x1", 2);
    const auto different = parseDiscreteExpression("x1 + gcd(x0 + 3, x1)", 2);
    const auto equivalent = rowlandRecognition(discovered, reference) == "equivalent";
    const auto differentUnrecognized = rowlandRecognition(different, reference) == "unrecognized";
    const auto moduloByZero = parseDiscreteExpression("x0 % 0", 1).evaluate(
        std::array<std::int64_t, 1>{0});
    const auto validZero = parseDiscreteExpression("0", 1).evaluate(
        std::array<std::int64_t, 1>{0});
    const bool validZeroHasInteger = hasValidInteger(validZero);
    const bool statusesDistinguished =
        moduloByZero.status == DiscreteEvaluationStatus::MathematicalInvalid &&
        !moduloByZero.value && validZero.status == DiscreteEvaluationStatus::Valid &&
        validZeroHasInteger && validZero.value->integer() == 0;
    std::cout << "control.discovered_transition=" << rowlandRecognition(discovered, reference)
              << " expression=" << "gcd(x1, (2 + x0)) + x1" << '\n';
    std::cout << "control.different_transition=" << rowlandRecognition(different, reference)
              << " expression=" << "x1 + gcd((x0 + 3), x1)" << '\n';
    std::cout << "control.modulo_by_zero status=" << evaluationStatusName(moduloByZero.status)
              << " value=unavailable\n";
    std::cout << "control.valid_zero status=" << evaluationStatusName(validZero.status);
    if (validZeroHasInteger)
        std::cout << " value=" << validZero.value->integer() << '\n';
    else
        std::cout << " value=unavailable\n";
    printDivergence("control.modulo_by_zero ", 0, 0, moduloByZero);
    printDivergence("control.valid_zero ", 0, 1, validZero);
    return equivalent && differentUnrecognized && statusesDistinguished;
}

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
                  << " reference_transition=unrecognized\n";
        return;
    }

    const auto assessment = assessRecurrenceTransition(found, 1, {7}, heldOut);
    std::int64_t firstMismatch = -1;
    std::optional<std::int64_t> actualAtMismatch;
    DiscreteEvaluationStatus mismatchStatus = DiscreteEvaluationStatus::Valid;
    const IntegerRecurrence selectedModel(0, {7}, *found.transition);
    for (std::int64_t index = 0; index <= static_cast<std::int64_t>(lastHeldOutIndex); ++index) {
        const auto predicted = selectedModel.evaluate(index);
        if (!hasValidInteger(predicted) || predicted.value->integer() !=
                reference.at(static_cast<std::size_t>(index))) {
            firstMismatch = index;
            if (hasValidInteger(predicted))
                actualAtMismatch = predicted.value->integer();
            mismatchStatus = predicted.status;
            break;
        }
    }
    // Constructed only after selection. This recognizes only recursive operand
    // swaps at Add and GCD nodes; all other structures remain unrecognized.
    std::string_view recognition = "unrecognized";
    if (enableGcd) {
        const auto referenceTransition = parseDiscreteExpression("x1 + gcd(x0 + 2, x1)", 2);
        recognition = rowlandRecognition(*found.transition, referenceTransition);
    }
    const auto exactTraining = found.trainingRecovery == TrainingRecoveryStatus::Exact;
    const auto exactHeldOut = assessment.status == AssessmentStatus::ExactHeldOut;
    std::cout << "arm=" << label << " exact_training_match=" << (exactTraining ? "yes" : "no")
              << " exact_held_out_match=" << (exactHeldOut ? "yes" : "no")
              << " held_out=" << toString(assessment.status)
              << " matched=" << assessment.matchedObservations << '/' << heldOut.size();
    if (firstMismatch >= 0) {
        std::cout << '\n';
        DiscreteEvaluationResult mismatch;
        mismatch.status = mismatchStatus;
        if (actualAtMismatch) mismatch.value = DiscreteValue::integer(*actualAtMismatch);
        printDivergence("arm=" + std::string(label) + " ", firstMismatch,
                        reference.at(static_cast<std::size_t>(firstMismatch)), mismatch);
    } else {
        std::cout << " first_divergence=none";
    }
    std::cout << " reference_transition=" << recognition << '\n';
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
    // These reference-based controls run after both searches, just like the
    // assessment of each returned model.
    if (!verifyAssessmentControls()) return 2;
    return 0;
}
