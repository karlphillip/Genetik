#include <MixedExpression.hpp>
#include <stdexcept>

MixedExpression makeMixedAcrossTranslationUnits();

int main() {
    auto expression = makeMixedAcrossTranslationUnits();
    std::int64_t n = 5;
    const auto result = expression.evaluate(std::span(&n, 1));
    if (!result.valid() || result.value->integer() != 13)
        throw std::runtime_error("mixed multi-translation-unit evaluation failed");
}
