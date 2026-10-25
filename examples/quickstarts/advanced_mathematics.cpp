#include <ScalarMathematicsRun.hpp>
#include <IntegerRecurrence.hpp>
#include <SymbolicAlgebra.hpp>
#include <ExpressionParsing.hpp>
#include <DiscreteExpressionParsing.hpp>
#include <iostream>
int main(int argc, char **argv) {
    try {
        unsigned workers = 1;
        if (argc == 3 && std::string_view(argv[1]) == "--threads")
            workers = static_cast<unsigned>(std::stoul(argv[2]));
        else if (argc != 1)
            throw std::invalid_argument("usage: advanced_mathematics_quickstart [--threads N]");
        auto integer = parseDiscreteExpression("modular_power(-2,63,97)", 0);
        std::cout << integer.toString() << " = " << integer.evaluate({}).value->integer() << '\n';
        auto witness = parseDiscreteExpression("first(k0,1,10,k0*k0>=x0)", 1);
        auto found = witness.evaluate(std::array<std::int64_t, 1>{17});
        if (!found.valid() || found.value->integer() != 5)
            return 1;
        std::cout << witness.toString() << " = 5 for x0=17\n";
        IntegerRecurrence fibonacci(0, {0, 1}, parseDiscreteExpression("x1+x2", 3));
        if (fibonacci.evaluate(20).value->integer() != 6765)
            return 1;
        std::cout << "F(20) = 6765\n";
        auto scalar = parseExpression("tanh(x0)+powi(x0,-2)", 1);
        auto partial = evaluatePartial(scalar, std::array{2.}, 0);
        if (!partial.derivativeAvailable())
            return 1;
        std::cout << scalar.toString() << " value=" << partial.value() << " partial=" << partial.derivative()
                  << '\n';
        auto mathematical = SymbolicExpression::fromExpression(parseExpression("(x0+2)*(x0-2)", 1));
        auto expanded = mathematical.expandPolynomial();
        auto derivative = mathematical.differentiate(0);
        if (!expanded.expression || !derivative.expression)
            return 1;
        std::cout << "Mathematical polynomial: " << expanded.expression->toString()
                  << "\nDerivative under recorded assumptions: " << derivative.expression->toString() << '\n';
        auto L = PhysicalDimension::base(SIBaseDimension::Length),
             T = PhysicalDimension::base(SIBaseDimension::Time);
        DimensionalExpression velocity(parseExpression("x0/x1", 2), {L, T});
        std::cout << "Velocity: " << velocity.toString() << '\n';
        auto restored = restoreDimensionalExpression(
            decodeCheckpoint(encodeCheckpoint(checkpointDimensionalExpression(velocity))));
        if (restored.dimension() != velocity.dimension())
            return 1;
        MathematicalConstraints policy({VariableDomain{0., 10., true, true}},
                                       {{ExpressionKind::Log, 0, true, false}}, {}, 0, true);
        if (policy.assess(parseExpression("log(x0)", 1), std::array{2.}).status !=
            ConstraintStatus::Satisfied)
            return 1;
        // Opt-in scientific evolution: no advanced operations added to legacy defaults.
        auto problem = ScalarMathematicsProblem::scientific(
            {{{-.5}, -.46211715726000976}, {{.5}, .46211715726000976}}, {15, 5, 8});
        SymbolicRegressionConfig config;
        config.population = 8;
        config.maxGenerationAttempts = 2;
        config.maxEvaluations = 24;
        config.maxTrainingLossAttempts = 24;
        config.grammar.operators = {ExpressionKind::Add, ExpressionKind::Tanh};
        RandomContext random(42, {workers});
        ScalarMathematicsRun run(problem, config, random);
        while (!run.stoppingReason())
            run.step();
        std::cout << "Bounded discovery: " << searchStopName(*run.stoppingReason()) << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
