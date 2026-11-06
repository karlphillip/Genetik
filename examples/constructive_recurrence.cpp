#include <ConstructiveRecurrenceSynthesis.hpp>

#include <iostream>
#include <string_view>

namespace {
using namespace genetik::constructive;

std::vector<SequencePoint> observations(std::initializer_list<std::int64_t> values) {
    std::vector<SequencePoint> result;
    std::int64_t index = 0;
    for (const auto value : values) result.push_back({index++, value});
    return result;
}

void discover(std::string_view name, std::size_t order, std::vector<std::int64_t> initial,
              std::vector<SequencePoint> training, std::vector<SequencePoint> heldOut) {
    RecurrenceSynthesisConfig limits;
    limits.maximumNodes = 7;
    const auto result = synthesizeRecurrenceTransition(order, initial, training, limits);
    const auto assessment = assessRecurrenceTransition(result, order, initial, heldOut);
    const bool success = result.trainingRecovery == TrainingRecoveryStatus::Exact &&
        result.stop == SynthesisStop::ExactTrainingMatch &&
        assessment.status == AssessmentStatus::ExactHeldOut;
    std::cout << name << ": ";
    if (result.transition) {
        std::cout << result.canonicalTransition << " | training=" << toString(result.trainingRecovery);
    } else {
        std::cout << "no transition | training=" << toString(result.trainingRecovery);
    }
    std::cout << " | stop=" << toString(result.stop)
              << " | held_out=" << toString(assessment.status)
              << " | success=" << (success ? "yes" : "no")
              << " | structures=" << result.stats.uniqueStructures
              << " evaluations=" << result.stats.candidateRollouts
              << " training_points=" << result.stats.trainingPoints
              << " transitions=" << result.stats.transitionSteps
              << " model_replacements=" << result.stats.selectedModelReplacements
              << " nodes=" << result.stats.nodeVisits
              << " operations=" << result.stats.arithmeticOperations << std::endl;
}
}

int main() {
    discover("arithmetic progression", 1, {0}, observations({0, 3, 6, 9, 12, 15}),
             {{6, 18}, {7, 21}});
    discover("triangular numbers", 1, {0}, observations({0, 1, 3, 6, 10, 15, 21}),
             {{7, 28}, {8, 36}});
    discover("Fibonacci", 2, {0, 1}, observations({0, 1, 1, 2, 3, 5, 8, 13}),
             {{8, 21}, {9, 34}});
    discover("non-Fibonacci second order", 2, {0, 1}, observations({0, 1, 2, 5, 12, 29, 70}),
             {{7, 169}, {8, 408}});
}
