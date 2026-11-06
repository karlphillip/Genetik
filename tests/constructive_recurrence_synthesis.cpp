#include <ConstructiveRecurrenceSynthesis.hpp>
#include <DiscreteExpressionParsing.hpp>

#include <iostream>
#include <stdexcept>

namespace {
using namespace genetik::constructive;
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
std::vector<SequencePoint> points(std::initializer_list<std::int64_t> values) {
    std::vector<SequencePoint> out;
    std::int64_t n = 0;
    for (auto value : values) out.push_back({n++, value});
    return out;
}
SynthesisResult synth(std::size_t order, std::vector<std::int64_t> initial,
                      std::vector<SequencePoint> training, std::size_t nodes = 7) {
    RecurrenceSynthesisConfig config;
    config.maximumNodes = nodes;
    return synthesizeRecurrenceTransition(order, initial, training, config);
}
void tests() {
    const auto ap = synth(1, {0}, points({0,3,6,9,12}));
    require(ap.transition && ap.stop == SynthesisStop::ExactTrainingMatch, "first order recovery");
    require(ap.trainingRecovery == TrainingRecoveryStatus::Exact && ap.matchedTrainingPoints == 5,
            "exact recovery status");
    const auto fib = synth(2, {0,1}, points({0,1,1,2,3,5,8}));
    require(fib.transition && fib.transition->nodeCount() == 3, "second order recovery");
    require(synth(1, {1}, points({1,2,4,8,16}), 3).transition.has_value(), "geometric recovery");
    require(synth(1, {0}, points({0,1,0,1,0,1})).transition.has_value(), "modular recovery");
    RecurrenceSynthesisConfig modularGrammar;
    modularGrammar.maximumNodes = 5;
    modularGrammar.operators = {DiscreteKind::Add, DiscreteKind::Modulo};
    require(synthesizeRecurrenceTransition(1, {0}, points({0,1,2,0,1,2,0,1}), modularGrammar)
                .transition.has_value(), "periodic modulo recovery");
    require(synth(1, {0}, points({0,1,3,6,10,15}), 5).transition.has_value(), "index binding");
    require(synth(1, {1}, points({1,1,2,6,24,120}), 7).transition.has_value(), "nonlinear state-index recovery");
    require(synth(2, {0,1}, points({0,1,2,5,12,29,70}), 7).transition.has_value(), "non-Fibonacci second order recovery");

    const auto first = synth(2, {0,1}, points({0,1,1,2,3,5,8}));
    const auto second = synth(2, {0,1}, points({0,1,1,2,3,5,8}));
    require(first.canonicalTransition == second.canonicalTransition &&
            first.stats.compositionAttempts == second.stats.compositionAttempts,
            "deterministic enumeration");
    require(first.stats.duplicateStructures == 0 &&
            first.stats.uniqueStructures <= first.stats.compositionAttempts,
            "stable structural enumeration has no duplicate candidates");
    IntegerRecurrence autonomous(0, {0,1}, *first.transition);
    require(autonomous.evaluate(7).valid() && autonomous.evaluate(7).value->integer() == 13,
            "held-out observation uses autonomous rollout after selection");
    const auto assessment = assessRecurrenceTransition(first, 2, {0,1}, {{7,13},{8,21}});
    require(assessment.status == AssessmentStatus::ExactHeldOut && assessment.matchedObservations == 2,
            "separate held-out assessment exact");
    require(toString(assessment.status) == "exact_held_out" && toString(first.stop) == "exact_training_match",
            "deterministic result diagnostics");
    require(assessRecurrenceTransition(first, 2, {0,1}, {{7,13},{8,99}}).status ==
                AssessmentStatus::PartialHeldOut,
            "separate held-out assessment partial");
    SynthesisResult noCandidate;
    require(assessRecurrenceTransition(noCandidate, 2, {0,1}, {{4,3}}).status ==
                AssessmentStatus::NoCandidate,
            "no-candidate held-out outcome distinct");
    SynthesisResult invalidSelection;
    invalidSelection.transition = parseDiscreteExpression("x1+1", 2);
    const auto invalidAssessment = assessRecurrenceTransition(invalidSelection, 1,
        {std::numeric_limits<std::int64_t>::max()}, {{1,0}});
    require(invalidAssessment.status == AssessmentStatus::InvalidCandidate,
            "invalid held-out candidate outcome distinct");
    RecurrenceAssessmentConfig assessmentLimits;
    assessmentLimits.maximumTransitionSteps = 1;
    require(assessRecurrenceTransition(first, 2, {0,1}, {{8,21}}, assessmentLimits).status ==
                AssessmentStatus::ResourceExhausted,
            "held-out assessment resource exhaustion distinct");
    auto config = RecurrenceSynthesisConfig{};
    config.maximumStructures = 2;
    auto limited = synthesizeRecurrenceTransition(2, {0,1}, points({0,1,1,2,3}), config);
    require(limited.stop == SynthesisStop::StructureLimit &&
            limited.trainingRecovery != TrainingRecoveryStatus::Exact,
            "structure exhaustion explicit");
    config = {};
    config.maximumArithmeticOperations = 1;
    limited = synthesizeRecurrenceTransition(2, {0,1}, points({0,1,1,2,3}), config);
    require(limited.stop == SynthesisStop::ArithmeticWorkLimit &&
            limited.trainingRecovery != TrainingRecoveryStatus::Exact &&
            limited.stats.arithmeticOperations <= config.maximumArithmeticOperations,
            "mathematical work exhaustion explicit");
    config = {};
    config.maximumCompositionAttempts = 1;
    limited = synthesizeRecurrenceTransition(2, {0,1}, points({0,1,1,2,3}), config);
    require(limited.stop == SynthesisStop::CompositionLimit &&
            limited.stats.compositionAttempts <= config.maximumCompositionAttempts,
            "candidate construction limit explicit");
    config = {};
    config.maximumTrainingPointEvaluations = 1;
    limited = synthesizeRecurrenceTransition(2, {0,1}, points({0,1,1,2,3}), config);
    require(limited.stop == SynthesisStop::TrainingPointLimit &&
            limited.stats.trainingPoints <= config.maximumTrainingPointEvaluations,
            "training point limit explicit");
    config = {};
    config.maximumTransitionSteps = 1;
    limited = synthesizeRecurrenceTransition(2, {0,1}, points({0,1,1,2,3}), config);
    require(limited.stop == SynthesisStop::TransitionLimit &&
            limited.stats.transitionSteps <= config.maximumTransitionSteps,
            "recurrence transition limit explicit");
    config = {};
    config.maximumNodeVisits = 1;
    limited = synthesizeRecurrenceTransition(2, {0,1}, points({0,1,1,2,3}), config);
    require(limited.stop == SynthesisStop::NodeVisitLimit &&
            limited.stats.nodeVisits <= config.maximumNodeVisits,
            "node visit limit explicit");
    config = {};
    config.maximumNodes = 1;
    limited = synthesizeRecurrenceTransition(2, {0,1}, points({0,1,1,2,3}), config);
    require(limited.stop == SynthesisStop::NodeLimit &&
            limited.trainingRecovery != TrainingRecoveryStatus::Exact,
            "node bound explicit");
    config = {};
    config.expressionLimits.maxDepth = 1;
    config.maximumCompositionAttempts = 100;
    limited = synthesizeRecurrenceTransition(2, {0,1}, points({0,1,1,2,3}), config);
    require(limited.stop == SynthesisStop::CompositionLimit &&
            limited.stats.compositionAttempts <= config.maximumCompositionAttempts &&
            limited.trainingRecovery != TrainingRecoveryStatus::Exact,
            "depth and failed construction attempts are explicitly bounded");
    config = {};
    config.maximumEstimatedStorageBytes = 1;
    limited = synthesizeRecurrenceTransition(2, {0,1}, points({0,1,1,2,3}), config);
    require(limited.stop == SynthesisStop::StorageLimit && !limited.transition &&
            limited.stats.estimatedStoredBytes <= config.maximumEstimatedStorageBytes,
            "memory estimate limit explicit");
    std::atomic<bool> cancelled{true};
    config = {};
    config.cancelled = &cancelled;
    limited = synthesizeRecurrenceTransition(2, {0,1}, points({0,1,1,2,3}), config);
    require(limited.stop == SynthesisStop::Cancelled && !limited.transition,
            "caller cancellation explicit");
    const auto noExact = synth(1, {100}, points({100, 7, 8, 15, 42}), 3);
    require(noExact.trainingRecovery != TrainingRecoveryStatus::Exact,
            "finite bounded grammar reports no exact match");
    RecurrenceSynthesisConfig terminalOnly;
    terminalOnly.maximumNodes = 1;
    const auto noSolution = synthesizeRecurrenceTransition(1, {0}, points({0,9}), terminalOnly);
    require(noSolution.trainingRecovery == TrainingRecoveryStatus::None && !noSolution.transition &&
            noSolution.stop == SynthesisStop::NodeLimit,
            "complete bounded enumeration with no exact candidate is explicit");
    config = {};
    config.operators.push_back(DiscreteKind::Negate);
    bool rejected = false;
    try { (void)synthesizeRecurrenceTransition(1, {0}, points({0,1,2}), config); }
    catch (const std::invalid_argument &) { rejected = true; }
    require(rejected, "operator outside qualified grammar rejected");
    IntegerRecurrence overflow(0, {std::numeric_limits<std::int64_t>::max()},
        parseDiscreteExpression("x1+1", 2));
    require(overflow.evaluate(1).status == DiscreteEvaluationStatus::MathematicalInvalid,
            "checked Int64 overflow");
    IntegerRecurrence invalidModulo(0, {2}, parseDiscreteExpression("x1%0", 2));
    require(invalidModulo.evaluate(1).status == DiscreteEvaluationStatus::MathematicalInvalid,
            "invalid mathematical operation remains explicit");
}
}
int main() {
    try { tests(); std::cout << "constructive recurrence contracts passed\n"; return 0; }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
