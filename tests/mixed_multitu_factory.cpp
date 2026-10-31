#include <MixedExpression.hpp>

MixedExpression makeMixedAcrossTranslationUnits() {
    const std::array nodes{
        MixedNode::input(0), MixedNode::operation(MixedKind::IntToReal),
        MixedNode::real(2.3), MixedNode::operation(MixedKind::RMultiply),
        MixedNode::real(1.7), MixedNode::operation(MixedKind::RAdd),
        MixedNode::operation(MixedKind::FloorToInt)};
    return MixedExpression(nodes, 1);
}
