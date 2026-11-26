#include <ConservationDiscovery.hpp>
#include <PolynomialConservationDiscovery.hpp>
#include <SymbolicRegression.hpp>

#include <cmath>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
Expression mul(const Expression& a, const Expression& b) {
    return Expression::binary(ExpressionKind::Multiply, a, b);
}
} // namespace

int main() {
    try {
        std::vector<ConservationTrajectory> trajectories;
        constexpr double pi = 3.1415926535897932384626433832795;
        for (const double radius : {1.0, 2.0, 3.0}) {
            ConservationTrajectory trajectory;
            for (int step = 0; step < 16; ++step) {
                const double t = 2.0 * pi * static_cast<double>(step) / 16.0;
                trajectory.states.push_back({radius * std::cos(t), -radius * std::sin(t)});
            }
            trajectories.push_back(std::move(trajectory));
        }
        const ConservationDiscoveryProblem problem(trajectories);
        const auto y = Expression::variable(0, 2), v = Expression::variable(1, 2);
        const auto energy = Expression::binary(ExpressionKind::Add, mul(y, y), mul(v, v));
        const auto scaledEnergy = mul(Expression::constant(0.5, 2), energy);
        const auto varying = mul(y, y);
        const auto constant = Expression::constant(1.0, 2);

        const auto conserved = problem.assess(energy);
        const auto scaled = problem.assess(scaledEnergy);
        const auto incorrect = problem.assess(varying);
        const auto degenerate = problem.assess(constant);
        check(conserved.score.state() == FitnessState::Valid && conserved.withinFraction &&
                  conserved.betweenFraction && conserved.separation &&
                  *conserved.withinFraction < 1e-20 && *conserved.betweenFraction > 0.99,
              "grouped harmonic trajectories reward low within variation and energy separation");
        check(scaled.score.state() == FitnessState::Valid && scaled.withinFraction &&
                  conserved.withinFraction &&
                  std::abs(*scaled.withinFraction - *conserved.withinFraction) < 1e-14 &&
                  scaled.score.score() >= conserved.score.score(),
              "shrinking a nonzero expression cannot improve its normalized objective");
        check(incorrect.score.state() == FitnessState::Valid &&
                  conserved.score.score() < incorrect.score.score(),
              "a varying within-trajectory expression receives a worse score");
        check(degenerate.score.state() == FitnessState::Invalid,
              "constant expressions have no between-trajectory separation and are rejected");

        const auto tiny = mul(Expression::constant(1e-14, 2), varying);
        check(problem.assess(tiny).score.state() == FitnessState::Invalid,
              "numerically degenerate shrinkage is rejected");
        const auto cancellation = Expression::binary(
            ExpressionKind::Add, Expression::constant(0.5, 2),
            Expression::binary(
                ExpressionKind::Add, v,
                Expression::binary(ExpressionKind::Subtract, Expression::constant(1.0, 2), v)));
        check(
            problem.assess(cancellation).score.state() == FitnessState::Invalid,
            "roundoff-only between-trajectory variation cannot make a constant appear informative");
        PolynomialConservationConfig polynomialConfig;
        const auto discovered = discoverPolynomialConservation(problem, polynomialConfig);
        check(discovered.found && discovered.degree == 2 && discovered.monomials == 5 &&
                  discovered.trainingAssessment.withinFraction &&
                  *discovered.trainingAssessment.withinFraction <= 1e-6,
              "public polynomial discovery selects the lowest valid degree from observations");
        check(discovered.expression.variableCount() == 2 && discovered.expression.nodeCount() <= 63,
              "polynomial discovery returns a bounded ordinary expression");
        check(discovered.expression.evaluate(std::array{2.0, 3.0}).value() == 13.0,
              "harmonic selection predicts the independent expected invariant value");
        check(discovered.cumulativeFeatureValues == 48 * (2 + 5),
              "public work reports every cumulative degree matrix");
        auto unsupportedConfig = polynomialConfig;
        unsupportedConfig.maximumDegree = 5;
        bool rejectedDegree = false;
        try {
            (void)discoverPolynomialConservation(problem, unsupportedConfig);
        } catch (const std::invalid_argument&) {
            rejectedDegree = true;
        }
        check(rejectedDegree, "polynomial discovery enforces its documented degree cap");
        auto exhaustedConfig = polynomialConfig;
        exhaustedConfig.maximumFeatureValues = 48 * (2 + 5) - 1;
        bool rejectedWork = false;
        try {
            (void)discoverPolynomialConservation(problem, exhaustedConfig);
        } catch (const std::length_error&) {
            rejectedWork = true;
        }
        check(rejectedWork, "polynomial discovery enforces its configured feature-work limit");
        std::vector<ConservationTrajectory> quarticGroups;
        for (const double level : {1.0, 2.0, 4.0}) {
            ConservationTrajectory group;
            const double extent = std::pow(level, 0.25);
            for (int i = 0; i <= 32; ++i) {
                const double q = -1.0 + 2.0 * static_cast<double>(i) / 32.0;
                const double yValue = extent * q;
                const double vMagnitude = std::sqrt(std::max(0.0, level - std::pow(yValue, 4)));
                group.states.push_back({yValue, vMagnitude});
                group.states.push_back({-yValue, -vMagnitude});
            }
            quarticGroups.push_back(std::move(group));
        }
        const ConservationDiscoveryProblem quarticProblem(quarticGroups);
        const auto quartic = discoverPolynomialConservation(quarticProblem, polynomialConfig);
        check(quartic.found && quartic.degree == 4 && quartic.trainingAssessment.withinFraction &&
                  *quartic.trainingAssessment.withinFraction <= 1e-6,
              "the same polynomial method selects a degree-four invariant when lower degrees fail");
        check(quartic.cumulativeFeatureValues == 30 * quarticProblem.sampleCount(),
              "degree-four public work sums all cumulative matrices, not only the final one");
        check(quartic.expression.evaluate(std::array{2.0, 3.0}).value() == 25.0,
              "quartic selection predicts the independent expected invariant value");

        std::vector<ConservationTrajectory> fixedCoordinateGroups;
        for (const double fixedY : {-2.0, 1.0, 2.0}) {
            ConservationTrajectory group;
            for (const double velocity : {-2.0, -1.0, 1.0, 2.0})
                group.states.push_back({fixedY, velocity});
            fixedCoordinateGroups.push_back(std::move(group));
        }
        const ConservationDiscoveryProblem fixedCoordinateProblem(fixedCoordinateGroups);
        auto linearConfig = polynomialConfig;
        linearConfig.maximumDegree = 1;
        const auto fixedCoordinate =
            discoverPolynomialConservation(fixedCoordinateProblem, linearConfig);
        check(fixedCoordinate.found && fixedCoordinate.degree == 1 &&
                  fixedCoordinate.trainingAssessment.withinFraction &&
                  *fixedCoordinate.trainingAssessment.withinFraction == 0.0,
              "a zero within-trajectory feature is tested directly as an invariant");
        const auto fixedAt = fixedCoordinate.expression.evaluate(std::array{-2.0, 0.0}).value();
        const auto fixedAtOther = fixedCoordinate.expression.evaluate(std::array{1.0, 0.0}).value();
        check(fixedAt != 0.0 && fixedAtOther / fixedAt == -0.5,
              "degree-one direct selection recovers the grouped fixed coordinate up to scale");
        std::vector<ConservationTrajectory> constantCoordinateGroups;
        for (const double velocityOffset : {-2.0, 1.0, 2.0}) {
            ConservationTrajectory group;
            for (const double velocity : {-1.0, 0.0, 1.0})
                group.states.push_back({7.0, velocity + velocityOffset});
            constantCoordinateGroups.push_back(std::move(group));
        }
        const ConservationDiscoveryProblem constantCoordinateProblem(constantCoordinateGroups);
        const auto constantCoordinate =
            discoverPolynomialConservation(constantCoordinateProblem, linearConfig);
        check(!constantCoordinate.found,
              "a coordinate constant globally is not accepted as a direct invariant");

        std::vector<ConservationTrajectory> affineGroups;
        for (const double invariantValue : {-5.0, 2.0, 8.0}) {
            ConservationTrajectory group;
            for (const double time : {-3.0, -1.0, 1.0, 3.0})
                group.states.push_back({time, invariantValue - 3.0 * time});
            affineGroups.push_back(std::move(group));
        }
        const ConservationDiscoveryProblem affineProblem(affineGroups);
        const auto affineLaw = discoverPolynomialConservation(affineProblem, linearConfig);
        check(affineLaw.found && affineLaw.degree == 1 &&
                  affineLaw.trainingAssessment.withinFraction &&
                  *affineLaw.trainingAssessment.withinFraction < 1e-24,
              "linear nullspace search reconstructs a shared rational coefficient ratio");
        for (const auto& group : affineGroups) {
            const double expected = group.states.front()[1] + 3.0 * group.states.front()[0];
            for (const auto& state : group.states)
                check(affineLaw.expression.evaluate(state).value() == expected,
                      "cleared integer coefficients preserve the exact grouped invariant");
        }
        std::cout << "linear shear selected expression: " << affineLaw.expression.toString()
                  << '\n';
        const ConservationDiscoveryProblem limitedAffineProblem(affineGroups,
                                                                ExpressionLimits{63, 8, 0.5});
        const auto unavailableAffine =
            discoverPolynomialConservation(limitedAffineProblem, linearConfig);
        check(!unavailableAffine.found && !unavailableAffine.exactRepresentationAvailable,
              "coefficient clearing reports when exact integer representation exceeds limits");
        std::uint64_t rows = 0;
        RegressionSupplementalWork work;
        const auto measured = problem.measureWithWork(energy, &rows, work);
        check(measured.loss == conserved.score && rows == trajectories.size() * 16 &&
                  measured.samplesAttempted == rows && work.fieldEvaluations == rows,
              "ordinary regression measurement hooks retain row and expression-work accounting");

        SymbolicRegressionConfig search;
        search.population = 8;
        search.elites = 1;
        search.maxGenerationAttempts = 1;
        search.maxEvaluations = 16;
        search.maxTrainingLossAttempts = 16 * problem.sampleCount();
        search.initialExpressionSlots = 2;
        search.maximumInitializerWorkUnits = 1;
        search.grammar.operators = {ExpressionKind::Add, ExpressionKind::Multiply};
        search.grammar.constants = {0.25, 1.0, 2.0};
        search.grammar.initialMaxDepth = 3;
        RandomContext random(7781);
        auto ownedProblem = std::make_shared<const ConservationDiscoveryProblem>(trajectories);
        InitialExpressionProvider initial = [](const RegressionProblem& p, std::size_t,
                                               std::uint64_t) {
            const auto a = Expression::binary(ExpressionKind::Multiply,
                                              Expression::variable(0, p.dimensions()),
                                              Expression::variable(0, p.dimensions()), p.limits());
            const auto b = Expression::binary(ExpressionKind::Multiply,
                                              Expression::variable(1, p.dimensions()),
                                              Expression::variable(1, p.dimensions()), p.limits());
            return InitialExpressionBatch{{a, b}, 1, "success", {}};
        };
        const auto searched = symbolicRegression(ownedProblem, search, random, {}, initial);
        check(searched.best && searched.candidateEvaluations == search.population * 2 &&
                  searched.variation.mutationProposals + searched.variation.crossoverProposals >
                      0 &&
                  searched.best->fitnessState() == FitnessState::Valid &&
                  searched.best->fitness() ==
                      ownedProblem->measure(searched.best->expression()).loss.score(),
              "ordinary symbolic search evaluates grouped conservation fitness through its public "
              "path");
        std::cout << "conservation objective contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
