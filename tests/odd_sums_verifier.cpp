#include "../benchmarks/odd_sums_verifier.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

using namespace genetik::constructive;

namespace {
DiscreteExpression expression(std::initializer_list<DiscreteNode> nodes) {
    return DiscreteExpression(std::vector<DiscreteNode>(nodes), 3);
}
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
} // namespace

int main() {
    using odd_sums_verifier::Proof;
    const auto sum =
        expression({DiscreteNode::variable(2), DiscreteNode::integer(2), DiscreteNode::variable(0),
                    DiscreteNode::operation(DiscreteKind::Multiply),
                    DiscreteNode::operation(DiscreteKind::Add), DiscreteNode::integer(3),
                    DiscreteNode::operation(DiscreteKind::Add)});
    const auto secondDifference =
        expression({DiscreteNode::integer(2), DiscreteNode::variable(2),
                    DiscreteNode::operation(DiscreteKind::Multiply), DiscreteNode::variable(1),
                    DiscreteNode::operation(DiscreteKind::Subtract), DiscreteNode::integer(2),
                    DiscreteNode::operation(DiscreteKind::Add)});
    require(odd_sums_verifier::inductionCheck(sum) == Proof::Proved,
            "x2 + 2*x0 + 3 should prove by induction");
    require(odd_sums_verifier::inductionCheck(secondDifference) == Proof::Proved,
            "2*x2 - x1 + 2 should prove by induction");
    const auto tooLarge =
        expression({DiscreteNode::integer(INT64_MAX), DiscreteNode::variable(2),
                    DiscreteNode::operation(DiscreteKind::Multiply), DiscreteNode::variable(2),
                    DiscreteNode::operation(DiscreteKind::Multiply)});
    require(odd_sums_verifier::inductionCheck(tooLarge) == Proof::Inconclusive,
            "coefficient overflow should be inconclusive");
    return 0;
}
