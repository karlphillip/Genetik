#include <ConstructiveRecurrenceSynthesis.hpp>
#include <DiscreteExpressionParsing.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

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
void requireExact(std::string_view label, const SynthesisResult &result,
                 const std::vector<SequencePoint> &training, std::size_t order,
                 const std::vector<std::int64_t> &initial,
                 const std::vector<SequencePoint> &heldOut) {
    require(result.trainingRecovery == TrainingRecoveryStatus::Exact,
            "successful discovery must report exact training recovery");
    require(result.stop == SynthesisStop::ExactTrainingMatch,
            "successful discovery must stop at exact training match");
    require(result.matchedTrainingPoints == training.size(),
            "exact discovery must match every training observation");
    require(result.transition.has_value(), "exact discovery must return a transition");
    require(result.expressionNodes == result.transition->nodeCount(),
            "reported expression complexity must match returned transition");
    require(result.stats.selectedModelBytes > 0,
            "selected model storage estimate must describe the returned transition");
    require(result.stats.selectedModelBytes == detail::selectedEstimate(
                *result.transition, result.canonicalTransition),
            "selected model byte estimate must match the returned expression");
    require(result.stats.estimatedPeakBytes <= RecurrenceSynthesisConfig{}.maximumEstimatedStorageBytes,
            "reported estimated storage peak must obey the configured cap");
    const auto assessment = assessRecurrenceTransition(result, order, initial, heldOut);
    if (assessment.status != AssessmentStatus::ExactHeldOut ||
        assessment.matchedObservations != heldOut.size())
        throw std::runtime_error(std::string(label) +
            ": independent autonomous held-out rollout did not match expected values (" +
            result.canonicalTransition + ", " + std::string(toString(assessment.status)) + ", " +
            assessment.diagnostic + ")");
}
void requireInvalidAssessment(const SynthesisResult &selected, std::size_t order,
                              const std::vector<std::int64_t> &initial,
                              const std::vector<SequencePoint> &heldOut,
                              RecurrenceAssessmentConfig config = {}) {
    bool rejected = false;
    try { (void)assessRecurrenceTransition(selected, order, initial, heldOut, config); }
    catch (const std::invalid_argument &) { rejected = true; }
    require(rejected, "malformed assessment input must be rejected even without a candidate");
}
void tests() {
    const auto apTraining = points({0,3,6,9,12,15});
    const auto ap = synth(1, {0}, apTraining);
    requireExact("arithmetic progression", ap, apTraining, 1, {0}, {{6,18},{7,21}});
    const auto shortTraining = points({0,0});
    const auto exactWithoutPartial = synth(1, {0}, shortTraining, 1);
    requireExact("exact recovery without preceding partial", exactWithoutPartial,
                 shortTraining, 1, {0}, {{2,1}});
    require(exactWithoutPartial.stats.selectedModelReplacements == 0,
            "first selected model must be accounted without a prior selection");
    const auto fib = synth(2, {0,1}, points({0,1,1,2,3,5,8}));
    const auto fibTraining = points({0,1,1,2,3,5,8});
    requireExact("Fibonacci", fib, fibTraining, 2, {0,1}, {{7,13},{8,21}});
    const auto geometricTraining = points({1,2,4,8,16});
    const auto geometric = synth(1, {1}, geometricTraining, 3);
    requireExact("geometric progression", geometric, geometricTraining, 1, {1}, {{5,32},{6,64}});
    const auto modularTraining = points({0,1,0,1,0,1});
    const auto modular = synth(1, {0}, modularTraining);
    requireExact("modular recurrence", modular, modularTraining, 1, {0}, {{6,0},{7,1}});
    RecurrenceSynthesisConfig modularGrammar;
    modularGrammar.maximumNodes = 5;
    modularGrammar.operators = {DiscreteKind::Add, DiscreteKind::Modulo};
    const auto periodicTraining = points({0,1,2,0,1,2,0,1});
    const auto periodic = synthesizeRecurrenceTransition(1, {0}, periodicTraining, modularGrammar);
    requireExact("periodic modular recurrence", periodic, periodicTraining, 1, {0}, {{8,2},{9,0}});
    const auto triangularTraining = points({0,1,3,6,10,15});
    const auto triangular = synth(1, {0}, triangularTraining, 5);
    requireExact("triangular numbers", triangular, triangularTraining, 1, {0}, {{6,21},{7,28}});
    const auto nonlinearTraining = points({1,1,2,6,24,120});
    const auto nonlinear = synth(1, {1}, nonlinearTraining, 7);
    requireExact("nonlinear recurrence", nonlinear, nonlinearTraining, 1, {1}, {{6,720},{7,5040}});
    const auto nonFibonacciTraining = points({0,1,2,5,12,29,70});
    const auto nonFibonacci = synth(2, {0,1}, nonFibonacciTraining, 7);
    requireExact("non-Fibonacci second order", nonFibonacci, nonFibonacciTraining, 2, {0,1}, {{7,169},{8,408}});
    require(ap.stats.selectedModelReplacements > 0,
            "exact recovery after an earlier partial model accounts replacement peak");
    RecurrenceSynthesisConfig exactStorageConfig;
    exactStorageConfig.maximumEstimatedStorageBytes = ap.stats.estimatedPeakBytes - 1;
    const auto exactReplacementLimited = synthesizeRecurrenceTransition(
        1, {0}, apTraining, exactStorageConfig);
    require(exactReplacementLimited.stop == SynthesisStop::StorageLimit &&
            exactReplacementLimited.trainingRecovery != TrainingRecoveryStatus::Exact &&
            exactReplacementLimited.stats.estimatedPeakBytes <=
                exactStorageConfig.maximumEstimatedStorageBytes,
            "storage admission rejects a model replacement whose estimated peak exceeds the cap");
    if (exactReplacementLimited.transition)
        require(exactReplacementLimited.stats.selectedModelBytes == detail::selectedEstimate(
                    *exactReplacementLimited.transition,
                    exactReplacementLimited.canonicalTransition),
                "storage exhaustion retains accurate accounting for its prior partial model");
    else
        require(exactReplacementLimited.stats.selectedModelBytes == 0,
                "storage exhaustion without a selected model reports zero selected bytes");

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
    requireInvalidAssessment(noCandidate, 3, {0,1,2}, {{4,3}});
    requireInvalidAssessment(noCandidate, 2, {0}, {{4,3}});
    requireInvalidAssessment(noCandidate, 2, {0,1}, {});
    requireInvalidAssessment(noCandidate, 2, {0,1}, {{-1,3}});
    requireInvalidAssessment(noCandidate, 2, {0,1}, {{5,3},{4,2}});
    auto invalidAssessmentLimits = RecurrenceAssessmentConfig{};
    invalidAssessmentLimits.maximumTransitionSteps = 0;
    requireInvalidAssessment(noCandidate, 2, {0,1}, {{4,3}}, invalidAssessmentLimits);
    invalidAssessmentLimits = {};
    invalidAssessmentLimits.maximumObservations = 0;
    requireInvalidAssessment(noCandidate, 2, {0,1}, {{4,3}}, invalidAssessmentLimits);
    invalidAssessmentLimits = {};
    invalidAssessmentLimits.evaluationLimits.maxWorkspaceBytes = 0;
    requireInvalidAssessment(noCandidate, 2, {0,1}, {{4,3}}, invalidAssessmentLimits);
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
            limited.stats.estimatedStoredBytes <= config.maximumEstimatedStorageBytes &&
            limited.stats.selectedModelBytes == 0 && limited.stats.estimatedPeakBytes <=
                config.maximumEstimatedStorageBytes,
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
    const auto partialTraining = points({0,1,2,5,12,29,71});
    RecurrenceSynthesisConfig partialConfig;
    partialConfig.maximumNodes = 3;
    const auto replacedPartial = synthesizeRecurrenceTransition(1, {0}, partialTraining, partialConfig);
    require(replacedPartial.trainingRecovery == TrainingRecoveryStatus::Partial &&
            replacedPartial.stats.selectedModelReplacements > 0 &&
            replacedPartial.stats.selectedModelBytes == detail::selectedEstimate(
                *replacedPartial.transition, replacedPartial.canonicalTransition),
            "replacement of one partial model with a better partial model updates storage stats");
    partialConfig.maximumNodes = 1;
    const auto storageLimitedTraining = points({0,1,5});
    const auto storageBaseline = synthesizeRecurrenceTransition(
        1, {0}, storageLimitedTraining, partialConfig);
    require(storageBaseline.trainingRecovery == TrainingRecoveryStatus::Partial &&
            storageBaseline.stats.estimatedPeakBytes > 1,
            "storage-limit fixture first retains a deterministic partial model");
    partialConfig.maximumEstimatedStorageBytes = storageBaseline.stats.estimatedPeakBytes - 1;
    const auto resourceLimitedPartial = synthesizeRecurrenceTransition(
        1, {0}, storageLimitedTraining, partialConfig);
    require(resourceLimitedPartial.stop == SynthesisStop::StorageLimit &&
            resourceLimitedPartial.trainingRecovery == TrainingRecoveryStatus::Partial &&
            resourceLimitedPartial.transition && resourceLimitedPartial.stats.selectedModelBytes ==
                detail::selectedEstimate(*resourceLimitedPartial.transition,
                                         resourceLimitedPartial.canonicalTransition) &&
            resourceLimitedPartial.stats.estimatedPeakBytes <=
                partialConfig.maximumEstimatedStorageBytes,
            "storage exhaustion retains and accounts the selected partial model");
    RecurrenceSynthesisConfig terminalOnly;
    terminalOnly.maximumNodes = 1;
    const auto noSolution = synthesizeRecurrenceTransition(1, {0}, points({0,9}), terminalOnly);
    require(noSolution.trainingRecovery == TrainingRecoveryStatus::None && !noSolution.transition &&
            noSolution.stop == SynthesisStop::NodeLimit && noSolution.stats.selectedModelBytes == 0,
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
