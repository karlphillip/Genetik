#include <DiscreteExpressionComponents.hpp>
#include <DiscreteExpressionParsing.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>

int main() {
    try {
        DiscreteExpressionComponents components(1);
        const auto x = components.addExpression(DiscreteExpression::variable(0, 1));
        const auto five = components.addExpression(DiscreteExpression::integer(5, 1));
        const auto residue = components.binary(DiscreteKind::Modulo, x, five);
        const auto condition = components.addExpression(parseDiscreteExpression("x0 < 0", 1));
        const auto positiveMagnitude = components.binary(DiscreteKind::Subtract, five, x);
        const auto root = components.conditional(condition, positiveMagnitude, residue);

        const auto model = components.materialize(root);
        if (!model.success())
            throw std::runtime_error(model.diagnostic);

        std::cout << "Shared component structure: " << components.toString(root) << '\n';
        std::cout << "Unique components=" << components.storageStats().components
                  << " edges=" << components.storageStats().edges
                  << " expanded nodes=" << model.expansion.nodes
                  << " materialization work=" << model.work.totalUnits << '\n';
        std::cout << "Ordinary expression: " << model.expression->toString() << '\n';

        const std::array<std::pair<std::int64_t, std::int64_t>, 4> cases{{
            {-3, 8}, {-1, 6}, {0, 0}, {7, 2}}};
        for (const auto& [input, expected] : cases) {
            const auto result = model.expression->evaluate(std::array{input});
            if (!result.valid() || result.value->integer() != expected)
                throw std::runtime_error("model disagrees with the independent expected value");
            std::cout << "x=" << input << " result=" << result.value->integer()
                      << " visits=" << result.nodeVisits
                      << " operations=" << result.operations << '\n';
        }

        // This representation shares structure while being built. Materializing
        // intentionally expands each use; the normal evaluator does not memoize.
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
