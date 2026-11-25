#include <MinimumDescriptionLength.hpp>

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
}

int main() {
    try {
        check(MinimumDescriptionLengthResult::formulation ==
                  "genetik-mdl-fixed-precision-signed-elias-gamma/1",
              "formulation has a stable identifying name");
        MinimumDescriptionLengthConfig config;
        config.coefficientPrecision = 1.0;
        config.residualPrecision = 1.0;
        const std::array models{
            MinimumDescriptionLengthCandidate{Expression::constant(0.0, 1)},
            MinimumDescriptionLengthCandidate{Expression::variable(0)}};

        // Independent code-length arithmetic: each model has header 8, one
        // three-bit node tag and gamma(1)=1, hence 12 model bits. A zero
        // residual costs gamma(1)=1; residual integers 1 and 2 cost 3 and 5.
        const auto constantData = RegressionProblem::create(
            {{{0.0}, 0.0}, {{1.0}, 0.0}, {{2.0}, 0.0}}, {31, 6, 8});
        const auto constant = selectMinimumDescriptionLength(*constantData, models, config);
        check(constant.status == MinimumDescriptionLengthStatus::Success &&
                  constant.selectedIndex == 0 && constant.candidates[0].modelBits == 12 &&
                  constant.candidates[0].residualBits == 3 &&
                  constant.candidates[0].totalBits == 15 &&
                  constant.candidates[1].residualBits == 9 &&
                  constant.candidates[1].totalBits == 21 && constant.selectedModel &&
                  constant.selectedModel->nodes().front().constantValue() == 0.0,
              "first fixture independently selects the constant with exact code lengths");

        const auto linearData = RegressionProblem::create(
            {{{0.0}, 0.0}, {{1.0}, 1.0}, {{2.0}, 2.0}}, {31, 6, 8});
        const auto linear = selectMinimumDescriptionLength(*linearData, models, config);
        check(linear.status == MinimumDescriptionLengthStatus::Success &&
                  linear.selectedIndex == 1 && linear.candidates[1].modelBits == 12 &&
                  linear.candidates[1].residualBits == 3 &&
                  linear.candidates[1].totalBits == 15 &&
                  linear.candidates[0].residualBits == 9 &&
                  linear.candidates[0].totalBits == 21,
              "second fixture reverses the winner with independently calculated code lengths");

        MinimumDescriptionLengthConfig limited = config;
        limited.maximumBits = 10;
        const auto exhausted = selectMinimumDescriptionLength(*linearData, models, limited);
        check(exhausted.status == MinimumDescriptionLengthStatus::ResourceExhausted &&
                  !exhausted.selectedIndex,
              "resource exhaustion never returns a partial winner");

        const std::array unsupported{MinimumDescriptionLengthCandidate{
            Expression::binary(ExpressionKind::Divide, Expression::variable(0),
                               Expression::constant(2.0, 1))}};
        const auto unsupportedModel =
            selectMinimumDescriptionLength(*linearData, unsupported, config);
        check(!unsupportedModel.selectedIndex &&
                  unsupportedModel.candidates[0].status ==
                      MinimumDescriptionLengthStatus::InvalidModel,
              "non-polynomial operators are outside the certified code model");
        std::cout << "minimum-description-length contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
