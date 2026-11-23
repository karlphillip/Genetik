#include <ConstructiveRecurrenceSynthesis.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

using namespace genetik::constructive;

namespace {
using Monomial = std::array<unsigned, 3>;
using Polynomial = std::map<Monomial, std::int64_t>;

void add(Polynomial& to, const Polynomial& from, std::int64_t sign = 1) {
    for (const auto& [m, c] : from) {
        auto& slot = to[m];
        slot += sign * c;
        if (!slot)
            to.erase(m);
    }
}

std::optional<Polynomial> integerPolynomial(const DiscreteExpression& e) {
    std::vector<Polynomial> stack;
    for (const auto& node : e.nodes()) {
        if (node.kind() == DiscreteKind::Integer) {
            Polynomial p;
            if (node.integer())
                p[{}] = node.integer();
            stack.push_back(std::move(p));
        } else if (node.kind() == DiscreteKind::Variable) {
            Polynomial p;
            Monomial m{};
            m.at(node.variable()) = 1;
            p[m] = 1;
            stack.push_back(std::move(p));
        } else if (node.kind() == DiscreteKind::Add || node.kind() == DiscreteKind::Subtract ||
                   node.kind() == DiscreteKind::Multiply) {
            if (stack.size() < 2)
                return std::nullopt;
            auto rhs = std::move(stack.back());
            stack.pop_back();
            auto lhs = std::move(stack.back());
            stack.pop_back();
            if (node.kind() == DiscreteKind::Add || node.kind() == DiscreteKind::Subtract) {
                add(lhs, rhs, node.kind() == DiscreteKind::Add ? 1 : -1);
                stack.push_back(std::move(lhs));
            } else {
                Polynomial product;
                for (const auto& [a, ca] : lhs)
                    for (const auto& [b, cb] : rhs) {
                        Monomial m{};
                        for (std::size_t i = 0; i < m.size(); ++i)
                            m[i] = a[i] + b[i];
                        product[m] += ca * cb;
                    }
                stack.push_back(std::move(product));
            }
        } else
            return std::nullopt;
    }
    if (stack.size() != 1)
        return std::nullopt;
    return stack.back();
}

enum class Proof { Proved, Counterexample, Inconclusive };

Proof inductionCheck(const DiscreteExpression& transition) {
    const auto actual = integerPolynomial(transition);
    // The public recurrence binds x0 to the transition step i, x1 to the
    // older state, and x2 to the newest state. For squares the identity is
    // s(i+2) = s(i+1) + 2*i + 3.
    Polynomial expected;
    expected[Monomial{1, 0, 0}] = 2;
    expected[Monomial{0, 0, 1}] = 1;
    expected[Monomial{}] = 3;
    if (actual && *actual == expected)
        return Proof::Proved;

    IntegerRecurrence model(0, {0, 1}, transition);
    for (std::int64_t n = 2; n <= 128; ++n) {
        const auto value = model.evaluate(n);
        if (!value.valid())
            return Proof::Inconclusive;
        const auto target = n * n;
        if (value.value->integer() != target)
            return Proof::Counterexample;
    }
    return Proof::Inconclusive;
}

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
    std::cout << "control.expression=" << control.toString() << "\n"
              << "control.held_out=" << toString(controlAssessment.status)
              << " matched=" << controlAssessment.matchedObservations << "/2"
              << " induction=" << name(controlProof) << "\n";
    return found.trainingRecovery == TrainingRecoveryStatus::Exact &&
                   assessment.status == AssessmentStatus::ExactHeldOut && proof == Proof::Proved &&
                   controlAssessment.status == AssessmentStatus::PartialHeldOut &&
                   controlProof == Proof::Counterexample
               ? 0
               : 1;
}
